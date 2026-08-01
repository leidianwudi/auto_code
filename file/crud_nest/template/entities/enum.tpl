${# ============================================================================}
${# enum.tpl — TypeScript 枚举文件模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 crud_api_enum 表的数据生成 TypeScript 枚举文件，包含：                }
${#   - import 引用实体类                                                        }
${#   - 枚举定义（export enum）                                                  }
${#   - 枚举项注释                                                               }
${# 数据来源（tplData）：                                                        }
${#   entityClass     - 实体类名（如 EnUser）                                    }
${#   entityClassFile - 实体文件名（如 en_user）                                 }
${#   tableDesc       - 表说明文字                                                }
${#   hasEnums        - 是否有枚举定义                                            }
${#   enums           - 枚举定义数组                                             }
${#     [{enumName, columnName, columnComment, items: [{value, label}]}]          }
${# ============================================================================}
${if hasEnums}
// 此代码为AutoCode框架生成，请勿手动修改
import { ${entityClass} } from './${entityClassFile}'

${each enum in enums}
/**
 * ${tableName} 表的 ${enum.columnName} 属性枚举
 * @see ${entityClass}#${enum.columnName} ${enum.columnComment}
 */
export enum ${enum.enumName} {
${each item in enum.items}  /* ${item.label} */
  ${item.key} = '${item.value}',
${/each}}

${/each}
${/if}