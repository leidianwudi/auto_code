${# ============================================================================}
${# upload.tpl — 上传预设 api 请求文件模板                                        }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonupload 上传预设文件生成共享上传函数（.ts），供各 vue 界面上传复用：}
${#   每条预设一个函数：FormData 组装（附加参数/旧图地址/文件字段）→ POST →      }
${#   按 responsePath 提取图片地址并返回（返回空串 = 上传失败，调用方据此提示）  }
${# 数据来源（tplData，由 admin_data.ac 的 buildUploadTplData 加工）：           }
${#   uploadName - .jsonupload 文件名（不含扩展名），import 路径使用            }
${#   uploads    - 预设数组                                                     }
${#     [{funcName, remark, url, fileField, params: [{name, value}], resChain}] }
${#   params 统一按字符串字面量生成（后端隐式转换会转回数字）                    }
${# ============================================================================}
//此文件为AutoCode编译器生成，请勿手动修改
import request from '@/axios';

// ==================== 上传预设（${uploadName}）api ====================
${each up in uploads}
// ${up.remark}
export const ${up.funcName} = async (file: File, oldImgUrl?: string): Promise<string> => {
  const fd = new FormData();
  // 附加 form 参数（如图片类型 type，决定后端存储子目录）
${each p in up.params}  fd.append('${p.name}', '${p.value}');
${/each}  // 换图时带上旧图地址，后端删除旧文件避免垃圾图片残留
  if (oldImgUrl) fd.append('oldImgUrl', String(oldImgUrl));
  fd.append('${up.fileField}', file);
  try {
    const res: any = await request.post({ url: '${up.url}', data: fd });
    return ${up.resChain} ?? '';
  } catch (e: any) {
    console.error('上传失败:', e?.message ?? e);
    return '';
  }
};
${/each}
