/**
 * @file workspace_index.cpp
 * @brief 工作区语义索引实现
 *
 * 组成：
 * - 模块表（rebuild）：解析工作区所有 .ac 文件，收集顶层导出符号、类成员、import 绑定。
 * - 引用解析（findReferences）：作用域感知 + 成员类型推断 + 跨文件 import/别名，
 *   供「查找所有引用」与「重命名」共用（替代原先散落各处的实现）。
 * - 定义解析（resolveDefinition）：供「跳转定义」跨文件定位。
 *
 * 线程安全：纯计算，不访问 UI；rebuild / findReferences 可在后台线程调用。
 */

#include "workspace_index.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include "src/engine/ac_language.h"
#include "src/engine/script/ac_lexer.h"
#include "src/engine/script/ac_parser.h"
#include "src/engine/script/ast_visitor.h"
#include "src/engine/tpl/tpl_lexer.h"
#include "src/util/common/workspace_iter.h"
#include "src/util/ui/code/comment_scan.h"

namespace {

/// 读取文件文本（UTF-8），失败返回空串
QString readText(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QTextStream in(&f);
  return in.readAll();
}

/// 优先读实时缓冲内容（已打开编辑器 / 未打开但有缓冲修改的文件），否则读磁盘
QString readSource(const QString &path, const QHash<QString, QString> &liveContents) {
  auto it = liveContents.constFind(path);
  if (it != liveContents.constEnd()) return it.value();
  return readText(path);
}

/// 解析 import 相对路径：先相对导入文件所在目录，再相对工作区根目录；返回规范路径
QString resolveImportPath(const QString &importingFile, const QString &importPath,
                          const QString &rootDir) {
  if (QFileInfo(importPath).isAbsolute()) return QFileInfo(importPath).canonicalFilePath();
  const QString base1 = QFileInfo(importingFile).absolutePath() + QLatin1Char('/') + importPath;
  const QString c1 = QFileInfo(base1).canonicalFilePath();
  if (!c1.isEmpty()) return c1;
  return QFileInfo(rootDir + QLatin1Char('/') + importPath).canonicalFilePath();
}

/// 用文本扫描（跳过注释/字符串）在指定行集合上取精确 (列, 长度)
void appendLineMatches(QVector<RenameRef> &refs, const QString &text, const QString &filePath,
                       const QString &name, const QSet<int> &lines, const QSet<int> &declLines) {
  if (name.isEmpty() || text.isEmpty()) return;
  QVector<int> lineStarts;
  lineStarts.append(0);
  for (int i = 0; i < text.size(); ++i) {
    if (text.at(i) == QLatin1Char('\n')) lineStarts.append(i + 1);
  }
  const auto ranges = findIdentifierRanges(text, name);  // 精确命中（排除注释/字符串）
  for (const auto &r : ranges) {
    int line = 1;
    while (line < lineStarts.size() && lineStarts[line] <= r.first) ++line;
    if (!lines.contains(line)) continue;
    RenameRef ref;
    ref.filePath = filePath;
    ref.line = line;
    ref.column = r.first - lineStarts[line - 1];
    ref.length = r.second;
    ref.isDeclaration = declLines.contains(line);
    refs.append(ref);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  AC 语义遍历（作用域感知 + 模型收集 + 成员访问记录）
// ══════════════════════════════════════════════════════════════════════════════

/// 作用域：文件级(id=0) + 嵌套块
struct AcScope {
  int id = 0;
  bool isFile = false;
  QHash<QString, int> declLine;  ///< 本作用域声明：名 → 声明行
};

/// 类成员声明行信息
struct AcClassInfo {
  QHash<QString, int> methods;  ///< 方法名 → 声明行
  QHash<QString, int> props;    ///< 属性名 → 声明行
};

/// 成员访问（obj.method / obj.prop）
struct MemberUsage {
  int line = 0;
  bool isThis = false;    ///< 接收者为 this
  QString receiverBase;   ///< 接收者基础变量名（obj）
  QString receiverClass;  ///< 静态访问的类名
  QString usageClass;     ///< 记录时的当前类（isThis=true 时有效）
};

class AcRenameWalker : public AstVisitor {
public:
  QString target;       ///< 目标符号名
  int triggerLine = 0;  ///< 触发行（1-based）

  // ── 模型输出（始终收集）──
  QHash<QString, AcClassInfo> classes;   ///< 本文件类定义：类名 → 成员
  QHash<QString, QString> classImports;  ///< 本地类名 → 来源文件规范路径
  QHash<QString, QString> varClass;      ///< 变量 → 类名（类型推断）
  QVector<MemberUsage> memberUsages;     ///< target 作为成员访问的所有出现
  /// 来源文件规范路径 → import 子句中出现 target 原导出名的行号（1-based）。
  /// 用于"本地已有同名全局符号"的文件：只改 import 子句里的原导出名，不改本地符号。
  QHash<QString, QSet<int>> importNameLines;

  // ── 目标输出 ──
  bool targetIsLocal = false;
  int targetScopeId = 0;
  int targetDeclLine = -1;
  QSet<int> matchedLines;
  QSet<int> globalDeclLines;
  QSet<int> globalRefLines;
  QSet<int> classMemberDeclLines;
  QHash<QString, int> globalDecls;
  QVector<QPair<QString, QString>> imports;  ///< (解析后规范路径, 本地拼写)
  bool triggerIsMember = false;              ///< 触发行上 target 作为成员访问
  QString triggerClass;                      ///< 触发行成员访问解析到的类（空=未知）

  QString m_filePath;
  QString m_rootDir;

  /// 遍历入口
  void run(const Block &program, int trigLine) {
    target = target.trimmed();
    triggerLine = trigLine;
    m_scopes.clear();
    m_nextId = 0;
    m_targetCandidate = -1;
    m_targetDeclLineCandidate = -1;
    m_triggerMemberSeen = false;
    pushScope(true);
    visitBlock(program);
    finalize();
  }

  // ── 语句 ──

  void visitStmt(const Block::Stmt &stmt) override {
    if (stmt.kind == Block::Stmt::kBlock) {
      ScopeGuard guard(*this);
      visitBlock(stmt.blockBody);
      return;
    }
    AstVisitor::visitStmt(stmt);
  }

  void visitAssignStmt(const AssignStmt &as) override {
    if (as.isDeclaration) {
      declareCurrent(as.name, as.line);
    } else if (!as.name.isEmpty() && as.name == target) {
      recordUsage(as.name, as.line);
    }
    // 类型推断：new Class / 类型注解
    if (!as.name.isEmpty()) {
      if (as.value.kind == Expr::kNewInstance && !as.value.className.isEmpty()) {
        varClass[as.name] = as.value.className;
      } else if (as.typeAnnotation.kind == AcType::kClass &&
                 !as.typeAnnotation.className.isEmpty()) {
        varClass[as.name] = as.typeAnnotation.className;
      }
    }
    AstVisitor::visitAssignStmt(as);
  }

  void visitUsingStmt(const UsingStmt &us) override {
    // 用 using 语句所在行（us.line）：声明行必须是真实行号，
    // 否则右键 using 变量/重命名时声明行无法被捕获 → 漏改 using X = ... 那一行
    declareCurrent(us.varName, us.line);
    if (!us.value) return;
    visitExpr(*us.value);
  }

  void visitFuncDef(const MethodDef &md) override {
    declareCurrent(md.name, md.line);
    ScopeGuard guard(*this);
    for (const ParamDef &p : md.params) {
      // 用参数自身的行号（p.line）而非方法名行：签名跨多行时，右键参数/重命名
      // 必须能命中参数声明行，否则声明行被记成方法名行导致漏改
      declareCurrent(p.name, p.line);
      if (p.type.kind == AcType::kClass && !p.type.className.isEmpty()) {
        varClass[p.name] = p.type.className;
      }
    }
    visitBlock(md.body);
  }

  void visitForStmt(const ForStmt &fs) override {
    if (fs.isStandard) {
      ScopeGuard guard(*this);
      if (!fs.varName.isEmpty()) declareCurrent(fs.varName, fs.line);
      visitBlock(fs.initBlock);
      visitExpr(fs.condition);
      visitExpr(fs.updateExpr);
      visitBlock(fs.body);
    } else {
      ScopeGuard guard(*this);
      if (!fs.varName.isEmpty()) {
        declareCurrent(fs.varName, fs.line);
        if (!fs.varType.isEmpty()) varClass[fs.varName] = fs.varType;
      }
      visitExpr(fs.arrayExpr);
      visitBlock(fs.body);
    }
  }

  void visitIfStmt(const IfStmt &is) override {
    ScopeGuard guard(*this);
    AstVisitor::visitIfStmt(is);
  }

  void visitWhileStmt(const WhileStmt &ws) override {
    ScopeGuard guard(*this);
    AstVisitor::visitWhileStmt(ws);
  }

  void visitSwitchStmt(const SwitchStmt &ss) override {
    ScopeGuard guard(*this);
    AstVisitor::visitSwitchStmt(ss);
  }

  void visitClassDef(const ClassDef &cd) override {
    declareCurrent(cd.name, 0);
    m_inClass = true;
    m_currentClass = cd.name;
    // 保存/恢复：类可能是嵌套在方法体里的（此时外层 m_inMethodBody 为 true），
    // 类自身的成员声明仍应按类成员处理
    const bool savedInMethodBody = m_inMethodBody;
    m_inMethodBody = false;
    {
      ScopeGuard guard(*this);
      AcClassInfo info;
      for (const ObjectEntry &prop : cd.properties) {
        declareCurrent(prop.key, prop.line);
        if (prop.line > 0) info.props.insert(prop.key, prop.line);
        if (prop.value) visitExpr(*prop.value);
      }
      for (const MethodDef &m : cd.methods) {
        declareCurrent(m.name, m.line);
        if (m.line > 0) info.methods.insert(m.name, m.line);
        ScopeGuard mGuard(*this);
        // 方法体内（参数/局部 let 等）声明的名字是局部变量，不是类成员，
        // 不能写入 m_memberDeclClass（否则右键局部变量会误判成类成员 → 走成员路径 → 0 引用）
        m_inMethodBody = true;
        for (const ParamDef &p : m.params) {
          // 同 visitFuncDef：用参数自身行号，跨行签名时保证参数声明行可被命中
          declareCurrent(p.name, p.line);
          if (p.type.kind == AcType::kClass && !p.type.className.isEmpty()) {
            varClass[p.name] = p.type.className;
          }
        }
        visitBlock(m.body);
        m_inMethodBody = false;
      }
      classes.insert(cd.name, info);
    }
    m_inClass = false;
    m_currentClass.clear();
    m_inMethodBody = savedInMethodBody;
  }

  void visitImportStmt(const ImportStmt &imp) override {
    if (imp.filePath.isEmpty()) return;
    const QString resolved = resolveImportPath(m_filePath, imp.filePath, m_rootDir);
    if (resolved.isEmpty()) return;
    for (const QString &n : imp.names) {
      imports.append(qMakePair(resolved, n));
      classImports.insert(n, resolved);
      // import 语句中的符号名也是目标符号的一次引用：
      // 重命名/查找引用必须包含 import 行（否则跨文件重命名会漏改 import 列表）
      if (n == target) {
        // 多行 import 时每个名字各有自己的行号，不能统一用 imp.line
        const int ln = imp.nameLines.value(n, imp.line);
        recordUsage(n, ln);
        importNameLines[resolved].insert(ln);
      }
    }
    for (auto it = imp.aliases.begin(); it != imp.aliases.end(); ++it) {
      // it.key()=原始名，it.value()=别名；登记别名供别名引用解析到 origin 文件
      imports.append(qMakePair(resolved, it.value()));
      classImports.insert(it.value(), resolved);
      // 别名的绑定处（import { A as B } 里的 B）也是别名符号的一次引用：
      // 否则重命名别名会漏改 import 绑定行，导致调用处改新名、import 仍绑定旧名
      if (it.value() == target) recordUsage(it.value(), imp.aliasLines.value(it.value(), imp.line));
    }
  }

  // ── 表达式 ──

  void visitIdentExpr(const Expr &expr) override { recordUsage(expr.ident, expr.line); }

  void visitFuncCallExpr(const Expr &expr) override {
    recordUsage(expr.funcCall.name, expr.line);
    AstVisitor::visitFuncCallExpr(expr);
  }

  void visitMethodCallExpr(const Expr &expr) override {
    if (expr.methodCall.methodName == target) {
      recordMember(expr.line, expr.methodCall.object.get(), expr.methodCall.objName);
    }
    // 简单接收者：AST 存于 objName（object 为空），视为一次引用
    if (!expr.methodCall.objName.isEmpty() && !expr.methodCall.object) {
      recordUsage(expr.methodCall.objName, expr.line);
    }
    if (expr.methodCall.object) visitExpr(*expr.methodCall.object);
    for (const auto &arg : expr.methodCall.args) {
      if (arg) visitExpr(*arg);
    }
  }

  void visitPropAccessExpr(const Expr &expr) override {
    if (expr.prop == target) {
      recordMember(expr.line, expr.propObject.get(), expr.ident);
    }
    // 简单接收者：AST 存于 ident（propObject 为空），视为一次引用
    if (!expr.ident.isEmpty() && !expr.propObject) {
      recordUsage(expr.ident, expr.line);
    }
    if (expr.propObject) visitExpr(*expr.propObject);
  }

  void visitStaticAccessExpr(const Expr &expr) override {
    recordUsage(expr.className, expr.line);
    if (!expr.funcCall.name.isEmpty()) recordUsage(expr.funcCall.name, expr.line);
    for (const auto &arg : expr.funcCall.args) {
      if (arg) visitExpr(*arg);
    }
  }

  void visitNewInstanceExpr(const Expr &expr) override {
    recordUsage(expr.className, expr.line);
    AstVisitor::visitNewInstanceExpr(expr);
  }

  void visitFuncExprExpr(const Expr &expr) override {
    ScopeGuard guard(*this);
    for (const ParamDef &p : expr.funcExpr.params) {
      // 同 visitFuncDef：用参数自身行号，跨行签名时保证参数声明行可被命中
      declareCurrent(p.name, p.line);
      if (p.type.kind == AcType::kClass && !p.type.className.isEmpty()) {
        varClass[p.name] = p.type.className;
      }
    }
    visitBlock(expr.funcExpr.body);
  }

private:
  struct ScopeGuard {
    AcRenameWalker &w;
    explicit ScopeGuard(AcRenameWalker &walker) : w(walker) { w.pushScope(false); }
    ~ScopeGuard() { w.popScope(); }
  };

  QVector<AcScope> m_scopes;
  int m_nextId = 0;
  int m_targetCandidate = -1;
  bool m_inClass = false;
  bool m_inMethodBody = false;  ///< 是否处于类方法体内（方法体 let/参数为局部变量，非类成员）
  bool m_triggerMemberSeen = false;
  QVector<QPair<int, int>> m_usageRecords;  ///< (解析作用域 id, 行)

  void pushScope(bool isFile) {
    AcScope s;
    s.id = m_nextId++;
    s.isFile = isFile;
    m_scopes.append(s);
  }
  void popScope() { m_scopes.pop_back(); }

  void declareCurrent(const QString &name, int line) {
    if (name.isEmpty() || m_scopes.isEmpty()) return;
    AcScope &cur = m_scopes.last();
    if (line > 0) {
      cur.declLine.insert(name, line);
      // 声明行 == 触发行 → 目标作用域候选（声明的所在作用域）
      if (name == target && line == triggerLine && m_targetCandidate < 0) {
        m_targetCandidate = cur.id;
        m_targetDeclLineCandidate = line;
      }
    } else {
      cur.declLine.insert(name, cur.declLine.value(name, 0));
    }
    if (cur.isFile) {
      globalDecls.insert(name, line);
      if (name == target) globalDeclLines.insert(line);
    } else if (m_inClass && !m_inMethodBody && name == target && line > 0) {
      // 仅在类级别的成员/属性声明时记录为类成员（方法体内的 let/参数是局部变量，不是成员）
      classMemberDeclLines.insert(line);
      if (!m_currentClass.isEmpty()) m_memberDeclClass.insert(line, m_currentClass);
    }
  }

  void recordUsage(const QString &name, int line) {
    if (name.isEmpty() || name != target) return;
    int scopeId = 0;
    int declLineOfScope = -1;
    for (int i = m_scopes.size() - 1; i >= 0; --i) {
      if (m_scopes[i].declLine.contains(target)) {
        scopeId = m_scopes[i].id;
        declLineOfScope = m_scopes[i].declLine.value(target, -1);
        break;
      }
    }
    m_usageRecords.append(qMakePair(scopeId, line));
    if (scopeId == 0) globalRefLines.insert(line);
    if (line == triggerLine && (m_targetCandidate < 0 || scopeId > m_targetCandidate)) {
      m_targetCandidate = scopeId;
      if (m_targetDeclLineCandidate < 0) m_targetDeclLineCandidate = declLineOfScope;
    }
  }

  /// 记录一次成员访问（方法/属性名 == target）
  void recordMember(int line, const Expr *object, const QString &objName) {
    MemberUsage mu;
    mu.line = line;
    mu.usageClass = m_currentClass;
    // 从接收者表达式提取基础信息
    const Expr *o = object;
    // 顺着属性访问链找到最底层（基础变量 / this / 类名）
    while (o) {
      if (o->kind == Expr::kThis) {
        mu.isThis = true;
        break;
      }
      if (o->kind == Expr::kIdent) {
        mu.receiverBase = o->ident;
        break;
      }
      if (o->kind == Expr::kStaticAccess) {
        mu.receiverClass = o->className;
        break;
      }
      if (o->kind == Expr::kPropAccess) {
        o = o->propObject.get();
        continue;
      }
      break;
    }
    if (!mu.isThis && mu.receiverBase.isEmpty() && mu.receiverClass.isEmpty() &&
        !objName.isEmpty()) {
      mu.receiverBase = objName;
    }
    memberUsages.append(mu);
    // 触发行上的成员访问 → 目标为成员，解析其类
    if (line == triggerLine && !m_triggerMemberSeen) {
      m_triggerMemberSeen = true;
      m_triggerIsMemberNow = true;
      QString cls;
      if (mu.isThis)
        cls = mu.usageClass;
      else if (!mu.receiverClass.isEmpty())
        cls = mu.receiverClass;
      else if (!mu.receiverBase.isEmpty())
        cls = varClass.value(mu.receiverBase);
      m_triggerMemberClass = cls;
    }
  }

  void finalize() {
    targetScopeId = (m_targetCandidate >= 0) ? m_targetCandidate : 0;
    targetIsLocal = (targetScopeId != 0);
    for (const auto &u : m_usageRecords) {
      if (u.first == targetScopeId) matchedLines.insert(u.second);
    }
    // 目标声明行：在遍历过程中（作用域还在栈上时）捕获，
    // 避免遍历结束后嵌套作用域已出栈导致查不到
    targetDeclLine = m_targetDeclLineCandidate;
    if (targetDeclLine > 0) matchedLines.insert(targetDeclLine);
    // 触发行是类成员（方法/属性）声明 → 走类型推断的成员路径
    if (!m_triggerIsMemberNow && m_memberDeclClass.contains(triggerLine)) {
      m_triggerIsMemberNow = true;
      m_triggerMemberClass = m_memberDeclClass.value(triggerLine);
    }
    triggerIsMember = m_triggerIsMemberNow;
    triggerClass = m_triggerMemberClass;
  }

  QString m_currentClass;  ///< 当前类名（this 类型解析用）
  bool m_triggerIsMemberNow = false;
  QString m_triggerMemberClass;
  int m_targetDeclLineCandidate = -1;     ///< 遍历中捕获的目标声明行（作用域在栈上时）
  QHash<int, QString> m_memberDeclClass;  ///< 类成员声明行 → 所在类名
};

// ══════════════════════════════════════════════════════════════════════════════
//  TPL 语义遍历（each 作用域感知）
// ══════════════════════════════════════════════════════════════════════════════

/// 提取表达式中的标识符及其在表达式内的偏移（0-based）
QVector<QPair<QString, int>> extractTplIdentifiers(const QString &expr) {
  QVector<QPair<QString, int>> out;
  int i = 0;
  const int n = expr.size();
  while (i < n) {
    const QChar c = expr.at(i);
    if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$')) {
      const int start = i;
      while (i < n) {
        const QChar d = expr.at(i);
        if (d.isLetterOrNumber() || d == QLatin1Char('_') || d == QLatin1Char('$')) {
          ++i;
        } else {
          break;
        }
      }
      out.append(qMakePair(expr.mid(start, i - start), start));
    } else {
      ++i;
    }
  }
  return out;
}

/// 收集单个 TPL 文件中 target 的出现（each 作用域感知）
void collectTplFile(QVector<RenameRef> &refs, const QString &path, const QString &target,
                    bool localEach, int targetScopeStart,
                    const QHash<QString, QString> &liveContents) {
  const QString text = readSource(path, liveContents);
  if (text.isEmpty()) return;

  QString lexErr;
  const auto tokens = TplLexer::tokenize(text, lexErr);
  if (tokens.isEmpty()) return;

  QVector<int> lineStarts;
  lineStarts.append(0);
  for (int i = 0; i < text.size(); ++i) {
    if (text.at(i) == QLatin1Char('\n')) lineStarts.append(i + 1);
  }
  auto lineOf = [&lineStarts](int pos) {
    int l = 1;
    while (l < lineStarts.size() && lineStarts[l] <= pos) ++l;
    return l;
  };
  auto addRef = [&](int pos, int len, bool isDecl) {
    const int line = lineOf(pos);
    RenameRef r;
    r.filePath = path;
    r.line = line;
    r.column = pos - lineStarts[line - 1];
    r.length = len;
    r.isDeclaration = isDecl;
    refs.append(r);
  };

  struct EachScope {
    QString itemName;
    int startPos = -1;
  };
  QVector<EachScope> eachStack;

  auto recordIdent = [&](const QString &ident, int absPos) {
    if (ident != target) return;
    if (localEach) {
      for (int i = eachStack.size() - 1; i >= 0; --i) {
        if (eachStack[i].itemName == ident) {
          if (eachStack[i].startPos == targetScopeStart) addRef(absPos, ident.size(), false);
          return;
        }
      }
      return;
    }
    for (int i = eachStack.size() - 1; i >= 0; --i) {
      if (eachStack[i].itemName == ident) return;  // 被遮蔽
    }
    addRef(absPos, ident.size(), false);
  };

  for (const auto &tok : tokens) {
    switch (tok.type) {
      case TplLexer::TokenType::EachOpen: {
        QString itemName = tok.value;
        QString arrayExpr;
        const int inIdx = itemName.indexOf(QStringLiteral(" in "));
        if (inIdx >= 0) {
          itemName = itemName.left(inIdx).trimmed();
          arrayExpr = tok.value.mid(inIdx + 4).trimmed();
        } else {
          itemName = itemName.trimmed();
        }
        eachStack.append({itemName, tok.pos});
        for (const auto &id : extractTplIdentifiers(arrayExpr)) {
          recordIdent(id.first, tok.pos + id.second);
        }
        break;
      }
      case TplLexer::TokenType::EndEach:
        if (!eachStack.isEmpty()) eachStack.pop_back();
        break;
      case TplLexer::TokenType::Variable:
      case TplLexer::TokenType::IfOpen:
      case TplLexer::TokenType::ElseIf:
        for (const auto &id : extractTplIdentifiers(tok.value)) {
          recordIdent(id.first, tok.pos + id.second);
        }
        break;
      default:
        break;
    }
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  AC 收集
// ══════════════════════════════════════════════════════════════════════════════

/// 解析并遍历单个 .ac 文件，返回是否成功
bool walkAcFile(const QString &path, const QString &rootDir, const QString &target, int triggerLine,
                AcRenameWalker &walker, const QHash<QString, QString> &liveContents) {
  const QString text = readSource(path, liveContents);
  if (text.isEmpty()) return false;
  QString lexErr;
  const auto tokens = AcLexer::tokenize(text, lexErr);
  if (tokens.isEmpty()) return false;
  QSet<QString> declaredVars;
  Block program;
  AcParser parser;
  parser.setFilePath(path);
  if (!parser.parse(tokens, program, declaredVars)) return false;
  walker.m_filePath = path;
  walker.m_rootDir = rootDir;
  walker.target = target;
  walker.run(program, triggerLine);
  return true;
}

/// 全局符号引用/重命名：跨文件 import 感知收集
QVector<RenameRef> collectAcGlobals(const QString &rootDir, const QString &originFile,
                                    const QString &name,
                                    const QHash<QString, QString> &liveContents) {
  QVector<RenameRef> refs;
  const QString originCanonical = QFileInfo(originFile).canonicalFilePath();
  if (originCanonical.isEmpty()) return refs;

  forEachWorkspaceFile(
      rootDir, false,
      [](const QString &p) { return p.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive); },
      [&](const QString &path) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const bool isOrigin = (canonical == originCanonical);
        AcRenameWalker w;
        if (!walkAcFile(path, rootDir, name, 1, w, liveContents)) {
          if (isOrigin) {
            const QString text = readSource(path, liveContents);
            QSet<int> all;
            for (int i = 1; i <= text.count(QLatin1Char('\n')) + 1; ++i) all.insert(i);
            appendLineMatches(refs, text, path, name, all, QSet<int>());
          }
          return;
        }
        // 本地声明了同名全局符号？若是且未通过 import 引用 origin 的该符号 → 不相关，跳过
        const bool hasLocalGlobal = w.globalDecls.contains(name);
        if (!isOrigin && hasLocalGlobal && !w.importNameLines.contains(originCanonical)) return;
        bool linked = isOrigin;
        if (!isOrigin) {
          for (const auto &imp : w.imports) {
            if (imp.first == originCanonical && imp.second == name) {
              linked = true;
              break;
            }
          }
        }
        if (!linked) return;
        const QString text = readSource(path, liveContents);
        // 全局路径只收集全局符号本身：不能合并 classMemberDeclLines，
        // 否则会把类中与全局函数同名的成员方法一并改掉（如全局 add 与 Calculator.add）
        QSet<int> refLines;
        QSet<int> declLines;
        if (!isOrigin && hasLocalGlobal) {
          // 本地同名全局符号与 origin 的 name 是两个符号：只改 import 子句里的原导出名
          // （它指向 origin 的 name），本地声明及使用保持不动
          refLines = w.importNameLines.value(originCanonical);
        } else {
          refLines = w.globalRefLines;
          declLines = w.globalDeclLines;
        }
        appendLineMatches(refs, text, path, name, refLines, declLines);
        if (isOrigin) {
          appendLineMatches(refs, text, path, name, declLines, declLines);
          const QRegularExpression classRe(QStringLiteral("\\bclass\\s+") +
                                           QRegularExpression::escape(name) +
                                           QStringLiteral("\\b"));
          auto mit = classRe.globalMatch(text);
          while (mit.hasNext()) {
            const int pos = mit.next().capturedStart();
            int ln = 1;
            for (int i = 0; i < pos && i < text.size(); ++i) {
              if (text.at(i) == QLatin1Char('\n')) ++ln;
            }
            QSet<int> single;
            single.insert(ln);
            appendLineMatches(refs, text, path, name, single, single);
          }
        }
      });
  return refs;
}

/// 成员符号（方法/属性）引用/重命名：按接收者类型推断到具体类，跨文件 import 感知收集
QVector<RenameRef> collectAcMembers(const QString &rootDir, const QString &originFile,
                                    const QString &className, const QString &memberName,
                                    const QHash<QString, QString> &liveContents) {
  QVector<RenameRef> refs;
  const QString originCanonical = QFileInfo(originFile).canonicalFilePath();
  if (originCanonical.isEmpty()) return refs;

  forEachWorkspaceFile(
      rootDir, false,
      [](const QString &p) { return p.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive); },
      [&](const QString &path) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const bool isOrigin = (canonical == originCanonical);
        AcRenameWalker w;
        if (!walkAcFile(path, rootDir, memberName, 1, w, liveContents)) return;

        // 该文件能否"看到"目标类（origin 或 import 了该类）
        bool seesClass = isOrigin;
        if (!isOrigin) {
          auto it = w.classImports.constFind(className);
          if (it != w.classImports.constEnd() && it.value() == originCanonical) seesClass = true;
        }
        if (!seesClass) return;

        // 收集接收者类型解析到目标类的成员访问
        QSet<int> memberLines;
        for (const MemberUsage &mu : w.memberUsages) {
          QString cls;
          if (mu.isThis) {
            cls = mu.usageClass;
          } else if (!mu.receiverClass.isEmpty()) {
            cls = mu.receiverClass;
          } else if (!mu.receiverBase.isEmpty()) {
            cls = w.varClass.value(mu.receiverBase);
          }
          if (cls == className) memberLines.insert(mu.line);
        }
        QSet<int> declLines;
        if (isOrigin) {
          const AcClassInfo &ci = w.classes.value(className);
          int dl = ci.methods.value(memberName, 0);
          if (dl <= 0) dl = ci.props.value(memberName, 0);
          if (dl > 0) {
            memberLines.insert(dl);
            declLines.insert(dl);
          }
        }
        const QString text = readSource(path, liveContents);
        appendLineMatches(refs, text, path, memberName, memberLines, declLines);
      });
  return refs;
}

