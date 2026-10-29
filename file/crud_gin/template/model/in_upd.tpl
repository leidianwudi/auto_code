${# ============================================================================}
${# InUpd 模板：modules/admin/{dataDir}/in_upd_{tableName}_.go     }
${# ----------------------------------------------------------------------------}
${#   - Id 值类型（0=新增/>0=更新）+ 其余列全指针（nil=不改，非 nil 含 0 也写）  }
${#   - ToEntity 用 common.CopyNonNil；ToUpdates 用 common.BuildUpdates 成对    }
${# 数据来源：pkgName/updClass/enClass/dataAlias/idGoType/idGoName/tableComment  }
${#           updImports/updFieldLines/toEntityLines/toUpdatesLines             }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${updImports}

// ${updClass} ${tableComment}(${tableName}) 保存入参（save 单接口：Id=0 新增，Id>0 更新）
// 除主键外全部指针：nil = 本次不传 / 不参与，非 nil = 按它写入（含 0、空串）。
type ${updClass} struct {
${each line in updFieldLines}
${line}
${/each}}

// ToEntity 新增用：把非 nil 指针解包进实体，nil 留零值、由数据库默认值兜底
func (in *${updClass}) ToEntity() *${dataAlias}.${enClass} {
	e := &${dataAlias}.${enClass}{}
${each line in toEntityLines}
${line}
${/each}
	return e
}

// ToUpdates 更新用：nil 字段不写；key 一律用 data 层列名常量
func (in *${updClass}) ToUpdates() map[string]any {
	return common.BuildUpdates(
${each line in toUpdatesLines}
${line}
${/each}
	)
}
