${# ============================================================================}
${# C 端 Router 模板：modules/app/{dataDir}/{tableName}_router_.go             }
${# ----------------------------------------------------------------------------}
${#   - 只挂 selectByIn / selectById；appAuth=user 时挂 middleware.UserAuth    }
${#   - appEmpty=空接口时不挂只读路由，RegisterRoutes 仅调 RegisterExtRoutes    }
${#   - 不挂 RequirePerm（管理端专用）                                          }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${appPkgComment}
package ${pkgName}

${appRouterImports}

// mount${tableClass} 装配 C 端${tableComment}路由组与 handler（路径 / 中间件 / DB 的唯一来源）
// 只读查询与扩展路由都复用它；${appRouterMountNote}
func mount${tableClass}(rg *gin.RouterGroup) (*gin.RouterGroup, *${handlerClass}) {
	h := New${handlerClass}(New${serviceClass}(common.DB))
	g := rg.Group("/${tableName}")
${if !appIsPublic}
	g.Use(middleware.UserAuth())
${/if}
	return g, h
}

// RegisterRoutes 注册${tableComment}(${tableName}) C 端路由（${appRouterAuthNote}）
// 挂在 /api/app 组下；自定义接口写在 ${tableName}_router.go / ${tableName}_handler.go，自动挂载。
func RegisterRoutes(rg *gin.RouterGroup) {
${if appEmpty}	g, _ := mount${tableClass}(rg)
${else}	g, h := mount${tableClass}(rg)
${/if}${if !appEmpty}	{
		g.POST("/selectByIn", h.SelectByIn)
		g.POST("/selectById", h.SelectById)
	}
${/if}	RegisterExtRoutes(g)
}
