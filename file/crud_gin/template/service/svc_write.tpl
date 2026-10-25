${# ============================================================================}
${# 写路径骨架（split 布局）：modules/admin/{dataDir}/{tableName}_service.go     }
${# ----------------------------------------------------------------------------}
${#   - 仅首次生成（fileExists 守卫），之后重新生成不会覆盖                     }
${#   - 提供默认 Save/Delete，保证新表生成后即可编译；业务逻辑在此手改          }
${# 数据来源：pkgName/serviceClass/updClass/idGoType                           }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件
// ${tableClass} 写路径（Save/Delete）：生成器只在首次创建骨架，之后不再覆盖。
package ${pkgName}

// Save 保存（save 单接口）：Id=0 走新增，Id>0 走更新
func (s *${serviceClass}) Save(data *${updClass}) (${idGoType}, error) {
	if data.Id == 0 {
		return s.Repo.Insert(s.DB, data.ToEntity())
	}
	return s.Repo.UpdateById(s.DB, data.Id, data.ToUpdates())
}

// Delete 删除一条或多条记录，返回受影响行数
func (s *${serviceClass}) Delete(ids []${idGoType}) (int64, error) {
	return s.Repo.Delete(s.DB, ids)
}
${/if}
