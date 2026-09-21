/**
 * @file bench_main.cpp
 * @brief 基准测试套件（阶段 4）— 对比解释器 / 字节码 VM 性能并校验结果一致
 *
 * 用法：
 *   ac_bench [脚本目录或单个 .ac 文件] [每模式测量次数=N]
 *
 * 流程：每脚本 预热 1 次 → N 次测量 → 取 min 与 median → 打印对比表。
 * 两模式的执行结果（序列化文本 + 错误串）必须完全一致，否则返回非零。
 *
 * 构建：cmake --build <build> --target ac_bench --config RelWithDebInfo
 */

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStringList>
#include <algorithm>
#include <cstdio>
#include <vector>

#include "src/engine/script/ac_executor.h"

namespace {

/// 结果序列化（与对拍测试同规则）：标量/数组/对象统一转字符串
QString toJsonText(const QJsonValue &v) {
  if (v.isObject())
    return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
  if (v.isArray())
    return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
  return v.toVariant().toString();
}

struct RunOutcome {
  bool ok = false;
  QString signature;   ///< 结果序列化 / 错误串（一致性命中依据）
  double timeMs = 0;   ///< 单次 execute() 耗时
};

/// 单次运行：parse 不计时（只测执行），execute 计时
RunOutcome runOnce(const QString &src, AcExecMode mode) {
  AcExecutor ex;
  ex.setExecMode(mode);
  if (!ex.parse(src)) {
    RunOutcome o;
    o.signature = QStringLiteral("parse:%1").arg(ex.error());
    return o;
  }
  QElapsedTimer t;
  t.start();
  const QJsonValue r = ex.execute();
  const double ms = double(t.nsecsElapsed()) / 1e6;
  const QString e = ex.error();
  RunOutcome o;
  if (!e.isEmpty()) {
    o.signature = QStringLiteral("err:%1").arg(e);
  } else {
    o.signature = toJsonText(r);
    o.ok = true;
  }
  o.timeMs = ms;
  return o;
}

/// 跑 N 次测量（含 1 次预热），返回 min/median
struct Timing {
  double minMs = 0;
  double medianMs = 0;
  RunOutcome outcome;
};

Timing benchMode(const QString &src, AcExecMode mode, int n) {
  Timing result;
  // 预热
  runOnce(src, mode);
  std::vector<double> times;
  times.reserve(n);
  for (int i = 0; i < n; ++i) {
    const RunOutcome o = runOnce(src, mode);
    if (i == 0) result.outcome = o;
    times.push_back(o.timeMs);
  }
  std::sort(times.begin(), times.end());
  result.minMs = times.front();
  result.medianMs = times[times.size() / 2];
  return result;
}

}  // namespace

int main(int argc, char **argv) {
  QString target = argc > 1 ? QString::fromLocal8Bit(argv[1])
                            : QStringLiteral(PROJECT_SOURCE_DIR "/tests/bench/scripts");
  int n = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 5;
  if (n < 1) n = 1;

  // 输入归一为脚本文件列表（目录 or 单一文件）
  QStringList scripts;
  const QFileInfo targetInfo(target);
  if (targetInfo.isDir()) {
    const QDir dir(target);
    const QStringList files =
        dir.entryList(QStringList() << QStringLiteral("*.ac"), QDir::Files, QDir::Name);
    for (const auto &f : files) scripts.append(dir.absoluteFilePath(f));
  } else if (targetInfo.isFile()) {
    scripts.append(targetInfo.absoluteFilePath());
  } else {
    std::printf("ac_bench: 路径不存在或不是目录/脚本: %s\n", target.toUtf8().constData());
    return 2;
  }
  if (scripts.isEmpty()) {
    std::printf("ac_bench: 目录下没有 .ac 脚本: %s\n", target.toUtf8().constData());
    return 2;
  }

  std::printf("%-16s %12s %10s %8s   %s\n", "脚本", "解释器min(ms)", "VMmin(ms)", "倍率", "一致性");
  std::printf("%s\n",
              QStringLiteral("──────────────────────────────────────────────────────").toUtf8().constData());
  int failed = 0;
  for (const auto &path : scripts) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
      std::printf("%-16s 读取失败\n", QFileInfo(path).fileName().toUtf8().constData());
      ++failed;
      continue;
    }
    const QString src = QString::fromUtf8(f.readAll());

    const Timing interp = benchMode(src, AcExecMode::kInterpreter, n);
    const Timing vm = benchMode(src, AcExecMode::kBytecode, n);

    const bool same = interp.outcome.ok == vm.outcome.ok &&
                      interp.outcome.signature == vm.outcome.signature;
    if (!same) {
      std::printf("%-16s 结果不一致! interp='%s' vm='%s'\n",
                  QFileInfo(path).fileName().toUtf8().constData(),
                  interp.outcome.signature.toUtf8().constData(),
                  vm.outcome.signature.toUtf8().constData());
      ++failed;
      continue;
    }
    const double ratio = vm.minMs > 0 ? interp.minMs / vm.minMs : 0.0;
    std::printf("%-16s %12.2f %10.2f %7.2fx   %s\n",
                QFileInfo(path).fileName().toUtf8().constData(), interp.minMs, vm.minMs, ratio,
                same ? "✓" : "✗");
  }

  std::printf("%s\n",
              QStringLiteral("──────────────────────────────────────────────────────").toUtf8().constData());
  std::printf("ac_bench: %d 个脚本，%d 个不一致\n", scripts.size(), failed);
  return failed == 0 ? 0 : 1;
}