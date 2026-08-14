/**
 * @file help_doc_ui.cpp
 * @brief 帮助文档界面 — 视图层实现
 */

#include "help_doc_ui.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "help_doc_model.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/setting_store.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

HelpDocUi::HelpDocUi(QWidget *parent) : QDialog(parent) {}

void HelpDocUi::setModel(HelpDocModel *model) { m_model = model; }

// ════════════════════════════════════════════════════════════
//  setupUI — 初始化界面布局
// ════════════════════════════════════════════════════════════

void HelpDocUi::setupUI() {
  setWindowTitle(QStringLiteral("帮助文档"));
  resize(920, 640);

  // ── 无边框非模态对话框（复用 AuiWindow 统一样式） ──
  AuiWindow::setupFramelessDialog(this, false);

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏
  // ════════════════════════════════════════════════════════════
  TitleBarOptions opts;
  opts.title = QStringLiteral("帮助文档");
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
  contentLayout->setContentsMargins(12, 12, 12, 12);
  contentLayout->setSpacing(8);

  auto *bodyLayout = new QHBoxLayout;
  bodyLayout->setSpacing(12);

  // ── 左侧分类（滚动单选按钮列） ──
  buildCategoryList();
  bodyLayout->addWidget(m_categoryScroll);

  // ── 右侧代码展示区 ──
  buildCodePanel();
  bodyLayout->addWidget(m_codeEdit, 1);

  contentLayout->addLayout(bodyLayout, 1);

  // ── 信号连接 ──
  connect(m_categoryGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
          &HelpDocUi::onCategoryChanged);

  // 主题 / 自定义颜色变化时重建固化样式并刷新高亮（无需重启）
  connect(&SettingStore::ins(), &SettingStore::themeChanged, this, &HelpDocUi::refreshStyle);
  connect(&SettingStore::ins(), &SettingStore::colorsChanged, this, &HelpDocUi::refreshStyle);

  // 默认选中第一个分类
  if (m_model && m_model->count() > 0) {
    auto *first = qobject_cast<QRadioButton *>(m_categoryGroup->button(0));
    if (first) first->setChecked(true);
    m_codeEdit->setPlainText(m_model->codeAt(0));
  }

  // ── 应用窗口框架（标题栏 + 内容 + 1px 边框） ──
  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

  // ── Win32 边框拉伸 ──
  AuiWindow::enableWin32Resize(this);
}

// ════════════════════════════════════════════════════════════
//  buildCategoryList — 左侧分类单选按钮列
// ════════════════════════════════════════════════════════════

void HelpDocUi::buildCategoryList() {
  m_categoryScroll = new QScrollArea(this);
  m_categoryScroll->setWidgetResizable(true);
  m_categoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_categoryScroll->setStyleSheet(categoryScrollStyle());

  auto *listWidget = new QWidget;
  m_categoryLayout = new QVBoxLayout(listWidget);
  m_categoryLayout->setContentsMargins(8, 8, 8, 8);
  m_categoryLayout->setSpacing(4);

  m_categoryGroup = new QButtonGroup(this);
  m_categoryGroup->setExclusive(true);

  if (m_model) {
    const int n = m_model->count();
    for (int i = 0; i < n; ++i) {
      auto *radio = new QRadioButton(m_model->titleAt(i), listWidget);
      radio->setStyleSheet(radioStyle());
      m_categoryGroup->addButton(radio, i);
      m_categoryLayout->addWidget(radio);
    }
  }

  m_categoryLayout->addStretch();
  m_categoryScroll->setWidget(listWidget);
  m_categoryScroll->setFixedWidth(200);
}

// ════════════════════════════════════════════════════════════
//  buildCodePanel — 右侧 AC 代码展示区
// ════════════════════════════════════════════════════════════

void HelpDocUi::buildCodePanel() {
  m_codeEdit = new CodeEditor(this);

  // 只读展示：保留文本选择与 Ctrl+C 复制能力
  m_codeEdit->setReadOnly(true);
  m_codeEdit->setFrameStyle(QFrame::StyledPanel);
  m_codeEdit->setStyleSheet(codeEditStyle());

  // AC 语法高亮（复用封装好的高亮器，实现代码变色）
  m_codeEdit->setSyntaxHighlighter(new LightAc(m_codeEdit->document()));
}

// ════════════════════════════════════════════════════════════
//  refreshStyle — 主题/自定义颜色变化时重建固化样式并刷新高亮
// ════════════════════════════════════════════════════════════

void HelpDocUi::refreshStyle() {
  // 左侧分类滚动区（背景 / 边框随主题重建）
  if (m_categoryScroll) m_categoryScroll->setStyleSheet(categoryScrollStyle());

  // 左侧单选按钮（文字 / 悬停背景随主题重建）
  if (m_categoryGroup) {
    const auto buttons = m_categoryGroup->buttons();
    for (QAbstractButton *btn : buttons) {
      if (auto *radio = qobject_cast<QRadioButton *>(btn)) radio->setStyleSheet(radioStyle());
    }
  }

  // 右侧代码编辑器（背景 / 边框 + 语法高亮颜色随主题刷新）
  if (m_codeEdit) {
    m_codeEdit->setStyleSheet(codeEditStyle());
    m_codeEdit->reloadColors();
    m_codeEdit->viewport()->update();
  }

  update();
}

// ════════════════════════════════════════════════════════════
//  样式表构建（使用当前主题色，构造与 refreshStyle 共用）
// ════════════════════════════════════════════════════════════

QString HelpDocUi::categoryScrollStyle() const {
  return QStringLiteral(
             "QScrollArea { background: %1; border: 1px solid %2; border-radius: 3px; }"
             "QScrollArea > QWidget > QWidget { background: %1; }")
      .arg(AuiStyle::panelBackground().name(), AuiStyle::borderColor().name());
}

QString HelpDocUi::radioStyle() const {
  return QStringLiteral(
             "QRadioButton { color: %1; padding: 4px 6px; }"
             "QRadioButton:hover { background: %2; }")
      .arg(AuiStyle::textColor().name(), AuiStyle::hoverBackground().name());
}

QString HelpDocUi::codeEditStyle() const {
  return QStringLiteral(
             "QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 3px; }")
      .arg(AuiStyle::panelBackground().name(), AuiStyle::borderColor().name());
}

// ════════════════════════════════════════════════════════════
//  onCategoryChanged — 分类切换：刷新右侧代码内容
// ════════════════════════════════════════════════════════════

void HelpDocUi::onCategoryChanged(int index) {
  if (m_model && index >= 0 && index < m_model->count()) {
    m_codeEdit->setPlainText(m_model->codeAt(index));
  }
}

// ════════════════════════════════════════════════════════════
//  nativeEvent — Win32 原生事件（标题栏拖拽 / 边框拉伸）
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool HelpDocUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) return true;
  return QDialog::nativeEvent(eventType, message, result);
}
#endif
