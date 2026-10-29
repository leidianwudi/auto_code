${# ============================================================================}
${# Handler 模板：modules/admin/{dataDir}/{tableName}_handler_.go  }
${# ----------------------------------------------------------------------------}
${#   - 4 接口：selectByIn / selectById / update / delete（无 insert，save 单接口）}
${#   - common.Bind 绑定 + common.OK/Fail/NewOutPage 统一响应                   }
${# 数据来源：pkgName/handlerClass/serviceClass/updClass/selClass/dataAlias      }
${#           hdlImports/tableComment/tableName                                 }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${hdlImports}

// ${handlerClass} ${tableComment}(${tableName}) HTTP 层
// 薄层：参数绑定 → service → 统一响应（common.OK/common.Fail）
// 自定义 HTTP 方法写在手写件 ${tableName}_handler.go
type ${handlerClass} struct {
	Svc *${serviceClass}
}

// New${handlerClass} 创建 HTTP 层对象
func New${handlerClass}(svc *${serviceClass}) *${handlerClass} {
	return &${handlerClass}{Svc: svc}
}

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

// Update save 单接口：Id=0 新增，Id>0 更新
func (h *${handlerClass}) Update(c *gin.Context) {
	data, ok := common.Bind[${updClass}](c)
	if !ok {
		return
	}
	id, err := h.Svc.Save(&data)
	if err != nil {
		common.Fail(c, err.Error())
		return
	}
	common.OK(c, gin.H{"id": id})
}

// Delete 删除记录（支持多条）
func (h *${handlerClass}) Delete(c *gin.Context) {
	body, ok := common.Bind[common.InDelIds](c)
	if !ok {
		return
	}
	affected, err := h.Svc.Delete(body.Ids)
	if err != nil {
		common.Fail(c, err.Error())
		return
	}
	common.OK(c, gin.H{"affected": affected})
}
