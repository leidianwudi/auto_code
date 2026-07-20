/**
 * @file tpl_renderer.h
 * @brief 模板渲染器
 *
 * 遍历 AST 树，输出最终字符串。
 *
 * Renderer 依赖 TplEngine 提供：
 *   - resolvePath()      变量路径解析（user.name 等）
 *   - resolveFuncCall()  函数调用（str.toLowerCase() 等）
 *   - logCallback()      日志回调（用于 ${printLog(...)}）
 *   - setError()         错误设置
 *
 * 表达式求值策略（与旧实现兼容）：
 *   1. 内置函数：printLog(text)、fileExists(path)
 *   2. 循环变量：${this}、${.}
 *   3. 算术表达式：含四则运算符的表达式
 *   4. 普通变量路径：通过 resolvePath 解析
 *
 * 空行控制：
 *   Renderer 不做任何空行处理，所有空行控制都在 Parser 中完成。
 *   模板里几个换行符就输出几个换行符，不多不少。
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QSharedPointer>
#include <QString>

#include "tpl_ast.h"

class TplEngine;

namespace TplRenderer {

/// @brief 渲染 AST 树
///
/// @param nodes AST 节点列表
/// @param context JSON 数据上下文
/// @param engine 模板引擎（提供 resolvePath 等工具方法）
/// @return 渲染后的字符串，出错时返回空字符串（错误信息通过 engine.setError 设置）
QString render(const QList<QSharedPointer<TplAst::AstNode>> &nodes, const QJsonObject &context,
               const TplEngine &engine);

}  // namespace TplRenderer
