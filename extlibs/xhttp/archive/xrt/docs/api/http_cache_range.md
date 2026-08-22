# HTTP 缓存部分响应

头文件：`<xrt/http_cache_range.h>`

裁剪宏：`XRT_FEATURE_HTTP_CACHE_RANGE`

依赖：`http_cache_validate`、`http_range`

本模块实现 RFC 9111 第 3.3、3.4 节和 RFC 9110 第 15.3.7.3 节规定的
不完整响应存储与部分内容组合。模块不拥有正文，不建立缓存键、索引或淘汰器，
也不要求内存缓存或磁盘缓存；它只把已解析响应规范化为片段，并输出存储后端
必须执行的区间和 Header 动作。

## 分层

- `xrtHttpCacheFragmentPlan` 把完整或截断 200、单段 206、multipart 中的一个
  part 统一为 `xhttpcachefragment`。
- `xrtHttpCacheCoverageCovers` 判断部分条目能否完整满足一个已解析字节范围。
- `xrtHttpCacheMissingNext` 迭代尚未保存的连续缺口，可直接生成后续 Range 请求。
- `xrtHttpCacheCombinePlan` 验证强验证器、完整长度和规范区间，输出增量替换窗口。
- Header 更新继续使用 `xrtHttpCacheFieldUpdate`，不会出现第二套字段过滤实现。

这些入口全部不分配内存。正文可以保存到稀疏文件、固定块、引用计数缓冲、
数据库或应用自定义存储。

本模块不重复判断完整缓存策略。调用 `xrtHttpCacheFragmentPlan` 前，必须已经用
缓存策略层确认请求与响应可以存储，并完成缓存键、Vary 变体、授权和共享缓存
约束。`XHTTP_CACHE_FRAGMENT_STORE` 只表示收到的正文能安全规范化为缓存片段，
不表示 `no-store`、`private` 等策略已经自动通过。

## 片段输入

```c
xhttpcachefragmentinput Input;
xhttpcachefragmentplan Plan;

xrtHttpCacheFragmentInputInit(&Input);
Input.Method = XRT_STR_LITERAL("GET");
Input.Status = 206;
Input.Fields = Fields;
Input.FieldCount = FieldCount;
Input.BodySize = ReceivedBytes;
Input.Flags =
	XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
	XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;

if ( xrtHttpCacheFragmentPlan(
	&Input, &Plan
) == XHTTP_CACHE_FRAGMENT_STORE ) {
	/* 在 Plan.Fragment.Range 对应偏移保存正文。 */
}
```

`BodySize` 是移除 HTTP 传输分帧后、尚未执行 Content-Encoding 变换的表示字节
数量。已经解压或进行其他表示变换时必须设置
`XHTTP_CACHE_FRAGMENT_TRANSFORMED`；部分响应会被保守跳过，因为原
Content-Range 和验证器不再描述变换后的字节。

只有完整接收 Header 的 GET 200/206 可以产生片段：

- 完整 200 形成完整表示；没有 Content-Length 时以实际正文长度为完整长度。
- 截断 200 形成从零开始的前缀；Content-Length 存在时保留完整表示长度。
- 单段 206 从响应 Header 读取唯一 `Content-Range`。
- 截断 206 只保存实际收到的声明范围前缀。
- multipart 206 设置 `XHTTP_CACHE_FRAGMENT_MULTIPART_PART`，外层 Header
  放入 `Fields`，单个 part Header 放入 `RangeFields`。强验证器来自外层响应，
  区间来自 part。

重复或非法 Content-Range、矛盾 Content-Length、正文长度不符、空的截断响应
均返回 `XHTTP_CACHE_FRAGMENT_SKIP`。`Reasons` 保留全部高层跳过原因，正常
协议拒绝不会污染线程错误。

## 片段动作

`xhttpcachefragmentplan.Actions` 描述首次存储时的 Header 规范化：

| 动作 | 含义 |
| --- | --- |
| `AS_200` | 把 206 按不完整 200 保存 |
| `MARK_INCOMPLETE` | 条目不能满足普通完整 GET |
| `REMOVE_CONTENT_RANGE` | Content-Range 已被处理，不能保留为 200 元数据 |
| `REMOVE_CONTENT_LENGTH` | 删除只描述 206 消息正文的长度 |
| `SET_CONTENT_LENGTH` | 已覆盖完整表示，写入完整长度 |

片段的 `Entry.Flags` 会同步设置 `XHTTP_CACHE_ENTRY_PARTIAL`。调用
`xrtHttpCacheValidatePlan` 前，只有请求范围被覆盖时才为临时条目视图增加
`XHTTP_CACHE_ENTRY_RANGE_COVERED`。

