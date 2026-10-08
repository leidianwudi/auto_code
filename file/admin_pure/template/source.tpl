${# ============================================================================}
${# source.tpl — 数据源 api 请求文件模板（vue-pure-admin）                        }
${# ----------------------------------------------------------------------------}
${# 作用：与 admin_vue/source.tpl 逻辑等价，仅 http 调用改写为 pure-admin 形态：   }
${#   - 动态数据源：生成 http.request 请求函数（get/post）                         }
${#   - 静态数据源：生成静态选项函数，返回结构与动态一致（{ data: { list: [...] } }），}
${#     使下拉消费方（res?.data?.list）无需区分数据源类型                          }
${# 数据来源（tplData，由 admin_data.ac 的 buildSourceTplData 加工）：             }
${#   sourceName - .jsonsource 文件名（不含扩展名），import 路径使用               }
${#   sources    - 数据源数组                                                      }
${#     [{isStatic, isDynamic, remark, funcName, methodLower, url,                 }
${#      hasItems, items: [{labelEsc, valueLiteral}], hasTags, tagsList}]          }
${#        valueLiteral 由 makeValueLiteral 生成：数字 value → 数字字面量          }
${#        （如 1），其余 → 带引号字符串字面量（如 'abc'），保持提交类型正确       }
${# 说明：                                                                        }
${#   funcName 由 admin_data.ac 的 urlToFuncName 统一推导（动态=URL 全路径驼峰，    }
${#   静态=文件名驼峰+Static+url 名），jsonvue 引用数据源的列（write.vue）通过     }
${#   同名函数调用本文件接口，两侧保持一致                                         }
${# ============================================================================}
//此文件为AutoCode编译器生成，请勿手动修改
import { http } from "@/utils/http";

/** 后端统一响应结构（{ code, msg, data }，与 crud_gin handler 约定一致） */
type ApiResponse<T = any> = { code: number; msg: string; data: T };

// ==================== 数据源（${sourceName}）api ====================
${each src in sources}${if src.isDynamic}
//查询${src.remark}
export const ${src.funcName} = () => {
  return http.request<ApiResponse>("${src.methodLower}", "${src.url}")${if src.hasTags}.then((res: any) => {
    res.tags = {
${each tag in src.tagsList}      '${tag.value}': '${tag.color}',${/each}
    };
    return res;
  })${/if};
};
${else}
//${src.remark}（静态选项，同步返回：组件 setup 可直接读取，无需异步/兜底）
export const ${src.funcName} = () => {
  return {
    data: {
      list: [${if src.hasItems}
${each it in src.items}        { label: '${it.labelEsc}', value: ${it.valueLiteral} },${/each}${/if}
      ]
    }${if src.hasTags},
    tags: {
${each tag in src.tagsList}      '${tag.value}': '${tag.color}',${/each}
    }${/if}
  };
};
${/if}${/each}
