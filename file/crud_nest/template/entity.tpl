${# === 实体模板：生成 TypeScript 实体类 ===}
${# 仅当输出文件不存在时才生成，避免覆盖已有代码}
${if !fileExists(outputPath)}
${# 输出文件头部注释，会原样写入目标文件}
// 此代码为AutoCode框架生成，请勿手动修改
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
import { ApiProperty } from '@nestjs/swagger';
import { ToolStr } from '@/common/tool/tool_str';
import { Coin, COIN_PRECISION, COIN_SCALE } from '@/common/tool/coin';
import { CoinTransformer } from '@/common/tool/coin_transformer';
import { Transform } from 'class-transformer';
${# 动态导入列表}
${each imports}${imports};
${/each}a

${# 实体声明开始}
// ${tableDesc} 实体
@Entity('${tableName}')
export class ${entityClass} {${# 以下循环展开所有字段，主键走不同分支}${each fields}${if isPrimary}
  @ApiProperty({ description: '${comment}' })
  @PrimaryGeneratedColumn()
  ${name}: ${tsType};${else}${if isCoin}
  @ApiProperty({ description: '${comment}' })
  @Column(${columnOptions})
  @Transform(({ value }) => value.toJSON())
  ${name}: ${tsType};${else}
  @ApiProperty({ description: '${comment}' })
  @Column(${columnOptions})
  ${name}: ${tsType};${/if}${/if}${/each}
${# 关系字段声明}
${each relations}
  @${decoratorType}(() => ${targetEntity}, (${propertyName}) => ${propertyName}.${inverseProperty})
  ${propertyName}: ${targetEntity}[];${/each}

  static getTableName() {
    return '${tableName}';
  }
}
${/if}