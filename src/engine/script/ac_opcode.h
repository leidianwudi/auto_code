/**
 * @file ac_opcode.h
 * @brief 字节码指令集 — 栈式 VM 的指令与模块结构定义
 *
 * 指令格式：AcInstr{ AcOpcode op; int32_t a, b, c; }，三个整型操作数
 * （a 通常为常量池索引/标识符 id/行号，b/c 为辅助参数，未用置 0）。
 *
 * 运行时值栈元素为 accore::AcJsonValue。局部变量语义与解释器一致
 * （作用域链哈希，按名字查找），保证 Any 动态语义与闭包正确性。
 */

#pragma once

#include <QString>
#include <QVector>

#include "ac_type.h"                       // ClassDef / MethodDef / Block
#include "src/core/json/ac_json_value.h"  // accore::AcJsonValue

/// @brief 指令操作码
enum class AcOpcode : uint8_t {
  // ── 常量 ──
  kNil,       ///< push Null
  kTrue,      ///< push True
  kFalse,     ///< push False
  kConst,     ///< a=常量池索引 → push 常量
  // ── 名字（作用域链查找，语义与解释器 resolveVar/setVar 一致） ──
  kLoadName,     ///< a=identId → push 变量值（含 this/super 特判）
  kStoreName,    ///< a=identId — 栈值存入变量（含 const 检查）
  kDeclareName,  ///< a=identId, b=isConst — 在最新作用域声明变量
  kCompoundName, ///< a=identId, b=CompoundOp — pop new → 读旧值复合运算后存回, push 结果
  kLoadThis,     ///< push m_currentThis
  kStoreThisProp,///< a=propId — pop 值写 this.prop
  kDeclareUsing, ///< a=identId — pop 值声明 using 变量（登记 using 栈）
  // ── 静态成员 ──
  kLoadStatic,   ///< a=clsId, b=propId → push 静态属性值
  kStoreStatic,  ///< a=clsId, b=propId — pop 值写静态属性
  kCompoundStatic,   ///< a=clsId, b=propId, c=CompoundOp — pop new → 读旧静态值复合写回
  // ── 属性 / 索引 ──
  kGetProp,     ///< a=propId, b=optional — pop obj → push obj.prop
  kSetProp,     ///< a=propId — pop obj, newVal → obj.prop = newVal
  kCompoundProp,///< a=propId, b=CompoundOp — pop obj, new → 读旧复合写回, push 结果
  kIncProp,     ///< a=propId, b=delta(±1), c=postFlag — pop obj → 自增回写, push 旧/新值
  kGetIndex,    ///< b=optional — pop obj, idx → push obj[idx]（对象/数组/字符串语义）
  kSetIndex,    ///< pop obj, idx, newVal → obj[idx] = newVal
  kCompoundIndex,///< b=CompoundOp — pop obj, idx, new → 读旧复合写回, push 结果
  kIncIndex,    ///< b=delta(±1), c=postFlag — pop obj, idx → 自增回写, push 旧/新值
  kIncName,     ///< a=identId, b=delta(±1), c=postFlag — 变量自增回写, push 旧/新值
  kCompoundThisProp, ///< a=propId, b=CompoundOp — pop new → 读 this.prop 旧值复合写回
  // ── 运算（栈约定：先入栈为左操作数） ──
  kAdd, kSub, kMul, kDiv, kMod,
  kNeg, kNot,
  kEq, kNeq, kLt, kGt, kLte, kGte,
  // ── 跳转 ──
  kJmp,       ///< a=目标地址
  kJmpIfT,    ///< a=目标 — pop 条件, 真则跳
  kJmpIfF,    ///< a=目标 — pop 条件, 假则跳
  kNullJmpT,  ///< a=目标 — pop 条件, 非 null 则跳（空值合并用）
  // ── 字面量 ──
  kNewArray,   ///< a=元素个数 — pop a 个 → 组装数组 push
  kNewObject,  ///< a=键个数, b=常量池键起点 — pop a 个值（逆序）→ 按常量池键组装对象 push
  // ── 调用 ──
  kCallFunc,   ///< a=identId, b=实参数, c=0 普通 / 1 call("cls","fn",args) 形式
  kCallMethod, ///< a=propId, b=实参数, c=flags(bit0 chained/bit1 optional/bit2 JSON/bit3 super)
  kCallStatic, ///< a=clsId, b=propId, c=实参数 — 静态方法调用
  kCallSuper,  ///< a=propId, b=实参数 — super 方法调用
  kCallValue,  ///< a=实参数 — pop funcRef, args → 函数引用回调（高阶函数）
  kNew,        ///< a=clsId, b=实参数 — new 实例
  kFuncExpr,   ///< a=函数单元下标 — 创建函数引用并注册 m_functions, push FuncRef
  kRet,        ///< 设置返回标志并终止当前函数执行（返回上一帧）
  // ── 控制流 ──
  kEnterTry,   ///< a=try 表下标 — 注册活动 try 块
  kLeaveTry,   ///< a=try 表下标 — 注销最近的 try 块
  kCatchBegin, ///< a=try 表下标 — catch 入口：声明 catchVar + 进入 catch 状态
  kCatchEnd,   ///< a=try 表下标 — catch 出口
  kFinallyBegin,///< a=try 表下标 — finally 入口：保存待传播错误
  kFinallyEnd, ///< a=try 表下标 — finally 出口：恢复（传播）错误
  kForInInit,  ///< pop 数组/字符串/对象 → 建立迭代器帧（字符串转字符数组、对象转键数组）
  kForInNext,  ///< a=varNameId, b=endAddr — 迭代器取下一元素；无则跳 end
  kDeclareIterVar, ///< a=varNameId — 从迭代槽声明 for-in 变量（覆盖时处理旧值）
  kThrowValue, ///< pop 值作为错误消息经 m_error 通道传播
  // ── 栈操作 ──
  kPop,        ///< 丢弃栈顶
  kDup,        ///< 复制栈顶
  kSwap,       ///< 交换栈顶两个值
  kSwap3,      ///< 栈 [a,b,c]→[c,a,b]（三值循环左移）
  // ── 语句边界 ──
  kOpStmt,     ///< a=行号, b=语句块深度 — 调试钩子 + 运行时错误行号 + 作用域深度同步
};

