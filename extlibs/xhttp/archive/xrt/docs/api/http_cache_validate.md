# HTTP 缓存验证、更新与失效

头文件：`<xrt/http_cache_validate.h>`

裁剪宏：`XRT_FEATURE_HTTP_CACHE_VALIDATE`

依赖：`http_cache_policy`、`http_precondition`、`url`

本模块实现 RFC 9111 第 3.2、4.3 和 4.4 节的协议判断，但不绑定缓存容器、
索引、磁盘格式或网络客户端。所有入口都只借用 Header 和 URI，计划、选择、
字段过滤和直接目标 URI 写出不分配内存。相对 `Location` /
`Content-Location` 的绝对化复用 URL 模块的 RFC 3986 解析器，该便利路径当前
可能使用临时内存；对失效热路径有严格分配约束的缓存可以直接消费候选视图，
交给自己的缓存键规范化层处理。

## 分层

- `xrtHttpCacheValidatePlan` 从已经完成 URI、方法和 Vary 选择的缓存条目生成
  `If-None-Match` / `If-Modified-Since` 计划。
- `xrtHttpCacheValidateETagsWrite` 写出多条缓存响应的去重 ETag 联集。
- `xrtHttpCacheIfRangePlan` 为续传选择强 ETag 或强 Last-Modified。
- `xrtHttpCachePreconditionsEvaluate` 供缓存评估客户端条件请求，不会错误评估
  只适用于源站的 `If-Match` 和 `If-Unmodified-Since`。
- `xrtHttpCache304Select`、`xrtHttpCacheHeadPlan` 和
  `xrtHttpCacheFieldUpdate` 决定如何更新已保存响应。
- `xrtHttpCacheInvalidationNext` 与 `xrtHttpCacheInvalidationWrite` 输出
  unsafe 方法成功后必须或可以失效的同源 URI。

缓存键和实体存储属于上层：候选集合在传入本模块前必须已经完成目标 URI、
方法规则和 Vary 匹配。这个边界允许内存缓存、磁盘缓存、代理缓存和应用自定义
缓存复用同一协议实现。

## 缓存条目

```c
typedef struct xhttpcacheentry {
	const xhttpfield* Fields;
	size_t FieldCount;
	xtime ResponseTime;
	uint32 Flags;
} xhttpcacheentry;
```

`Fields` 借用已保存响应 Header。`ResponseTime` 是收到或成功验证响应时的墙钟；
当条目没有有效 `Date` 时，它用于选出最新的弱验证器匹配项。

部分正文设置 `XHTTP_CACHE_ENTRY_PARTIAL`。当前 Range 完全位于已保存范围内时，
再设置 `XHTTP_CACHE_ENTRY_RANGE_COVERED`。未覆盖请求的部分条目不会把 ETag
加入验证请求。

## 验证请求

```c
xhttpcachevalidateplan Plan;

if ( xrtHttpCacheValidatePlan(
	Entries, EntryCount, false, &Plan
) == XHTTP_CACHE_VALIDATE_CONDITIONAL ) {
	/* 按 Plan.Actions 设置条件字段。 */
}
```

完整请求可以同时生成 ETag 和 Last-Modified。Range 请求仍可使用 ETag，
但不会生成 `If-Modified-Since`。这避免把弱日期验证误当成子范围表示选择。

`xrtHttpCacheValidateETagsWrite` 支持空输出长度查询，结果不含零结尾。相同
opaque-tag 的强弱 ETag 按 `If-None-Match` 的弱比较语义去重。

`xrtHttpCacheIfRangePlan` 只接受强 ETag。条目存在弱或非法 ETag 时不会退回
日期；只有完全没有 ETag，且 `Date` 至少比 `Last-Modified` 晚一秒时，日期
才能作为强 `If-Range` 验证器。

## 缓存侧条件请求

`xrtHttpCachePreconditionsEvaluate` 只对 GET/HEAD 评估
`If-None-Match` 和 `If-Modified-Since`。前者优先并使用弱 ETag 比较。
后者缺少 Last-Modified 时按 `Date`、`ResponseTime` 顺序回退。

缓存不会评估 `If-Match` 或 `If-Unmodified-Since`，因为它们只应由源站处理。
Range 的 `If-Range` 可继续复用 `<xrt/http_semantics.h>` 中的
`xrtHttpIfRangeMatch`。

