/**
 * @file fun_const.h
 * @brief 函数名称常量 — 集中管理所有支持的 C++ 函数名及子命令
 *
 * 分两类管理：
 * 1. AcBuiltin: .ac 脚本直接调用的顶级内置函数（C++ 后端实现）
 * 2. AcClasses: 通过 call("类名", "方法名") 调用的 C++ 类及其方法
 *
 * 所有函数名称统一在此声明为 constexpr 常量，避免散布裸字符串，
 * 重命名时只需改此处，编译期即可发现所有引用点。
 */

#pragma once

#include <QString>
#include <QStringList>

/// @brief .ac 脚本顶级内置函数（直接调用）常量
///
/// 所有可以在 .ac 脚本中直接写函数名调用的内置函数，
/// 全部由 C++ 引擎后端直接实现，不需要通过 call() 路由。
namespace AcBuiltin {
/// @brief 调用 C++ 注册函数：call("类名", "方法名", [参数])
inline constexpr const char *kCall = "call";
/// @brief 读取 JSON 文件：readJson("path.json") → 返回 QJsonValue
inline constexpr const char *kReadJson = "readJson";
/// @brief 渲染模板文件：renderTpl("template.tpl", data) → 返回渲染文本
inline constexpr const char *kRenderTpl = "renderTpl";
/// @brief 写入文本文件：write("path.txt", content) → 返回 bool 是否成功
inline constexpr const char *kWrite = "write";
/// @brief 打印日志到控制台：print("message") → 无返回值
inline constexpr const char *kPrint = "print";
/// @brief 获取 tree.config 中勾选的文件列表：getCheckedFiles() → 返回字符串数组
inline constexpr const char *kGetCheckedFiles = "getCheckedFiles";
/// @brief 合并两个 JSON 对象：merge(obj1, obj2) → 返回新对象，后者覆盖前者
inline constexpr const char *kMerge = "merge";
/// @brief 获取文件的无后缀基名：basename("path/file.cpp") → 返回 "file"
inline constexpr const char *kBasename = "basename";
} // namespace AcBuiltin

/// @brief .ac 支持的内置函数名列表（由 AcBuiltin 常量组装）
///
/// 供解析器校验、语法高亮、自动补全使用。新增函数只需改 AcBuiltin
/// 命名空间和本列表，所有模块自动同步。
inline const QStringList kAcBuiltinFunctions = {
    QString(AcBuiltin::kCall),      QString(AcBuiltin::kReadJson),
    QString(AcBuiltin::kRenderTpl), QString(AcBuiltin::kWrite),
    QString(AcBuiltin::kPrint),     QString(AcBuiltin::kGetCheckedFiles),
    QString(AcBuiltin::kMerge),     QString(AcBuiltin::kBasename),
};

/// @brief C++ 类方法（需 call() 路由）常量
///
/// 所有可以通过 call("类名", "方法名") 调用的 C++ 类及其方法，
/// 名称都在这里集中定义。新增类/方法只需改此处。
struct AcClasses {
  // ── 类名 ──
  static constexpr const char *kClsStr = "str";   ///< 字符串处理类
  static constexpr const char *kClsDb = "db";     ///< 数据库查询类
  static constexpr const char *kClsFile = "file"; ///< 文件读写类

  // ── FunStr 子命令 ──
  static constexpr const char *kToLowerCase = "toLowerCase"; ///< 转小写
  static constexpr const char *kToUpperCase = "toUpperCase"; ///< 转大写
  static constexpr const char *kTrim = "trim";               ///< 去首尾空白
  static constexpr const char *kCapitalize = "capitalize";   ///< 首字母大写
  static constexpr const char *kSubstring = "substring";     ///< 子串
  static constexpr const char *kReplace = "replace";         ///< 替换

  /// 返回所有支持的字符串子命令列表
  static QStringList stringMethods();

  // ── FunDb 子命令 ──
  static constexpr const char *kTableSchema = "tableSchema"; ///< 获取表结构
  static constexpr const char *kQuery = "query";             ///< 执行 SQL
  static constexpr const char *kConnect = "connect";         ///< 连接数据库
  static constexpr const char *kDisconnect = "disconnect";   ///< 断开连接

  /// 返回所有支持的数据库子命令列表
  static QStringList dbMethods();

  // ── FunFile 子命令 ──
  static constexpr const char *kRead = "read";   ///< 读文件
  static constexpr const char *kWrite = "write"; ///< 写文件

  /// 返回所有支持的文件子命令列表
  static QStringList fileMethods();
};