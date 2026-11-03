${# ============================================================================}
${# C 端出参白名单模板：modules/app/{dataDir}/out_{tableName}_.go             }
${# ----------------------------------------------------------------------------}
${#   - 仅 appOutCols 非空时生成；字段只含白名单列                             }
${#   - 实体 → 出参转换由 ToOut<表> 完成，service 返回 Out<表>                  }
${# 数据来源：pkgName/tableClass/tableComment/enClass/dataAlias/dataPath       }
${#           appOutClass/appOutFields                                        }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

import (
	${dataAlias} "${dataPath}"
)

// ${appOutClass} ${tableComment}(${tableName}) C 端出参白名单
type ${appOutClass} struct {
${each line in appOutFieldLines}
${line}
${/each}}

// To${appOutClass} 实体 → C 端出参
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
