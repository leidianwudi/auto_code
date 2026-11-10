${# ============================================================================}
${# 枚举模板：data/{dataDir}/enum_{tableName}_.go                 }
${# ----------------------------------------------------------------------------}
${#   - 无枚举时调用方不渲染本模板（"无枚举不生成"）                           }
${#   - Go const 块（成员名 = 枚举名 + 选项帕斯卡）+ var {EnumName}Map           }
${#   - enumRegPath 非空时：import 公共枚举字典包 + init() 逐个注册本文件枚举   }
${#     （C 端 /api/app/enum/dict 汇总输出；为空则维持旧产物，不引入新依赖）    }
${# 数据来源：pkgName / enumRegPath / enums: [{enumName, columnName, comment,   }
${#           mapKeyType, regName, items: [{key, label, valueStr}]}]            }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${if enumRegPath != ""}import (
	enumreg "${enumRegPath}"
)
${/if}

${each enum in enums}
// ${enum.enumName} ${enum.columnName} 的枚举：${enum.comment}
const (
${each item in enum.items}
	// ${item.label}
	${enum.enumName}${item.key} = ${item.valueStr}
${/each}
)

// ${enum.enumName}Map 枚举值 → 说明映射
var ${enum.enumName}Map = map[${enum.mapKeyType}]string{
${each item in enum.items}
	${item.valueStr}: "${item.label}",
${/each}
}
${/each}

${if enumRegPath != ""}
// init 把本文件全部枚举注册进公共枚举字典（C 端 /api/app/enum/dict 汇总输出）
func init() {
${each enum in enums}
	enumreg.Register("${enum.regName}", ${enum.enumName}Map)
${/each}
}
${/if}
