/**
 * @file schema_json_widget.h
 * @brief 通用 schema 驱动的 JSON 编辑器包装器
 *
 * 在一个 tab 内提供两种视图：
 *  - 代码编辑视图（CodeEditor，复用现有 JSON 高亮/校验）
 *  - 可视化编辑视图（SchemaFormEditor，按 $schema 渲染表单）
 *
 * 复用编辑器工具栏的「可视化/代码/还原/保存」按钮切换（与 JsonVueWidget 同模式）。
 * 两个视图共享同一份 JSON 对象，切换时自动同步；未知字段/格式信息保留（保真合并）。
 */

#pragma once

#include <QStackedWidget>
#include <QString>

#include "src/engine/schema_validator.h"

class CodeEditor;
class SchemaFormEditor;

/**
 * @class SchemaJsonWidget
 * @brief 带代码/可视化两种视图的 .json schema 编辑器包装器
 */
class SchemaJsonWidget : public QStackedWidget {
  Q_OBJECT

public:
  explicit SchemaJsonWidget(QWidget *parent = nullptr);

  /// 内部代码编辑器（供 MainDevMgr 集成：修改标记、保存等）
  CodeEditor *codeEditor() const { return m_editor; }

  /// 可视化（表单）编辑器
  SchemaFormEditor *visualEditor() const { return m_visual; }

  /// 当前是否为可视化模式
  bool isVisualMode() const { return currentIndex() == 1; }

  /// 聚焦当前显示的视图
  void focusActiveView();

  /// 切换到代码模式
  void switchToCode();

  /// 切换到可视化模式
  void switchToVisual();

  /// 切换模式
  void toggleMode();

  /// 设置 schema（供可视化表单渲染）
  void setSchema(const SchemaValidator &schema);

  /// 主题变化后重建边框样式（边框色随主题更新）
  void reloadStyle();

  /// 从代码编辑器内容加载到可视化编辑器
  void syncCodeToVisual();

  /// 从可视化编辑器内容写回代码编辑器
  void syncVisualToCode();

signals:
  /// 模式切换时发射
  void modeChanged(bool visualMode);

  /// 内容发生变化时发射（用于触发修改标记）
  void contentChanged();

private:
  CodeEditor *m_editor = nullptr;      ///< 代码编辑器
  SchemaFormEditor *m_visual = nullptr;  ///< 可视化（表单）编辑器
  bool m_syncing = false;              ///< 同步中标志，避免循环
};