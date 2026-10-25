${# ============================================================================}
${# Router 模板（split 布局）：modules/admin/{dataDir}/{tableName}_router_.go   }
${# ----------------------------------------------------------------------------}
${#   - mount<表>(rg) 装配 group/handler/中间件（路径与 DB 的唯一来源）         }
${#   - RegisterRoutes 只注册固定 CRUD，admin 端每接口串 RequirePerm           }
${#   - 不生成 CustomRoutes 钩子；自定义路由写手写件 <表>_router.go            }
${# 数据来源：pkgName/adminPkgComment/tableClass/handlerClass/serviceClass      }
${#           tableName/routerImports/permPrefix                               }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改

// Package ${pkgName} ${adminPkgComment}
package ${pkgName}

${routerImports}

// mount${tableClass} 装配${tableComment}路由组与 handler（路径 / 中间件 / DB 的唯一来源）
// CRUD 路由与手写件的自定义路由都复用它，避免两处各建一套导致中间件漂移。
func mount${tableClass}(rg *gin.RouterGroup) (*gin.RouterGroup, *${handlerClass}) {
	h := New${handlerClass}(New${serviceClass}(common.DB))
	g := rg.Group("/${tableName}")
	g.Use(middleware.JWTAuth())
	return g, h
}

// RegisterRoutes 注册${tableComment}(${tableName})模块路由（JWT 认证 + 接口级 RBAC）
// 自定义路由在 router.Setup 中显式调用 ${pkgName}.RegisterCustomRoutes(api)，生成件不再提供钩子。
func RegisterRoutes(rg *gin.RouterGroup) {
	g, h := mount${tableClass}(rg)
	{
		g.POST("/selectByIn", middleware.RequirePerm("${permPrefix}:list"), h.SelectByIn)
		g.POST("/selectById", middleware.RequirePerm("${permPrefix}:list"), h.SelectById)
		g.POST("/update", middleware.RequirePerm("${permPrefix}:edit"), h.Update)
		g.POST("/delete", middleware.RequirePerm("${permPrefix}:delete"), h.Delete)
	}
}
