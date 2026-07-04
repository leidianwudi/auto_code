/**
 * @file lighter_color.h
 * @brief 语法高亮统一颜色定义
 *
 * 所有高亮器（AcHighlighter、TplHighlighter、JsonHighlighter、
 * TypeScriptHighlighter）共用此文件中定义的颜色常量，确保视觉风格一致。
 */

#pragma once

#include <QColor>

/**
 * @namespace LighterColor
 * @brief 语法高亮颜色常量
 *
 * 按语义分类定义颜色，每个高亮器按需引用：
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
namespace LighterColor {

/// @brief 关键字（蓝色）— main, for, ${each}, JSON 键名
inline const QColor keyword{0x00, 0x00, 0xFF};

/// @brief 注释（灰色）
inline const QColor comment{0x80, 0x80, 0x80};

/// @brief 字符串（橙色）
inline const QColor string_{0xFF, 0x8C, 0x00};

/// @brief 数字（橙色）
inline const QColor number{0xFF, 0x8C, 0x00};

/// @brief 布尔值（红色）
inline const QColor boolean_{0xFF, 0x00, 0x00};

/// @brief 内置函数 / null（紫色）
inline const QColor builtin{0x80, 0x00, 0x80};

/// @brief 函数调用（紫色）— render(), write(), readJson() 等
inline const QColor call{0x80, 0x00, 0x80};

/// @brief 变量 / JSON 字符串值（绿色）
inline const QColor variable{0x00, 0x80, 0x00};

/// @brief 算术运算符（红色）
inline const QColor operator_{0xFF, 0x00, 0x00};

/// @brief 类型标注（青色）
inline const QColor type{0x26, 0x7F, 0x99};

/// @brief 装饰器（洋红色）
inline const QColor decorator{0xFF, 0x00, 0xFF};

} // namespace LighterColor