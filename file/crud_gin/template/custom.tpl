${# ============================================================================}
${# Custom 模板：生成模块自定义层骨架（modules/{moduleDir}/{tableName}_custom.go）}
${# ----------------------------------------------------------------------------}
${# 作用：手改层骨架，只生成一次（fileExists 守卫，存在即跳过，永不覆盖）。        }
${#   覆盖层（model/dto/repo/service/handler/router 七文件）每次重新生成会覆盖，  }
${#   自定义逻辑全部写在本文件：                                                  }
${#   - 业务钩子：BeforeInsert/AfterInsert/BeforeUpdate/AfterUpdate/             }
${#     BeforeDelete/AfterDelete（包级函数变量，init() 中赋值）                   }
${#   - 自定义路由：CustomRoutes（追加进 /{tableName} 路由组，同挂 JWT）          }
${#   - 新增方法：Go 同包多文件，可直接为 {repoClass}/{serviceClass}/{handlerClass}}
${#     定义新方法（禁止与覆盖层同名方法 = 重复定义编译错误）                      }
${# 数据来源（tplData）：                                                        }
${#   pkgName/entityClass/repoClass/serviceClass/handlerClass/tableName          }
${#   insClass/updClass/selClass/idGoType                                        }
${# ============================================================================}
${if !fileExists(outputPath)}
// 此代码为AutoCode编译器生成（骨架，仅首次生成，之后重新生成不会覆盖本文件）
// ─────────────────────────────────────────────────────────────────────
// ${entityClass} 模块自定义层说明（Go 同包多文件，本文件与覆盖层同包）：
//   1. 覆盖层 model.go/dto.go/repo.go/service.go/handler.go/router.go
//      每次重新生成会被覆盖，请勿手改
//   2. 自定义逻辑都写在本文件：
//      - 业务钩子：BeforeInsert/AfterInsert/BeforeUpdate/AfterUpdate/
//        BeforeDelete/AfterDelete（包级函数变量，在 init() 中赋值）
//      - 自定义路由：CustomRoutes（追加到 /${tableName} 路由组，同挂 JWT）
//      - 新增方法：直接为 ${repoClass}/${serviceClass}/${handlerClass} 定义方法
//   3. 使用第三方包时自行在本文件补 import 块
// ─────────────────────────────────────────────────────────────────────
package ${pkgName}

// （示例）新增前钩子 + 自定义路由（按需取消注释并补 import）：
// func init() {
//   BeforeInsert = func(data *${insClass}) error {
//     return nil
//   }
//   CustomRoutes = func(g *gin.RouterGroup, h *${handlerClass}) {
//     g.POST("/custom", h.CustomAction)
//   }
// }
//
// （示例）为服务层新增方法：
// func (s *${serviceClass}) DoSomething(id ${idGoType}) (*${entityClass}, error) {
//   return s.Repo.SelectById(s.DB, id)
// }
${/if}
