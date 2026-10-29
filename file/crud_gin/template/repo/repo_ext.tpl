${# ============================================================================}
${# Repo 手写扩展件骨架：data/{dataDir}/{tableName}_repo.go        }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                       }
${#   - 与生成物 {tableName}_repo_.go（每次覆盖）同包；自定义查询/统计方法写这里  }
${#   - 方法首参 db *gorm.DB：传 common.DB 走独立查询，传 tx 进入事务             }
${# 数据来源：pkgName/repoClass/enClass                                          }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────────
// 为 ${repoClass} 追加自定义查询/统计方法（Go 同包多文件，与 ${tableName}_repo_.go 同包）：
//   1. 生成物 ${tableName}_repo_.go 每次重新生成会被覆盖，请勿手改
//   2. 本文件仅首次生成骨架；自定义方法直接定义为 ${repoClass} 的方法，
//      禁止与生成物同名方法（重复定义编译错误）
//   3. 方法首参 db *gorm.DB：传 common.DB 走独立查询，传 tx 进入事务（与生成物一致）
//   4. 使用第三方包时自行补 import 块
// ─────────────────────────────────────────────────────────────────────
package ${pkgName}

// （示例）按唯一键查询（按需取消注释并补 import，Col 常量见生成物）：
// func (r *${repoClass}) SelectByAccount(db *gorm.DB, account string) (*${enClass}, error) {
// 	var entity ${enClass}
// 	if err := common.WhereEq(db, ColAccount, account).First(&entity).Error; err != nil {
// 		return nil, err
// 	}
// 	return &entity, nil
// }
${/if}
