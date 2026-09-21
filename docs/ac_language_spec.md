# AC 语言规范（简版）

> 状态：与当前实现（`src/engine/script/`）对齐的快照。
> 用途：固定语言语义决策，防止实现漂移；新增语言能力时先改本文件再改代码。
> 语法以 `ac_parser*.cpp` 为准，运行时语义以 `ac_interpreter*.cpp` 与 `ac_vm.cpp`
> 双实现为准（两模式必须语义一致，见 architecture.md §4.6 收敛计划）。

## 1. 概述与定位

AC 是 AutoCode 低代码编译器内置的脚本语言，用于代码生成模板、测试脚本与自动化任务。
定位为**脚本语言**：支持顶层语句（顶层即隐式 main 函数体），由解释器逐语句执行。

## 2. 词法约定

- 注释：`//` 行注释、`/* */` 块注释
- 字符串：双引号 `"..."`、单引号 `'...'`；模板字符串（反引号）支持 `${expr}` 插值
- 标识符：字母/`_`/`$` 开头；`from` 可作参数名（表达式位置不支持）
- 文件后缀：`.ac`

## 3. 类型系统

- 基本标注：`Number`、`String`、`Bool`/`Boolean`、`Int`、`Float`、`Double`、`Any`、`Object`、`Void`
- 数组：`T[]`（如 `String[]`、`Object[]`、`Any[]`）
- 运行时值统一为一类 JSON 值（null/bool/number/string/array/object），类型标注用于静态检查与文档
- 枚举：数字枚举（自动递增或显式赋值）；成员经 `EnumName.Member` 访问
- 接口：仅声明（方法签名），不做运行时检查；`class X implements I` 记录实现关系

## 4. 变量、作用域与语句

- 声明：`let name: Type = expr;`；未初始化的**顶层/局部变量声明必须带初始化表达式**
  （类属性声明可以不带：`public let tag: String;` → 实例上为 null）
- **顶层即隐式 main 函数体**：`for`/`if`/`while`/`switch`/独立块 `{...}`/表达式语句均可在顶层使用
- 独立入口块：`main { ... }`（可选；与顶层语句按出现顺序混排执行）
- 控制流：`for (init; cond; update)`、`for (let x: T in target)`、`while`、`if/else`、
  `switch/case/default`（case 内 `break` 必须显式）、`break`/`continue`
- `for-in` 目标语义：
  - 对象 → 迭代**键**（插入序，见 §7.3）
  - 数组 → 迭代元素
  - 字符串 → 迭代单字符
- `using varName = expr;`：声明并记录资源变量
- 模块：`import { A, B as C } from "path";`、`export function/class/interface/enum/let ...`

## 5. 函数

```ac
function add(a: Number, b: Number): Number { return a + b; }
```

- 可选参数**两种语法**（等价，二者取一）：
  - `param?: Type` —— 缺省传 null
  - `param: Type = 字面量` —— 字面量限 Number/String/Bool/null（可为负数）
- lambda：`(x: T): R => expr` 形式，经 `kFuncExpr` 注册为匿名函数
- 顶层函数与类方法都经统一调用约定执行；递归深度上限 512 层（超出报"调用栈溢出"）
- 顶层 `return` 仅在函数体内有效

## 6. 类与对象

```ac
class Admin extends User implements Comparable {
  private let permissions: String[];
  public constructor(id: Any, name: String, email: String) {
    super(id, name, email);
    this.permissions = ["read"];
  }
  public override function toString(): String { return `Admin(${this.name})`; }
}
```

- 访问修饰符：`private`/`protected`/`public`（记录语义，解释器不强制）
- 构造器：`constructor`；继承经 `super(...)` 调用基类构造器；实例属性先按声明序初始化，再执行构造器
- **静态成员**：`static let`/`static function`，经 `ClassName.member` 读写；
  **静态属性不嵌入实例对象**（实例序列化不含静态键）
- 单例惯用法（语言级保证）：

