${# ============================================================================}
${# 实体模板：data/{dataDir}/en_{tableName}_.go                    }
${# ----------------------------------------------------------------------------}
${#   - 生成头第一行 → 空行 → 包注释（紧贴 package）→ 实体 → TableName → Col 常量}
${#   - 字段行由 main_api.ac 预渲染（go fmt 列对齐），模板只逐行输出            }
${# 数据来源：pkgName/dataPkgComment/enClass/tableComment/tableName/enImports   }
${#           enFieldLines/colLines                                             }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${dataPkgComment}
package ${pkgName}

${if enImports}
${enImports}

${/if}// ${enClass} ${tableComment}(${tableName}) 实体
// 整数统一 int64（覆盖数据库 int/int unsigned，类型少、避免 uint/int 混用）
type ${enClass} struct {
${each line in enFieldLines}
${line}
${/each}}

// TableName 表名
func (${enClass}) TableName() string { return "${tableName}" }

${if hasJoin}// ${enClass}View 用户列表视图（联表）：主表实体 + 联表列
// 匿名嵌入 ${enClass}：GORM Scan 自动展开主表列，JSON 序列化字段平铺
// 联表列输出名统一 = "表名_列名"（前端契约稳定，不随表结构变化漂移）；
// tag 为字面量，需与 viewXxx 常量保持一致
type ${enClass}View struct {
${each line in enViewFieldLines}
${line}
${/each}}

// 联表列名常量（编译期拼写检查；与各联表 data 包的 ColXxx 对应；
// 本地声明而非 import 对方包：联表可能双向声明，避免包循环引用）
const (
${each line in joinConstLines}
${line}
${/each})

${joinSpecBlock}

${/if}// 列名常量
const (
${each line in colLines}
${line}
${/each})
