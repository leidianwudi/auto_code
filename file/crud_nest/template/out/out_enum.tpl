${# ============================================================================}
${# out_enum.tpl — 枚举输出记录模板                                            }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成枚举的输出记录文件，包含：                                            }
${#   - import 引用枚举定义                                                     }
${#   - 每个 records 数组（StringKeyOutEnum[]）                                 }
${# 数据来源（tplData）：                                                        }
${#   tableName  - 表名（如 user）                                              }
${#   enumNames  - 枚举名列表（如 "EnumUserType, EnumUserVipStatus"）           }
${#   hasEnums   - 是否有枚举定义                                               }
${#   enums      - 枚举定义数组                                                 }
${#     [{enumName, recordsName, columnName, items: [{key, value, label}]}]     }
${# ============================================================================}
${if hasEnums}
// 此代码为AutoCode框架生成，请勿手动修改
import {
  ${enumNames}
} from "../entities/enum_${tableName}";
import { NumberKeyOutEnum, StringKeyOutEnum } from "@/common/tool/out_enum_record";
${each enum in enums}

/**
 * ${tableName}.${enum.columnName} 的枚举输出列表
 */
export const ${enum.recordsName}: StringKeyOutEnum[] = [
${each item in enum.items}  { key: ${enum.enumName}.${item.key}, value: "${item.label}" },
${/each}];
${/each}
${/if}