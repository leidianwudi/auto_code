/**
 * @file demo_ui.cpp
 * @brief Demo 视图层实现
 */

#include "demo_ui.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSizeGrip>
#include <QTimer>
#include <QVBoxLayout>

#include "src/tool/ui/code/code_editor.h"
#include "src/tool/ui/component/aui_button.h"
#include "src/tool/ui/component/aui_style.h"
#include "src/tool/ui/component/aui_window.h"

// ──────────────────────────────────────────────────────────────
//  DemoUi — 构造函数
// ──────────────────────────────────────────────────────────────

DemoUi::DemoUi(QWidget *parent) : QMainWindow(parent) {}

// ──────────────────────────────────────────────────────────────
//  setupUI — 初始化主界面布局
// ──────────────────────────────────────────────────────────────

void DemoUi::setupUI() {
  // ── 无边框窗口（复用 AuiWindow 统一样式） ──
  AuiWindow::setupFramelessWindow(this);

  // ════════════════════════════════════════════════════════════
  //  自定义标题栏（单行：AC 图标 + 菜单 + 生成按钮 + 窗口控制）
  // ════════════════════════════════════════════════════════════
  m_titleBar = new QWidget;
  m_titleBar->setObjectName(QStringLiteral("TitleBar"));
  auto *titleLayout = new QHBoxLayout(m_titleBar);
  titleLayout->setContentsMargins(AuiStyle::titleBarMargins());
  titleLayout->setSpacing(AuiStyle::titleBarSpacing());

  // ── AC 程序图标 ──
  titleLayout->addWidget(AuiWindow::createAppIcon(nullptr, 20));

  // ── 菜单栏 ──
  auto *menuBar = new QMenuBar;
  menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

  auto *fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
  m_loadTemplateAction = fileMenu->addAction(QStringLiteral("加载模板(&T)..."));
  m_loadTemplateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));

  m_loadDataAction = fileMenu->addAction(QStringLiteral("加载数据(&D)..."));
  m_loadDataAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));

  fileMenu->addSeparator();

  m_saveOutputAction = fileMenu->addAction(QStringLiteral("保存结果(&S)..."));
  m_saveOutputAction->setShortcut(QKeySequence::Save);

  m_generateAction = new QAction(QStringLiteral("生成代码(&G)"), this);
  m_generateAction->setShortcut(QKeySequence(QStringLiteral("F5")));

  titleLayout->addWidget(menuBar);

  // ── 生成按钮 ──
  auto *genBtn = AuiButton::createBuildButton();
  connect(genBtn, &QPushButton::clicked, m_generateAction, &QAction::trigger);
  titleLayout->addWidget(genBtn);

  titleLayout->addStretch();

  // ── 窗口控制按钮 ──
  auto *minBtn = AuiButton::createMinButton();
  auto *maxBtn = AuiButton::createMaxButton();
  auto *closeBtn = AuiButton::createCloseButton();
  connect(closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
  connect(minBtn, &QPushButton::clicked, this, &QMainWindow::showMinimized);
  connect(maxBtn, &QPushButton::clicked, this, [this]() { AuiWindow::toggleMaximize(this); });
  titleLayout->addWidget(minBtn);
  titleLayout->addWidget(maxBtn);
  titleLayout->addWidget(closeBtn);

  // ════════════════════════════════════════════════════════════
  //  内容区域
  // ════════════════════════════════════════════════════════════
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);

  // ── 上方分割器：左侧模板 / 右侧数据 ──
  auto *topSplitter = new QSplitter(Qt::Horizontal);

  // 左：模板编辑区
  auto *templatePanel = new QWidget;
  auto *templateLayout = new QVBoxLayout(templatePanel);
  templateLayout->setContentsMargins(0, 0, 0, 0);
  templateLayout->setSpacing(4);
  auto *templateLabel = new QLabel(QStringLiteral("模板"));
  QFont sectionFont = templateLabel->font();
  sectionFont.setBold(true);
  sectionFont.setPointSize(sectionFont.pointSize() + 1);
  templateLabel->setFont(sectionFont);
  m_templateEdit = new CodeEditor;
  m_templateEdit->setValidationMode(CodeEditor::TemplateValidation);
  templateLayout->addWidget(templateLabel);
  templateLayout->addWidget(m_templateEdit);
  topSplitter->addWidget(templatePanel);

  // 右：JSON 数据编辑区
  auto *dataPanel = new QWidget;
  auto *dataLayout = new QVBoxLayout(dataPanel);
  dataLayout->setContentsMargins(0, 0, 0, 0);
  dataLayout->setSpacing(4);
  auto *dataLabel = new QLabel(QStringLiteral("数据 (JSON)"));
  dataLabel->setFont(sectionFont);
  m_dataEdit = new CodeEditor;
  m_dataEdit->setValidationMode(CodeEditor::JsonValidation);
  dataLayout->addWidget(dataLabel);
  dataLayout->addWidget(m_dataEdit);
  topSplitter->addWidget(dataPanel);
  topSplitter->setStretchFactor(0, 1);
  topSplitter->setStretchFactor(1, 1);

  contentLayout->addWidget(topSplitter, 2);

  // ── 下方：输出区 ──
  auto *outputPanel = new QWidget;
  auto *outputLayout = new QVBoxLayout(outputPanel);
  outputLayout->setContentsMargins(0, 0, 0, 0);
  outputLayout->setSpacing(4);
  auto *outputLabel = new QLabel(QStringLiteral("输出"));
  outputLabel->setFont(sectionFont);
  m_outputEdit = new CodeEditor;
  m_outputEdit->setReadOnly(true);
  outputLayout->addWidget(outputLabel);
  outputLayout->addWidget(m_outputEdit);
  contentLayout->addWidget(outputPanel, 1);

  // ── 底部状态栏（放在内容区内，确保在 WindowFrame 边框内） ──
  m_statusLabel = new QLabel(QStringLiteral("就绪"));
  m_statusLabel->setContentsMargins(4, 0, 4, 0);
  contentLayout->addWidget(AuiWindow::createStatusBar(contentWidget, m_statusLabel));

  // ── 应用窗口框架（2px 边框，包裹标题栏 + 内容） ──
  AuiWindow::applyWindowFrame(this, m_titleBar, contentWidget);

  // ── Win32 边框拉伸 ──
  AuiWindow::enableWin32Resize(this);

  // ── 示例数据 ──
  loadExampleData();
}

