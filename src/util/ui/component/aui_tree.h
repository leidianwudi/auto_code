/**
 * @file aui_tree.h
 * @brief UI 公共列表/树控件工厂类
 *
 * 提供风格统一的列表树控件（交替行、可拖动列宽、选中/悬停态），
 * 供调试面板、输出面板等复用，避免各面板重复编写样式。
 */

#pragma once

#include <QString>

class QTreeWidget;

class AuiTree {
public:
  /// 列表样式表（QTreeWidget/QTreeView 通用）：交替行、选中/悬停态、行内边距、表头
  static QString listStyleSheet();

  /// 创建风格统一的列表树控件（交替行 + 可拖动列宽 + 应用列表样式）
  /// @note 调用方仍需自行设置列数、表头文字与列宽
  static QTreeWidget *createListTree();

private:
  AuiTree() = delete;
};