/// 触发文件内局部（非成员）引用/重命名：目标作用域精确收集
QVector<RenameRef> collectAcLocal(const QString &triggerFilePath, const QString &name,
                                  const AcRenameWalker &walker,
                                  const QHash<QString, QString> &liveContents) {
  QVector<RenameRef> refs;
  QSet<int> declLines;
  if (walker.targetDeclLine > 0) declLines.insert(walker.targetDeclLine);
  appendLineMatches(refs, readSource(triggerFilePath, liveContents), triggerFilePath, name,
                    walker.matchedLines, declLines);
  return refs;
}

/// 按 (文件, 行, 列) 去重引用结果
void dedupRefs(QVector<RenameRef> &refs) {
  QSet<QString> seen;
  QVector<RenameRef> out;
  out.reserve(refs.size());
  for (const RenameRef &r : refs) {
    const QString key = r.filePath + QLatin1Char('#') + QString::number(r.line) + QLatin1Char(':') +
                        QString::number(r.column);
    if (seen.contains(key)) continue;
    seen.insert(key);
    out.append(r);
  }
  refs = out;
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════════════
//  模块表
// ══════════════════════════════════════════════════════════════════════════════

/// 单文件模块信息
struct WorkspaceIndex::ModuleInfo {
  QString filePath;                                             ///< 规范绝对路径
  QHash<QString, SemanticSymbol> topSymbols;                    ///< 顶层符号：名字 → 符号
  QHash<QString, QHash<QString, SemanticSymbol>> classMembers;  ///< 类名 → (成员名 → 符号)
  QVector<SemanticImport> imports;                              ///< import 绑定
  QHash<QString, QString> importLocalToSource;                  ///< localName → 来源文件
  QHash<QString, QString> classImportToSource;                  ///< 类名 → 来源文件（import 的类）
  QSet<QString> localGlobals;                                   ///< 本文件声明的顶层名字
};

/// 解析源码文本并填充模块信息（顶层符号 / 类成员 / import 绑定）。
/// 供 rebuild（全量）与 resolveDefinition（按需解析实时缓冲）共用。
/// @param text 源码内容；@param path 规范绝对路径（用于键与 import 相对路径解析）
void WorkspaceIndex::collectModuleSymbols(const QString &text, const QString &path,
                                          const QString &root, WorkspaceIndex::ModuleInfo &mi) {
  mi.filePath = path;
  if (text.trimmed().isEmpty()) return;
  QString lexErr;
  const auto tokens = AcLexer::tokenize(text, lexErr);
  if (tokens.isEmpty()) return;
  QSet<QString> declaredVars;
  Block program;
  AcParser parser;
  parser.setFilePath(path);
  if (!parser.parse(tokens, program, declaredVars)) return;
  for (const Block::Stmt &stmt : program.stmts) {
    switch (stmt.kind) {
      case Block::Stmt::kFuncDef: {
        SemanticSymbol s;
        s.name = stmt.funcDef.name;
        s.kind = QStringLiteral("function");
        s.filePath = path;
        s.line = stmt.funcDef.line;
        s.isExported = stmt.funcDef.isExported;
        for (const ParamDef &p : stmt.funcDef.params) s.params << p.name;
        s.key = QStringLiteral("g:") + path + QLatin1Char('#') + s.name;
        mi.topSymbols.insert(s.name, s);
        mi.localGlobals.insert(s.name);
        break;
      }
      case Block::Stmt::kClassDef: {
        SemanticSymbol cs;
        cs.name = stmt.classDef.name;
        cs.kind = QStringLiteral("class");
        cs.filePath = path;
        cs.line = stmt.line;
        cs.isExported = stmt.classDef.isExported;
        cs.key = QStringLiteral("g:") + path + QLatin1Char('#') + cs.name;
        mi.topSymbols.insert(cs.name, cs);
        mi.localGlobals.insert(cs.name);
        auto &members = mi.classMembers[cs.name];
        for (const MethodDef &m : stmt.classDef.methods) {
          SemanticSymbol ms;
          ms.name = m.name;
          ms.kind = QStringLiteral("method");
          ms.filePath = path;
          ms.line = m.line;
          ms.parentClass = cs.name;
          ms.isStatic = m.isStatic;
          ms.key =
              QStringLiteral("m:") + path + QLatin1Char('#') + cs.name + QLatin1Char('#') + ms.name;
          members.insert(ms.name, ms);
        }
        for (const ObjectEntry &prop : stmt.classDef.properties) {
          SemanticSymbol ps;
          ps.name = prop.key;
          ps.kind = QStringLiteral("property");
          ps.filePath = path;
          ps.line = prop.line;
          ps.parentClass = cs.name;
          ps.isStatic = prop.isStatic;
          ps.key =
              QStringLiteral("m:") + path + QLatin1Char('#') + cs.name + QLatin1Char('#') + ps.name;
          members.insert(ps.name, ps);
        }
        break;
      }
      case Block::Stmt::kInterfaceDef: {
        SemanticSymbol s;
        s.name = stmt.interfaceDef.name;
        s.kind = QStringLiteral("interface");
        s.filePath = path;
        s.line = stmt.line;
        s.isExported = stmt.interfaceDef.isExported;
        s.key = QStringLiteral("g:") + path + QLatin1Char('#') + s.name;
        mi.topSymbols.insert(s.name, s);
        mi.localGlobals.insert(s.name);
        break;
      }
      case Block::Stmt::kEnumDef: {
        SemanticSymbol s;
        s.name = stmt.enumDef.name;
        s.kind = QStringLiteral("enum");
        s.filePath = path;
        s.line = stmt.line;
        s.isExported = stmt.enumDef.isExported;
        s.key = QStringLiteral("g:") + path + QLatin1Char('#') + s.name;
        mi.topSymbols.insert(s.name, s);
        mi.localGlobals.insert(s.name);
        break;
      }
      case Block::Stmt::kAssign: {
        if (stmt.assign.isDeclaration && !stmt.assign.name.isEmpty()) {
          SemanticSymbol s;
          s.name = stmt.assign.name;
          s.kind = QStringLiteral("variable");
          s.filePath = path;
          s.line = stmt.assign.line;
          s.isExported = stmt.assign.isExported;
          s.key = QStringLiteral("g:") + path + QLatin1Char('#') + s.name;
          mi.topSymbols.insert(s.name, s);
          mi.localGlobals.insert(s.name);
        }
        break;
      }
      case Block::Stmt::kImport: {
        if (stmt.importStmt.filePath.isEmpty()) break;
        const QString resolved = resolveImportPath(path, stmt.importStmt.filePath, root);
        if (resolved.isEmpty()) break;
        for (const QString &n : stmt.importStmt.names) {
          SemanticImport si;
          si.sourceFile = resolved;
          si.originalName = n;
          si.localName = n;
          mi.imports.append(si);
          mi.importLocalToSource.insert(n, resolved);
        }
        for (auto it = stmt.importStmt.aliases.begin(); it != stmt.importStmt.aliases.end(); ++it) {
          SemanticImport si;
          si.sourceFile = resolved;
          si.originalName = it.key();
          si.localName = it.value();
          mi.imports.append(si);
          mi.importLocalToSource.insert(it.value(), resolved);
        }
        break;
      }
      default:
        break;
    }
  }
}

WorkspaceIndex &WorkspaceIndex::ins() {
  static WorkspaceIndex instance;
  return instance;
}

void WorkspaceIndex::rebuild(const QString &rootDir) {
  qDeleteAll(m_modules);
  m_modules.clear();
  m_rootDir.clear();
  if (rootDir.isEmpty()) return;
  const QString root = QDir::cleanPath(rootDir);

  // 第一遍：解析所有 .ac 文件，收集顶层符号 / 类成员 / import 绑定
  forEachWorkspaceFile(
      root, false,
      [](const QString &p) { return p.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive); },
      [&](const QString &path) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty()) return;
        ModuleInfo *mi = new ModuleInfo;
        collectModuleSymbols(readText(path), canonical, root, *mi);
        m_modules.insert(canonical, mi);
      });

  // 第二遍：登记"import 的类"（来源文件的顶层符号 kind==class → 类 import 映射）
  for (auto it = m_modules.begin(); it != m_modules.end(); ++it) {
    ModuleInfo *mi = it.value();
    for (const SemanticImport &imp : mi->imports) {
      auto srcIt = m_modules.find(imp.sourceFile);
      if (srcIt == m_modules.end()) continue;
      auto t = srcIt.value()->topSymbols.find(imp.originalName);
      if (t != srcIt.value()->topSymbols.end() && t->kind == QStringLiteral("class")) {
        mi->classImportToSource.insert(imp.originalName, imp.sourceFile);
      }
    }
  }

  m_rootDir = root;
}