// ──────────────────────────────────────────────────────────────
//  loadExampleData — 加载示例模板和数据
// ──────────────────────────────────────────────────────────────

void DemoUi::loadExampleData() {
  m_templateEdit->setPlainText(
      "// ${modelName} Model\n"
      "export interface ${modelName} {\n"
      "${each fields}"
      "  ${name}: ${type};\n"
      "${/each}"
      "}\n"
      "\n"
      "${if hasService}"
      "export class ${modelName}Service {\n"
      "  static async getAll(): Promise<${modelName}[]> {\n"
      "    return fetch('/api/${modelName.toLowerCase}').then(r => r.json());\n"
      "  }\n"
      "}\n"
      "${/if}\n"
      "\n"
      "// 算术运算示例\n"
      "// 总价: ${price * quantity}\n"
      "// 含税价: ${price * 1.13}\n"
      "// 折扣后: ${(price * quantity) * discount}\n"
      "// 平均值: ${total / count}\n"
      "// 复杂计算: ${(a + b) * c - d / 2}\n");

  m_dataEdit->setPlainText(R"({
  "modelName": "User",
  "fields": [
    {"name": "id", "type": "number"},
    {"name": "name", "type": "string"},
    {"name": "email", "type": "string"}
  ],
  "hasService": true,
  "price": 100,
  "quantity": 5,
  "discount": 0.9,
  "total": 500,
  "count": 10,
  "a": 10,
  "b": 20,
  "c": 3,
  "d": 8
})");
}

// ──────────────────────────────────────────────────────────────
//  setStatusText — 设置状态栏文字
// ──────────────────────────────────────────────────────────────

void DemoUi::setStatusText(const QString &text, int timeout) {
  if (m_statusLabel) m_statusLabel->setText(text);
  if (timeout > 0)
    QTimer::singleShot(timeout, this, [this]() {
      if (m_statusLabel) m_statusLabel->setText(QStringLiteral("就绪"));
    });
}

// ════════════════════════════════════════════════════════════
//  Win32 原生事件 — 边框拉伸 + 标题栏拖拽
// ════════════════════════════════════════════════════════════

#if defined(Q_OS_WIN)
bool DemoUi::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  if (AuiWindow::handleNativeEvent(this, m_titleBar, eventType, message, result)) {
    return true;
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif