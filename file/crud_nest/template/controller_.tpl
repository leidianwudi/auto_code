${# ============================================================================}
${# Controller_ 模板：生成 TypeScript 控制器基类（请勿手动修改）                 }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成通用的增删改查控制器基类，包含：                                        }
${#   - selectByIn 分页查询接口                                                  }
${#   - selectById 单条查询接口                                                  }
${#   - insert 新增接口                                                          }
${#   - update 更新/新增接口                                                     }
${#   - delete 删除接口                                                          }
${# ============================================================================}
${if !fileExists(outputPath)}

// 此代码为AutoCode框架生成，请勿手动修改
import { Controller, Get, Post, Put, Body, Param, Query, Delete, HttpCode, HttpStatus } from '@nestjs/common';
import { NumberKeyOutEnum, StringKeyOutEnum } from '@/common/tool/out_enum_record';
import { OutBasePage } from '@/common/interface/out_base_page';
import { OutReq } from 'src/common/interface/out_req';
import { HttpBackCode, HttpBackCodeMsg } from '@/common/core/http_back_code';
import { OutBaseId, OutBaseIdsCount } from '@/common/interface/out_base_id';
import { InBaseId } from '@/common/interface/in_base_id';
import { ${serviceClass} } from './${serviceClassFile}';
import { ApiTags, ApiOperation, ApiResponse, ApiParam } from '@nestjs/swagger';
import { ${insClass} } from './in/in_ins_${tableName}';
import { ${selClass} } from './in/in_sel_${tableName}';
import { ${outPageClass} } from './out/out_page_${tableName}';
import { ${entityClass} } from './entities/${entityClassFile}';


/**
 * ${entityClass}实体(${tableDesc})，通用的增删改查控制器基类。
 */
export class ${controllerBaseClass} {
  /**
   * 注入当前控制器依赖的服务实例。
   */
  constructor(protected readonly ${serviceVarName}: ${serviceClass}) {}

  /**
   * 根据参数查询
   */
  @ApiOperation({ summary: '根据参数查询' })
  @ApiResponse({ status: 200, description: '返回分页数据', type: ${entityClass} })
  @HttpCode(HttpStatus.OK)
  @Post('selectByIn')
  async selectByIn(@Body() sel: ${selClass}) {
    const result = await this.${serviceVarName}.selectByIn(sel);
    // 未使用Out_Req对返回结果进行封装, 拦截器会自动进行包装
    return ${outPageClass}.of(result.list, result.total);
  }

  /**
   * 根据ID查询
   */
  @ApiOperation({ summary: '根据ID查询' })
  @ApiParam({ name: 'id', type: Number, description: '${tableDesc}ID' })
  @ApiResponse({ status: 200, description: '返回全部数据', type: ${entityClass} })
  @HttpCode(HttpStatus.OK)
  @Post('selectById')
  async selectById(@Body() body: InBaseId): Promise<${entityClass}> {
    // 未使用Out_Req对返回结果进行封装, 拦截器会自动进行包装
    return await this.${serviceVarName}.selectById(body.id);
  }

  /**
   * 新增记录
   */
  @ApiOperation({ summary: '新增记录' })
  @ApiResponse({ status: 200, description: '返回操作结果消息', type: OutBaseId })
  @HttpCode(HttpStatus.OK)
  @Post('insert')
  async insert(@Body() dto: ${insClass}): Promise<OutReq> {
    const result = await this.${serviceVarName}.insert(dto.toEntity());
    // 使用 OutReq返回，可以控制code，msg, data
    return OutReq.success({id: result});
  }

  /**
   * 更新记录或者插入记录（没有传id则为插入数据）
   */
  @ApiOperation({ summary: '更新/新增记录' })
  @ApiResponse({ status: 200, description: '返回操作结果消息', type: OutBaseId })
  @HttpCode(HttpStatus.OK)
  @Post('update')
  async update(@Body() body: ${insClass} ): Promise<OutReq> {
    let result = null;
    if(!body.id){
      result = await this.${serviceVarName}.insert(body.toEntity());
      // 使用 OutReq返回，可以控制code，msg, data
      return OutReq.success({id: result});
    } else {
      result = await this.${serviceVarName}.update(body.toEntity());
      // 使用 OutReq返回，可以控制code，msg, data
      return OutReq.success({id: body.id});
    }
  }

  /**
   * 删除记录(支持多条删除)
   */
  @ApiOperation({ summary: '删除记录' })
  @ApiResponse({ status: 200, description: '返回操作结果消息', type: OutBaseIdsCount })
  @HttpCode(HttpStatus.OK)
  @Post('delete')
  async delete(@Body() body: { ids: number[] }): Promise<OutBaseIdsCount> {
    const result = await this.${serviceVarName}.delete(body.ids);
    // 未使用Out_Req对返回结果进行封装, 拦截器会自动进行包装
    return OutBaseIdsCount.of(result)
  }

}

${/if}
