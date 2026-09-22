/**
 * @file json_global_enum_dialog.h
 * @brief 全局枚举编辑对话框
 *
 * 配置单条全局枚举：
 *   - 名称（snake_case，后端枚举名 = Enum + 帕斯卡(name)）
 *   - 说明（备注）
 *   - 选项列表（键名 / 显示文本 / 实际值 / 值类型）
 */

#pragma once

#include <QDialog>

#include "json_global_enum_model.h"

class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QTableWidget;

/**
 * @class JsonGlobalEnumDialog
 * @brief .jsonglobalenum 单条枚举编辑对话框
 */
class JsonGlobalEnumDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonGlobalEnumDialog(QWidget *parent = nullptr);

  /// 设置要编辑的枚举（编辑已有枚举时调用，保留 id）
  void setEnum(const JsonGlobalEnum &e);

  /// 获取编辑结果（调用方自行设置/保留 id）
  JsonGlobalEnum enumData() const;

protected:
  /// 确定前校验（名称必填、选项完整）
  void accept() override;

private slots:
  /// 添加选项
  void onAddOption();
  /// 删除选项
  void onRemoveOption();
  /// 选项上移
  void onOptionUp();
  /// 选项下移
  void onOptionDown();

private:
  void setupUI();

  /// 从表格收集选项（行内校验键名/显示文本/实际值必填）
  QVector<JsonGlobalEnumOption> collectOptions(QString *error) const;
  /// 用选项填充表格
  void populateOptions(const QVector<JsonGlobalEnumOption> &options);
  /// 交换表格两行（a、b 行号）
  void swapRows(int a, int b);

  QLineEdit *m_nameEdit = nullptr;        ///< 枚举名称（如 is_enable）
  QLineEdit *m_remarkEdit = nullptr;      ///< 说明（备注）
  QTableWidget *m_optionTable = nullptr;  ///< 选项表格（键名/显示文本/实际值/类型）
  QPushButton *m_addOptionBtn = nullptr;
  QPushButton *m_removeOptionBtn = nullptr;
  QPushButton *m_optionUpBtn = nullptr;
  QPushButton *m_optionDownBtn = nullptr;

  QString m_id;  ///< 编辑时保留的 id（新增时为空）
};

/// 选项表格列索引
enum JsonGlobalEnumOptionCols {
  GEOColKey = 0,    ///< 键名（TS 成员名）
  GEOColLabel,      ///< 显示文本
  GEOColValue,      ///< 实际值
  GEOColValueType,  ///< 值类型（cellWidget 下拉框）
  GEOColCount
};
