/**
 * @file ac_engine.h
 * @brief .ac 脚本引擎头文件 — 解析并执行 .ac 脚本文件
 *
 * AcEngine 是 auto_code 的脚本执行引擎，以单例模式对外提供服务。
 * 核心流程：
 *   1. 设置项目根目录（setRootDir）
 *   2. 注册日志回调（setLogCallback）
 *   3. 调用 execute() 传入 .ac 文件路径，解析并执行脚本
 *   4. 通过 generatedFiles() 获取本次生成的文件列表
 *
 * 脚本解析和执行的具体工作委托给 AcExecutor 完成。
 */

#pragma once

#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

#include "src/util/design/singleton.h"

class AcDebugger;

/**
 * @class AcEngine
 * @brief .ac 脚本引擎 — 解析并执行 .ac 脚本文件
 *
 * 单例类（继承 Singleton<AcEngine>），负责整个脚本生命周期的管理：
 * - 读取 .ac 源文件
 * - 调用 AcExecutor 解析并执行脚本
 * - 收集执行过程中生成的文件列表
 * - 通过回调机制将 print() 输出传递给 UI
 */
class AcEngine : public Singleton<AcEngine> {
public:
  AcEngine() = default;

  using LogCallback = std::function<void(const QString &text, bool isError)>;

  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }
  void setRootDir(const QString &dir) { m_rootDir = dir; }
  /// @brief 设置调试器（为空则不启用调试）
  void setDebugger(AcDebugger *dbg) { m_debugger = dbg; }

  /// @brief 请求取消正在执行的脚本（工作线程中由解释器轮询检查）
  void requestCancel() { m_cancelRequested.store(true); }
  /// @brief 复位取消标志（每次执行前调用）
  void resetCancel() { m_cancelRequested.store(false); }
  /// @brief 是否已请求取消
  bool isCancelRequested() const { return m_cancelRequested.load(); }

  /**
   * @brief 执行 .ac 脚本文件
   * @param acFilePath .ac 文件的绝对路径
   * @return 错误信息，空字符串表示执行成功
   */
  QString execute(const QString &acFilePath);

  QStringList generatedFiles() const { return m_generatedFiles; }

private:
  QString readFileUtf8(const QString &path);

  QString m_rootDir;
  QStringList m_generatedFiles;
  LogCallback m_logCallback;
  AcDebugger *m_debugger = nullptr;            ///< 调试器（为空则不启用调试）
  std::atomic<bool> m_cancelRequested{false};  ///< 取消标志（工作线程执行时检查）
};