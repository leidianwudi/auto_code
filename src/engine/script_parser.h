#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <utility>

/// @brief 脚本解析器 — 将 main.ac 脚本解析为 AST 并解释执行
///
/// 支持的语法：
///   - 变量声明：  let name = expr
///   - 变量赋值：  name = expr（变量必须先 let 声明才能赋值）
///   - 类定义：    class ClassName { let prop = val; function method(args) {
///   ... } }
///   - 实例化：    let obj = new ClassName()
///   - 方法调用：  obj.method(args)
///   - 函数调用：  call("cls", "func", arg)
///   - for 循环：  for (var in arrayExpr) { ... }
///   - if 条件：   if (cond) { ... } else { ... }
///   - return 语句：return expr
///   - 对象字面量：{ key: val, ... }
///   - 数组字面量：[item, ...]
///   - 属性访问：  obj.prop / obj["key"]
///   - 字符串拼接：expr + expr
///
/// 变量规则：
///   - 所有变量必须先使用 let 声明才能使用
///   - 未声明的变量赋值会报错
///   - for 循环变量自动视为已声明
///
/// 内置函数：call / readJson / render / write / getCheckedFiles / merge /
/// basename / print
class ScriptParser {
public:
  ScriptParser();

  QString error() const { return m_error; }
  void setScriptDir(const QString &dir) { m_scriptDir = dir; }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  void setJsonFile(const QString &path) { m_jsonPath = path; }
  QStringList generatedFiles() const { return m_generatedFiles; }

  /// 日志回调：print() 的输出通过此回调通知 UI
  using LogCallback = std::function<void(const QString &text, bool isError)>;
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

  bool parse(const QString &source);
  QJsonValue execute();

private:
  /// @brief 词法单元类型
  enum TokenType {
    TOK_EOF,      ///< 输入结束
    TOK_IDENT,    ///< 标识符（变量名、函数名、方法名）
    TOK_STRING,   ///< 字符串字面量 "hello"
    TOK_NUMBER,   ///< 数字字面量 123
    TOK_LBRACE,   ///< {
    TOK_RBRACE,   ///< }
    TOK_LPAREN,   ///< (
    TOK_RPAREN,   ///< )
    TOK_LBRACKET, ///< [
    TOK_RBRACKET, ///< ]
    TOK_COMMA,    ///< ,
    TOK_COLON,    ///< :
    TOK_DOT,      ///< .（属性访问）
    TOK_EQUALS,   ///< =（赋值）
    TOK_PLUS,     ///< +
    TOK_MINUS,    ///< -
    TOK_MUL,      ///< *
    TOK_DIV,      ///< /
    TOK_SEMI,     ///< ;（语句结束）
    TOK_FOR,      ///< for 关键字
    TOK_IN,       ///< in 关键字
    TOK_IF,       ///< if 关键字
    TOK_ELSE,     ///< else 关键字
    TOK_LET,      ///< let 关键字（变量声明）
    TOK_CLASS,    ///< class 关键字（类定义）
    TOK_FUNCTION, ///< function 关键字（方法定义）
    TOK_NEW,      ///< new 关键字（实例化）
    TOK_THIS,     ///< this 关键字（当前实例引用）
    TOK_RETURN,   ///< return 关键字（返回值）
  };

  /// @brief 词法单元
  struct Token {
    TokenType type = TOK_EOF;
    QString text; ///< 原始文本
    int line = 0; ///< 行号（用于错误报告）
  };

  struct Stmt;
  struct Expr;

  /// @brief 语句块 — 由 { } 包裹的一组语句
  struct Block {
    QVector<Stmt> stmts;
  };

  /// @brief 对象字面量中的键值对
  struct ObjectEntry {
    QString key;
    Expr *value = nullptr; ///< 使用指针避免递归类型导致编译错误
  };

  /// @brief 函数调用表达式：name(arg1, arg2, ...)
  struct FuncCall {
    QString name;
    QVector<Expr *> args; ///< 使用指针避免递归类型导致编译错误
  };

  /// @brief 方法调用表达式：obj.method(arg1, arg2, ...)
  struct MethodCall {
    QString objName;      ///< 对象变量名
    QString methodName;   ///< 方法名
    QVector<Expr *> args; ///< 参数列表
  };

  /// @brief 方法定义：function name(params) { body }
  struct MethodDef {
    QString name;
    QStringList params; ///< 参数名列表
    Block body;         ///< 方法体
  };

  /// @brief 类定义：class Name { let prop = val; function method(args) { ... }
  /// }
  struct ClassDef {
    QString name;
    QVector<ObjectEntry> properties; ///< 类属性及其默认值
    QVector<MethodDef> methods;      ///< 类方法
  };

