/**
 * @file setting_ui.cpp
 * @brief 设置界面 — 视图层实现
 */

#include "setting_ui.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

#include "setting_model.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

SettingUi::SettingUi(QWidget *parent) : QDialog(parent) {}

void SettingUi::setModel(SettingModel *model) { m_model = model; }

// ════════════════════════════════════════════════════════════
//  setupUI — 初始化界面布局
// ════════════════════════════════════════════════════════════

void SettingUi::setupUI() {
  setWindowTitle(QStringLiteral("设置"));
  resize(860, 740);

  // ── 无边框非模态对话框（复用 AuiWindow 统一样式） ──
  AuiWindow::setupFramelessDialog(this, false);

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏
  // ════════════════════════════════════════════════════════════
  TitleBarOptions opts;
  opts.title = QStringLiteral("设置");
  opts.showMinButton = true;
  opts.showMaxButton = true;
  opts.showCloseButton = true;
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

  auto *bodyLayout = new QHBoxLayout;
  bodyLayout->setSpacing(12);

  // ── 左侧分区导航 ──
  m_sections = new QListWidget(this);
  m_sections->setObjectName(QStringLiteral("settingSectionList"));
  m_sections->addItem(QStringLiteral("颜色"));
  m_sections->addItem(QStringLiteral("快捷键"));
  m_sections->setFixedWidth(140);
  m_sections->setCurrentRow(0);
  m_sections->setStyleSheet(
      QStringLiteral("QListWidget { background: %1; color: %2; border: 1px solid %3; "
                     "outline: none; }"
                     "QListWidget::item { padding: 8px 12px; }"
                     "QListWidget::item:selected { background: %4; color: %2; }")
          .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
               AuiStyle::borderColor().name(), AuiStyle::listSelectionBackground().name()));
  bodyLayout->addWidget(m_sections);

  // ── 右侧设置页堆栈 ──
  m_stack = new QStackedWidget(this);
  buildColorPage();
  buildShortcutPage();
  bodyLayout->addWidget(m_stack, 1);

  contentLayout->addLayout(bodyLayout, 1);

  // ── 底部按钮（关闭） ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
  AuiButton::applyDialogButtonStyle(closeBtn);
  closeBtn->setMinimumWidth(90);
  btnLayout->addWidget(closeBtn);
  contentLayout->addLayout(btnLayout);

  // ── 信号连接 ──
  connect(m_sections, &QListWidget::currentRowChanged, this, &SettingUi::onSectionChanged);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

  // 主题 / 颜色变化时即时刷新本对话框固化的样式表（无需重启）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &SettingUi::refreshStyle);
  connect(&SettingStore::ins(), &SettingStore::colorsChanged, this, &SettingUi::refreshStyle);

  // ── 应用窗口框架（标题栏 + 内容 + 1px 边框） ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // ── Win32 边框拉伸 ──
  AuiWindow::enableWin32Resize(this);
}

// ════════════════════════════════════════════════════════════
//  两个设置页构建
// ════════════════════════════════════════════════════════════

