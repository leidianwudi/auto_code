/**
 * @file project_config_dialog.h
 * @brief project.acproj 项目配置可视化编辑对话框
 *
 * 编辑项目根标记文件 project.acproj：
 *   { "name": "shop", "meta": { "db": {...}, "goBasePath": "...", "webBasePath": "..." } }
 *
 * 只支持新格式；未知键（根 / meta / db 三层）原样保留，避免未来扩展字段丢失。
 */

#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLineEdit;
class QPushButton;
class QSpinBox;

class ProjectConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// filePath：project.acproj 绝对路径（不存在时按新建处理，项目名取目录名）
  explicit ProjectConfigDialog(const QString &filePath, QWidget *parent = nullptr);

protected:
  /// 确定：校验 → 写盘 → 关闭（校验/写盘失败时不关闭）
  void accept() override;

private:
  void setupUI();
  bool loadFile(QString *error);
  bool validate(QString *error) const;
  bool saveFile(QString *error);
  bool testConnection();

  QString m_filePath;
  // 未知键保真（分别对应根 / meta / db 三层）
  QJsonObject m_rootExtra;
  QJsonObject m_metaExtra;
  QJsonObject m_dbExtra;

  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_hostEdit = nullptr;
  QSpinBox *m_portSpin = nullptr;
  QLineEdit *m_userEdit = nullptr;
  QLineEdit *m_passwordEdit = nullptr;
  QLineEdit *m_databaseEdit = nullptr;
  QLineEdit *m_goBasePathEdit = nullptr;
  QLineEdit *m_webBasePathEdit = nullptr;
};