  /// @brief 表达式节点 — AST 中的表达式
  struct Expr {
    enum Kind {
      kString,      ///< 字符串字面量
      kNumber,      ///< 数字字面量
      kIdent,       ///< 变量引用
      kPropAccess,  ///< 属性访问 obj.prop
      kIndexAccess, ///< 索引访问 obj["key"]
      kObject,      ///< 对象字面量 { key: val }
      kArray,       ///< 数组字面量 [item, ...]
      kFuncCall,    ///< 函数调用 name(args)
      kMethodCall,  ///< 方法调用 obj.method(args)
      kNewInstance, ///< new ClassName()
      kThis,        ///< this 关键字
      kBinary,      ///< 二元运算 left op right
    } kind = kString;
    int line = 0;                    ///< 源码行号（用于错误报告）
    QString strVal;                  ///< 字符串值
    double numVal = 0;               ///< 数值
    QString ident;                   ///< 标识符名
    QString prop;                    ///< 属性名（用于 kPropAccess）
    QString indexKey;                ///< 索引键（用于 kIndexAccess）
    QVector<ObjectEntry> objEntries; ///< 对象条目
    QVector<Expr *> arrItems;        ///< 数组元素（指针）
    FuncCall funcCall;               ///< 函数调用信息
    MethodCall methodCall;           ///< 方法调用信息
    QString className;               ///< 类名（用于 kNewInstance）
    enum BinaryOp { kAdd, kSub, kMul, kDiv } binOp = kAdd; ///< 二元运算符
    Expr *left = nullptr;                                  ///< 二元运算左子树
    Expr *right = nullptr;                                 ///< 二元运算右子树

    Expr() = default;
    Expr(const Expr &other) { copyFrom(other); }
    Expr(Expr &&other) noexcept { moveFrom(std::move(other)); }
    Expr &operator=(const Expr &other) {
      if (this != &other) {
        freeOwned();
        copyFrom(other);
      }
      return *this;
    }
    Expr &operator=(Expr &&other) noexcept {
      if (this != &other) {
        freeOwned();
        moveFrom(std::move(other));
      }
      return *this;
    }
    ~Expr() { freeOwned(); }

  private:
    /// @brief 深拷贝 — 递归复制所有指针成员，避免共享所有权
    void copyFrom(const Expr &other) {
      kind = other.kind;
      line = other.line;
      strVal = other.strVal;
      numVal = other.numVal;
      ident = other.ident;
      prop = other.prop;
      indexKey = other.indexKey;
      // 深拷贝对象条目
      for (const auto &e : other.objEntries)
        objEntries.append({e.key, e.value ? new Expr(*e.value) : nullptr});
      // 深拷贝数组元素
      for (auto *e : other.arrItems)
        arrItems.append(e ? new Expr(*e) : nullptr);
      // 深拷贝函数调用参数
      funcCall.name = other.funcCall.name;
      for (auto *e : other.funcCall.args)
        funcCall.args.append(e ? new Expr(*e) : nullptr);
      // 深拷贝方法调用信息
      methodCall.objName = other.methodCall.objName;
      methodCall.methodName = other.methodCall.methodName;
      for (auto *e : other.methodCall.args)
        methodCall.args.append(e ? new Expr(*e) : nullptr);
      // 拷贝类名
      className = other.className;
      // 拷贝运算符和子树
      binOp = other.binOp;
      left = other.left ? new Expr(*other.left) : nullptr;
      right = other.right ? new Expr(*other.right) : nullptr;
    }
    /// @brief 移动构造 — 转移指针所有权，源对象指针置空
    void moveFrom(Expr &&other) {
      kind = other.kind;
      line = other.line;
      strVal = std::move(other.strVal);
      numVal = other.numVal;
      ident = std::move(other.ident);
      prop = std::move(other.prop);
      indexKey = std::move(other.indexKey);
      objEntries = std::move(other.objEntries);
      arrItems = std::move(other.arrItems);
      funcCall = std::move(other.funcCall);
      methodCall = std::move(other.methodCall);
      className = std::move(other.className);
      binOp = other.binOp;
      left = other.left;
      right = other.right;
      other.left = nullptr;
      other.right = nullptr;
    }
    /// @brief 释放所有指针成员，防止内存泄漏
    void freeOwned() {
      for (auto &e : objEntries) {
        delete e.value;
        e.value = nullptr;
      }
      for (auto *e : arrItems)
        delete e;
      arrItems.clear();
      for (auto *e : funcCall.args)
        delete e;
      funcCall.args.clear();
      for (auto *e : methodCall.args)
        delete e;
      methodCall.args.clear();
      delete left;
      left = nullptr;
      delete right;
      right = nullptr;
    }
  };

  /// @brief 调用语句：call("cls", "func", arg)
  struct CallStmt {
    Expr className;
    Expr funcName;
    Expr args;
  };

  /// @brief 赋值语句：name = expr 或 this.prop = expr
  struct AssignStmt {
    QString name;     ///< 变量名（thisProp 为空时有效）
    QString thisProp; ///< this 属性名（非空时表示 this.thisProp = val）
    Expr value;
  };

  /// @brief 索引赋值语句：obj["key"] = expr
  struct IndexAssignStmt {
    QString objName;
    QString key;
    Expr value;
  };

