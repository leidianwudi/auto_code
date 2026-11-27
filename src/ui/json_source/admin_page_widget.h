/** @file admin_page_widget.h
 * @brief 页面说明与自定义保存接口设置；字段展示、编辑及筛选统一由字段呈现管理。
 */
#pragma once
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QComboBox;
class QPlainTextEdit;

/** 仅编辑页面级说明与自定义保存接口，不再维护另一套字段配置。 */
class AdminPageWidget : public QWidget {
  Q_OBJECT
public:
  explicit AdminPageWidget(QWidget *parent = nullptr);
  /// 回填配置并阻止程序性加载触发文件修改。
  void setConfig(const QJsonObject &config);
  /// 未修改时返回原对象，未知属性保真。
  QJsonObject config() const;
  /// 校验自定义保存接口设置，逐字段的动态绑定由字段设置面板负责。
  bool validate(QString *error) const;
signals:
  void configChanged();
private:
  /// 广播实际编辑变化。
  void changed();
  /// 收集控件，用于识别未修改及修改后又恢复原值。
  QJsonObject collectConfig() const;
  bool m_loading = false;
  QJsonObject m_original;
  QJsonObject m_baseline;
  QCheckBox *m_customSave = nullptr;
  QWidget *m_customSaveForm = nullptr;
  QLineEdit *m_saveUrl = nullptr;
  QLineEdit *m_keyField = nullptr;
  QLineEdit *m_editFields = nullptr;
  QLineEdit *m_switchField = nullptr;
  QPlainTextEdit *m_notice = nullptr;
  QComboBox *m_noticeType = nullptr;
  QCheckBox *m_noticeClosable = nullptr;
};
