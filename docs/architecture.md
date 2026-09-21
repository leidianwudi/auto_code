# Auto Code 项目整体架构

## 1. 项目定位

Auto Code 是一个**代码生成工具**，通过自定义脚本语言（.ac）和模板引擎（.tpl）驱动，从数据库表结构或 JSON 配置生成前端/后端代码文件。

## 2. 技术栈与文档导航

- **C++** + **Qt 6.12.0** (msvc2022_64)
- **构建系统**：CMake（日常 VS 2022 生成器预设，见 CMakePresets.json；clang-tidy 场景另配 Ninja 目录）
- **编译器**：MSVC 2022
- **代码缩进**：2 空格（`.editorconfig`）
- **注释规范**：Doxygen 风格，注释内容用中文

| 文档 | 内容 | 何时读 |
|------|------|--------|
| 本文 | 整体架构、目录、数据流、设计决策、常见陷阱 | 新人入门 / 跨模块改动前 |
| [ac_language_spec.md](ac_language_spec.md) | .ac 语言语义权威（值语义/错误模型/决策记录） | 改语言特性前必读，先改它再改代码 |
| [ui_architecture.md](ui_architecture.md) | UI 组件规范、样式表、对话框/Tab/QSS 规范 | 写界面代码前 |
| [code_style.md](code_style.md) | C++ 命名/格式/分层边界（工具自动执行） | 写任何 C++ 代码前 |
| [changelog.md](changelog.md) | 语言/内置函数/字节码对外可见变更记录 | 发版评估影响面 / 加破坏性变更时更新 |

## 3. 目录结构

```
src/
├── engine/                     # 脚本引擎 + 模板引擎
│   ├── ac_language.h           # 语言常量中心（关键字、内置函数、类、模板标记）
│   ├── ac_value_str.h          # 值转字符串辅助
│   ├── json_validator.h/.cpp   # JSON 校验器
│   ├── schema_validator.h/.cpp # Schema 校验器
│   ├── validation_result.h     # 校验结果
│   ├── script/                 # .ac 脚本引擎（Lexer → Parser → Interpreter）
│   ├── tpl/                    # .tpl 模板引擎（Lexer → Parser → Renderer）
│   └── function/               # C++ 函数注册中心（FunMgr）
├── ui/                         # 界面层
│   ├── main_dev/               # 主开发界面（代码编辑器 + 文件树 + 输出面板）
│   ├── json_vue/               # JSON Vue 可视化编辑器
│   ├── create/                 # 创建项目界面
│   └── demo/                   # 演示界面
└── util/                       # 工具层
    ├── common/                 # 通用工具（文件、JSON、HTTP、路径解析、日志）
    ├── design/                 # 设计模式（单例）
    └── ui/                     # UI 工具（窗口框架、样式、按钮、代码编辑器、高亮器）
        ├── code/               # 代码编辑器（括号匹配、缩进参考线、查找、补全、导航）
        ├── component/          # UI 组件（按钮、样式、图标、消息框、下拉框）
        └── highlighter/        # 语法高亮（AC、JSON、TPL、TS）
```

## 4. AC 脚本引擎

### 4.1 架构总览

```
.ac 源文件
    │
    ▼
AcEngine（单例入口）
    │
    ▼
AcExecutor（编排层）
    │
    ├─ AcLexer         词法分析 → Token 流
    ├─ AcParser        语法分析 → AST 树
    ├─ linkImports     模块链接（import/export）
    ├─ AcTypeChecker   静态类型检查
    ├─ AcValidator / AcDiagnostic   语义验证 + 结构化诊断
    │
    ├─ AcInterpreter   解释执行 AST（kInterpreter 默认模式）
    │    └─ FunMgr::call()  调用 C++ 后端函数
    │
    └─ AcCompiler → AcModule → AcVm   字节码执行（kBytecode 模式）
         └─ AcBytecodeCache  预编译缓存：命中跳过 parse+compile
              └─ AcModuleIo  字节码二进制序列化（.ac_cache/*.acb）
```

