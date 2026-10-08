${# ============================================================================}
${# 请求结构模板：生成 Go 请求参数（internal/model/{tableName}_in.go）        }
${# ----------------------------------------------------------------------------}
${# 作用：覆盖层，每次重新生成，请勿手改。对齐 NestJS in_*.ts 的角色。            }
${#   - SelectParams：分页 + 查询字段 + 语言码（gin ShouldBindJSON 绑定）         }
${#   - InsertParams：新增参数（required → binding:"required"，自增主键不入参）  }
${#   - UpdateParams：更新参数（除主键外全指针，nil 字段不更新）                  }
${#   - hasI18n 时定义 {Entity}I18nMap 全语言翻译 map 类型（值为翻译实体）        }
${# 数据来源（tplData）：                                                        }
${#   selClass/insClass/updClass/selFields/insFields/updFields/inImports         }
${#   hasI18n/i18nMapType/i18nEntityClass/i18nExtKey/i18nLangKey/i18nDefaultLang }
${#   idGoName/idGoType/idColName（主键）                                        }
${# ============================================================================}
// 此代码为AutoCode编译器生成，请勿手动修改
package model

${inImports}
${if hasI18n}
// ${i18nMapType} 多语言翻译数据：key 为语言码（如 zh / en），value 为翻译字段；${i18nExtKey}/${i18nLangKey} 由服务端覆盖，客户端无需传
type ${i18nMapType} map[string]${i18nEntityClass}

${/if}
// ${selClass} ${tableComment}(${tableName})查询参数，客户端查询分页数据时传输的数据格式
type ${selClass} struct {
  Page int `json:"page" binding:"omitempty,min=1"` // 页码（缺省 1）
  PageSize int `json:"pageSize" binding:"omitempty,min=1"` // 每页条数（缺省 10）
${each field in selFields}
  ${field.goName} ${field.declType} `json:"${field.name}"` // ${field.comment}
${/each}
${if hasI18n}
  Lang string `json:"lang"` // 语言码，如 zh / en，不传使用默认语言 ${i18nDefaultLang}
${/if}
}

// ${insClass} ${tableComment}(${tableName})新增参数
type ${insClass} struct {
${each field in insFields}
  ${field.goName} ${field.declType} `${field.jsonTag}${field.bindingTag}` // ${field.comment}
${/each}
${if hasI18n}
  I18n ${i18nMapType} `json:"i18n" binding:"omitempty"` // 多语言翻译数据，如 {"zh": {"name": "中文名"}, "en": {"name": "Name"}}；${i18nExtKey}/${i18nLangKey} 由服务端覆盖
${/if}
}

// ${updClass} ${tableComment}(${tableName})更新参数（除主键外全部指针，nil 字段不更新）
type ${updClass} struct {
  ${idGoName} ${idGoType} `json:"${idColName}" binding:"required"` // 主键
${each field in updFields}
  ${field.goName} ${field.ptrType} `${field.jsonTag}` // ${field.comment}
${/each}
${if hasI18n}
  I18n ${i18nMapType} `json:"i18n" binding:"omitempty"` // 多语言翻译数据
${/if}
}
