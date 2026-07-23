${# ============================================================================}
${# Module 模板：生成 TypeScript NestJS 模块类（可以手动修改）                  }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 @Module 装饰的 NestJS 模块类，注册实体、控制器和服务。                }
${#   开发者可在此类中添加自定义导入和提供者，不会被框架覆盖。                    }
${# ============================================================================}

// 此代码为 AutoCode 框架生成，需要扩展时，可以手动修改
import { Module } from '@nestjs/common';
import { TypeOrmModule } from '@nestjs/typeorm';
import { ${serviceClass} } from './${serviceClassFile}';
import { ${controllerClass} } from './${controllerClassFile}';
import { ${entityClass} } from './entities/${entityClassFile}';
import { ${dbClass} } from './model/${dbClassFile}';

// 注册当前模块依赖的实体、控制器和服务
@Module({
  imports: [TypeOrmModule.forFeature([${entityClass}])],
  controllers: [${controllerClass}],
  providers: [${serviceClass}, ${dbClass}],
  exports: [${serviceClass}, ${dbClass}],
})
// 对外提供当前业务模块，供其他模块按需引入
export class ${modelBaseName}Module {}
