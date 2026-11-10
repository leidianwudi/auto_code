# auto_code

基于自定义脚本语言（`.ac`）与模板引擎（`.tpl`）的代码生成工具：以 `.jsontable` 表配置为事实源，一次配置生成前后端代码。

- 后端：NestJS（`file/crud_nest`）、Go + gin + gorm（`file/crud_gin`）
- 前端：Vue 3 管理端（`file/admin_vue` 基于 Element Plus、`file/admin_pure` 基于 vue-pure-admin）

## 文档

- [整体架构](docs/architecture.md) — 目录结构、数据流、设计决策、常见陷阱（新人入门）
- [AC 语言规范](docs/ac_language_spec.md) — .ac 语言语义权威
- [UI 架构与样式规范](docs/ui_architecture.md)
- [代码风格规范](docs/code_style.md)
- [变更日志与兼容性策略](docs/changelog.md)

## 构建与测试

```
cmake --preset vs2022
cmake --build build --target auto_code --config RelWithDebInfo      # 主程序
cmake --build build --target auto_code_tests --config RelWithDebInfo  # 测试
build\RelWithDebInfo\auto_code_tests.exe                            # 返回 0 = 全部通过
```
