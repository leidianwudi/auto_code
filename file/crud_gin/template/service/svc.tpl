${# ============================================================================}
${# Service 模板：modules/admin/{dataDir}/{tableName}_service_.go  }
${# ----------------------------------------------------------------------------}
${#   - CRUD 全量生成（查询 + 写路径 Save/Delete）：凡 handler 生成件调用的       }
${#     方法都在本覆盖件提供，手改不丢、契约不断                                 }
${#   - 自定义方法（生成件不调用）写在手写件 {tableName}_service.go              }
${#   - DB/Repo 两列由 main_api.ac 预渲染对齐                                  }
${# 数据来源：pkgName/serviceClass/repoClass/enClass/selClass/updClass/dataAlias  }
${#           svcImports/svcFieldLines/idGoType/tableView                        }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${svcImports}

// ${serviceClass} ${tableComment}(${tableName}) 业务层
// CRUD 全量生成；自定义方法（生成件不调用）写在手写件 ${tableName}_service.go。
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
	return s.Repo.SelectByIn(s.DB, sel)
}

// SelectById 根据id查询单条记录
func (s *${serviceClass}) SelectById(id ${idGoType}) (*${dataAlias}.${enClass}, error) {
	return s.Repo.SelectById(s.DB, id)
}

// Save 保存（save 单接口）：Id=0 走新增，Id>0 走更新；手写件注册 hook 可接管
func (s *${serviceClass}) Save(data *${updClass}) (${idGoType}, error) {
	if ${tableClass}SaveHook != nil {
		return ${tableClass}SaveHook(s, data)
	}
	if data.Id == 0 {
		return s.Repo.Insert(s.DB, data.ToEntity())
	}
	return s.Repo.UpdateById(s.DB, data.Id, data.ToUpdates())
}

// Delete 删除一条或多条记录，返回受影响行数；手写件注册 hook 可接管
func (s *${serviceClass}) Delete(ids []${idGoType}) (int64, error) {
	if ${tableClass}DeleteHook != nil {
		return ${tableClass}DeleteHook(s, ids)
	}
	return s.Repo.Delete(s.DB, ids)
}
