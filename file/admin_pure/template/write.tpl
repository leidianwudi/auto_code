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
${# 数据来源（tplData）：formColumns 表单列数组（按 editCols/字段呈现行序排序，       }
${#   与列表演示顺序解耦；键同 admin_vue/write.tpl 头注释），                        }
${#   另有 main_admin.ac enrichPureTplData 补充：isDatetimeFormat 等              }
${# POST 后缀式约定：submit 由 updateApi 承接（POST {model}/update，无 id 新增、   }
${#   有 id 更新，与 admin_vue 链及 crud_gin router.tpl 对齐）                     }
${# ============================================================================}
<script setup lang="ts">
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== ${commentTitle}（${pageName}）编辑界面 ====================
import { reactive, ref, computed, watch${if hasDefaultValues}, nextTick${/if}${if hasI18n}, onMounted${/if} } from "vue";
import type { PropType } from "vue";
import type { FormInstance, FormRules } from "element-plus";
${if hasUploadFields}import { ElMessage } from "element-plus";
${/if}${if hasI18n}import { http } from "@/utils/http";
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

// 表单数据模型（id 隐藏携带：save 接口靠它区分新增/更新；数值/日期初始化 null，其余空串）
const formRef = ref<FormInstance>();
const form = reactive<Record<string, any>>({
  id: null,
${if hasI18n}  i18n: {},
${/if}${each col in formColumns}${if col.editVisible}${if col.isInt}  ${col.dataName}: null,
${else if col.isFloat}  ${col.dataName}: null,
${else if col.isMoney}  ${col.dataName}: null,
${else if col.isDate}  ${col.dataName}: null,
${else}  ${col.dataName}: "",
${/if}${/if}${/each}});

${if hasI18n}
// ==================== 多语言（i18n）====================
// i18nInfo：{ defaultLang, table, fields:[{name,label,mysqlType,widget}] }
const i18nInfo = ${i18nJson};
const i18nActive = ref<string>(i18nInfo.defaultLang);
const i18nLangs = ref<any[]>([{ lang: i18nInfo.defaultLang, name: i18nInfo.defaultLang }]);
const i18nFields: any[] = i18nInfo.fields || [];

// 确保每种语言的对象与字段都存在（模板里 v-model 直接绑 form.i18n[lang][field]）
const ensureI18n = () => {
  const langs: string[] = i18nLangs.value.map((l: any) => l.lang);
  if (!langs.includes(i18nInfo.defaultLang)) langs.unshift(i18nInfo.defaultLang);
  langs.forEach((lang: string) => {
    if (!form.i18n[lang]) form.i18n[lang] = {};
    i18nFields.forEach((f: any) => {
      if (form.i18n[lang][f.name] == null) form.i18n[lang][f.name] = "";
    });
  });
};

// 语言清单来自 base_lang（enable=1；取不到时只用默认语言）
const loadI18nLangs = async () => {
  try {
    const res: any = await http.request<any>("post", "/api/base_lang/selectByIn", {
      data: { page: 1, pageSize: 100, enable: 1 }
    });
    const list: any[] = res?.data?.list || [];
    if (list.length > 0) {
      i18nLangs.value = list.map((x: any) => ({ lang: x.lang, name: x.name || x.lang }));
    }
  } catch (e) {
    // base_lang 不可用：保留默认语言
  }
  ensureI18n();
};

// 编辑/详情：拉详情拿到全语言翻译 map（列表行只平铺当前语言文本）
const loadI18nDetail = async (id: any) => {
  if (id == null || id === "") { ensureI18n(); return; }
  try {
    const res: any = await http.request<any>("post", "${detailUrl}", { data: { id } });
    if (res?.code === 0 && res.data?.i18n) {
      form.i18n = res.data.i18n;
    }
  } catch (e) {
    // 详情失败：保留默认语言平铺值
  }
  ensureI18n();
};
onMounted(loadI18nLangs);
${/if}
// 回填：编辑/详情把当前行写入表单（含 id）；新增（currentRow=null）清空
watch(
  () => props.currentRow,
  row => {
    if (row) {
      form.id = row.id ?? null;
${if hasI18n}      form.i18n = row.i18n ? JSON.parse(JSON.stringify(row.i18n)) : {};
      ensureI18n();
      // 详情未返回前先用列表行的当前语言平铺值兜底
      i18nFields.forEach((f: any) => {
        if (form.i18n[i18nInfo.defaultLang][f.name] === "" && row[f.name] != null) {
          form.i18n[i18nInfo.defaultLang][f.name] = row[f.name];
        }
      });
      loadI18nDetail(row.id);
${/if}${each col in formColumns}${if col.editVisible}${if col.isInt}      form.${col.dataName} = row.${col.dataName} ?? null;
${else if col.isFloat}      form.${col.dataName} = row.${col.dataName} ?? null;
${else if col.isMoney}      form.${col.dataName} = row.${col.dataName} ?? null;
${else if col.isDate}      form.${col.dataName} = row.${col.dataName} ?? null;
${else}      form.${col.dataName} = row.${col.dataName} ?? "";
${/if}${/if}${/each}    } else {
      form.id = null;
${if hasI18n}      form.i18n = {};
      ensureI18n();
${/if}${each col in formColumns}${if col.editVisible}${if col.isInt}      form.${col.dataName} = null;
${else if col.isFloat}      form.${col.dataName} = null;
${else if col.isMoney}      form.${col.dataName} = null;
${else if col.isDate}      form.${col.dataName} = null;
${else}      form.${col.dataName} = "";
${/if}${/if}${/each}    }
  },
  { immediate: true }
);

${if hasUploadFields}
// ==================== 图片上传（引用 .jsonupload 上传预设）====================
// 上传逻辑需在模板渲染前定义：上传卡片直接引用这些函数与列表
${each col in formColumns}${if col.hasUpload}${if col.editVisible}
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
${each col in formColumns}${if col.isSelect}${if col.hasSelectApi}
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
${each col in formColumns}${if col.isBooleanSwitch}${if col.hasBoolApi}
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
${each col in formColumns}${if col.isBooleanEdit}${if col.hasBoolApi}
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
${each col in formColumns}${if col.required}${if col.editVisible}${if col.isSelect}  ${col.dataName}: [{ required: true, message: '请选择${col.editName}', trigger: 'change' }],
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
${each col in formColumns}${if col.hasDefaultValue}  ${col.dataName}: ${col.defaultValueLiteral},
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
${each col in formColumns}${if col.editVisible}
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
${if hasI18n}
      <el-col :span="24">
        <el-divider content-position="left">多语言内容</el-divider>
        <el-tabs v-model="i18nActive">
          <el-tab-pane v-for="lg in i18nLangs" :key="lg.lang" :label="lg.name" :name="lg.lang">
            <el-row>
              <el-col :span="24" v-for="f in i18nFields" :key="f.name">
                <el-form-item :label="f.label">
                  <el-input v-if="f.widget === 'textarea'" v-model="form.i18n[lg.lang][f.name]" type="textarea" :rows="3" />
                  <el-input v-else v-model="form.i18n[lg.lang][f.name]" />
                </el-form-item>
              </el-col>
            </el-row>
          </el-tab-pane>
        </el-tabs>
      </el-col>
${/if}
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
