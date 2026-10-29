${# ============================================================================}
${# Service 模板：modules/admin/{dataDir}/{tableName}_service_.go  }
${# ----------------------------------------------------------------------------}
${#   - 生成件只出查询编排；写路径（Save/Delete）与自定义方法放手写 service 件  }
${#   - DB/Repo 两列由 main_api.ac 预渲染对齐                                  }
${# 数据来源：pkgName/serviceClass/repoClass/enClass/selClass/dataAlias         }
${#           svcImports/svcFieldLines/tableView                                }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${svcImports}

// ${serviceClass} ${tableComment}(${tableName}) 业务层
// 生成件只提供查询；写路径（Save/Delete）与自定义方法写在手写件 ${tableName}_service.go。
type ${serviceClass} struct {
${each line in svcFieldLines}
${line}
${/each}}

// New${serviceClass} 创建业务层对象
func New${serviceClass}(db *gorm.DB) *${serviceClass} {
	return &${serviceClass}{DB: db, Repo: ${dataAlias}.New${repoClass}(db)}
}

// SelectByIn 分页查询列表数据，返回数据列表和总记录数
func (s *${serviceClass}) SelectByIn(sel *${dataAlias}.${selClass}) ([]${dataAlias}.${enClass}, int64, error) {
	return s.Repo.SelectByIn(s.DB, sel)
}

// SelectById 根据id查询单条记录
func (s *${serviceClass}) SelectById(id ${idGoType}) (*${dataAlias}.${enClass}, error) {
	return s.Repo.SelectById(s.DB, id)
}
