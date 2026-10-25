${# ============================================================================}
${# InSel 模板（split 布局）：data/{dataDir}/in_sel_{tableName}_.go              }
${# ----------------------------------------------------------------------------}
${#   - 嵌入 common.BasePage（page/pageSize）+ 全指针筛选字段 + Sort 白名单入参  }
${#   - 字段行由 main_api.ac 预渲染（含嵌入列对齐），模板只逐行输出             }
${# 数据来源：pkgName/selClass/tableComment/tableName/selImports/selFieldLines  }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${selImports}

// ${selClass} ${tableComment}(${tableName}) 查询入参
// 嵌入 common.BasePage 获得 page / pageSize；筛选字段全指针：nil=不加条件，非 nil=按它筛选（含 0、空串）
type ${selClass} struct {
${each line in selFieldLines}
${line}
${/each}}
