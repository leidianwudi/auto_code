# auto_code
自动生成代码程序，支持生成nest，vue代码

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
