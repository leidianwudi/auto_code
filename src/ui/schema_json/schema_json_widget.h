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

#include <QDateTime>
#include <QStackedWidget>
#include <QString>

#include "src/engine/schema_validator.h"

class CodeEditor;
class QComboBox;
class QLabel;
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

  /// 设置当前 json 文件路径（解析相对 $schema 引用、扫描模板候选用；
  /// 须在 switchToVisual() 之前调用）
  void setSourceFilePath(const QString &path) { m_filePath = path; }

  /// 主题变化后重建边框样式（边框色随主题更新）
  void reloadStyle();

  /// 从代码编辑器内容加载到可视化编辑器（含按 $schema 重新加载 schema）
  void syncCodeToVisual();

  /// 从可视化编辑器内容写回代码编辑器
  void syncVisualToCode();

signals:
  /// 模式切换时发射
  void modeChanged(bool visualMode);

  /// 内容发生变化时发射（用于触发修改标记）
  void contentChanged();

private:
  /// 从代码内容提取 $schema 引用值；找不到返回空串（兼容带/不带引号等写法）
  static QString extractSchemaRef(const QString &content);

  /// 解析 $schema 引用为绝对路径（委托 PathResolver::resolveSchemaPath，规则全局统一）
  QString resolveSchemaPathFor(const QString &schemaRef) const;

  /// 模板行右侧提示文本：空串清除，非空以错误色显示
  void setTemplateError(const QString &text);

  /// 按引用加载 schema；成功更新 m_schema 与表单，失败显示提示
  void reloadSchemaFor(const QString &ref);

  /// 扫描候选模板（json 所在目录 + 项目根 file 目录）填充下拉框
  void refreshTemplateCombo(const QString &currentRef);

  /// 用户选定模板：加载 schema、写回 $schema、空文档生成必填骨架
  void applyTemplateRef(const QString &ref);

private:
  CodeEditor *m_editor = nullptr;        ///< 代码编辑器
  SchemaFormEditor *m_visual = nullptr;  ///< 可视化（表单）编辑器
  bool m_syncing = false;                ///< 同步中标志，避免循环
  QString m_filePath;                    ///< 当前 json 文件路径（模板扫描/schema 解析基准）
  QString m_currentRef;                  ///< 当前 $schema 引用（同步下拉框显示）
  bool m_schemaInit = false;  ///< 已执行过 schema 加载（含"无 schema"状态），用于跳过重复加载
  QDateTime m_schemaMtime;    ///< 已加载 schema 文件的修改时间（磁盘变化时触发重载）
  QComboBox *m_templateCombo = nullptr;  ///< 模板下拉框（可编辑，显示/选择 .schema.json）
  QLabel *m_templateHint = nullptr;      ///< 模板加载失败等状态提示
  SchemaValidator m_schema;              ///< 当前生效的 schema
};