## 304 更新

```c
size_t Indices[8];
size_t Selected;
xhttpcacheupdatematch Match;

Match = xrtHttpCache304Select(
	ResponseFields, ResponseFieldCount,
	Entries, EntryCount,
	Indices, 8, &Selected
);
```

选择顺序固定为：

1. 304 含强验证器时，选择全部具有相同强验证器的候选。
2. 没有强验证器但有弱验证器时，只选择匹配且 `Date` 最新的候选。
3. 304 没有任何验证器时，只有唯一候选且该候选也没有验证器才能更新。
4. 非法或重复验证器不会被当作“字段不存在”。

空 `Indices` 可查询数量。短下标数组返回范围错误且不修改数组。

## HEAD freshening

`xrtHttpCacheHeadPlan` 只处理 200 HEAD。HEAD 中实际出现的 ETag、
Last-Modified 和 Content-Length 必须在已保存响应中存在且匹配；否则条目应
标记为 stale。HEAD 不含某字段时，该字段不参与比较。

## Header 更新

更新 304、HEAD 或组合部分响应的 Header 时，逐项调用：

```c
xhttpcachefieldupdate Update = xrtHttpCacheFieldUpdate(
	NewFields, NewFieldCount, Index, Shared, Flags
);
```

函数自动跳过：

- `Content-Length`；
- `Connection` 及其提名字段；
- `Proxy-Authenticate`、`Proxy-Authentication-Info` 和
  `Proxy-Authorization`；
- 限定 `no-cache="field-name"`；
- 共享缓存中的限定 `private="field-name"`。

如果已保存正文依赖某字段，传入 `XHTTP_CACHE_UPDATE_FIELD_DEPENDENT`。
如果 HTTP 层已经消费或删除某字段，例如自动解码后删除 Content-Encoding，
传入 `XHTTP_CACHE_UPDATE_FIELD_PROCESSED`。

同名新字段应作为一个字段组处理：先删除旧组，再按新响应中的原顺序添加所有
返回 `REPLACE` 的同名字段。

## unsafe 方法失效

只有方法不安全或安全性未知，并且最终响应是 2xx 或 3xx 时才产生候选。
第一个候选始终是请求目标 URI。单个有效 `Location` 和
`Content-Location` 只有与目标 URI 同源时才返回；跨源、重复或非法字段被
忽略。

```c
xhttpcacheinvalidatecursor Cursor;
xhttpcacheinvalidateitem Item;

xrtHttpCacheInvalidationCursorInit(&Cursor);
while ( xrtHttpCacheInvalidationNext(
	Method, Status, Target, Fields, FieldCount,
	&Cursor, &Item
) == XHTTP_NEXT_ITEM ) {
	/* 使用 xrtHttpCacheInvalidationWrite 得到绝对、无 fragment 的 URI。 */
}
```

目标 URI 必须是可确定 origin 的绝对 HTTP 或 HTTPS URI。相对位置字段会按
目标 URI 解析；默认端口参与同源比较。实现只可能保守地少返回可选位置候选，
不会返回跨源候选。

完整可运行示例位于 `examples/http/cache_validate/main.c`。

不完整 200、206、multipart part、覆盖缺口和强验证器片段组合由
[`http_cache_range.md`](http_cache_range.md) 提供。

## 错误与所有权

- 所有字段、Etag 和 URI 视图都由调用方拥有。
- 修改或释放输入会使计划中借用的 ETag 失效。
- 参数和结构错误设置统一 XRT 错误。
- 线路上非法的可选验证器按不可用处理，不会越界，也不会被误当成缺失字段。
- 所有输出在短缓冲和重叠输入时保持原子。
- 条件计划、304/HEAD 选择、字段过滤和候选迭代均不分配内存；相对 URI
  写出遵循 `<xrt/url.h>` 的解析分配契约。

规范依据：

- [RFC 9111 §3.2](https://www.rfc-editor.org/rfc/rfc9111.html#section-3.2)
- [RFC 9111 §4.3](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.3)
- [RFC 9111 §4.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.4)
- [RFC 9110 §13](https://www.rfc-editor.org/rfc/rfc9110.html#section-13)
