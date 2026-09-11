/**
 * @file schema_json_widget.h
 * @brief 通用 schema 驱动的 JSON 编辑器包装器
 *
 * 继承 CodeVisualSyncWidget（index 0 = 代码编辑器，index 1 = 可视化编辑器），
 * 模式切换/焦点/防循环同步由基类统一处理，这里只实现 .json 的数据搬运：
 *  - 代码 → 可视化：JSON5 解析 + 按 $schema 重载 schema 定义；
 *  - 可视化 → 代码：表单对象缩进序列化（保真合并由 SchemaFormEditor 内部处理）。
 *
 * 可视化页顶部带「模板」选择行，可从下拉选定 .schema.json 切换模板，
 * 空文件/无 $schema 也能进入可视化编辑。
 * 两个视图共享同一份 JSON 对象，切换时自动同步；未知字段/格式信息保留（保真合并）。
 */

#pragma once

#include <QDateTime>
#include <QString>

#include "src/engine/schema_validator.h"
#include "src/util/ui/code/code_visual_sync_widget.h"

class QComboBox;
class QLabel;
class SchemaFormEditor;

/**
 * @class SchemaJsonWidget
 * @brief 带代码/可视化两种视图的 .json schema 编辑器包装器
 */
class SchemaJsonWidget : public CodeVisualSyncWidget {
  Q_OBJECT

public:
  explicit SchemaJsonWidget(QWidget *parent = nullptr);

  /// 可视化（表单）编辑器
  SchemaFormEditor *visualEditor() const { return m_visual; }

  /// 设置当前 json 文件路径（解析相对 $schema 引用、扫描模板候选用；
  /// 须在 switchToVisual() 之前调用）
  void setSourceFilePath(const QString &path) { m_filePath = path; }

  /// 主题变化后重建边框样式（边框色随主题更新）
  void reloadStyle();

protected:
  QWidget *visualView() const override;
  void syncCodeToVisualImpl() override;
  void syncVisualToCodeImpl() override;

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
  SchemaFormEditor *m_visual = nullptr;  ///< 可视化（表单）编辑器
  QString m_filePath;                    ///< 当前 json 文件路径（模板扫描/schema 解析基准）
  QString m_currentRef;                  ///< 当前 $schema 引用（同步下拉框显示）
  bool m_schemaInit = false;  ///< 已执行过 schema 加载（含"无 schema"状态），用于跳过重复加载
  QDateTime m_schemaMtime;    ///< 已加载 schema 文件的修改时间（磁盘变化时触发重载）
  QComboBox *m_templateCombo = nullptr;  ///< 模板下拉框（可编辑，显示/选择 .schema.json）
  QLabel *m_templateHint = nullptr;      ///< 模板加载失败等状态提示
  SchemaValidator m_schema;              ///< 当前生效的 schema
};
