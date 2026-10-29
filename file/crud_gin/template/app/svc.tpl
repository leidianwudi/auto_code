${# ============================================================================}
${# C 端 Service 模板：modules/app/{dataDir}/{tableName}_service_.go           }
${# ----------------------------------------------------------------------------}
${#   - 只读：仅 SelectByIn/SelectById 转发 data 层 repo                        }
${#   - 永不生成写接口（C 端写操作都是业务流程，手写）                           }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${svcImports}

// ${serviceClass} ${tableComment}(${tableName}) C 端业务层（只读）
// 生成件只提供查询转发；C 端不生成写接口，业务流程写在手写件。
type ${serviceClass} struct {
${each line in svcFieldLines}
${line}
${/each}}

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
