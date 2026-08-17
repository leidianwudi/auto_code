${# ============================================================================}
${# enum.tpl — TypeScript 枚举文件模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 TypeScript 枚举文件，支持两种数据来源：                               }
${#   1) crud_api_enum 表 — 按列分组生成枚举（字符串值，带单引号）              }
${#   2) tableEnum 配置    — 查询源表整表生成枚举（数值型，不带引号）           }
${#   生成内容：                                                                 }
${#   - import 引用实体类                                                        }
${#   - 枚举定义（export enum）                                                  }
${#   - 枚举项注释                                                               }
${# 数据来源（tplData）：                                                        }
${#   entityClass     - 实体类名（如 EnUser）                                    }
${#   entityClassFile - 实体文件名（如 en_user）                                 }
${#   tableName       - 表名                                                      }
${#   hasEnums        - 是否有枚举定义                                            }
${#   enums           - 枚举定义数组                                             }
${#     [{enumName, columnName, enumDesc, columnComment, isTableEnum,            }
${#       recordsName, methodName,                                               }
${#       items: [{key, value, valueStr, label}]}]                               }
${#     enumDesc   - JSDoc 描述行（已预计算，含表名和列名大写）                 }
${#     valueStr   - 枚举值字面量（crud_api_enum 带引号，tableEnum 不带）       }
${# ============================================================================}
${if hasEnums}
// 此代码为AutoCode框架生成，请勿手动修改
import { ${entityClass} } from './${entityClassFile}'

${each enum in enums}
/**
 * ${enum.enumDesc}
 * @see ${entityClass}#${enum.columnName} ${enum.columnComment}
 */
export enum ${enum.enumName} {
${each item in enum.items}  /* ${item.label} */
  ${item.key} = ${item.valueStr}${if !item_last},${/if}
${/each}}

${/each}
${/if}