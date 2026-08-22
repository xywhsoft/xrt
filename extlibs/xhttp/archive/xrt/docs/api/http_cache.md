# HTTP Cache-Control

`<xrt/http_cache.h>` 提供独立于网络、客户端、服务端和缓存存储的
`Cache-Control` 协议层。它复用 `<xrt/http.h>` 的通用 directive 与
quoted-string 解析器，所有读取和汇总操作都不分配内存。

## 分层

需要处理任意 `name[=value]` 列表时，直接使用 `xrtHttpDirectiveNext`。这一层
不了解缓存语义，适合私有协议字段。

`xrtHttpCacheNext` 在该基础上跨越重复 `Cache-Control` 字段，返回原名称、原值、
参数标志和已知指令枚举。未知扩展仍返回 `XHTTP_NEXT_ITEM`，代理和框架可以原样
保留或交给应用处理。

`xrtHttpCacheControlInit` 与 `xrtHttpCacheControlAdd` 适合 Header 到达时增量合并。
`xrtHttpCacheControlParse` 则一次扫描字段数组。两条路径生成相同的
`xhttpcachecontrol` 协议事实，不决定缓存是否命中、是否存储或如何重新验证。

当前年龄和显式新鲜寿命由可独立裁剪的
[`http_cache_time.md`](http_cache_time.md) 提供。
存储、复用、陈旧许可和 `only-if-cached` 决策由
[`http_cache_policy.md`](http_cache_policy.md) 提供。
条件验证、304/HEAD 更新和 unsafe 方法失效由
[`http_cache_validate.md`](http_cache_validate.md) 提供。

## 汇总语义

`Flags` 中的 `XHTTP_CACHE_MAX_AGE`、`XHTTP_CACHE_NO_STORE` 等位表示对应已知指令
至少出现一次。数值指令保留第一次出现的值；重复项由 `XHTTP_CACHE_DUPLICATE`
和 `DuplicateCount` 报告，调用方可以把响应视为陈旧、拒绝它，或采用更保守策略。
`DuplicateDirectives` 精确指出哪些已知指令发生重复，策略层不需要再次扫描全部
字段。

已知指令的参数不符合语义时，字段列表解析仍成功，但设置
`XHTTP_CACHE_INVALID` 并增加 `InvalidCount`。这一区分让诊断工具仍能看到完整事实，
同时让自动压缩、缓存和代理拒绝不可靠的控制信息。通用列表语法损坏时函数失败，
且不会修改调用方原有汇总。
`InvalidDirectives` 精确指出参数非法的已知指令；例如共享缓存可以在有效
`s-maxage` 已被选中时忽略无关的非法 `max-age`。

`no-cache` 与 `private` 支持无参数形式，也接收 token 或 quoted-string 字段名列表。
带字段名时分别设置 `XHTTP_CACHE_NO_CACHE_FIELDS` 和
`XHTTP_CACHE_PRIVATE_FIELDS`。同时出现 `public` 与 `private` 会设置
`XHTTP_CACHE_CONFLICT`，具体保守策略仍由上层决定。

## 数值

`xrtHttpCacheDeltaParse` 严格解析一个完整字段值并允许两端 OWS。
`xrtHttpCacheDeltaRead` 接收缓存指令中的 token 和 quoted-string 两种
`delta-seconds` 表示。
纯数字超过实现安全范围时按 RFC 9111 饱和到
`XHTTP_CACHE_DELTA_MAX`，即 `2147483648`；符号、空值和非数字内容会失败。
`max-stale` 没有参数时不调用该函数，汇总会设置
`XHTTP_CACHE_MAX_STALE_ANY`。

```c
static const xhttpfield Fields[] = {
	{
		XRT_STR_INIT("Cache-Control"),
		XRT_STR_INIT("max-age=60, no-transform")
	},
	{
		XRT_STR_INIT("cache-control"),
		XRT_STR_INIT("x-storage=memory")
	}
};
xhttpcachecontrol Control;

if ( xrtHttpCacheControlParse(Fields, 2, &Control) ) {
	if ( (Control.Flags & XHTTP_CACHE_INVALID) == 0 ) {
		/* 在这里应用客户端、服务端或代理自己的缓存策略。 */
	}
}
```

## 边界与所有权

游标条目借用 Header 文本，只在原字段存储仍有效时使用。字段数组、游标、条目、
汇总和数值输出不能相互覆盖；别名参数会在读取前被拒绝。空字段是存在但没有指令
的合法列表，`FieldCount` 会增加而 `DirectiveCount` 保持不变。

本模块没有缓存键、存储容器、淘汰或 `Vary` 字段值匹配。独立的
[`http_vary.md`](http_vary.md) 已提供选择字段枚举和星号语义；
[`http_cache_time.md`](http_cache_time.md) 提供当前年龄和显式新鲜寿命；
[`http_cache_policy.md`](http_cache_policy.md) 组合这些事实生成存储与复用计划；
[`http_cache_validate.md`](http_cache_validate.md) 生成验证、更新与失效计划；
[`http_cache_range.md`](http_cache_range.md) 规范化不完整响应、覆盖缺口和
强验证器片段组合。
需要现成的有界内存存储时使用
[`http_cache_store.md`](http_cache_store.md)，它提供主键、`Vary` 变体、
不可变记录、引用生命周期和 LRU，但不改变本模块的零分配协议边界。缓存实现仍应
按各请求字段自己的语义归一化缓存键。