  /// @brief for 循环语句：for (var in arrayExpr) { body }
  struct ForStmt {
    QString varName;
    Expr arrayExpr;
    Block body;
  };

  /// @brief if 条件语句：if (cond) { then } else { else }
  struct IfStmt {
    Expr condition;
    Block thenBlock;
    Block elseBlock;
    bool hasElse = false;
  };

  /// @brief 语句 — 包含调用、赋值、类定义、循环、条件、返回等类型
  struct Stmt {
    enum Kind {
      kCall,
      kAssign,
      kIndexAssign,
      kFor,
      kIf,
      kExpr,
      kClassDef,
      kReturn
    } kind = kCall;
    CallStmt call;
    AssignStmt assign;
    IndexAssignStmt indexAssign;
    ForStmt forStmt;
    IfStmt ifStmt;
    Expr exprStmt;
    ClassDef classDef;
    Expr returnValue;
  };

  // ── 词法分析 ──
  QVector<Token> tokenize(const QString &source);
  void skipLineComment(const QString &source, int &pos);

  // ── 语法分析（递归下降） ──
  Token peek();
  Token advance();
  bool match(TokenType t);
  bool expect(TokenType t, const QString &msg);
  bool parseProgram(Block &block);
  bool parseBlock(Block &block);
  bool parseStmt(Stmt &stmt);
  bool parseCallStmt(CallStmt &cs);
  bool parseAssignStmt(AssignStmt &as);
  bool parseIndexAssignStmt(IndexAssignStmt &ias);
  bool parseForStmt(ForStmt &fs);
  bool parseIfStmt(IfStmt &is);
  bool parseClassDef(ClassDef &cd);
  bool parseReturnStmt(Expr &retVal);
  bool parseExpr(Expr &expr);
  bool parseAddSub(Expr &expr);
  bool parseMulDiv(Expr &expr);
  bool parsePrimary(Expr &expr);
  bool parseObject(Expr &expr);
  bool parseArray(Expr &expr);
  bool parseFuncCall(const QString &name, Expr &expr);
  bool parseMethodCallTail(const QString &objName, Expr &expr);
  bool parseMethodDef(MethodDef &md);

  // ── 解释执行 ──
  QJsonValue evalExpr(const Expr &expr);
  QJsonValue evalExprWithThis(const Expr &expr, const QJsonObject &thisObj);
  QJsonValue evalBinary(const Expr &expr);
  QJsonValue callBuiltin(const QString &name, const QVector<Expr *> &args);
  QJsonValue callFunMgr(const QString &cls, const QString &func,
                        const QJsonValue &args);
  void execStmt(const Stmt &stmt);
  void execBlock(const Block &block);
  void execBlockWithThis(const Block &block, const QJsonObject &thisObj,
                         QJsonValue &returnVal);

  // ── 类方法执行 ──
  QJsonValue execMethod(const MethodDef &method, const QJsonObject &thisObj,
                        const QJsonValue &callArgs);

public:
  // ── 未声明标识符验证 ──
  /// @brief 解析后调用，检查所有 kIdent 是否都已用 let 声明
  /// @return 错误信息列表，每个元素格式 "undefined variable 'xxx' at line N"
  QStringList validateUndeclaredIdents() const;
  void validateExprIdents(const Expr &expr, QStringList &errors,
                          const QSet<QString> &scopeVars) const;
  void validateStmtIdents(const Stmt &stmt, QStringList &errors,
                          const QSet<QString> &scopeVars) const;
  void validateBlockIdents(const Block &block, QStringList &errors,
                           const QSet<QString> &scopeVars) const;

private:
  QString m_error;                      ///< 错误信息
  QVector<Token> m_tokens;              ///< 词法分析结果
  int m_pos = 0;                        ///< 当前 token 位置
  QString m_scriptDir;                  ///< .ac 文件所在目录
  QString m_rootDir;                    ///< 项目根目录
  QString m_jsonPath;                   ///< 当前处理的 JSON 文件路径
  Block m_program;                      ///< 解析后的 AST 根节点
  QHash<QString, QJsonValue> m_vars;    ///< 局部变量表
  QHash<QString, QJsonValue> m_globals; ///< 全局变量表
  QSet<QString> m_declaredVars;         ///< 已用 let 声明的变量名
  QHash<QString, ClassDef> m_classes;   ///< 类定义表（类名 → ClassDef）
  QHash<QString, QJsonObject>
      m_instances; ///< 实例标识表（变量名 → 实例对象，用于跟踪 __class__）
  QJsonObject m_currentThis;    ///< 当前方法调用的 this 对象
  QJsonObject m_modifiedThis;   ///< execMethod 执行后修改过的 this 对象
  bool m_hasReturned = false;   ///< return 语句是否已执行
  QJsonValue m_returnValue;     ///< return 语句的返回值
  QStringList m_generatedFiles; ///< 本次执行生成的文件列表
  LogCallback m_logCallback;    ///< 日志回调
};