# HTTP 缓存策略

`<xrt/http_cache_policy.h>` 在 `Cache-Control`、`Date/Age/Expires` 与新鲜寿命
协议事实之上提供 RFC 9111 存储和复用计划。模块不分配内存，不拥有响应，不选择
缓存键，也不绑定哈希表、磁盘、淘汰算法或网络客户端。

启用 `XRT_FEATURE_HTTP_CACHE_POLICY` 会依赖
`XRT_FEATURE_HTTP_CACHE_TIME`。只需要读取指令或计算年龄时，可以分别裁剪掉
策略层。

## 分层边界

缓存实现按以下顺序组合模块：

1. `xrtHttpCacheControlParse` 分别汇总请求和响应 `Cache-Control`；
2. `xrtHttpCacheTimeParse` 与 `xrtHttpCacheCurrentAge` 计算响应年龄；
3. `xrtHttpCacheFreshness` 选择显式寿命，上层也可提供启发式或扩展寿命；
4. `xrtHttpVaryPlan`、URI 和方法规则选择候选；
5. `xrtHttpCacheStorePlan` 决定是否保存以及保存前必须执行的动作；
6. `xrtHttpCacheUsePlan` 决定直接使用、验证、转发或生成 504。

URI、方法和 `Vary` 的实际比较留在键实现中，因为不同缓存可能使用规范化 URI、
双重隐私键或自定义方法。复用策略只接收
`XHTTP_CACHE_USE_CANDIDATE_MATCH` 事实。完整响应或当前 Range 已被存储表示覆盖，
由 `XHTTP_CACHE_USE_REPRESENTATION` 表达。

## 存储输入

`xrtHttpCacheStoreInputInit` 建立最常用的完整响应输入：

- GET、HEAD 和 POST 自动标记为当前策略理解且方法定义允许缓存；
- 公共状态表中的状态自动标记为已理解；
- 200、203、204、206、300、301、308、404、405、410、414 和 501 自动标记为
  可使用启发式寿命；
- Header 与响应消息默认完整；
- `Shared` 决定私有缓存或共享缓存语义。

调用方随后只设置实际存在或已经实现的能力：

| 标志 | 含义 |
| --- | --- |
| `XHTTP_CACHE_STORE_AUTHORIZATION` | 原请求含 `Authorization` |
| `XHTTP_CACHE_STORE_RANGE_SUPPORTED` | 存储实现理解 Range、Content-Range 和实际单位 |
| `XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH` | POST 的 Content-Location 与目标 URI 相同 |
| `XHTTP_CACHE_STORE_EXTENSION` | 已理解的缓存扩展明确允许存储 |
| `XHTTP_CACHE_STORE_EXTENSION_FRESHNESS` | 扩展提供 POST 可依赖的显式寿命 |
| `XHTTP_CACHE_STORE_EXTENSION_OVERRIDE` | 已实现的扩展明确接管全部标准存储约束 |

扩展方法应由调用方设置 `METHOD_UNDERSTOOD` 和 `METHOD_CACHEABLE`。扩展状态只在
206、304 或响应带有效 `must-understand` 时必须设置 `STATUS_UNDERSTOOD`。
`EXTENSION_OVERRIDE` 是有意开放的低层入口，不能仅因看到未知指令就设置；调用方
必须已经实现该扩展对所有被覆盖条件的要求。

## 存储计划

`xrtHttpCacheStorePlan` 返回：

- `XHTTP_CACHE_STORE_KEEP`：可以保存，但必须执行 `Actions`；
- `XHTTP_CACHE_STORE_SKIP`：不得保存，`Reasons` 给出全部阻断原因；
- `XHTTP_CACHE_STORE_ERROR`：参数、公开结构或别名错误，输出不变。

计划覆盖：

- 方法已理解且定义允许缓存；
- 最终状态和 206、304、`must-understand` 状态理解；
- 请求与响应 `no-store`；
- `must-understand` 对配套 `no-store` 的规范覆盖；
- 共享缓存的限定与非限定 `private`；
- 带 Authorization 请求只接受有效 `must-revalidate`、`public` 或 `s-maxage`；
- `public`、私有缓存的 `private`、Expires、max-age、共享 s-maxage、扩展许可和
  启发式状态；
- POST 的显式寿命与匹配 Content-Location；
- 206 和不完整 GET 的 Range 能力。

成功计划始终要求移除 `Connection` 及其命名字段，移除当前代理专用认证字段，
并保持 trailer 与 Header 分开。其他动作包括：

| 动作 | 存储实现责任 |
| --- | --- |
| `REMOVE_NO_CACHE` | 不保存限定 `no-cache` 列出的字段 |
| `REMOVE_PRIVATE` | 共享缓存不保存限定 `private` 列出的字段 |
| `MARK_INCOMPLETE` | 条目必须记录为不完整表示 |
| `STORE_AS_200` | 按 RFC 9111 将 206 作为不完整 200 保存 |
| `IGNORE_NO_STORE` | 已理解状态的 `must-understand` 覆盖了配套 `no-store` |

限定字段名可由 `xrtHttpCacheNext` 找到对应指令，再用
`xrtHttpParamValueWrite` 解码 quoted-string，并用 `xrtHttpTokenNext` 遍历。
实现不支持删字段时可以保守地放弃保存或要求验证，不能静默忽略动作。

## 复用输入

