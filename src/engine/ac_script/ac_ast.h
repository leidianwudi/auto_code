#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

/// @defgroup ac_ast .ac 脚本 AST 类型定义
/// @{

// ── 前置声明 ──
struct Expr;

// ═════════════════════════════════════════════════════════════════════════════
//  词法单元
// ═════════════════════════════════════════════════════════════════════════════

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

// ═════════════════════════════════════════════════════════════════════════════
//  AST 节点类型
// ═════════════════════════════════════════════════════════════════════════════

/// @brief 语句块 — 由 { } 包裹的一组语句
struct Block {
  struct Stmt;
  QVector<Stmt> stmts;
};

/// @brief 对象字面量中的键值对
struct ObjectEntry {
  QString key;
  Expr *value = nullptr;
};

/// @brief 函数调用表达式：name(arg1, arg2, ...)
struct FuncCall {
  QString name;
  QVector<Expr *> args;
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
  QStringList params;
  Block body;
};

/// @brief 类定义：class Name { let prop = val; function method(args) { ... } }
struct ClassDef {
  QString name;
  QVector<ObjectEntry> properties;
  QVector<MethodDef> methods;
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
  enum BinaryOp { kAdd, kSub, kMul, kDiv } binOp = kAdd;
  Expr *left = nullptr;
  Expr *right = nullptr;

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
  void copyFrom(const Expr &other) {
    kind = other.kind;
    line = other.line;
    strVal = other.strVal;
    numVal = other.numVal;
    ident = other.ident;
    prop = other.prop;
    indexKey = other.indexKey;
    for (const auto &e : other.objEntries)
      objEntries.append({e.key, e.value ? new Expr(*e.value) : nullptr});
    for (auto *e : other.arrItems)
      arrItems.append(e ? new Expr(*e) : nullptr);
    funcCall.name = other.funcCall.name;
    for (auto *e : other.funcCall.args)
      funcCall.args.append(e ? new Expr(*e) : nullptr);
    methodCall.objName = other.methodCall.objName;
    methodCall.methodName = other.methodCall.methodName;
    for (auto *e : other.methodCall.args)
      methodCall.args.append(e ? new Expr(*e) : nullptr);
    className = other.className;
    binOp = other.binOp;
    left = other.left ? new Expr(*other.left) : nullptr;
    right = other.right ? new Expr(*other.right) : nullptr;
  }
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
  QString name;
  QString thisProp;
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
struct Block::Stmt {
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

/// @}