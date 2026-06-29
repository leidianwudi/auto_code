/**
 * @file main_window.h
 * @brief 主窗口头文件
 *
 * 定义应用程序的主窗口类，提供模板编辑、数据输入和代码生成功能的界面。
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "src/engine/template_engine.h"
#include "src/ui/tool_ui/highlighter/json_highlighter.h"
#include "src/ui/tool_ui/highlighter/template_highlighter.h"
#include "src/ui/tool_ui/highlighter/ts_highlighter.h"
#include <QMainWindow>


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief 应用程序主窗口
 *
 * 提供图形用户界面，包含三个编辑区：
 * 1. 模板编辑区（左侧）：输入模板代码，带模板语法高亮和实时验证
 * 2. 数据输入区（右上）：输入 JSON 数据，带 JSON 语法高亮和实时验证
 * 3. 输出显示区（右下）：显示生成的 TypeScript 代码，带 TypeScript 语法高亮
 *
 * 菜单栏提供：加载模板、加载数据、保存输出、生成代码、退出
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  /**
   * @brief 生成代码
   *
   * 将模板和 JSON 数据结合，生成 TypeScript 代码并显示在输出区。
   * 失败时弹出错误对话框。
   */
  void on_actionGenerate_triggered();
  /// 加载模板文件（.tmpl, .txt 或任意文件）
  void on_actionLoadTemplate_triggered();
  /// 加载 JSON 数据文件（.json 或任意文件）
  void on_actionLoadData_triggered();
  /// 保存生成结果到文件（默认 .ts）
  void on_actionSaveOutput_triggered();
  /// 退出应用程序
  void on_actionExit_triggered();

private:
  Ui::MainWindow *ui;                         ///< UI 界面对象
  TemplateEngine m_engine;                    ///< 模板引擎实例
  TemplateHighlighter *m_templateHighlighter; ///< 模板语法高亮器
  JsonHighlighter *m_jsonHighlighter;         ///< JSON 语法高亮器
  TypeScriptHighlighter *m_tsHighlighter;     ///< TypeScript 语法高亮器
};

#endif // MAIN_WINDOW_H