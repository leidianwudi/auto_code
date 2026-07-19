/**
 * @file ac_language.h
 * @brief .ac 脚本语言 — 关键字、内置函数、类定义集中声明
 *
 * 按调用方式分五大区，看一眼就能找到引擎支持的全部内容：
 * 0. AcKeyword::kAll  语言关键字列表    let  new  for  if  else  return  ...
 * 1. AcBuiltin::kAll  一级函数列表       renderTpl(...)  write(...)  print(...)
 * 2. AcClass::kAll    new 实例化类列表   DB  File
 * 3. AcCall*:          call() 路由类     call("str", "toLowerCase", ...)
 *
 * 新增函数/类/方法/关键字只需改此文件，脚本引擎、语法高亮、自动补全自动同步。
 */

#pragma once

#include <QJsonObject>
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
inline constexpr const char *kStatic = "static";
inline constexpr const char *kPublic = "public";
inline constexpr const char *kProtected = "protected";
inline constexpr const char *kPrivate = "private";
inline constexpr const char *kExtends = "extends";
inline constexpr const char *kOverride = "override";
inline constexpr const char *kInterface = "interface";
inline constexpr const char *kImplements = "implements";
inline constexpr const char *kSuper = "super";
inline constexpr const char *kExport = "export";
inline constexpr const char *kImport = "import";
inline constexpr const char *kFrom = "from";
inline constexpr const char *kNull = "null";
inline constexpr const char *kUndefined = "undefined";
inline constexpr const char *kWhile = "while";
inline constexpr const char *kBreak = "break";
inline constexpr const char *kContinue = "continue";
inline constexpr const char *kSwitch = "switch";
inline constexpr const char *kCase = "case";
inline constexpr const char *kDefault = "default";
inline constexpr const char *kEnum = "enum";
inline constexpr const char *kConstructor = "constructor";
inline constexpr const char *kUsing = "using";
inline constexpr const char *kDispose = "dispose";
inline constexpr const char *kAs = "as";

/// @brief 关键字列表（供高亮、补全使用）
inline const QStringList kAll = {
    QString::fromLatin1(kLet),       QString::fromLatin1(kNew),
    QString::fromLatin1(kMain),      QString::fromLatin1(kFor),
    QString::fromLatin1(kIn),        QString::fromLatin1(kIf),
    QString::fromLatin1(kElse),      QString::fromLatin1(kCall),
    QString::fromLatin1(kReturn),    QString::fromLatin1(kThis),
    QString::fromLatin1(kTrue),      QString::fromLatin1(kFalse),
    QString::fromLatin1(kClass),     QString::fromLatin1(kFunction),
    QString::fromLatin1(kStatic),    QString::fromLatin1(kPublic),
    QString::fromLatin1(kProtected), QString::fromLatin1(kPrivate),
    QString::fromLatin1(kExtends),   QString::fromLatin1(kOverride),
    QString::fromLatin1(kInterface), QString::fromLatin1(kImplements),
    QString::fromLatin1(kSuper),     QString::fromLatin1(kExport),
    QString::fromLatin1(kImport),    QString::fromLatin1(kFrom),
    QString::fromLatin1(kNull),      QString::fromLatin1(kUndefined),
    QString::fromLatin1(kWhile),     QString::fromLatin1(kBreak),
    QString::fromLatin1(kContinue),  QString::fromLatin1(kSwitch),
    QString::fromLatin1(kCase),      QString::fromLatin1(kDefault),
    QString::fromLatin1(kEnum),      QString::fromLatin1(kConstructor),
    QString::fromLatin1(kUsing),     QString::fromLatin1(kDispose),
    QString::fromLatin1(kAs),
};
}  // namespace AcKeyword

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
/// @brief 打印日志到控制台：printLog("message") → 无返回值
inline constexpr const char *kPrintLog = "printLog";
/// @brief 打印错误到控制台：printError("message") → 无返回值（log 面板显示为红色）
inline constexpr const char *kPrintError = "printError";
/// @brief 获取 tree.config 中勾选的文件列表：getCheckedFiles() → 返回字符串数组
inline constexpr const char *kGetCheckedFiles = "getCheckedFiles";
/// @brief 合并两个 JSON 对象：merge(obj1, obj2) → 返回新对象
inline constexpr const char *kMerge = "merge";
/// @brief 获取文件无后缀基名：basename("path/file.cpp") → 返回 "file"
inline constexpr const char *kBasename = "basename";
/// @brief 判断文件是否存在（模板专用）：fileExists("path.txt") → 返回 bool
inline constexpr const char *kFileExists = "fileExists";

/// @brief 一级函数名列表（供解析器校验、高亮、补全使用）
inline const QStringList kAll = {
    QString(kCall),      QString(kReadJson), QString(kReadFile),   QString(kRenderTpl),
    QString(kWriteFile), QString(kPrintLog), QString(kPrintError), QString(kGetCheckedFiles),
    QString(kMerge),     QString(kBasename), QString(kFileExists),
};
}  // namespace AcBuiltin

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

