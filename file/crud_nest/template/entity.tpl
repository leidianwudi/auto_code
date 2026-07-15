${if !fileExists(outputPath)}
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
${each imports}${imports}
${/each}

// ${tableDesc}(${tableName}) 实体
@Entity('${tableName}')
export class ${entityClass} {

${each fields}
${if isPrimary}
  @ApiProperty({ description: '${comment}' })
  @PrimaryGeneratedColumn()
  ${name}: ${tsType};
${else}
${if isCoin}
  @ApiProperty({ description: '${comment}' })
  @Column(${columnOptions})
  @Transform(({ value }) => value.toJSON())
  ${name}: ${tsType};
${else}
  @ApiProperty({ description: '${comment}' })
  @Column(${columnOptions})
  ${name}: ${tsType};
${/if}
${/if}
${/each}

${each relations}
	@${decoratorType}(() => ${targetEntity}, (${propertyName}) => ${propertyName}.${inverseProperty})
	${propertyName}: ${targetEntity}[];

${/each}

  static getTableName() {
    return '${tableName}';
  }
}
${/if}