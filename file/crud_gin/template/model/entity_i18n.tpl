${# ============================================================================}
${# i18n 翻译实体：data/{dataDir}/en_{i18nTable}_.go                          }
${# ----------------------------------------------------------------------------}
${#   - 翻译表依附主表：与主表实体同包（不单独成包、不生成 CRUD）                }
${#   - 内容 = 翻译表列定义（id/ext/lang/翻译字段/时间戳）+ 列名常量            }
${#   - 读写方法在 {tableName}_repo_i18n_.go 中挂在主表 Repo 上                 }
${# 数据来源：pkgName/i18nEnClass/i18nTable/i18nTableComment/tableName/         }
${#           i18nEntityFieldLines/i18nColLines/i18nEntityNeedTime              }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${if i18nEntityNeedTime}import (
	"time"
)

${/if}// ${i18nEnClass} ${i18nTableComment}(${i18nTable}) 翻译实体
// 由主表 ${tableName} 的 i18n 配置派生，仅供本包翻译读写使用，不对外暴露 CRUD。
type ${i18nEnClass} struct {
${each line in i18nEntityFieldLines}
${line}
${/each}}

// TableName 表名
func (${i18nEnClass}) TableName() string { return "${i18nTable}" }

// 列名常量（翻译读写引用）
const (
${each line in i18nColLines}
${line}
${/each})