void SettingUi::buildColorPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  // ── 顶部：主题（整体配色） ──
  auto *title = new QLabel(QStringLiteral("颜色主题"), page);
  QFont tf = AuiStyle::createEditorFont();
  tf.setBold(true);
  title->setFont(tf);
  layout->addWidget(title);

  auto *desc = new QLabel(
      QStringLiteral("选择整体配色。选择「自定义」后，可在下方单独修改任意颜色。"), page);
  desc->setWordWrap(true);
  desc->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::inactiveTabColor().name()));
  layout->addWidget(desc);

  m_lightRadio = new QRadioButton(QStringLiteral("浅色"), page);
  m_darkRadio = new QRadioButton(QStringLiteral("深色"), page);
  m_customRadio = new QRadioButton(QStringLiteral("自定义"), page);

  auto *group = new QHBoxLayout;
  group->setSpacing(20);
  group->addWidget(m_lightRadio);
  group->addWidget(m_darkRadio);
  group->addWidget(m_customRadio);
  layout->addLayout(group);

  connect(m_lightRadio, &QRadioButton::toggled, this, &SettingUi::onThemeToggled);
  connect(m_darkRadio, &QRadioButton::toggled, this, &SettingUi::onThemeToggled);
  connect(m_customRadio, &QRadioButton::toggled, this, &SettingUi::onThemeToggled);

  // 分隔线
  auto *sep = new QFrame(page);
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::borderColor().name()));
  layout->addWidget(sep);

  auto *hint =
      new QLabel(QStringLiteral("点击色块可修改颜色，点击「重置」恢复当前主题默认。"), page);
  hint->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::inactiveTabColor().name()));
  layout->addWidget(hint);

  m_colorTable = new QTableWidget(page);
  m_colorTable->setColumnCount(4);
  m_colorTable->setHorizontalHeaderLabels({QStringLiteral("分类"), QStringLiteral("名称"),
                                           QStringLiteral("颜色"), QStringLiteral("操作")});
  m_colorTable->setFont(AuiStyle::createEditorFont());
  m_colorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_colorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_colorTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_colorTable->setFocusPolicy(Qt::NoFocus);
  m_colorTable->verticalHeader()->setVisible(false);
  m_colorTable->horizontalHeader()->setStretchLastSection(false);
  m_colorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_colorTable->setShowGrid(true);

  m_colorTable->setStyleSheet(
      QStringLiteral(
          "QTableWidget { background: %1; color: %2; border: 1px solid %3; gridline-color: %3; }"
          "QHeaderView::section { background: %4; color: %2; padding: 6px; border: none; "
          "border-right: 1px solid %3; border-bottom: 1px solid %3; font-weight: bold; }")
          .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
               AuiStyle::borderColor().name(), AuiStyle::background().name()));

  connect(m_colorTable, &QTableWidget::cellClicked, this, &SettingUi::onColorCellClicked);

  layout->addWidget(m_colorTable, 1);

  // ── 底部：重置所有颜色（仅在颜色页显示） ──
  auto *resetRow = new QHBoxLayout;
  resetRow->addStretch();
  m_resetAllColorBtn = new QPushButton(QStringLiteral("重置所有颜色"), page);
  AuiButton::applyDialogButtonStyle(m_resetAllColorBtn);
  m_resetAllColorBtn->setMinimumWidth(120);
  resetRow->addWidget(m_resetAllColorBtn);
  layout->addLayout(resetRow);

  connect(m_resetAllColorBtn, &QPushButton::clicked, this, &SettingUi::onResetAllColors);

  m_stack->addWidget(page);
}

void SettingUi::buildShortcutPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *hint =
      new QLabel(QStringLiteral("双击「快捷键」列可修改；按下组合键后点击确定保存。"), page);
  hint->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::inactiveTabColor().name()));
  layout->addWidget(hint);

  m_shortcutTable = new QTableWidget(page);
  m_shortcutTable->setColumnCount(3);
  m_shortcutTable->setHorizontalHeaderLabels(
      {QStringLiteral("分类"), QStringLiteral("功能"), QStringLiteral("快捷键")});
  m_shortcutTable->setFont(AuiStyle::createEditorFont());
  m_shortcutTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_shortcutTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_shortcutTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_shortcutTable->setFocusPolicy(Qt::NoFocus);
  m_shortcutTable->verticalHeader()->setVisible(false);
  m_shortcutTable->horizontalHeader()->setStretchLastSection(false);
  m_shortcutTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_shortcutTable->setShowGrid(true);

  m_shortcutTable->setStyleSheet(
      QStringLiteral(
          "QTableWidget { background: %1; color: %2; border: 1px solid %3; gridline-color: %3; }"
          "QHeaderView::section { background: %4; color: %2; padding: 6px; border: none; "
          "border-right: 1px solid %3; border-bottom: 1px solid %3; font-weight: bold; }")
          .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
               AuiStyle::borderColor().name(), AuiStyle::background().name()));

  connect(m_shortcutTable, &QTableWidget::cellDoubleClicked, this,
          &SettingUi::onShortcutDoubleClicked);

  layout->addWidget(m_shortcutTable, 1);

  m_stack->addWidget(page);
}

// ════════════════════════════════════════════════════════════
//  数据刷新
// ════════════════════════════════════════════════════════════

void SettingUi::reloadAll() {
  reloadTheme();
  reloadColors();
  reloadShortcuts();
}

