${# ============================================================================}
${# global_enum_.tpl — 全局枚举文件模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonglobalenum 全局枚举配置生成 TypeScript 公共枚举文件，           }
${#   全部枚举聚合一份（{basePath}/common/enum/global_enum_.ts），             }
${#   供各业务模块 import 引用，替代每表一份的枚举副本。                        }
${# 数据来源（tplData，由 tool_global_enum.ac 的 buildGlobalEnumTplData 加工）： }
${#   hasGlobalEnums - 是否有全局枚举（渲染前置条件）                          }
${#   globalEnums   - 全局枚举数组                                             }
${#     [{name, enumName, remark, items: [{key, label, valueStr}]}]            }
${#       enumName  - 枚举名（Enum + 名称帕斯卡，如 is_enable → EnumIsEnable）  }
${#       remark    - 枚举说明（JSDoc 描述行）                                  }
${#       key       - 枚举成员名（选项 key 转帕斯卡，如 enable → Enable）        }
${#       label     - 枚举项注释文字                                            }
${#       valueStr  - 枚举值字面量（valueType=number 不带引号，字符串带单引号）  }
${# 说明：                                                                       }
${#   key 成员名在后端（枚举定义）与前端（jsonsource 显示）两侧职责不同：        }
${#   后端需要合法 TS 标识符，前端只需要 label/value。                          }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

${each enum in globalEnums}
/**
 * ${enum.remark}
 */
export enum ${enum.enumName} {
${each item in enum.items}
  /* ${item.label} */
  ${item.key} = ${item.valueStr}${if !item_last},${/if}
${/each}}

${/each}
