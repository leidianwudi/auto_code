${# ============================================================================}
${# Controller 模板：生成 TypeScript 控制器子类（可以手动修改）                  }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成 ApiTags+Controller 装饰的控制器子类，继承 Controller_ 父类。          }
${#   开发者可在此类中添加自定义接口，不会被框架覆盖。                            }
${# ============================================================================}

// 此代码为AutoCode框架生成，需要扩展时，可以手动修改
import { Controller, Post, Req } from '@nestjs/common';
import { ApiOperation, ApiResponse, ApiTags } from '@nestjs/swagger';
import { EnumLang } from '@/common/core/enum_common';
import { OutBasePage } from '@/common/interface/out_base_page';
import { ToolLang } from '@/common/tool/tool_lang';
import { OutTypeName } from './out/out_type_name';
import { ${controllerBaseClass} } from './${controllerBaseClassFile}';
import { ${serviceClass} } from './${serviceClassFile}';
import { Public } from '@/auth/public.decorator';

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
