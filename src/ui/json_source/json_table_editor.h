/**
 * @file json_table_editor.h
 * @brief .jsontable 可视化编辑器面板（内嵌表设计器）
 *
 * 可视化页 = JsonTableDesigner 六页签设计器本体（对齐 jsonvue「可视化页即编辑器」
 * 心智，无摘要卡/弹窗中转）：加载单表配置直达设计器，逐编辑经 configChanged
 * 广播写回代码侧（防抖由 CodeVisualSyncWidget 基类处理）。
 * 顶层未知键经 m_preserved 保真合并写回（一文件一表后旧 meta/tables 键不复活；
 * 旧多表格式解析失败时写回磁盘原文，避免可视化误覆盖）。
 */

#pragma once

#include <QJsonObject>
#include <QWidget>

#include "json_table_model.h"

class JsonTableDesigner;

/**
 * @class JsonTableEditor
 * @brief .jsontable 文件可视化编辑面板（内嵌设计器）
 */
class JsonTableEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonTableEditor(QWidget *parent = nullptr);
  ~JsonTableEditor() override = default;

  /// 加载单表配置到设计器（解析失败时传入空表，写回走磁盘原文保护）
  void loadConfig(const JsonTableTable &table);

  /// 从设计器收集当前表配置
  JsonTableTable collectTable() const;

  /// 缓存磁盘原始 .jsontable 内容（写回时做保真合并，避免未知字段丢失）
  void setPreservedSource(const QString &src);

  /// 以界面当前配置为主、磁盘原文为底做保真合并，返回可写回磁盘的完整 JSON 对象
  QJsonObject collectMergedObject() const;

  /// 设置当前文件路径（供设计器定位项目 meta 与数据源作用域）
  void setFilePath(const QString &path) { m_filePath = path; }

  /// 项目配置（project.acproj）变更后重载项目上下文（DB/路径/跨文件表清单）
  void reloadProjectContext();

  /// 主题/颜色变化后重新应用样式表（设计器自带样式，此处无需额外处理）
  void reloadStyle() {}

signals:
  /// 配置发生变化时发射
  void configChanged();

private:
  /// 重建项目上下文（proj meta + 同项目表清单），仅在文件路径变化时扫描
  void ensureProjectContext();

  JsonTableDesigner *m_designer = nullptr;  ///< 内嵌设计器（可视化页本体）

  // ── 状态 ──
  QJsonObject m_preserved;   ///< 磁盘原文（保真合并底）
  bool m_hasTable = false;   ///< 是否成功解析出表（旧多表格式失败时为 false）
  bool m_loading = false;    ///< 加载时抑制写回
  QString m_filePath;        ///< 当前 .jsontable 文件路径
  JsonTableConfig m_ctx;     ///< 项目上下文（proj meta + 跨文件表清单，按路径缓存）
  bool m_ctxBuilt = false;   ///< 项目上下文是否已按当前路径构建
};