void SettingUi::refreshStyle() {
  // 刷新本对话框标题栏（背景）与标题文字，保证主题切换时即时生效（不依赖全局循环）
  if (m_titleBar) {
    AuiStyle::applyTitleBarStyle(m_titleBar);
    m_titleBar->update();
    for (QToolButton *btn : m_titleBar->findChildren<QToolButton *>())
      AuiStyle::applyMenuButtonStyle(btn);
    // 重绘标题栏 AC 应用图标（背景/文字色随主题变色）
    AuiWindow::refreshAppIcons(m_titleBar);
  }
  if (QLabel *tl = findChild<QLabel *>(QStringLiteral("AuiTitleLabel")))
    AuiStyle::applyTitleLabelStyle(tl);

  // 左侧分区导航列表（颜色 / 快捷键 tab）
  if (m_sections) {
    m_sections->setStyleSheet(
        QStringLiteral("QListWidget { background: %1; color: %2; border: 1px solid %3; "
                       "outline: none; }"
                       "QListWidget::item { padding: 8px 12px; }"
                       "QListWidget::item:selected { background: %4; color: %2; }")
            .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
                 AuiStyle::borderColor().name(), AuiStyle::listSelectionBackground().name()));
  }

  // 颜色表 / 快捷键表（背景 / 文字 / 表头颜色随主题重建）
  auto applyTableStyle = [](QTableWidget *t) {
    if (!t) return;
    t->setStyleSheet(
        QStringLiteral(
            "QTableWidget { background: %1; color: %2; border: 1px solid %3; gridline-color: %3; }"
            "QHeaderView::section { background: %4; color: %2; padding: 6px; border: none; "
            "border-right: 1px solid %3; border-bottom: 1px solid %3; font-weight: bold; }")
            .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name(),
                 AuiStyle::borderColor().name(), AuiStyle::background().name()));
  };
  applyTableStyle(m_colorTable);
  applyTableStyle(m_shortcutTable);

  // 重新填充色块与快捷键徽章（用当前主题色重建）
  reloadColors();
  reloadShortcuts();
}

void SettingUi::reloadTheme() {
  if (!m_model) return;
  switch (m_model->themeIndex()) {
    case 0:
      m_lightRadio->setChecked(true);
      break;
    case 1:
      m_darkRadio->setChecked(true);
      break;
    default:
      m_customRadio->setChecked(true);
      break;
  }
}

void SettingUi::populateColorTable() {
  if (!m_model) return;
  const QList<ColorEntry> entries = m_model->colors();

  // 按分类分组（保持首次出现顺序）
  m_colorTable->setRowCount(entries.size());
  for (int i = 0; i < entries.size(); ++i) {
    const ColorEntry &e = entries.at(i);

    // 分类
    auto *catItem = new QTableWidgetItem(e.category);
    QFont catFont = AuiStyle::createEditorFont();
    catFont.setBold(true);
    catItem->setFont(catFont);
    catItem->setTextAlignment(Qt::AlignCenter);
    m_colorTable->setItem(i, 0, catItem);

    // 名称
    auto *nameItem = new QTableWidgetItem(e.label);
    m_colorTable->setItem(i, 1, nameItem);

    // 颜色色块（点击弹出取色器）
    auto *swatch = new QPushButton;
    swatch->setFixedHeight(24);
    swatch->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 3px; }")
            .arg(e.hex, AuiStyle::borderColor().name()));
    swatch->setToolTip(e.hex + (e.custom ? QStringLiteral("（已自定义）") : QStringLiteral("")));
    m_colorTable->setCellWidget(i, 2, swatch);

    // 重置按钮
    auto *resetBtn = new QPushButton(QStringLiteral("重置"));
    AuiButton::applyDialogButtonStyle(resetBtn);
    resetBtn->setEnabled(e.custom);
    resetBtn->setProperty("colorKey", e.key);
    connect(resetBtn, &QPushButton::clicked, this, &SettingUi::onResetColor);
    m_colorTable->setCellWidget(i, 3, resetBtn);
  }

  m_colorTable->setColumnWidth(0, 90);
  m_colorTable->setColumnWidth(1, 220);
  m_colorTable->setColumnWidth(2, 140);
  m_colorTable->setColumnWidth(3, 90);
  m_colorTable->verticalHeader()->setDefaultSectionSize(32);
}

void SettingUi::reloadColors() {
  if (!m_model) return;
  populateColorTable();
}

void SettingUi::reloadShortcuts() {
  if (!m_model) return;
  const QList<ShortcutSettingEntry> entries = m_model->shortcuts();

  m_shortcutTable->setRowCount(entries.size());
  for (int i = 0; i < entries.size(); ++i) {
    const ShortcutSettingEntry &e = entries.at(i);

    auto *catItem = new QTableWidgetItem(e.category);
    QFont catFont = AuiStyle::createEditorFont();
    catFont.setBold(true);
    catItem->setFont(catFont);
    catItem->setTextAlignment(Qt::AlignCenter);
    m_shortcutTable->setItem(i, 0, catItem);

    auto *descItem = new QTableWidgetItem(e.label);
    m_shortcutTable->setItem(i, 1, descItem);

    m_shortcutTable->setCellWidget(i, 2, createKeyBadge(e.sequence));
  }

  m_shortcutTable->setColumnWidth(0, 120);
  m_shortcutTable->setColumnWidth(1, 260);
  m_shortcutTable->setColumnWidth(2, 220);
  m_shortcutTable->verticalHeader()->setDefaultSectionSize(34);
}

