${# ============================================================================}
${# Service 模板：生成 TypeScript 服务子类（可以手动修改）                      }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 Injectable 装饰的服务子类，继承 Service_ 父类。                      }
${#   开发者可在此类中添加自定义业务方法，不会被框架覆盖。                        }
${# ============================================================================}

// 此代码为AutoCode框架生成，需要扩展时，可以手动修改
import { Injectable } from '@nestjs/common';
import { InjectRepository } from '@nestjs/typeorm';
import { Repository } from 'typeorm';
import { ${entityClass} } from './entities/${entityClassFile}';
import { ${serviceBaseClass} } from './${serviceBaseClassFile}';

/**
 * ${entityClass} 实体(${tableDesc})，服务类。
 */
@Injectable()
export class ${serviceClass} extends ${serviceBaseClass} {

  constructor(
    @InjectRepository(${entityClass})
    db: Repository<${entityClass}>,
  ) {
    super(db);
  }

}
