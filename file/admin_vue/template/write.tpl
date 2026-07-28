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
${#      textareaRows, required, formSpan, editComponent}]                       }
${# ============================================================================}
//此代码为AutoCode框架生成，请勿手动修改
<script setup lang="tsx">
import { Form, FormSchema } from '@/components/form';
import { useForm } from '@/hooks/web/use_form';
import { PropType, reactive, ref, computed } from 'vue';
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
${each col in columns}
${if col.isSwitch}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Select',
    componentProps: {
      options: [
        {
          label: '禁用',
          value: 0
        },
        {
          label: '启用',
          value: 1
        }
      ]
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
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'DatePicker',
    componentProps: {
      type: '${col.dateFormat}'
    }${if col.hasFormSpan},
    colProps: { span: ${col.formSpan} }${/if}
  },
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
${/each}]);

// 详情模式下将所有组件设为只读
const formSchemaComputed = computed(() => {
  if (!isDetail.value) return formSchema.value;
  return formSchema.value.map((item: any) => ({
    ...item,
    component: 'Input',
    componentProps: {
      ...(item.componentProps || {}),
      disabled: true
    }
  }));
});

// 表单验证规则（根据配置的 required 字段生成）
const rules = reactive({
${each col in columns}${if col.required}  ${col.dataName}: [required()],
${/if}${/each}});

const { formRegister, formMethods } = useForm();

// 使用 useWriteLogic 钩子，仅使用其 submit 方法
const { submit } = uiWriteLogic(
  computed(() => props.currentRow),
  formSchemaComputed,
  formMethods
);

// 向父组件暴露接口
defineExpose({
  submit
});
</script>

<template>
  <Form :rules="isDetail ? {} : rules" @register="formRegister" :schema="formSchemaComputed" />
</template>