## 规范覆盖集

`xhttpcachecoverage` 借用按起点排序、互不重叠且不相邻的闭区间数组：

```c
const xhttpbyterange Ranges[] = {
	{ 0, 4095 },
	{ 8192, 12287 }
};
xhttpcachecoverage Coverage = {
	Ranges, 2, 16384, true
};
```

相邻区间必须预先合并，这使命中判断、缺口迭代和持久化格式具有唯一表示。
`HasLength` 为 true 时，每个区间的末端必须小于 `Length`。长度为零的完整表示
使用空区间数组和 `Length=0`。

`xrtHttpCacheCoverageCovers` 只在一个连续已存区间完整包含目标时返回 HIT。
跨越任何缺口都返回 MISS。

## 缺口迭代

```c
xhttpcachemissingcursor Cursor;
xhttpbyterange Missing;

xrtHttpCacheMissingCursorInit(&Cursor);
while ( xrtHttpCacheMissingNext(
	&Coverage, &Wanted, &Cursor, &Missing
) == XHTTP_NEXT_ITEM ) {
	/* 为 Missing 生成后续 Range 请求。 */
}
```

游标、覆盖集和目标在迭代期间必须保持不变。实现显式处理
`UINT64_MAX` 末端，不会在 `Last + 1` 时回绕。

## 组合计划

`xrtHttpCacheCombinePlan` 要求现有片段已经按区间规范排序，并属于同一强验证器。
调用方还必须保证全部片段属于同一缓存键、Vary 变体和表示编码。新片段和现有
片段只有满足以下条件才能组合：

1. 全部片段共享相同强 ETag；或者
2. 全部片段都具有可证明为强验证器的相同 Last-Modified；并且
3. 已知完整长度不冲突，所有区间都位于该长度内。

匹配不能在片段间链式传递：整组必须存在一个共同验证器。任意两个强 ETag
明确冲突时，即使日期相同也拒绝现有集合。弱 ETag、无验证器或新片段不共享
整组验证器时返回 `XHTTP_CACHE_COMBINE_SEPARATE`，调用方可以建立独立候选或
按自己的替换策略处理。相同强验证器却声明不同完整长度返回
`XHTTP_CACHE_COMBINE_CONFLICT`，不能静默合并。

成功计划中的 `Index`、`RemoveCount` 和 `Range` 描述对规范区间数组的一次连续
替换：

```text
删除 [Index, Index + RemoveCount)
在 Index 插入 Range
```

`ResultCount` 是应用后的区间数。正文后端仍应在新片段自己的原始区间写入新
字节；`Plan.Range` 描述写入后形成的覆盖并集，不是要求复制一份合并正文。

完整新响应返回 `REPLACE`，无需依赖旧验证器。部分响应返回 `APPLY`。当最终
区间并集覆盖整个表示时，`Complete` 为 true，并要求写入完整 Content-Length；
否则条目继续标记为不完整。

## Header 来源

组合计划严格区分 Header 来源：

- 最新响应是截断 200 时，使用新 200 Header。
- 最新响应是 206 且存在匹配的已存 200 时，使用最近的已存 200 Header，
  再按 `xrtHttpCacheFieldUpdate` 更新新 206 提供的字段。
- 全部响应都是 206 时，同样使用最近的已存 Header，再用新 206 字段更新。
- `Content-Range` 始终作为已处理字段移除；206 的 Content-Length 不会冒充
  完整表示长度。

`HeaderIndex == XRT_NPOS` 表示使用新响应 Header，否则表示借用
`Stored[HeaderIndex]`。应用区间变更前必须先复制或采用所选 Header。

## 安全边界

- 所有 Header、范围和验证器均为借用视图。
- 参数错误和内存别名返回 ERROR，计划保持不变。
- 正常协议冲突和保守跳过会写出明确决定，不设置程序错误。
- 所有范围加法、长度比较和数组大小均检查溢出。
- 片段、覆盖、缺口和组合入口不分配内存。
- 多段 206 的 MIME 边界解析继续复用 `<xrt/multipart.h>`，本模块不实现第二套
  multipart 解析器。

完整示例位于 `examples/http/cache_range/main.c`。

规范依据：

- [RFC 9111 §3.3](https://www.rfc-editor.org/rfc/rfc9111.html#section-3.3)
- [RFC 9111 §3.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-3.4)
- [RFC 9110 §14.4](https://www.rfc-editor.org/rfc/rfc9110.html#section-14.4)
- [RFC 9110 §15.3.7.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.3.7.3)
