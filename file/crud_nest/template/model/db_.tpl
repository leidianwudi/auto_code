${# ============================================================================}
${# Db_ 模板：生成 TypeScript 模型父类（请勿手动修改）                       }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成数据库操作基类，包含：                                                  }
${#   - extends RepositorySuper<EnXxx>（通用 CRUD 封装）                        }
${#   - selectByIn(sel) 分页查询                                                 }
${#   - getWhereByIn(sel) 构建查询条件（精确+模糊）                              }
${#   - getOrderByKey/getOrderByIn 排序逻辑                                      }
${#   - selectById/insert/update/delete 基本 CRUD                                }
${# ============================================================================}
${# 安全保护：只有当输出文件不存在时才生成，防止开发者已修改的代码被覆盖           }
${if !fileExists(outputPath)}
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }

//此代码为AutoCode框架生成，请勿手动修改
${# Repository 基类封装 }
import { RepositorySuper } from "src/common/tool/repository_super";
${# TypeORM 核心类型 }
import { Repository, InsertResult, UpdateResult, DeleteResult  } from "typeorm";
${# NestJS 异常处理 }
import { HttpException } from "@nestjs/common";
${# 对应的实体类 }
import { ${entityClass} } from '../entities/${entityClassFile}';
${# 时间工具 }
import { ToolTime } from 'src/common/tool/tool_time';
${# 对应的 InIns 类 }
import { ${insClass} } from '../in/in_ins_${tableName}';
${# 对应的 InSel 类 }
import { ${selClass} } from '../in/in_sel_${tableName}';
${# 查询工具 }
import { ToolDb, Where, Order } from "@/common/tool/tool_db";

${# ── 类声明 ───────────────────────────────────────────────────────────────   }
/**
 * ${dbBaseClass} 是模型父类，请勿使用此类，请使用 ${dbClass} 类
 */
export class ${dbBaseClass} extends RepositorySuper<${entityClass}> {
  constructor(db: Repository<${entityClass}>) {
       super(db);
  }

  ${# ── 分页查询 ────────────────────────────────────────────────────────   }
  // 分页查询
  public async selectByIn(sel: ${selClass}): Promise<{list: ${entityClass}[], total: number, sumList?: Record<string, number> | null}> {
    const where = this.getWhereByIn(sel);
    const order = this.getOrderByIn(sel);
    // 可根据实际字段完善查询条件
    const { list, total } = await this.findAndCountSp(
      sel.page,
      sel.pageSize,
      {
        where,
        relations: [], //关联的属性名
        order,    //排序
      }
    );
    
    return { list, total };
  }

  ${# ── 构建查询条件 ────────────────────────────────────────────────────   }
  public getWhereByIn(sel: ${selClass}): Where<${entityClass}> {
    const where = ToolDb.getWhere<${entityClass}>();
    // 可根据实际字段完善查询条件
  ${each cond in whereConditions}
    ${cond}
  ${/each}
    return where;
  }

  ${# ── 排序逻辑 ────────────────────────────────────────────────────────   }
  // 返回正排序或者反排序，只有asc和desc两种情况
  private getOrderByKey(key: keyof ${entityClass}): 'ASC' | 'DESC' {
    //定义字段排序
    const res = {${orderByDef}};
    return res[key];
  }

  private getOrderByIn(sel: ${selClass}): Order<${entityClass}> {
    const order: Order<${entityClass}> = ToolDb.getOrder<${entityClass}>();
    if (ToolDb.isNotEmpty(sel.sort)) {
      sel.sort.forEach((item) => {
        const sort = this.getOrderByKey(item as keyof ${entityClass});
        order.add(item as keyof ${entityClass}, sort);
      });
    }
    else
      order.add('${defaultOrderField}', '${defaultOrderDir}');

    return order;
  }

  ${# ── 基本 CRUD ───────────────────────────────────────────────────────   }
  // 根据id查询单条记录
  public async selectById(id: number): Promise<${entityClass}>  {
    return await this.db.findOne({ where: { id } });
  }

  // 新增一条记录
  public async insert(data: ${entityClass}): Promise<number> {
    const res = await this.db.insert(data);
    return res.identifiers[0].id;//返回id
  }

  // 更新一条记录
  public async update(data: ${entityClass}): Promise<number> {
    const res = await this.db.update(data.id, data);
    return res.affected;//返回更新的记录数
  }

  // 删除记录
  public async delete(ids: number[]): Promise<DeleteResult> {
    return await this.db.delete(ids);
  }
}

${/if}
