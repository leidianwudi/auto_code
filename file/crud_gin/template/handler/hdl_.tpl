${# ============================================================================}
${# Hdl_ 模板：生成 Go HTTP 层覆盖层（internal/handler/{tableName}_.go）       }
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成，请勿手改。对齐 NestJS controller_.ts 的角色。     }
${#   - type {HandlerClass} struct{ Svc *service.{ServiceClass} }               }
${#   - gin 上下文绑定 ShouldBindJSON → svc → 统一响应 {code, msg, data}        }
${#     （code: 0 成功 / 1 失败；薄层，无业务逻辑）                              }
${#   - 分页请求解析 page/pageSize（SelectParams）                               }
${# 手改约定：自定义接口/换编排写 {tableName}.go，勿动本文件                     }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package handler

${hdlImports}

// ${handlerClass} ${tableComment}(${tableName}) HTTP 层（覆盖层，重新生成会覆盖本文件）
// 薄层：参数绑定 → service → 统一响应 {"code": 0, "msg": "ok", "data": ...}（0 成功 / 1 失败），无业务逻辑
type ${handlerClass} struct {
  Svc *service.${serviceClass}
}

// New${handlerClass} 创建 HTTP 层对象
func New${handlerClass}(svc *service.${serviceClass}) *${handlerClass} {
  return &${handlerClass}{Svc: svc}
}

// SelectByIn 分页查询
func (h *${handlerClass}) SelectByIn(c *gin.Context) {
  var sel model.${selClass}
  if err := c.ShouldBindJSON(&sel); err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  list, total, err := h.Svc.SelectByIn(&sel)
  if err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "ok", "data": gin.H{"list": list, "total": total}})
}

// SelectById 根据id查询
func (h *${handlerClass}) SelectById(c *gin.Context) {
  var body struct {
    ${idGoName} ${idGoType} `json:"${idColName}" binding:"required"` // 主键
  }
  if err := c.ShouldBindJSON(&body); err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  entity, err := h.Svc.SelectById(body.${idGoName})
  if err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "ok", "data": entity})
}

// Insert 新增记录
func (h *${handlerClass}) Insert(c *gin.Context) {
  var data model.${insClass}
  if err := c.ShouldBindJSON(&data); err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  newId, err := h.Svc.Insert(&data)
  if err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "ok", "data": gin.H{"id": newId}})
}

// Update 更新记录
func (h *${handlerClass}) Update(c *gin.Context) {
  var data model.${updClass}
  if err := c.ShouldBindJSON(&data); err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  affected, err := h.Svc.Update(&data)
  if err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "ok", "data": gin.H{"affected": affected}})
}

// Delete 删除记录（支持多条）
func (h *${handlerClass}) Delete(c *gin.Context) {
  var body struct {
    Ids []${idGoType} `json:"ids" binding:"required"` // 主键列表
  }
  if err := c.ShouldBindJSON(&body); err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  affected, err := h.Svc.Delete(body.Ids)
  if err != nil {
    c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
    return
  }
  c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "ok", "data": gin.H{"affected": affected}})
}