```ac
class ConfigManager {
  private static let instance: ConfigManager;
  private constructor() { this.settings = {}; }
  public static function getInstance(): ConfigManager {
    if (ConfigManager.instance == null) {   // 未赋值静态 == null 成立
      ConfigManager.instance = new ConfigManager();
    }
    return ConfigManager.instance;
  }
}
```

- 原生类（C++ 注册，如 `File`/`DB`/`Logger`）：`new File()`、实例方法经 FunMgr 分发

## 7. 值语义（核心决策，勿漂移）

### 7.1 null 唯一空值
- 运行时**没有 undefined**：缺失对象键、未赋值类属性、缺省可选参数统一为 `null`
- `null == null` 成立；宽松相等 `==` 中 null 与任何非空值不等
- 与迁移前 QJson 行为的差异：QJson 的 Undefined 语义已合并为 null（`== null` 判断保持成立）

### 7.2 对象键保序
- 对象字面量、`JSON.parse` 结果、类属性声明序均**按插入序保留**（自有有序容器 accore 实现）
- `for-in`、`JSON.stringify` 按插入序产出（QJson 的字母序缺陷已在编译器核心层消除）
- 顺序敏感数据优先用数组表达；`JSON.parse` 按原文键序

### 7.3 错误传播（对齐 JS TypeError）
运行时非法操作**必须报错并中断脚本**，禁止静默 no-op（消息含行号，headless 模式以非零退出码结束）：

| 操作 | 错误消息 |
|---|---|
| 对 null/标量做索引赋值 `o[k] = v` | `cannot index-assign on value` |
| 对 null/标量做属性赋值 `o.p = v` | `cannot set property 'p' on value` |
| 对 null/标量做索引读取 `o[k]` | `cannot access index on value` |
| 对 null 调用方法 `s.trim()` | `cannot call method ... on null value ...` |
| 除零 / 取模零 | `division by zero` / `modulo by zero` |

- 表达式求值一旦出错立即向上传播；后续求值不再执行
- 未赋值/不存在的静态成员读取返回 null（`X.member == null` 成立），**不算错误**
- 裸类名是合法表达式，求值为类引用对象（JS 语义：类名即类对象）

### 7.4 深拷贝与环防护
- 容器插入（对象 set / 数组 append）对值做深拷贝，杜绝引用环（有意的正确性取舍，
  大对象高频插入有性能成本）

## 8. 内置能力

- 输出：`printLog(...)`（普通）、`printError(...)`（红色）
- JSON：`JSON.parse(text)`（保序，支持 JSON5 超集：注释/单引号/无引号键/尾逗号）、
  `JSON.stringify(value)`（保序紧凑输出）
- 反射式调用：`call("ClassName", "methodName", args...)`
- 原生类：`File`（write/read）、`DB`、`Logger` 等（FunMgr 注册，详见 `fun_builtin.cpp`）
- 工具函数：`basename`、`fileName`、`formatPath`、`merge`、`scriptDir` 等

## 9. 错误模型

- `try/catch/finally/throw` 已支持（2026-09 新增，详见 §10 决策记录与 §11.3 错误处理语义）
- 未捕获的运行时错误：设置带行号的错误消息 → 当前语句链立即中断 → 脚本终止
- headless 模式：`auto_code.exe --run <script.ac> [--root <dir>] [--vm]`，成功退出码 0，
  失败非 0 且输出 `[ERROR] 文件: 消息 at line N`
- 解析错误：报 `parse error: ... at line N`，不执行任何语句

## 10. 语言决策记录

