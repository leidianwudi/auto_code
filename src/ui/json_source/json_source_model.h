/**
 * @file json_source_model.h
 * @brief .jsonsource 文件数据模型
 *
 * 定义下拉框数据源集中管理的配置文件数据结构。
 * 一个 .jsonsource 文件包含多个数据源（sources），供各 jsonvue 界面下拉框共用：
 *   - 静态数据源（static）：直接配置 显示文本/实际值 选项列表
 *   - 动态数据源（dynamic）：配置请求 URL、显示/实际值字段等，与 jsonvue 下拉框配置一致
 *
 * jsonvue 的下拉框通过引用本文件（selectSourceFile）+ 数据源 id（selectSourceId）
 * 复用这里配置的数据，无需重复填写 URL。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// ════════════════════════════════════════════════════════════
//  .jsonsource 序列化键名常量
//  model（toJson/fromJson）与 UI 层共用，避免硬编码字符串，
//  防止读写键名不一致导致数据丢失。
// ════════════════════════════════════════════════════════════

/// .jsonsource 文件的 JSON 键名常量
namespace JsonSourceKey {
// 顶层结构
inline constexpr const char *kSources = "sources";
// 数据源通用
inline constexpr const char *kId = "id";
inline constexpr const char *kType = "type";
inline constexpr const char *kRemark = "remark";
// 动态数据源
inline constexpr const char *kUrl = "url";
inline constexpr const char *kMethod = "method";
inline constexpr const char *kLabelField = "labelField";
inline constexpr const char *kValueField = "valueField";
inline constexpr const char *kPaged = "paged";
inline constexpr const char *kPageKey = "pageKey";
inline constexpr const char *kPageSizeKey = "pageSizeKey";
inline constexpr const char *kPageSize = "pageSize";
inline constexpr const char *kSearchTitle = "searchTitle";
inline constexpr const char *kSearchField = "searchField";
// 静态数据源选项
inline constexpr const char *kOptions = "options";
inline constexpr const char *kLabel = "label";
inline constexpr const char *kValue = "value";
}  // namespace JsonSourceKey

/// 数据源类型字符串常量
namespace JsonSourceType {
inline constexpr const char *kStatic = "static";
inline constexpr const char *kDynamic = "dynamic";
}  // namespace JsonSourceType

/**
 * @struct JsonSourceOption
 * @brief 静态数据源选项（显示文本 + 实际值）
 */
struct JsonSourceOption {
  QString label;  ///< 显示文本
  QString value;  ///< 实际值

  QJsonObject toJson() const;
  static JsonSourceOption fromJson(const QJsonObject &obj);
};

/**
 * @struct JsonSource
 * @brief .jsonsource 中的一条数据源
 *
 * type == "static" 时使用 options；type == "dynamic" 时使用 url/method/字段等。
 */
struct JsonSource {
  /// 数据源唯一标识（jsonvue 通过 selectSourceId 引用，排序变化时保持稳定）
  QString id;
  /// 数据源类型（"static" / "dynamic"）
  QString type = QString::fromLatin1(JsonSourceType::kDynamic);
  /// 数据源说明（备注）
  QString remark;

  // ── 动态数据源字段 ──
  QString url;                       ///< 请求 URL
  QString method = QString::fromLatin1("POST");  ///< 请求方式（GET/POST）
  QString labelField;                ///< 显示文本字段名
  QString valueField;                ///< 实际值字段名
  bool paged = false;                ///< 是否查询分页加载
  QString pageKey = QStringLiteral("page");      ///< 页码参数名
  QString pageSizeKey = QStringLiteral("pageSize");  ///< 页大小参数名
  int pageSize = 20;                 ///< 默认页大小
  QString searchTitle;               ///< 查询标题（搜索框提示）
  QString searchField;               ///< 字段名（搜索参数 key）

  // ── 静态数据源字段 ──
  QVector<JsonSourceOption> options;   ///< 静态选项列表

  /// 是否为静态数据源
  bool isStatic() const { return type == QString::fromLatin1(JsonSourceType::kStatic); }
  /// 是否为动态数据源
  bool isDynamic() const { return !isStatic(); }

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static JsonSource fromJson(const QJsonObject &obj);
};

/**
 * @class JsonSourceConfig
 * @brief .jsonsource 文件完整配置
 */
class JsonSourceConfig {
public:
  /// 数据源数组
  QVector<JsonSource> sources;

  /// 判断是否为空配置
  bool isEmpty() const { return sources.isEmpty(); }

  /// 按 id 查找数据源，找不到返回 -1
  int indexOfSource(const QString &id) const;
  /// 按 id 查找数据源，找不到返回 nullptr
  const JsonSource *sourceById(const QString &id) const;

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;
  /// 将给定的完整 JSON 对象序列化为 JSON 文档字符串（紧凑格式）
  static QString toJsonString(const QJsonObject &root);

  /// 从 JSON 字符串反序列化（失败时 error 非空）
  static JsonSourceConfig fromJsonString(const QString &jsonStr, QString *error = nullptr);
  /// 从 JSON 对象反序列化
  static JsonSourceConfig fromJson(const QJsonObject &obj);

  /// 转为 JSON 对象
  QJsonObject toJsonObject() const;
};
