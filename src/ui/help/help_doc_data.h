/**
 * @file help_doc_data.h
 * @brief 帮助文档数据常量 — AC 语言全部类型、系统函数、语法的分类与示例代码
 *
 * 帮助文档界面左侧单选按钮的分类标题与右侧展示的 AC 示例代码统一在此定义。
 * 新增语法/函数/类型时只需修改此文件，界面自动同步。
 */

#pragma once

#include <QVector>

// ════════════════════════════════════════════════════════════
//  帮助文档分类数据
// ════════════════════════════════════════════════════════════

/// @brief 一条帮助文档分类（按钮标题 + AC 示例代码）
struct HelpDocEntry {
  const char *title;  ///< 左侧单选按钮标题
  const char *code;   ///< 右侧展示的 AC 示例代码
};

/// @brief 帮助文档分类列表（顺序即左侧按钮顺序）
namespace HelpDocData {
inline const QVector<HelpDocEntry> kEntries = {
    {
        // ── 1. 程序入口与注释 ──
        "程序入口与注释",
        R"AC(
// ============================================================
// 1. 程序入口 main 块
// ============================================================
// 每个 AC 脚本通过 main { } 定义执行入口，
// 顶层 main 会在脚本加载后自动执行。
// （main 是可选项：只有类/函数定义的库文件可以没有 main）

main {
  printLog("Hello, Auto Code!");
}

// ------------------------------------------------------------
// 2. 注释
// ------------------------------------------------------------
// 单行注释：以 // 开头
/* 块注释：以 /* 开头、*/ 结尾，可跨多行 */

// 内联注释（放在代码行末尾）
let x: Number = 1;  /* 内联块注释 */
)AC",
    },
    {
        // ── 2. 变量与数据类型 ──
        "变量与数据类型",
        R"AC(
// ============================================================
// 变量声明：let 名称: 类型 = 值;
// ============================================================
main {
  let count: Number = 10;     // 数字（浮点/整数通吃）
  let name: String = "AC";    // 字符串（双引号）
  let flag: Bool = true;      // 布尔（true/false）
  let anyVal: Any = null;     // 任意类型（可存放任意值）

  // 内置类型一览：
  //   Number  数字     String  字符串
  //   Bool    布尔     Any     任意
  //   Array   数组     Object  对象/映射
  //   Void    无返回值（仅用于函数返回类型）

  // 数组：元素类型 + []
  let arr: Number[] = [1, 2, 3];
  // 对象：键值对集合，键可用 . 或 ["..."] 访问
  let user: Object = { name: "Tom", age: 20 };
  printLog(user.name);
  printLog(user["age"]);
}
)AC",
    },
    {
        // ── 3. 运算符与表达式 ──
        "运算符与表达式",
        R"AC(
// ============================================================
// 算术运算符
// ============================================================
main {
  let a: Number = 10;
  let b: Number = 3;

  printLog(a + b);   // 加法 → 13
  printLog(a - b);   // 减法 → 7
  printLog(a * b);   // 乘法 → 30
  printLog(a / b);   // 除法 → 3.333...
  printLog(a % b);   // 取模 → 1

  // ------------------------------------------------------------
  // 比较运算符（结果都是 Bool）
  let eq: Bool = (a == b);    // 相等
  let ne: Bool = (a != b);    // 不等
  let gt: Bool = (a > b);     // 大于
  let ge: Bool = (a >= b);    // 大于等于
  let lt: Bool = (a < b);     // 小于
  let le: Bool = (a <= b);    // 小于等于

  // ------------------------------------------------------------
  // 逻辑运算符（与或非）
  let and: Bool = (true && false);  // 与
  let or: Bool = (true || false);   // 或
  let not: Bool = !true;            // 非
}
)AC",
    },
    {
        // ── 4. 控制流 ──
        "控制流（if/for/while）",
        R"AC(
// ============================================================
// if / else if / else 条件分支
// ============================================================
main {
  let score: Number = 85;
  if (score >= 90) {
    printLog("优秀");
  } else if (score >= 60) {
    printLog("及格");
  } else {
    printLog("不及格");
  }

  // ------------------------------------------------------------
  // for 循环（传统三段式）
  for (let i: Number = 0; i < 5; i = i + 1) {
    printLog(i);
  }

  // ------------------------------------------------------------
  // for...in 遍历数组
  let items: Number[] = [10, 20, 30];
  for (let item: Number in items) {
    printLog(item);
  }

  // ------------------------------------------------------------
  // while 循环
  let n: Number = 0;
  while (n < 3) {
    printLog(n);
    n = n + 1;
  }

  // ------------------------------------------------------------
  // break / continue
  for (let i: Number = 0; i < 10; i = i + 1) {
    if (i == 2) continue;   // 跳过本次
    if (i == 5) break;      // 终止循环
    printLog(i);
  }
}
)AC",
    },
    {
        // ── 5. switch 分支 ──
        "switch 分支",
        R"AC(
// ============================================================
// switch / case / default 多分支
// ============================================================
main {
  let code: Number = 200;
  let msg: String = "";

  switch (code) {
    case 200:
      msg = "OK";
      break;              // 每个 case 需显式 break
    case 404:
      msg = "Not Found";
      break;
    case 500:
      msg = "Server Error";
      break;
    default:
      msg = "Unknown";
  }
  printLog(msg);
}
)AC",
    },
    {
        // ── 6. 函数 ──
        "函数定义与调用",
        R"AC(
// ============================================================
// 函数定义：function 名称(参数: 类型): 返回类型 { }
// 返回类型必须有明确标注；无返回值用 Void
// ============================================================
function add(a: Number, b: Number): Number {
  return a + b;
}

// 无返回值函数
function log(msg: String): Void {
  printLog(msg);
}

// 可选参数：参数可省略，内部用 null 判断
function greet(name: String, greeting: String): String {
  if (greeting == null) {
    greeting = "Hello";
  }
  return `${greeting}, ${name}!`;
}

main {
  let sum: Number = add(1, 2);
  log("sum = " + sum);
  printLog(greet("AC"));       // Hello, AC!
  printLog(greet("AC", "Hi")); // Hi, AC!
}
)AC",
    },
    {
        // ── 7. 字符串模板 ──
        "字符串模板（反引号）",
        R"AC(
// ============================================================
// 字符串模板：用反引号 ` 包裹，${表达式} 插入变量
// ============================================================
main {
  let name: String = "Tom";
  let age: Number = 20;

  let msg: String = `我叫 ${name}，今年 ${age} 岁`;
  printLog(msg);   // 我叫 Tom，今年 20 岁

  // 模板中可插入任意表达式
  let calc: String = `3 + 4 = ${3 + 4}`;
  printLog(calc);  // 3 + 4 = 7
}
)AC",
    },
    {
        // ── 8. 类与继承 ──
        "类与继承",
        R"AC(
// ============================================================
// 类定义：class 名称 { }
// 属性需用 let 声明，访问用 this.xxx
// ============================================================
class Animal {
  protected let name: String;

  // 构造器
  public constructor(name: String) {
    this.name = name;
  }

  // 方法
  public function speak(): String {
    return this.name + " 发出叫声";
  }
}

// 继承：extends，子类通过 super() 调用父类构造器
class Dog extends Animal {
  public constructor(name: String) {
    super(name);
  }

  // 方法重写：override 关键字
  public override function speak(): String {
    return this.name + " 汪汪叫";
  }
}

main {
  let dog: Dog = new Dog("旺财");
  printLog(dog.speak());   // 旺财 汪汪叫
}
)AC",
    },
    {
        // ── 9. 接口与实现 ──
        "接口与实现",
        R"AC(
// ============================================================
// 接口：interface 名称 { 方法签名 }
// 类通过 implements 实现接口，必须实现所有方法
// ============================================================
interface Printable {
  function toString(): String;
}

interface Comparable {
  function compareTo(other: Any): Number;
}

class Item implements Printable, Comparable {
  private let value: Number;

  public constructor(value: Number) {
    this.value = value;
  }

  public function toString(): String {
    return "Item(" + this.value + ")";
  }

  public function compareTo(other: Any): Number {
    if (this.value > other.value) return 1;
    if (this.value < other.value) return -1;
    return 0;
  }
}

main {
  let a: Item = new Item(10);
  let b: Item = new Item(20);
  printLog(a.toString());
  printLog(a.compareTo(b));   // -1
}
)AC",
    },
    {
        // ── 10. 枚举 ──
        "枚举（enum）",
        R"AC(
// ============================================================
// 枚举：enum 名称 { 成员, ... }
// 数字枚举自动递增，也可显式赋值
// ============================================================
enum UserRole {
  Admin,
  Editor,
  Viewer
}

// 显式赋值的数字枚举
enum HttpStatus {
  OK = 200,
  NotFound = 404
}

main {
  let role: UserRole = UserRole.Editor;
  printLog(role);                     // Editor

  let status: HttpStatus = HttpStatus.OK;
  printLog(status);                   // 200
}
)AC",
    },
    {
        // ── 11. 数组方法 ──
        "数组与常用方法",
        R"AC(
// ============================================================
// 数组常用方法
// ============================================================
main {
  let arr: Number[] = [3, 1, 4, 1, 5];

  arr.push(9);            // 末尾追加
  printLog(arr.length);   // 长度 → 6

  let first: Number = arr[0];   // 下标访问 → 3
  arr[0] = 100;                 // 按下标赋值

  // indexOf 查找索引
  let idx: Number = arr.indexOf(5);
  printLog(idx);          // 4

  // splice 删除（索引, 个数）
  arr.splice(0, 1);       // 删除第 0 个

  // includes 是否包含
  let has: Bool = arr.includes(9);
  printLog(has);          // true

  // join 连接成字符串
  let joined: String = arr.join(",");
  printLog(joined);       // 1,4,1,5,9
}
)AC",
    },
    {
        // ── 12. 字符串方法 ──
        "字符串常用方法",
        R"AC(
// ============================================================
// 字符串常用方法
// ============================================================
main {
  let s: String = "Hello, AutoCode";

  printLog(s.length);               // 长度 → 16
  printLog(s.toUpperCase());        // 转大写
  printLog(s.toLowerCase());        // 转小写

  // substring(起始, 结束) 截取子串（不含结束）
  printLog(s.substring(0, 5));      // Hello
  printLog(s.substring(7));         // AutoCode

  // indexOf / lastIndexOf 查找
  printLog(s.indexOf("o"));         // 4
  printLog(s.lastIndexOf("o"));     // 11

  // replace 替换
  printLog(s.replace("AutoCode", "AC"));  // Hello, AC

  // 下标访问单个字符
  printLog(s[0]);                   // H

  // 字符串拼接
  let concat: String = "A" + "B" + "C";
  printLog(concat);                 // ABC
}
)AC",
    },
    {
        // ── 13. JSON 操作 ──
        "JSON 读写",
        R"AC(
// ============================================================
// JSON 序列化 / 反序列化
// ============================================================
main {
  // 构造对象
  let data: Object = {
    name: "AutoCode",
    version: 1.0,
    features: ["syntax", "debug"],
    meta: { author: "AC Team" }
  };

  // JSON.stringify 序列化为字符串（支持缩进格式化）
  let json: String = JSON.stringify(data, null, 2);
  printLog(json);

  // JSON.parse 反序列化
  let parsed: Object = JSON.parse(json);
  printLog(parsed["name"]);                 // AutoCode
  printLog(parsed["features"][0]);          // syntax
  printLog(parsed["meta"]["author"]);       // AC Team
}
)AC",
    },
    {
        // ── 14. 内置函数（一） ──
        "内置函数（文件与日志）",
        R"AC(
// ============================================================
// 内置一级函数：文件操作与日志
// ============================================================
main {
  // printLog 打印日志
  printLog("普通日志");

  // printError 打印错误（日志面板红色显示）
  printError("错误信息");

  // readJson 读取 JSON 文件 → 返回解析后的值
  let cfg: Object = readJson("config.json");
  printLog(cfg["appName"]);

  // readFile 读取文本文件 → 返回字符串
  let content: String = readFile("readme.txt");
  printLog(content);

  // writeFile 写入文本文件 → 返回是否成功
  let ok: Bool = writeFile("out.txt", "写入内容");
  printLog(ok);

  // scriptDir 当前脚本所在目录
  let dir: String = scriptDir();
  printLog(dir);

  // fileExists 文件是否存在
  let exists: Bool = fileExists("config.json");
  printLog(exists);
}
)AC",
    },
    {
        // ── 15. 内置函数（二） ──
        "内置函数（路径与工具）",
        R"AC(
// ============================================================
// 内置一级函数：路径处理与工具
// ============================================================
main {
  // basename 获取无后缀文件名
  let base: String = basename("D:/out/user.ts");   // user
  printLog(base);

  // fileName 获取完整文件名（含扩展名）
  let fname: String = fileName("D:/out/user.ts");   // user.ts
  printLog(fname);

  // formatPath 按模板格式化输出路径
  // 占位符 {key} 从第二个参数的对象中取值
  let path: String = formatPath("{base}/{name}.ts",
                                { base: "D:/out", name: "user" });
  printLog(path);    // D:/out/user.ts

  // merge 合并两个对象（后者覆盖前者同名键）
  let a: Object = { x: 1, y: 2 };
  let b: Object = { y: 3, z: 4 };
  let m: Object = merge(a, b);   // { x: 1, y: 3, z: 4 }
  printLog(m["y"]);              // 3

  // getCheckedFiles 获取勾选的文件列表
  let files: String[] = getCheckedFiles();
  printLog(files.length);

  // assert 断言：条件为 false 时输出错误
  assert(1 + 1 == 2, "算术断言失败");
}
)AC",
    },
    {
        // ── 16. 内置函数（三） ──
        "内置函数（模板渲染）",
        R"AC(
// ============================================================
// renderTpl 渲染 .tpl 模板文件
// ============================================================
main {
  // 准备模板数据
  let data: Object = {
    title: "用户列表",
    users: [
      { name: "Tom", age: 20 },
      { name: "Jerry", age: 22 }
    ]
  };

  // 渲染模板（模板路径相对于当前脚本目录）
  let html: String = renderTpl("table.tpl", data);
  printLog(html);
}

// ── 对应 table.tpl 模板内容 ──
// ${# 这是模板注释，渲染时跳过}
// <h1>${title}</h1>
// ${each users}
//   <p>${name} - ${age}</p>
// ${/each}
// ${if users.length > 0}
//   <p>共 ${users.length} 条</p>
// ${else}
//   <p>暂无数据</p>
// ${/if}
)AC",
    },
    {
        // ── 17. 实例化类 ──
        "实例化类（new）",
        R"AC(
// ============================================================
// new 实例化系统类：DB / File
// ============================================================
main {
  // ── File 文件类 ──
  let f: File = new File();
  let content: String = f.read("data.txt");
  f.write("backup.txt", content);

  // ── DB 数据库类 ──
  let db: DB = new DB({
    host: "localhost",
    port: 3306,
    user: "root",
    password: "123456",
    database: "test_db"
  });

  // 查询表结构
  let schema: Object = db.tableSchema({ table: "news" });
  printLog(schema);

  // 获取表信息
  let info: Object = db.tableInfo({ table: "news" });
  printLog(info);

  // 执行 SQL
  let result: Object = db.query({ sql: "SELECT * FROM news" });
  printLog(result);

  // 断开连接
  db.disconnect();
}
)AC",
    },
    {
        // ── 18. call() 路由 ──
        "call() 系统函数调用",
        R"AC(
// ============================================================
// call("类名", "方法名", 参数...)
// 调用 str 字符串系统函数
// ============================================================
main {
  // 字符串类：call("str", ...)
  printLog(call("str", "toLowerCase", "ABC"));   // abc
  printLog(call("str", "toUpperCase", "abc"));   // ABC
  printLog(call("str", "trim", "  hi  "));       // hi
  printLog(call("str", "capitalize", "hello"));  // Hello

  // substring(字符串, 起始, 长度)
  printLog(call("str", "substring", "hello", 1, 3));  // ell

  // replace(字符串, 查找, 替换)
  printLog(call("str", "replace", "abc", "b", "x"));  // axc
}
)AC",
    },
    {
        // ── 19. 模块化 ──
        "模块化（import/export）",
        R"AC(
// ============================================================
// export 导出：类、函数、变量
// ============================================================
export function add(a: Number, b: Number): Number {
  return a + b;
}

export class MathUtil {
  public static function square(n: Number): Number {
    return n * n;
  }
}

// ============================================================
// import 导入（在其他 .ac 文件中）
// ============================================================
import { add } from "./lib/math.ac";
import { MathUtil } from "./lib/math.ac";

main {
  printLog(add(3, 4));               // 7
  printLog(MathUtil.square(5));      // 25
}
)AC",
    },
    {
        // ── 20. 静态成员与访问控制 ──
        "静态成员与访问控制",
        R"AC(
// ============================================================
// 访问控制：public / protected / private
// 静态成员：static（类名直接访问）
// ============================================================
class Config {
  private static let instance: Config;   // 静态私有成员
  public static let appName: String = "AutoCode";

  private let settings: Object;

  private constructor() {
    this.settings = {};
  }

  // 静态方法：通过类名调用
  public static function getInstance(): Config {
    if (Config.instance == null) {
      Config.instance = new Config();
    }
    return Config.instance;
  }

  public function set(key: String, value: Any): Void {
    this.settings[key] = value;
  }

  public function get(key: String): Any {
    return this.settings[key];
  }
}

main {
  printLog(Config.appName);              // 静态属性：AutoCode
  let cfg: Config = Config.getInstance();
  cfg.set("theme", "dark");
  printLog(cfg.get("theme"));            // dark
}
)AC",
    },
    {
        // ── 21. 类型系统 ──
        "类型系统",
        R"AC(
// ============================================================
// 内置类型：严格区分大小写
//   String  Number  Bool  Boolean  Int  Float  Double
//   Any     Array   Object        Void
// （小写 string/number 会被标记为错误并提示正确拼写）
// ============================================================
main {
  let s: String = "文本";
  let n: Number = 3.14;
  let i: Int = 10;
  let f: Float = 1.5;
  let d: Double = 2.718;
  let b: Bool = true;
  let any: Any = "可以是任意类型";

  // 数组：元素类型 + []
  let intArr: Int[] = [1, 2, 3];
  let strArr: String[] = ["a", "b"];

  // 对象
  let obj: Object = { key: "value" };

  // 数组与对象访问
  printLog(intArr[0]);
  printLog(obj["key"]);
}

// ------------------------------------------------------------
// 函数返回类型必须有明确标注，无返回值用 Void
// ------------------------------------------------------------
function noReturn(): Void {
  printLog("无返回值");
}
)AC",
    },
};
}  // namespace HelpDocData
