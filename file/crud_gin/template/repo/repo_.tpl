${# ============================================================================}
${# Repo_ 模板：生成 Go 数据访问层覆盖层（internal/repository/{tableName}_.go）}
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成，请勿手改。对齐 NestJS db_.ts 的角色。             }
${#   - type {RepoClass} struct{ DB *gorm.DB }（DB 供手改层便捷使用）            }
${#   - 五方法 SelectByIn/SelectById/Insert/Update/Delete                       }
${#     全部接收 db *gorm.DB 第一参数：传 s.DB 走独立查询，传事务 tx 进事务      }
${#   - hasI18n 时含 UpsertI18n/FillI18nList/GetI18nMap 翻译辅助                }
${#     （clause.OnConflict 按翻译表 uk(extKey, langKey) 唯一键 upsert）         }
${# 手改约定：新增方法写 {tableName}.go（Go 同包多文件定义方法），勿动本文件      }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package repository

${repoImports}

// ${repoClass} ${tableComment}(${tableName}) 数据访问层（覆盖层，重新生成会覆盖本文件）
// 方法第一参数 db *gorm.DB：传 s.DB 走独立查询，传事务 tx 进入事务
type ${repoClass} struct {
  DB *gorm.DB
}

// New${repoClass} 创建数据访问层对象
func New${repoClass}(db *gorm.DB) *${repoClass} {
  return &${repoClass}{DB: db}
}

// SelectByIn 分页查询，返回数据列表和总记录数
func (r *${repoClass}) SelectByIn(db *gorm.DB, sel *model.${selClass}) ([]model.${entityClass}, int64, error) {
  var list []model.${entityClass}
  var total int64
  if sel.Page <= 0 {
    sel.Page = 1
  }
  if sel.PageSize <= 0 {
    sel.PageSize = 10
  }
  // 总记录数与数据列表分别构建查询，避免 gorm 语句复用相互污染
  if err := r.getWhereByIn(db.Model(&model.${entityClass}{}), sel).Count(&total).Error; err != nil {
    return nil, 0, err
  }
  query := r.getOrderBy(r.getWhereByIn(db.Model(&model.${entityClass}{}), sel), sel)
  if err := query.Offset((sel.Page - 1) * sel.PageSize).Limit(sel.PageSize).Find(&list).Error; err != nil {
    return nil, 0, err
  }
${if hasI18n}
  // i18n：平铺当前语言文本到每行（sel.Lang 不传用默认语言 ${i18nDefaultLang}，当前语言缺失回退默认语言）
  if err := r.FillI18nList(db, list, sel.Lang); err != nil {
    return nil, 0, err
  }
${/if}
  return list, total, nil
}

// getOrderBy 应用默认排序（${sortDesc}）
func (r *${repoClass}) getOrderBy(db *gorm.DB, sel *model.${selClass}) *gorm.DB {
  return db.Order("`${defaultOrderField}` ${defaultOrderDir}")
}

// getWhereByIn 根据查询参数构建查询条件
func (r *${repoClass}) getWhereByIn(db *gorm.DB, sel *model.${selClass}) *gorm.DB {
${each cond in whereConditions}
${cond}
${/each}
  return db
}

// SelectById 根据id查询单条记录
func (r *${repoClass}) SelectById(db *gorm.DB, id ${idGoType}) (*model.${entityClass}, error) {
  var entity model.${entityClass}
  if err := db.Where("`${idColName}` = ?", id).First(&entity).Error; err != nil {
    return nil, err
  }
${if hasI18n}
  // i18n：平铺默认语言文本 + 附带全语言翻译 map（供管理端逐语言编辑）
  list := []model.${entityClass}{entity}
  if err := r.FillI18nList(db, list, ""); err != nil {
    return nil, err
  }
  entity = list[0]
  i18n, err := r.GetI18nMap(db, id)
  if err != nil {
    return nil, err
  }
  entity.I18n = i18n
${/if}
  return &entity, nil
}

// Insert 新增一条记录，返回新记录主键
func (r *${repoClass}) Insert(db *gorm.DB, data *model.${insClass}) (${idGoType}, error) {
  var newId ${idGoType}
  entity := model.${entityClass}{
${each field in insFields}
    ${field.goName}: data.${field.goName},
${/each}
  }
  if err := db.Create(&entity).Error; err != nil {
    return newId, err
  }
  return entity.${idGoName}, nil
}

// Update 更新记录（UpdateParams 指针字段 nil 不更新），返回受影响行数
func (r *${repoClass}) Update(db *gorm.DB, data *model.${updClass}) (int64, error) {
  // 逐字段判空组装更新集：nil = 未传 = 保持原值（gorm Updates(map) 全量更新 map 内字段）
  updates := map[string]interface{}{}
${each field in updFields}
  if data.${field.goName} != nil {
    updates["${field.name}"] = ${if field.isPtr}*${/if}data.${field.goName}
  }
${/each}
  if len(updates) == 0 {
    return 0, nil
  }
  res := db.Model(&model.${entityClass}{}).Where("`${idColName}` = ?", data.${idGoName}).Updates(updates)
  if res.Error != nil {
    return 0, res.Error
  }
  return res.RowsAffected, nil
}

// Delete 删除记录（支持多条），返回受影响行数
func (r *${repoClass}) Delete(db *gorm.DB, ids []${idGoType}) (int64, error) {
${if hasI18n}
  // i18n：先删翻译行再删主表（外键无级联时避免残留孤儿翻译行）
  if err := db.Where("`${i18nExtKey}` IN ?", ids).Delete(&model.${i18nEntityClass}{}).Error; err != nil {
    return 0, err
  }
${/if}
  res := db.Where("`${idColName}` IN ?", ids).Delete(&model.${entityClass}{})
  if res.Error != nil {
    return 0, res.Error
  }
  return res.RowsAffected, nil
}
${if hasI18n}

${# ── i18n 翻译表辅助方法（翻译表是主表从属，操作内聚在主表 repo）────────     }
// UpsertI18n 写入翻译行（按 uk(${i18nExtKey}, ${i18nLangKey}) upsert：存在则更新，不存在则插入）
// extId/lang 由服务端覆盖，客户端只传翻译字段；依赖翻译表唯一键 uk(${i18nExtKey}, ${i18nLangKey})
func (r *${repoClass}) UpsertI18n(db *gorm.DB, extId ${i18nExtGoType}, data model.${i18nMapType}) error {
  if len(data) == 0 {
    return nil
  }
  rows := make([]model.${i18nEntityClass}, 0, len(data))
  for lang, item := range data {
    item.${i18nExtGoName} = extId
    item.${i18nLangGoName} = lang
    rows = append(rows, item)
  }
  return db.Clauses(clause.OnConflict{
    Columns:   []clause.Column{{Name: "${i18nExtKey}"}, {Name: "${i18nLangKey}"}},
    DoUpdates: clause.AssignmentColumns([]string{${i18nColsStr}}),
  }).Create(&rows).Error
}

// FillI18nList 平铺翻译文本进每行（lang 为空用默认语言 ${i18nDefaultLang}；当前语言缺失回退默认语言）
func (r *${repoClass}) FillI18nList(db *gorm.DB, list []model.${entityClass}, lang string) error {
  if len(list) == 0 {
    return nil
  }
  if lang == "" {
    lang = "${i18nDefaultLang}"
  }
  // 收集主表主键，一次查出全部翻译行
  ids := make([]${idGoType}, 0, len(list))
  for _, item := range list {
    ids = append(ids, item.${idGoName})
  }
  var rows []model.${i18nEntityClass}
  if err := db.Where("`${i18nExtKey}` IN ?", ids).Where("`${i18nLangKey}` = ?", lang).Find(&rows).Error; err != nil {
    return err
  }
  if len(rows) == 0 && lang != "${i18nDefaultLang}" {
    // 当前语言缺失 → 回退默认语言
    if err := db.Where("`${i18nExtKey}` IN ?", ids).Where("`${i18nLangKey}` = ?", "${i18nDefaultLang}").Find(&rows).Error; err != nil {
      return err
    }
  }
  // 按外键分组，平铺翻译字段
  rowMap := make(map[${i18nExtGoType}]model.${i18nEntityClass}, len(rows))
  for _, row := range rows {
    rowMap[row.${i18nExtGoName}] = row
  }
  for i := range list {
    row, ok := rowMap[list[i].${idGoName}]
    if !ok {
      continue
    }
${each f in i18nFlatFields}
    list[i].${f.goName} = row.${f.goName}
${/each}
  }
  return nil
}

// GetI18nMap 查询指定记录的全语言翻译 map（key 为语言码，供详情逐语言编辑）
func (r *${repoClass}) GetI18nMap(db *gorm.DB, id ${idGoType}) (model.${i18nMapType}, error) {
  var rows []model.${i18nEntityClass}
  if err := db.Where("`${i18nExtKey}` = ?", id).Find(&rows).Error; err != nil {
    return nil, err
  }
  result := make(model.${i18nMapType}, len(rows))
  for _, row := range rows {
    result[row.${i18nLangGoName}] = row
  }
  return result, nil
}
${/if}
