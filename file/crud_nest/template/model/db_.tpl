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
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }
//此代码为AutoCode编译器生成，请勿手动修改
${# Repository 基类封装 }
import { RepositorySuper } from "src/common/tool/repository_super";
${# TypeORM 核心类型 }
import { EntityManager, Repository, InsertResult, UpdateResult, DeleteResult } from "typeorm";
${# NestJS 异常处理 }
import { HttpException } from "@nestjs/common";
${# 对应的实体类 }
import { ${entityClass} } from '../entities/${entityClassFile}';
${if hasI18n}
${# i18n 翻译表实体（多语言配置时生成，仓储经 EntityManager 现取）}
import { ${i18nEntityClass} } from '../entities/${i18nEntityClassFile}';
${# i18n 多语言工具（手写公共层 tool_i18n.ts，默认语言兜底用 EnumBaseLangLang.Zh 枚举）}
import { ToolI18n, I18nOpt } from '@/common/tool/tool_i18n';
${/if}
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
${if hasI18n}
  //i18n 翻译表配置（json i18n 节点）；默认语言缺省用 tool_db 的 EnumBaseLangLang.Zh 枚举
  private static i18nOpt: I18nOpt = {
    extKey: '${i18nExtKey}',   // 外部ID字段名
    langKey: '${i18nLangKey}',    // 语言字段名
    fields: [${i18nFieldsStr}], // 翻译字段名
${i18nDefaultLangLine}  };

${/if}
  constructor(db: Repository<${entityClass}>) {
    super(db);
  }

  ${# ── 分页查询 ────────────────────────────────────────────────────────   }
  // 分页查询（manager 可选：传入事务管理器使查询加入事务）
  public async selectByIn(sel: ${selClass}, manager?: EntityManager): Promise<{ list: ${entityClass}[], total: number, sumList?: Record<string, number> | null }> {
    const where = this.getWhereByIn(sel);
    const order = this.getOrderByIn(sel);
    // 可根据实际字段完善查询条件
    const { list, total } = await this.findAndCountSp(
      sel.page,
      sel.pageSize,
      {
        where,
        relations: [${relationsForQuery}], //关联的属性名
        order,    //排序
      },
      manager,
    );
${if hasI18n}
    // i18n：平铺当前语言文本到每行（sel.lang 不传用默认语言，当前语言缺失回退默认语言）
    await ToolI18n.fillList(this.getI18nRepo(manager), list, sel.lang, ${dbBaseClass}.i18nOpt);
${/if}

    return { list, total };
  }

  ${# ── 构建查询条件 ────────────────────────────────────────────────────   }
  // 根据查询参数 sel 构建查询条件
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

  // 根据查询参数 sel 构建排序条件
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
${if hasI18n}

  ${# ── i18n 翻译仓储（hasI18n 时生成；全部多语言逻辑在公共层 tool_i18n.ts）─────────   }
  // i18n 翻译表仓储：经 EntityManager 现取（翻译实体注册在 app.module.ts 全局 entities，
  // 无需 forFeature 注入）；manager 为事务管理器时，翻译表操作加入同一事务
  private getI18nRepo(manager?: EntityManager): Repository<${i18nEntityClass}> {
    return this.getRepository(manager).manager.getRepository(${i18nEntityClass});
  }
${/if}

  ${# ── 基本 CRUD ───────────────────────────────────────────────────────   }
  // 根据id查询单条记录
  public async selectById(id: number, manager?: EntityManager): Promise<${entityClass}> {
    const where: Where<${entityClass}> = ToolDb.getWhere<${entityClass}>();
    where.add('id', id);
    const entity = await this.getRepository(manager).findOne({ where });
${if hasI18n}
    if (entity) {
      // i18n：平铺默认语言文本 + 附带全语言翻译 map（供管理端逐语言编辑）
      await ToolI18n.fillList(this.getI18nRepo(manager), [entity], undefined, ${dbBaseClass}.i18nOpt);
      entity.i18n = await ToolI18n.mapById(this.getI18nRepo(manager), entity.id, ${dbBaseClass}.i18nOpt);
    }
${/if}
    return entity;
  }

  // 新增一条记录
  public async insert(data: ${entityClass}, manager?: EntityManager): Promise<number> {
${if hasI18n}
    // i18n：主表与翻译表同事务写入（manager 已传入时顺序执行，否则自开事务）
    const run = async (mgr: EntityManager): Promise<number> => {
      const res = await mgr.getRepository(${entityClass}).insert(data);
      const newId = res.identifiers[0].id;
      await ToolI18n.upsert(this.getI18nRepo(mgr), newId, data.i18n, ${dbBaseClass}.i18nOpt);
      return newId;
    };
    if (manager) { return await run(manager); }
    return await this.getRepository().manager.transaction(run);
${else}
    const res = await this.getRepository(manager).insert(data);
    return res.identifiers[0].id;//返回id
${/if}
  }

  // 更新一条记录
  public async update(data: ${entityClass}, manager?: EntityManager): Promise<number> {
${if hasI18n}
    // i18n：主表与翻译表同事务更新（翻译行按 (extKey, langKey) upsert）
    const run = async (mgr: EntityManager): Promise<number> => {
      const res = await mgr.getRepository(${entityClass}).update(data.id, data);
      await ToolI18n.upsert(this.getI18nRepo(mgr), data.id, data.i18n, ${dbBaseClass}.i18nOpt);
      return res.affected;
    };
    if (manager) { return await run(manager); }
    return await this.getRepository().manager.transaction(run);
${else}
    const res = await this.getRepository(manager).update(data.id, data);
    return res.affected;//返回更新的记录数
${/if}
  }

  // 删除记录
  public async delete(ids: number[], manager?: EntityManager): Promise<DeleteResult> {
${if hasI18n}
    // i18n：同事务先删翻译行再删主表（外键有 ON DELETE CASCADE 时翻译删 0 行，无害）
    const run = async (mgr: EntityManager): Promise<DeleteResult> => {
      await ToolI18n.removeByExtIds(this.getI18nRepo(mgr), ids, ${dbBaseClass}.i18nOpt);
      return await mgr.getRepository(${entityClass}).delete(ids);
    };
    if (manager) { return await run(manager); }
    return await this.getRepository().manager.transaction(run);
${else}
    return await this.getRepository(manager).delete(ids);
${/if}
  }
}
