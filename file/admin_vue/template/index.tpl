${# ============================================================================}
${# index.tpl — Vue3 后台管理列表页模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonvue 配置生成 Vue3 列表页（与 jsonvue 同名的 .vue），包含：          }
${#   - 搜索表单（searchSchema）                                                 }
${#   - 表格列定义（tableColumns，支持文本/开关样式）                            }
${#   - CRUD 逻辑（通过 uiCrudLogic Hook）                                      }
${#   - 编辑弹窗（引用 write.vue）                                               }
${# 数据来源（tplData）：                                                        }
${#   apiImports    - API 导入列表字符串，如 "getConfigGListApi, delConfigGListApi"}
${#   apiModule     - API 模块路径，如 "setting"                                 }
${#   queryApi      - 查询接口名                                                 }
${#   deleteApi     - 删除接口名                                                 }
${#   updateApi     - 修改接口名                                                 }
${#   noDelete      - 不可删除                                                   }
${#   noEdit        - 不可编辑                                                   }
${#   queryFields   - 查询字段数组 [{displayName, dataName, isSelect, isDate,    }
${#                    selectUrl, selectValueField, selectLabelField,            }
${#                    placeholder, dateFormat, component}]                       }
${#   columns       - 列配置数组 [{dataName, label, isSwitch, columnWidth,       }
${#                    columnFixed}]                                             }
${# ============================================================================}
<script setup lang="tsx">
//此文件为AutoCode编译器生成，请勿手动修改
// ==================== ${commentTitle}（${pageName}）查询列表 ====================
import { BaseButton } from '@/components/button';
import { FormSchema } from '@/components/form';
import { Icon } from '@/components/icon';
import { Table, TableColumn } from '@/components/table';
import { ElTooltip, ElSwitch${if hasConfirmButtons}, ElMessageBox${/if}${if hasTagColumns}, ElTag${/if}${if hasImageColumns}, ElImage${/if} } from 'element-plus';
import { reactive, ref, unref } from 'vue';
import { Search } from '@/components/search';
import { ContentWrap } from '@/components/content_wrap';
import Write from './components/write.vue';
import { Dialog } from '@/components/dialog';
import { ${apiImports} } from '@/api/${apiModule}/${pageName}';
import { uiCrudLogic } from '@/utils/ui_crud_logic';
${if hasLinkButtons}import { useRouter } from 'vue-router';${/if}

// 定义表单引用
const writeRef = ref<ComponentRef<typeof Write>>();
${if hasLinkButtons}const { push } = useRouter();${/if}

// 使用 CRUD Hook
const { crudState, tableRegister, tableState, crudMethods } = uiCrudLogic({
  fetchDataApi: async () => {
    // 构建请求参数，包含分页和搜索参数
    const params = {
      page: tableState.currentPage.value,
      pageSize: tableState.pageSize.value,
      ...unref(crudState.searchParams)
    };
    // 设置查询接口
    const response = await ${queryApi}(params);

    // 将 IResponse 格式转换为 useCrud 期望的格式
    return {
      list: response.data.list || [],
      total: response.data.total || 0
    };
  },

${if !noDelete}  // 设置删除接口
  fetchDelApi: (ids) => ${deleteApi}(ids),

${/if}
${if !noEdit}  // 设置更新接口
  fetchUpdateApi: (data) => ${updateApi}(data)
${/if}
});



// 解构需要的状态和方法
const { dataList, loading, total, currentPage, pageSize } = tableState;
const {
  updateStatusAndTip,
  delData,
  setSearchParams,
  changePage,
  changePageSize,
  addAction,
  rowAction,
  save
} = crudMethods;



// 搜索表单
const searchSchema = reactive<FormSchema[]>([
${each q in queryFields}${if q.isRange}  {
    field: '${q.dataName}Start',
    label: '${q.displayName}开始',
    component: '${q.rangeComp}',
    componentProps: {
      ${if q.isDate}type: '${q.dateFormat}'${else}placeholder: '${q.placeholder}'${/if}
    }
  },
  {
    field: '${q.dataName}End',
    label: '${q.displayName}结束',
    component: '${q.rangeComp}',
    componentProps: {
      ${if q.isDate}type: '${q.dateFormat}'${else}placeholder: '${q.placeholder}'${/if}
    }
  },
${else if q.isSelect}  {
    field: '${q.dataName}',
    label: '${q.displayName}',
    component: 'ApiSelect',
    componentProps: {
      url: '${q.selectUrl}',
      valueField: '${q.selectValueField}',
      labelField: '${q.selectLabelField}'
    }
  },
${else if q.isDate}  {
    field: '${q.dataName}',
    label: '${q.displayName}',
    component: 'DatePicker',
    componentProps: {
      type: '${q.dateFormat}'
    }
  },
${else}  {
    field: '${q.dataName}',
    label: '${q.displayName}',
    component: '${q.component}'${if q.hasPlaceholder},
    componentProps: {
      placeholder: '${q.placeholder}'
    }${/if}
  },
${/if}
${/each}]);

${if hasTagColumns}// tag 列映射表（提前构造，避免每次单元格渲染都重建对象）
const tagMaps: Record<string, Record<string, { text: string; color: string }>> = {
${each col in columns}${if col.isTagDisplay}  '${col.dataName}': ${col.tagItemsMapStr},
${/if}${/each}};

${/if}
// 表格列定义
const tableColumns = reactive<TableColumn[]>([
  {
    field: 'selection',
    type: 'selection'
  },
${each col in columns}
${if col.queryVisible}
${if col.isBooleanSwitch}
  {
    field: '${col.dataName}',
    label: '${col.label}',
${if col.hasColumnWidth}    width: ${col.columnWidth},
${/if}${if col.hasColumnFixed}    fixed: '${col.columnFixed}',
${/if}    slots: {
      default: (data: any) => {
        return (
          <ElSwitch
            modelValue={data.row.${col.dataName} == 1}
            disabled={${col.switchDisabledStr}}
            onChange={() => {
              updateStatusAndTip(data.row);
            }}
            activeText="${col.switchActiveText}"
            inactiveText="${col.switchInactiveText}"
            inlinePrompt={true}
            active-color="#13ce66"
            inactive-color="#ff4949"
          />
        );
      }
    }
  },
${else if col.isTagSwitch}
  {
    field: '${col.dataName}',
    label: '${col.label}',
${if col.hasColumnWidth}    width: ${col.columnWidth},
${/if}${if col.hasColumnFixed}    fixed: '${col.columnFixed}',
${/if}    slots: {
      default: (data: any) => {
        return (
          <ElSwitch
            modelValue={String(data.row.${col.dataName}) == '${col.switchActiveValue}'}
            disabled={${col.switchDisabledStr}}
            onChange={() => {
              updateStatusAndTip(data.row);
            }}
            activeText="${col.switchActiveText}"
            inactiveText="${col.switchInactiveText}"
            inlinePrompt={true}
            active-color="#13ce66"
            inactive-color="#ff4949"
          />
        );
      }
    }
  },
${else if col.isMoneyDisplay}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasDefaultSort},
    sortable: true${/if}${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if},
    slots: {
      default: (data: any) => {
        const v = data.row.${col.dataName};
        if (v == null || v === '') return '';
        const num = Number(v);
        if (isNaN(num)) return v;
        return num.toLocaleString('zh-CN', { minimumFractionDigits: ${col.precision}, maximumFractionDigits: ${col.precision} });
      }
    }
  },
${else if col.isTagDisplay}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasDefaultSort},
    sortable: true${/if}${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if},
    slots: {
      default: (data: any) => {
        const tagMap = ${col.tagItemsMapStr};
        const item = tagMap[String(data.row.${col.dataName})];
        if (!item) return data.row.${col.dataName};
        return <ElTag type={item.color}>{item.text}</ElTag>;
      }
    }
  },
${else if col.isBooleanDisplay}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasDefaultSort},
    sortable: true${/if}${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if},
    slots: {
      default: (data: any) => {
        return data.row.${col.dataName} ? '${col.boolTrueText}' : '${col.boolFalseText}';
      }
    }
  },
${else if col.isImageDisplay}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasDefaultSort},
    sortable: true${/if}${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if},
    slots: {
      default: (data: any) => {
        const src = data.row.${col.dataName};
        return src ? <ElImage src={src} fit="cover" style="width: 50px; height: 50px" preview-src-list={[src]} preview-teleported /> : '';
      }
    }
  },
${else}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasDefaultSort},
    sortable: true${/if}${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if}${if col.hasFormatter},
    formatter: (row: any, column: any, cellValue: any) => {
      ${if col.isFormatterDate}return cellValue ? String(cellValue).substring(0, 10) : '';${/if}${if col.isFormatterStatus}return cellValue ? '启用' : '禁用';${/if}${if col.isFormatterCurrency}return '¥' + Number(cellValue).toFixed(2);${/if}
    }${/if}
  },
${/if}
${/if}
${/each}
  {
    field: 'action',
    label: '操作',
    width: ${actionColumnWidth},
    slots: {
      default: (data: any) => {
        const row = data.row;
        return (
          <>
            ${if !noDetail}<ElTooltip content="详情" placement="top">
              <BaseButton size="small" onClick={() => rowAction(row, 'detail')}>
                <Icon icon="vi-mingcute:eye-line" />
              </BaseButton>
            </ElTooltip>
            ${/if}
            <ElTooltip content="编辑" placement="top">
              <BaseButton size="small" onClick={() => rowAction(row, 'edit')}>
                <Icon icon="vi-mingcute:edit-line" />
              </BaseButton>
            </ElTooltip>
            <ElTooltip content="删除" placement="top">
              <BaseButton size="small" onClick={() => delData(row)}>
                <Icon icon="vi-mingcute:delete-2-line" />
              </BaseButton>
            </ElTooltip>
            ${each btn in rowButtons}<ElTooltip content="${btn.label}" placement="top">
              <BaseButton size="small"${if btn.hasType} type="${btn.type}"${/if}
                onClick={() => onCustomAction(row, '${btn.key}')}>
                ${if btn.hasIcon}<Icon icon="${btn.icon}" />${/if}
              </BaseButton>
            </ElTooltip>
            ${/each}
          </>
        );
      }
    }
  }
]);

${if hasCustomButtons}
// ── 自定义按钮动作处理 ──
// row 为 null 表示工具栏按钮触发，有值表示行操作列按钮触发
const onCustomAction = (row: any, actionKey: string) => {
${each btn in ajaxButtons}  if (actionKey === '${btn.key}') {
    ${btn.apiName}(row ? row.id : undefined);
    return;
  }
${/each}
${each btn in confirmButtons}  if (actionKey === '${btn.key}') {
    ElMessageBox.confirm('${btn.confirmText}', '提示', { type: 'warning' })
      .then(() => ${btn.apiName}(row ? row.id : undefined));
    return;
  }
${/each}
${each btn in dialogButtons}  if (actionKey === '${btn.key}') {
    ${btn.key}Visible.value = true;
    ${btn.key}Row.value = row;
    return;
  }
${/each}
${each btn in linkButtons}  if (actionKey === '${btn.key}') {
    push('${btn.linkPath}');
    return;
  }
${/each}
};
${/if}
${if hasDialogButtons}
${each btn in dialogButtons}
// ── ${btn.label} 对话框 ──
const ${btn.key}Visible = ref(false);
const ${btn.key}Row = ref<any>(null);
const ${btn.key}Form = reactive({
${each f in btn.dialogFields}  ${f.fieldName}: '',
${/each}});
const ${btn.key}FormRef = ref();
const ${btn.key}FormRegister = (form: any) => {
  ${btn.key}FormRef.value = form;
};
const ${btn.key}Schema = reactive<FormSchema[]>([
${each f in btn.dialogFields}  {
    field: '${f.fieldName}',
    label: '${f.label}',
    component: '${f.component}'${if f.hasProps},
    componentProps: { ${f.componentPropsStr} }${/if}${f.requiredStr}
  },
${/each}]);
${/each}

// 自定义对话框提交
const onCustomDialogSubmit = async (actionKey: string) => {
${each btn in dialogButtons}  if (actionKey === '${btn.key}') {
    await ${btn.dialogApi}({ id: ${btn.key}Row.value?.id, ...${btn.key}Form });
    ${btn.key}Visible.value = false;
    Object.keys(${btn.key}Form).forEach((k: string) => (${btn.key}Form as any)[k] = '');
    return;
  }
${/each}
};
${/if}

function closeDialog() {
  crudState.dialogVisible = false;
  crudState.actionType = null;
  crudState.currentRow = null;
}

</script>

<template>
  <ContentWrap>
    <Search :schema="searchSchema as any" @reset="setSearchParams" @search="setSearchParams" />

    <div class="mb-10px">
      <BaseButton type="primary" @click="addAction">{{ '新增' }}</BaseButton>
      <BaseButton :loading="crudState.delLoading" type="danger" @click="delData()">删除</BaseButton>
      ${each btn in toolbarButtons}<BaseButton${if btn.hasType} type="${btn.type}"${/if}
        @click="onCustomAction(null, '${btn.key}')">
        ${if btn.hasIcon}<Icon icon="${btn.icon}" />${/if}
        {{ '${btn.label}' }}
      </BaseButton>
      ${/each}
    </div>

    <Table
      v-model:pageSize="pageSize"
      v-model:currentPage="currentPage"
      @update:current-page="changePage"
      @update:page-size="changePageSize"
      :pagination="{ total: total }"
      :columns="tableColumns"
      :data="dataList"
      :loading="loading"
      @register="tableRegister"${if hasTableDefaultSort}
      :default-sort="{ prop: '${defaultSortField}', order: '${defaultSortOrder}' }"${/if}
    />
  </ContentWrap>

  <Dialog v-model="crudState.dialogVisible" :title="crudState.dialogTitle" @closed="closeDialog">

    <Write
      v-if="crudState.actionType === 'edit' || crudState.actionType === 'add' || crudState.actionType === 'detail'"
      ref="writeRef"
      :current-row="crudState.currentRow"
      :action-type="crudState.actionType"
    />

    <template #footer>
      <BaseButton
        v-if="crudState.actionType !== 'detail'"
        type="primary"
        :loading="crudState.saveLoading"
        @click="save(() => unref(writeRef))"
      >
        {{ '保存' }}
      </BaseButton>
      <BaseButton @click="closeDialog()">{{ '关闭' }}</BaseButton>
    </template>
  </Dialog>
${each btn in dialogButtons}
  <Dialog v-model="${btn.key}Visible" title="${btn.dialogTitle}">
    <Form :schema="${btn.key}Schema" @register="${btn.key}FormRegister" />
    <template #footer>
      <BaseButton type="primary" @click="onCustomDialogSubmit('${btn.key}')">确定</BaseButton>
      <BaseButton @click="${btn.key}Visible = false">取消</BaseButton>
    </template>
  </Dialog>
${/each}
</template>
