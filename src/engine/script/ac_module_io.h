/**
 * @file ac_module_io.h
 * @brief AcModule 二进制序列化 — 预编译缓存（阶段 3）的数据层
 *
 * 只持久化 VM 运行时必需的成员（指令、常量池、ident 表、try 表、函数单元、
 * 类精简元数据）。ClassDef 内嵌 AST（属性初始值表达式、方法体等）不可持久化，
 * 但字节码已把它们编码进 <init:%1>/<static:%1> 单元，故运行时类表只需
 * 名称/基类/原生标记/属性 isStatic 标志序/方法名与静态标志。
 */

#pragma once

#include <QByteArray>

#include "ac_opcode.h"

/// @brief AcModule 序列化/反序列化
namespace AcModuleIo {

/// @brief 序列化整个 AcModule（仅运行时子集）
bool save(const AcModule &module, QByteArray &out);

/// @brief 从字节流还原 AcModule；失败返回 false（格式错/版本不匹配）
bool load(const QByteArray &in, AcModule &module);

}  // namespace AcModuleIo