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
#include <QJsonObject>
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
   * @brief 按原文顺序提取响应中第一个数组首元素的顶层键名列表
   * @param jsonText 原始 JSON 响应文本（严格 JSON，无需支持注释/JSON5）
   * @param ok 输出参数：提取成功返回 true（找不到数组/首元素非对象/无有效键返回 false）
   * @return 键名列表（原始顺序）。QJsonObject 迭代按键字母序，无法还原服务端
   *         返回的属性顺序（通常是数据库表结构列顺序），需要保序时用本函数
   *
   * 定位规则：找到第一个不在字符串内的 '['，取其后第一个 '{' 作为首行对象，
   * 覆盖 { data: { list: [...] } }、{ data: [...] }、[...] 三种响应包裹结构；
   * 解析失败时调用方应回退到 QJsonObject::keys()（字母序）。
   */
  static QStringList objectKeysInOrder(const QString &jsonText, bool *ok = nullptr);

  /**
   * @brief 从文件加载并解析 JSON（自动剥离注释）
   * @param filePath JSON 文件路径
   * @param error 解析错误信息（可选，传入则填充，offset 已修正为原始文本位置）
   * @return 解析得到的 JSON 文档；文件打开失败返回空文档且 error 置为 NoError
   */
  static QJsonDocument loadFile(const QString &filePath, QJsonParseError *error = nullptr);

  /**
   * @brief 读取整个文件文本（UTF-8 解码；不解析）
   * @param filePath 文件路径
   * @return 文件全文；打开失败返回空串（与 loadFile 的失败语义一致，由调用方判断）
   */
  static QString readTextFile(const QString &filePath);

  /**
   * @brief 计算 JSON 对象的内容指纹（MD5，紧凑序列化）
   * @param obj JSON 对象
   * @return 指纹哈希；用于判断内容是否变化（如可视化表单跳过重建）
   */
  static QByteArray fingerprint(const QJsonObject &obj);

private:
  /**
   * @brief 将 JSON5 文本转换为标准 JSON 文本（fromJson 的内部实现）
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
   * @brief 剥离 JSON 文本中的注释并记录位置映射（fromJson 的内部实现）
   */
  static QString stripCommentsWithMap(const QString &text, QVector<int> &offsetMap);
};