${# ============================================================================}
${# 枚举模板：生成 Go 枚举（internal/model/{tableName}_enum.go）              }
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成（无枚举时调用方不渲染本模板，"无枚举不生成"）。    }
${#   - Go const 块（成员名 = 枚举名 + 选项帕斯卡）+ var {EnumName}Map           }
${#     （枚举值 → 说明映射，map 键类型按取值类别 int/string）                   }
${#   - 数据来源（tplData.enums，两种来源合并）：                                }
${#     1) 表级 enums 节点（.jsontable 表配置，事实源）                          }
${#     2) globalEnumCols 引用的全局枚举列（tool_global_enum.ac 收集合并；       }
${#        同名 const 跨表只生成一次，Go 同包不允许重复定义）                    }
${# 数据来源（tplData）：                                                        }
${#   hasEnums / enums: [{enumName, columnName, comment, mapKeyType,            }
${#     items: [{key, label, valueStr}]}]                                       }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package model

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