/// @brief 编译期位置：指令地址（b/c 未用时置 0）
struct AcInstr {
  AcOpcode op = AcOpcode::kNil;
  int32_t a = 0;
  int32_t b = 0;
  int32_t c = 0;
  int32_t line = 0;  ///< 该指令对应的源码行号（错误定位 + 调试）
};

/// @brief try/catch 表项：错误经 m_error 通道在块边界拦截（语义与解释器 execTryStmt 一致）
struct AcTryEntry {
  int tryStart = -1;    ///< try 块起始指令地址
  int tryEnd = -1;      ///< try 块结束地址（不含 catch 前）
  int catchAddr = -1;   ///< catch 块地址（-1 表示无 catch）
  int finallyAddr = -1; ///< finally 块地址（-1 表示无 finally）
  int catchVarSlot = -1;///< 预留（catchVar 声明在运行时按名处理，此处未用）
  QString catchVar;     ///< catch 变量名（空则 catch 不绑定）
};

/// @brief 函数单元 — 一个可执行代码单元的字节码（顶层脚本体 / 方法体 / 函数体 / 类初始化）
struct AcFuncUnit {
  QString name;                    ///< 函数名/单元名（调试展示）
  int identId = 0;                 ///< 名字（用户函数名）驻留 id，0 表示无名
  bool isMethod = false;           ///< 是否方法（需要 thisObj 上下文）
  QVector<AcInstr> code;           ///< 指令流
  QVector<accore::AcJsonValue> constants;  ///< 常量池（仅可持久化标量/字符串）
  QVector<AcTryEntry> tryTable;    ///< try/catch 表
  QVector<QString> paramNames;     ///< 参数名（运行时按名声明，与解释器一致）
  QVector<accore::AcJsonValue> paramDefaults;  ///< 参数默认值（长度=paramNames；Null 表示无默认）
  int numLocals = 0;               ///< 预留：局部变量总数（后续静态槽位优化，v1 未用）
};

/// @brief 编译模块 — 整个程序的字节码（入口为函数单元下标 0）
struct AcModule {
  QVector<AcFuncUnit> funcs;          ///< 函数单元：0 = 顶层脚本体，其余按注册顺序
  int entry = 0;                      ///< 入口函数单元下标（恒为 0）
  QVector<QString> idents;            ///< 标识符驻留表（identId → 名字）
  QHash<QString, ClassDef> classes;   ///< 类定义（运行时初始化用，含原生类注册）
  /// 函数名 → 函数单元下标（顶层函数 / "类.方法" / lambda 名）
  QHash<QString, int> funcUnits;
  int version = 1;                    ///< 字节码格式版本（缓存失效用）
  QString sourceHash;                 ///< 源码内容哈希（缓存失效用）
};