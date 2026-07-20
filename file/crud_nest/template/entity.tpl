${# ============================================================================}
${# 实体模板：生成 TypeScript 实体类 (TypeORM + NestJS Swagger 格式)     }
${# ----------------------------------------------------------------------------}
${# 执行流程：                                                                  }
${#   1. news_main.ac 读取数据库表结构和 JSON 配置                               }
${#   2. param.ac 将原始数据转换为模板需要的字段格式 (fields、relations 等)       }
${#   3. 本模板通过 ${变量名}、${each ...}、${if ...} 等标记展开生成代码         }
${#   4. renderTpl 渲染结果 → 写入 xxx.entity.ts                                }
${# ============================================================================}
${# 安全保护：只有当输出文件不存在时才生成，防止开发者已修改的代码被覆盖           }
${if !fileExists(outputPath)}
${# ── 头部：输出到 TS 文件的注释和静态 import ──────────────────────────────   }
${# 注意：下面以 "//" 开头的行会原样输出到生成的 TS 文件，不是模板注释            }
// 此代码为AutoCode框架生成，请勿手动修改

${# TypeORM 装饰器：@Entity @Column @PrimaryGeneratedColumn 等                 }
import {
  Entity,
  PrimaryGeneratedColumn,
  Column,
  JoinColumn,
  OneToOne,
  ManyToOne,
  OneToMany,
  ManyToMany,
  CreateDateColumn,
  UpdateDateColumn,
} from 'typeorm';

${# Swagger API 文档：@ApiProperty 用于生成接口字段说明                          }
import { ApiProperty } from '@nestjs/swagger';
${# 工具类：字符串处理                                                           }
import { ToolStr } from '@/common/tool/tool_str';
${# Coin 类型：处理金额/decimal/浮点数，精度和缩放由常量统一管理                 }
import { Coin, COIN_PRECISION, COIN_SCALE } from '@/common/tool/coin';
${# Coin 类型的 TypeORM 转换器：数据库 decimal <-> 业务层 Coin 对象             }
import { CoinTransformer } from '@/common/tool/coin_transformer';
${# class-transformer：Coin 转 JSON 用于 API 响应序列化                         }
import { Transform } from 'class-transformer';
${# ── 动态 import：由 param.ac 处理生成，用于关联实体类 ───────────────────   }
${# 例如：import { EnNewsCategory } from './en_news_category' }
${each imp in imports}
${imp};
${/each}

${# ── 实体类声明 ───────────────────────────────────────────────────────────   }
${# @Entity('table_name') 标记这是一个数据库实体类，TypeORM 会根据它建表/建约束  }
// ${tableDesc} 实体
@Entity('${tableName}')
export class ${entityClass} {
  ${# ── 字段循环展开 ────────────────────────────────────────────────────   }
  ${# param.ac 生成的 fields 数组，每个字段（field）包含：                 }
  ${#   .name          字段名                                                }
  ${#   .comment       字段注释（@ApiProperty 的 description）               }
  ${#   .tsType        TypeScript 类型（number/string/Date/Coin/boolean）    }
  ${#   .isPrimary     是否主键 → @PrimaryGeneratedColumn                    }
  ${#   .isCoin        是否 decimal → @Column + @Transform + CoinTransformer }
  ${#   .columnOptions @Column 的完整参数字符串（由 param.getColumnOptions 生成）}
  ${each field in fields}
    ${if field.isPrimary}
  @ApiProperty({ description: '${field.comment}' })
  @PrimaryGeneratedColumn()
  ${field.name}: ${field.tsType};
  
    ${else if field.isCoin}   
  @ApiProperty({ description: '${field.comment}' })
  @Column(${field.columnOptions})
  @Transform(({ value }) => value.toJSON())
  ${field.name}: ${field.tsType};
  
    ${else}
  @ApiProperty({ description: '${field.comment}' })
  @Column(${field.columnOptions})
  ${field.name}: ${field.tsType};
  
    ${/if}
  ${/each}
  ${# ── 关联关系：由 param.ac 生成的 relations 数组 ─────────────────────   }
  ${# 每个关系（rel）包含：                                                         }
  ${#   .decoratorType     OneToOne / ManyToOne / OneToMany / ManyToMany            }
  ${#   .targetEntity      目标实体类名，如 EnNewsCategory                          }
  ${#   .propertyName      本端属性名                                               }
  ${#   .inverseProperty   对方属性名（用于双向关系）                               }
  ${each rel in relations}
  @${rel.decoratorType}(() => ${rel.targetEntity}, (${rel.propertyName}) => ${rel.propertyName}.${rel.inverseProperty})
  ${rel.propertyName}: ${rel.targetEntity}[];
  ${/each}
  
  ${# ── 辅助方法：返回数据库表名，用于查询/联表时拼接 SQL ──────────   }
  static getTableName() {
    return '${tableName}';
  }
}
${/if}