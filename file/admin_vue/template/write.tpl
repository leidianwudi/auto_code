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
${#     [{dataName, editName, isSwitch, isTextArea, editComponent}]              }
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
  }
});

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
    }
  },
${else if col.isTextArea}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: 'Input',
    componentProps: {
      type: 'textarea',
      rows: 3
    }
  },
${else}
  {
    field: '${col.dataName}',
    label: '${col.editName}',
    component: '${col.editComponent}'
  },
${/if}
${/each}]);

// 设置表单规则，必填规则
const rules = reactive({
  key: [required()],
  value: [required()]
});

const { formRegister, formMethods } = useForm();

// 使用 useWriteLogic 钩子，仅使用其 submit 方法
const { submit } = uiWriteLogic(
  computed(() => props.currentRow),
  formSchema,
  formMethods
);

// 向父组件暴露接口
defineExpose({
  submit
});
</script>

<template>
  <Form :rules="rules" @register="formRegister" :schema="formSchema" />
</template>
