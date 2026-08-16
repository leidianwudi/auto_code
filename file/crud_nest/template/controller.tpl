${# ============================================================================}
${# Controller 模板：生成 TypeScript 控制器子类（可以手动修改）                  }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 ApiTags+Controller 装饰的控制器子类，继承 Controller_ 父类。          }
${#   开发者可在此类中添加自定义接口，不会被框架覆盖。                            }
${# ============================================================================}
${# 安全保护：只有当输出文件不存在时才生成，防止开发者已修改的代码被覆盖           }
${if !fileExists(outputPath)}
// 此代码为AutoCode框架生成，需要扩展时，可以手动修改
import { Controller } from '@nestjs/common';
import { ApiTags } from '@nestjs/swagger';
import { ${controllerBaseClass} } from './${controllerBaseClassFile}';
import { ${serviceClass} } from './${serviceClassFile}';

/**
 * ${entityClass}实体(${tableDesc})，控制器类。
 */
@ApiTags('${modelBaseName}')
@Controller('${tableName}')
export class ${controllerClass} extends ${controllerBaseClass} {
  constructor(service: ${serviceClass}) {
    super(service);
  }

}
${/if}
