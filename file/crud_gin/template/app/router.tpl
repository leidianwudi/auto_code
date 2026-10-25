${# ============================================================================}
${# C 端 Router 模板：modules/app/{dataDir}/{tableName}_router_.go             }
${# ----------------------------------------------------------------------------}
${#   - 只挂 selectByIn / selectById；appAuth=user 时挂 middleware.UserAuth    }
${#   - 不挂 RequirePerm（管理端专用）；不生成 CustomRoutes 钩子               }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${appPkgComment}
package ${pkgName}

${appRouterImports}

// mount${tableClass} 装配 C 端${tableComment}路由组与 handler（路径 / 中间件 / DB 的唯一来源）
// 只读查询与手写件的自定义路由都复用它；${appRouterMountNote}
func mount${tableClass}(rg *gin.RouterGroup) (*gin.RouterGroup, *${handlerClass}) {
	h := New${handlerClass}(New${serviceClass}(common.DB))
	g := rg.Group("/${tableName}")
${if !appIsPublic}
	g.Use(middleware.UserAuth())
${/if}
	return g, h
}

// RegisterRoutes 注册${tableComment}(${tableName}) C 端查询路由（${appRouterAuthNote}）
// 挂在 /api/app 组下：/api/app/${tableName}/selectByIn、/api/app/${tableName}/selectById
func RegisterRoutes(rg *gin.RouterGroup) {
	g, h := mount${tableClass}(rg)
	{
		g.POST("/selectByIn", h.SelectByIn)
		g.POST("/selectById", h.SelectById)
	}
}
