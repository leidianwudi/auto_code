/**
 * @file ac_compiler.h
 * @brief 字节码编译器 — 将已链接的 AST 编译为栈式字节码模块
 *
 * 作用域管理：kOpStmt 指令携带「语句块深度」，VM 在每条语句执行前把运行时
 * 作用域栈同步到该深度（push/pop 双向）。这是解释器 RAII 作用域的镜像：
 * break/continue/return 跳转跨块时，下一条语句的 kOpStmt 自动把作用域
 * 弹回目标深度，无需编译期穿插 push/pop 指令。
 */

#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "ac_diagnostic.h"
#include "ac_opcode.h"
#include "ac_type.h"

/// @brief 跳转槽：循环/switch 结构的待回填占位（break/continue/条件跳）。
/// 注意：不可命名为 slots/signal 等 Qt 关键字宏（QT_NO_KEYWORDS 未定义时冲突）。
struct AcJumpSlots {
  QVector<int> breakSlots;     ///< break 占位指令下标
  QVector<int> continueSlots;  ///< continue 占位指令下标
  QVector<int> ifFalseSlots;   ///< kJmpIfF 占位（条件假跳转）
  QVector<int> jmpSlots;       ///< 普通 kJmp 占位
};

/// @brief 字节码编译器
class AcCompiler {
public:
  /// @brief 将已链接的 program 编译为 AcModule
  /// @return true 成功；false 失败（error() 获取原因，同时产出 AC6001 结构化诊断）
  bool compile(const Block &program, AcModule &module);

  QString error() const { return m_error; }

  /// @brief 绑定结构化诊断收集器：编译失败时产出 AC6001 内部错误诊断
  void setDiagCollector(AcDiagCollector *d) { m_diagCollector = d; }

  /// @brief 类名 → 实例初始化单元下标（VM 的 NEW 使用）
  const QHash<QString, int> &classInitUnits() const { return m_classInitUnits; }
  /// @brief 类名 → 静态初始化单元下标
  const QHash<QString, int> &classStaticUnits() const { return m_classStaticUnits; }
  /// 类名 → 实例属性名列表（与初始化单元同序）
  const QHash<QString, QStringList> &classPropNames() const { return m_classPropNames; }

private:
  QSet<QString> m_classNames;  ///< 脚本声明的类名（编译函数体前收集，静态成员/属性定位用）
  /// 枚举扁平成员名（"Color.Blue"）→ 常量值（编译期直出 kConst）
  QHash<QString, accore::AcJsonValue> m_enumValues;

  // ── 槽位局部变量（A2）──
  QHash<QString, int> m_slotOf;       ///< 可槽化名字 → 槽号（参数 + 函数级 let/const）
  QSet<QString> m_shadowActive;       ///< 当前路径上被内层块重声明的名字（禁用槽位）
  QVector<QSet<QString>> m_shadowStack;  ///< 每层块声明的名字（块退出时清影子）
  int m_slotCount = 0;                ///< 当前单元已分配的槽数（= numLocals）

  /// 名字是否可槽化（本函数内且当前路径无影子声明）；-1 表示走名字查找
  int32_t slotFor(const QString &name) const;
  /// 发射名字加载：可槽化 → kLoadSlot，否则 kLoadName
  void emitLoadName(const QString &name, int line);
  /// 发射名字存储：可槽化 → kStoreSlot（写穿），否则 kStoreName
  void emitStoreName(const QString &name, int line);
  /// 发射名字复合写回（+= 等）：可槽化 → kCompoundSlot，否则 kCompoundName
  void emitCompoundName(const QString &name, CompoundOp op, int line);
  /// 重置槽位状态（每个函数单元开始前调用），并登记参数槽
  void beginUnitSlots(const MethodDef &md);
  // ── 编译上下文 ──
  AcModule *m_module = nullptr;
  AcFuncUnit *m_cur = nullptr;  ///< 当前正在生成的函数单元
  QString m_error;
  AcDiagCollector *m_diagCollector = nullptr;  ///< 结构化诊断收集器（编译失败时产出）

  /// 函数名 → 函数单元下标（方法名为 "类名.方法名"）
  QHash<QString, int> m_funcUnits;
  QHash<QString, int> m_classInitUnits;
  QHash<QString, int> m_classStaticUnits;
  QHash<QString, QStringList> m_classPropNames;

  // ── 跳转槽（回填避免嵌套污染）──
  QVector<AcJumpSlots> m_jumpStack;  ///< 循环/switch 结构栈（栈顶为最内层）

  /// 把当前栈顶结构的占位跳转回填
  void fillSlots(int breakTarget, int continueTarget, int loopElseTarget = -1);

  // ── 指令发射 ──
  void emitInstr(AcOpcode op, int32_t a = 0, int32_t b = 0, int32_t c = 0, int line = 0);
  int emitHere();
  int32_t constIdx(const accore::AcJsonValue &v);
  int32_t identOf(const QString &name);

  // ── 顶层收集 ──
  void collectTopLevel(const Block &program);
  /// 编译块到 m_cur：每条语句前 emit kOpStmt(line, depth)
  bool compileBlock(const Block &block, int depth);

  /// 编译内部错误出口：写 legacy 串（无行号 → 纯消息）并产出 AC6001 结构化诊断
  void reportCompileError(const QString &msg, int line);

  // ── 语句编译 ──
  bool compileStmt(const Block::Stmt &stmt, int depth);
  void compileAssignStmt(const Block::Stmt &s, int depth);
  void compileForStmt(const Block::Stmt &s, int depth);
  void compileIfStmt(const Block::Stmt &s, int depth);
  void compileWhileStmt(const Block::Stmt &s, int depth);
  void compileSwitchStmt(const Block::Stmt &s, int depth);
  void compileTryStmt(const Block::Stmt &s, int depth);

  // ── 表达式编译 ──
  void compileExpr(const Expr &e);
  void compileBinary(const Expr &e);
  void compileMethodCall(const Expr &e);
  void compileNew(const Expr &e);
  void compileStaticAccess(const Expr &e);
  void compilePropChain(const Expr &e);
  void compileIncDec(const Expr &e, double delta, bool post);
  void compileAssignExpr(const Expr &e);
  void compileFuncExprInner(const Expr &e);

  // ── 单元生成 ──
  int addFuncUnit(const QString &name, const MethodDef &md, bool isMethod);
  int addClassInitUnit(const ClassDef &cd, int depth);
  int addClassStaticUnit(const ClassDef &cd, int depth);
};