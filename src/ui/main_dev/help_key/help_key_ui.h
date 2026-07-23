/**
 * @file help_key_ui.h
 * @brief 快捷键查看器 — 视图层
 *
 * 非模态对话框，使用 QTableWidget 展示应用中所有快捷键。
 * 由 HelpKeyMgr 控制器创建和管理。复用 AuiWindow 无边框窗口样式，
 * 保持与项目其他对话框（如 CreateUi）一致的外观。
 */

#pragma once

#include <QDialog>
#include <QList>

class QPushButton;
class QTableWidget;
class QWidget;

struct ShortcutEntry;

/**
 * @class HelpKeyUi
 * @brief 快捷键查看对话框（视图层）
 *
 * MVC 中的视图层，负责界面布局和数据展示。
 * 表格包含三列：分类、功能、快捷键。
 * 快捷键列使用等宽字体并以浅灰徽章样式显示。
 */
class HelpKeyUi : public QDialog {
  Q_OBJECT

public:
  explicit HelpKeyUi(QWidget *parent = nullptr);
  ~HelpKeyUi() override = default;

  /// 初始化界面布局
  void setupUI();

  /// 填充表格数据（按分类排序展示）
  void populateTable(const QList<ShortcutEntry> &entries);

private:
  /// 创建快捷键徽章控件（等宽字体 + 浅灰背景）
  QWidget *createKeyBadge(const QString &text);

  QTableWidget *m_table = nullptr;   ///< 快捷键表格
  QPushButton *m_closeBtn = nullptr;  ///< 关闭按钮
  QWidget *m_titleBar = nullptr;      ///< 自定义标题栏（用于 nativeEvent 拖拽）

  // ── 窗口事件 ──
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};
