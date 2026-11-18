${# ============================================================================}
${# C 端出参白名单模板：modules/app/{dataDir}/out_{tableName}_.go             }
${# ----------------------------------------------------------------------------}
${#   - 仅 appOutCols 非空时生成；字段只含白名单列                              }
${#     （主表列 / 联表输出列 / i18n 平铺字段，按白名单配置顺序）               }
${#   - ToOut<表>：实体源转换（详情接口 SelectById 用；联表列零值）             }
${#   - ToOut<表>View：查询视图源转换（列表接口 SelectByIn 用；联表列取         }
${#     EnXxxView 平铺字段，主表/i18n 字段经匿名嵌入提升直接可达）——            }
${#     无联表时不生成该函数（列表/详情共用实体源 ToOut<表>）                    }
${#   - C 方案：实体普通列已是值类型，出参直接赋值，无需解引用兜底；           }
${#     仅数据库可空列（*T）出参同步为指针，JSON 输出 null（真实 NULL 语义）    }
${# 数据来源：pkgName/tableClass/tableComment/enClass/dataAlias/dataPath       }
${#           appOutClass/appOutFieldLines/appOutAssignLines                  }
${#           appOutEntityAssignLines/appOutToViewFn/appOutRecvView/appOutImports}
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${appOutImports}

// ${appOutClass} ${tableComment}(${tableName}) C 端出参白名单（普通列值类型、可空列指针）
type ${appOutClass} struct {
${each line in appOutFieldLines}
${line}
${/each}}

// To${appOutClass} 实体 → C 端出参（字段直接赋值，不做 nil 兜底；联表列零值）
func To${appOutClass}(e *${dataAlias}.${enClass}) *${appOutClass} {
	if e == nil {
		return nil
	}
	return &${appOutClass}{
${each line in appOutEntityAssignLines}
${line}
${/each}
	}
}
${if hasJoin}
// To${appOutClass}View 查询视图 → C 端出参（含联表列；列表接口 SelectByIn 用）
func To${appOutClass}View(e *${appOutRecvView}) *${appOutClass} {
	if e == nil {
		return nil
	}
	return &${appOutClass}{
${each line in appOutAssignLines}
${line}
${/each}
	}
}
${/if}
