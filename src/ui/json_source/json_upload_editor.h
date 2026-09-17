/**
 * @file json_upload_editor.h
 * @brief .jsonupload 可视化编辑器面板
 *
 * 以表格形式管理 .jsonupload 文件中的多条上传预设：
 *   - 表格列：说明 / 上传地址 / 文件字段 / 张数 / 配置（摘要）
 *   - 支持添加 / 删除 / 上移 / 下移上传预设
 *   - 表格只读，双击数据行或点击「配置」按钮打开 JsonUploadDialog
 *     编辑单条预设
 */

#pragma once

#include <QWidget>

#include "json_upload_model.h"

class QPushButton;
class QTableWidget;

/**
 * @class JsonUploadEditor
 * @brief .jsonupload 文件可视化编辑面板
 */
class JsonUploadEditor : public QWidget {
  Q_OBJECT

public:
  explicit JsonUploadEditor(QWidget *parent = nullptr);
  ~JsonUploadEditor() override = default;

  /// 加载配置到界面
  void loadConfig(const JsonUploadConfig &config);

  /// 从界面收集配置
  JsonUploadConfig collectConfig() const;

  /// 缓存磁盘原始 .jsonupload 内容（写回时做保真合并，避免未知字段丢失）
  void setPreservedSource(const QString &src);

  /// 以界面当前配置为主、磁盘原文为底做保真合并，返回可写回磁盘的完整 JSON 对象
  QJsonObject collectMergedObject() const;

  /// 主题/颜色变化后重新应用样式表
  void reloadStyle();

signals:
  /// 配置发生变化时发射
  void configChanged();

private slots:
  /// 添加上传预设
  void onAddUpload();
  /// 删除上传预设
  void onRemoveUpload();
  /// 上移选中预设
  void onMoveUp();
  /// 下移选中预设
  void onMoveDown();
  /// 编辑选中预设（配置按钮）
  void onEditUpload();

private:
  /// 构建界面
  void setupUI();
  /// 应用样式
  void applyStyle();
  /// 创建「配置」按钮并连接到 onEditUpload（用于表格行）
  QPushButton *makeConfigButton();
  /// 刷新指定行的配置摘要文本
  void refreshSummary(int row);

  QTableWidget *m_table = nullptr;   ///< 上传预设列表表格
  QPushButton *m_addBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;
  QPushButton *m_moveUpBtn = nullptr;
  QPushButton *m_moveDownBtn = nullptr;

  QVector<JsonUpload> m_uploads;  ///< 上传预设数据（表格编辑态）

  // ── 状态 ──
  QJsonObject m_preserved;  ///< 磁盘原文（保真合并底）
  bool m_loading = false;   ///< 加载时抑制 configChanged
};

/// 上传预设表格列索引
enum JsonUploadTableCols {
  JUColRemark = 0,   ///< 说明
  JUColUrl,          ///< 上传地址
  JUColFileField,    ///< 文件字段名
  JUColMaxCount,     ///< 最多张数
  JUColConfig,       ///< 配置摘要
  JUColCount
};
