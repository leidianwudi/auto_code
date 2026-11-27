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

## jsontable 页面与接口

`shop.ac`和`erp.ac`按当前项目目录内勾选的`.jsontable`生成，不扫描全项目作为生成目标。多语言项目仍固定生成基础语言表，并随主表生成翻译实体；公共路由与枚举字典会同步更新。未勾选表的定义只用于关联、翻译及公共字典元数据，不改写其业务文件。单表回归入口为`file/test/partial_generation/main.ac`。

页面统一使用“字段呈现”配置列表、标题、列宽、查询、表单与开关。“页面与接口”只设置页面说明和自定义保存接口，不再维护第二套字段；原有标准CRUD页面继续按默认规则生成。

“字段”页是数据库物理结构快照：列顺序、类型、默认值、可空、主键及表/列注释以实际数据库为准。数据库变更后使用“从数据库刷新字段”查看并应用差异。界面标题使用独立的`title/label`，不能为了修改页面文案改写数据库注释；刷新和编辑字段会保留开关文案等扩展属性。枚举的`label`只写面向用户的文案，实际数字放在`value`；游戏目录及语言配置的启停筛选/编辑显示“停用、启用”，列表使用`switchEditable`开关。

- “字段呈现”：`selCols`控制列表及标题/列宽，`editCols`控制表单控件/必填/长度；精确查询复用`selColsEq`，单字段模糊查询复用`selColsLike`，各条件按AND组合，不提供跨列OR关键字搜索。标题由同一列统一维护。
- 字段来源（数据库/联表/多语言）显示在“字段引用”单元格的悬停提示中，不再单独占列。接口/临时字段（`viewFields`）已随自定义业务页模式一并移除，不再提供新增入口。
- “页面与接口”：标准CRUD页面默认每页10条，可切换10/20/50/100条；查询走服务端分页。勾选“使用自定义保存接口”后，表单保存只提交`keyField + editFields`白名单，列表开关只提交`keyField + switchField`，其余字段不进入业务接口；保存接口固定POST。
- 标准页面均可设置页尾纯文本说明，生成在列表和分页下方；枚举仍只在本表定义一次，列表和筛选复用。
- 平台配置只提供配置编码精确匹配、配置分组枚举筛选，多个条件必须同时满足。

`adminPage`不再允许`columns/formFields/filters/searchFields`重复节点。`base_config`使用标准CRUD页面，关闭新增/删除，保留详情和编辑，`adminQueryMode: read`只生成后端读接口；写入通过`customSave`调用`/api/market/base-config`，由后端做范围校验、事务和审计。配置实例见`file/go_pure/shop/base/base_config.jsontable`，沿用`shop.ac`生成，禁止手改产物。

## 构建与测试

```
cmake --preset vs2022
cmake --build build --target auto_code --config RelWithDebInfo      # 主程序
cmake --build build --target auto_code_tests auto_code_ui_tests --config RelWithDebInfo  # 测试
build\RelWithDebInfo\auto_code_tests.exe                            # 引擎与服务回归
build\RelWithDebInfo\auto_code_ui_tests.exe                         # 编辑器回归（offscreen）
```
