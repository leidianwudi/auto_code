${# ============================================================================}
${# Db 模板：生成 TypeScript 模型子类（可以手动修改）                        }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 Injectable 装饰的数据库操作子类，继承 Db_ 父类。                     }
${#   开发者可在此类中添加自定义查询方法，不会被框架覆盖。                        }
${# ============================================================================}

//此代码为AutoCode框架生成，可以手动修改
${# TypeORM 核心类型 }
import { EntityManager, Repository } from "typeorm";
${# 对应的实体类 }
import { ${entityClass} } from '../entities/${entityClassFile}';
${# 父类 }
import { ${dbBaseClass} } from './${dbBaseClassFile}';
${# NestJS Injectable 装饰器 }
import { Injectable } from "@nestjs/common";
${# NestJS TypeORM 注入 }
import { InjectRepository } from "@nestjs/typeorm";
${# 查询构建器 }
import { ToolQueryBuilder } from '@/common/tool/tool_query_builder';

/*
 * ${dbClass} 是模型子类,请使用此类;
 */
@Injectable()
export class ${dbClass} extends ${dbBaseClass} {

  constructor(@InjectRepository(${entityClass}) dbRepo: Repository<${entityClass}>) {
       super(dbRepo);
  }


}
