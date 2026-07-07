/**
 * @file fun_builtin.h
 * @brief 内置函数实现 — 脚本可直接调用的顶级函数
 *
 * 内置函数在 .ac 脚本中直接调用，无需类名前缀。例如：
 * @code
 *   renderTpl("template.tpl", {name: "world"})
 *   readFile("input.txt")
 *   writeFile("output.txt", "content")
 *   print("hello")
 *   let files = getCheckedFiles()
 *   let merged = merge({a: 1}, {b: 2})
 *   let name = basename("/path/to/file.txt")  // → "file"
 * @endcode
 *
 * 这些函数通过 FunMgr 的 "builtin" 伪类注册，由 AcInterpreter::callBuiltin()
 * 统一路由。函数需要的解释器上下文（路径、日志回调等）通过 BuiltinContext
 * 在每次执行前注入。
 */

#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <functional>

/**
 * @brief 解释器上下文 — 内置函数需要的运行时状态
 *
 * 由 AcInterpreter::execute() 在每次执行前调用 FunBuiltin::setContext() 注入。
 * 每个字段对应 AcInterpreter 的一个成员变量。
 */
struct BuiltinContext {
  QString scriptDir;                                       ///< 脚本所在目录
  QString rootDir;                                         ///< 项目根目录
  std::function<void(const QString &, bool)> logCallback;  ///< print() 日志回调
  QStringList *generatedFiles = nullptr;                   ///< write() 生成的文件列表
  int currentLine = 0;                                     ///< 当前执行的 .ac 行号
};

/**
 * @brief 内置函数实现类
 *
 * 所有函数签名统一为 FunMgr::FunPtr：QJsonValue(const QJsonArray &)
 * 参数已在 AcInterpreter 中通过 evalExpr() 求值完毕，此处直接使用。
 * 每个函数实现对应 ac_language.h 中 AcBuiltin 命名空间的一个常量。
 */
class FunBuiltin {
public:
  /// 注册所有内置函数到 FunMgr（"builtin" 伪类）
  static void init();

  /// 注入解释器上下文（每次脚本执行前由 AcInterpreter 调用）
  static void setContext(const BuiltinContext &ctx);

  /// 设置当前执行的行号（printLog/printError 输出时带上行号）
  static void setCurrentLine(int line);

private:
  static BuiltinContext s_ctx;

  /// 渲染模板文件，返回渲染后的字符串
  /// @param args [0] 模板路径, [1] 数据对象
  static QJsonValue renderTpl(const QJsonArray &args);

  /// 读文件（委托 FunFile::read）
  /// @param args [0] 文件路径
  static QJsonValue readFile(const QJsonArray &args);

  /// 写文件（委托 FunFile::write），自动创建父目录并追踪生成文件
  /// @param args [0] 文件路径, [1] 文件内容
  static QJsonValue writeFile(const QJsonArray &args);

  /// 打印日志到 UI 输出面板
  /// @param args [0] 要打印的文本
  static QJsonValue printLog(const QJsonArray &args);

  /// 打印错误到 UI 输出面板（红色显示）
  /// @param args [0] 要打印的错误文本
  static QJsonValue printError(const QJsonArray &args);

  /// 获取 tree.config 中勾选的文件列表（绝对路径）
  static QJsonValue getCheckedFiles(const QJsonArray &args);

  /// 合并两个 JSON 对象（浅合并，b 覆盖 a 的同名键）
  /// @param args [0] 基础对象, [1] 覆盖对象
  static QJsonValue merge(const QJsonArray &args);

  /// 获取文件名（不含扩展名）
  /// @param args [0] 文件路径
  static QJsonValue basename(const QJsonArray &args);
};