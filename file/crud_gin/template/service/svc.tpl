${# ============================================================================}
${# Service 模板：modules/admin/{dataDir}/{tableName}_service_.go  }
${# ----------------------------------------------------------------------------}
${#   - CRUD 全量生成（查询 + 写路径 Save/Delete）：凡 handler 生成件调用的       }
${#     方法都在本覆盖件提供，手改不丢、契约不断                                 }
${#   - 自定义方法（生成件不调用）写在手写件 {tableName}_service.go              }
${#   - Repo 持有连接（C 方案），生成件不再把 db 传给 repo 方法；事务走 Repo.Tx  }
${#   - 更新：SelectById → TakeSnapshot() → ApplyTo(入参) → Repo.Update(e)        }
${#   - 无自增主键表：不生成 Update(e)，Save 只做新增并标注 TODO                  }
${#   - DB/Repo 两列由 main_api.ac 预渲染对齐                                  }
${# 数据来源：pkgName/serviceClass/repoClass/enClass/selClass/updClass/dataAlias  }
${#           svcImports/svcFieldLines/idGoType/idGoName/idZeroExpr/              }
${#           hasAutoIncPk/tableView                                              }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${svcImports}

// ${serviceClass} ${tableComment}(${tableName}) 业务层
// CRUD 全量生成；自定义方法（生成件不调用）写在手写件 ${tableName}_service.go。
// DB 供手写件做自定义 SQL；写路径统一走 Repo（事务用 s.Repo.Tx(tx)）。
// 禁止在 Repo.Transaction 回调内使用 s.DB：它会从连接池另借一条连接、静默脱离事务
// （Rollback 撤不回该连接上的写），事务内必须用 s.Repo.Tx(tx) 派生的实例。
type ${serviceClass} struct {
${each line in svcFieldLines}
${line}
${/each}}

// ${tableClass}SaveHook 手写件可赋值以接管 Save（密码加密、唯一性校验等）；nil 走默认
var ${tableClass}SaveHook func(s *${serviceClass}, data *${updClass}) (${idGoType}, error)

// ${tableClass}DeleteHook 手写件可赋值以接管 Delete（会话/缓存失效等）；nil 走默认
var ${tableClass}DeleteHook func(s *${serviceClass}, ids []${idGoType}) (int64, error)

// New${serviceClass} 创建业务层对象
func New${serviceClass}(db *gorm.DB) *${serviceClass} {
	return &${serviceClass}{DB: db, Repo: ${dataAlias}.New${repoClass}(db)}
}

// SelectByIn ${selComment}
func (s *${serviceClass}) SelectByIn(sel *${dataAlias}.${selClass}) ([]${dataAlias}.${listType}, int64, error) {
	return s.Repo.SelectByIn(sel)
}

// SelectById 根据id查询单条记录
func (s *${serviceClass}) SelectById(id ${idGoType}) (*${dataAlias}.${enClass}, error) {
	return s.Repo.SelectById(id)
}

${if hasAutoIncPk}// Save 保存（save 单接口）：Id=0 新增；Id>0 查库 → 拍快照 → 应用入参 → 按 diff 更新
func (s *${serviceClass}) Save(data *${updClass}) (${idGoType}, error) {
	if ${tableClass}SaveHook != nil {
		return ${tableClass}SaveHook(s, data)
	}
${if hasI18n}	// i18n：主表与翻译表同事务写入（实现见 ${tableName}_service_i18n_.go）
	return s.saveI18n(data)
${else}	if data.${idGoName} == ${idZeroExpr} {
		e := data.ToEntity()
${each line in svcCreateTimeLines}
${line}
${/each}		if err := s.Repo.Insert(e); err != nil {
			return ${idZeroExpr}, err
		}
		return e.${idGoName}, nil
	}
	e, err := s.Repo.SelectById(data.${idGoName})
	if err != nil {
		return ${idZeroExpr}, err
	}
	// 必须在改字段之前拍快照：Update 只写与基线不同的列
	e.TakeSnapshot()
	data.ApplyTo(e)
	return s.Repo.Update(e)
${/if}}
${else}// Save 保存（save 单接口）：本表无自增主键，Repo 未生成 Update(e)
// TODO 更新语义无法按主键 diff：请在手写件 ${tableName}_service.go 按业务键实现，
// 或用 s.Repo.UpdateById(id, updates)（事务内先 s.Repo.Tx(tx) 派生实例）。
func (s *${serviceClass}) Save(data *${updClass}) (${idGoType}, error) {
	if ${tableClass}SaveHook != nil {
		return ${tableClass}SaveHook(s, data)
	}
	if err := s.Repo.Insert(data.ToEntity()); err != nil {
		return ${idZeroExpr}, err
	}
	return ${idZeroExpr}, nil
}
${/if}

// Delete 删除一条或多条记录，返回受影响行数；手写件注册 hook 可接管
func (s *${serviceClass}) Delete(ids []${idGoType}) (int64, error) {
	if ${tableClass}DeleteHook != nil {
		return ${tableClass}DeleteHook(s, ids)
	}
${if hasI18n}	// i18n：主表与翻译表同事务删除（实现见 ${tableName}_service_i18n_.go）
	return s.deleteI18n(ids)
${else}	return s.Repo.Delete(ids)
${/if}}
