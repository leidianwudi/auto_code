/**
 * @file ac_headless_runner.h
 * @brief 无界面（headless）脚本执行封装（header-only）
 *
 * 解决：headless 逻辑不再内联在 main.cpp，而是收敛到专门位置，供命令行/CI/AI 复用。
 *
 * 调用方式（命令行或子进程）：
 *   请推进自动执行脚本的 AI 或外部工具，以子进程方式调用本程序并携带参数即可：
 *     auto_code --run <script.ac> [--root <dir>]
 *   - 无界面执行 .ac 脚本，日志与生成文件清单输出到 stdout
 *   - 返回 0 表示脚本执行成功，非 0 表示执行出错
 *   - 内部自动完成 FunMgr::init / FunMgr::cleanup，调用方无需重复注册
 *
 * main.cpp 典型用法：
 *   if (const int h = runAcScriptHeadless(QApplication::arguments()); h >= 0) return h;
 */

#pragma once

#include <cstdio>
#include <QStringList>
#include <QTextStream>

#include "ac_engine.h"
#include "src/engine/function/fun_mgr.h"

/// 无界面执行脚本：解析 --run/--root，执行并把日志与生成文件打到 stdout
/// @param args 命令行参数（一般传入 QApplication::arguments()）
/// @return 0 表示脚本执行成功；非 0（脚本出错）返回 1；未提供 --run 返回 -1（由调用方走 GUI）
inline int runAcScriptHeadless(const QStringList &args) {
  const int runIdx = args.indexOf(QStringLiteral("--run"));
  if (runIdx < 0 || runIdx + 1 >= args.size()) return -1;

  const QString scriptPath = args.at(runIdx + 1);
  QString rootDir = QStringLiteral("d:/work/github/auto_code/file");
  const int rootIdx = args.indexOf(QStringLiteral("--root"));
  if (rootIdx >= 0 && rootIdx + 1 < args.size()) rootDir = args.at(rootIdx + 1);

  // 注册 C++ 函数 -> FunMgr（headless 独立初始化/释放，不依赖 GUI 路径）
  FunMgr::init();

  AcEngine::ins().setRootDir(rootDir);
  AcEngine::ins().setLogCallback([](const QString &text, bool isError) {
    QTextStream ts(stdout);
    ts << (isError ? QStringLiteral("[ERR] ") : QString()) << text << Qt::endl;
  });

  const QString runErr = AcEngine::ins().execute(scriptPath);

  QTextStream ts(stdout);
  ts << "[generated]" << Qt::endl;
  for (const QString &f : AcEngine::ins().generatedFiles()) ts << f << Qt::endl;
  if (!runErr.isEmpty()) ts << "[ERROR] " << runErr << Qt::endl;

  FunMgr::cleanup();
  return runErr.isEmpty() ? 0 : 1;
}