/**
 * @file json_source_dialog.h
 * @brief .jsonsource 数据源编辑对话框
 *
 * 配置单条数据源：
 *   - 类型（静态数据源 / 动态数据源）
 *   - 说明（备注）
 *   - 静态：选项列表（显示文本 / 实际值）
 *   - 动态：复用 SelectSourcePanel（URL / 字段 / 分页等）
 */

#pragma once

#include <QDialog>

#include "json_source_model.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

/**
 * @class JsonSourceDialog
 * @brief .jsonsource 数据源编辑对话框
 */
class JsonSourceDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonSourceDialog(QWidget *parent = nullptr);

  /// 设置要编辑的数据源（编辑已有数据源时调用，保留 id）
  void setSource(const JsonSource &source);

  /// 设置 HTTP 请求参数（动态数据源"测试"按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  /// 获取编辑结果（调用方自行设置/保留 id）
  JsonSource source() const;

protected:
  /// 确定前校验（静态数据源必须填写函数 URL）
  void accept() override;

private slots:
  /// 类型切换：切换静态/动态配置区
  void onTypeChanged(int index);
  /// 添加选项（静态）
  void onAddOption();
  /// 删除选项（静态）
  void onRemoveOption();
  /// 选项上移（静态）
  void onOptionUp();
  /// 选项下移（静态）
  void onOptionDown();

private:
  void setupUI();
  void setupStaticPage();
  void setupDynamicPage();

  /// 从表格收集静态选项
  QVector<JsonSourceOption> collectOptions() const;
  /// 用静态选项填充表格
  void populateOptions(const QVector<JsonSourceOption> &options);
  /// 交换表格两行（a、b 行号）
  void swapRows(int a, int b);

  // ── 顶部控件 ──
  QComboBox *m_typeCombo = nullptr;   ///< 类型（静态/动态）
  QLineEdit *m_remarkEdit = nullptr;  ///< 说明（备注）
  QStackedWidget *m_stack = nullptr;  ///< 静态/动态配置区

  // ── 静态配置区 ──
  QLineEdit *m_staticUrlEdit = nullptr;   ///< 函数 URL（必填，用于生成函数名）
  QTableWidget *m_optionTable = nullptr;  ///< 选项表格（显示文本/实际值/类型）
  QPushButton *m_addOptionBtn = nullptr;
  QPushButton *m_removeOptionBtn = nullptr;
  QPushButton *m_optionUpBtn = nullptr;
  QPushButton *m_optionDownBtn = nullptr;

  // ── 动态配置区 ──
  class SelectSourcePanel *m_panel = nullptr;

  // ── 状态 ──
  QString m_id;  ///< 编辑时保留的 id（新增时为空）
  QString m_baseUrl;
  QString m_authHeader;
  QString m_postData;
};
