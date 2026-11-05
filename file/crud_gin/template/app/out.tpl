${# ============================================================================}
${# C 端出参白名单模板：modules/app/{dataDir}/out_{tableName}_.go             }
${# ----------------------------------------------------------------------------}
${#   - 仅 appOutCols 非空时生成；字段只含白名单列                             }
${#   - 实体 → 出参转换由 ToOut<表> 完成，service 返回 Out<表>                  }
${#   - C 方案：实体普通列已是值类型，出参直接赋值，无需解引用兜底；           }
${#     仅数据库可空列（*T）出参同步为指针，JSON 输出 null（真实 NULL 语义）    }
${# 数据来源：pkgName/tableClass/tableComment/enClass/dataAlias/dataPath       }
${#           appOutClass/appOutFieldLines/appOutAssignLines/appOutImports     }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${appOutImports}

// ${appOutClass} ${tableComment}(${tableName}) C 端出参白名单（普通列值类型、可空列指针）
type ${appOutClass} struct {
${each line in appOutFieldLines}
${line}
${/each}}

// To${appOutClass} 实体 → C 端出参（字段直接赋值，不做 nil 兜底）
func To${appOutClass}(e *${dataAlias}.${enClass}) *${appOutClass} {
	if e == nil {
		return nil
	}
	return &${appOutClass}{
${each line in appOutAssignLines}
${line}
${/each}
	}
}
