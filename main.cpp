/**
 * @file main.cpp
 * @brief 应用程序入口文件
 * 
 * Auto Code - TypeScript 代码生成器
 * 
 * 这是一个基于 Qt 的桌面应用程序，用于通过模板引擎生成 TypeScript 代码。
 * 主要功能：
 * - 模板编辑：支持变量替换、循环、条件判断
 * - 数据输入：使用 JSON 格式定义变量值
 * - 代码生成：将模板和数据结合生成 TypeScript 代码
 * - 文件操作：支持加载模板/数据文件，保存生成的代码
 * 
 * @author Auto Code Team
 * @version 1.0
 */

#include "mainwindow.h"

#include <QApplication>

/**
 * @brief 应用程序主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 * 
 * 执行流程：
 * 1. 创建 QApplication 实例
 * 2. 创建并显示主窗口
 * 3. 启动事件循环
 */
int main(int argc, char *argv[])
{
    // 创建应用程序实例
    QApplication a(argc, argv);
    
    // 创建主窗口
    MainWindow w;
    w.show();  // 显示主窗口
    
    // 启动事件循环，等待用户操作
    return QApplication::exec();
}
