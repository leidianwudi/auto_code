${# ============================================================================}
${# Repo 模板：data/{dataDir}/{tableName}_repo_.go               }
${# ----------------------------------------------------------------------------}
${#   - 共享数据层：admin/app 两端复用                                         }
${#   - Repo 持有默认连接，方法零 db 传参；事务用 Tx(tx) 派生新实例              }
${#     （绝不写 r.db = tx，否则并发会串事务）                                 }
${#   - 所有表统一走 querySpec 流水线（Apply→Where→ApplyOrder→分页），           }
${#     排序白名单在 querySpec.Order（无联表时 Joins 为空）                       }
${#   - Insert(Create 实体，nil 指针列由 GORM 写 SQL NULL)；                     }
${#     Update(e) 按快照 diff 只写变更列，成功后重拍快照                         }
${#   - 复合主键表没有自增主键：不生成 Update，只留 Insert/Delete              }
${# 数据来源：pkgName/repoClass/enClass/selClass/listType/idGoType/idGoName/       }
${#           idInsertWithIdCond/idInvalidEntityCond/hasAutoIncPk/                }
${#           tableComment/repoImports/whereConditions                          }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

${repoImports}

// ${repoClass} ${tableComment}(${tableName}) 数据访问层（共享，admin 端与 app 端复用）
// 持有默认连接，方法不接收 db；需要事务时用 Tx(tx) 派生新对象。
type ${repoClass} struct {
	db *gorm.DB // 默认连接
}

// New${repoClass} 创建数据访问层对象（db 传 cdb.DB）
func New${repoClass}(db *gorm.DB) *${repoClass} {
	return &${repoClass}{db: db}
}

// Tx 返回绑定到该事务的新实例（不修改 r.db，原对象可继续并发使用）
func (r *${repoClass}) Tx(tx *gorm.DB) *${repoClass} {
	return &${repoClass}{db: tx}
}

${if hasJoin}// SelectByIn 分页联表查询，返回视图列表和总记录数
// 列表返回 ${listType}（主表实体 + 联表输出列）
${else}// SelectByIn 分页查询，返回数据列表和总记录数
${/if}// 查询流水线：querySpec.Apply → WHERE（getWhereByIn）→ ORDER（querySpec.ApplyOrder）→ 分页
func (r *${repoClass}) SelectByIn(sel *${selClass}) ([]${listType}, int64, error) {
	query := querySpec.Apply(r.db.Model(&${enClass}{}))
	query = r.getWhereByIn(query, sel)
	query = querySpec.ApplyOrder(query, sel.Sort)
	var list []${listType}
	total, err := cdb.Paginate(query, sel.Page, sel.PageSize, &list)
	if err != nil {
		return nil, 0, err
	}
	return list, total, nil
}

${# getWhereByIn 的 db 参数是"查询流水线"而非连接参数（条件块内部以 db 作累加器）}
// getWhereByIn 根据查询参数构建查询条件（指针 nil=不加条件，非 nil=按它筛选，含 0）
// 全部裸列名：主表列与联表输出列由 QuerySpec 自动限定表名
func (r *${repoClass}) getWhereByIn(db *gorm.DB, sel *${selClass}) *gorm.DB {
${each cond in whereConditions}
${cond}
${/each}
	return db
}

// SelectById 根据主键查询单条记录（不拍快照：调用方要改字段时自行 TakeSnapshot）
func (r *${repoClass}) SelectById(id ${idGoType}) (*${enClass}, error) {
	var entity ${enClass}
	if err := cdb.WhereEq(r.db, Col${idGoName}, id).First(&entity).Error; err != nil {
		return nil, err
	}
	return &entity, nil
}

${if hasAutoIncPk}// Insert 新增一条记录：主键必须为 0（由数据库自增），成功后 GORM 回填 e.${idGoName}
// 可空列（指针）为 nil 时 GORM 直接写 SQL NULL
func (r *${repoClass}) Insert(e *${enClass}) error {
	if e == nil {
		return ErrNilEntity
	}
	if ${idInsertWithIdCond} {
		return ErrInsertWithId
	}
	return r.db.Create(e).Error
}

// Update 按主键更新：只写与 TakeSnapshot 基线不同的列，空 diff 直接返回 (0, nil)
// 主键取自 e.${idGoName}；成功后重拍快照，便于连续多次更新
func (r *${repoClass}) Update(e *${enClass}) (int64, error) {
	if e == nil {
		return 0, ErrNilEntity
	}
	if e.orig == nil {
		return 0, ErrNoSnapshot
	}
	if ${idInvalidEntityCond} {
		return 0, ErrInvalidId
	}
	updates := e.changedColumns()
	if len(updates) == 0 {
		return 0, nil
	}
	n, err := r.UpdateById(r.db, e.${idGoName}, updates)
	if err != nil {
		return 0, err
	}
	e.TakeSnapshot()
	return n, nil
}
${else}// Insert 新增一条记录（本表无自增主键：主键由调用方指定，不做自增检查）
// 可空列（指针）为 nil 时 GORM 直接写 SQL NULL
// TODO 复合/业务主键表不生成 Update(e)：更新请在手写件按业务键实现
func (r *${repoClass}) Insert(e *${enClass}) error {
	if e == nil {
		return ErrNilEntity
	}
	return r.db.Create(e).Error
}
${/if}

// UpdateById 按主键更新（updates 的 key 用列名常量），空更新集直接返回 0
// 底层原语：保留 db 首参，供事务内或自定义 SQL 场景直接调用
func (r *${repoClass}) UpdateById(db *gorm.DB, id ${idGoType}, updates map[string]any) (int64, error) {
	if len(updates) == 0 {
		return 0, nil
	}
	query := cdb.WhereEq(db.Model(&${enClass}{}).Omit(Col${idGoName}), Col${idGoName}, id).Updates(updates)
	if query.Error != nil {
		return 0, query.Error
	}
	return query.RowsAffected, nil
}

// Delete 删除记录（支持多条），返回受影响行数
func (r *${repoClass}) Delete(ids []${idGoType}) (int64, error) {
	res := cdb.WhereIn(r.db, Col${idGoName}, ids).Delete(&${enClass}{})
	if res.Error != nil {
		return 0, res.Error
	}
	return res.RowsAffected, nil
}
