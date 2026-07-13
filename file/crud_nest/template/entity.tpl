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
import { EnSportType } from 'src/sport/sport_type/entities/en_sport_type';
import { EnSportHonorPlayer } from 'src/sport/honor/sport_honor_player/entities/en_sport_honor_player';
import { EnSportHonorTeam } from 'src/sport/honor/sport_honor_team/entities/en_sport_honor_team';

// 荣誉名称数据(sport_honor) 实体
@Entity('sport_honor')
export class ${modelName} {
${each fields}  ${name}: ${type};
${/each}}

${if hasService}export class ${modelName}Service {
  static async getAll(): Promise<${modelName}[]> {
    return fetch('/api/${modelName.toLowerCase}').then(r => r.json());
  }
}

${/if}
${/if}