/**
 * @file light_color.h
 * @brief 语法高亮统一颜色定义
 *
 * 所有高亮器（LightAc、LightTpl、LightJson、
 * LightTs）共用此文件中定义的颜色，确保视觉风格一致。
 * 颜色从 SettingStore 读取，支持浅/深主题与自定义。
 */

#pragma once

#include <QColor>

#include "src/util/ui/setting_store.h"

/**
 * @namespace LightColor
 * @brief 语法高亮颜色
 *
 * 每个颜色均为函数，从 SettingStore 读取当前主题对应的颜色：
 * - keyword     : 关键字、控制标签、JSON 键名
 * - comment     : 注释
 * - string_     : 字符串字面量
 * - number      : 数字
 * - boolean_    : 布尔值
 * - builtin     : 内置函数、null
 * - call        : 函数调用
 * - variable    : 变量、JSON 字符串值
 * - operator_   : 算术运算符
 * - type        : 类型标注
 * - decorator   : 装饰器
 */
namespace LightColor {

/// @brief 关键字 — main, for, ${each}, JSON 键名
inline QColor keyword() { return SettingStore::ins().color(QStringLiteral("hl.keyword")); }

/// @brief 注释
inline QColor comment() { return SettingStore::ins().color(QStringLiteral("hl.comment")); }

/// @brief 字符串
inline QColor string_() { return SettingStore::ins().color(QStringLiteral("hl.string")); }

/// @brief 数字
inline QColor number() { return SettingStore::ins().color(QStringLiteral("hl.number")); }

/// @brief 布尔值
inline QColor boolean_() { return SettingStore::ins().color(QStringLiteral("hl.boolean")); }

/// @brief 内置函数 / null
inline QColor builtin() { return SettingStore::ins().color(QStringLiteral("hl.builtin")); }

/// @brief 函数调用 — render(), write(), readJson() 等
inline QColor call() { return SettingStore::ins().color(QStringLiteral("hl.call")); }

/// @brief 变量 / JSON 字符串值
inline QColor variable() { return SettingStore::ins().color(QStringLiteral("hl.variable")); }

/// @brief 算术运算符
inline QColor operator_() { return SettingStore::ins().color(QStringLiteral("hl.operator")); }

/// @brief 类型标注
inline QColor type() { return SettingStore::ins().color(QStringLiteral("hl.type")); }

/// @brief 装饰器
inline QColor decorator() { return SettingStore::ins().color(QStringLiteral("hl.decorator")); }

/// @brief 类/接口/枚举声明名
inline QColor className() { return SettingStore::ins().color(QStringLiteral("hl.classname")); }

/// @brief 函数声明名
inline QColor funcDecl() { return SettingStore::ins().color(QStringLiteral("hl.funcdecl")); }

}  // namespace LightColor
