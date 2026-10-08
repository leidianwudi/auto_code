${# ============================================================================}
${# 实体模板：生成 Go gorm 实体（internal/model/en_{tableName}.go）          }
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成，请勿手改。                                        }
${#   - struct + gorm/json 标签（列→goType 映射见 core/param_go.ac）             }
${#   - 可空列 → 指针类型（如 *string）；json 列 → datatypes.JSON                }
${#   - hasI18n 时含 transient 平铺字段（gorm:"-"）与全语言翻译 I18n 字段         }
${#   - 翻译表实体（如 en_shop0.go）也由本模板生成（hasI18n=false）               }
${# 数据来源（tplData）：                                                        }
${#   entityClass / tableComment / tableName / fields / entityImports            }
${#   hasI18n / i18nFlatFields / i18nMapType / i18nDefaultLang                   }
${#   field 各键：goName / declType / gormTag / jsonTag / comment                }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package model

${entityImports}
// ${tableComment}(${tableName}) 实体
type ${entityClass} struct {
${each field in fields}
  ${field.goName} ${field.declType} `${field.gormTag} ${field.jsonTag}`${if field.comment} // ${field.comment}${/if}
${/each}
${if hasI18n}
${# ── i18n 多语言 transient 字段（非数据库列，gorm insert/update/find 均忽略）─  }
${# 平铺字段由 repo 层查询翻译表后填入当前语言文本（缺失回退默认语言），          }
${# I18n map 由 SelectById 填充全语言翻译，供管理端逐语言编辑                    }
${each f in i18nFlatFields}
  ${f.goName} ${f.declType} `${f.gormTag} ${f.jsonTag}`${if f.comment} // ${f.comment}（当前语言平铺，缺失回退 ${i18nDefaultLang}）${/if}
${/each}
  I18n ${i18nMapType} `gorm:"-" json:"i18n"` // 全语言翻译数据（key 为语言码，如 {"zh": {...}, "en": {...}}）
${/if}
}
