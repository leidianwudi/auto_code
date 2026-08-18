${# ============================================================================}
${# InSel 模板：生成 TypeScript InSel 查询输入类                              }
${# ----------------------------------------------------------------------------}
${# 作用：                                                                       }
${#   生成客户端查询分页数据时传输的数据格式类，包含：                              }
${#   - extends In_BasePage（分页基类）                                          }
${#   - @ApiProperty + @IsOptional + @IsXxx 验证装饰器                           }
${#   - 固定的 sort 字段（可排序字段列表）                                        }
${# ============================================================================}
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }
//此代码为AutoCode框架生成，请勿手动修改
${# 分页基类 }
import { In_BasePage } from 'src/common/interface/in_base_page';
${# class-validator：请求参数验证装饰器（查询参数均可空，仅保留 IsOptional）      }
import { IsOptional } from 'class-validator';
${# Swagger API 文档：@ApiProperty 用于生成接口字段说明                          }
import { ApiProperty } from '@nestjs/swagger';
${# class-transformer：类型转换                                                  }
import { Type } from 'class-transformer';

${# ── 类声明 ───────────────────────────────────────────────────────────────   }
${# 类名格式：InSel + 表名首字母大写，如 InSelUser                               }
${# 继承 In_BasePage 提供分页参数（pageNum、pageSize 等）                        }
//${tableDesc}(${entityClass})实体对应in类，客户端查询分页数据时，传输的数据格式
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
  ${# 注意：查询参数均可空，不加 IsString/IsInt 等类型限制校验，只保留    }
  ${#       IsOptional 与类型转换 Type（保证从 query 传入的字符串正确转型）}
  ${each field in selFields}
  @ApiProperty({ description: '${field.comment}', required: false })
  @IsOptional()
  ${if field.tsType == "number"}
  @Type(() => Number)
  ${else if field.tsType == "boolean"}
  @Type(() => Boolean)
  ${else if field.tsType == "Date"}
  @Type(() => Date)
  ${/if}
  ${field.name}?: ${if field.tsType == "Coin"}string${else}${field.tsType}${/if};

  ${/each}
  ${# ── 固定的 sort 字段 ────────────────────────────────────────────   }
  ${# description 列出所有可排序字段，来自 JSON 配置的 selColsSort           }
  @ApiProperty({ description: '${sortDesc}', required: false })
  @IsOptional()
  @Type(() => Array)
  sort?: string[];
}
