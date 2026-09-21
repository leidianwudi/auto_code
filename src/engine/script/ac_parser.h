/**
 * @file ac_parser.h
 * @brief 语法分析器 — 将 Token 序列解析为 AST
 */

#pragma once

#include <utility>
#include <vector>

#include "ac_diagnostic.h"
#include "ac_lexer.h"
#include "ac_type.h"

/// @brief 语法分析器 — 将 Token 序列解析为 AST
class AcParser {
public:
  /// @brief 解析入口：将 tokens 解析为 AST（跳过可选的 main 关键字）
  /// @param tokens 词法分析结果
  /// @param[out] program 输出的 AST 根节点
  /// @param[in,out] declaredVars 已声明的变量名集合（解析过程中会新增）
  /// @return true 解析成功，false 解析失败（调用 error() 获取错误信息）
  bool parse(const QVector<Token> &tokens, Block &program, QSet<QString> &declaredVars);

  /// @brief 设置当前解析的源文件路径（用于断点定位）
  void setFilePath(const QString &path) { m_filePath = path; }

  /// @brief 设置诊断收集器（可选；设置后所有解析错误同步产出结构化诊断）
  void setDiagCollector(AcDiagCollector *diags) { m_diags = diags; }

  /// @brief 启用错误恢复模式：语句解析失败不中断，跳到同步点继续，
  ///        失败语句以 isRecovered 标记（残缺表达式为 Expr::kError）。
  ///        供编辑器诊断/语义服务使用；执行管线（解释器/VM）保持 fail-fast。
  void setRecoveryMode(bool on) { m_recoveryMode = on; }
  bool recoveryMode() const { return m_recoveryMode; }

  /// @brief 获取错误信息
  QString error() const { return m_error; }

private:
  // ── token 操作 ──
  Token peek();
  Token peek(int offset);
  Token advance();
  bool match(TokenType t);
  bool expect(TokenType t, const QString &msg);
  /// @brief 期望分号；缺失时错误定位到语句所在行（stmtLine），而非下一个 token 的行
  bool expectSemi(const QString &msg, int stmtLine);
  /// @brief 判断语句之后是否需要分号：声明类（class/interface/enum/function）与
  ///        复合语句（if/for/while/switch）以 } 结尾不需要分号；
  ///        blockAllowed 为 true 时独立块语句 { } 同样豁免
  bool stmtNeedsSemi(const Block::Stmt &stmt, bool blockAllowed) const;
  bool isPropertyName(TokenType t) const;
  /// @brief 判断 token 是否可作为参数名；关键字 from 可用作参数名（如 indexOf(sub, from?: Number)）
  bool isParamName(TokenType t) const;
  /// @brief 前瞻：语句开头 ident[...] 的匹配 ']' 之后是否紧跟 ++/--（索引自增语句判定，不消耗
  /// token）
  bool indexTargetFollowedByIncDec() const;

  // ── 二元运算解析辅助（模板化，消除重复代码） ──
  using ParseNextFn = bool (AcParser::*)(Expr &);
  template <ParseNextFn parseNext>
  bool parseBinary(Expr &expr, const std::vector<std::pair<TokenType, Expr::BinaryOp>> &ops);

  // ── 语法分析（递归下降） ──
  bool parseProgram(Block &block);
  bool parseBlock(Block &block);
  bool parseBlockOrStmt(Block &block);
  bool parseStmt(Block::Stmt &stmt);
  /// @brief 解析以标识符开头的语句（赋值/属性赋值/方法调用/索引赋值/表达式）
  bool parseIdentStmt(Block::Stmt &stmt, const Token &t);
  /// @brief 解析静态属性赋值 (ClassName::member = value)，成功时返回 true
  bool tryParseStaticAssign(Block::Stmt &stmt);
  /// @brief 解析以 this 开头的语句（属性赋值/表达式）
  bool parseThisStmt(Block::Stmt &stmt);
  bool parseCallStmt(CallStmt &cs);
  bool parseAssignStmt(AssignStmt &as);
  bool parseIndexAssignStmt(IndexAssignStmt &ias);
  bool parseForStmt(ForStmt &fs);
  bool parseIfStmt(IfStmt &is);
  bool parseWhileStmt(WhileStmt &ws);
  bool parseSwitchStmt(SwitchStmt &ss);
  bool parseImportStmt(ImportStmt &imp);
  /// @brief 解析 try/catch/finally（stmt.kind 已置 kTry，try 关键字已消耗）
  bool parseTryStmt(Block::Stmt &stmt);
  bool parseClassDef(ClassDef &cd);
  bool parseInterfaceDef(InterfaceDef &iface);
  bool parseEnumDef(EnumDef &ed);
  bool parseReturnStmt(Expr &retVal);
  bool parseExpr(Expr &expr);
  bool parseTernary(Expr &expr);
  bool parseLogicalOr(Expr &expr);
  bool parseLogicalAnd(Expr &expr);
  bool parseComparison(Expr &expr);
  bool parseAddSub(Expr &expr);
  bool parseMulDiv(Expr &expr);
  bool parseUnary(Expr &expr);
  bool parsePostfix(Expr &expr);
  bool parsePrimary(Expr &expr);
  bool parseObject(Expr &expr);
  bool parseArray(Expr &expr);
  bool parseFuncCall(const QString &name, Expr &expr);
  bool parseTemplateString(Expr &expr);
  bool parseMethodDef(MethodDef &md);
  /// @brief 解析参数列表统一入口（构造器/接口方法/方法/函数表达式原先各写一份）。
  ///        前置：调用方已消费 '('；返回后停在 ')' 之前，由调用方 expect(')') 收尾。
  /// @param requireType  参数是否必须带类型注解（接口方法/方法要求，构造器可选）
  /// @param allowDefault 是否允许 = 字面量默认值（接口方法不允许）
  /// @param declareVars  参数名是否加入 m_declaredVars（接口方法不注入）
  bool parseParamList(QVector<ParamDef> &out, bool requireType, bool allowDefault,
                      bool declareVars);
  /// @brief 解析参数默认值字面量（= 后仅允许 数字/字符串/布尔/null，支持负数）
  /// @param[out] out 解析到的字面量值（Null 类型表示显式 = null）
  bool parseParamDefault(QJsonValue &out);
  /// @brief 解析类属性（static let / let / ident = value），返回是否成功
  bool parseClassProperty(ClassDef &cd, AccessLevel access, bool isStatic);

