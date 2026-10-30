${# ============================================================================}
${# Router 手写扩展件骨架：modules/admin/{dataDir}/{tableName}_router.go       }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                       }
${#   - RegisterExtRoutes 由生成件 {tableName}_router_.go 调用（挂载点契约）      }
${# 数据来源：pkgName/tableComment/tableName/tableClass/handlerClass/permPrefix  }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────
// ${tableComment}(${tableName}) 扩展路由（admin 端）：自定义接口挂载点
//   1. 生成件 ${tableName}_router_.go 每次重新生成都会调用 RegisterExtRoutes，
//      请勿删除该函数（函数体可自由修改）
//   2. rg 复用 mount${tableClass} 的路由组（已挂 JWTAuth）；
//      接口级权限自行调用 middleware.RequirePerm("${permPrefix}:xxx")
//   3. 扩展 Handler 方法建议写在 ${tableName}_handler.go
//   4. 使用第三方包时自行补 import 块
// ─────────────────────────────────────────────────────────────────
package ${pkgName}

import "github.com/gin-gonic/gin"

// RegisterExtRoutes 挂载${tableComment}扩展路由（生成件 ${tableName}_router_.go 调用）
func RegisterExtRoutes(rg *gin.RouterGroup) {
	// 示例：rg.POST("/approve", middleware.RequirePerm("${permPrefix}:edit"), h.Approve)
	_ = rg
}
${/if}
