# 代码风格规范

本文是仓库 C++ 代码风格的唯一权威来源，配合工具自动执行：

- 格式化：`.clang-format`（Google 基础、2 空格缩进、行宽 100）
- 命名检查：`.clang-tidy`（`readability-identifier-naming`）
- 缩进/换行：`.editorconfig`

## 1. 命名规范

| 对象 | 风格 | 示例 |
|------|------|------|
| 类型（类/结构体/枚举） | 大驼峰 | `AcLexer`、`TokenType`、`AcLoc` |
| 枚举常量 | `k` + 大驼峰 | `TokenType::kIdent`、`AcType::kString` |
| 命名空间 | 大驼峰 | `namespace AcKeyword` |
| 函数/方法 | 小驼峰 | `parseStringLiteral()` |
| 局部变量/参数 | 小驼峰 | `tokenCount` |
| 类私有/保护成员 | `m_` + 小驼峰 | `m_error`、`m_scopeStack` |
| 结构体公有成员 | 小驼峰，无前缀 | `Token.text`、`Expr.kind` |
| 命名空间级常量 | `k` + 大驼峰 | `AcKeyword::kLet` |
| 宏 | 全大写下划线 | `PROJECT_SOURCE_DIR` |

规则：

1. **禁止裸全局枚举**：枚举一律 `enum class`，使用时必须带类型限定（`TokenType::kIf`），
   避免常量泄漏到外层命名空间。
2. **禁止匈牙利命名**：不使用 `str`、`n`、`b` 等类型前缀，类型信息由类型系统表达。
3. **不使用下划线开头的标识符**（保留给编译器/标准库）。

## 2. 格式

- 缩进：2 空格，禁止 Tab
- 行宽：100 列
- 大括号：函数/类定义换行，控制语句同行（`.clang-format: Google` 决定）
- 头文件顺序（clang-format `SortIncludes` 自动排列）：本文件头 → 模块头 → Qt 头 → STL 头

## 3. 注释

- Doxygen 风格，注释内容用中文：`/// @brief`、`/// @param`、`/// @return`
- 注释解释"为什么"，不复述"做什么"
- 公共 API（头文件中的类与函数）必须有注释

## 4. 分层边界（与 [architecture.md](architecture.md) 配合）

- `src/core/`：与 UI 无关的底层类型，**禁止 include QtGui**；QtCore 依赖按模块逐步削减
- `src/engine/`：脚本/模板引擎，可依赖 QtCore，**禁止依赖 QtGui/QtWidgets**
- `src/ui/`、`src/util/ui/`：界面层，可使用全部 Qt 模块
- 新增代码必须遵守边界；存量代码修改时顺手收敛，不做无关联的大规模搬移

## 5. 工具用法

```bash
# 格式化单个文件（clang-format 10+）
clang-format -i src/engine/script/ac_parser.cpp

# 命名检查（需 compile_commands.json；Ninja 生成器自动生成）
clang-tidy -p build src/engine/script/ac_parser.cpp
```

VS 生成器不产出 `compile_commands.json`；CI 或本机需要跑 clang-tidy 时，
用 Ninja 生成器额外配置一个构建目录即可：

```bash
cmake -S . -B build-tidy -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```
