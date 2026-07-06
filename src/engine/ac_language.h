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

/// @brief 语言关键字常量（代码逻辑引用，避免硬编码字符串）
namespace AcKeyword {
inline constexpr const char *kLet = "let";
inline constexpr const char *kNew = "new";
inline constexpr const char *kMain = "main";
inline constexpr const char *kFor = "for";
inline constexpr const char *kIn = "in";
inline constexpr const char *kIf = "if";
inline constexpr const char *kElse = "else";
inline constexpr const char *kCall = "call";
inline constexpr const char *kReturn = "return";
inline constexpr const char *kThis = "this";
inline constexpr const char *kTrue = "true";
inline constexpr const char *kFalse = "false";
inline constexpr const char *kClass = "class";
inline constexpr const char *kFunction = "function";
} // namespace AcKeyword

/// @brief .ac 语言关键字列表（供高亮、补全使用）
inline const QStringList kAcKeywords = {
    QString::fromLatin1(AcKeyword::kLet),
    QString::fromLatin1(AcKeyword::kNew),
    QString::fromLatin1(AcKeyword::kMain),
    QString::fromLatin1(AcKeyword::kFor),
    QString::fromLatin1(AcKeyword::kIn),
    QString::fromLatin1(AcKeyword::kIf),
    QString::fromLatin1(AcKeyword::kElse),
    QString::fromLatin1(AcKeyword::kCall),
    QString::fromLatin1(AcKeyword::kReturn),
    QString::fromLatin1(AcKeyword::kThis),
    QString::fromLatin1(AcKeyword::kTrue),
    QString::fromLatin1(AcKeyword::kFalse),
    QString::fromLatin1(AcKeyword::kClass),
    QString::fromLatin1(AcKeyword::kFunction),
};

// ═════════════════════════════════════════════════════════════════════════════
// 一、一级函数 — .ac 脚本直接调用，C++ 引擎后端实现
// ═════════════════════════════════════════════════════════════════════════════

namespace AcBuiltin {
/// @brief 调用 C++ 注册函数：call("类名", "方法名", [参数])
inline constexpr const char *kCall = "call";
/// @brief 读取 JSON 文件：readJson("path.json") → 返回 QJsonValue
inline constexpr const char *kReadJson = "readJson";
/// @brief 读取文本文件：readFile("path.txt") → 返回内容
inline constexpr const char *kReadFile = "readFile";
/// @brief 渲染模板文件：renderTpl("template.tpl", data) → 返回渲染文本
inline constexpr const char *kRenderTpl = "renderTpl";
/// @brief 写入文本文件：writeFile("path.txt", content) → 返回 bool
inline constexpr const char *kWriteFile = "writeFile";
/// @brief 打印日志到控制台：print("message") → 无返回值
inline constexpr const char *kPrint = "print";
/// @brief 获取 tree.config 中勾选的文件列表：getCheckedFiles() → 返回字符串数组
inline constexpr const char *kGetCheckedFiles = "getCheckedFiles";
/// @brief 合并两个 JSON 对象：merge(obj1, obj2) → 返回新对象
inline constexpr const char *kMerge = "merge";
/// @brief 获取文件无后缀基名：basename("path/file.cpp") → 返回 "file"
inline constexpr const char *kBasename = "basename";
/// @brief 判断文件是否存在（模板专用）：fileExists("path.txt") → 返回 bool
inline constexpr const char *kFileExists = "fileExists";
} // namespace AcBuiltin

/// @brief 一级函数名列表（供解析器校验、高亮、补全使用）
inline const QStringList kAcBuiltins = {
    QString(AcBuiltin::kCall),
    QString(AcBuiltin::kReadJson),
    QString(AcBuiltin::kReadFile),
    QString(AcBuiltin::kRenderTpl),
    QString(AcBuiltin::kWriteFile),
    QString(AcBuiltin::kPrint),
    QString(AcBuiltin::kGetCheckedFiles),
    QString(AcBuiltin::kMerge),
    QString(AcBuiltin::kBasename),
    QString(AcBuiltin::kFileExists),
};

// ═════════════════════════════════════════════════════════════════════════════
// 二、new 实例化类 — new DB({...})  →  db.query(...)
// ═════════════════════════════════════════════════════════════════════════════

/// @brief DB 数据库类 — 类名与方法名常量
namespace AcDB {
/// @brief 类名：new DB({...})
inline constexpr const char *kClassName = "DB";
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

/// @brief File 文件类 — 类名与方法名常量
namespace AcFile {
/// @brief 类名：new File()
inline constexpr const char *kClassName = "File";
/// @brief 读文件：f.read("path.txt") → 返回内容
inline constexpr const char *kRead = "read";
/// @brief 写文件：f.write("path.txt", content)
inline constexpr const char *kWrite = "write";

/// @brief File 类所有方法名列表
inline const QStringList kMethods = {
    QString::fromLatin1(kRead),
    QString::fromLatin1(kWrite),
};
} // namespace AcFile

/// @brief 可实例化类名列表（供解释器注册、高亮、补全使用）
inline const QStringList kAcClasses = {
    QString::fromLatin1(AcDB::kClassName),
    QString::fromLatin1(AcFile::kClassName),
};

// ═════════════════════════════════════════════════════════════════════════════
// 三、call() 路由类 — call("str", "toLowerCase", ...)
// ═════════════════════════════════════════════════════════════════════════════

/// @brief str 字符串类 — 类名与方法名常量
namespace AcCallStr {
/// @brief 类名：call("str", "toLowerCase", ...)
inline constexpr const char *kClassName = "str";
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

// ═════════════════════════════════════════════════════════════════════════════
// 四、运行时内部协议 — 实例对象标记、构造器入口、伪类名
// ═════════════════════════════════════════════════════════════════════════════

namespace AcRuntime {
/// @brief 实例对象中标记类名的内部键：instance["__class__"] = "DB"
inline constexpr const char *kClassKey = "__class__";
/// @brief 构造方法名：new DB({...}) 自动调用 __construct__
inline constexpr const char *kConstructor = "__construct__";
/// @brief 一级函数注册的伪类名：FunMgr::call("builtin", name, args)
inline constexpr const char *kBuiltinClass = "builtin";
} // namespace AcRuntime

// ═════════════════════════════════════════════════════════════════════════════
// 五、模板语法标记 — ${...} 占位符、块标签
// ═════════════════════════════════════════════════════════════════════════════

namespace AcTpl {
/// @brief 表达式开始标记：${variable}、${each items} 等
inline constexpr const char *kExprOpen = "${";
/// @brief if 块前缀：${if condition}
inline constexpr const char *kIfPrefix = "if ";
/// @brief each 块前缀：${each items}
inline constexpr const char *kEachPrefix = "each ";
/// @brief if 块闭合标签：${/if}
inline constexpr const char *kIfClose = "${/if}";
/// @brief each 块闭合标签：${/each}
inline constexpr const char *kEachClose = "${/each}";
/// @brief else 标签：${else}
inline constexpr const char *kElse = "${else}";
/// @brief 循环当前元素的 context 键：${.} 或 ${this}
inline constexpr const char *kCurrentItem = ".";
} // namespace AcTpl