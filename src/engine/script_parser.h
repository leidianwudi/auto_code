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
///   - 函数调用：  call("cls", "func", arg)
///   - for 循环：  for (var in arrayExpr) { ... }
///   - 对象字面量：{ key: val, ... }
///   - 数组字面量：[item, ...]
///   - 属性访问：  obj.prop
///   - 字符串拼接：expr + expr
///
/// 变量规则：
///   - 所有变量必须先使用 let 声明才能使用
///   - 未声明的变量赋值会报错
///   - for 循环变量自动视为已声明
///
/// 内置函数：call / readJson / render / write / getCheckedFiles / merge /
/// basename
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
    TOK_IDENT,    ///< 标识符（变量名、函数名）
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

  /// @brief 表达式节点 — AST 中的表达式
  struct Expr {
    enum Kind {
      kString,     ///< 字符串字面量
      kNumber,     ///< 数字字面量
      kIdent,      ///< 变量引用
      kPropAccess, ///< 属性访问 obj.prop
      kObject,     ///< 对象字面量 { key: val }
      kArray,      ///< 数组字面量 [item, ...]
      kFuncCall,   ///< 函数调用 name(args)
      kBinary,     ///< 二元运算 left op right
    } kind = kString;
    int line = 0;                    ///< 源码行号（用于错误报告）
    QString strVal;                  ///< 字符串值
    double numVal = 0;               ///< 数值
    QString ident;                   ///< 标识符名
    QString prop;                    ///< 属性名（用于 kPropAccess）
    QVector<ObjectEntry> objEntries; ///< 对象条目
    QVector<Expr *> arrItems;        ///< 数组元素（指针）
    FuncCall funcCall;               ///< 函数调用信息
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
      // 拷贝基础字段
      kind = other.kind;
      line = other.line;
      strVal = other.strVal;
      numVal = other.numVal;
      ident = other.ident;
      prop = other.prop;
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
      // 拷贝运算符和子树
      binOp = other.binOp;
      left = other.left ? new Expr(*other.left) : nullptr;
      right = other.right ? new Expr(*other.right) : nullptr;
    }
    /// @brief 移动构造 — 转移指针所有权，源对象指针置空
    void moveFrom(Expr &&other) {
      // 移动基础字段
      kind = other.kind;
      line = other.line;
      strVal = std::move(other.strVal);
      numVal = other.numVal;
      ident = std::move(other.ident);
      prop = std::move(other.prop);
      // 移动容器（直接转移内部指针，无需深拷贝）
      objEntries = std::move(other.objEntries);
      arrItems = std::move(other.arrItems);
      funcCall = std::move(other.funcCall);
      // 转移树指针所有权
      binOp = other.binOp;
      left = other.left;
      right = other.right;
      // 源对象指针置空，防止双重释放
      other.left = nullptr;
      other.right = nullptr;
    }
    /// @brief 释放所有指针成员，防止内存泄漏
    void freeOwned() {
      // 释放对象条目中的值指针
      for (auto &e : objEntries) {
        delete e.value;
        e.value = nullptr;
      }
      // 释放数组元素指针
      for (auto *e : arrItems)
        delete e;
      arrItems.clear();
      // 释放函数调用参数指针
      for (auto *e : funcCall.args)
        delete e;
      funcCall.args.clear();
      // 释放子树指针
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

  /// @brief 赋值语句：name = expr
  struct AssignStmt {
    QString name;
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

  /// @brief 语句 — 包含调用、赋值、循环三种类型
  struct Stmt {
    enum Kind { kCall, kAssign, kIndexAssign, kFor, kIf, kExpr } kind = kCall;
    CallStmt call;
    AssignStmt assign;
    IndexAssignStmt indexAssign;
    ForStmt forStmt;
    IfStmt ifStmt;
    Expr exprStmt;
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
  bool parseExpr(Expr &expr);
  bool parseAddSub(Expr &expr);
  bool parseMulDiv(Expr &expr);
  bool parsePrimary(Expr &expr);
  bool parseObject(Expr &expr);
  bool parseArray(Expr &expr);
  bool parseFuncCall(const QString &name, Expr &expr);

  // ── 解释执行 ──
  QJsonValue evalExpr(const Expr &expr);
  QJsonValue evalBinary(const Expr &expr);
  QJsonValue callBuiltin(const QString &name, const QVector<Expr *> &args);
  QJsonValue callFunMgr(const QString &cls, const QString &func,
                        const QJsonValue &args);
  void execStmt(const Stmt &stmt);
  void execBlock(const Block &block);

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
  QString m_scriptDir;                  ///< main.ac 所在目录
  QString m_rootDir;                    ///< 项目根目录
  QString m_jsonPath;                   ///< 当前处理的 JSON 文件路径
  Block m_program;                      ///< 解析后的 AST 根节点
  QHash<QString, QJsonValue> m_vars;    ///< 局部变量表
  QHash<QString, QJsonValue> m_globals; ///< 全局变量表
  QSet<QString> m_declaredVars;         ///< 已用 let 声明的变量名
  QStringList m_generatedFiles;         ///< 本次执行生成的文件列表
  LogCallback m_logCallback;            ///< 日志回调
};