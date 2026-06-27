/**
 * @file main_window.h
 * @brief 主窗口头文件
 * 
 * 定义应用程序的主窗口类，提供模板编辑、数据输入和代码生成功能的界面。
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include "template_engine.h"
#include "template_highlighter.h"
#include "json_highlighter.h"
#include "ts_highlighter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief 应用程序主窗口
 * 
 * 提供图形用户界面，包含：
 * - 模板编辑区：输入模板代码（带语法高亮）
 * - 数据输入区：输入 JSON 数据（带语法高亮）
 * - 输出显示区：显示生成的 TypeScript 代码（带语法高亮）
 * - 菜单栏和工具栏：提供文件操作和生成功能
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MainWindow(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MainWindow() override;

private slots:
    /**
     * @brief 生成代码
     * 
     * 将模板和 JSON 数据结合，生成 TypeScript 代码并显示在输出区
     */
    void on_actionGenerate_triggered();
    
    /**
     * @brief 加载模板文件
     * 
     * 从文件对话框选择模板文件并加载到模板编辑区
     */
    void on_actionLoadTemplate_triggered();
    
    /**
     * @brief 加载数据文件
     * 
     * 从文件对话框选择 JSON 数据文件并加载到数据输入区
     */
    void on_actionLoadData_triggered();
    
    /**
     * @brief 保存输出结果
     * 
     * 将生成的代码保存到文件
     */
    void on_actionSaveOutput_triggered();
    
    /**
     * @brief 退出应用程序
     */
    void on_actionExit_triggered();

private:
    Ui::MainWindow *ui;                          ///< UI 界面对象
    TemplateEngine m_engine;                     ///< 模板引擎实例
    TemplateHighlighter *m_templateHighlighter;  ///< 模板语法高亮器
    JsonHighlighter *m_jsonHighlighter;          ///< JSON 语法高亮器
    TypeScriptHighlighter *m_tsHighlighter;      ///< TypeScript 语法高亮器
};

#endif // MAIN_WINDOW_H
