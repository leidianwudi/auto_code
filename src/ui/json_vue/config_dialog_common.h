/**
 * @file config_dialog_common.h
 * @brief json_vue 配置对话框公共工具
 *
 * 三个配置对话框（ButtonConfigDialog / ColumnStyleDialog / QueryStyleDialog）的
 * 重复样板统一封装：
 * - 无边框框架搭建（标题栏 + 内容区 + 确定/取消按钮 + 窗口框架应用）
 * - QComboBox 按 userData 选中（findData + setCurrentIndex 样板）
 * - 数值可编辑下拉框构建与读取（Int/Float 的最小值/最大值）
 * - 表格行删除按钮（自动定位按钮所在行并删除）
 * - 表格单元格紧凑按钮样式
 */

#pragma once

#include <QMargins>
#include <QVariant>

class QComboBox;
class QDialog;
class QPushButton;
class QTableWidget;
class QVBoxLayout;
class QWidget;

/// 配置对话框框架信息（beginConfigDialog 创建，finishConfigDialog 完成后失效）
struct ConfigDialogFrame {
  QWidget *titleBar = nullptr;           ///< 自定义标题栏
  QWidget *contentWidget = nullptr;      ///< 内容区域（含 contentLayout）
  QVBoxLayout *contentLayout = nullptr;  ///< 内容区主布局（业务控件添加到这里）
};

/**
 * @brief 搭建配置对话框框架（第一步）：无边框 + 自定义标题栏 + 内容区布局
 *
 * 使用方式：
 *   auto frame = beginConfigDialog(this, QStringLiteral("标题"));
 *   // ... 向 frame.contentLayout 添加业务控件 ...
 *   finishConfigDialog(this, frame);  // 添加确定/取消按钮并应用窗口框架
 */
ConfigDialogFrame beginConfigDialog(QDialog *dialog, const QString &title,
                                    const QMargins &margins = QMargins(8, 8, 8, 8),
                                    int spacing = 6);

/**
 * @brief 完成配置对话框框架（第二步）：添加确定/取消按钮、应用窗口框架
 *
 * 确定 → QDialog::accept，取消 → QDialog::reject（AuiButton::createDialogButtons 统一样式）
 */
void finishConfigDialog(QDialog *dialog, const ConfigDialogFrame &frame);

/// QComboBox 按 userData 选中项；未找到时选中 fallback（默认 -1 表示不改变当前项）
void comboSelectData(QComboBox *combo, const QVariant &data, int fallback = -1);

/// 创建可编辑数值下拉框（预设值列表 + 当前值，未匹配预设时填入编辑框）
QComboBox *createNumericCombo(QWidget *parent, const QList<double> &presetValues, double current);

/// 读取可编辑数值下拉框的当前值（优先 userData，其次编辑框文本）
double numericComboValue(const QComboBox *combo, double fallback = 0);

/// 创建表格行删除按钮：点击时自动定位按钮所在行（按 column 匹配）并删除该行
QPushButton *makeTableDeleteButton(QTableWidget *table, int column, QWidget *parent = nullptr);

/// 创建紧凑小按钮（适合表格单元格内使用，主题色随 AuiStyle 变化）
QPushButton *makeCompactButton(const QString &text, QWidget *parent = nullptr);
