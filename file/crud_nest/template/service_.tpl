${# ============================================================================}
${# Service_ 模板：生成 TypeScript 服务基类（请勿手动修改）                     }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成通用的增删改查服务基类，包含：                                          }
${#   - 注入 Repository 和初始化 Db 类                                           }
${#   - selectByIn 分页查询                                                      }
${#   - selectById 单条查询                                                      }
${#   - insert / update / delete 增删改                                          }
${# ============================================================================}
${if !fileExists(outputPath)}

// 此代码为AutoCode框架生成，请勿手动修改
import { Injectable } from '@nestjs/common';
import { InjectRepository } from '@nestjs/typeorm';
import { Repository, InsertResult, UpdateResult, Like, FindOptionsWhere, Between, MoreThanOrEqual, LessThanOrEqual } from 'typeorm';
import { HttpException } from '@nestjs/common';
import { OutBasePage } from '@/common/interface/out_base_page';
import { ${entityClass} } from './entities/${entityClassFile}';
import { ${insClass} } from './in/in_ins_${tableName}';
import { ${selClass} } from './in/in_sel_${tableName}';
import { ${outPageClass} } from './out/out_page_${tableName}';
import { ${dbClass} } from './model/${dbClassFile}';

/**
 * ${entityClass} 实体(${tableDesc})，通用的增删改查服务基类。
 */
export class ${serviceBaseClass} {
  /**
   * 当前业务对应的数据访问对象。
   */
  protected ${dbVarName}: ${dbClass};
  /**
   * 注入实体仓储，并初始化数据访问对象。
   */
  constructor(
    protected db: Repository<${entityClass}>,
  ) {
    this.${dbVarName} = new ${dbClass}(db);
  }

  // 分页查询列表数据，返回数据列表和总记录数
  async selectByIn(sel: ${selClass}): Promise<{ list: ${entityClass}[]; total: number }> {
    const { list, total } = await this.${dbVarName}.selectByIn(sel);
    return { list, total};
  }

  // 根据id查询单条记录
  async selectById(id: number): Promise<${entityClass}> {
    return await this.${dbVarName}.selectById(id);
  }

  // 新增一条记录
  async insert(dto: ${entityClass}): Promise<number> {
    return await this.${dbVarName}.insert(dto);
  }

  // 更新一条记录
  async update(dto: ${entityClass}): Promise<number> {
    return await this.${dbVarName}.update(dto);
  }

  // 删除一条或多条记录
  async delete(ids: number[]): Promise<number> {
    const result = await this.${dbVarName}.delete(ids);
    return result.affected;
  }
}

${/if}