// ══════════════════════════════════════════════════════════════════════════════
//  引用收集（查找所有引用 / 重命名）
// ══════════════════════════════════════════════════════════════════════════════

QVector<RenameRef> WorkspaceIndex::findReferences(const QString &rootDir,
                                                  const QString &triggerFilePath, int triggerLine,
                                                  int triggerColumn, const QString &name,
                                                  const QHash<QString, QString> &liveContents) {
  // 注意：本函数可能在工作线程调用（QtConcurrent）。模块表（m_modules）由主线程在
  // 启动/保存后重建，这里只读传入的 liveContents + 磁盘、不触碰 m_modules，避免数据竞争。
  Q_UNUSED(triggerColumn);
  QVector<RenameRef> refs;
  const QString nm = name.trimmed();
  if (nm.isEmpty() || rootDir.isEmpty() || triggerFilePath.isEmpty()) return refs;

  // ── TPL ──
  if (triggerFilePath.endsWith(AcFileSuffix::kTpl, Qt::CaseInsensitive)) {
    bool localEach = false;
    int targetScopeStart = -1;
    {
      const QString text = readSource(triggerFilePath, liveContents);
      QString lexErr;
      const auto tokens = TplLexer::tokenize(text, lexErr);
      for (const auto &tok : tokens) {
        if (tok.type == TplLexer::TokenType::EachOpen) {
          QString item = tok.value;
          const int inIdx = item.indexOf(QStringLiteral(" in "));
          if (inIdx >= 0) item = item.left(inIdx).trimmed();
          if (item.trimmed() == nm) {
            localEach = true;
            targetScopeStart = tok.pos;
            break;
          }
        }
      }
    }
    if (localEach) {
      collectTplFile(refs, triggerFilePath, nm, true, targetScopeStart, liveContents);
      dedupRefs(refs);
      return refs;
    }
    forEachWorkspaceFile(
        rootDir, false,
        [](const QString &p) { return p.endsWith(AcFileSuffix::kTpl, Qt::CaseInsensitive); },
        [&](const QString &path) { collectTplFile(refs, path, nm, false, -1, liveContents); });
    dedupRefs(refs);
    return refs;
  }

  // ── AC ──
  if (!triggerFilePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) return refs;

  AcRenameWalker triggerWalker;
  if (!walkAcFile(triggerFilePath, rootDir, nm, triggerLine, triggerWalker, liveContents)) {
    // 解析失败：退化为全文件文本扫描
    const QString text = readSource(triggerFilePath, liveContents);
    QSet<int> all;
    for (int i = 1; i <= text.count(QLatin1Char('\n')) + 1; ++i) all.insert(i);
    appendLineMatches(refs, text, triggerFilePath, nm, all, QSet<int>());
    dedupRefs(refs);
    return refs;
  }

  // 成员访问（类型推断路径）
  if (triggerWalker.triggerIsMember && !triggerWalker.triggerClass.isEmpty()) {
    const QString &cls = triggerWalker.triggerClass;
    // 确定类的来源文件：本文件定义 or import
    QString origin = triggerFilePath;
    bool definedHere = triggerWalker.classes.contains(cls);
    if (!definedHere) {
      auto it = triggerWalker.classImports.constFind(cls);
      if (it != triggerWalker.classImports.constEnd()) {
        origin = it.value();
      } else {
        dedupRefs(refs);
        return refs;  // 类无法定位 → 无引用
      }
    }
    refs = collectAcMembers(rootDir, origin, cls, nm, liveContents);
    dedupRefs(refs);
    return refs;
  }

  // 局部（作用域精确）
  if (triggerWalker.targetIsLocal) {
    refs = collectAcLocal(triggerFilePath, nm, triggerWalker, liveContents);
    dedupRefs(refs);
    return refs;
  }

  // 全局：确定 origin 文件
  QString origin = triggerFilePath;
  if (!triggerWalker.globalDecls.contains(nm)) {
    for (const auto &imp : triggerWalker.imports) {
      if (imp.second == nm) {
        origin = imp.first;
        break;
      }
    }
  }
  refs = collectAcGlobals(rootDir, origin, nm, liveContents);
  dedupRefs(refs);
  return refs;
}

