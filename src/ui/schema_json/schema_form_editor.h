/**
 * @file schema_form_editor.h
 * @brief 通用 schema 驱动的 JSON 可视化表单编辑器
 *
 * 无业务语义，纯「按 schema 渲染表单」：
 *  - 根据 `$schema` 指向的类定义，把 JSON 对象递归渲染成可编辑表单；
 *  - 控件按字段 type 映射（string/bool/int/double/enum/array/object/any）；
 *  - schema 的 description 渲染为行内副标题；required 标 *；
 *  - `any`/未知类型及 additionalProperties 用「内嵌 JSON 兜底框」编辑；
 *  - 编辑基于全量 JSON 对象 + JSON 路径，schema 不认识的键原样保留（保真合并）。
 *
 * 编辑结果通过 contentChanged() 通知外部（由 SchemaJsonWidget 写回代码编辑器）。
 */

#pragma once

#include <QJsonObject>
#include <QWidget>

#include "src/engine/schema_validator.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;

/**
 * @class SchemaFormEditor
 * @brief 通用 schema 表单编辑器
 */
class SchemaFormEditor : public QWidget {
  Q_OBJECT

public:
  explicit SchemaFormEditor(QWidget *parent = nullptr);
  ~SchemaFormEditor() override = default;

  /// 设置 schema 定义（拷贝；供渲染控件元数据），并按当前内容重建表单
  void setSchema(const SchemaValidator &schema);

  /// 应用新模板：切换 schema、把 $schema 引用写入根对象；
  /// 根对象为空时按必填字段生成默认值骨架（skeletonIfEmpty=true）
  void applySchema(const SchemaValidator &schema, const QString &schemaRef, bool skeletonIfEmpty);

  /// 加载一份 JSON 对象（含 $schema、schema 认识的字段与未知字段），并重建表单
  void loadJson(const QJsonObject &root);

  /// 返回编辑后的完整 JSON 对象（含保真的未知字段与 $ 元键）
  QJsonObject mergedObject() const { return m_root; }

  /// 是否有可用 schema（根类存在）
  bool hasSchema() const { return !m_schema.rootClassName().isEmpty(); }

signals:
  /// 任一字段变化（含结构增删）时发射
  void contentChanged();

private:
  // —— JSON 路径读写 ——
  static bool nodeAt(const QJsonObject &root, const QStringList &path, QJsonValue *out);
  void setNodeValue(const QStringList &path, const QJsonValue &value);
  void removeNodeValue(const QStringList &path);
  void insertArrayItem(const QStringList &arrayPath, int index, const QJsonValue &element);
  void removeArrayItem(const QStringList &arrayPath, int index);
  void moveArrayItem(const QStringList &arrayPath, int index, int delta);

  // —— 表单构建 ——
  void rebuild();  ///< 清空并重建整棵表单（结构性变化后用）
  QWidget *buildObjectForm(const QString &schemaClass, const QStringList &path, QString *titleOut,
                           QWidget *titleActions = nullptr, bool showTitle = true);
  QWidget *buildPropertyControl(const QString &name, const SchemaValidator::SchemaPropInfo &prop,
                                const QStringList &parentPath, const QStringList &required);
  QWidget *makeRawJsonFallback(const QStringList &path, const QJsonValue &cur);
  void fillRequiredSkeleton(const QString &cls, const QStringList &path);  ///< 递归生成必填字段骨架

  QStringList propertyPath(const QStringList &parentPath, const QString &name) const {
    QStringList p = parentPath;
    p.append(name);
    return p;
  }

private:
  SchemaValidator m_schema;      ///< schema 定义（渲染元数据）
  QJsonObject m_root;            ///< 编辑中的全量 JSON
  QWidget *m_content = nullptr;  ///< 滚动区内承载表单的内容控件
  bool m_busy = false;           ///< 正在重建/加载，屏蔽信号
  QByteArray m_renderHash;       ///< 已渲染表单对应的 JSON 指纹（内容未变则跳过重建）
};