// ════════════════════════════════════════════════════════════
//  槽函数
// ════════════════════════════════════════════════════════════

void SettingUi::onSectionChanged(int row) {
  if (m_stack && row >= 0 && row < m_stack->count()) m_stack->setCurrentIndex(row);
}

void SettingUi::onThemeToggled() {
  if (!m_model) return;
  int idx = 0;
  if (m_lightRadio->isChecked())
    idx = 0;
  else if (m_darkRadio->isChecked())
    idx = 1;
  else if (m_customRadio->isChecked())
    idx = 2;
  else
    return;
  m_model->setThemeIndex(idx);
  m_model->save();
  // 主题变化后颜色展示跟随刷新
  reloadColors();
}

void SettingUi::onColorCellClicked(int row, int column) {
  if (!m_model || column != 2) return;
  const QList<ColorEntry> entries = m_model->colors();
  if (row < 0 || row >= entries.size()) return;
  const ColorEntry &e = entries.at(row);

  QColor initial(e.hex);
  const QColor chosen =
      QColorDialog::getColor(initial, this, QStringLiteral("选择颜色 - ") + e.label);
  if (!chosen.isValid()) return;
  m_model->setColor(e.key, chosen.name());
  m_model->save();
  reloadColors();
}

void SettingUi::onResetColor() {
  auto *btn = qobject_cast<QPushButton *>(sender());
  if (!btn || !m_model) return;
  const QString key = btn->property("colorKey").toString();
  if (key.isEmpty()) return;
  m_model->resetColor(key);
  m_model->save();
  reloadColors();
}

void SettingUi::onResetAllColors() {
  if (!m_model) return;
  m_model->resetAllColors();
  m_model->save();
  reloadTheme();
  reloadColors();
}

void SettingUi::onShortcutDoubleClicked(int row, int column) {
  if (!m_model || column != 2) return;
  const QList<ShortcutSettingEntry> entries = m_model->shortcuts();
  if (row < 0 || row >= entries.size()) return;
  const ShortcutSettingEntry &e = entries.at(row);

  QString newSeq;
  if (!captureShortcut(e.sequence, &newSeq)) return;
  m_model->setShortcut(e.key, newSeq);
  m_model->save();
  reloadShortcuts();
}

// ════════════════════════════════════════════════════════════
//  辅助方法
// ════════════════════════════════════════════════════════════

QWidget *SettingUi::createKeyBadge(const QString &text) {
  auto *container = new QWidget;
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(0);

  auto *label = new QLabel(text.isEmpty() ? QStringLiteral("（未设置）") : text);
  QFont font = AuiStyle::createEditorFont();
  font.setBold(true);
  label->setFont(font);
  label->setAlignment(Qt::AlignCenter);
  label->setStyleSheet(QStringLiteral("QLabel { background: %1; color: %2; padding: 2px 10px; "
                                      "border: 1px solid %3; border-radius: 3px; }")
                           .arg(AuiStyle::tabHoverBackground().name(), AuiStyle::textColor().name(),
                                AuiStyle::borderColor().name()));
  layout->addWidget(label, 0, Qt::AlignCenter);
  return container;
}

bool SettingUi::captureShortcut(const QString &current, QString *out) {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("设置快捷键"));
  dlg.setModal(true);
  auto *layout = new QVBoxLayout(&dlg);
  layout->setSpacing(10);

  auto *hint = new QLabel(QStringLiteral("按下新的组合键，或点击「清除」移除快捷键。"), &dlg);
  layout->addWidget(hint);

  auto *edit = new QKeySequenceEdit(&dlg);
  if (!current.isEmpty()) edit->setKeySequence(QKeySequence(current));
  layout->addWidget(edit);

  auto *clearBtn = new QPushButton(QStringLiteral("清除"), &dlg);
  AuiButton::applyDialogButtonStyle(clearBtn);
  layout->addWidget(clearBtn);

  auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(box);

  connect(clearBtn, &QPushButton::clicked, edit, &QKeySequenceEdit::clear);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return false;
  *out = edit->keySequence().toString();
  return true;
}

// ════════════════════════════════════════════════════════════
//  Win32 原生事件 — 边框拉伸 + 标题栏拖拽
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool SettingUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) {
    return true;
  }
  return QDialog::nativeEvent(eventType, message, result);
}
#endif
