${# ============================================================================}
${# source.tpl — 数据源 api 请求文件模板                                         }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonsource 数据源文件生成接口请求文件（.ts），供各 vue 界面下拉框共用：}
${#   - 动态数据源：生成 http 请求函数（request.get / request.post）             }
${#   - 静态数据源：生成静态选项函数，返回结构与动态一致（{ data: { list: [...] } }），}
${#     使 write.vue 的 optionApi（res.data?.list）无需区分数据源类型           }
${# 数据来源（tplData，由 admin_data.ac 的 buildSourceTplData 加工）：           }
${#   sourceName - .jsonsource 文件名（不含扩展名），import 路径使用            }
${#   sources    - 数据源数组                                                   }
${#     [{isStatic, isDynamic, remark, funcName, methodLower, url,              }
${#      hasItems, items: [{labelEsc, valueEsc}]}]                              }
${# 说明：                                                                       }
${#   funcName 由 urlToFuncName 统一推导（动态=URL 全路径驼峰，静态=文件名驼峰+序号），}
${#   jsonvue 引用数据源的列（selectSourceFile/selectSourceId）生成的           }
${#   write.vue 通过同名函数调用本文件接口，两侧保持一致                         }
${# ============================================================================}
//此文件为AutoCode编译器生成，请勿手动修改
import request from '@/axios';

// ==================== 数据源（${sourceName}）api ====================
${each src in sources}${if src.isDynamic}
//查询${src.remark}
export const ${src.funcName} = () => {
  return request.${src.methodLower}({ url: '${src.url}' })
};
${else}
//${src.remark}（静态选项）
export const ${src.funcName} = () => {
  return Promise.resolve({
    data: {
      list: [${if src.hasItems}
${each it in src.items}        { label: '${it.labelEsc}', value: ${if it.valueIsNum}${it.valueEsc}${else}'${it.valueEsc}'${/if} },${/each}${/if}
      ]
    }
  });
};
${/if}${/each}
