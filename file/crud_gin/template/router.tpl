${# ============================================================================}
${# Router 模板：modules/admin/{dataDir}/{tableName}_router_.go   }
${# ----------------------------------------------------------------------------}
${#   - mount<表>(rg) 装配 group/handler/中间件（路径与 DB 的唯一来源）         }
${#   - RegisterRoutes 只注册固定 CRUD，admin 端每接口串 RequirePerm；           }
${#     末尾调用 RegisterExtRoutes 挂载手写扩展路由（ext_router 首次骨架提供）   }
${# 数据来源：pkgName/adminPkgComment/tableClass/handlerClass/serviceClass      }
${#           tableName/routerImports/permPrefix                               }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${adminPkgComment}
package ${pkgName}

${routerImports}

// mount${tableClass} 装配${tableComment}路由组与 handler（路径 / 中间件 / DB 的唯一来源）
// CRUD 路由与扩展路由都复用它，避免两处各建一套导致中间件漂移。
func mount${tableClass}(rg *gin.RouterGroup) (*gin.RouterGroup, *${handlerClass}) {
	h := New${handlerClass}(New${serviceClass}(common.DB))
	g := rg.Group("/${tableName}")
	g.Use(middleware.JWTAuth())
	return g, h
}

// RegisterRoutes 注册${tableComment}(${tableName})模块路由（JWT 认证 + 接口级 RBAC）
// 自定义接口写在 ${tableName}_router.go / ${tableName}_handler.go
// （首次骨架，重新生成不覆盖），由下方 RegisterExtRoutes 自动挂载。
func RegisterRoutes(rg *gin.RouterGroup) {
	g, h := mount${tableClass}(rg)
	{
		g.POST("/selectByIn", middleware.RequirePerm("${permPrefix}:list"), h.SelectByIn)
		g.POST("/selectById", middleware.RequirePerm("${permPrefix}:list"), h.SelectById)
		g.POST("/update", middleware.RequirePerm("${permPrefix}:edit"), h.Update)
		g.POST("/delete", middleware.RequirePerm("${permPrefix}:delete"), h.Delete)
	}
	RegisterExtRoutes(g)
}
