/**
 * @file engine_ac.cpp
 * @brief 脚本引擎实现文件
 */

#include "engine_ac.h"

#include "ac_script/ac_engine.h"

#include <QFile>
#include <QFileInfo>

/**
 * @brief 获取单例实例
 *
 * 使用 C++11 线程安全的局部静态变量方式实现单例。
 * 在首次调用时创建唯一实例，程序退出时自动销毁。
 *
 * @return EngineAc 全局唯一实例的引用
 */
EngineAc &EngineAc::ins() {
  static EngineAc instance;
  return instance;
}

/**
 * @brief 以 UTF-8 编码读取文件全部内容
 * @param path 文件绝对路径
 * @return 文件内容字符串，读取失败返回空 QString
 *
 * 打开文件后一次性读取全部字节，以 UTF-8 编码转为 QString。
 * 文件不存在或打开失败时返回空字符串。
 */
QString EngineAc::readFileUtf8(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}

/**
 * @brief 执行 .ac 脚本文件
 * @param acFilePath .ac 文件的绝对路径
 * @return 错误信息，空字符串表示执行成功
 *
 * 执行流程：
 *   1. 读取 .ac 源文件，若文件为空则返回错误
 *   2. 获取 .ac 文件所在目录作为脚本目录
 *   3. 创建 ScriptParser 并配置环境
 *   4. 调用 parser.parse() 将源码解析为 AST
 *   5. 调用 parser.execute() 解释执行 AST
 *   6. 收集生成的文件列表
 *   7. 返回解析或执行过程中的错误信息
 */
QString EngineAc::execute(const QString &acFilePath) {
  // 步骤 1：读取 .ac 源文件
  QString source = readFileUtf8(acFilePath);
  if (source.isEmpty())
    return QStringLiteral("failed to read file: %1").arg(acFilePath);

  // 步骤 2：获取脚本所在目录
  QFileInfo fi(acFilePath);
  QString dir = fi.absolutePath();

  // 步骤 3：创建解析器并配置环境
  ScriptParser parser;
  parser.setScriptDir(dir);
  if (!m_rootDir.isEmpty())
    parser.setRootDir(m_rootDir);
  if (m_logCallback)
    parser.setLogCallback(m_logCallback);

  // 步骤 4：解析源码为 AST
  if (!parser.parse(source))
    return QStringLiteral("parse error: %1").arg(parser.error());

  // 步骤 5：解释执行 AST 并收集生成的文件
  QJsonValue result = parser.execute();
  m_generatedFiles = parser.generatedFiles();
  if (!parser.error().isEmpty())
    return parser.error();

  return {};
}