// ══════════════════════════════════════════════════════════════════════════════
//  定义解析（跳转定义）
// ══════════════════════════════════════════════════════════════════════════════

const SemanticSymbol *WorkspaceIndex::findTopSymbol(const QString &filePath,
                                                    const QString &name) const {
  auto mi = m_modules.constFind(QFileInfo(filePath).canonicalFilePath());
  if (mi == m_modules.constEnd()) return nullptr;
  auto it = mi.value()->topSymbols.constFind(name);
  if (it == mi.value()->topSymbols.constEnd()) return nullptr;
  return &it.value();
}

const SemanticSymbol *WorkspaceIndex::findMemberSymbol(const QString &filePath,
                                                       const QString &className,
                                                       const QString &memberName) const {
  auto mi = m_modules.constFind(QFileInfo(filePath).canonicalFilePath());
  if (mi == m_modules.constEnd()) return nullptr;
  auto cit = mi.value()->classMembers.constFind(className);
  if (cit == mi.value()->classMembers.constEnd()) return nullptr;
  auto mit = cit->constFind(memberName);
  if (mit == cit->constEnd()) return nullptr;
  return &mit.value();
}

QString WorkspaceIndex::importSourceOf(const QString &filePath, const QString &localName) const {
  auto mi = m_modules.constFind(QFileInfo(filePath).canonicalFilePath());
  if (mi == m_modules.constEnd()) return QString();
  return mi.value()->importLocalToSource.value(localName);
}

