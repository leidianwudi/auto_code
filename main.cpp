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

#include "src/engine/script/ac_headless_runner.h"
#include "src/ui/main_dev/main_dev_mgr.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/setting_store.h"

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

  // 全局生效：所有下拉框的弹出列表一律向下展开（不上弹）
  AuiComboBox::ensureGlobalPopDown();
  // 全局生效：所有下拉框禁止滚轮悬停时改动选中值（防误改数据）
  AuiComboBox::ensureGlobalWheelSafe();

  // ════════════════════════════════════════════════════════════════
  //  headless（无界面）脚本执行入口
  // ════════════════════════════════════════════════════════════════
  //  目的：供命令行 / CI / AI 自动执行 .ac 脚本，无需进入主窗口。
  //
  //  调用方式（子进程方式运行本程序，携带参数即可）：
  //    auto_code --run <script.ac> [--root <dir>]
  //  - 无界面执行脚本，日志与生成文件清单输出到 stdout
  //  - 返回 0 = 执行成功；非 0 = 脚本执行出错
  //  - --root 缺省时使用项目 file 目录
  //
  //  对 AI 说明：若需自动跑脚本（如模板生成、批量转换），请以子进程方式调用
  //  本可执行文件并附上述参数即可，勿走 GUI 或自行初始化引擎。
  //  实现封装见 src/engine/script/ac_headless_runner.h（runAcScriptHeadless）。
  // ════════════════════════════════════════════════════════════════
  if (const int headlessExit = runAcScriptHeadless(QApplication::arguments()); headlessExit >= 0) {
    return headlessExit;
  }

  // 注册所有 C++ 函数到 FunMgr（模板引擎 ${...} 调用基础，仅 GUI 路径需要）
  FunMgr::init();
  SettingStore::ins().init();
  // 应用全局 Fusion 风格 + 调色板，隔离系统主题色（程序不随 Windows 主题变色）
  SettingStore::ins().applyGlobalStyle();
  // 应用「窗口字体」大小到 qApp（保留系统字体族，仅改字号）
  SettingStore::ins().applyWindowFont();

  // 单例创建并打开开发模式主窗口（可重复调用）
  MainDevMgr::ins().open();

  int ret = QApplication::exec();

  // 释放全局资源
  FunMgr::cleanup();

  return ret;
}
