${# ============================================================================}
${# index.tpl — Vue3 后台管理列表页模板                                          }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   根据 .jsonvue 配置生成 Vue3 列表页（index.vue），包含：                    }
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
//此代码为AutoCode框架生成，请勿手动修改
<script setup lang="tsx">
import { BaseButton } from '@/components/button';
import { FormSchema } from '@/components/form';
import { Icon } from '@/components/icon';
import { Table, TableColumn } from '@/components/table';
import { ElTooltip, ElSwitch } from 'element-plus';
import { reactive, ref, unref } from 'vue';
import { Search } from '@/components/search';
import { ContentWrap } from '@/components/content_wrap';
import Write from './components/write.vue';
import { Dialog } from '@/components/dialog';
import { ${apiImports} } from '@/api/${apiModule}';
import { uiCrudLogic } from '@/utils/ui_crud_logic';

// 定义表单引用
const writeRef = ref<ComponentRef<typeof Write>>();

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
${if !noDelete}
  // 设置删除接口
  fetchDelApi: (ids) => ${deleteApi}(ids),
${/if}
${if !noEdit}
  // 设置更新接口
  fetchUpdateApi: (data) => ${updateApi}(data),
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
${each q in queryFields}${if q.isSelect}  {
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

// 表格列定义
const tableColumns = reactive<TableColumn[]>([
  {
    field: 'selection',
    type: 'selection'
  },
${each col in columns}
${if col.isSwitch}
  {
    field: '${col.dataName}',
    label: '${col.label}',
${if col.hasColumnWidth}    width: ${col.columnWidth},
${/if}${if col.hasColumnFixed}    fixed: '${col.columnFixed}',
${/if}    slots: {
      default: (data: any) => {
        return (
          <ElSwitch
            modelValue={data.row.${col.dataName} === 1}
            onChange={() => {
              updateStatusAndTip(data.row);
            }}
            activeText="启用"
            inactiveText="禁用"
            inlinePrompt={true}
            active-color="#13ce66"
            inactive-color="#ff4949"
          />
        );
      }
    }
  },
${else}
  {
    field: '${col.dataName}',
    label: '${col.label}'${if col.hasColumnWidth},
    width: ${col.columnWidth}${/if}${if col.hasColumnFixed},
    fixed: '${col.columnFixed}'${/if}
  },
${/if}
${/each}
  {
    field: 'action',
    label: '操作',
    width: 120,
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
          </>
        );
      }
    }
  }
]);

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
      @register="tableRegister"
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
</template>
