/**
 * @file util_json.h
 * @brief JSON 解析工具类 — 支持注释剥离
 *
 * 基于 Qt 官方 QJsonDocument 解析，在解析前自动剥离行注释和块注释。
 * 使用状态机正确处理字符串内部的注释符号，避免误删字符串内容（如 URL）。
 */

#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QString>

/**
 * @class UtilJson
 * @brief JSON 解析工具类，统一支持注释
 *
 * Qt 官方 QJsonDocument 严格遵循 RFC 8259，不支持注释。
 * 本工具类在解析前先用状态机剥离注释，使所有 JSON 入口行为一致。
 *
 * 使用方式：
 * @code
 *   QJsonParseError err;
 *   QJsonDocument doc = UtilJson::fromJson(text, &err);
 *   if (err.error != QJsonParseError::NoError) { ... }
 *
 *   // 或直接从文件加载
 *   QJsonDocument doc = UtilJson::loadFile("data.json", &err);
 * @endcode
 */
class UtilJson {
public:
  /**
   * @brief 将 JSON5 文本转换为标准 JSON 文本
   * @param text JSON5 文本
   * @param offsetMap 可选，输出"转换后文本索引 → 原始文本索引"的映射
   * @return 标准 JSON 文本
   *
   * 处理的 JSON5 特性：
   *   - `//` 行注释 和 `/* *\/` 块注释 → 剥离
   *   - 无引号 key（标识符）→ 加双引号
   *   - 单引号字符串 → 改为双引号
   *   - 尾随逗号（trailing comma）→ 删除
   *   - 十六进制数字 `0xFF` → 转为十进制
   *   - 前导小数点 `.5` → `0.5`
   *   - 后导小数点 `5.` → `5`
   *   - `Infinity` / `-Infinity` / `NaN` → 转为字符串字面量
   *   - 字符串跨行（反斜杠续行）→ 合并为单行
   */
  static QString json5ToJson(const QString &text, QVector<int> *offsetMap = nullptr);

  /**
   * @brief 剥离 JSON 文本中的注释
   * @param text 包含注释的 JSON 文本
   * @return 剥离注释后的纯 JSON 文本
   *
   * 支持行注释（双斜杠）和块注释（斜杠星号）。
   * 使用状态机正确识别字符串边界，不会删除字符串内部的注释符号。
   * 保留换行符以维持行号一致，便于错误定位。
   */
  static QString stripComments(const QString &text);

  /**
   * @brief 剥离 JSON 文本中的注释，同时记录位置映射
   * @param text 包含注释的 JSON 文本
   * @param offsetMap 输出参数，存储"剥离后文本索引 → 原始文本索引"的映射
   * @return 剥离注释后的纯 JSON 文本
   */
  static QString stripCommentsWithMap(const QString &text, QVector<int> &offsetMap);

  /**
   * @brief 解析 JSON 字符串（支持 JSON5 语法自动转换），错误位置已映射回原始文本
   * @param text JSON/JSON5 文本
   * @param error 解析错误信息（可选，传入则填充，offset 已修正为原始文本位置）
   * @return 解析得到的 JSON 文档
   */
  static QJsonDocument fromJson(const QString &text, QJsonParseError *error = nullptr);

  /**
   * @brief 解析 JSON 字节数组（自动剥离注释）
   * @param data JSON 字节数据（可包含注释，按 UTF-8 解码）
   * @param error 解析错误信息（可选，传入则填充）
   * @return 解析得到的 JSON 文档
   */
  static QJsonDocument fromJson(const QByteArray &data, QJsonParseError *error = nullptr);

  /**
   * @brief 从文件加载并解析 JSON（自动剥离注释）
   * @param filePath JSON 文件路径
   * @param error 解析错误信息（可选，传入则填充）
   * @return 解析得到的 JSON 文档；文件打开失败返回空文档且 error 置为 NoError
   */
  static QJsonDocument loadFile(const QString &filePath, QJsonParseError *error = nullptr);
};