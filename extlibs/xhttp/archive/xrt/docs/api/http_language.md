# HTTP 语言协商

`XRT_FEATURE_HTTP_LANGUAGE` 提供 RFC 9110 `Accept-Language` 字段和 RFC 4647
Basic Filtering、Lookup。模块只依赖 HTTP 基础层，不依赖 MIME、网络、客户端或
服务器运行时；解析、匹配和选择均不分配堆内存。

## 语法与迭代

`xrtHttpLanguageRangeValid` 验证 basic language range：首段为 1 到 8 个 ASCII
字母，后续段为 1 到 8 个 ASCII 字母或数字，或者整个范围为 `*`。
`xrtHttpLanguageTagValid` 只验证用于协议匹配的基本连字符分段形状，不是完整的
BCP 47 语义验证器；它不查询 IANA Registry，也不擅自规范化已废弃或
grandfathered 标签。

```c
XRT_API xhttpnext xrtHttpLanguageRangeNext(
	xstrview List, size_t* pOffset, xhttplanguagerange* pRange);
XRT_API xhttpnext xrtHttpAcceptLanguageNext(
	const xhttpfield* pFields, size_t iCount,
	xhttplanguagecursor* pCursor, xhttplanguagerange* pRange);
```

单字段迭代器复用 HTTP qvalue 语法，缺省质量为 1000，忽略列表空成员，拒绝
extended range、额外参数和非法质量值。重复字段迭代器保持线路顺序。错误不推进
游标并清空结果；固定输出支持未对齐存储。

## Basic Filtering

`xrtHttpLanguageBasicMatch` 按 ASCII 大小写不敏感规则比较：范围与标签完全相等，
或者范围是以连字符为边界的标签前缀时匹配。`*` 匹配任意合法标签。

`xrtHttpAcceptLanguageMatch` 和 `xrtHttpAcceptLanguageQuality` 计算一个标签的有效
质量。多个范围命中时，非通配范围优先于 `*`，子标签更多的范围优先，具体度仍
相同时保留最早成员。因此 `en-US` 可以覆盖 `en`，而 `*` 只处理没有更具体范围的
语言。

没有 `Accept-Language` 字段表示任意语言均可接受，质量为 1000；字段存在但为空
表示没有语言可接受。`xrtHttpAcceptLanguageSelect` 使用 Basic Filtering 比较全部
服务端标签，先选最高客户端质量，同质量保留服务端数组中更靠前的项。

## Lookup

`xrtHttpAcceptLanguageLookup` 用于必须返回单个语言表示的场景。函数按质量值降序、
同质量线路顺序处理语言范围，并从尾部逐段截断，直到与一个可用标签完全相等。
删除 extension 或 private-use 尾项时，会同时删除相邻 singleton。例如：

```text
zh-Hant-CN-x-private1-private2
zh-Hant-CN-x-private1
zh-Hant-CN
zh-Hant
zh
```

Lookup 不把 `*` 擅自解释为某个默认语言；只有通配符或没有匹配时返回
`XHTTP_NEXT_END`，由调用方选择项目默认值。字段完全缺失时没有客户端偏好，函数
直接选择服务端数组首项。该区别避免协议层替应用做不可逆的默认语言决定。

```c
static const xstrview Available[] = {
	XRT_STR_INIT("en"),
	XRT_STR_INIT("zh-Hant"),
	XRT_STR_INIT("zh")
};
size_t iIndex;

if ( xrtHttpAcceptLanguageLookup(
	Fields, iFieldCount, Available, 3, &iIndex
) == XHTTP_NEXT_ITEM ) {
	/* Available[iIndex] 是查找结果。 */
}
```

需要 Registry 规范化、`nn`/`nb` 到 `no` 等业务映射、extended filtering 或自定义
默认值时，使用 `xrtHttpAcceptLanguageNext` 取得范围后在应用层扩展，不需要复制
字段和值解析器。

## 裁剪与测试

`XRT_MODULE_HTTP_LANGUAGE` 只拉入 HTTP 基础层。测试覆盖语法、重复字段、质量值、
通配符、前缀边界、缺失与空字段、稳定选择、Lookup singleton 回退、零分配和单头
发布。