/// @brief DB 类内部 JSON 键名常量（DbConfig 依赖这些常量，必须在前）
inline constexpr const char *kConnId = "connId";
inline constexpr const char *kHost = "host";
inline constexpr const char *kPort = "port";
inline constexpr const char *kUser = "user";
inline constexpr const char *kPassword = "password";
inline constexpr const char *kDatabase = "database";
inline constexpr const char *kConnected = "connected";
inline constexpr const char *kTable = "table";
inline constexpr const char *kSql = "sql";
/// @brief tableSchema 返回的列信息 JSON 键名
inline constexpr const char *kColName = "name";
inline constexpr const char *kColType = "type";
inline constexpr const char *kColNullable = "nullable";
inline constexpr const char *kColKey = "key";
inline constexpr const char *kColDefault = "default";
inline constexpr const char *kColExtra = "extra";
inline constexpr const char *kColComment = "comment";

/// @brief 数据库连接配置 — 对应 new DB({host, port, user, password, database})
struct DbConfig {
  QString host;
  int port = 3306;
  QString user;
  QString password;
  QString database;

  /// @brief 从 JSON 对象解析配置
  static DbConfig fromJson(const QJsonObject &obj) {
    DbConfig cfg;
    cfg.host = obj.value(QString::fromLatin1(kHost)).toString();
    cfg.port = obj.value(QString::fromLatin1(kPort)).toInt(3306);
    cfg.user = obj.value(QString::fromLatin1(kUser)).toString();
    cfg.password = obj.value(QString::fromLatin1(kPassword)).toString();
    cfg.database = obj.value(QString::fromLatin1(kDatabase)).toString();
    return cfg;
  }

  /// @brief 配置是否有效（必需的 host、user、database 均已填写）
  bool isValid() const { return !host.isEmpty() && !user.isEmpty() && !database.isEmpty(); }
};

/// @brief DB 类所有方法名列表
inline const QStringList kMethods = {
    QString::fromLatin1(kTableSchema),
    QString::fromLatin1(kQuery),
    QString::fromLatin1(kDisconnect),
    QString::fromLatin1(AcKeyword::kDispose),
};
}  // namespace AcDB

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
}  // namespace AcFile

/// @brief 可实例化类名列表（供解释器注册、高亮、补全使用）
namespace AcClass {
inline const QStringList kAll = {
    QString::fromLatin1(AcDB::kClassName),
    QString::fromLatin1(AcFile::kClassName),
};
}  // namespace AcClass

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
}  // namespace AcCallStr

// ═════════════════════════════════════════════════════════════════════════════
// 四、运行时内部协议 — 实例对象标记、构造器入口、伪类名
// ═════════════════════════════════════════════════════════════════════════════

namespace AcRuntime {
/// @brief 实例对象中标记类名的内部键：instance["__class__"] = "DB"
inline constexpr const char *kClassKey = "__class__";
/// @brief 构造方法名：new DB({...}) 自动调用 __construct__
inline constexpr const char *kConstructor = "__construct__";
/// @brief 析构方法名：引用计数归零时自动调用 __destruct__
inline constexpr const char *kDestructor = "__destruct__";
/// @brief 实例对象中标记唯一ID的内部键：instance["__objId__"] = "uuid"
inline constexpr const char *kObjId = "__objId__";
/// @brief 一级函数注册的伪类名：FunMgr::call("builtin", name, args)
inline constexpr const char *kBuiltinClass = "builtin";
}  // namespace AcRuntime

// ═════════════════════════════════════════════════════════════════════════════
// 五、模板语法标记 — ${...} 占位符、块标签
// ═════════════════════════════════════════════════════════════════════════════

namespace AcTemplate {
/// @brief 表达式开始标记：${variable}、${each items} 等
inline constexpr const char *kExprOpen = "${";
/// @brief if 块前缀：${if condition}
inline constexpr const char *kIfPrefix = "if ";
/// @brief each 块前缀：${each items} 或 ${each item in items}
inline constexpr const char *kEachPrefix = "each ";
/// @brief each 块中 "in" 分隔关键字：each item **in** items
inline constexpr const char *kEachIn = " in ";
/// @brief if 块闭合标签：${/if}
inline constexpr const char *kIfClose = "${/if}";
/// @brief each 块闭合标签：${/each}
inline constexpr const char *kEachClose = "${/each}";
/// @brief else 标签：${else}
inline constexpr const char *kElse = "${else}";
/// @brief 注释标签：${# 注释内容}，渲染时整段跳过不输出
inline constexpr const char *kCommentPrefix = "#";
/// @brief 循环当前元素的 context 键：${.} 或 ${this}
inline constexpr const char *kCurrentItem = ".";
}  // namespace AcTemplate

// ═════════════════════════════════════════════════════════════════════════════
// 六、文件后缀常量 — 统一管理，避免硬编码字符串
// ═════════════════════════════════════════════════════════════════════════════

namespace AcFileSuffix {
inline constexpr const char *kAc = ".ac";
inline constexpr const char *kTpl = ".tpl";
inline constexpr const char *kTpljson = ".tpljson";
inline constexpr const char *kJson = ".json";
}  // namespace AcFileSuffix