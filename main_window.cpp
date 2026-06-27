/**
 * @file main_window.cpp
 * @brief 主窗口实现文件
 *
 * 实现主窗口的所有功能，包括：
 * - 初始化界面和示例数据
 * - 模板渲染和代码生成
 * - 文件的加载和保存
 */

#include "main_window.h"
#include "ui_main_window.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>

/**
 * @brief 构造函数
 *
 * 初始化主窗口并设置示例模板和数据。
 * 示例展示了模板引擎的所有功能：
 * - 变量替换：${modelName}
 * - 循环：${each fields}...${/each}
 * - 条件判断：${if hasService}...${/if}
 * - 算术运算：${price * quantity}
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_templateHighlighter(nullptr)
    , m_jsonHighlighter(nullptr)
    , m_tsHighlighter(nullptr)
{
    ui->setupUi(this);

    // 初始化语法高亮器
    m_templateHighlighter = new TemplateHighlighter(ui->templateEdit->document());
    m_jsonHighlighter = new JsonHighlighter(ui->dataEdit->document());
    m_tsHighlighter = new TypeScriptHighlighter(ui->outputEdit->document());

    // 设置示例模板
    // 演示 TypeScript 接口和服务类的生成
    ui->templateEdit->setPlainText(
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
        "// 复杂计算: ${(a + b) * c - d / 2}\n"
    );

    // 设置示例 JSON 数据
    // 包含模板中使用的所有变量
    ui->dataEdit->setPlainText(R"({
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

/**
 * @brief 析构函数
 *
 * 释放语法高亮器和 UI 资源
 */
MainWindow::~MainWindow()
{
    // 删除语法高亮器
    delete m_templateHighlighter;
    delete m_jsonHighlighter;
    delete m_tsHighlighter;

    // 删除 UI
    delete ui;
}

/**
 * @brief 生成代码
 *
 * 核心功能：将模板和数据结合生成代码
 *
 * 处理流程：
 * 1. 获取模板文本和 JSON 数据
 * 2. 解析 JSON 数据
 * 3. 调用模板引擎渲染
 * 4. 显示结果或错误信息
 */
void MainWindow::on_actionGenerate_triggered()
{
    // 获取模板和数据
    QString tmpl = ui->templateEdit->toPlainText();
    QString dataStr = ui->dataEdit->toPlainText();

    // 解析 JSON 数据
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8(), &error);

    // 检查 JSON 解析错误
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, QStringLiteral("JSON 解析错误"),
                           QStringLiteral("JSON 数据格式错误: %1").arg(error.errorString()));
        return;
    }

    // 检查是否为 JSON 对象
    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("JSON 格式错误"),
                           QStringLiteral("JSON 数据必须是一个对象"));
        return;
    }

    // 执行模板渲染
    QJsonObject data = doc.object();
    QString result = m_engine.render(tmpl, data);

    // 检查渲染错误
    if (!m_engine.lastError().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("模板渲染错误"),
                           m_engine.lastError());
        return;
    }

    // 显示结果
    ui->outputEdit->setPlainText(result);
    ui->statusbar->showMessage(QStringLiteral("生成成功"), 3000);
}

/**
 * @brief 加载模板文件
 *
 * 从文件系统加载模板文件到编辑区。
 * 支持的文件类型：.tmpl, .txt 或任意文件
 */
void MainWindow::on_actionLoadTemplate_triggered()
{
    // 显示文件选择对话框
    QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("加载模板文件"), QString(),
        QStringLiteral("模板文件 (*.tmpl *.txt);;所有文件 (*)"));

    if (fileName.isEmpty()) return;

    // 打开并读取文件
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                           QStringLiteral("无法打开文件: %1").arg(fileName));
        return;
    }

    // 读取文件内容（UTF-8 编码）
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    ui->templateEdit->setPlainText(in.readAll());
    ui->statusbar->showMessage(QStringLiteral("已加载模板: %1").arg(fileName), 3000);
}

/**
 * @brief 加载数据文件
 *
 * 从文件系统加载 JSON 数据文件到编辑区。
 * 支持的文件类型：.json 或任意文件
 */
void MainWindow::on_actionLoadData_triggered()
{
    // 显示文件选择对话框
    QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("加载数据文件"), QString(),
        QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));

    if (fileName.isEmpty()) return;

    // 打开并读取文件
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                           QStringLiteral("无法打开文件: %1").arg(fileName));
        return;
    }

    // 读取文件内容（UTF-8 编码）
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    ui->dataEdit->setPlainText(in.readAll());
    ui->statusbar->showMessage(QStringLiteral("已加载数据: %1").arg(fileName), 3000);
}

/**
 * @brief 保存输出结果
 *
 * 将生成的代码保存到文件。
 * 默认保存为 TypeScript 文件 (.ts)
 */
void MainWindow::on_actionSaveOutput_triggered()
{
    // 获取输出内容
    QString output = ui->outputEdit->toPlainText();
    if (output.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                               QStringLiteral("没有可保存的内容"));
        return;
    }

    // 显示文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存结果"), QString(),
        QStringLiteral("TypeScript 文件 (*.ts);;所有文件 (*)"));

    if (fileName.isEmpty()) return;

    // 打开文件进行写入
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                           QStringLiteral("无法保存文件: %1").arg(fileName));
        return;
    }

    // 写入文件内容（UTF-8 编码）
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << output;
    ui->statusbar->showMessage(QStringLiteral("已保存: %1").arg(fileName), 3000);
}

/**
 * @brief 退出应用程序
 *
 * 关闭主窗口，结束应用程序
 */
void MainWindow::on_actionExit_triggered()
{
    close();
}
