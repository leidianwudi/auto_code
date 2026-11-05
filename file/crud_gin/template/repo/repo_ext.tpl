${# ============================================================================}
${# Repo 手写扩展件骨架：data/{dataDir}/{tableName}_repo.go        }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                       }
${#   - 与生成物 {tableName}_repo_.go（每次覆盖）同包；自定义查询/统计方法写这里  }
${#   - Repo 自带连接，方法不再接收 db；事务用 r.Tx(tx) 派生新实例                 }
${#   - 更新约定：SelectById → TakeSnapshot() → 改字段 → Update(e)                }
${# 数据来源：pkgName/repoClass/enClass                                          }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────────
// 为 ${repoClass} 追加自定义查询/统计方法（Go 同包多文件，与 ${tableName}_repo_.go 同包）：
//   1. 生成物 ${tableName}_repo_.go 每次重新生成会被覆盖，请勿手改
//   2. 本文件仅首次生成骨架；自定义方法直接定义为 ${repoClass} 的方法，
//      禁止与生成物同名方法（重复定义编译错误）
//   3. 方法不接收 db：生成物内部统一用 r.db；需要事务时用 r.Tx(tx) 派生新对象，
//      绝不写 r.db = tx（原对象会被并发请求复用，会串事务）
//   4. 更新实体统一走快照 diff：SelectById 取实体 → e.TakeSnapshot() → 改字段 → Update(e)；
//      不要直接拼 UPDATE SQL（diff 已覆盖除主键外全部列，含可空列写 NULL）
//   5. 使用第三方包时自行补 import 块
// ─────────────────────────────────────────────────────────────────────
package ${pkgName}

// （示例）按唯一键查询（按需取消注释并补 import，Col 常量见生成物）：
// func (r *${repoClass}) SelectByAccount(account string) (*${enClass}, error) {
// 	var entity ${enClass}
// 	if err := cdb.WhereEq(r.db, ColAccount, account).First(&entity).Error; err != nil {
// 		return nil, err
// 	}
// 	return &entity, nil
// }
${/if}
