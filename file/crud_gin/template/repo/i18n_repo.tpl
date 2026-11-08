${# ============================================================================}
${# i18n 主表仓储补充：data/{dataDir}/{tableName}_repo_i18n_.go                  }
${# ----------------------------------------------------------------------------}
${#   - 翻译实体 en_{i18nTable}_.go 与主表同包（翻译表不单独成包）               }
${#   - fillI18n：列表批量平铺当前语言文本（缺失回退 defaultLang）               }
${#   - fillI18nDetail：详情平铺默认语言文本 + 附全语言 map                      }
${#   - selectI18nByExtIds / selectI18nByExtId / upsertI18n / deleteI18nByExtIds  }
${#     翻译行读写统一挂在主表 Repo 上（写路径由 service 事务调用）               }
${# 数据来源：pkgName/repoClass/listType/enClass/idGoType/idGoName/             }
${#           i18nEnClass/i18nExtGoName/i18nLangGoName/i18nDefaultLang/        }
${#           i18nIdExpr/i18nAssignLines/i18nDetailAssignLines/i18nMapAssignLine/ }
${#           i18nTypeName/i18nExtGoType/i18nUpdateCols                          }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

import (
	"gorm.io/gorm/clause"

	"${dbPath}"
)

// fillI18n 批量平铺当前语言文本（lang 空=默认语言；当前语言缺失回退默认语言）
func (r *${repoClass}) fillI18n(list []${listType}, lang string) error {
	if len(list) == 0 {
		return nil
	}
	if lang == "" {
		lang = "${i18nDefaultLang}"
	}
	ids := make([]${idGoType}, 0, len(list))
	for i := range list {
		ids = append(ids, ${i18nIdExpr})
	}
	rows, err := r.selectI18nByExtIds(ids)
	if err != nil {
		return err
	}
	byExt := make(map[${idGoType}]map[string]${i18nEnClass}, len(ids))
	for _, tr := range rows {
		m := byExt[tr.${i18nExtGoName}]
		if m == nil {
			m = make(map[string]${i18nEnClass})
			byExt[tr.${i18nExtGoName}] = m
		}
		m[tr.${i18nLangGoName}] = tr
	}
	for i := range list {
		tr, ok := byExt[ids[i]][lang]
		if !ok {
			tr, ok = byExt[ids[i]]["${i18nDefaultLang}"]
		}
		if ok {
${each line in i18nAssignLines}
${line}
${/each}
		}
	}
	return nil
}

// fillI18nDetail 详情：平铺默认语言文本 + 附全语言翻译 map（供管理端逐语言编辑）
func (r *${repoClass}) fillI18nDetail(e *${enClass}) error {
	if e == nil {
		return nil
	}
	rows, err := r.selectI18nByExtId(e.${idGoName})
	if err != nil {
		return err
	}
	e.I18n = make(map[string]${i18nTypeName}, len(rows))
	for _, tr := range rows {
${i18nMapAssignLine}
	}
	for _, tr := range rows {
		if tr.${i18nLangGoName} == "${i18nDefaultLang}" {
${each line in i18nDetailAssignLines}
${line}
${/each}
			break
		}
	}
	return nil
}

// selectI18nByExtIds 按关联ID批量查询翻译行（语言过滤由调用方处理）
func (r *${repoClass}) selectI18nByExtIds(extIds []${i18nExtGoType}) ([]${i18nEnClass}, error) {
	var rows []${i18nEnClass}
	if len(extIds) == 0 {
		return rows, nil
	}
	if err := cdb.WhereIn(r.db, ${i18nColPrefix}${i18nExtGoName}, extIds).Find(&rows).Error; err != nil {
		return nil, cdb.WrapErr("${tableName}.selectI18nByExtIds", err)
	}
	return rows, nil
}

// selectI18nByExtId 查询某条记录的全部语言翻译
func (r *${repoClass}) selectI18nByExtId(extId ${i18nExtGoType}) ([]${i18nEnClass}, error) {
	var rows []${i18nEnClass}
	if err := cdb.WhereEq(r.db, ${i18nColPrefix}${i18nExtGoName}, extId).Find(&rows).Error; err != nil {
		return nil, cdb.WrapErr("${tableName}.selectI18nByExtId", err)
	}
	return rows, nil
}

// upsertI18n 按 (${i18nExtGoName}, ${i18nLangGoName}) 唯一键 upsert 单语言翻译行
func (r *${repoClass}) UpsertI18n(e *${i18nEnClass}) error {
	if e == nil {
		return ErrNilEntity
	}
	if err := r.db.Clauses(clause.OnConflict{
		Columns:   []clause.Column{{Name: ${i18nColPrefix}${i18nExtGoName}}, {Name: ${i18nColPrefix}${i18nLangGoName}}},
		DoUpdates: clause.AssignmentColumns([]string{${i18nUpdateCols}}),
	}).Create(e).Error; err != nil {
		return cdb.WrapErr("${tableName}.UpsertI18n", err)
	}
	return nil
}

// deleteI18nByExtIds 按关联ID删除全部语言翻译（主表删除时级联）
func (r *${repoClass}) DeleteI18nByExtIds(extIds []${i18nExtGoType}) (int64, error) {
	if len(extIds) == 0 {
		return 0, nil
	}
	n, err := cdb.DeleteByIn(r.db, &${i18nEnClass}{}, ${i18nColPrefix}${i18nExtGoName}, extIds)
	return n, cdb.WrapErr("${tableName}.DeleteI18nByExtIds", err)
}
