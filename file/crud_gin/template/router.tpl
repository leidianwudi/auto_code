${# ============================================================================}
${# Router 模板：渲染单表路由注册函数（幂等注入 internal/router/router_.go）   }
${# ----------------------------------------------------------------------------}
${# 说明：本模板只渲染函数文本（不含 package/import），由 go_module.ac 的        }
${#   updateRouterFile() 按 "func RegisterXxxRoutes(" 签名整行匹配去重后，      }
${#   追加进 router_.go 的 {autocode:routes} 标记区（照 app_module.ac 模式）。  }
${# ============================================================================}
// Register${entityClass}Routes 注册${tableComment}(${tableName})路由（AutoCode 生成，请勿手改）
func Register${entityClass}Routes(r *gin.RouterGroup, h *handler.${handlerClass}) {
  g := r.Group("/${tableName}")
  g.POST("/selectByIn", h.SelectByIn)
  g.POST("/selectById", h.SelectById)
  g.POST("/insert", h.Insert)
  g.POST("/update", h.Update)
  g.POST("/delete", h.Delete)
}
