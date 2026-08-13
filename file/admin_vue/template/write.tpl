${# ============================================================================}
${# write.tpl — Vue3 后台管理编辑页模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonvue 配置生成 Vue3 编辑页（write.vue），包含：                    }
${#   - 表单字段定义（formSchema）                                               }
${#   - 不同输入样式（文本/整型/浮点/日期/下拉/多行文本/开关）                   }
${#   - 表单验证规则                                                             }
${# 数据来源（tplData）：                                                        }
${#   columns       - 列配置数组                                                 }
${#     [{dataName, editName, isSwitch, isSelect, isTextArea, isText,           }
${#      isInt, isFloat, isDate, selectUrl, selectValueField, selectLabelField,  }
${#      placeholder, maxlength, minValue, maxValue, precision, dateFormat,      }
${#      textareaRows, required, formSpan, editComponent,                        }
${#      hasDefaultValue, defaultValue}]                                         }
${# ============================================================================}
<script setup lang="tsx">
//此代码为AutoCode框架生成，请勿手动修改
import { Form, FormSchema } from '@/components/form';
import { useForm } from '@/hooks/web/use_form';
import { PropType, reactive, ref, computed${if hasDefaultValues}, watch, nextTick${/if} } from 'vue';
import { useValidator } from '@/hooks/web/use_validator';
import { uiWriteLogic } from '@/utils/ui_write_logic';

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
  {
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
${else if col.isTagSwitch}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
${each t in col.tagItems}        { label: '${t.textEsc}', value: '${t.valueEsc}' },
${/each}      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isSelect}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'ApiSelect',
    componentProps: {
      url: '${col.selectUrl}',
      valueField: '${col.selectValueField}',
      labelField: '${col.selectLabelField}'
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isTagEdit}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
${each t in col.tagItems}        { label: '${t.textEsc}', value: '${t.valueEsc}' },
${/each}      ]
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
${else if col.isBooleanEdit}
  {
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
const formSchemaComputed = computed(() => {
${if hasHiddenFields}  const hiddenFields = [${hiddenFieldsStr}];
${/if}${if hasDisabledFields}  const disabledFields = [${disabledFieldsStr}];
${/if}  return formSchema.value.map((item: any) => {
    const result = { ...item };
${if hasHiddenFields}    if (hiddenFields.includes(item.field)) {
      result.ifShow = false;
    }
${/if}    if (isDetail.value) {
      result.componentProps = { ...(item.componentProps || {}), disabled: true };
    }${if hasDisabledFields} else if (disabledFields.includes(item.field)) {
      result.componentProps = { ...(item.componentProps || {}), disabled: true };
    }${/if}
    return result;
  });
});

// 表单验证规则（根据配置的 required 字段生成）
const rules = reactive({
${each col in columns}${if col.required}${if col.editVisible}  ${col.dataName}: [required()],
${/if}${/if}${/each}});

const { formRegister, formMethods } = useForm();

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
</template>
