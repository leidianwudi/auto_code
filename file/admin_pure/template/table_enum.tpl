${# ============================================================================}
${# table_enum.tpl — 表级枚举字典（跨页面共享，vue-pure-admin）                   }
${# ----------------------------------------------------------------------------}
${# 作用：把本次生成的全部表级枚举汇总成一个共享映射文件，输出到              }
${#   {basePath}/src/api/source/table_enum.ts（与 global_enum.ts 同目录），      }
${#   各页面 import { tableEnumMap } 后按 '表名.列名' 查文字/颜色，              }
${#   避免每个页面各自定义一份 tagMaps、多页面无法复用同一枚举。               }
${# 键约定：表名.列名（如 user.is_real_name），与后端 Go 侧 enumreg 注册名一致；  }
${#   联表虚拟列的键取「目标表名.列名」（如 user.is_real_name），而非输出别名。  }
${# 数据来源（tplData）：tableEnums: [{keyEsc, mapStr}]                        }
${#   mapStr 形如 {'0': { text: '未实名', color: 'primary' }, ...}              }
${# ============================================================================}
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== 表级枚举字典（跨页面共享） ====================
// 键 = 表名.列名（与后端 enumreg 注册名一致）；多页面共用，改枚举只需重新生成
export const tableEnumMap: Record<string, Record<string, { text: string; color: string }>> = {
${each en in tableEnums}  '${en.keyEsc}': ${en.mapStr},
${/each}};
