/**
 * @file ac_type.h
 * @brief .ac 脚本语言的类型定义集合
 *
 * 包含词法分析阶段的 Token 类型和语法分析阶段的 AST 节点类型。
 * 这些类型在词法分析器（AcLexer）、语法分析器（AcParser）和
 * 解释执行器（AcInterpreter）之间共享。
 *
 * 类型分类：
 * - 词法单元：TokenType, Token
 * - AST 节点：Block, Expr, Stmt, ClassDef, FuncCall, MethodCall 等
 */

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

/// @defgroup ac_type .ac 脚本类型定义
/// @{

// ── 前置声明 ──
struct Expr;

// ═════════════════════════════════════════════════════════════════════════════
//  词法单元
// ═════════════════════════════════════════════════════════════════════════════

/// @brief 访问级别
enum class AccessLevel { kPublic, kProtected, kPrivate };

/// @brief 复合赋值运算符类型
enum class CompoundOp {
  kNone,  ///< 无复合赋值
  kAdd,   ///< +=
  kSub,   ///< -=
  kMul,   ///< *=
  kDiv,   ///< /=
  kMod    ///< %=
};

/// @brief 词法单元类型
enum TokenType {
  TOK_EOF,              ///< 输入结束
  TOK_IDENT,            ///< 标识符（变量名、函数名、方法名）
  TOK_STRING,           ///< 字符串字面量 "hello"
  TOK_NUMBER,           ///< 数字字面量 123
  TOK_LBRACE,           ///< {
  TOK_RBRACE,           ///< }
  TOK_LPAREN,           ///< (
  TOK_RPAREN,           ///< )
  TOK_LBRACKET,         ///< [
  TOK_RBRACKET,         ///< ]
  TOK_COMMA,            ///< ,
  TOK_COLON,            ///< :
  TOK_DOT,              ///< .（属性访问）
  TOK_EQUALS,           ///< =（赋值）
  TOK_PLUS,             ///< +
  TOK_MINUS,            ///< -
  TOK_MUL,              ///< *
  TOK_DIV,              ///< /
  TOK_MOD,              ///< %（取模）
  TOK_PLUSEQ,           ///< +=
  TOK_MINUSEQ,          ///< -=
  TOK_MULEQ,            ///< *=
  TOK_DIVEQ,            ///< /=
  TOK_MODEQ,            ///< %=（取模赋值）
  TOK_PLUSPLUS,         ///< ++（自增）
  TOK_MINUSMINUS,       ///< --（自减）
  TOK_OR,               ///< ||（逻辑或）
  TOK_AND,              ///< &&（逻辑与）
  TOK_NOT,              ///< !（逻辑非）
  TOK_EQ,               ///< ==（等于）
  TOK_NEQ,              ///< !=（不等于）
  TOK_LT,               ///< <（小于）
  TOK_GT,               ///< >（大于）
  TOK_LTE,              ///< <=（小于等于）
  TOK_GTE,              ///< >=（大于等于）
  TOK_SEMI,             ///< ;（语句结束）
  TOK_FOR,              ///< for 关键字
  TOK_IN,               ///< in 关键字
  TOK_IF,               ///< if 关键字
  TOK_ELSE,             ///< else 关键字
  TOK_LET,              ///< let 关键字（变量声明）
  TOK_CLASS,            ///< class 关键字（类定义）
  TOK_FUNCTION,         ///< function 关键字（方法定义）
  TOK_NEW,              ///< new 关键字（实例化）
  TOK_THIS,             ///< this 关键字（当前实例引用）
  TOK_RETURN,           ///< return 关键字（返回值）
  TOK_TRUE,             ///< true 布尔字面量
  TOK_FALSE,            ///< false 布尔字面量
  TOK_SCOPE,            ///< ::（作用域解析）
  TOK_STATIC,           ///< static 关键字（静态成员）
  TOK_PUBLIC,           ///< public 访问修饰符
  TOK_PROTECTED,        ///< protected 访问修饰符
  TOK_PRIVATE,          ///< private 访问修饰符
  TOK_EXTENDS,          ///< extends 关键字（继承）
  TOK_OVERRIDE,         ///< override 关键字（重写标注）
  TOK_INTERFACE,        ///< interface 关键字（接口定义）
  TOK_IMPLEMENTS,       ///< implements 关键字（接口实现）
  TOK_SUPER,            ///< super 关键字（父类引用）
  TOK_EXPORT,           ///< export 关键字（导出）
  TOK_IMPORT,           ///< import 关键字（导入）
  TOK_FROM,             ///< from 关键字（import ... from "file"）
  TOK_NULL,             ///< null 关键字
  TOK_UNDEFINED,        ///< undefined 关键字
  TOK_WHILE,            ///< while 关键字
  TOK_BREAK,            ///< break 关键字
  TOK_CONTINUE,         ///< continue 关键字
  TOK_SWITCH,           ///< switch 关键字
  TOK_CASE,             ///< case 关键字
  TOK_DEFAULT,          ///< default 关键字
  TOK_ENUM,             ///< enum 关键字（枚举定义）
  TOK_CONSTRUCTOR,      ///< constructor 关键字（类构造函数）
  TOK_USING,            ///< using 关键字（显式资源管理）
  TOK_DISPOSE,          ///< dispose 关键字（资源释放方法）
  TOK_AS,               ///< as 关键字（导入别名）
  TOK_QUESTION,         ///< ?（三元运算符）
  TOK_TEMPLATE_STRING,  ///< 模板字符串 `...${...}...`
};

