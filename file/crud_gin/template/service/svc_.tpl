${# ============================================================================}
${# Svc_ 模板：生成 Go 业务层覆盖层（internal/service/{tableName}_.go）        }
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成，请勿手改。对齐 NestJS service_.ts 的角色。        }
${#   - type {ServiceClass} struct{ DB *gorm.DB; Repo *repository.{RepoClass} }  }
${#   - 五方法编排：i18n 场景用 DB.Transaction 包裹 repo 调用并传 tx            }
${#     （事务边界在 service，repo 方法接收 db/tx 第一参数）                     }
${# 手改约定：新增业务方法写 {tableName}.go（同 struct 跨文件定义），勿动本文件   }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package service

${svcImports}

// ${serviceClass} ${tableComment}(${tableName}) 业务层（覆盖层，重新生成会覆盖本文件）
// 事务边界在本层：i18n 场景用 DB.Transaction 包裹 repo 调用并传 tx
type ${serviceClass} struct {
  DB *gorm.DB
  Repo *repository.${repoClass}
}

// New${serviceClass} 创建业务层对象
func New${serviceClass}(db *gorm.DB) *${serviceClass} {
  return &${serviceClass}{DB: db, Repo: &repository.${repoClass}{DB: db}}
}

// SelectByIn 分页查询列表数据，返回数据列表和总记录数
func (s *${serviceClass}) SelectByIn(sel *model.${selClass}) ([]model.${entityClass}, int64, error) {
  return s.Repo.SelectByIn(s.DB, sel)
}

// SelectById 根据id查询单条记录
func (s *${serviceClass}) SelectById(id ${idGoType}) (*model.${entityClass}, error) {
  return s.Repo.SelectById(s.DB, id)
}

// Insert 新增一条记录
func (s *${serviceClass}) Insert(data *model.${insClass}) (${idGoType}, error) {
${if hasI18n}
  // i18n：主表与翻译表同事务写入（翻译行按 (${i18nExtKey}, ${i18nLangKey}) upsert）
  var newId ${idGoType}
  err := s.DB.Transaction(func(tx *gorm.DB) error {
    var err error
    newId, err = s.Repo.Insert(tx, data)
    if err != nil {
      return err
    }
    return s.Repo.UpsertI18n(tx, newId, data.I18n)
  })
  return newId, err
${else}
  return s.Repo.Insert(s.DB, data)
${/if}
}

// Update 更新一条记录，返回受影响行数
func (s *${serviceClass}) Update(data *model.${updClass}) (int64, error) {
${if hasI18n}
  // i18n：主表与翻译表同事务更新（翻译行按 (${i18nExtKey}, ${i18nLangKey}) upsert）
  var affected int64
  err := s.DB.Transaction(func(tx *gorm.DB) error {
    var err error
    affected, err = s.Repo.Update(tx, data)
    if err != nil {
      return err
    }
    return s.Repo.UpsertI18n(tx, data.${idGoName}, data.I18n)
  })
  return affected, err
${else}
  return s.Repo.Update(s.DB, data)
${/if}
}

// Delete 删除一条或多条记录，返回受影响行数
func (s *${serviceClass}) Delete(ids []${idGoType}) (int64, error) {
${if hasI18n}
  // i18n：同事务先删翻译行再删主表
  var affected int64
  err := s.DB.Transaction(func(tx *gorm.DB) error {
    var err error
    affected, err = s.Repo.Delete(tx, ids)
    return err
  })
  return affected, err
${else}
  return s.Repo.Delete(s.DB, ids)
${/if}
}
