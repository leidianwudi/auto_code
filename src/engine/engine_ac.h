#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class EngineAc {
public:
  static EngineAc &ins();

  /// 日志回调：UI 设置后，脚本中 print() 的输出会通过此回调通知
  /// @param text  日志文本
  /// @param isError  是否为错误信息
  using LogCallback = std::function<void(const QString &text, bool isError)>;
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

  /**
   * @brief 设置项目根目录（tree.config 所在目录）
   */
  void setRootDir(const QString &dir) { m_rootDir = dir; }

  /**
   * @brief 执行 main.ac 脚本
   * @param mainAcPath main.ac 文件的绝对路径
   * @return 错误信息，空字符串表示成功
   */
  QString execute(const QString &mainAcPath);

  /// 获取本次执行生成的文件列表
  QStringList generatedFiles() const { return m_generatedFiles; }

private:
  EngineAc() = default;
  QString readFileUtf8(const QString &path);
  QString m_rootDir;
  QStringList m_generatedFiles;
  LogCallback m_logCallback;
};