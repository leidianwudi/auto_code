${# ============================================================================}
${# 枚举模板：data/{dataDir}/enum_{tableName}_.go                 }
${# ----------------------------------------------------------------------------}
${#   - 无枚举时调用方不渲染本模板（"无枚举不生成"）                           }
${#   - Go const 块（成员名 = 枚举名 + 选项帕斯卡）+ var {EnumName}Map           }
${# 数据来源：pkgName / enums: [{enumName, columnName, comment, mapKeyType,      }
${#           items: [{key, label, valueStr}]}]                                 }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

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