  // ── 复合赋值运算符解析 ──
  /// @brief 检查当前 token 是否为复合赋值运算符，如果是则消耗并返回对应枚举值
  CompoundOp parseCompoundOp();

  // ── 类型解析 ──
  /// @brief 类型注解解析结果三态：无注解 / 有注解且成功 / 裸冒号语法错误
  enum class TypeAnnResult { kNone = 0, kOk = 1, kError = -1 };
  /// @brief 解析类型注解（: TypeExpr），返回类型结构
  AcType parseType();
  /// @brief 将类型名称解析为 AcType（内建类型 / 自定义类名）
  AcType resolveTypeName(const QString &name);
  /// @brief 解析可选的 : Type 类型注解，返回三态（见 TypeAnnResult）：
  ///        kNone=无注解；kOk=有注解且成功；kError=有冒号但类型名缺失（语法错误，已报告）
  /// @param outType 输出解析到的类型（如果有注解）
  TypeAnnResult parseTypeAnnotation(AcType &outType);

  // ── 作用域与声明 ──
  /// @brief 作用域守卫：构造时压栈，析构时弹栈（配合 RAII 保证异常/提前返回也能恢复）
  struct ScopeGuard {
    QVector<QSet<QString>> &scopes;
    explicit ScopeGuard(QVector<QSet<QString>> &s) : scopes(s) { scopes.append(QSet<QString>()); }
    ~ScopeGuard() { scopes.pop_back(); }
  };
  /// @brief 在当前作用域声明变量；重复声明时设置错误并返回 false
  bool declareVar(const QString &name, int line);

  /// @brief 恢复模式：声明语句解析失败被丢弃（isRecovered 占位）时，回滚已登记的声明
  ///        （declaredVars + 当前作用域），避免后续语句对已失败名字的误判（伪已声明 /
  ///        伪重复声明）。非恢复模式无需调用（失败即整体终止，declaredVars 被丢弃）。
  void rollbackDeclared(const QString &name);

  /// @brief 错误恢复：把一次语句解析失败转化为残缺语句并同步 token。
  ///        非恢复模式直接返回 false（保持 fail-fast）。
  /// @param stmt 失败的语句（可能已被部分填充）
  /// @param block 当前语句块（用于追加残缺语句）
  /// @param loc 失败语句的起始位置（残缺占位与诊断定位）
  /// @return true 已恢复（调用方应 continue）；false 需 return false
  bool recoverStatement(Block::Stmt &stmt, Block &block, const AcLoc &loc);

  /// @brief 消耗 token 至下一个语句同步点（错误恢复核心）。
  ///        同步点：同层/浅层的 ;（消耗并停止）、语句起始关键字与 {（不消耗）、
  ///        使括号深度回到基线的 }（不消耗，交回外层）。
  /// @param baseBraceDepth 当前构造括号深度：parseProgram=0，parseBlock=1（块已吞 '{'）
  void recoverToStatementBoundary(int baseBraceDepth);

  /// @brief 语句后的分号缺失时同步到下一语句边界（语句已成功入块，无需追加残缺占位）。
  ///        非恢复模式返回 false。
  bool recoverSemi(Block &block);


  // ── 内部状态 ──
  int m_pos = 0;
  int m_exprDepth = 0;  ///< 表达式嵌套深度（parseExpr 递归计数，防超深嵌套打爆栈）
  int m_blockDepth = 0; ///< 语句块嵌套深度（parseBlock 递归计数，防超深嵌套打爆栈）
  QVector<Token> m_tokens;
  QString m_error;
  QString m_filePath;  ///< 当前解析的源文件路径
  QSet<QString> *m_declaredVars = nullptr;
  bool m_recoveryMode = false;  ///< 错误恢复模式（编辑器诊断用；执行管线保持 false）
  /// 作用域栈：栈顶为当前作用域，用于检测同一作用域内的重复声明
  QVector<QSet<QString>> m_scopes;
  AcDiagCollector *m_diags = nullptr;  ///< 诊断收集器（可选；空时不产出诊断）

  /// @brief 统一错误出口：写 legacy 单错误串（"msg at line N"）并同步诊断收集器
  /// @param code 诊断码（AcDiagCode::xxx）
  /// @param msg 干净消息（不含行号后缀）
  /// @param loc 错误位置（loc.line 用于 legacy 行号）
  void reportError(const QString &code, const QString &msg, const AcLoc &loc);
};
