${# ============================================================================}
${# api.tpl — Vue3 后台管理接口请求文件模板                                       }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonvue 配置生成接口请求文件（.ts），包含：                            }
${#   - 查询（分页）接口 ${queryApi}                                              }
${#   - 新增/更新接口 ${updateApi}                                                 }
${#   - 删除（批量）接口 ${deleteApi}                                              }
${# 数据来源（tplData）：                                                          }
${#   commentTitle - 界面说明（注解标题），例如 "系统任务"                        }
${#   pageName     - jsonvue 文件名，例如 "system_task"                            }
${#   queryApi     - 查询接口名（meta 配置，如 systemtaskSelect）                   }
${#   updateApi    - 更新接口名                                                     }
${#   deleteApi    - 删除接口名                                                     }
${#   selectUrl    - 查询请求地址，如 "/systemtask/selectByIn"                    }
${#   updateUrl    - 更新请求地址，如 "/systemtask/update"                        }
${#   deleteUrl    - 删除请求地址，如 "/systemtask/delete"                        }
${# ============================================================================}
//此文件为AutoCode编译器生成，请勿手动修改
import request from '@/axios';

// ==================== ${commentTitle}（${pageName}）api ====================
// 查询 ${commentTitle}列表（分页）
export const ${queryApi} = (params: any) => {
  return request.post({
    url: '${selectUrl}',
    data: { ...params }
  });
};

// 新增/更新（无 id 则新增,有 id 则更新）
export const ${updateApi} = (data: any) => {
  return request.post({ url: '${updateUrl}', data });
};

// 删除（支持批量）
export const ${deleteApi} = (ids: string[] | number[]) => {
  return request.post({ url: '${deleteUrl}', data: { ids } });
};