/// @brief 词法单元
struct Token {
  TokenType type = TOK_EOF;
  QString text;  ///< 原始文本
  int line = 0;  ///< 行号（用于错误报告）
};

// ═════════════════════════════════════════════════════════════════════════════
//  AST 节点类型
// ═════════════════════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════════════════════
//  类型系统
// ═════════════════════════════════════════════════════════════════════════════

/// @brief 类型表示 — 参数注解、返回类型、属性类型的统一表示
struct AcType {
  enum Kind {
    kNumber,     ///< 数字
    kString,     ///< 字符串
    kBool,       ///< 布尔
    kAny,        ///< 任意类型（不检查）
    kVoid,       ///< 无返回值
    kArray,      ///< 数组（elementType 表示元素类型）
    kClass,      ///< 用户自定义类（className 表示类名）
    kInterface,  ///< 接口类型（interfaceName 表示接口名）
    kNull,       ///< 未知/未推导
  } kind = kAny;

  QString className;                   ///< kClass 时有效
  QString interfaceName;               ///< kInterface 时有效
  QSharedPointer<AcType> elementType;  ///< kArray 时有效

  /// @name 内置类型工厂方法
  /// @{
  static AcType number() {
    AcType t;
    t.kind = kNumber;
    return t;
  }
  static AcType string() {
    AcType t;
    t.kind = kString;
    return t;
  }
  static AcType boolean() {
    AcType t;
    t.kind = kBool;
    return t;
  }
  static AcType any() {
    AcType t;
    t.kind = kAny;
    return t;
  }
  static AcType voidType() {
    AcType t;
    t.kind = kVoid;
    return t;
  }
  static AcType arrayOf(const AcType &elem) {
    AcType t;
    t.kind = kArray;
    t.elementType = QSharedPointer<AcType>::create(elem);
    return t;
  }
  static AcType classType(const QString &name) {
    AcType t;
    t.kind = kClass;
    t.className = name;
    return t;
  }
  static AcType interfaceType(const QString &name) {
    AcType t;
    t.kind = kInterface;
    t.interfaceName = name;
    return t;
  }
  /// @}
};

/// @brief 参数定义：带可选类型注解
struct ParamDef {
  QString name;  ///< 参数名
  AcType type;   ///< 类型（默认 Any，无注解时）
};

