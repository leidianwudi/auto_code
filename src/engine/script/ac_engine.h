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

#include <functional>

/**
 * @class AcEngine
 * @brief .ac 脚本引擎 — 解析并执行 .ac 脚本文件
 *
 * 单例类，负责整个脚本生命周期的管理：
 * - 读取 .ac 源文件
 * - 调用 AcExecutor 解析并执行脚本
 * - 收集执行过程中生成的文件列表
 * - 通过回调机制将 print() 输出传递给 UI
 *
 * 使用示例：
 * @code
 * AcEngine &engine = AcEngine::ins();
 * engine.setRootDir("/project/root");
 * engine.setLogCallback([](const QString &text, bool isError) {
 *   qDebug() << (isError ? "[ERR]" : "[LOG]") << text;
 * });
 * QString err = engine.execute("/project/root/main.ac");
 * if (err.isEmpty()) {
 *   qDebug() << "生成文件:" << engine.generatedFiles();
 * }
 * @endcode
 */
class AcEngine {
public:
  /**
   * @brief 获取单例实例
   * @return AcEngine 全局唯一实例的引用
   */
  static AcEngine &ins();

  /**
   * @brief 日志回调类型
   * 脚本中 print() 的输出通过此回调通知 UI 显示
   * @param text    日志文本
   * @param isError 是否为错误信息（true=错误，false=普通日志）
   */
  using LogCallback = std::function<void(const QString &text, bool isError)>;

  /**
   * @brief 设置日志回调
   * @param cb 回调函数对象，执行脚本后 print() 的输出会触发此回调
   */
  void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

  /**
   * @brief 设置项目根目录
   * @param dir tree.config 所在目录的绝对路径
   *
   * 根目录用于脚本中访问项目文件时的相对路径基准。
   * 如果不设置，默认使用 .ac 文件所在目录。
   */
  void setRootDir(const QString &dir) { m_rootDir = dir; }

  /**
   * @brief 执行 .ac 脚本文件
   * @param acFilePath .ac 文件的绝对路径
   * @return 错误信息，空字符串表示执行成功
   *
   * 执行流程：
   *   1. 读取 .ac 文件内容
   *   2. 创建 AcExecutor 并配置环境（脚本目录、根目录、日志回调）
   *   3. 调用 executor.parse() 解析为 AST
   *   4. 调用 executor.execute() 解释执行
   *   5. 收集生成的文件列表到 m_generatedFiles
   *   6. 返回解析或执行过程中的错误信息
   */
  QString execute(const QString &acFilePath);

  /**
   * @brief 获取本次执行生成的文件列表
   * @return 文件路径字符串列表
   *
   * 在 execute() 调用之后调用，获取脚本执行过程中
   * 通过 write() 等内置函数生成的所有文件路径。
   */
  QStringList generatedFiles() const { return m_generatedFiles; }

private:
  /// @brief 构造函数私有化（单例模式）
  AcEngine() = default;

  /**
   * @brief 以 UTF-8 编码读取文件全部内容
   * @param path 文件绝对路径
   * @return 文件内容字符串，读取失败返回空字符串
   */
  QString readFileUtf8(const QString &path);

  /// @brief 项目根目录（tree.config 所在目录）
  QString m_rootDir;
  /// @brief 本次执行生成的文件路径列表
  QStringList m_generatedFiles;
  /// @brief 日志回调函数
  LogCallback m_logCallback;
};