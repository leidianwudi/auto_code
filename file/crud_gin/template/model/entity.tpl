${# ============================================================================}
${# 实体模板：data/{dataDir}/en_{tableName}_.go                    }
${# ----------------------------------------------------------------------------}
${#   - C 方案：普通列值类型 + 仅数据库可空列指针 + 快照 diff 更新             }
${#   - 结构体 / 常量行由 main_api.ac 预渲染（go fmt 列对齐），模板逐行输出      }
${#   - TakeSnapshot() 记基线；changedColumns() 覆盖除主键外全部列              }
${# 数据来源：pkgName/dataPkgComment/enClass/tableComment/tableName/enImports   }
${#           enFieldLines/colLines/hasAutoIncPk/hasNullableField/idGoName       }
${#           enSnapshotLines/enChangedLines/enChangedCount                    }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${dataPkgComment}
package ${pkgName}

${if enImports}
${enImports}

${/if}// ${enClass} ${tableComment}(${tableName}) 实体
// 普通列一律值类型（整数统一 int64，覆盖数据库 int/int unsigned）；
// 仅数据库允许 NULL 的列为指针：nil = SQL NULL，非 nil = 按它写入（含 0、空串）。
// 更新走快照 diff：改字段前先 TakeSnapshot()，Update 只写与基线不同的列。
type ${enClass} struct {
${each line in enFieldLines}
${line}
${/each}

	orig *${enClass} // 原值快照，nil = 未拍快照
}

// TableName 表名
func (${enClass}) TableName() string { return "${tableName}" }

// TakeSnapshot 把当前值记为基线，之后 Update 只写与基线不同的列。
// 必须在修改字段之前调用。
func (e *${enClass}) TakeSnapshot() {
	s := *e
${if hasNullableField}	// 可空列是指针：必须深拷贝（后续 *e.X = v 就地修改，共享指针会同时改到基线而漏检）
${each line in enSnapshotLines}
${line}
${/each}
${/if}	s.orig = nil
	e.orig = &s
}

// changedColumns 返回与快照相比发生变化的列（key 为列名常量）
// 覆盖除主键外的全部列；orig 为 nil（未拍快照）时返回 nil
func (e *${enClass}) changedColumns() map[string]any {
	o := e.orig
	if o == nil {
		return nil
	}
	m := make(map[string]any, ${enChangedCount})
${each line in enChangedLines}
${line}
${/each}
	return m
}

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

${/if}${joinSpecBlock}

// 列名常量
const (
${each line in colLines}
${line}
${/each})

// 数据层错误常量（调用方可用 errors.Is 判断）
var (
	// ErrNilEntity 实体为 nil
	ErrNilEntity = errors.New("${tableName}: 实体为 nil")
	// ErrNoSnapshot 更新前未拍快照
	ErrNoSnapshot = errors.New("${tableName}: 更新前必须先调用 TakeSnapshot")
	// ErrInsertWithId 新增时实体已带自增主键
	ErrInsertWithId = errors.New("${tableName}: 新增时实体主键必须为 0（由数据库生成）")
	// ErrInvalidId 主键参数不合法
	ErrInvalidId = errors.New("${tableName}: 主键非法，必须大于 0")
)
