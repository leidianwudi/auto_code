/**
 * @file main.cpp
 * @brief 应用程序入口文件
 *
 * Auto Code - 代码生成与编辑工具
 *
 * 启动 MainDevMgr 开发模式（VS Code 风格的多文件编辑器），
 * 采用单例 + MVC 架构，由 MainDevMgr 充当主窗口和控制器。
 */

#include <QApplication>

#include "src/engine/function/fun_mgr.h"
#include "src/tool/ui/aui_window.h"
#include "src/ui/main_dev/main_dev_mgr.h"

/**
 * @brief 应用程序主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 */
int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // 设置应用程序级图标（影响 Windows 任务管理器进程图标）
  a.setWindowIcon(QIcon(AuiWindow::appIconPixmap(256)));

  // 注册所有 C++ 函数到 FunMgr（模板引擎 ${...} 调用基础）
  FunMgr::init();

  // 单例创建并打开开发模式主窗口（可重复调用）
  MainDevMgr::ins().open();

  int ret = QApplication::exec();

  // 释放全局资源
  FunMgr::cleanup();

  return ret;
}