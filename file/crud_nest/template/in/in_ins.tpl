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
${# ── 头部：注释和静态 import ──────────────────────────────────────────────   }
//此代码为AutoCode编译器生成，请勿手动修改
${# Swagger API 文档：@ApiProperty 用于生成接口字段说明                          }
import { ApiProperty } from '@nestjs/swagger';
${# 引用对应的实体类                                                             }
import { ${entityClass} } from '../entities/${entityClassFile}';
${if hasI18n}
${# i18n 翻译实体（多语言翻译数据 map 的值类型）}
import { ${i18nEntityClass} } from '../entities/${i18nEntityClassFile}';
${/if}
${# class-validator：请求参数验证装饰器（所有属性均可空，仅保留 IsOptional）      }
import { IsOptional } from 'class-validator';
${# Coin 类型：处理金额/decimal/浮点数                                           }
import { Coin } from '@/common/tool/coin';
${# 时间工具类                                                                  }
import { ToolTime } from '@/common/tool/tool_time';

${# ── 类声明 ───────────────────────────────────────────────────────────────   }
${# 类名格式：InIns + 表名首字母大写，如 InInsUser                               }
//${tableDesc}(${entityClass})实体对应in类，客户端插入或修改表时，传输的数据格式
${if hasI18n}
${# 多语言翻译 map 类型别名：key 为语言码，value 为翻译字段子集；ext_id/langKey 由服务端覆盖}
/** 多语言翻译数据：key 为语言码（如 zh / en），value 为 ${i18nEntityClass} 的翻译字段；ext_id/lang 由服务端覆盖，客户端无需传 */
export type ${insClass}I18nMap = Record<string, Partial<${i18nEntityClass}>>;
${/if}
export class ${insClass} {
  ${# ── 字段循环展开 ────────────────────────────────────────────────────   }
  ${# param.ac 生成的 insFields 数组（排除自动维护的时间列），每个字段（field）包含：    }
  ${#   .name          字段名                                                }
  ${#   .comment       字段注释（@ApiProperty 的 description）               }
  ${#   .tsType        TypeScript 类型（number/string/Date/Coin/boolean）    }
  ${#   .isPrimary     是否主键                                              }
  ${#   .isCoin        是否 decimal 金额类型                                 }
  ${#   .isNullable    是否可空（对应实体表列是否允许 NULL）                 }
  ${# 注意：仅当列允许 NULL 时，才加上 required: false + @IsOptional + ?；  }
  ${#       非空列不加任何修饰（不加 IsNumber/IsString 等类型限制校验）      }
  ${each field in insFields}
  ${if field.isNullable}
  @ApiProperty({ description: '${field.comment}', required: false })
  @IsOptional()
  ${field.name}?: ${if field.isCoin}string${else}${field.tsType}${/if};
  ${else}
  @ApiProperty({ description: '${field.comment}' })
  ${field.name}: ${if field.isCoin}string${else}${field.tsType}${/if};
  ${/if}

  ${/each}
  ${# ── i18n 翻译 map 字段（hasI18n 时生成）──────────────────────────   }
  ${# insert/update 时随主表数据一起提交：{ zh: {name..}, en: {name..} }      }
  ${if hasI18n}
  @ApiProperty({ description: '多语言翻译数据，如 { zh: { name: "中文名" }, en: { name: "Name" } }；ext_id/lang 由服务端覆盖', required: false, type: Object })
  @IsOptional()
  i18n?: ${insClass}I18nMap;

  ${/if}
  ${# ── constructor：从实体构造 InIns ────────────────────────────────   }
  ${# Coin 字段需要 .toString() 转为字符串，其余直接赋值                    }
  constructor(data?: ${entityClass}) {
    if (!data) return;
${each field in insFields}
${if field.isCoin}       this.${field.name} = data.${field.name}.toString();${else}       this.${field.name} = data.${field.name};${/if}
${/each}
${if hasI18n}    this.i18n = data.i18n;
${/if}  }

  ${# ── toEntity：InIns 转回实体 ─────────────────────────────────────   }
  ${# Coin 字段需要 new Coin() 包装，其余直接赋值                           }
  toEntity(): ${entityClass} {
       const entity = new ${entityClass}();
${each field in insFields}
${if field.isCoin}       entity.${field.name} = Coin.getCoinByStr(this.${field.name});${else}       entity.${field.name} = this.${field.name};${/if}
${/each}
${if hasI18n}       entity.i18n = this.i18n;
${/if}       return entity;
  }
}