/// @brief 语句块 — 由 { } 包裹的一组语句
struct Block {
  struct Stmt;
  QVector<Stmt> stmts;
};

/// @brief 对象字面量中的键值对
struct ObjectEntry {
  QString key;
  AcType type;  ///< 属性类型（默认 Any，无注解时）
  std::unique_ptr<Expr> value;
  bool isStatic = false;                      ///< 是否为静态属性
  AccessLevel access = AccessLevel::kPublic;  ///< 访问级别

  ObjectEntry() = default;
  ObjectEntry(const ObjectEntry &other);
  ObjectEntry &operator=(const ObjectEntry &other);
  ObjectEntry(ObjectEntry &&) = default;
  ObjectEntry &operator=(ObjectEntry &&) = default;
};

/// @brief 函数调用表达式：name(arg1, arg2, ...)
struct FuncCall {
  QString name;
  std::vector<std::unique_ptr<Expr>> args;
};

/// @brief 方法调用表达式：obj.method(arg1, arg2, ...)
struct MethodCall {
  QString objName;                          ///< 对象变量名（用于简单变量）
  QString methodName;                       ///< 方法名
  std::vector<std::unique_ptr<Expr>> args;  ///< 参数列表
  std::unique_ptr<Expr>
      object;  ///< 对象表达式（用于链式访问，如 this.engine.start()，优先级高于 objName）

  MethodCall() = default;
  MethodCall(const MethodCall &other);
  MethodCall &operator=(const MethodCall &other);
  MethodCall(MethodCall &&) = default;
  MethodCall &operator=(MethodCall &&) = default;
};

/// @brief using 语句：using var = expr（离开作用域时自动调用 dispose()）
struct UsingStmt {
  QString varName;              ///< 变量名
  std::unique_ptr<Expr> value;  ///< 初始化表达式

  UsingStmt() = default;
  UsingStmt(const UsingStmt &other);
  UsingStmt &operator=(const UsingStmt &other);
  UsingStmt(UsingStmt &&) = default;
  UsingStmt &operator=(UsingStmt &&) = default;
};

/// @brief 方法定义：function name(params) { body }
struct MethodDef {
  QString name;
  QVector<ParamDef> params;  ///< 参数列表（带类型注解）
  AcType returnType;         ///< 返回类型（默认 Any，无注解时）
  Block body;
  bool isStatic = false;                      ///< 是否为静态方法
  AccessLevel access = AccessLevel::kPublic;  ///< 访问级别
  bool isOverride = false;                    ///< 是否重写父类方法
  bool isExported = false;                    ///< 是否导出（仅顶层函数有效）
};

/// @brief 接口方法签名
struct InterfaceMethod {
  QString name;
  QVector<ParamDef> params;
  AcType returnType;
};

/// @brief 接口定义：interface Name { function method(); }
struct InterfaceDef {
  QString name;
  QVector<InterfaceMethod> methods;
  QStringList baseInterfaces;  ///< 接口继承的父接口列表
  bool isExported = false;     ///< 是否导出
};

/// @brief 枚举成员定义：Name 或 Name = Value
struct EnumMember {
  QString name;           ///< 成员名
  QJsonValue value;       ///< 成员值（未指定时自动递增）
  bool hasValue = false;  ///< 是否显式指定了值
};

/// @brief 枚举定义：enum Name { A, B = 10, C }
struct EnumDef {
  QString name;
  QVector<EnumMember> members;
  bool isExported = false;  ///< 是否导出
};

/// @brief 类定义：class Name extends Base implements I1, I2 { ... }
/// 也支持 C++ 原生类（isNative=true），通过 FunMgr 路由方法调用
struct ClassDef {
  QString name;
  QString baseClass;       ///< 父类名（空表示无继承）
  QStringList interfaces;  ///< 实现的接口列表
  QVector<ObjectEntry> properties;
  QVector<MethodDef> methods;
  bool isNative = false;    ///< 是否为 C++ 原生类（非 AC 脚本定义）
  bool isExported = false;  ///< 是否导出
};

