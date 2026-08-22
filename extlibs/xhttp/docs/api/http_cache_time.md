# HTTP 缓存时间

`<xrt/http_cache_time.h>` 提供独立于网络、缓存存储和淘汰算法的 HTTP 缓存
时间协议层。模块复用 `<xrt/http_cache.h>` 的 `Cache-Control` 汇总和
`<xrt/time.h>` 已压实的三种 HTTP 日期解析，不分配内存。

启用 `XRT_FEATURE_HTTP_CACHE_TIME` 会依赖 `XRT_FEATURE_HTTP_CACHE` 和
`XRT_FEATURE_TIME_TEXT`。只需要解析缓存指令而不计算时间时，可以只启用较轻的
`XRT_FEATURE_HTTP_CACHE`。

## 元数据

`xrtHttpCacheTimeParse` 一次扫描 `xhttpfield[]`，建立 `xhttpcachetime`：

| 成员 | 含义 |
| --- | --- |
| `Date` | 第一个 `Date` 在有效时的值，Unix Epoch 微秒 |
| `Expires` | 第一个 `Expires` 在有效时的值，Unix Epoch 微秒 |
| `Age` | 合并 `Age` 列表的第一个非空成员，单位为秒 |
| `DateCount` | `Date` 字段行数 |
| `AgeCount` | `Age` 字段行数 |
| `AgeMemberCount` | 合并后非空 `Age` 成员数 |
| `ExpiresCount` | `Expires` 字段行数 |
| `Flags` | 字段存在、重复成员和非法值事实 |

`Date` 和 `Expires` 接收 IMF-fixdate、RFC 850 与 asctime 三种 HTTP 日期，
时区始终按 GMT 处理。重复或非法日期不会让字段扫描失败，而是分别设置
`XHTTP_CACHE_TIME_*_DUPLICATE` 或 `XHTTP_CACHE_TIME_*_INVALID`。

`Age` 是列表型单值字段。解析器跳过空列表成员，只采用第一个非空成员，后续成员
通过 `XHTTP_CACHE_TIME_AGE_EXTRA` 报告。第一个成员非法时设置
`XHTTP_CACHE_TIME_AGE_INVALID`；年龄计算按 RFC 9111 忽略它并使用零。

`xrtHttpCacheTimeInit` 建立空元数据，`xrtHttpCacheTimeValid` 可在组合低层 API
之前验证公开结构的一致性。

## 当前年龄

`xrtHttpCacheCurrentAge` 完整实现 RFC 9111 的保守公式：

```text
apparent_age = max(0, response_time - date_value)
response_delay = response_clock - request_clock
corrected_age_value = age_value + response_delay
corrected_initial_age = max(apparent_age, corrected_age_value)
resident_time = now_clock - response_clock
current_age = corrected_initial_age + resident_time
```

`ResponseTime` 是收到响应时的 Unix Epoch 微秒墙钟。`RequestClock`、
`ResponseClock` 和 `NowClock` 必须来自同一个 `xrtClock()` 单调时钟，且保持
非递减顺序。分开两个时钟域可以避免系统时间校准使驻留时间变负。

`xhttpcacheage` 公开公式的全部中间量，单位均为微秒。`CurrentAgeSeconds` 是可用于
生成 `Age` 字段的饱和秒数。所有累加都检测溢出并饱和，不会回绕为较年轻的响应。

不存在 `Date` 或单个 `Date` 非法时使用 `ResponseTime`。非法或重复 `Age`
不阻断计算；重复 `Date` 会返回 `XHTTP_CACHE_CALC_INVALID`，由缓存保守地
重新验证。非法事实仍保留在 `Flags` 中，计算时的替代不会隐藏输入问题。

## 显式新鲜寿命

`xrtHttpCacheFreshness` 只计算协议明确给出的寿命，不擅自绑定启发式算法：

| 缓存类型 | 选择顺序 |
| --- | --- |
| 共享缓存 | `s-maxage`、`max-age`、`Expires - Date` |
| 私有缓存 | `max-age`、`Expires - Date` |

如果缺少 `Date` 或单个 `Date` 非法，`Expires` 以 `ResponseTime` 为基准；
过期时间不晚于基准时，寿命为零。单个非法 `Expires` 按 RFC 9111 解释为已经
过期，同样返回零寿命。选中的数值指令非法或重复、`Date` 或 `Expires` 重复时
返回 `XHTTP_CACHE_CALC_INVALID`。未选中的非法指令不污染更高优先级的有效结果。

`xhttpcachefreshness.Source` 说明寿命来自 `S_MAXAGE`、`MAX_AGE` 或
`EXPIRES`。没有显式寿命时返回 `XHTTP_CACHE_CALC_NONE`，调用方可以选择自己的
启发式策略；本模块不会把策略假装成协议事实。

上层生成的启发式与缓存扩展寿命分别使用
`XHTTP_CACHE_FRESHNESS_HEURISTIC` 和 `XHTTP_CACHE_FRESHNESS_EXTENSION`。
`xrtHttpCacheAgeValid` 与 `xrtHttpCacheFreshnessValid` 允许策略或自定义存储在
组合前验证公开结果；`NONE` 只允许零寿命。

`xrtHttpCacheFresh` 实现规范中的严格比较：

```text
freshness_lifetime > current_age
```

年龄等于寿命时已经陈旧。

## 返回状态

| 状态 | 含义 |
| --- | --- |
| `XHTTP_CACHE_CALC_ERROR` | 参数、别名或单调时钟顺序错误，已设置 XRT 错误 |
| `XHTTP_CACHE_CALC_NONE` | 响应没有显式新鲜寿命 |
| `XHTTP_CACHE_CALC_READY` | 输出结构已完整写入 |
| `XHTTP_CACHE_CALC_INVALID` | 选中的线路元数据不可靠，应保守处理 |

`ERROR`、`NONE` 和 `INVALID` 都不修改结果输出，便于调用方保留上一次可用值或统一
执行回退路径。

## 示例

```c
xhttpcachecontrol Control;
xhttpcachetime Time;
xhttpcacheage Age;
xhttpcachefreshness Freshness;
uint64 RequestClock;
uint64 ResponseClock;
uint64 NowClock;
xtime ResponseTime;

if ( xrtHttpCacheControlParse(Fields, FieldCount, &Control) &&
	xrtHttpCacheTimeParse(Fields, FieldCount, &Time) &&
	(xrtHttpCacheCurrentAge(
		&Time,
		ResponseTime,
		RequestClock,
		ResponseClock,
		NowClock,
		&Age
	 ) == XHTTP_CACHE_CALC_READY) &&
	(xrtHttpCacheFreshness(
		&Control,
		&Time,
		ResponseTime,
		false,
		&Freshness
	 ) == XHTTP_CACHE_CALC_READY) ) {
	bool Fresh = xrtHttpCacheFresh(&Age, &Freshness);
}
```

完整可运行示例位于 `examples/http/cache_time/main.c`。

## 边界

字段数组和输出结构不能覆盖彼此，所有写出保持失败原子性。模块不拥有输入字段
文本，解析结束后只保存数值和计数，不保留借用指针。

缓存键、`Vary` 请求字段匹配、启发式算法、重新验证线路、淘汰和持久化不属于
本模块。可存储性、请求指令和陈旧响应许可由
[`http_cache_policy.md`](http_cache_policy.md) 组合本模块的公开结果，但不会
反向改变时间协议契约。
