${# ============================================================================}
${# api.tpl — vue-pure-admin 接口请求文件模板                                     }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                         }
${#   根据 tplData 生成 pure-admin 接口请求文件（.ts），函数集/URL/参数与          }
${#   admin_vue 链 api.tpl 产物完全同构（仅 http 层换 pure-admin 写法）：          }
${#   - 查询（分页）接口 ${queryApi}                                               }
${#   - 新增/更新接口 ${updateApi}（无 id 新增/有 id 更新，/update 承接）          }
${#   - 删除（批量）接口 ${deleteApi}                                              }
${#   - 下拉接口（手动 URL 列的追加函数，request 轻量适配 selectApiStr）           }
${# http 写法（pure-admin 标准）：@/utils/http 的 http.request，                   }
${#   http.request<T>(method, url, { params / data })；                            }
${#   响应统一 { code, msg, data }（与 crud_gin handler 约定一致）                 }
${# URL 路由约定（POST 后缀式，与 admin_vue 链及 NestJS/crud_gin router            }
${#   对齐，api.ts 函数集/URL/参数与 admin_vue api.tpl 产物完全同构）：            }
${#   POST /{apiBase}/selectByIn  分页列表（page/pageSize + 查询参数走 data）      }
${#   POST /{apiBase}/update      新增/更新（无 id 新增、有 id 更新）              }
${#   POST /{apiBase}/delete      删除（{ ids } 支持批量）                         }
${# 数据来源（tplData）：                                                          }
${#   commentTitle/pageName - 注解标题与页面名                                     }
${#   queryApi/updateApi/deleteApi - 接口函数名                                    }
${#   selectUrl/updateUrl/deleteUrl - 后缀式请求地址（admin_data.ac 由             }
${#   meta.dataUrl 首段 apiBase + 请求后缀拼出，如 "/shop/selectByIn"）            }
${#   hasSelectApi/selectApiStr - 下拉接口（admin_data.ac 产出，函数体为           }
${#   request.post({url,data}) 形态，由下方 request 适配统一转发）                 }
${# ----------------------------------------------------------------------------}
//此文件为AutoCode编译器生成，请勿手动修改
import { http } from "@/utils/http";

/** 后端统一响应结构（{ code, msg, data }，与 crud_gin handler 约定一致） */
type ApiResponse<T = any> = { code: number; msg: string; data: T };

// ==================== ${commentTitle}（${pageName}）api ====================
// 查询 ${commentTitle}列表（分页）
export const ${queryApi} = (params: any) => {
  return http.request<ApiResponse>("post", "${selectUrl}", { data: { ...params } });
};

// 新增/更新（无 id 则新增,有 id 则更新）
export const ${updateApi} = (data: any) => {
  return http.request<ApiResponse>("post", "${updateUrl}", { data });
};

// 删除（支持批量）
export const ${deleteApi} = (ids: string[] | number[]) => {
  return http.request<ApiResponse>("post", "${deleteUrl}", { data: { ids } });
}${if hasSelectApi};
// ── 下拉接口（手动 URL 列）────────────────────────────────────────
// request 轻量适配：admin_data.ac 生成的下拉函数体为 request.post({ url, data })
// 形态（element-plus-admin 惯例），此处统一转发到 pure-admin 的 http 封装，
// 两框架共享的 selectApiStr 无需感知 http 差异
const request = {
  post: (o: { url: string; data?: any }) =>
    http.request<ApiResponse>("post", o.url, { data: o.data })
};
${selectApiStr}${else};${/if}