`xrtHttpCacheUseInputInit` 默认建立已经完成键匹配、表示可用且具有可靠时钟的环境。
调用方按真实状态清除相应位，或增加：

| 标志 | 含义 |
| --- | --- |
| `XHTTP_CACHE_USE_VALIDATED` | 候选刚刚由源站成功验证 |
| `XHTTP_CACHE_USE_DISCONNECTED` | 无法联系源站或找到转发路径 |
| `XHTTP_CACHE_USE_STALE_ALLOWED` | 已实现扩展或站外契约明确允许陈旧响应 |
| `XHTTP_CACHE_USE_EXTENSION` | 已实现扩展明确允许直接复用并接管标准限制 |
| `XHTTP_CACHE_USE_AUTHORIZATION` | 产生该存储条目的原请求含 Authorization |

`STATUS_UNDERSTOOD` 只用于合法保存了
`must-understand, no-store` 的响应。`EXTENSION` 与存储覆盖一样是低层入口，不是
未知扩展的默认行为。

## 复用计划

`xrtHttpCacheUsePlan` 返回：

| 决定 | 含义 |
| --- | --- |
| `XHTTP_CACHE_USE_STORED` | 直接生成缓存响应并执行动作 |
| `XHTTP_CACHE_USE_VALIDATE` | 向源站发送条件请求；没有 validator 时可普通转发 |
| `XHTTP_CACHE_USE_FORWARD` | 候选不匹配、表示不可用或条目禁止复用 |
| `XHTTP_CACHE_USE_GATEWAY_TIMEOUT` | `only-if-cached` 或断网状态下不能联系源站 |
| `XHTTP_CACHE_USE_ERROR` | 参数或公开结构错误，输出不变 |

策略完整处理请求 `max-age`、`min-fresh`、`max-stale`、`no-cache` 和
`only-if-cached`。请求 `no-store` 不影响已经保存的响应复用，只阻止本次响应被
保存。响应的非限定 `no-cache` 强制验证；限定形式可以直接复用，但输出层必须
执行 `XHTTP_CACHE_USE_REMOVE_NO_CACHE`。

陈旧响应只有在请求 `max-stale`、断网状态或显式
`STALE_ALLOWED` 时使用。`must-revalidate` 始终禁止未验证的陈旧复用；
`proxy-revalidate` 和 `s-maxage` 对共享缓存具有同样约束。无法验证时计划返回
504，而不是悄悄发送陈旧内容。

共享缓存还会在每次复用时重新检查原请求的 Authorization 事实，只有有效
`must-revalidate`、`public` 或 `s-maxage` 允许使用；不合规的意外条目会要求
淘汰。该事实必须和存储条目一起保存，不能在首次存储检查后丢弃。

每个缓存命中都设置 `XHTTP_CACHE_USE_SET_AGE`，输出层必须替换现有 `Age`。
陈旧命中还设置 `XHTTP_CACHE_USE_STALE`，`StaleBy` 给出超过新鲜寿命的微秒数。
意外进入存储的 `no-store` 响应设置 `XHTTP_CACHE_USE_EVICT`。

## 寿命扩展

`xrtHttpCacheFreshness` 只生成 `S_MAXAGE`、`MAX_AGE` 或 `EXPIRES`。当它返回
`XHTTP_CACHE_CALC_NONE` 时，上层可以根据 Last-Modified 和站点策略生成
`XHTTP_CACHE_FRESHNESS_HEURISTIC`；缓存扩展可以生成
`XHTTP_CACHE_FRESHNESS_EXTENSION`。两者都必须通过
`xrtHttpCacheFreshnessValid`，并遵守显式寿命优先于启发式寿命的规则。

没有配置启发式算法时，传入 `Source=NONE, Lifetime=0`，策略会把条目视为需要
验证，而不会擅自猜测寿命。它不属于 RFC 定义的陈旧响应，因此 `max-stale`、
断网和 `STALE_ALLOWED` 都不会把它变成可直接复用的条目。

## 示例

```c
xhttpcachestoreinput Input;
xhttpcachestoreplan Plan;

xrtHttpCacheStoreInputInit(
	&Input, XRT_STR_LITERAL("GET"), 200, true
);
Input.Flags |= XHTTP_CACHE_STORE_AUTHORIZATION;

if ( xrtHttpCacheStorePlan(
	&RequestControl,
	&ResponseControl,
	&ResponseTime,
	&Input,
	&Plan
) == XHTTP_CACHE_STORE_KEEP ) {
	/* 按 Plan.Actions 清理并保存响应。 */
}
```

完整可运行示例位于 `examples/http/cache_policy/main.c`。

## 安全边界

所有公开结构都在使用前验证，计划输出不能与任何输入重叠。错误路径不修改输出。
策略不把重复或非法数值指令当作可靠寿命；限定 `private` 和 `no-cache` 发生重复、
参数非法或形式冲突时采用更保守的非限定语义。

验证器生成、缓存侧条件请求、304/HEAD 更新和 unsafe 方法失效由
[`http_cache_validate.md`](http_cache_validate.md) 提供。不完整 200、206、
multipart part、实际 Range 覆盖与强验证器组合由
[`http_cache_range.md`](http_cache_range.md) 提供。缓存键、正文介质、淘汰和
持久化仍属于可替换的存储层；这些协议计划公开了后端必须遵守的决定，因此无需
复制 RFC 判断。
