${# ============================================================================}
${# Repo 模板：data/{dataDir}/{tableName}_repo_.go               }
${# ----------------------------------------------------------------------------}
${#   - 共享数据层：admin/app 两端复用；方法首参 db 支持独立查询/事务           }
${#   - 白名单排序（selColsSort 派生）+ common.Paginate/Where*/ApplyOrder      }
${#   - 写路径只留 UpdateById(map) 原子方法，Save/Delete 编排在手写 service 件 }
${# 数据来源：pkgName/repoClass/enClass/selClass/idGoType/idGoName/tableComment  }
${#           repoImports/sortWhitelistName/sortEntryLines/whereConditions      }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${repoImports}

// ${repoClass} ${tableComment}(${tableName}) 数据访问层（共享，admin 端与 app 端复用）
// 方法首参 db *gorm.DB：传 common.DB 走独立查询，传 tx 进入事务。
type ${repoClass} struct {
	DB *gorm.DB
}

// New${repoClass} 创建数据访问层对象
func New${repoClass}(db *gorm.DB) *${repoClass} {
	return &${repoClass}{DB: db}
}

// ${sortWhitelistName} 排序白名单：列名 → 固定方向（前端只传字段名，防注入）
var ${sortWhitelistName} = map[string]string{
${each line in sortEntryLines}
${line}
${/each}
}

// SelectByIn 分页查询，返回数据列表和总记录数
func (r *${repoClass}) SelectByIn(db *gorm.DB, sel *${selClass}) ([]${enClass}, int64, error) {
	query := r.getWhereByIn(db.Model(&${enClass}{}), sel)
	query = common.ApplyOrder(query, sel.Sort, ${sortWhitelistName})
	var list []${enClass}
	total, err := common.Paginate(query, sel.Page, sel.PageSize, &list)
	if err != nil {
		return nil, 0, err
	}
	return list, total, nil
}

// getWhereByIn 根据查询参数构建查询条件（指针 nil=不加条件，非 nil=按它筛选，含 0）
func (r *${repoClass}) getWhereByIn(db *gorm.DB, sel *${selClass}) *gorm.DB {
${each cond in whereConditions}
${cond}
${/each}
	return db
}

// SelectById 根据id查询单条记录
func (r *${repoClass}) SelectById(db *gorm.DB, id ${idGoType}) (*${enClass}, error) {
	var entity ${enClass}
	if err := common.WhereEq(db, Col${idGoName}, id).First(&entity).Error; err != nil {
		return nil, err
	}
	return &entity, nil
}

// Insert 新增一条记录，返回新记录主键
func (r *${repoClass}) Insert(db *gorm.DB, entity *${enClass}) (${idGoType}, error) {
	if err := db.Create(entity).Error; err != nil {
		return 0, err
	}
	return entity.${idGoName}, nil
}

// UpdateById 按主键更新（updates 的 key 用列名常量），空更新集直接返回 0
func (r *${repoClass}) UpdateById(db *gorm.DB, id ${idGoType}, updates map[string]any) (int64, error) {
	if len(updates) == 0 {
		return 0, nil
	}
	query := common.WhereEq(db.Model(&${enClass}{}).Omit(Col${idGoName}), Col${idGoName}, id).Updates(updates)
	if query.Error != nil {
		return 0, query.Error
	}
	return query.RowsAffected, nil
}

// Delete 删除记录（支持多条），返回受影响行数
func (r *${repoClass}) Delete(db *gorm.DB, ids []${idGoType}) (int64, error) {
	res := common.WhereIn(db, Col${idGoName}, ids).Delete(&${enClass}{})
	if res.Error != nil {
		return 0, res.Error
	}
	return res.RowsAffected, nil
}
