${# ============================================================================}
${# Handler 手写扩展件骨架：modules/admin/{dataDir}/{tableName}_handler.go      }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                       }
${#   - 示例全部注释（零 import 可编译）；方法由手写 router 件挂载                }
${# 数据来源：pkgName/tableComment/tableName/handlerClass                        }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────
// ${tableComment}(${tableName}) 扩展 HTTP 层（admin 端）：自定义接口 Handler 写这里
//   1. 建议定义为 *${handlerClass} 的方法（与生成件 ${tableName}_handler_.go 同包，
//      直接用 h.Svc 调业务层；生成件每次重写请勿手改）
//   2. 挂载到路由：在 ${tableName}_router.go 的 RegisterExtRoutes 里 rg.POST(...)
//   3. 使用第三方包时自行补 import 块（gin/common 等）
// ─────────────────────────────────────────────────────────────────
package ${pkgName}

// （示例）自定义接口 Handler：在 router 手写件里挂载 rg.POST("/approve", h.Approve)
// func (h *${handlerClass}) Approve(c *gin.Context) {
// 	common.OK(c, gin.H{"ok": true})
// }
${/if}
