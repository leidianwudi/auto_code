${# ============================================================================}
${# InSel 模板：生成 TypeScript InSel 查询输入类                              }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成客户端查询分页数据时传输的数据格式类，包含：                              }
${#   - extends In_BasePage（分页基类）                                          }
${#   - @ApiProperty + @IsOptional + @IsXxx 验证装饰器                           }
${#   - 固定的 sort 字段（可排序字段列表）                                        }
${# ============================================================================}
${# 安全保护：只有当输出文件不存在时才生成，防止开发者已修改的代码被覆盖           }
${if !fileExists(outputPath)}
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }

//此代码为AutoCode框架生成，请勿手动修改
${# 分页基类 }
import { In_BasePage } from 'src/common/interface/in_base_page';
${# class-validator：请求参数验证装饰器                                          }
import { IsOptional, IsInt, IsString, IsBoolean } from 'class-validator';
${# Swagger API 文档：@ApiProperty 用于生成接口字段说明                          }
import { ApiProperty } from '@nestjs/swagger';
${# class-transformer：类型转换                                                  }
import { Type } from 'class-transformer';

${# ── 类声明 ───────────────────────────────────────────────────────────────   }
${# 类名格式：InSel + 表名首字母大写，如 InSelUser                               }
${# 继承 In_BasePage 提供分页参数（pageNum、pageSize 等）                        }
//${tableDesc}实体对应in类，客户端查询分页数据时，传输的数据格式
export class ${selClass} extends In_BasePage {
  ${# ── 查询字段循环展开 ──────────────────────────────────────────────   }
  ${# selFields 数组由 news_main.ac 从 selCols + selColsLike + 主键 构建  }
  ${# 每个字段（field）包含：                                             }
  ${#   .name          字段名                                            }
  ${#   .comment       字段注释                                          }
  ${#   .tsType        TypeScript 类型（number/string/Date/Coin/boolean） }
  ${#   .isPrimary     是否主键                                          }
  ${#   .isCoin        是否 decimal 金额类型                             }
  ${#   .isLike        是否模糊查询字段                                  }
  ${each field in selFields}
  @ApiProperty({ description: '${field.comment}', required: false })
  @IsOptional()
${if field.isPrimary}@IsInt({ message: "${field.name}必须是整数" })
  @Type(() => Number)${else if field.tsType == "number"}@IsInt({ message: "${field.name}必须是整数" })
  @Type(() => Number)${else if field.tsType == "string"}@IsString()${else if field.tsType == "boolean"}@IsBoolean()${/if}
  ${field.name}?: ${if field.tsType == "Coin"}string${else}${field.tsType}${/if};

  ${/each}
  ${# ── 固定的 sort 字段 ────────────────────────────────────────────   }
  ${# description 列出所有可排序字段，来自 JSON 配置的 selColsSort           }
  @ApiProperty({ description: '${sortDesc}', required: false })
  @IsOptional()
  @Type(() => Array)
  sort?: string[];
}
${/if}
