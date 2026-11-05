${# ============================================================================}
${# Cache 骨架模板：data/{dataDir}/{tableName}_cache.go（手写件，仅首次生成）    }
${# ----------------------------------------------------------------------------}
${#   - genCache=true 才生成；fileExists 守卫，存在即跳过，永不覆盖               }
${#   - 生成可直接使用的 XxxCache：读穿透 + 单条/批量失效，key 拼接收口           }
${#   - 全表/列表缓存按需在文件内追加，key 用 cache:<表>:all / :list 等视图名     }
${# 数据来源：pkgName/tableName/tableComment/tableClass/enClass/redisxPath     }
${# ============================================================================}
${if !fileExists(outputPath)}
// 手写扩展件：可手动修改，重新生成不会覆盖本文件。
// ─────────────────────────────────────────────────────────────────────
// ${tableComment}(${tableName}) 缓存层：归属本表（依赖表：${tableName}）
// key 统一 "cache:${tableName}:<视图>:<参数>"，本文件内集中定义，禁止手写前缀
// 读：GetOrLoad 读穿透（miss 回源并回填）；写：事务提交后再 Invalidate
// 只放本表缓存；多表且无主数据源的统计/报表放 data/query，不要塞进表包
// admin / user_ext 等强一致表不要勾选生成
// ─────────────────────────────────────────────────────────────────────
package ${pkgName}

import (
	"time"

	"${redisxPath}"
)

const (
	cache${tableClass}Prefix = redisx.CacheKeyRoot + "${tableName}:"
	cache${tableClass}ById   = cache${tableClass}Prefix + "by_id:"
	cache${tableClass}TTL    = 10 * time.Minute // 缓存过期时间（10 分钟）
)

// ${tableClass}Cache 本表读缓存：key 拼接与读穿透收口在这里，业务只调 Get / Invalidate。
type ${tableClass}Cache struct {
	repo *${tableClass}Repo
}

// New${tableClass}Cache 创建缓存对象（与 repo 共用同一 DB 连接来源）
func New${tableClass}Cache(repo *${tableClass}Repo) *${tableClass}Cache {
	return &${tableClass}Cache{repo: repo}
}

// GetById 按主键读缓存：命中直接返回；miss 回源并回填（查不到不回填）
// repo 自带连接，且每次查出来的实体都是新实例（快照基线是实体私有字段）：
// 业务改字段前会自行 TakeSnapshot，不会污染缓存值，所以不需要 clone / capture，直接返回查询结果
func (c *${tableClass}Cache) GetById(id ${idGoType}) (*${enClass}, error) {
	return redisx.GetOrLoad(c.keyById(id), cache${tableClass}TTL, func() (*${enClass}, error) {
		return c.repo.SelectById(id)
	})
}

// InvalidateById 失效单条；在 service 的写事务提交之后调用（不要放在 tx 内）
func (c *${tableClass}Cache) InvalidateById(id int64) {
	_ = redisx.DelCache(c.keyById(id))
}

// InvalidateByIds 批量失效（删除 / 批量更新后调用）
func (c *${tableClass}Cache) InvalidateByIds(ids []int64) {
	_ = redisx.DelCacheByIds(cache${tableClass}ById, ids)
}

// keyById 单条 key 的唯一拼接点（新增其它视图时按同样方式补 keyXxx）
func (c *${tableClass}Cache) keyById(id int64) string {
	return redisx.CacheKeyById(cache${tableClass}ById, id)
}
${/if}
