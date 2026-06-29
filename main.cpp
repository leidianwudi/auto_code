/**
 * @file main.cpp
 * @brief 应用程序入口文件
 *
 * Auto Code - 代码生成与编辑工具
 *
 * 提供两种模式：
 * - MainWindow：模板生成模式（传统单窗口编辑）
 * - MainDev：开发模式（VS Code 风格的多文件编辑器）
 *
 * 当前启动 MainDev 开发模式。
 *
 * @author Auto Code Team
 * @version 1.0
 */

#include "main_window.h"
#include "src/ui/main_dev/main_dev.h"

#include <QApplication>

/**
 * @brief 应用程序主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 *
 * 执行流程：
 * 1. 创建 QApplication 实例
 * 2. 创建并显示 MainDev 主窗口（VS Code 风格编辑器）
 * 3. 启动事件循环
 */
int main(int argc, char *argv[]) {
  // 创建应用程序实例
  QApplication a(argc, argv);

  // 启动开发模式主窗口
  MainDev dev;
  dev.show();

  // 启动事件循环，等待用户操作
  return QApplication::exec();
}