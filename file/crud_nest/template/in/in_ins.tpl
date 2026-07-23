${# ============================================================================}
${# InIns 模板：生成 TypeScript InIns 输入类 (NestJS Swagger + class-validator)  }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成客户端插入或修改表时传输的数据格式类，包含：                              }
${#   - @ApiProperty 装饰器（Swagger 文档）                                      }
${#   - @IsXxx 验证装饰器（class-validator）                                     }
${#   - constructor(data?: EnXxx)：从实体构造 InIns                              }
${#   - toEntity()：InIns 转回实体                                               }
${# ============================================================================}
${# 安全保护：只有当输出文件不存在时才生成，防止开发者已修改的代码被覆盖           }
${if !fileExists(outputPath)}
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }

//此代码为AutoCode框架生成，请勿手动修改
${# Swagger API 文档：@ApiProperty 用于生成接口字段说明                          }
import { ApiProperty } from '@nestjs/swagger';
${# 引用对应的实体类                                                             }
import { ${entityClass} } from '../entities/${entityClassFile}';
${# class-validator：请求参数验证装饰器                                          }
import { IsNotEmpty, IsString, IsOptional, IsNumber, IsDateString } from 'class-validator';
${# Coin 类型：处理金额/decimal/浮点数                                           }
import { Coin } from '@/common/tool/coin';
${# 时间工具类                                                                  }
import { ToolTime } from '@/common/tool/tool_time';

${# ── 类声明 ───────────────────────────────────────────────────────────────   }
${# 类名格式：InIns + 表名首字母大写，如 InInsUser                               }
//${tableDesc}实体对应in类，客户端插入或修改表时，传输的数据格式
export class ${insClass} {
  ${# ── 字段循环展开 ────────────────────────────────────────────────────   }
  ${# param.ac 生成的 fields 数组，每个字段（field）包含：                 }
  ${#   .name          字段名                                                }
  ${#   .comment       字段注释（@ApiProperty 的 description）               }
  ${#   .tsType        TypeScript 类型（number/string/Date/Coin/boolean）    }
  ${#   .isPrimary     是否主键                                              }
  ${#   .isCoin        是否 decimal 金额类型                                 }
  ${each field in fields}
  @ApiProperty({ description: '${field.comment}' })
  ${if field.isCoin}@IsString()${else if field.tsType == "number"}@IsNumber()${else if field.tsType == "string"}@IsString()${else if field.tsType == "Date"}@IsDateString()${else if field.tsType == "boolean"}@IsNotEmpty()${/if}
  ${field.name}: ${if field.isCoin}string${else}${field.tsType}${/if};

  ${/each}
  ${# ── constructor：从实体构造 InIns ────────────────────────────────   }
  ${# Coin 字段需要 .toString() 转为字符串，其余直接赋值                    }
  constructor(data?: ${entityClass}) {
    if (!data) return;
${each field in fields}
${if field.isCoin}       this.${field.name} = data.${field.name}.toString();${else}       this.${field.name} = data.${field.name};${/if}
${/each}
  }

  ${# ── toEntity：InIns 转回实体 ─────────────────────────────────────   }
  ${# Coin 字段需要 new Coin() 包装，其余直接赋值                           }
  toEntity(): ${entityClass} {
       const entity = new ${entityClass}();
${each field in fields}
${if field.isCoin}       entity.${field.name} = new Coin(this.${field.name});${else}       entity.${field.name} = this.${field.name};${/if}
${/each}
       return entity;
  }
}
${/if}
