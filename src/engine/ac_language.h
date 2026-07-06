/**
 * @file ac_language.h
 * @brief .ac 脚本语言 — 关键字、内置函数、类定义集中声明
 *
 * 按调用方式分四大区，看一眼就能找到引擎支持的全部内容：
 * 0. kAcKeywords:   语言关键字       let  new  for  if  else  return  ...
 * 1. AcBuiltin:     一级函数          renderTpl(...)  write(...)  print(...)
 * 2. AcClass*:      new 实例化类     new DB({...})  → db.query(...)
 * 3. AcCall*:       call() 路由类    call("str", "toLowerCase", ...)
 *
 * 新增函数/类/方法/关键字只需改此文件，脚本引擎、语法高亮、自动补全自动同步。
 */

#pragma once

#include <QString>
#include <QStringList>

// ═════════════════════════════════════════════════════════════════════════════
// 零、语言关键字 — 语法高亮、补全共用
// ═════════════════════════════════════════════════════════════════════════════

/// @brief new 关键字（补全时判断上下文）
inline constexpr const char *kAcKeywordNew = "new";

/// @brief .ac 语言关键字列表（供高亮、补全使用）
inline const QStringList kAcKeywords = {
    QStringLiteral("let"),  QStringLiteral("new"),   QStringLiteral("main"),
    QStringLiteral("for"),  QStringLiteral("in"),    QStringLiteral("if"),
    QStringLiteral("else"), QStringLiteral("call"),  QStringLiteral("return"),
    QStringLiteral("true"), QStringLiteral("false"),
};

// ═════════════════════════════════════════════════════════════════════════════
// 一、一级函数 — .ac 脚本直接调用，C++ 引擎后端实现
// ═════════════════════════════════════════════════════════════════════════════

namespace AcBuiltin {
/// @brief 调用 C++ 注册函数：call("类名", "方法名", [参数])
inline constexpr const char *kCall = "call";
/// @brief 读取 JSON 文件：readJson("path.json") → 返回 QJsonValue
inline constexpr const char *kReadJson = "readJson";
/// @brief 渲染模板文件：renderTpl("template.tpl", data) → 返回渲染文本
inline constexpr const char *kRenderTpl = "renderTpl";
/// @brief 写入文本文件：write("path.txt", content) → 返回 bool
inline constexpr const char *kWrite = "write";
/// @brief 打印日志到控制台：print("message") → 无返回值
inline constexpr const char *kPrint = "print";
/// @brief 获取 tree.config 中勾选的文件列表：getCheckedFiles() → 返回字符串数组
inline constexpr const char *kGetCheckedFiles = "getCheckedFiles";
/// @brief 合并两个 JSON 对象：merge(obj1, obj2) → 返回新对象
inline constexpr const char *kMerge = "merge";
/// @brief 获取文件无后缀基名：basename("path/file.cpp") → 返回 "file"
inline constexpr const char *kBasename = "basename";
} // namespace AcBuiltin

/// @brief 一级函数名列表（供解析器校验、高亮、补全使用）
inline const QStringList kAcBuiltins = {
    QString(AcBuiltin::kCall),      QString(AcBuiltin::kReadJson),
    QString(AcBuiltin::kRenderTpl), QString(AcBuiltin::kWrite),
    QString(AcBuiltin::kPrint),     QString(AcBuiltin::kGetCheckedFiles),
    QString(AcBuiltin::kMerge),     QString(AcBuiltin::kBasename),
};

// ═════════════════════════════════════════════════════════════════════════════
// 二、new 实例化类 — new DB({...})  →  db.query(...)
// ═════════════════════════════════════════════════════════════════════════════

/// @brief DB 类名常量
inline constexpr const char *kAcClassDB = "DB";

/// @brief DB 数据库类 — 方法名常量
namespace AcDB {
/// @brief 获取表结构：db.tableSchema({table: "news"})
inline constexpr const char *kTableSchema = "tableSchema";
/// @brief 执行 SQL：db.query({sql: "SELECT * FROM t"})
inline constexpr const char *kQuery = "query";
/// @brief 断开连接：db.disconnect()
inline constexpr const char *kDisconnect = "disconnect";

/// @brief DB 类所有方法名列表
inline const QStringList kMethods = {
    QString::fromLatin1(kTableSchema),
    QString::fromLatin1(kQuery),
    QString::fromLatin1(kDisconnect),
};
} // namespace AcDB

/// @brief 可实例化类名列表（供解释器注册、高亮、补全使用）
inline const QStringList kAcClasses = {
    QString::fromLatin1(kAcClassDB),
};

// ═════════════════════════════════════════════════════════════════════════════
// 三、call() 路由类 — call("str", "toLowerCase", ...)
// ═════════════════════════════════════════════════════════════════════════════

/// @brief str 类名常量
inline constexpr const char *kAcCallStr = "str";

/// @brief str 字符串类 — 方法名常量
namespace AcCallStr {
/// @brief 转小写：call("str", "toLowerCase", "ABC") → "abc"
inline constexpr const char *kToLowerCase = "toLowerCase";
/// @brief 转大写：call("str", "toUpperCase", "abc") → "ABC"
inline constexpr const char *kToUpperCase = "toUpperCase";
/// @brief 去首尾空白：call("str", "trim", "  hi  ") → "hi"
inline constexpr const char *kTrim = "trim";
/// @brief 首字母大写：call("str", "capitalize", "hello") → "Hello"
inline constexpr const char *kCapitalize = "capitalize";
/// @brief 子串：call("str", "substring", "hello", 1, 3) → "el"
inline constexpr const char *kSubstring = "substring";
/// @brief 替换：call("str", "replace", "abc", "b", "x") → "axc"
inline constexpr const char *kReplace = "replace";

/// @brief str 类所有方法名列表
inline const QStringList kMethods = {
    QString::fromLatin1(kToLowerCase), QString::fromLatin1(kToUpperCase),
    QString::fromLatin1(kTrim),        QString::fromLatin1(kCapitalize),
    QString::fromLatin1(kSubstring),   QString::fromLatin1(kReplace),
};
} // namespace AcCallStr

/// @brief file 类名常量
inline constexpr const char *kAcCallFile = "file";

/// @brief file 文件类 — 方法名常量
namespace AcCallFile {
/// @brief 读文件：call("file", "read", "path.txt") → 返回内容
inline constexpr const char *kRead = "read";
/// @brief 写文件：call("file", "write", "path.txt", content)
inline constexpr const char *kWrite = "write";

/// @brief file 类所有方法名列表
inline const QStringList kMethods = {
    QString::fromLatin1(kRead),
    QString::fromLatin1(kWrite),
};
} // namespace AcCallFile