### 4.2 核心文件

执行模式：`AcExecutor::setExecMode()`（默认 `kInterpreter`；`kBytecode` 编译为栈式字节码后经 `AcVm` 执行）。两模式经 `tests/test_ac_vm.cpp` 双跑对拍断言结果与错误完全一致。灰度开关见 4.6。

| 文件 | 职责 |
|------|------|
| [ac_engine.h](file:///d:/work/github/auto_code/src/engine/script/ac_engine.h) | 单例入口，读取文件 → 委托 AcExecutor |
| [ac_executor.h](file:///d:/work/github/auto_code/src/engine/script/ac_executor.h) | 编排层：Lexer → Parser → 链接 → 类型检查 → 解释执行 |
| [ac_lexer.h](file:///d:/work/github/auto_code/src/engine/script/ac_lexer.h) | 词法分析，源码 → Token 流 |
| [ac_parser.h](file:///d:/work/github/auto_code/src/engine/script/ac_parser.h) | 语法分析，Token 流 → AST |
| [ac_parser_expr.cpp](file:///d:/work/github/auto_code/src/engine/script/ac_parser_expr.cpp) | 表达式解析（拆分自 ac_parser.cpp） |
| [ac_parser_stmt.cpp](file:///d:/work/github/auto_code/src/engine/script/ac_parser_stmt.cpp) | 语句解析（拆分自 ac_parser.cpp） |
| [ac_interpreter.h](file:///d:/work/github/auto_code/src/engine/script/ac_interpreter.h) | 解释器，执行 AST 语句和表达式 |
| [ac_interpreter_expr.cpp](file:///d:/work/github/auto_code/src/engine/script/ac_interpreter_expr.cpp) | 表达式求值（拆分自 ac_interpreter.cpp） |
| [ac_interpreter_stmt.cpp](file:///d:/work/github/auto_code/src/engine/script/ac_interpreter_stmt.cpp) | 语句执行（拆分自 ac_interpreter.cpp） |
| [ac_type.h](file:///d:/work/github/auto_code/src/engine/script/ac_type.h) | Token 类型 + AST 节点 + 类型系统定义 |
| [ac_type_checker.h](file:///d:/work/github/auto_code/src/engine/script/ac_type_checker.h) | 静态类型检查 |
| [ac_symbol_table.h](file:///d:/work/github/auto_code/src/engine/script/ac_symbol_table.h) | 符号表（变量、函数、类声明） |
| [ac_object_manager.h](file:///d:/work/github/auto_code/src/engine/script/ac_object_manager.h) | 对象生命周期管理（引用计数 + dispose） |
| [ac_builtin_loader.h](file:///d:/work/github/auto_code/src/engine/script/ac_builtin_loader.h) | builtin.d.ac 加载 + 原生类注册 |
| [ac_builtin_eval.h](file:///d:/work/github/auto_code/src/engine/script/ac_builtin_eval.h) | 内置类型方法求值（String/Array 方法） |
| [ac_validator.h](file:///d:/work/github/auto_code/src/engine/script/ac_validator.h) | 语义验证器 |
| [ac_diagnostic.h](file:///d:/work/github/auto_code/src/engine/script/ac_diagnostic.h) | 结构化诊断（错误码/定位/收集器，替代正则反解） |
| [ac_compiler.h](file:///d:/work/github/auto_code/src/engine/script/ac_compiler.h) | AST → 字节码模块（AcModule）编译 |
| [ac_opcode.h](file:///d:/work/github/auto_code/src/engine/script/ac_opcode.h) | 栈式指令集 + AcModule/AcFuncUnit/AcTryEntry |
| [ac_vm.h](file:///d:/work/github/auto_code/src/engine/script/ac_vm.h) | 栈式虚拟机执行 AcModule（语义镜像解释器） |
| [ac_module_io.h](file:///d:/work/github/auto_code/src/engine/script/ac_module_io.h) | AcModule 二进制序列化（运行时子集） |
| [ac_bytecode_cache.h](file:///d:/work/github/auto_code/src/engine/script/ac_bytecode_cache.h) | 预编译缓存：内容哈希 + 版本失效 |
| [ac_semantic_service.h](file:///d:/work/github/auto_code/src/engine/script/ac_semantic_service.h) | 进程内语义服务：诊断/补全/定义跳转/引用查找 |
| [ast_visitor.h](file:///d:/work/github/auto_code/src/engine/script/ast_visitor.h) | AST 访问者模式基类 |

### 4.3 语言特性

#### 关键字（[ac_language.h](file:///d:/work/github/auto_code/src/engine/ac_language.h)）

```
let  new  for  if  else  return  function  class  extends  implements
interface  enum  import  export  from  as  static  public  protected
private  override  super  this  null  undefined  true  false  while
break  continue  switch  case  default  constructor  using  dispose  in
```

#### 类型系统

| 类型 | 说明 |
|------|------|
| `Number` | 数字（int64） |
| `String` | 字符串 |
| `Bool` | 布尔 |
| `Void` | 无返回值 |
| `Any` | 任意类型（不检查） |
| `Type[]` / `Array<Type>` | 数组（必须带元素类型） |
| `ClassName` | 用户自定义类 |
| `InterfaceName` | 接口类型 |

#### 语法示例

```typescript
// 类定义
class User {
  let name: String = ""
  let age: Number = 0

  function greet(): String {
    return "Hello, " + this.name
  }
}

// for-in 循环
for (let item: String in items) {
  printLog(item)
}

// C-style for 循环
for (let i = 0; i < 10; i = i + 1) {
  printLog(i)
}

// 模块导入
import { User, Order } from "models.ac"

// 导出函数
export function generate(data: Object): Void {
  let db = new DB({host: "localhost", user: "root", database: "test"})
  let schema = db.tableSchema({table: "users"})
  renderTpl("user.tpl", {schema: schema})
  db.dispose()
}
```

#### 内置一级函数

| 函数 | 说明 |
|------|------|
| `renderTpl(tplPath, data)` | 渲染模板文件 |
| `readJson(path)` | 读取 JSON 文件 |
| `readFile(path)` | 读取文本文件 |
| `writeFile(path, content)` | 写入文本文件 |
| `printLog(text)` | 打印日志 |
| `printError(text)` | 打印错误 |
| `getCheckedFiles()` | 获取勾选文件列表 |
| `merge(obj1, obj2)` | 合并 JSON 对象 |
| `basename(path)` | 文件基名（无扩展名） |
| `formatPath(pattern, data)` | 格式化路径 |
| `assert(cond, msg)` | 断言 |
| `fileExists(path)` | 文件是否存在 |

#### 内置类

| 类 | 方法 | 说明 |
|----|------|------|
| `DB` | `tableSchema()`, `tableInfo()`, `query()`, `disconnect()` | 数据库操作 |
| `File` | `read()`, `write()` | 文件读写 |

#### call() 路由函数

| 类 | 方法 | 说明 |
|----|------|------|
| `str` | `toLowerCase()`, `toUpperCase()`, `trim()`, `capitalize()`, `substring()`, `replace()` | 字符串操作 |

### 4.4 类型注解规则

- `let` 声明无类型注解时，必须能从初始化值推断类型（字面量、`new`、null/undefined）
- 数组必须带元素类型：`Type[]` 或 `Array<Type>`，禁止裸 `Array`
- 非空对象字面量必须带类型注解
- 函数返回值必须带类型注解（默认 Any）
- 动态添加未声明的类属性禁止

### 4.5 模块系统

- `import { A, B as C } from "file.ac"` — 导入符号
- `export class/function/enum` — 导出符号
- 导入文件自动注入所有导出类作为类型依赖
- 循环导入检测（visited 集合）

### 4.6 双执行路径收敛计划

**现状**：默认解释器；字节码 VM 语义 1:1 对齐中。VM 已具备与解释器相同的语句级
调试钩子（`kOpStmt` 指令 → `AcDebugger::onStatement`，非调试会话零开销）。

**灰度开关**（不影响脚本语义）：
- 环境变量 `AC_EXEC_MODE=vm`：全局默认走 VM（所有入口生效），日常灰度试用
- headless 单次执行：`auto_code --run <script.ac> --vm`

**对拍护栏**：
- `tests/test_ac_vm.cpp`：35 项进程内双跑（结果序列化 + 错误串一致）
- `tests/test_golden_script.cpp` dual-mode 用例：端到端（--run 子进程）双模式
  stdout 逐字节一致 + 退出码一致，覆盖 FunMgr 集成、日志输出、错误传播

**收敛步骤**：灰度试用积累对拍数据 → 无差异后切默认 `kBytecode` → 解释器冻结
特性、只修 bug，保留一个版本周期作逃生口 → 移除或降级为对拍基准。

**新语言特性开发规约**：解释器与 VM **必须同时实现**（kOpStmt 行号、错误消息、
对象生命周期三处最易漂移），并至少各加一条 `test_ac_vm.cpp` 对拍用例。

### 4.7 错误通道冻结点

**冻结点已宣布**（2026-09）：`AcExecutor::error()` 等 legacy `QString` 错误串为
冻结接口——**新代码禁止新增依赖**，一律使用结构化诊断：

- `AcDiagCollector`（编译期各阶段 + 运行时 AC5001 均已汇入）
- 出口：`AcExecutor::validationResults()` / `diagnostics()`

既有 legacy 调用方（UI 若干处）逐步迁移，迁移完成后删除 `error()`。
语言层面的兼容性策略（废弃流程 / 字节码版本戳 / 存量普查）见
[changelog.md](file:///d:/work/github/auto_code/docs/changelog.md)。

## 5. 模板引擎

### 5.1 架构总览

```
.tpl 模板文件
    │
    ▼
TplEngine（协调者）
    │
    ├─ TplLexer       词法分析 → Token 流
    ├─ TplParser      语法分析 → AST 树
    ├─ TplValidator   模板校验（括号匹配、注释处理）
    └─ TplRenderer    遍历 AST → 输出字符串
         │
         └─ resolvePath()  变量/函数解析
              │
              └─ FunMgr::call()  调用 C++ 函数
```

### 5.2 核心文件

| 文件 | 职责 |
|------|------|
| [tpl_engine.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_engine.h) | 协调者：render() 入口 + resolvePath() |
| [tpl_lexer.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_lexer.h) | 词法分析，模板 → Token 流 |
| [tpl_parser.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_parser.h) | 语法分析，Token 流 → AST |
| [tpl_ast.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_ast.h) | AST 节点定义（Text/Variable/If/Each） |
| [tpl_renderer.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_renderer.h) | 渲染器，遍历 AST 输出字符串 |
| [tpl_validator.h](file:///d:/work/github/auto_code/src/engine/tpl/tpl_validator.h) | 模板校验 |

### 5.3 模板语法

| 语法 | 说明 |
|------|------|
| `${variable}` | 变量替换 |
| `${obj.property}` | 嵌套属性 |
| `${a + b * c}` | 算术运算 |
| `${str.toLowerCase(x)}` | 函数调用 |
| `${if condition}` ... `${else}` ... `${/if}` | 条件判断 |
| `${else if condition}` | else-if 链 |
| `${each item in items}` ... `${/each}` | 循环（显式命名） |
| `${each items}` ... `${/each}` | 循环（隐式命名，用 `${.}` 引用） |
| `${# 注释内容}` | 注释（整行跳过） |

### 5.4 AST 节点

```
AstNode（基类）
├── TextNode       纯文本（原样输出）
├── VariableNode   ${expression}
├── IfNode         条件分支（branches 数组，支持 else-if 链）
└── EachNode       循环（itemName + arrayName + body）
```

### 5.5 空行控制规则

1. **块标签独占一行**时，Lexer 标记 `aloneOnLine`，整行（含缩进和换行符）从输出中剔除
2. **块标签行内出现**时，保留所有空白字符
3. **不做智能空行压缩**，模板里几个 `\n` 就输出几个 `\n`

### 5.6 注释处理

`${# ...}` 注释必须在预处理阶段**跳到行尾**，替换为空格。不能用深度计数找闭合 `}`，否则注释中包含 `[{...}]` 等嵌套括号会导致误判。

## 6. 函数管理器（FunMgr）

### 6.1 架构

```
FunMgr（单例）
  │
  ├── "builtin"  → FunBuiltin    （renderTpl, readJson, writeFile, ...）
  ├── "str"      → FunStr        （toLowerCase, toUpperCase, trim, ...）
  ├── "DB"       → FunDb         （tableSchema, query, ...）
  ├── "file"     → FunFile       （read, write）
  └── "json"     → FunJson       （JSON 操作）
```

### 6.2 调用方式

```cpp
// C++ 内部调用（值模型为 accore::AcJsonValue，参数为 accore 数组值）
accore::AcJsonValue args = accore::AcJsonValue::makeArray();
args.append(QStringLiteral("Hello"));
accore::AcJsonValue r = FunMgr::ins().call("str", "toLowerCase", args);

// .ac 脚本中调用
call("str", "toLowerCase", "Hello")

// .tpl 模板中调用
${str.toLowerCase(Hello)}
```

新增内置函数三步：实现（fun_*.cpp）→ 注册表加一项 → `ac_language.h` 加名字常量
与 `kAll` 列表（供解析/高亮/补全）。参数校验统一走 `fun_args.h` 的
`requireCount/requireString/...`（失败自动 setError）。

### 6.3 文件列表

| 文件 | 职责 |
|------|------|
| [fun_mgr.h](file:///d:/work/github/auto_code/src/engine/function/fun_mgr.h) | 单例管理器，二级映射 className → funcName → FunPtr |
| [fun_builtin.h](file:///d:/work/github/auto_code/src/engine/function/fun_builtin.h) | 内置一级函数 |
| [fun_str.h](file:///d:/work/github/auto_code/src/engine/function/fun_str.h) | 字符串函数 |
| [fun_db.h](file:///d:/work/github/auto_code/src/engine/function/fun_db.h) | 数据库函数 |
| [fun_file.h](file:///d:/work/github/auto_code/src/engine/function/fun_file.h) | 文件函数 |
| [fun_json.h](file:///d:/work/github/auto_code/src/engine/function/fun_json.h) | JSON 函数 |

## 7. UI 层架构

### 7.1 主开发界面（main_dev）

```
MainDevUi（主窗口）
├── MainDevMgr（管理器）
│   ├── 文件树（TreeDir）
│   ├── 代码编辑器（CodeEditor，多 Tab）
│   ├── 符号导航器（SymbolNavigator）
│   └── 输出面板（CodeLog）
├── MainDevModel（数据模型）
└── MainDevUiExt（扩展功能：Tab 红点、拖拽等）
```

`MainDevMgr` 分文件实现（_file/_tab/_connect/_navigate/_rename/_session/_theme）
只是物理拆分；**collaborator 化进行中**：首例 `NavigationHistory`
（[navigation_history.h](file:///d:/work/github/auto_code/src/ui/main_dev/navigation_history.h)，
纯逻辑双栈状态机，2026-09 拆出）。后续按同一模式拆：问题聚合（m_fileIssues）、
防抖定时器群、会话恢复。调试已拆出 `DebugController`。

脚本执行：`QtConcurrent::run` 工作线程 + `requestCancel()` 取消 +
`QueuedConnection` 回 GUI（见 debug_controller.cpp），长脚本不卡 UI。

### 7.2 JSON Vue 可视化编辑器（json_vue）

```
JsonVueWidget
├── JsonVueEditor（可视化编辑表格）
├── JsonVueModel（JSON 序列化/反序列化）
├── IconPickerDialog（图标选择器，3 套图标库）
├── IconLoader（图标异步加载单例）
├── ButtonConfigDialog（按钮配置）
├── ComboboxConfigDialog（下拉框配置）
└── StyleConfigDialog（样式配置）
```

### 7.3 代码编辑器

```
CodeEditor（QPlainTextEdit 扩展）
├── BracketMatcher    括号匹配高亮
├── IndentGuide       缩进参考线
├── CodeFindBar       查找替换栏
├── CodeValidator     语法校验
├── CodeEditorComplete 代码补全
├── CodeEditorNavigate 跳转定义
├── FormatCode        代码格式化
├── GuessCode         代码推测
└── SymbolNavigator   符号导航
```

### 7.4 语法高亮器

| 文件 | 语言 |
|------|------|
| [light_ac.cpp](file:///d:/work/github/auto_code/src/util/ui/highlighter/light_ac.cpp) | .ac 脚本 |
| [light_json.cpp](file:///d:/work/github/auto_code/src/util/ui/highlighter/light_json.cpp) | .json |
| [light_tpl.cpp](file:///d:/work/github/auto_code/src/util/ui/highlighter/light_tpl.cpp) | .tpl 模板 |
| [light_ts.cpp](file:///d:/work/github/auto_code/src/util/ui/highlighter/light_ts.cpp) | TypeScript |

## 8. 工具层

### 8.1 通用工具（common）

| 文件 | 职责 |
|------|------|
| [path_resolver.h](file:///d:/work/github/auto_code/src/util/common/path_resolver.h) | 文件搜索路径集中管理 |
| [util_file.h](file:///d:/work/github/auto_code/src/util/common/util_file.h) | 文件操作（统一 UTF-8 读取） |
| [util_json.h](file:///d:/work/github/auto_code/src/util/common/util_json.h) | JSON 工具 |
| [http_client.h](file:///d:/work/github/auto_code/src/util/common/http_client.h) | HTTP 客户端 |
| [ac_log.h](file:///d:/work/github/auto_code/src/util/common/ac_log.h) | 日志宏 |
| [code_constants.h](file:///d:/work/github/auto_code/src/util/common/code_constants.h) | 代码常量 |

### 8.2 UI 工具

详见 [ui_architecture.md](file:///d:/work/github/auto_code/docs/ui_architecture.md)。

## 9. 数据流

### 9.1 代码生成流程

```
1. 用户打开 .jsonvue 配置文件
2. JsonVueEditor 可视化编辑 → JsonVueModel 序列化为 JSON
3. 用户在代码编辑器中编写 .ac 脚本
4. 点击构建按钮
5. AcEngine::execute(acFilePath)
   │
   ├─ AcLexer     词法分析
   ├─ AcParser    语法分析 → AST
   ├─ linkImports 模块链接
   ├─ AcTypeChecker 类型检查
   └─ AcInterpreter 执行
        │
        ├─ readJson()    读取 .jsonvue 配置
        ├─ new DB()      连接数据库
        ├─ db.tableSchema() 获取表结构
        ├─ renderTpl()   渲染 .tpl 模板
        │    │
        │    ├─ TplLexer   词法分析
        │    ├─ TplParser  语法分析 → AST
        │    └─ TplRenderer 渲染输出
        │         │
        │         └─ FunMgr::call()  调用 C++ 函数
        │
        └─ writeFile()   写入生成文件
6. generatedFiles() 返回文件列表
7. UI 显示生成结果
```

### 9.2 文件类型

| 后缀 | 类型 | 说明 |
|------|------|------|
| `.ac` | 脚本 | AC 脚本源文件 |
| `.d.ac` | 声明 | 类型声明文件（类似 .d.ts） |
| `.tpl` | 模板 | 字符串模板文件 |
| `.json` | 数据 | JSON 数据文件 |
| `.jsonvue` | 配置 | JSON Vue 可视化配置 |

## 10. 关键设计决策

### 10.1 语言常量集中管理

[ac_language.h](file:///d:/work/github/auto_code/src/engine/ac_language.h) 集中定义所有关键字、内置函数名、类名、方法名。新增函数/类/关键字只需改此文件，脚本引擎、语法高亮、自动补全自动同步。

### 10.2 大文件拆分

大型文件按职责拆分：
- `ac_parser.cpp` → `ac_parser_expr.cpp` + `ac_parser_stmt.cpp`
- `ac_interpreter.cpp` → `ac_interpreter_expr.cpp` + `ac_interpreter_stmt.cpp`
- `main_dev_mgr.cpp` → `main_dev_mgr_connect.cpp` + `main_dev_mgr_file.cpp` + `main_dev_mgr_navigate.cpp` + `main_dev_mgr_tab.cpp`

### 10.3 类型检查分派

类型检查逻辑按表达式类型分派到独立方法，使用 dispatch 模式。

### 10.4 builtin.d.ac 加载

执行阶段必须加载 `builtin.d.ac` 声明文件，用于内置函数和类方法的类型推断。加载逻辑集中在 `AcBuiltinLoader` 类。

### 10.5 文件路径解析

文件搜索路径逻辑集中在 `PathResolver` 类，文件读取统一使用 `UtilFile::readUtf8()`。

## 11. 常见陷阱

### 11.1 QJsonValue::toInt() 溢出

`QJsonValue::toInt()` 对超出 INT_MAX 的值触发 Qt 断言。使用 `safeJsonToInt()` 或 `toDouble()` 替代。

### 11.2 QJsonValue::toString() 不转换数值（Qt 6）

Qt 6 中 `QJsonValue(10.0).toString()` 返回空字符串，需显式转换：
```cpp
QString s = v.isString() ? v.toString() : QString::number(v.toDouble());
```

### 11.3 模板注释处理

`${# ...}` 注释不能用深度计数找闭合 `}`，必须跳到行尾，否则注释中的 `[{...}]` 会导致误判。

### 11.4 C-style for 循环必须用 let

`for (i = 0; ...)` 不带 `let` 会导致解析错误（`i` 被误认为 for-in 变量名）。必须写 `for (let i = 0; ...)`。

### 11.5 数组必须带元素类型

`let arr = []` 无类型注解禁止使用。必须写 `let arr: String[] = []`。

### 11.6 模板标签字符串构造

`${else if ...}` 必须用 `AcKeyword::kElse`（"else"）拼接，不能硬编码字符串。注意空格：`${else if `。

### 11.7 qFloor(double) 在 Qt 6.12 返回 int

`qFloor(double)` 在 Qt 6.12 返回 int 并对大值触发断言。时间戳处理应使用 `std::floor(double)`（返回 double）。

### 11.8 printLog 参数类型为 String

`printLog` 静态签名要求 String 参数，传 Number 会被类型检查拒绝。用拼接转换：
`printLog("" + someNumber)`。

## 12. 运行时值模型与标识符驻留

### 12.1 运行时值种类（accore::AcJsonValue）

运行时值不是"裸 JSON + 内部魔法键"，而是带种类标记的完整值模型：

| Type | 说明 | 存储 |
|------|------|------|
| Null/Bool/Number/String | 标量 | 直接成员 |
| Array/Object | 数据容器 | COW 共享 |
| `Instance` | 类实例 | 类名 + 唯一 objId 存**专用字段**，属性存对象存储 |
| `ClassRef` | 类引用（`ClassName::x` 上下文） | 类名存专用字段 |
| `FuncRef` | 函数引用（function 表达式） | 函数名存专用字段 |

设计要点：

- **无内部协议键**：实例的类名/objId 曾用 `__class__`/`__objId__` 键编码在用户属性空间里，
  用户数据若含这些键会污染引擎；现已全部迁入种类专用字段，`members()/keys()` 即纯属性
- **实例即对象**：`isObject()` 对 Instance 返回 true（JS 语义），属性访问接口直接可用；
  区分纯数据对象与实例用 `isInstance()`
- **序列化**：Instance 按属性集输出；ClassRef/FuncRef 输出 null（运行时引用不应持久化）
- 类名提取统一走 `instanceClass()`，唯一 id 走 `instanceObjId()`

引擎全链路值模型统一为 `AcJsonValue`（含 `AcExecutor::execute()` 返回值，
2026-09 从 QJsonValue 迁移）；QJson 只允许出现在 `ac_json_value.h` 适配层与
`fun_json.cpp` 边界，UI 消费时调 `toQJsonValue()`。

### 12.2 源码位置（AcLoc）

Token 与所有 AST 节点统一携带 `AcLoc{line, col, offset}`（列/偏移按 UTF-16 码元计，
与 QString 索引一致；line/col 1-based）。词法器逐 token 计算列号与偏移。
诊断信息目前仍只输出行号（`at line %N`），列号供 LSP/格式化等后续特性使用。

### 12.3 标识符驻留（accore::AcIdentPool）

[src/core/common/ac_ident_pool.h](file:///d:/work/github/auto_code/src/core/common/ac_ident_pool.h)
实现字符串驻留：同名标识符全局唯一 `uint32 id`，id 比较 O(1)，id↔名字互查 O(1)。

- **当前接入点**：AcLexer 对每个标识符 token 驻留（`Token::identId`），
  词法期一次哈希，全阶段共享
- **演进路径**：当引入字节码/IR 或 LSP 需要稳定符号身份时，将 AST 的 name 字段
  批量切换为 `AcIdent`；符号表/类型检查/重命名以 id 为键
- **为何不立即全量 id 化**：本语言运行时的热点是动态 JSON 属性访问（数据键，
  不可驻留）与 QHash 变量查找（已 O(1)），AST 全量 id 化当前收益低、扰动面大；
  待有真实消费方（IR/符号身份）时再做

## 13. QString 策略与分层边界

| 层 | Qt 依赖 | 字符串策略 |
|----|---------|-----------|
| `src/core/` | 仅 QtCore，禁 QtGui | 新代码优先 `std::string_view`/驻留 id；值类型内部沿用 QString（UTF-16 与运行时语义一致） |
| `src/engine/` | QtCore，禁 QtGui/QtWidgets | 同上；AST 标识符经驻留池去重 |
| `src/ui/`、`src/util/ui/` | 全量 Qt | QString 自由使用（与 QWidget API 交互） |

原则：

1. **问题不在 QString，在边界**：GUI 层用 QString 零成本；引擎核心若被外部嵌入
   （无 Qt 环境）才需要替换。先定边界，后按模块迁移（lexer → parser → interpreter），
   禁止一次性全局替换（UTF-8/UTF-16 边界 bug 风险）
2. 字符串字面量与 JSON 数据保留 QString：脚本的字符串值运行时即 UTF-16，
   全链路换 UTF-8 需要连带动 AccJsonValue/FunMgr 签名，收益不成比例，暂缓
3. 新增编译器基础设施（位置、驻留、未来 IR）一律落在 `src/core/`，与 Qt 解耦

## 14. 测试地图

| 文件 | 覆盖 |
|------|------|
| test_ac_interpreter / test_ac_vm | 解释器 116 项 / 双模式对拍 35 项 |
| test_ac_cache / test_ac_recovery | 字节码缓存 / parser 错误恢复 |
| test_ac_semantic / test_rename | 单文件语义 / 跨文件引用（import 别名、遮蔽） |
| test_ac_diagnostic | 结构化诊断 |
| test_tpl / test_function | 模板语法 / 内置函数 |
| test_golden_script | 端到端（--run 子进程：键序、错误传播、双模式对拍） |

构建：`cmake --build <build-dir> --target auto_code_tests --config RelWithDebInfo`，
运行 `build/RelWithDebInfo/auto_code_tests.exe`（返回 0 = 全部通过）。
**提交前必跑**；golden 段需 auto_code.exe 与测试 exe 同目录。
