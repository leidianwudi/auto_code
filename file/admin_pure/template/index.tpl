${# ============================================================================}
${# index.tpl — vue-pure-admin 后台管理列表页模板                                 }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 tplData 生成 pure-admin 列表页（el-table + el-pagination 自组装，     }
${#   不依赖 @pureadmin/components），页面骨架为 pure-admin 惯用的               }
${#   <div class="main"> + 搜索栏 + 工具栏 + 表格 + 分页，包含：                  }
${#   - 搜索表单（el-form inline：文本/日期/下拉/范围，逻辑同 admin_vue Search）  }
${#   - 工具栏（新增/批量删除/自定义工具栏按钮）                                  }
${#   - 表格列（文本/开关/金额/标签/布尔/下拉显示/图片 + 操作列，渲染逻辑等价     }
${#     admin_vue index.tpl 的 tableColumns 条件块）                              }
${#   - 编辑弹窗（引用 ./components/write.vue，与 admin_vue 交互同构）            }
${#   - 自定义 Dialog 按钮的弹窗（el-form 内联渲染，远程下拉选项弹窗打开时加载）  }
${# 数据来源（tplData）：admin_data.ac 的 buildAdminTplData 产出（键同 admin_vue），}
${#   另有 main_admin.ac enrichPureTplData 补充：isDatetimeFormat、dataNameCamel、}
${#   hasQueryFields/hasMoneyDisplay/hasRemoteDialogSelect/needMessageBox 等      }
${# http 约定：@/utils/http（pure-admin 标准），响应统一 { code, msg, data }；    }
${# URL 约定（POST 后缀式，与 admin_vue 链及 crud_gin router.tpl 对齐）：          }
${#   POST /{model}/selectByIn 列表 / update 新增更新 / delete 删除（data 传体）   }
${# ============================================================================}
<script setup lang="ts">
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== ${commentTitle}（${pageName}）查询列表 ====================
import { reactive, ref, unref } from "vue";
import { message } from "@/utils/message";
${if needMessageBox}import { ElMessageBox } from "element-plus";
${/if}${if hasRemoteDialogSelect}import { http } from "@/utils/http";
${/if}import Write from "./components/write.vue";
import { ${apiImports} } from "@/api/${apiModule}/${pageName}";
${if hasSelectApi}import { ${selectApiImports} } from "@/api/${apiModule}/${pageName}";
${/if}${if hasBoolSourceImports}${boolSourceImportLines}
${/if}${if hasLinkButtons}import { useRouter } from "vue-router";
${/if}
${if hasLinkButtons}
// 链接跳转按钮（actionType=link）
const { push } = useRouter();
${/if}
// ── 列表状态（pure-admin 惯用骨架：el-table + el-pagination 自组装）─────────
const loading = ref(false);
const dataList = ref<any[]>([]);
const total = ref(0);
const currentPage = ref(1);
const pageSize = ref(10);
const selectedRows = ref<any[]>([]);
const searchParams = ref<Record<string, any>>({});

// 搜索栏表单（范围查询生成 Start/End 两个参数）
${if hasQueryFields}const searchForm = reactive<Record<string, any>>({
${each q in queryFields}${if q.isRange}  ${q.dataName}Start: "",
  ${q.dataName}End: "",
${else}  ${q.dataName}: "",
${/if}${/each}});
${/if}
// 加载列表数据（POST ${selectUrl}，page/pageSize + 查询参数走 data）
const loadData = async () => {
  loading.value = true;
  try {
    const res: any = await ${queryApi}({
      page: currentPage.value,
      pageSize: pageSize.value,
      ...unref(searchParams)
    });
    dataList.value = res?.data?.list || [];
    total.value = res?.data?.total || 0;
  } finally {
    loading.value = false;
  }
};
loadData();

${if hasQueryFields}
// 搜索/重置（重置后回到第一页）
const onSearch = () => {
  currentPage.value = 1;
  searchParams.value = { ...searchForm };
  loadData();
};
const onReset = () => {
  Object.keys(searchForm).forEach((k: string) => ((searchForm as any)[k] = ""));
  onSearch();
};
${/if}
// 表格勾选（批量删除用）
const onSelectionChange = (rows: any[]) => {
  selectedRows.value = rows;
};

${if hasBoolApiColumns}
// 布尔开关引用的静态数据源文字：静态源函数同步返回，setup 直接读取，
// 渲染前数据已就绪；数据源缺项或函数抛错时在此直接暴露（不写死、不兜底）
const boolSwitchTexts = reactive<Record<string, { active: string; inactive: string }>>({});

${each col in columns}${if col.hasBoolApi}{
  const boolData = ${col.boolApiName}();
  const list = (boolData?.data?.list as Array<any>) || [];
  const activeItem = list.find((it: any) => String(it.value) === '1');
  const inactiveItem = list.find((it: any) => String(it.value) === '0');
  if (!activeItem) throw new Error('数据源缺少 value=1 的选项（字段 ${col.dataName}）');
  if (!inactiveItem) throw new Error('数据源缺少 value=0 的选项（字段 ${col.dataName}）');
  boolSwitchTexts['${col.dataName}'] = {
    active: String(activeItem.label),
    inactive: String(inactiveItem.label)
  };
}
${/if}${/each}
${/if}${if hasSelectApiColumns}
// 下拉框显示列选项映射：列表单元格按数据源把值映射为文字（与编辑/查询共享数据源）；
// tagDisplayMaps 按值映射 tag 样式（数据源 tags 配置），有样式时用 el-tag 带色显示
const selectDisplayMaps = reactive<Record<string, Record<string, string>>>({});
const tagDisplayMaps = reactive<Record<string, Record<string, string>>>({});

${each col in columns}${if col.isSelectDisplay}{
  const loadSelect${col.dataNamePascal} = async () => {
    try {
      const res: any = await ${col.selectApiName}();
      const map: Record<string, string> = {};
      (res?.data?.list || []).forEach((it: any) => {
        map[String(it.${col.selectValueField})] = String(it.${col.selectLabelField});
      });
      selectDisplayMaps['${col.dataName}'] = map;
      tagDisplayMaps['${col.dataName}'] = (res?.tags as Record<string, string>) || {};
    } catch (e) {
      selectDisplayMaps['${col.dataName}'] = {};
      tagDisplayMaps['${col.dataName}'] = {};
    }
  };
  loadSelect${col.dataNamePascal}();
}
${/if}${/each}
${/if}${each q in queryFields}${if q.isSelect}${if q.hasStaticRef}${else if q.selectApiName}
// ${q.displayName} 下拉选项（远程数据源，一次性加载）
const ${q.dataNameCamel}Options = ref<any[]>([]);
const load${q.dataNameCamel}Options = async () => {
  try {
    const res: any = await ${q.selectApiName}();
    ${q.dataNameCamel}Options.value = res?.data?.list || [];
  } catch (e) {
    ${q.dataNameCamel}Options.value = [];
  }
};
load${q.dataNameCamel}Options();
${/if}${/if}${/each}
${if hasMoneyDisplay}
// 金额格式化（千分位 + 固定小数位，逻辑同 admin_vue money 显示列）
const formatMoney = (v: any, precision: number) => {
  if (v == null || v === '') return '';
  const num = Number(v);
  if (isNaN(num)) return String(v);
  return num.toLocaleString('zh-CN', { minimumFractionDigits: precision, maximumFractionDigits: precision });
};
${/if}
${if hasTagColumns}
// tag 列映射表（提前构造，避免每次单元格渲染都重建对象）
const tagMaps: Record<string, Record<string, { text: string; color: string }>> = {
${each col in columns}${if col.isTagDisplay}  '${col.dataName}': ${col.tagItemsMapStr},
${/if}${/each}};
${/if}
// ── 编辑弹窗（与 admin_vue 交互同构：write.vue 挂在弹窗内，新增/编辑/详情共用）──
const dialogVisible = ref(false);
const dialogTitle = ref("");
const actionType = ref("");
const currentRow = ref<any>(null);
const saveLoading = ref(false);
const writeRef = ref();

const actionTitleMap: Record<string, string> = { add: '新增', edit: '编辑', detail: '详情' };

const addAction = () => {
  actionType.value = 'add';
  currentRow.value = null;
  dialogTitle.value = '新增${commentTitle}';
  dialogVisible.value = true;
};
const rowAction = (row: any, type: string) => {
  actionType.value = type;
  currentRow.value = row;
  dialogTitle.value = (actionTitleMap[type] ?? type) + '${commentTitle}';
  dialogVisible.value = true;
};
const closeDialog = () => {
  dialogVisible.value = false;
  actionType.value = '';
  currentRow.value = null;
};

// 保存（write.vue 校验通过后提交；POST ${updateUrl}，无 id 新增/有 id 更新）
const save = async () => {
  saveLoading.value = true;
  try {
    const ok = await writeRef.value?.submit();
    if (ok !== false) {
      message('保存成功', { type: 'success' });
      dialogVisible.value = false;
      loadData();
    }
  } catch (e: any) {
    message(e?.message ?? '保存失败', { type: 'error' });
  } finally {
    saveLoading.value = false;
  }
};

// 开关列状态切换（POST ${updateUrl} 局部更新，完成后重载列表回显）
const onSwitchChange = async (row: any, field: string, value: any) => {
  try {
    await ${updateApi}({ id: row.id, [field]: value });
    message('修改成功', { type: 'success' });
  } catch (e: any) {
    message(e?.message ?? '修改失败', { type: 'error' });
  } finally {
    loadData();
  }
};

${if !noDelete}
// 删除（支持批量，POST ${deleteUrl} 携 { ids } 请求体）
const delData = (row?: any) => {
  const ids = row ? [row.id] : selectedRows.value.map((it: any) => it.id);
  if (ids.length === 0) {
    message('请选择要删除的数据', { type: 'warning' });
    return;
  }
  ElMessageBox.confirm('确认删除选中的 ' + ids.length + ' 条数据？', '提示', { type: 'warning' })
    .then(async () => {
      await ${deleteApi}(ids);
      message('删除成功', { type: 'success' });
      loadData();
    })
    .catch(() => {});
};
${/if}
${if hasCustomButtons}
// ── 自定义按钮动作处理 ──
// row 为 null 表示工具栏按钮触发，有值表示行操作列按钮触发
const onCustomAction = (row: any, actionKey: string) => {
${each btn in ajaxButtons}
  if (actionKey === '${btn.key}') {
    ${btn.apiName}(row ? row.id : undefined);
    return;
  }
${/each}
${each btn in confirmButtons}
  if (actionKey === '${btn.key}') {
    ElMessageBox.confirm('${btn.confirmText}', '提示', { type: 'warning' })
      .then(() => ${btn.apiName}(row ? row.id : undefined));
    return;
  }
${/each}
${each btn in dialogButtons}
  if (actionKey === '${btn.key}') {
    ${btn.key}Visible.value = true;
    ${btn.key}Row.value = row;
    return;
  }
${/each}
${each btn in linkButtons}
  if (actionKey === '${btn.key}') {
    push('${btn.linkPath}');
    return;
  }
${/each}
};
${/if}
${if hasDialogButtons}
${each btn in dialogButtons}
// ── ${btn.dialogTitle} 对话框 ──
const ${btn.key}Visible = ref(false);
const ${btn.key}Row = ref<any>(null);
const ${btn.key}Form = reactive<Record<string, any>>({
${each f in btn.dialogFields}  ${f.fieldName}: '',
${/each}});
// 弹窗打开时加载远程下拉选项（isSelect 字段）
const on${btn.key}DialogOpen = () => {
${each f in btn.dialogFields}${if f.isSelect}  load${btn.key}${f.fieldNameCamel}Options();
${/if}${/each}};
${each f in btn.dialogFields}${if f.isSelect}
// ${f.label} 下拉选项（远程数据源）
const ${btn.key}${f.fieldNameCamel}Options = ref<any[]>([]);
const load${btn.key}${f.fieldNameCamel}Options = async () => {
  try {
    const res: any = await http.request<any>("${f.selectMethodLower}", "${f.selectUrl}");
    ${btn.key}${f.fieldNameCamel}Options.value = res?.data?.list || [];
  } catch (e) {
    ${btn.key}${f.fieldNameCamel}Options.value = [];
  }
};
${/if}${/each}
${/each}
// 自定义对话框提交
const onCustomDialogSubmit = async (actionKey: string) => {
${each btn in dialogButtons}
  if (actionKey === '${btn.key}') {
    await ${btn.dialogApi}({ id: ${btn.key}Row.value?.id, ...${btn.key}Form });
    ${btn.key}Visible.value = false;
    Object.keys(${btn.key}Form).forEach((k: string) => ((${btn.key}Form as any)[k] = ''));
    return;
  }
${/each}
};
${/if}
</script>

<template>
  <div class="main">
${if hasQueryFields}    <!-- 搜索栏 -->
    <el-form :inline="true" :model="searchForm" class="bg-white rounded p-3 mb-3">
${each q in queryFields}${if q.isRange}
      <el-form-item label="${q.displayName}开始">
${if q.isDate}        <el-date-picker v-model="searchForm.${q.dataName}Start" type="${q.dateFormat}"${if q.isDatetimeFormat} value-format="YYYY-MM-DD HH:mm:ss"${else} value-format="YYYY-MM-DD"${/if} placeholder="请选择" style="width: 180px" />
${else}        <el-input v-model="searchForm.${q.dataName}Start"${if q.hasPlaceholder} placeholder="${q.placeholder}"${else} placeholder="请输入"${/if} clearable style="width: 160px" />
${/if}
      </el-form-item>
      <el-form-item label="${q.displayName}结束">
${if q.isDate}        <el-date-picker v-model="searchForm.${q.dataName}End" type="${q.dateFormat}"${if q.isDatetimeFormat} value-format="YYYY-MM-DD HH:mm:ss"${else} value-format="YYYY-MM-DD"${/if} placeholder="请选择" style="width: 180px" />
${else}        <el-input v-model="searchForm.${q.dataName}End"${if q.hasPlaceholder} placeholder="${q.placeholder}"${else} placeholder="请输入"${/if} clearable style="width: 160px" />
${/if}
      </el-form-item>
${else if q.isSelect}${if q.hasStaticRef}
      <el-form-item label="${q.displayName}">
        <!-- 静态选项（沿用列取值域）：值域封闭，clearable 清空 = 不筛选 -->
        <el-select v-model="searchForm.${q.dataName}" clearable placeholder="请选择" style="width: 180px">
          <el-option v-for="it in (${q.selectStaticName}() as any)?.data?.list || []" :key="String(it.value)" :label="it.label" :value="it.value" />
        </el-select>
      </el-form-item>
${else if q.selectApiName}
      <el-form-item label="${q.displayName}">
        <el-select v-model="searchForm.${q.dataName}" clearable placeholder="请选择" style="width: 180px">
          <el-option v-for="it in ${q.dataNameCamel}Options" :key="String(it.${q.selectValueField})" :label="it.${q.selectLabelField}" :value="it.${q.selectValueField}" />
        </el-select>
      </el-form-item>
${else}
      <el-form-item label="${q.displayName}">
        <el-select v-model="searchForm.${q.dataName}" clearable placeholder="请选择" style="width: 180px" />
      </el-form-item>
${/if}
${else if q.isDate}
      <el-form-item label="${q.displayName}">
        <el-date-picker v-model="searchForm.${q.dataName}" type="${q.dateFormat}"${if q.isDatetimeFormat} value-format="YYYY-MM-DD HH:mm:ss"${else} value-format="YYYY-MM-DD"${/if} placeholder="请选择" style="width: 180px" />
      </el-form-item>
${else}
      <el-form-item label="${q.displayName}">
        <el-input v-model="searchForm.${q.dataName}"${if q.hasPlaceholder} placeholder="${q.placeholder}"${else} placeholder="请输入"${/if} clearable style="width: 180px" @keyup.enter="onSearch" />
      </el-form-item>
${/if}${/each}
      <el-form-item>
        <el-button type="primary" @click="onSearch">搜索</el-button>
        <el-button @click="onReset">重置</el-button>
      </el-form-item>
    </el-form>
${/if}
    <!-- 工具栏 + 表格 + 分页 -->
    <div class="bg-white rounded p-3">
      <div class="mb-3">
        <el-button type="primary" @click="addAction">新增</el-button>
${if !noDelete}        <el-button type="danger" :disabled="selectedRows.length === 0" @click="delData()">删除</el-button>
${/if}${each btn in toolbarButtons}        <el-button${if btn.hasType} type="${btn.type}"${/if} @click="onCustomAction(null, '${btn.key}')">${btn.label}</el-button>
${/each}      </div>

      <el-table v-loading="loading" :data="dataList" @selection-change="onSelectionChange"${if hasTableDefaultSort} :default-sort="{ prop: '${defaultSortField}', order: '${defaultSortOrder}' }"${/if}>
        <el-table-column type="selection" width="55" />
${each col in columns}${if col.queryVisible}${if col.isBooleanSwitch}
        <el-table-column label="${col.label}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <el-switch
              :model-value="row.${col.dataName} == 1"
              :disabled="${col.switchDisabledStr}"
              inline-prompt
              :active-text="${col.switchActiveTextExpr}"
              :inactive-text="${col.switchInactiveTextExpr}"
              @change="onSwitchChange(row, '${col.dataName}', row.${col.dataName} == 1 ? 0 : 1)"
            />
          </template>
        </el-table-column>
${else if col.isTagSwitch}
        <el-table-column label="${col.label}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <el-switch
              :model-value="String(row.${col.dataName}) == '${col.switchActiveValue}'"
              :disabled="${col.switchDisabledStr}"
              inline-prompt
              active-text="${col.switchActiveText}"
              inactive-text="${col.switchInactiveText}"
              @change="onSwitchChange(row, '${col.dataName}', String(row.${col.dataName}) == '${col.switchActiveValue}' ? '${col.switchInactiveValue}' : '${col.switchActiveValue}')"
            />
          </template>
        </el-table-column>
${else if col.isMoneyDisplay}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <span>{{ formatMoney(row.${col.dataName}, ${col.precision}) }}</span>
          </template>
        </el-table-column>
${else if col.isTagDisplay}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <el-tag v-if="tagMaps['${col.dataName}']?.[String(row.${col.dataName})]" :type="(tagMaps['${col.dataName}']?.[String(row.${col.dataName})]?.color as any)">
              {{ tagMaps['${col.dataName}']?.[String(row.${col.dataName})]?.text }}
            </el-tag>
            <span v-else>{{ row.${col.dataName} }}</span>
          </template>
        </el-table-column>
${else if col.isBooleanDisplay}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <span>{{ row.${col.dataName} ? ${col.switchActiveTextExpr} : ${col.switchInactiveTextExpr} }}</span>
          </template>
        </el-table-column>
${else if col.isSelectDisplay}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <el-tag v-if="tagDisplayMaps['${col.dataName}']?.[String(row.${col.dataName})]" :type="(tagDisplayMaps['${col.dataName}']?.[String(row.${col.dataName})] as any)">
              {{ selectDisplayMaps['${col.dataName}']?.[String(row.${col.dataName})] ?? row.${col.dataName} }}
            </el-tag>
            <span v-else>{{ selectDisplayMaps['${col.dataName}']?.[String(row.${col.dataName})] ?? row.${col.dataName} }}</span>
          </template>
        </el-table-column>
${else if col.isImageDisplay}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
          <template #default="{ row }">
            <el-image
              v-if="row.${col.dataName}"
              :src="row.${col.dataName}"
              fit="contain"
              :preview-src-list="[row.${col.dataName}]"
              preview-teleported
              :style="${if col.hasListThumbSize}'width: ${col.listThumbWidth}px; height: ${col.listThumbHeight}px'${else}'width: 80px; height: 40px'${/if}"
            />
          </template>
        </el-table-column>
${else}
        <el-table-column label="${col.label}" prop="${col.dataName}"${if col.hasDefaultSort} sortable${/if}${if col.hasColumnWidth} width="${col.columnWidth}"${/if}${if col.hasColumnFixed} fixed="${col.columnFixed}"${/if}>
${if col.isFormatterDate}          <template #default="{ row }">
            <span>{{ row.${col.dataName} ? String(row.${col.dataName}).substring(0, 10) : '' }}</span>
          </template>
${else if col.isFormatterStatus}          <template #default="{ row }">
            <span>{{ row.${col.dataName} ? '启用' : '禁用' }}</span>
          </template>
${else if col.isFormatterCurrency}          <template #default="{ row }">
            <span>{{ row.${col.dataName} == null ? '' : '¥' + Number(row.${col.dataName}).toFixed(2) }}</span>
          </template>
${/if}
        </el-table-column>
${/if}
${/if}${/each}
        <el-table-column label="操作" width="${actionColumnWidth}" fixed="right">
          <template #default="{ row }">
${if !noDetail}            <el-button link type="primary" size="small" @click="rowAction(row, 'detail')">详情</el-button>
${/if}${if !noEdit}            <el-button link type="primary" size="small" @click="rowAction(row, 'edit')">编辑</el-button>
${/if}${if !noDelete}            <el-button link type="danger" size="small" @click="delData(row)">删除</el-button>
${/if}${each btn in rowButtons}            <el-button link size="small"${if btn.hasType} type="${btn.type}"${/if} @click="onCustomAction(row, '${btn.key}')">${btn.label}</el-button>
${/each}
          </template>
        </el-table-column>
      </el-table>

      <div class="mt-3 flex justify-end">
        <el-pagination
          v-model:current-page="currentPage"
          v-model:page-size="pageSize"
          :page-sizes="[10, 20, 50, 100]"
          :total="total"
          layout="total, sizes, prev, pager, next, jumper"
          @current-change="loadData"
          @size-change="loadData"
        />
      </div>
    </div>

    <!-- 编辑弹窗（write.vue：新增/编辑/详情共用，交互与 admin_vue 同构） -->
    <el-dialog v-model="dialogVisible" :title="dialogTitle" width="760px" destroy-on-close>
      <Write ref="writeRef" :current-row="currentRow" :action-type="actionType" />
      <template #footer>
        <el-button v-if="actionType !== 'detail'" type="primary" :loading="saveLoading" @click="save">保存</el-button>
        <el-button @click="dialogVisible = false">关闭</el-button>
      </template>
    </el-dialog>
${if hasDialogButtons}${each btn in dialogButtons}
    <!-- ${btn.dialogTitle} 对话框 -->
    <el-dialog v-model="${btn.key}Visible" title="${btn.dialogTitle}" width="560px" @open="on${btn.key}DialogOpen">
      <el-form :model="${btn.key}Form" label-width="110px">
${each f in btn.dialogFields}
        <el-form-item label="${f.label}" prop="${f.fieldName}"${if f.required} required${/if}>
${if f.isSelect}          <el-select v-model="${btn.key}Form.${f.fieldName}" clearable placeholder="请选择" style="width: 100%">
            <el-option v-for="it in ${btn.key}${f.fieldNameCamel}Options" :key="String(it.${f.selectValueField})" :label="it.${f.selectLabelField}" :value="it.${f.selectValueField}" />
          </el-select>
${else if f.isInt}${else if f.isFloat}          <el-input-number v-model="${btn.key}Form.${f.fieldName}" style="width: 100%" />
${else if f.isDate}          <el-date-picker v-model="${btn.key}Form.${f.fieldName}"${if f.hasDateFormat} type="${f.dateFormat}"${/if} placeholder="请选择日期" style="width: 100%" />
${else if f.isTextArea}          <el-input v-model="${btn.key}Form.${f.fieldName}" type="textarea" :rows="${f.textareaRows}" placeholder="请输入" />
${else}          <el-input v-model="${btn.key}Form.${f.fieldName}" placeholder="请输入" />
${/if}
        </el-form-item>
${/each}
      </el-form>
      <template #footer>
        <el-button type="primary" @click="onCustomDialogSubmit('${btn.key}')">确定</el-button>
        <el-button @click="${btn.key}Visible = false">取消</el-button>
      </template>
    </el-dialog>
${/each}${/if}
  </div>
</template>

<style>
/* 图片缩略图单元格：压缩含图片单元格的上下内边距（8px → 2px），无图片的行和
   其它列不受影响。列内容由模板插槽渲染，插槽 VNode 不携带 SFC scope 属性，
   因此用全局样式；各生成页面的这份规则完全相同，重复加载无副作用 */
.el-table .el-table__cell:has(.el-image) {
  padding-top: 2px;
  padding-bottom: 2px;
}
/* el-image 默认 inline-block 按文本基线对齐，改块级后上下间隔对称 */
.el-table .el-table__cell .el-image {
  display: block;
}
</style>