SemanticSymbol WorkspaceIndex::resolveDefinition(
    const QString &filePath, int line, int column, const QString &name,
    const std::function<QString(const QString &)> &contentProvider) const {
  Q_UNUSED(column);
  SemanticSymbol out;
  if (name.isEmpty() || m_rootDir.isEmpty()) return out;
  const QString canonical = QFileInfo(filePath).canonicalFilePath();
  if (canonical.isEmpty()) return out;

  // 内容读取：优先提供器（已打开编辑器 / 缓冲文件），否则磁盘快照
  auto readSrc = [&contentProvider](const QString &p) -> QString {
    if (contentProvider) {
      const QString c = contentProvider(p);
      if (!c.isEmpty()) return c;
    }
    return readText(p);
  };

  // 触发文件用实时内容做作用域遍历（当前编辑器/缓冲，避免用旧磁盘内容判定）
  {
    const QString triggerText = readSrc(canonical);
    QHash<QString, QString> triggerLive;
    if (!triggerText.isEmpty()) triggerLive.insert(canonical, triggerText);
    AcRenameWalker w;
    if (walkAcFile(canonical, m_rootDir, name, line, w, triggerLive)) {
      // 成员访问（this.x / obj.x / Class.x）→ 解析到具体类的成员
      if (w.triggerIsMember && !w.triggerClass.isEmpty()) {
        const QString &cls = w.triggerClass;
        QString src = canonical;
        bool definedHere = w.classes.contains(cls);
        if (definedHere) {
          // 本文件类：直接用实时内容收集到的成员声明行
          const AcClassInfo &ci = w.classes.value(cls);
          int dl = ci.methods.value(name, 0);
          if (dl <= 0) dl = ci.props.value(name, 0);
          if (dl > 0) {
            out.key =
                QStringLiteral("m:") + canonical + QLatin1Char('#') + cls + QLatin1Char('#') + name;
            out.name = name;
            out.kind = QStringLiteral("method");
            out.parentClass = cls;
            out.filePath = canonical;
            out.line = dl;
            return out;
          }
          return out;
        }
        auto it = w.classImports.constFind(cls);
        if (it == w.classImports.constEnd()) return out;  // 类无法定位
        src = it.value();
        // 类文件优先用实时缓冲构建临时模块（可能打开/缓冲，索引模块表可能旧）
        {
          ModuleInfo tmp;
          collectModuleSymbols(readSrc(src), src, m_rootDir, tmp);
          auto cit = tmp.classMembers.constFind(cls);
          if (cit != tmp.classMembers.constEnd()) {
            auto mit = cit->constFind(name);
            if (mit != cit->constEnd()) return mit.value();
          }
        }
        if (const SemanticSymbol *ms = findMemberSymbol(src, cls, name)) return *ms;
        return out;
      }
      // 局部变量 / 参数：返回声明位置（key 用声明行标识）
      if (w.targetIsLocal) {
        out.key =
            QStringLiteral("l:") + canonical + QLatin1Char('#') + QString::number(w.targetDeclLine);
        out.name = name;
        out.kind = QStringLiteral("variable");
        out.filePath = canonical;
        out.line = w.targetDeclLine;
        return out;
      }
      // 顶层全局（本文件，用实时内容判定声明行）
      if (w.globalDecls.contains(name)) {
        const int dl = w.globalDecls.value(name);
        if (dl > 0) {
          out.key = QStringLiteral("g:") + canonical + QLatin1Char('#') + name;
          out.name = name;
          out.kind = QStringLiteral("variable");
          out.filePath = canonical;
          out.line = dl;
          return out;
        }
      }
    }
  }

  // 顶层全局：索引模块表兜底
  if (const SemanticSymbol *top = findTopSymbol(canonical, name)) return *top;

  // 导入符号（含别名）：localName → 来源文件的 originalName。
  // 来源文件优先用实时缓冲（可能打开/缓冲，索引模块表可能旧）
  auto mi = m_modules.constFind(canonical);
  if (mi != m_modules.constEnd()) {
    for (const SemanticImport &imp : mi.value()->imports) {
      if (imp.localName != name) continue;
      ModuleInfo tmp;
      collectModuleSymbols(readSrc(imp.sourceFile), imp.sourceFile, m_rootDir, tmp);
      auto it = tmp.topSymbols.constFind(imp.originalName);
      if (it != tmp.topSymbols.constEnd()) return it.value();
      if (const SemanticSymbol *src = findTopSymbol(imp.sourceFile, imp.originalName)) return *src;
      // 源文件可能是类成员（少见），回退返回空
      return out;
    }
  }

  return out;
}
