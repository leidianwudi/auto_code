${# ============================================================================}
${# InUpd 模板：modules/admin/{dataDir}/in_upd_{tableName}_.go     }
${# ----------------------------------------------------------------------------}
${#   - Id 值类型（0=新增/>0=更新）+ 其余列全指针（nil=不改，非 nil 含 0 也写）  }
${#   - ToEntity：新增用（非 nil 字段拷进新实体）                               }
${#   - ApplyTo：更新用（先 TakeSnapshot 再覆盖非 nil 字段，由 repo.Update 做 diff）}
${# 数据来源：pkgName/updClass/enClass/dataAlias/idGoType/idGoName/tableComment  }
${#           updImports/updFieldLines/applyToLines                             }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${updImports}

// ${updClass} ${tableComment}(${tableName}) 保存入参（save 单接口：Id=0 新增，Id>0 更新）
// 除主键外全部指针：nil = 本次不传 / 不改，非 nil = 按它写入（含 0、空串）。
type ${updClass} struct {
${each line in updFieldLines}
${line}
${/each}}

// ToEntity 新增用：把非 nil 字段拷进新实体（未传字段保持零值 / 数据库默认值）
func (in *${updClass}) ToEntity() *${dataAlias}.${enClass} {
	e := &${dataAlias}.${enClass}{}
${each line in applyToLines}
${line}
${/each}
	return e
}

// ApplyTo 更新用：把入参非 nil 的字段覆盖到已查询到的实体（nil 字段保持不变）
// 调用前必须先对实体 TakeSnapshot()，否则 repo.Update 会因缺基线返回 ErrNoSnapshot
func (in *${updClass}) ApplyTo(e *${dataAlias}.${enClass}) {
${each line in applyToLines}
${line}
${/each}}