/// @brief 表达式节点 — AST 中的表达式
struct Expr {
  enum Kind {
    kString,        ///< 字符串字面量
    kNumber,        ///< 数字字面量
    kIdent,         ///< 变量引用
    kPropAccess,    ///< 属性访问 obj.prop
    kIndexAccess,   ///< 索引访问 obj[expr]（left=对象, right=索引表达式）
    kNull,          ///< null 字面量
    kUndefined,     ///< undefined 字面量
    kTernary,       ///< 三元运算 cond ? trueExpr : falseExpr（left=cond, right=trueExpr,
                    ///< operand=falseExpr）
    kObject,        ///< 对象字面量 { key: val }
    kArray,         ///< 数组字面量 [item, ...]
    kFuncCall,      ///< 函数调用 name(args)
    kMethodCall,    ///< 方法调用 obj.method(args)
    kNewInstance,   ///< new ClassName()
    kThis,          ///< this 关键字
    kBinary,        ///< 二元运算 left op right
    kUnary,         ///< 一元运算 !expr（逻辑非）
    kPreInc,        ///< 前置自增 ++expr
    kPreDec,        ///< 前置自减 --expr
    kPostInc,       ///< 后置自增 expr++
    kPostDec,       ///< 后置自减 expr--
    kBool,          ///< 布尔字面量 true/false
    kStaticAccess,  ///< 静态访问 ClassName::member
    kFuncExpr,      ///< 函数表达式 function(params): Type { body }
  } kind = kString;
  int line = 0;          ///< 源码行号（用于错误报告）
  QString strVal;        ///< 字符串值
  double numVal = 0;     ///< 数值
  bool boolVal = false;  ///< 布尔值（用于 kBool）
  QString ident;         ///< 标识符名
  QString prop;          ///< 属性名（用于 kPropAccess）
  std::unique_ptr<Expr>
      propObject;  ///< 链式属性访问的对象表达式（用于 kPropAccess 链式，优先级高于 ident）
  QVector<ObjectEntry> objEntries;              ///< 对象条目
  std::vector<std::unique_ptr<Expr>> arrItems;  ///< 数组元素
  FuncCall funcCall;                            ///< 函数调用信息
  MethodCall methodCall;                        ///< 方法调用信息
  QString className;                            ///< 类名（用于 kNewInstance）
  std::vector<std::unique_ptr<Expr>>
      constructorArgs;  ///< 构造参数（用于 kNewInstance 的 native 类）
  MethodDef funcExpr;   ///< 函数表达式（用于 kFuncExpr）
  enum BinaryOp {
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kOr,
    kAnd,
    kEq,
    kNeq,
    kLt,
    kGt,
    kLte,
    kGte
  } binOp = kAdd;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;

  /// 一元运算符类型（用于逻辑非 !expr）
  enum UnaryOp { kNot } unaryOp = kNot;
  std::unique_ptr<Expr> operand;  ///< 一元运算符的操作数

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
  void copyFrom(const Expr &other);
  void moveFrom(Expr &&other);
  void freeOwned();
};

/// @brief 调用语句：call("cls", "func", arg)
struct CallStmt {
  Expr className;
  Expr funcName;
  Expr args;
};

/// @brief 赋值语句：name = expr 或 this.prop = expr 或 ClassName::prop = expr
struct AssignStmt {
  QString name;
  QString thisProp;
  Expr value;
  bool isStatic = false;                      ///< 是否为静态属性赋值
  QString staticClassName;                    ///< 静态类名（isStatic=true 时有效）
  AcType typeAnnotation;                      ///< let 声明时的类型注解（如 let x: Number = 1）
  bool hasTypeAnnotation = false;             ///< 是否有类型注解
  bool isExported = false;                    ///< 是否导出
  CompoundOp compoundOp = CompoundOp::kNone;  ///< 复合赋值运算符
};

