${# ============================================================================}
${# C 端 Handler 模板：modules/app/{dataDir}/{tableName}_handler_.go           }
${# ----------------------------------------------------------------------------}
${#   - 只读两接口：selectByIn / selectById（无写接口）；appEmpty=空接口时不生成  }
${#     查询方法（仅 struct + New），自定义接口写 ext 扩展件                      }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${hdlImports}

// ${handlerClass} ${tableComment}(${tableName}) C 端 HTTP 层（只读）
// 薄层：参数绑定 → service → 统一响应（common.OK/common.Fail）
${if appEmpty}// 空接口形态：不生成只读接口；自定义接口 Handler 写 ${tableName}_ext_handler.go
${/if}type ${handlerClass} struct {
	Svc *${serviceClass}
}

// New${handlerClass} 创建 HTTP 层对象
func New${handlerClass}(svc *${serviceClass}) *${handlerClass} {
	return &${handlerClass}{Svc: svc}
}
${if !appEmpty}
// SelectByIn 分页查询
func (h *${handlerClass}) SelectByIn(c *gin.Context) {
	sel, ok := common.Bind[${dataAlias}.${selClass}](c)
	if !ok {
		return
	}
	list, total, err := h.Svc.SelectByIn(&sel)
	if err != nil {
		common.Fail(c, err.Error())
		return
	}
	common.OK(c, common.NewOutPage(list, total))
}

// SelectById 根据id查询
func (h *${handlerClass}) SelectById(c *gin.Context) {
	body, ok := common.Bind[common.InSelId](c)
	if !ok {
		return
	}
	entity, err := h.Svc.SelectById(body.Id)
	if err != nil {
		common.Fail(c, err.Error())
		return
	}
	common.OK(c, entity)
}
${/if}
