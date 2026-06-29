/**
 * @file main.cpp
 * @brief 应用程序入口文件
 *
 * Auto Code - 代码生成与编辑工具
 *
 * 启动 MainDevMgr 开发模式（VS Code 风格的多文件编辑器），
 * 采用单例 + MVC 架构，由 MainDevMgr 充当主窗口和控制器。
 */

#include "src/ui/main_dev/main_dev_mgr.h"

#include <QApplication>

/**
 * @brief 应用程序主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 */
int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // 单例创建并打开开发模式主窗口（可重复调用）
  MainDevMgr::ins().open();

  return QApplication::exec();
}