${# ============================================================================}
${# OutPage 模板：生成 TypeScript 分页输出类（请勿手动修改）                    }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成分页查询的输出类型，继承 OutBasePage<EnXxx>。                           }
${#   用于 API 返回分页数据时的类型定义。                                        }
${# ============================================================================}
${if !fileExists(outputPath)}

//此代码为AutoCode框架生成，请勿手动修改
import { ApiProperty } from '@nestjs/swagger';
import { OutBasePage } from 'src/common/interface/out_base_page';
import { ${entityClass} } from '../entities/${entityClassFile}';

/**
 * 返回 ${tableDesc}(${entityClass}) 的分页数据
 */
export class ${outPageClass} extends OutBasePage<${entityClass}> {}

${/if}
