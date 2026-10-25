${# ============================================================================}
${# 实体模板（split 布局）：data/{dataDir}/en_{tableName}_.go                    }
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

// 列名常量
const (
${each line in colLines}
${line}
${/each})
