${# ============================================================================}
${# i18n 保存/删除事务：modules/admin/{dataDir}/{tableName}_service_i18n_.go    }
${# ----------------------------------------------------------------------------}
${#   - saveI18n：主表 Insert/Update 与翻译表 upsert 同一事务                  }
${#   - deleteI18n：主表删除 + 翻译行级联删除同一事务                           }
${#   - 翻译实体与仓储方法都在主表 data 包内（不跨包、不单独成表）               }
${# 数据来源：pkgName/serviceClass/updClass/dataAlias/dataPath/repoClass/       }
${#           idGoType/idGoName/idZeroExpr/i18nTypeName/i18nEnClass/            }
${#           i18nExtGoName/i18nLangGoName/i18nTransAssignLines/                 }
${#           i18nNeedTime/i18nCreateTimeLines                                 }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package ${pkgName}

import (
${if i18nNeedTime}	"time"

${/if}	"gorm.io/gorm"

	${dataAlias} "${dataPath}"
)

// saveI18n 主表与翻译表同事务写入（Id=0 新增；Id>0 查库 → 快照 → 应用入参 → diff 更新）
func (s *${serviceClass}) saveI18n(data *${updClass}) (${idGoType}, error) {
	var savedId ${idGoType}
	err := s.DB.Transaction(func(tx *gorm.DB) error {
		repo := s.Repo.Tx(tx)
		if data.${idGoName} == ${idZeroExpr} {
			e := data.ToEntity()
${each line in i18nCreateTimeLines}
${line}
${/each}			if err := repo.Insert(e); err != nil {
				return err
			}
			savedId = e.${idGoName}
		} else {
			e, err := repo.SelectById(data.${idGoName})
			if err != nil {
				return err
			}
			e.TakeSnapshot()
			data.ApplyTo(e)
			if _, err := repo.Update(e); err != nil {
				return err
			}
			savedId = data.${idGoName}
		}
		return s.upsertI18n(repo, savedId, data.I18n)
	})
	if err != nil {
		return ${idZeroExpr}, err
	}
	return savedId, nil
}

// upsertI18n 按语言 upsert 翻译行（ext/lang 由服务端覆盖）
func (s *${serviceClass}) upsertI18n(repo *${dataAlias}.${repoClass}, extId ${idGoType},
	m map[string]${dataAlias}.${i18nTypeName}) error {
	if len(m) == 0 {
		return nil
	}
	for lang, tr := range m {
		if lang == "" {
			continue
		}
		e := &${dataAlias}.${i18nEnClass}{}
		e.${i18nExtGoName} = extId
		e.${i18nLangGoName} = lang
${each line in i18nTransAssignLines}
${line}
${/each}
		if err := repo.UpsertI18n(e); err != nil {
			return err
		}
	}
	return nil
}

// deleteI18n 主表与翻译表同事务删除
func (s *${serviceClass}) deleteI18n(ids []${idGoType}) (int64, error) {
	var n int64
	err := s.DB.Transaction(func(tx *gorm.DB) error {
		repo := s.Repo.Tx(tx)
		var err error
		n, err = repo.Delete(ids)
		if err != nil {
			return err
		}
		_, err = repo.DeleteI18nByExtIds(ids)
		return err
	})
	return n, err
}
