/**
 * @file json_global_enum_model.h
 * @brief .jsonglobalenum 文件数据模型
 *
 * 定义全局枚举集中管理的配置文件数据结构。
 * 一个 .jsonglobalenum 文件包含多个全局枚举（enums），供各表/各项目共用：
 *   - name    - 枚举名称（snake_case，后端枚举名 = Enum + 帕斯卡(name)）
 *   - remark  - 枚举说明
 *   - options - 选项列表（键名/显示文本/实际值/值类型）
 *
 * 生成链路两侧消费同一份配置：
 *   - crud_nest 后端：main_api.ac 生成公共枚举文件 common/enum/global_enum_.ts
 *   - admin_vue 前端：main_admin.ac 同步生成 api/global_enum.jsonsource
 *     （静态数据源），再走 source.tpl 渲染 src/api/source/global_enum.ts
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// ════════════════════════════════════════════════════════════
//  .jsonglobalenum 序列化键名常量
//  model（toJson/fromJson）与 UI 层共用，避免硬编码字符串，
//  防止读写键名不一致导致数据丢失。
// ════════════════════════════════════════════════════════════

/// .jsonglobalenum 文件的 JSON 键名常量
namespace JsonGlobalEnumKey {
// 顶层结构
inline constexpr const char *kEnums = "enums";
// 枚举通用
inline constexpr const char *kId = "id";
inline constexpr const char *kName = "name";
inline constexpr const char *kRemark = "remark";
// 选项
inline constexpr const char *kOptions = "options";
inline constexpr const char *kKey = "key";              ///< 枚举成员名（后端 TS 标识符）
inline constexpr const char *kLabel = "label";          ///< 显示文本（前端下拉框文字）
inline constexpr const char *kValue = "value";          ///< 实际值（字符串保真存储）
inline constexpr const char *kValueType = "valueType";  ///< 实际值类型（""=字符串 / "number"=数字）
}  // namespace JsonGlobalEnumKey

/**
 * @struct JsonGlobalEnumOption
 * @brief 全局枚举选项（枚举成员）
 *
 * key 供后端生成 TS 枚举成员名（转帕斯卡），label/value 供前端下拉框使用；
 * valueType 标记实际值类型，决定代码生成时 value 字面量是否加引号。
 */
struct JsonGlobalEnumOption {
  QString key;        ///< 枚举成员名（如 enable，生成 TS 成员 Enable）
  QString label;      ///< 显示文本
  QString value;      ///< 实际值（字符串保真存储）
  QString valueType;  ///< 实际值类型（""=字符串 / "number"=数字）

  QJsonObject toJson() const;
  static JsonGlobalEnumOption fromJson(const QJsonObject &obj);
};

/**
 * @struct JsonGlobalEnum
 * @brief .jsonglobalenum 中的一条全局枚举
 */
struct JsonGlobalEnum {
  QString id;                             ///< 枚举唯一标识（jsonvue 引用同步生成的数据源时使用）
  QString name;                           ///< 枚举名称（snake_case，如 is_enable → EnumIsEnable）
  QString remark;                         ///< 枚举说明
  QVector<JsonGlobalEnumOption> options;  ///< 选项列表

  QJsonObject toJson() const;
  static JsonGlobalEnum fromJson(const QJsonObject &obj);
};

/**
 * @class JsonGlobalEnumConfig
 * @brief .jsonglobalenum 文件完整配置
 */
class JsonGlobalEnumConfig {
public:
  /// 枚举数组
  QVector<JsonGlobalEnum> enums;

  /// 判断是否为空配置
  bool isEmpty() const { return enums.isEmpty(); }

  /// 序列化为 JSON 文档字符串（紧凑格式）
  QString toJsonString() const;
  /// 将给定的完整 JSON 对象序列化为 JSON 文档字符串（紧凑格式）
  static QString toJsonString(const QJsonObject &root);

  /// 从 JSON 字符串反序列化（失败时返回空配置）
  static JsonGlobalEnumConfig fromJsonString(const QString &jsonStr);
  /// 从 JSON 对象反序列化
  static JsonGlobalEnumConfig fromJson(const QJsonObject &obj);

  /// 转为 JSON 对象
  QJsonObject toJsonObject() const;
};