| 决策 | 理由 |
|---|---|
| 顶层语句 = 隐式 main | 脚本语言惯例（Python/JS/C# 9/Java 25 均支持）；语句能力与函数体完全一致 |
| undefined 并入 null | 解释器值模型统一为 JSON 值；简化心智模型 |
| 对象键保序 | 低代码平台依赖属性声明序/响应列序；QJson 字母序是历史缺陷 |
| 静态属性不嵌入实例 | 对齐 JS（静态在类上）；实例序列化不含静态键 |
| 非法操作显式报错 | 静默 no-op 曾导致单例数据丢失且难排查（2026-09 单例回归教训） |
| 容器插入深拷贝 | 防引用环；接受性能取舍 |
| try/catch/throw（2026-09 新增） | 生成流水线需要"单表失败不中断整批"；错误经 m_error 通道在块边界拦截，throw 的值即错误消息（不附加行号） |
| 表达式嵌套上限 64 层（解析）/ 128 层（求值） | 实测每层解析递归约 9KB 栈；1MB 线程栈下的安全余量，不可随意调大 |
| 数据嵌套深度上限 256（clone/parse） | 深拷贝值语义下自包含追加会宽度爆炸/深度无界；超限截断，append 自包含直接拒绝 |
| 接口支持属性契约（对象形状，2026-09 新增） | `interface Pt { let x: Number; let tag?: String }`——类 implements 时属性必须存在且类型兼容（`?` 可选属性可缺失）；属性仅供声明/校验/符号表，不生成运行时代码。注：类型注解期无法区分接口与类（parseType 统一产 kClass），检查期按 m_interfaces 纠正 |

## 11. 值语义与相等语义（2026-09 定稿）

### 11.1 值语义 vs 引用语义

- **JSON 数据（对象/数组）是值语义**：`let b = a` 得到深拷贝快照，修改 b 不影响 a；
  作为参数传递、存入容器均为拷贝
- **类实例是引用语义**：实例由解释器登记（objId），赋值/传参共享同一实例
- **`a.append(a)` / `o.set(k, o)` 被直接拒绝**：深拷贝语义下自包含会宽度指数爆炸；
  脚本如需自引用结构，请使用类实例

### 11.2 `==` / `!=` 的精确定义

| 左 \ 右 | 行为 |
|---|---|
| 标量 vs 标量 | 数值/字符串/布尔按既有宽松规则比较 |
| null | 与 null 相等；与非 null 不等（含 undefined 并入 null） |
| 实例 vs 实例 | **引用相等**（objId 相同才相等，与 JS 一致） |
| 函数引用 | 函数名相等 |
| 类引用 | 类名相等 |
| 数组/普通对象 | **结构化深比较**（值语义下快照一致即相等；嵌套元素递归比较） |
| 不同种类之间 | 不相等；`< >` 按内部种类号全序（仅供排序，无语义承诺） |

### 11.3 错误处理语义

- `throw 值`：值即错误消息（字符串原样，非字符串经值转字符串），不附加行号
- `try { } catch (e) { }`：e 绑定错误消息字符串，仅 catch 块内可见；
  catch 内若再出错，新错误继续向上传播
- `finally`：无论是否出错都执行；try/catch 内 return/break 穿过 finally 后继续生效
- 未捕获的错误行为不变：带行号中断执行
- **错误文案策略**：引擎核心层错误为稳定英文文案（将来作为结构化 Diagnostic 的
  error code）；面向用户的中文提示由 UI 层负责，核心层不混用中英

## 12. 测试语料（语义护栏）

- `file/test/test_suite_main.ac`：全特性一致性套件（0–21 章，含顶层语句与静态边界）
- `tests/test_ac_interpreter.cpp`：解释器直测（含 == 语义 / sort / map/filter / ?.?? / const /
  对象方法 / do-while / try-catch 新特性用例）
- `tests/test_ac_vm.cpp`：解释器与字节码 VM 双跑对拍（35 项，结果与错误串必须一致）
- `file/test/test_engine_main.ac`、`file/test/test_block_comment_main.ac`：专项语料
- `tests/test_golden_script.cpp`：端到端 golden（键序正向 + 错误传播负向 + 双模式对拍）
- `tests/test_ac_param_default.cpp`、`tests/test_ac_json_value.cpp`：参数默认值与有序 JSON 单测

修改解释器/解析器后必须跑：`auto_code_tests`（全绿）+ `--run test_suite_main.ac`（无 [ERROR]）。
新语言特性须解释器与 VM 同时实现并补对拍用例（规约见 architecture.md §4.6）。
