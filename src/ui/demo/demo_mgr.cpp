/**
 * @file demo_mgr.cpp
 * @brief Demo 示例控制器层实现
 */

#include "demo_mgr.h"

#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTextStream>

#include "demo_model.h"
#include "demo_ui.h"
#include "src/engine/tpl/tpl_engine.h"
#include "src/tool/ui/code/code_editor.h"
#include "src/tool/ui/highlighter/light_json.h"
#include "src/tool/ui/highlighter/light_tpl.h"
#include "src/tool/ui/highlighter/light_ts.h"


// ──────────────────────────────────────────────────────────────
//  onCreateWindow — 创建 DemoUi 窗口（open() 时调用）
// ──────────────────────────────────────────────────────────────

QWidget *DemoMgr::onCreateWindow() {
  m_ui = new DemoUi;
  m_model = new DemoModel;
  m_engine = new TplEngine;

  m_ui->setupUI();
  m_ui->resize(1200, 750);

  // ── 语法高亮器 ──
  new LightTpl(m_ui->templateEdit()->document());
  new LightJson(m_ui->dataEdit()->document());
  new LightTs(m_ui->outputEdit()->document());

  initUi();

  return m_ui;
}

// ──────────────────────────────────────────────────────────────
//  initUi — 连接信号槽
// ──────────────────────────────────────────────────────────────

void DemoMgr::initUi() {
  connect(m_ui->generateAction(), &QAction::triggered, this, &DemoMgr::onGenerate);
  connect(m_ui->loadTemplateAction(), &QAction::triggered, this, &DemoMgr::onLoadTemplate);
  connect(m_ui->loadDataAction(), &QAction::triggered, this, &DemoMgr::onLoadData);
  connect(m_ui->saveOutputAction(), &QAction::triggered, this, &DemoMgr::onSaveOutput);

  // UI 发射的自定义信号
  connect(m_ui, &DemoUi::generateRequested, this, &DemoMgr::onGenerate);
  connect(m_ui, &DemoUi::loadTemplateRequested, this, &DemoMgr::onLoadTemplate);
  connect(m_ui, &DemoUi::loadDataRequested, this, &DemoMgr::onLoadData);
  connect(m_ui, &DemoUi::saveOutputRequested, this, &DemoMgr::onSaveOutput);
}

// ──────────────────────────────────────────────────────────────
//  onGenerate — 生成代码
// ──────────────────────────────────────────────────────────────

void DemoMgr::onGenerate() {
  m_model->setTemplate(m_ui->templateEdit()->toPlainText());
  m_model->setJsonData(m_ui->dataEdit()->toPlainText());

  // 解析 JSON
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(m_model->getJsonData().toUtf8(), &error);

  if (error.error != QJsonParseError::NoError) {
    m_model->setError(QStringLiteral("JSON 解析错误: %1").arg(error.errorString()));
    QMessageBox::warning(m_ui, QStringLiteral("JSON 解析错误"), m_model->error());
    return;
  }

  if (!doc.isObject()) {
    m_model->setError(QStringLiteral("JSON 数据必须是一个对象"));
    QMessageBox::warning(m_ui, QStringLiteral("JSON 格式错误"), m_model->error());
    return;
  }

  QJsonObject data = doc.object();
  m_model->setDataObject(data);
  m_model->clearError();

  // 渲染
  QString result = m_engine->render(m_model->getTemplate(), data);

  if (!m_engine->lastError().isEmpty()) {
    m_model->setError(m_engine->lastError());
    QMessageBox::warning(m_ui, QStringLiteral("模板渲染错误"), m_model->error());
    return;
  }

  m_model->setOutput(result);
  m_ui->outputEdit()->setPlainText(result);
  m_ui->setStatusText(QStringLiteral("生成成功"), 3000);
}

// ──────────────────────────────────────────────────────────────
//  onLoadTemplate — 加载模板文件
// ──────────────────────────────────────────────────────────────

void DemoMgr::onLoadTemplate() {
  QString fileName =
      QFileDialog::getOpenFileName(m_ui, QStringLiteral("加载模板文件"), QString(),
                                   QStringLiteral("模板文件 (*.tmpl *.txt);;所有文件 (*)"));

  if (fileName.isEmpty()) return;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(m_ui, QStringLiteral("错误"),
                         QStringLiteral("无法打开文件: %1").arg(fileName));
    return;
  }

  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  QString content = in.readAll();

  m_model->setTemplate(content);
  m_ui->templateEdit()->setPlainText(content);
  m_model->setSavePath(fileName);
  m_ui->setStatusText(QStringLiteral("已加载模板: %1").arg(fileName), 3000);
}

// ──────────────────────────────────────────────────────────────
//  onLoadData — 加载 JSON 数据文件
// ──────────────────────────────────────────────────────────────

void DemoMgr::onLoadData() {
  QString fileName =
      QFileDialog::getOpenFileName(m_ui, QStringLiteral("加载数据文件"), QString(),
                                   QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));

  if (fileName.isEmpty()) return;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(m_ui, QStringLiteral("错误"),
                         QStringLiteral("无法打开文件: %1").arg(fileName));
    return;
  }

  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  QString content = in.readAll();

  m_model->setJsonData(content);
  m_ui->dataEdit()->setPlainText(content);
  m_ui->setStatusText(QStringLiteral("已加载数据: %1").arg(fileName), 3000);
}

// ──────────────────────────────────────────────────────────────
//  onSaveOutput — 保存生成结果
// ──────────────────────────────────────────────────────────────

void DemoMgr::onSaveOutput() {
  QString output = m_ui->outputEdit()->toPlainText();
  if (output.isEmpty()) {
    QMessageBox::information(m_ui, QStringLiteral("提示"), QStringLiteral("没有可保存的内容"));
    return;
  }

  QString fileName =
      QFileDialog::getSaveFileName(m_ui, QStringLiteral("保存结果"), QString(),
                                   QStringLiteral("TypeScript 文件 (*.ts);;所有文件 (*)"));

  if (fileName.isEmpty()) return;

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(m_ui, QStringLiteral("错误"),
                         QStringLiteral("无法保存文件: %1").arg(fileName));
    return;
  }

  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << output;

  m_ui->setStatusText(QStringLiteral("已保存: %1").arg(fileName), 3000);
}