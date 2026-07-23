/**
 * @file help_key_ui.cpp
 * @brief 快捷键查看器 — 视图层实现
 */

#include "help_key_ui.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "help_key_model.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

HelpKeyUi::HelpKeyUi(QWidget *parent) : QDialog(parent) {}

// ════════════════════════════════════════════════════════════
//  setupUI — 初始化界面布局
// ════════════════════════════════════════════════════════════

void HelpKeyUi::setupUI() {
  setWindowTitle(QStringLiteral("快捷键"));
  resize(700, 500);

  // ── 无边框对话框（保留 Qt::Dialog 标志，复用 AuiWindow 统一样式） ──
  AuiWindow::setupFramelessDialog(this);

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏（AC 图标 + 标题文字 + 窗口控制按钮）
  // ════════════════════════════════════════════════════════════
  TitleBarOptions opts;
  opts.title = QStringLiteral("快捷键");
  opts.showMinButton = true;
  opts.showMaxButton = true;
  opts.showCloseButton = true;
  // 非模态对话框：关闭按钮调用 QWidget::close() 而非 reject()
  opts.closeRejectsDialog = false;
  auto tb = AuiWindow::createTitleBar(this, opts);
  m_titleBar = tb.titleBar;

  // ════════════════════════════════════════════════════════════
  //  内容区域
  // ════════════════════════════════════════════════════════════
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(16, 12, 16, 16);
  contentLayout->setSpacing(12);

  // ── 快捷键表格 ──
  m_table = new QTableWidget(this);
  m_table->setColumnCount(3);
  m_table->setHorizontalHeaderLabels(
      {QStringLiteral("分类"), QStringLiteral("功能"), QStringLiteral("快捷键")});
  m_table->setFont(AuiStyle::createEditorFont());
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setFocusPolicy(Qt::NoFocus);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(false);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_table->setShowGrid(true);

  // 表格统一样式（背景色 / 文字色 / 边框色 / 表头样式）
  QString tableStyle = QStringLiteral(
      "QTableWidget { background: %1; color: %2; border: 1px solid %3; "
      "gridline-color: %3; }"
      "QHeaderView::section { background: %4; color: %2; padding: 6px; "
      "border: none; border-right: 1px solid %3; border-bottom: 1px solid %3; "
      "font-weight: bold; }")
      .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
           AuiStyle::borderColor().name(), AuiStyle::background().name());
  m_table->setStyleSheet(tableStyle);

  contentLayout->addWidget(m_table, 1);

  // ── 关闭按钮（居中显示） ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  m_closeBtn = new QPushButton(QStringLiteral("关闭"), this);
  AuiButton::applyDialogButtonStyle(m_closeBtn);
  m_closeBtn->setMinimumWidth(90);
  btnLayout->addWidget(m_closeBtn);
  btnLayout->addStretch();
  contentLayout->addLayout(btnLayout);

  // ── 信号连接 ──
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);

  // ── 应用窗口框架（标题栏 + 内容 + 1px 边框） ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // ── Win32 边框拉伸（支持最大化按钮） ──
  AuiWindow::enableWin32Resize(this);

  // applyWindowFrame 会重置内容布局的边距和间距，需要恢复
  contentLayout->setContentsMargins(16, 12, 16, 16);
  contentLayout->setSpacing(12);
}

// ════════════════════════════════════════════════════════════
//  populateTable — 填充表格数据（按分类排序展示）
// ════════════════════════════════════════════════════════════

void HelpKeyUi::populateTable(const QList<ShortcutEntry> &entries) {
  m_table->setRowCount(entries.size());

  QFont tableFont = AuiStyle::createEditorFont();

  for (int i = 0; i < entries.size(); ++i) {
    const auto &entry = entries.at(i);

    // ── 分类列（粗体，居中） ──
    auto *catItem = new QTableWidgetItem(entry.category);
    QFont catFont = tableFont;
    catFont.setBold(true);
    catItem->setFont(catFont);
    catItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(i, 0, catItem);

    // ── 功能列（左对齐） ──
    auto *descItem = new QTableWidgetItem(entry.description);
    descItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->setItem(i, 1, descItem);

    // ── 快捷键列（徽章样式控件） ──
    m_table->setCellWidget(i, 2, createKeyBadge(entry.keySequence));
  }

  // ── 列宽设置 ──
  m_table->setColumnWidth(0, 120);
  m_table->setColumnWidth(1, 280);
  m_table->setColumnWidth(2, 200);

  // ── 行高（徽章需要足够空间） ──
  m_table->verticalHeader()->setDefaultSectionSize(34);
}

// ════════════════════════════════════════════════════════════
//  createKeyBadge — 创建快捷键徽章控件（等宽字体 + 浅灰背景）
// ════════════════════════════════════════════════════════════

QWidget *HelpKeyUi::createKeyBadge(const QString &text) {
  auto *container = new QWidget;
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(0);

  auto *label = new QLabel(text);
  QFont font = AuiStyle::createEditorFont();
  font.setBold(true);
  label->setFont(font);
  label->setAlignment(Qt::AlignCenter);

  // 浅灰背景徽章样式：圆角矩形 + 边框
  label->setStyleSheet(QStringLiteral(
      "QLabel { background: %1; color: %2; padding: 2px 10px; "
      "border: 1px solid %3; border-radius: 3px; }")
      .arg(AuiStyle::tabHoverBackground().name(), AuiStyle::textColor().name(),
           AuiStyle::borderColor().name()));

  layout->addWidget(label, 0, Qt::AlignCenter);
  return container;
}

// ════════════════════════════════════════════════════════════
//  Win32 原生事件 — 边框拉伸 + 标题栏拖拽
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool HelpKeyUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) {
    return true;
  }
  return QDialog::nativeEvent(eventType, message, result);
}
#endif
