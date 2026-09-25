${# ============================================================================}
${# write.tpl — Vue3 后台管理编辑页模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonvue 配置生成 Vue3 编辑页（write.vue），包含：                    }
${#   - 表单字段定义（formSchema）                                               }
${#   - 不同输入样式（文本/整型/浮点/日期/下拉/多行文本/开关）                   }
${#   - 图片上传区（引用 .jsonupload 上传预设的图片字段：axios 上传 + 预览）      }
${#   - 表单验证规则                                                             }
${# 数据来源（tplData）：                                                        }
${#   columns       - 列配置数组                                                 }
${#     [{dataName, editName, isSwitch, isSelect, isTextArea, isText,            }
${#      isInt, isFloat, isDate, selectUrl, selectValueField, selectLabelField,  }
${#      selectApiName, selectErrorText, hasSourceRef（引用 .jsonsource 时       }
${#      函数来自 source api 文件，由 sourceImportLines 导入）,                   }
${#      hasBoolApi/boolApiName（布尔开关引用静态数据源时 optionApi 使用）,       }
${#      hasUpload（引用 .jsonupload 上传预设时生成上传组件）,                    }
${#      uploadAction/uploadFileField/uploadLimit/uploadMulti/                   }
${#      uploadParamsStr（fd.append 片段）/uploadResChain（响应提取可选链）,      }
${#      placeholder, maxlength, minValue, maxValue, precision, dateFormat,      }
${#      textareaRows, required, formSpan, editComponent,                        }
${#      hasDefaultValue, defaultValue}]                                         }
${# ============================================================================}
<script setup lang="tsx">
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== ${commentTitle}（${pageName}）编辑界面 ====================
import { Form, FormSchema } from '@/components/form';
import { useForm } from '@/hooks/web/use_form';
import { PropType, reactive, ref, computed${if hasWatch}, watch${/if}${if hasDefaultValues}, nextTick${/if} } from 'vue';
import { useValidator } from '@/hooks/web/use_validator';
import { uiWriteLogic } from '@/utils/ui_write_logic';
${if hasUploadFields}import { ElUpload, ElImage } from 'element-plus';
import request from '@/axios';
${/if}${if hasSelectApi}import { ${selectApiImports} } from '@/api/${apiModule}/${pageName}';
${/if}${if hasSourceImports}${sourceImportLines}
${/if}${if hasSelectFields}import { onceSelect } from '@/utils/once_select';
${/if}import { applyFormMode } from '@/utils/form_mode';

const { required } = useValidator();

const props = defineProps({
  currentRow: {
    type: Object as PropType<any>,
    default: () => null
  },
  actionType: {
    type: String,
    default: ''
  }
});

const isDetail = computed(() => props.actionType === 'detail');

// 表单字段定义
const formSchema = ref<FormSchema[]>([
${each col in columns}${if col.editVisible}
${if col.isBooleanSwitch}
${if col.hasBoolApi}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      props: {
        label: 'label',
        value: 'value'
      }
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if},
    optionApi: onceSelect('${col.dataName}', () => Promise.resolve(${col.boolApiName}()).then((res: any) => res.data?.list))
  },
${else}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
        { label: '${col.switchInactiveText}', value: 0 },
        { label: '${col.switchActiveText}', value: 1 }
      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${/if}
${else if col.isTagSwitch}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
${each t in col.tagItems}
        { label: '${t.textEsc}', value: '${t.valueEsc}' },
${/each}      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isSelect}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      props: {
        label: '${col.selectLabelField}',
        value: '${col.selectValueField}'
      }
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if},
    optionApi: onceSelect('${col.dataName}', () => Promise.resolve(${col.selectApiName}()).then((res: any) => res.data?.list))
  },
${else if col.isTagEdit}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
${each t in col.tagItems}
        { label: '${t.textEsc}', value: '${t.valueEsc}' },
${/each}      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isBooleanEdit}
${if col.hasBoolApi}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      props: {
        label: 'label',
        value: 'value'
      }
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if},
    optionApi: onceSelect('${col.dataName}', () => Promise.resolve(${col.boolApiName}()).then((res: any) => res.data?.list))
  },
${else}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
        { label: '${col.boolFalseText}', value: '0' },
        { label: '${col.boolTrueText}', value: '1' }
      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${/if}
${else if col.isImageEdit}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Input'${if col.hasPlaceholder},
    componentProps: {
      placeholder: '${col.placeholder}'
    }${/if}${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isMoney}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'InputNumber',
    componentProps: {
      precision: ${col.precision}
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isTextArea}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Input',
    componentProps: {
      type: 'textarea',
      rows: ${col.textareaRows}
${if col.hasPlaceholder},      placeholder: '${col.placeholder}'${/if}
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isDate}
${if col.isTime}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'TimePicker',
    componentProps: {
      format: 'HH:mm:ss',
      valueFormat: 'HH:mm:ss'
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else}  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'DatePicker',
    componentProps: {
      type: '${col.dateFormat}'
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${/if}
${else if col.isInt}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'InputNumber',
    componentProps: {
${if col.hasMinValue}      min: ${col.minValue},
${/if}${if col.hasMaxValue}      max: ${col.maxValue},
${/if}    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isFloat}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'InputNumber',
    componentProps: {
      precision: ${col.precision}
${if col.hasMinValue},      min: ${col.minValue}${/if}
${if col.hasMaxValue},      max: ${col.maxValue}${/if}
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Input',
    componentProps: {
${if col.hasPlaceholder}      placeholder: '${col.placeholder}',
${/if}${if col.hasMaxlength}      maxlength: ${col.maxlength},
${/if}    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${/if}
${/if}
${/each}]);

// 详情模式下将所有组件设为只读；同时处理隐藏字段和只读字段
const formSchemaComputed = computed(() =>
  applyFormMode(formSchema.value, isDetail.value, {
${if hasHiddenFields}    hiddenFields: [${hiddenFieldsStr}],
${/if}${if hasDisabledFields}    disabledFields: [${disabledFieldsStr}],
${/if}  })
);

// 表单验证规则（根据配置的 required 字段生成）
const rules = reactive({
${each col in columns}${if col.required}${if col.editVisible}  ${col.dataName}: [required()],
${/if}${/if}${/each}});

const { formRegister, formMethods } = useForm();
${if hasUploadFields}
// ==================== 图片上传（引用 .jsonupload 上传预设）====================
${each col in columns}${if col.hasUpload}${if col.editVisible}
// ── ${col.editName}${if col.hasUploadLabel}（${col.uploadLabel}）${/if}：上传后路径写入表单字段，随保存接口一起提交
const ${col.dataName}FileList = reactive<any[]>([]);
watch(() => props.currentRow, (row: any) => {
  const v = row?.['${col.dataName}'];
  ${col.dataName}FileList.length = 0;
  const list: any[] = v == null || v === '' ? [] : (Array.isArray(v) ? v : [v]);
  list.forEach((u: any) => { ${col.dataName}FileList.push({ name: String(u), url: String(u) }); });
}, { immediate: true });

const upload${col.dataNamePascal} = async (opt: any) => {
  const fd = new FormData();
  fd.append('${col.uploadFileField}', opt.file);
${if col.uploadMulti}${else}  // 单图换图：带上旧图地址，后端删除旧文件避免垃圾图片残留（新增记录无旧图不传）
  const oldImgUrl = props.currentRow?.['${col.dataName}'];
  if (oldImgUrl) fd.append('oldImgUrl', String(oldImgUrl));
${/if}${col.uploadParamsStr}  try {
    const res: any = await request.post({ url: '${col.uploadAction}', data: fd });
    const url: string = ${col.uploadResChain} ?? '';
    if (!url) throw new Error('上传响应中未找到图片地址');
${if col.uploadMulti}    ${col.dataName}FileList.push({ name: url, url });
    formMethods.setValues({ ${col.dataName}: ${col.dataName}FileList.map((f: any) => f.url) });
${else}    ${col.dataName}FileList.splice(0, ${col.dataName}FileList.length, { name: url, url });
    formMethods.setValues({ ${col.dataName}: url });
${/if}  } catch (e: any) {
    console.error('${col.dataName} 上传失败:', e?.message ?? e);
  }
};

const remove${col.dataNamePascal} = (idx: number) => {
  ${col.dataName}FileList.splice(idx, 1);
  formMethods.setValues({ ${col.dataName}: ${col.uploadRemoveValueExpr} });
};
${/if}${/if}${/each}
${/if}

// 使用 useWriteLogic 钩子，仅使用其 submit 方法
const { submit } = uiWriteLogic(
  computed(() => props.currentRow),
  formSchemaComputed,
  formMethods
);
${if hasDefaultValues}
// 新增记录时的默认值
const defaultValues: Record<string, any> = {
${each col in columns}${if col.hasDefaultValue}  ${col.dataName}: ${col.defaultValueLiteral},
${/if}${/each}};

// 新增时设置默认值（在表单重置后通过 nextTick 注入）
watch(() => props.actionType, (newVal) => {
  if (newVal === 'add') {
    nextTick(() => {
      formMethods.setValues(defaultValues);
    });
  }
});
${/if}
// 向父组件暴露接口
defineExpose({
  submit
});
</script>

<template>
  <Form :rules="isDetail ? {} : rules" @register="formRegister" :schema="formSchemaComputed" />
${if hasUploadFields}
  <!-- 图片上传区（引用 .jsonupload 上传预设的图片字段） -->
  <div v-if="!isDetail" class="upload-section">
${each col in columns}${if col.hasUpload}${if col.editVisible}    <div class="upload-item">
      <div class="upload-label">${col.editName}</div>
      <div class="upload-cards">
        <div v-for="(f, idx) in ${col.dataName}FileList" :key="f.url" class="upload-card">
          <ElImage :src="f.url" fit="cover" class="upload-thumb" :preview-src-list="[f.url]" />
          <div class="upload-card-mask">
            <span class="upload-card-del" @click="remove${col.dataNamePascal}(idx)">×</span>
          </div>
        </div>
        <ElUpload
          action="#"
          accept="image/*"
          :show-file-list="false"
          :multiple="${col.uploadMulti}"
          :limit="${col.uploadLimit}"
          :http-request="upload${col.dataNamePascal}"
          class="upload-trigger"
        >
          <span class="upload-plus">+</span>
        </ElUpload>
      </div>
    </div>
${/if}${/if}${/each}  </div>
${/if}
</template>

<style scoped>
.upload-section { padding: 0 12px; }
.upload-item { margin-bottom: 14px; }
.upload-label { font-size: 14px; color: #606266; margin-bottom: 8px; }
.upload-cards { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.upload-card { position: relative; width: 80px; height: 80px; border: 1px solid #dcdfe6; border-radius: 6px; overflow: hidden; }
.upload-thumb { width: 100%; height: 100%; display: block; }
.upload-card-mask { position: absolute; top: 0; right: 0; left: 0; bottom: 0; display: none; align-items: center; justify-content: center; background: rgba(0, 0, 0, 0.4); }
.upload-card:hover .upload-card-mask { display: flex; }
.upload-card-del { color: #fff; font-size: 20px; cursor: pointer; line-height: 1; }
.upload-trigger :deep(.el-upload--picture-card) { width: 80px; height: 80px; display: flex; align-items: center; justify-content: center; }
.upload-plus { font-size: 24px; color: #909399; }
</style>
