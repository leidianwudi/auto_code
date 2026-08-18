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
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTextStream>

#include "src/engine/function/fun_mgr.h"
#include "src/engine/script/ac_engine.h"
#include "src/ui/main_dev/main_dev_mgr.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/setting_store.h"

static void appendLog(QTextStream &ts, const QString &title, const QString &content) {
  ts << "=== " << title << " ===" << Qt::endl;
  ts << content << Qt::endl << Qt::endl;
}

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

  // 注册所有 C++ 函数到 FunMgr（模板引擎 ${...} 调用基础）
  FunMgr::init();

  // ── headless 模式（临时）：--run <script.ac> [--root <dir>] ───────
  QStringList runArgs = QApplication::arguments();
  {
    QFile df(QStringLiteral("d:/work/github/auto_code/build/headless_debug.txt"));
    if (df.open(QIODevice::WriteOnly)) {
      QTextStream ts(&df);
      ts << "args: " << runArgs.join(QStringLiteral("|")) << "\n";
      df.close();
    }
  }
  int runIdx = runArgs.indexOf(QStringLiteral("--run"));
  if (runIdx >= 0 && runIdx + 1 < runArgs.size()) {
    QString scriptPath = runArgs.at(runIdx + 1);
    QString rootDir = QStringLiteral("d:/work/github/auto_code/file");
    int rootIdx = runArgs.indexOf(QStringLiteral("--root"));
    if (rootIdx >= 0 && rootIdx + 1 < runArgs.size()) rootDir = runArgs.at(rootIdx + 1);
    AcEngine::ins().setRootDir(rootDir);
    AcEngine::ins().setLogCallback([](const QString &text, bool isError) {
      QTextStream ts(stdout);
      ts << (isError ? QStringLiteral("[ERR] ") : QString()) << text << Qt::endl;
      QFile df(QStringLiteral("d:/work/github/auto_code/build/ac_log.txt"));
      if (df.open(QIODevice::Append)) {
        QTextStream ts2(&df);
        ts2 << (isError ? QStringLiteral("[ERR] ") : QString()) << text << "\n";
        df.close();
      }
    });
    QString runErr = AcEngine::ins().execute(scriptPath);
    {
      QFile df(QStringLiteral("d:/work/github/auto_code/build/headless_debug.txt"));
      if (df.open(QIODevice::WriteOnly)) {
        QTextStream ts(&df);
        ts << "args: " << runArgs.join(QStringLiteral("|")) << "\n";
        ts << "runErr: " << runErr << "\n";
        ts << "generatedFiles:\n";
        for (const QString &f : AcEngine::ins().generatedFiles()) ts << "  " << f << "\n";
        df.close();
      }
    }
    QTextStream ts(stdout);
    ts << "[generated]" << Qt::endl;
    for (const QString &f : AcEngine::ins().generatedFiles()) ts << f << Qt::endl;
    ts << "[debug] rootDir=" << rootDir
       << " treeExists=" << QFile::exists(rootDir + QStringLiteral("/tree.config")) << Qt::endl;
    if (!runErr.isEmpty()) ts << "[ERROR] " << runErr << Qt::endl;
    FunMgr::cleanup();
    return runErr.isEmpty() ? 0 : 1;
  }

  // 初始化全局设置存储（加载主题/颜色/快捷键/字体配置，在窗口样式应用前）
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
