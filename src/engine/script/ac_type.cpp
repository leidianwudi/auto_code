/**
 * @file ac_type.cpp
 * @brief Expr AST 节点的内存管理实现
 *
 * copyFrom/moveFrom/freeOwned 从 ac_type.h 积出，
 * 避免头文件中包含非平凡实现，减少编译依赖和遗漏字段的风险。
 */

#include "ac_type.h"

// ═════════════════════════════════════════════════════════════════════════════
//  Expr 内存管理
// ═════════════════════════════════════════════════════════════════════════════

void Expr::copyFrom(const Expr &other) {
  kind = other.kind;
  line = other.line;
  strVal = other.strVal;
  numVal = other.numVal;
  boolVal = other.boolVal;
  ident = other.ident;
  prop = other.prop;
  propObject = other.propObject ? std::make_unique<Expr>(*other.propObject) : nullptr;
  for (const auto &e : other.objEntries) {
    ObjectEntry oe;
    oe.key = e.key;
    oe.type = e.type;
    oe.value = e.value ? std::make_unique<Expr>(*e.value) : nullptr;
    oe.isStatic = e.isStatic;
    objEntries.append(oe);
  }
  for (const auto &e : other.arrItems) arrItems.push_back(e ? std::make_unique<Expr>(*e) : nullptr);
  funcCall.name = other.funcCall.name;
  for (const auto &e : other.funcCall.args)
    funcCall.args.push_back(e ? std::make_unique<Expr>(*e) : nullptr);
  methodCall = other.methodCall;
  className = other.className;
  for (const auto &e : other.constructorArgs)
    constructorArgs.push_back(e ? std::make_unique<Expr>(*e) : nullptr);
  binOp = other.binOp;
  left = other.left ? std::make_unique<Expr>(*other.left) : nullptr;
  right = other.right ? std::make_unique<Expr>(*other.right) : nullptr;
  unaryOp = other.unaryOp;
  operand = other.operand ? std::make_unique<Expr>(*other.operand) : nullptr;
  funcExpr = other.funcExpr;
}

void Expr::moveFrom(Expr &&other) {
  kind = other.kind;
  line = other.line;
  strVal = std::move(other.strVal);
  numVal = other.numVal;
  boolVal = other.boolVal;
  ident = std::move(other.ident);
  prop = std::move(other.prop);
  propObject = std::move(other.propObject);
  objEntries = std::move(other.objEntries);
  arrItems = std::move(other.arrItems);
  funcCall = std::move(other.funcCall);
  methodCall = std::move(other.methodCall);
  className = std::move(other.className);
  constructorArgs = std::move(other.constructorArgs);
  binOp = other.binOp;
  left = std::move(other.left);
  right = std::move(other.right);
  unaryOp = other.unaryOp;
  operand = std::move(other.operand);
  funcExpr = std::move(other.funcExpr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  MethodCall 拷贝语义（Expr 完整定义后实现）
// ═════════════════════════════════════════════════════════════════════════════

MethodCall::MethodCall(const MethodCall &other)
    : objName(other.objName),
      methodName(other.methodName),
      object(other.object ? std::make_unique<Expr>(*other.object) : nullptr) {
  for (const auto &e : other.args) args.push_back(e ? std::make_unique<Expr>(*e) : nullptr);
}

MethodCall &MethodCall::operator=(const MethodCall &other) {
  if (this != &other) {
    objName = other.objName;
    methodName = other.methodName;
    args.clear();
    for (const auto &e : other.args) args.push_back(e ? std::make_unique<Expr>(*e) : nullptr);
    object = other.object ? std::make_unique<Expr>(*other.object) : nullptr;
  }
  return *this;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ObjectEntry 拷贝语义
// ═════════════════════════════════════════════════════════════════════════════

ObjectEntry::ObjectEntry(const ObjectEntry &other)
    : key(other.key),
      type(other.type),
      value(other.value ? std::make_unique<Expr>(*other.value) : nullptr),
      isStatic(other.isStatic),
      access(other.access),
      line(other.line) {}

ObjectEntry &ObjectEntry::operator=(const ObjectEntry &other) {
  if (this != &other) {
    key = other.key;
    type = other.type;
    value = other.value ? std::make_unique<Expr>(*other.value) : nullptr;
    isStatic = other.isStatic;
    access = other.access;
    line = other.line;
  }
  return *this;
}

// ═════════════════════════════════════════════════════════════════════════════
//  UsingStmt 拷贝语义
// ═════════════════════════════════════════════════════════════════════════════

UsingStmt::UsingStmt(const UsingStmt &other)
    : varName(other.varName), value(other.value ? std::make_unique<Expr>(*other.value) : nullptr) {}

UsingStmt &UsingStmt::operator=(const UsingStmt &other) {
  if (this != &other) {
    varName = other.varName;
    value = other.value ? std::make_unique<Expr>(*other.value) : nullptr;
  }
  return *this;
}

// ═════════════════════════════════════════════════════════════════════════════
//  IfStmt 拷贝语义（深拷贝 elseIfBranch 链）
// ═════════════════════════════════════════════════════════════════════════════

IfStmt::IfStmt(const IfStmt &other)
    : condition(other.condition),
      thenBlock(other.thenBlock),
      elseBlock(other.elseBlock),
      hasElse(other.hasElse),
      isElseIf(other.isElseIf),
      elseIfBranch(other.elseIfBranch ? std::make_unique<IfStmt>(*other.elseIfBranch) : nullptr) {}

IfStmt &IfStmt::operator=(const IfStmt &other) {
  if (this != &other) {
    condition = other.condition;
    thenBlock = other.thenBlock;
    elseBlock = other.elseBlock;
    hasElse = other.hasElse;
    isElseIf = other.isElseIf;
    elseIfBranch = other.elseIfBranch ? std::make_unique<IfStmt>(*other.elseIfBranch) : nullptr;
  }
  return *this;
}

void Expr::freeOwned() {
  objEntries.clear();
  arrItems.clear();
  funcCall.args.clear();
  methodCall.args.clear();
  methodCall.object.reset();
  constructorArgs.clear();
  left.reset();
  right.reset();
  operand.reset();
  propObject.reset();
}