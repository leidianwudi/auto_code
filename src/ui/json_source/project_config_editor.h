/**
 * @file project_config_editor.h
 * @brief project.acproj 项目配置可视化编辑页
 *
 * 嵌入 ProjectAcprojWidget 的可视化页（index 1），与代码页双向同步。
 * 编辑对象为 project.acproj 的根 JSON：
 *   { "name": "shop", "meta": { "db": {...}, "goBasePath": "...", "webBasePath": "..." } }
 *
 * 只支持新格式；未知键（根 / meta / db 三层）在 loadFromObject 时缓存、
 * collectObject 时原样合回，避免未来扩展字段丢失。
 */

#pragma once

#include <QJsonObject>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @class ProjectConfigEditor
 * @brief project.acproj 项目配置表单页
 */
class ProjectConfigEditor : public QWidget {
  Q_OBJECT

public:
  explicit ProjectConfigEditor(QWidget *parent = nullptr);

  /// 从根 JSON 对象加载（解析失败/空对象时按空配置处理）
  void loadFromObject(const QJsonObject &root);

  /// 收集表单内容为根 JSON 对象（未知键合回；db 三件套缺一不入 meta.db）
  QJsonObject collectObject() const;

  /// 用当前表单的数据库配置测试连接
  bool testConnection();

signals:
  /// 任一字段变化时发射（驱动代码页写回）
  void contentChanged();

private:
  void setupUI();

  // 未知键保真（分别对应根 / meta / db 三层）
  QJsonObject m_rootExtra;
  QJsonObject m_metaExtra;
  QJsonObject m_dbExtra;
  bool m_loading = false;  ///< loadFromObject 期间抑制 contentChanged

  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_hostEdit = nullptr;
  QSpinBox *m_portSpin = nullptr;
  QLineEdit *m_userEdit = nullptr;
  QLineEdit *m_passwordEdit = nullptr;
  QLineEdit *m_databaseEdit = nullptr;
  QLineEdit *m_goBasePathEdit = nullptr;
  QLineEdit *m_webBasePathEdit = nullptr;
};
