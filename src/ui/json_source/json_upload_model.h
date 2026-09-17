/**
 * @file json_upload_model.h
 * @brief .jsonupload 文件数据模型
 *
 * 定义图片上传预设集中管理的配置文件数据结构。
 * 一个 .jsonupload 文件包含多个上传预设（uploads），供各 jsonvue 界面
 * 编辑页"图片字段"共用：
 *   - 每个预设声明上传接口（url/method/fileField/附加 form 参数 params）、
 *     响应提取路径（responsePath，如 data.url）与张数限制（maxCount）
 *   - 编辑页保存时，图片路径（单图=字符串 / 多图=字符串数组）作为普通表单
 *     字段随 JSON body 一起提交，无需额外编排接口
 *
 * jsonvue 的图片字段通过引用本文件（uploadSourceFile）+ 预设 id（uploadSourceId）
 * 复用这里的上传配置，无需在每字段重复填写。
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// ════════════════════════════════════════════════════════════
//  .jsonupload 序列化键名常量
//  model（toJson/fromJson）与 UI 层共用，避免硬编码字符串，
//  防止读写键名不一致导致数据丢失。
// ════════════════════════════════════════════════════════════

/// .jsonupload 文件的 JSON 键名常量
namespace JsonUploadKey {
// 顶层结构
inline constexpr const char *kUploads = "uploads";
// 上传预设通用
inline constexpr const char *kId = "id";
inline constexpr const char *kRemark = "remark";
inline constexpr const char *kUrl = "url";
inline constexpr const char *kMethod = "method";
inline constexpr const char *kFileField = "fileField";
// 附加 form 参数
inline constexpr const char *kParams = "params";
inline constexpr const char *kName = "name";
inline constexpr const char *kValue = "value";
inline constexpr const char *kValueType = "valueType";  ///< ""=字符串 / "number"=数字
// 响应提取与张数
inline constexpr const char *kResponsePath = "responsePath";  ///< 点分提取路径，如 data.url
inline constexpr const char *kMaxCount = "maxCount";          ///< 最多张数；1=单图 / >1=多图
}  // namespace JsonUploadKey

/// 上传预设的提交值形态常量（预设级 valueType）
namespace JsonUploadValueType {
inline constexpr const char *kAuto = "";        ///< 按 maxCount 推导（1→字符串 / >1→数组）
inline constexpr const char *kString = "string";  ///< 强制单值（字符串）
inline constexpr const char *kArray = "array";    ///< 强制数组（多图字符串数组）
}  // namespace JsonUploadValueType

/**
 * @struct JsonUploadParam
 * @brief 上传接口的附加 form-data 参数（如 type=0）
 *
 * value 始终以字符串保真存储，valueType 只做类型标记，
 * 供代码生成时决定参数字面量是否加引号（数字 0 vs 字符串 '0'）。
 */
struct JsonUploadParam {
  QString name;        ///< 参数名，如 "type"
  QString value;       ///< 参数值（字符串保真存储）
  QString valueType;   ///< 值类型（""=字符串 / "number"=数字）

  QJsonObject toJson() const;
  static JsonUploadParam fromJson(const QJsonObject &obj);
};

/**
 * @struct JsonUpload
 * @brief .jsonupload 中的一条上传预设
 */
struct JsonUpload {
  /// 预设唯一标识（jsonvue 通过 uploadSourceId 引用，排序变化时保持稳定）
  QString id;
  /// 预设说明（备注）
  QString remark;
  /// 上传接口相对路径（如 "upload/image"，生成代码走目标项目 axios baseURL）
  QString url;
  /// 请求方式（默认 POST）
  QString method = QStringLiteral("POST");
  /// form-data 中文件字段名
  QString fileField = QStringLiteral("file");
  /// 附加 form 参数（如 type=0）
  QVector<JsonUploadParam> params;
  /// 响应中图片路径的点分提取路径（如 data.url）
  QString responsePath = QStringLiteral("data.url");
  /// 最多上传张数；1=单图（表单值为字符串），>1=多图（字符串数组）
  int maxCount = 1;
  /// 提交值形态覆盖（""=按 maxCount 推导 / "string" / "array"）
  QString valueType;

  /// 是否为多图上传（表单值为字符串数组）
  bool isMulti() const {
    return valueType == QString::fromLatin1(JsonUploadValueType::kArray) ||
           (valueType.isEmpty() && maxCount > 1);
  }

  /// 序列化为 JSON
  QJsonObject toJson() const;
  /// 从 JSON 反序列化
  static JsonUpload fromJson(const QJsonObject &obj);
};

/**
 * @class JsonUploadConfig
 * @brief .jsonupload 文件完整配置
 */
class JsonUploadConfig {
public:
  /// 上传预设数组
  QVector<JsonUpload> uploads;

  /// 判断是否为空配置
  bool isEmpty() const { return uploads.isEmpty(); }

  /// 按 id 查找预设，找不到返回 -1
  int indexOfUpload(const QString &id) const;
  /// 按 id 查找预设，找不到返回 nullptr
  const JsonUpload *uploadById(const QString &id) const;

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;
  /// 将给定的完整 JSON 对象序列化为 JSON 文档字符串（紧凑格式）
  static QString toJsonString(const QJsonObject &root);

  /// 从 JSON 字符串反序列化（失败时 error 非空）
  static JsonUploadConfig fromJsonString(const QString &jsonStr, QString *error = nullptr);
  /// 从 JSON 对象反序列化
  static JsonUploadConfig fromJson(const QJsonObject &obj);

  /// 转为 JSON 对象
  QJsonObject toJsonObject() const;
};