/// @brief 索引赋值语句：obj[expr] = value
struct IndexAssignStmt {
  Expr objectExpr;  ///< 被索引的对象表达式
  Expr indexExpr;   ///< 索引表达式
  Expr value;       ///< 赋值表达式
};

/// @brief 属性赋值语句：obj.prop = value
struct PropAssignStmt {
  Expr objectExpr;                            ///< 对象表达式（如变量名、this 等）
  QString prop;                               ///< 属性名
  Expr value;                                 ///< 赋值表达式
  CompoundOp compoundOp = CompoundOp::kNone;  ///< 复合赋值运算符
};

/// @brief for 循环语句：for (var in array) { body } 或 for (init; cond; update) { body }
struct ForStmt {
  QString varName;
  QString varType;  ///< for-in 变量类型注解（如 for (let item: String in arr)）
  Expr arrayExpr;
  Block body;

  // 标准 for 循环支持：for (init; condition; update) { body }
  bool isStandard = false;  ///< 是否为标准 for 循环（非 for-in）
  Block initBlock;          ///< 初始化语句块（如 let i = 0）
  Expr condition;           ///< 循环条件（如 i < n）
  Expr updateExpr;          ///< 更新表达式（如 i++）
};

/// @brief if 条件语句：if (cond) { then } else { else }
struct IfStmt {
  Expr condition;
  Block thenBlock;
  Block elseBlock;
  bool hasElse = false;
  bool isElseIf = false;                 ///< 是否为 else if 分支
  std::unique_ptr<IfStmt> elseIfBranch;  ///< else if 分支（链式结构）

  IfStmt() = default;
  IfStmt(const IfStmt &other);
  IfStmt &operator=(const IfStmt &other);
  IfStmt(IfStmt &&) = default;
  IfStmt &operator=(IfStmt &&) = default;
};

/// @brief 导入语句：import { A, B as C } from "file"
struct ImportStmt {
  QStringList names;               ///< 导入的符号名列表（原始名）
  QMap<QString, QString> aliases;  ///< 别名映射：原始名 → 别名（无别名时不含该键）
  QString filePath;                ///< 源文件路径（相对或绝对）
};

/// @brief while 循环语句：while (condition) { body }
struct WhileStmt {
  Expr condition;
  Block body;
};

/// @brief switch-case 分支
struct SwitchCase {
  Expr value;              ///< 匹配值（isDefault=true 时未使用）
  bool isDefault = false;  ///< 是否为 default 分支
  Block body;
};

/// @brief switch 语句：switch (expr) { case val: ... default: ... }
struct SwitchStmt {
  Expr expr;
  QVector<SwitchCase> cases;
};

/// @brief 语句 — 包含调用、赋值、类定义、循环、条件、返回等类型
struct Block::Stmt {
  enum Kind {
    kCall,
    kAssign,
    kIndexAssign,
    kPropAssign,
    kFor,
    kIf,
    kExpr,
    kClassDef,
    kInterfaceDef,
    kEnumDef,
    kFuncDef,
    kReturn,
    kImport,
    kWhile,
    kSwitch,
    kBreak,
    kContinue,
    kUsing
  } kind = kCall;
  CallStmt call;
  AssignStmt assign;
  IndexAssignStmt indexAssign;
  PropAssignStmt propAssign;
  ForStmt forStmt;
  IfStmt ifStmt;
  Expr exprStmt;
  ClassDef classDef;
  InterfaceDef interfaceDef;
  EnumDef enumDef;
  MethodDef funcDef;
  Expr returnValue;
  ImportStmt importStmt;
  WhileStmt whileStmt;
  SwitchStmt switchStmt;
  UsingStmt usingStmt;
};

/// @}