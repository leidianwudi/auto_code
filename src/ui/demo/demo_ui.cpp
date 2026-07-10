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
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include "src/tool/ui/code/code_editor.h"
#include "src/tool/ui/component/aui_button.h"
#include "src/tool/ui/component/aui_style.h"


// ──────────────────────────────────────────────────────────────
//  DemoUi — 构造函数
// ──────────────────────────────────────────────────────────────

DemoUi::DemoUi(QWidget *parent) : QMainWindow(parent) {}

// ──────────────────────────────────────────────────────────────
//  setupUI — 初始化主界面布局
// ──────────────────────────────────────────────────────────────

void DemoUi::setupUI() {
  setWindowTitle(QStringLiteral("示例 - 模板代码生成"));

  // ── 中央区域 ──
  auto *centralWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── 顶部栏：菜单栏 + 生成按钮并排 ──
  auto *topBar = new QWidget;
  auto *topLayout = new QHBoxLayout(topBar);
  topLayout->setContentsMargins(0, 0, 6, 0);
  topLayout->setAlignment(Qt::AlignLeft);

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

  topLayout->addWidget(menuBar);

  // ── 生成按钮（大图标，紧接在文件菜单右边） ──
  auto *genBtn = AuiButton::createBuildButton();
  connect(genBtn, &QPushButton::clicked, m_generateAction, &QAction::trigger);
  topLayout->addWidget(genBtn);
  topLayout->addStretch();

  mainLayout->addWidget(topBar);

  // ── 内容区：上（模板 + 数据）下（输出），留边距 ──
  auto *contentWidget = new QWidget;
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(8, 8, 8, 8);
  contentLayout->setSpacing(6);

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

  mainLayout->addWidget(contentWidget, 1);

  setCentralWidget(centralWidget);

  // ── 状态栏 ──
  m_statusLabel = new QLabel(QStringLiteral("就绪"));
  statusBar()->addWidget(m_statusLabel, 1);

  // ── 示例数据 ──
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