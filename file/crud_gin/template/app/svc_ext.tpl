${# ============================================================================}
${# C 端 Service 手写扩展件骨架：modules/app/{dataDir}/{tableName}_service.go   }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                       }
${#   - 生成件不调用其中方法（无编译契约）；禁止与生成件方法重名                  }
${# 数据来源：pkgName/tableComment/tableName/serviceClass/dataAlias/enClass/idGoType }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────
// ${tableComment}(${tableName}) 扩展业务层（C 端）：自定义业务方法写这里
//   1. 建议定义为 *${serviceClass} 的方法（与生成件 ${tableName}_service_.go 同包，
//      直接用 s.DB / s.Repo；生成件每次重写请勿手改）
//      注意：Repo.Transaction 回调内禁止用 s.DB（会另借连接、脱离事务），须用 s.Repo.Tx(tx) 派生实例
//   2. 禁止与生成件方法重名（重复定义编译错误）
//   3. 使用第三方包时自行补 import 块
// ─────────────────────────────────────────────────────────────────
package ${pkgName}

// （示例）扩展业务方法：ext_handler / ext_router 按需调用
// func (s *${serviceClass}) MyBusiness(id ${idGoType}) error {
// 	return cdb.WhereEq(s.DB.Model(&${dataAlias}.${enClass}{}), ${dataAlias}.ColId, id).Update(${dataAlias}.ColEnable, 1).Error // Update 列按该表实际要改的列常量替换（如 ColEnable / ColLevel / ColIsDisable）
// }
${/if}
