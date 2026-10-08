${# ============================================================================}
${# write.tpl — vue-pure-admin 编辑表单模板（挂载在 index 的 el-dialog 内）       }
${# ----------------------------------------------------------------------------}
${# 作用：与 admin_vue/write.tpl 交互同构（纯表单组件、不含弹窗外壳），包含：      }
${#   - el-form 表单（布尔开关/标签下拉/远程下拉/多行文本/日期/数值/图片上传）    }
${#   - 图片上传卡片墙（引用 .jsonupload 上传预设：悬停出 × 删除、尾部 + 添加；   }
${#     详情模式仅预览；上传动作瘦包装调用共享上传函数 upload.tpl 产物）          }
${#   - 表单验证规则（根据配置的 required 字段生成）                              }
${#   - 新增记录默认值注入（hasDefaultValues，watch actionType=add 后 nextTick）  }
${#   - 详情模式：el-form 整体 disabled；editEditable=false 字段单独 disabled     }
${# 数据来源（tplData）：columns 列数组（键同 admin_vue/write.tpl 头注释），       }
${#   另有 main_admin.ac enrichPureTplData 补充：isDatetimeFormat 等              }
${# POST 后缀式约定：submit 由 updateApi 承接（POST {model}/update，无 id 新增、   }
${#   有 id 更新，与 admin_vue 链及 crud_gin router.tpl 对齐）                     }
${# ============================================================================}
<script setup lang="ts">
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== ${commentTitle}（${pageName}）编辑界面 ====================
import { reactive, ref, computed${if hasWatch}, watch${/if}${if hasDefaultValues}, nextTick${/if} } from "vue";
import type { PropType } from "vue";
import type { FormInstance, FormRules } from "element-plus";
${if hasUploadFields}import { ElMessage } from "element-plus";
${/if}import { ${updateApi} } from "@/api/${apiModule}/${pageName}";
${if hasSelectApi}import { ${selectApiImports} } from "@/api/${apiModule}/${pageName}";
${/if}${if hasSourceImports}${sourceImportLines}
${/if}${if hasUploadImports}${uploadImportLines}
${/if}
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

// 详情模式：表单整体只读（el-form disabled 会联动禁用全部控件）
const isDetail = computed(() => props.actionType === 'detail');

// 表单数据模型（数值/日期初始化 null，其余空串；新增默认值见下方 defaultValues）
const formRef = ref<FormInstance>();
const form = reactive<Record<string, any>>({
${each col in columns}${if col.editVisible}${if col.isInt}  ${col.dataName}: null,
${else if col.isFloat}  ${col.dataName}: null,
${else if col.isMoney}  ${col.dataName}: null,
${else if col.isDate}  ${col.dataName}: null,
${else}  ${col.dataName}: '',
${/if}${/if}${/each}});

${if hasUploadFields}
// ==================== 图片上传（引用 .jsonupload 上传预设）====================
// 上传逻辑需在模板渲染前定义：上传卡片直接引用这些函数与列表
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
  // 换图时带上旧图地址，后端删除旧文件避免垃圾图片残留（新增记录无旧图不传）
  const oldImgUrl = props.currentRow?.['${col.dataName}'] || undefined;
  const url: string = await ${col.uploadFuncName}(opt.file, oldImgUrl);
  if (!url) {
    ElMessage.error('上传响应中未找到图片地址');
    return;
  }
${if col.uploadMulti}  ${col.dataName}FileList.push({ name: url, url });
  form.${col.dataName} = ${col.dataName}FileList.map((f: any) => f.url);
${else}  ${col.dataName}FileList.splice(0, ${col.dataName}FileList.length, { name: url, url });
  form.${col.dataName} = url;
${/if}};
const remove${col.dataNamePascal} = (idx: number) => {
  ${col.dataName}FileList.splice(idx, 1);
  form.${col.dataName} = ${col.uploadRemoveValueExpr};
};
${/if}${/if}${/each}
${/if}
${if hasSelectFields}
// ==================== 下拉选项加载（onceSelect 语义：同字段只请求一次）========
// 引用 .jsonsource 数据源的列（函数来自 source api 文件）与手动 URL 列
// （函数来自本页 api 文件）统一走这里；静态源同步返回，await 兼容两种形态
const optionCache: Record<string, any[]> = {};
const onceOptions = async (key: string, loader: () => Promise<any[]> | any[]) => {
  if (optionCache[key]) return optionCache[key];
  const list = await loader();
  optionCache[key] = list;
  return list;
};
${each col in columns}${if col.isSelect}${if col.hasSelectApi}
// ${col.editName} 下拉选项（引用数据源：${col.selectApiName}）
const ${col.dataNamePascal}Options = ref<any[]>([]);
const load${col.dataNamePascal}Options = async () => {
  ${col.dataNamePascal}Options.value = await onceOptions('${col.dataName}', async () => {
    try {
      const res: any = await ${col.selectApiName}();
      return res?.data?.list || [];
    } catch (e) {
      return [];
    }
  });
};
load${col.dataNamePascal}Options();
${/if}${/if}${/each}
${each col in columns}${if col.isBooleanSwitch}${if col.hasBoolApi}
// ${col.editName} 布尔开关选项（引用静态数据源：${col.boolApiName}）
const ${col.dataNamePascal}Options = ref<any[]>([]);
const load${col.dataNamePascal}Options = async () => {
  ${col.dataNamePascal}Options.value = await onceOptions('${col.dataName}', async () => {
    try {
      const res: any = await ${col.boolApiName}();
      return res?.data?.list || [];
    } catch (e) {
      return [];
    }
  });
};
load${col.dataNamePascal}Options();
${/if}${/if}${/each}
${each col in columns}${if col.isBooleanEdit}${if col.hasBoolApi}
// ${col.editName} 布尔下拉选项（引用静态数据源：${col.boolApiName}）
const ${col.dataNamePascal}Options = ref<any[]>([]);
const load${col.dataNamePascal}Options = async () => {
  ${col.dataNamePascal}Options.value = await onceOptions('${col.dataName}', async () => {
    try {
      const res: any = await ${col.boolApiName}();
      return res?.data?.list || [];
    } catch (e) {
      return [];
    }
  });
};
load${col.dataNamePascal}Options();
${/if}${/if}${/each}
${/if}
// 表单验证规则（根据配置的 required 字段生成；选择/日期类触发 change）
const formRules = reactive<FormRules>({
${each col in columns}${if col.required}${if col.editVisible}${if col.isSelect}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else if col.isBooleanSwitch}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else if col.isTagSwitch}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else if col.isBooleanEdit}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else if col.isTagEdit}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else if col.isDate}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
${else}  ${col.dataName}: [{ required: true, message: '请输入${col.editName}', trigger: 'blur' }],
${/if}${/if}${/if}${/each}});

// 校验并提交（POST ${updateUrl}，无 id 新增/有 id 更新）
const submit = async (): Promise<boolean> => {
  const valid = await formRef.value?.validate().catch(() => false);
  if (!valid) return false;
  await ${updateApi}({ ...form });
  return true;
};
defineExpose({ submit });

${if hasDefaultValues}
// 新增记录时的默认值（数值类型不带引号，其余类型带引号）
const defaultValues: Record<string, any> = {
${each col in columns}${if col.hasDefaultValue}  ${col.dataName}: ${col.defaultValueLiteral},
${/if}${/each}};

// 新增时设置默认值（在表单重置后通过 nextTick 注入）
watch(() => props.actionType, (newVal) => {
  if (newVal === 'add') {
    nextTick(() => {
      Object.keys(defaultValues).forEach((k: string) => ((form as any)[k] = (defaultValues as any)[k]));
    });
  }
});
${/if}
</script>

<template>
  <el-form ref="formRef" :model="form" :rules="formRules" :disabled="isDetail" label-width="110px">
    <el-row>
${each col in columns}${if col.editVisible}
      <el-col :span="${col.formSpan}">
        <el-form-item label="${col.editName}" prop="${col.dataName}">
${if col.isBooleanSwitch}${if col.hasBoolApi}          <el-select v-model="form.${col.dataName}" clearable placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
            <el-option v-for="it in ${col.dataNamePascal}Options" :key="String(it.value)" :label="it.label" :value="it.value" />
          </el-select>
${else}          <el-select v-model="form.${col.dataName}" placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
            <el-option label="${col.switchInactiveText}" :value="0" />
            <el-option label="${col.switchActiveText}" :value="1" />
          </el-select>
${/if}
${else if col.isTagSwitch}          <el-select v-model="form.${col.dataName}" placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
${each t in col.tagItems}            <el-option label="${t.textEsc}" value="${t.valueEsc}" />
${/each}          </el-select>
${else if col.isSelect}${if col.hasSelectApi}          <el-select v-model="form.${col.dataName}" clearable placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
            <el-option v-for="it in ${col.dataNamePascal}Options" :key="String(it.${col.selectValueField})" :label="it.${col.selectLabelField}" :value="it.${col.selectValueField}" />
          </el-select>
${else}          <el-select v-model="form.${col.dataName}" clearable placeholder="请选择" style="width: 100%" />
${/if}
${else if col.isTagEdit}          <el-select v-model="form.${col.dataName}" placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
${each t in col.tagItems}            <el-option label="${t.textEsc}" value="${t.valueEsc}" />
${/each}          </el-select>
${else if col.isBooleanEdit}${if col.hasBoolApi}          <el-select v-model="form.${col.dataName}" clearable placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
            <el-option v-for="it in ${col.dataNamePascal}Options" :key="String(it.value)" :label="it.label" :value="it.value" />
          </el-select>
${else}          <el-select v-model="form.${col.dataName}" placeholder="请选择" style="width: 100%"${if !col.editEditable} disabled${/if}>
            <el-option label="${col.boolFalseText}" value="0" />
            <el-option label="${col.boolTrueText}" value="1" />
          </el-select>
${/if}
${else if col.isImageEdit}${if col.hasUpload}          <!-- 上传控件嵌在表单字段位置（顺序跟列配置一致，label 左对齐）；详情模式仅预览。 -->
          <!-- 卡片墙样式：图片悬停出 × 删除，尾部 + 卡片添加 -->
          <div class="ac-up-cards">
            <div v-for="(f, idx) in ${col.dataName}FileList" :key="f.url" class="ac-up-card"${if col.hasEditThumbSize} :style="{ width: '${col.editThumbWidth}px', height: '${col.editThumbHeight}px' }"${/if}>
              <el-image :src="f.url" fit="contain" class="ac-up-thumb" :preview-src-list="[f.url]" preview-teleported />
              <div v-if="!isDetail" class="ac-up-mask">
                <span class="ac-up-del" @click="remove${col.dataNamePascal}(idx)">×</span>
              </div>
            </div>
            <el-upload v-if="!isDetail" action="#" accept="image/*" :show-file-list="false" :multiple="${col.uploadMulti}" :limit="${col.uploadLimit}" :http-request="upload${col.dataNamePascal}" class="ac-up-trigger">
              <span class="ac-up-plus">+</span>
            </el-upload>
          </div>
${else}          <el-input v-model="form.${col.dataName}"${if col.hasPlaceholder} placeholder="${col.placeholder}"${else} placeholder="图片地址"${/if} />
${/if}
${else if col.isMoney}          <el-input-number v-model="form.${col.dataName}" :precision="${col.precision}" style="width: 100%"${if !col.editEditable} disabled${/if} />
${else if col.isTextArea}          <el-input v-model="form.${col.dataName}" type="textarea" :rows="${col.textareaRows}"${if col.hasPlaceholder} placeholder="${col.placeholder}"${/if}${if !col.editEditable} disabled${/if} />
${else if col.isDate}${if col.isTime}          <el-time-picker v-model="form.${col.dataName}" value-format="HH:mm:ss" placeholder="请选择时间" style="width: 100%"${if !col.editEditable} disabled${/if} />
${else}          <el-date-picker v-model="form.${col.dataName}" type="${col.dateFormat}"${if col.isDatetimeFormat} value-format="YYYY-MM-DD HH:mm:ss"${else} value-format="YYYY-MM-DD"${/if} placeholder="请选择日期" style="width: 100%"${if !col.editEditable} disabled${/if} />
${/if}
${else if col.isInt}          <el-input-number v-model="form.${col.dataName}"${if col.hasMinValue} :min="${col.minValue}"${/if}${if col.hasMaxValue} :max="${col.maxValue}"${/if} style="width: 100%"${if !col.editEditable} disabled${/if} />
${else if col.isFloat}          <el-input-number v-model="form.${col.dataName}" :precision="${col.precision}"${if col.hasMinValue} :min="${col.minValue}"${/if}${if col.hasMaxValue} :max="${col.maxValue}"${/if} style="width: 100%"${if !col.editEditable} disabled${/if} />
${else}          <el-input v-model="form.${col.dataName}"${if col.hasPlaceholder} placeholder="${col.placeholder}"${/if}${if col.hasMaxlength} :maxlength="${col.maxlength}"${/if}${if !col.editEditable} disabled${/if} />
${/if}
        </el-form-item>
      </el-col>
${/if}${/each}
    </el-row>
  </el-form>
</template>

<style>
/* 上传控件由模板内联渲染（el-upload/el-image）：卡片墙样式沿用 admin_vue 约定，
   类名统一 ac-up- 前缀避免与页面样式冲突；各生成页面的这份规则完全相同，
   同名全局规则重复加载无副作用 */
.ac-up-cards { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.ac-up-card { position: relative; width: 160px; height: 80px; border: 1px solid #dcdfe6; border-radius: 6px; overflow: hidden; }
.ac-up-thumb { width: 100%; height: 100%; display: block; }
.ac-up-mask { position: absolute; top: 0; right: 0; left: 0; bottom: 0; display: none; align-items: center; justify-content: center; background: rgba(0, 0, 0, 0.4); }
.ac-up-card:hover .ac-up-mask { display: flex; }
.ac-up-del { color: #fff; font-size: 20px; cursor: pointer; line-height: 1; }
.ac-up-plus { font-size: 24px; color: #909399; }
</style>
