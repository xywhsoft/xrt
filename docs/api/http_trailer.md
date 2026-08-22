# HTTP Trailer

`http_trailer` 是独立于 HTTP/1 分帧器的 Trailer 协议层。它只依赖基础 HTTP
字段能力，供底层解析器、客户端、服务器、代理和应用直接复用。

## 字段声明

`xrtHttpTrailerCount` 完整验证全部重复 `Trailer` 字段行，并返回声明名称总数。
字段值使用 token-list；按 RFC 列表扩展规则，空值表示零个声明。每个名称还必须
通过通用 trailer 策略。输出在失败时保持为零，后置畸形字段不会造成部分发布。

`xrtHttpTrailerFind` 完整验证全部声明后返回三态结果：

- `XHTTP_NEXT_ITEM`：名称已经声明；
- `XHTTP_NEXT_END`：声明有效但不包含该名称；
- `XHTTP_NEXT_ERROR`：查询名称或任一 `Trailer` 字段无效。

需要逐项处理时，可直接使用基础层 `xrtHttpFieldTokenNext`，字段名传入
`Trailer`；再用 `xrtHttpTrailerNameValid` 应用发送策略。

## 实际尾字段

`xrtHttpTrailerSectionValid` 校验实际 trailer section。它同时验证每个字段名是否
允许在线路末尾发送，以及字段值是否满足通用 HTTP 字段值语法。客户端、服务器、
HTTP/1 chunked 写出器和声明构建器共用这一入口，不再分别维护名称循环。

Header section 与 trailer section 必须保持分离。`Trailer` 声明只表示可能出现的
名称，声明缺少实际字段不是协议错误，实际字段未被提前声明也不应由协议解析层直接
判为错误；需要更严格对应关系的应用可用 `xrtHttpTrailerFind` 实施自己的策略。

## 发送策略

`xrtHttpTrailerNameValid` 拒绝会改变消息分帧、路由、请求条件、响应控制、认证或
正文解释的字段。返回 `true` 仍不代表某个扩展字段的定义允许它出现在 trailer；
发送方必须同时遵守该字段自己的规范。

`xrtHttpTrailerNamesWrite` 从实际 `xhttpfield[]` 生成声明值。它按 ASCII
大小写不敏感规则去重，保留第一次出现的名称与线路顺序；空输出可查询精确长度，
写入路径不分配内存，也不接受与输入描述符或借用文本重叠的输出。实际字段名称和值
会先经过 `xrtHttpTrailerSectionValid`，因此声明构建成功后不会在 chunk-end 写出阶段
才发现无效字段值。

`xrtHttpTrailerNamesBuild` 是一次分配便利层，返回零结尾文本并通过 `pSize` 返回
不含终止零的长度。返回值由 `xrtFree` 释放。

```c
#include <xrt/http_trailer.h>
#include <xrt/memory.h>

static const xhttpfield Trailers[] = {
	{ XRT_STR_INIT("Content-Digest"), XRT_STR_INIT("sha-256=:...:") },
	{ XRT_STR_INIT("X-Result"), XRT_STR_INIT("complete") }
};

size_t iSize;
str sNames = xrtHttpTrailerNamesBuild(Trailers, 2u, &iSize);
```

HTTP/1 的 chunked 解析和末块写出仍由 `http1_body` 负责；客户端与服务器准备层
使用本模块生成 `Trailer` Header，不再各自实现拼接逻辑。

## 裁剪与边界

- 宏：`XRT_MODULE_HTTP_TRAILER`；
- 功能：`XRT_FEATURE_HTTP_TRAILER`；
- 直接依赖：`http`；
- section 校验、声明统计、查询和缓冲写入零分配；
- 测试覆盖重复字段、后置错误、禁止名称、大小写去重、未对齐描述符、重叠输出、
  短缓冲、OOM、单头发布和超过 1 KiB 的动态声明。

协议依据：[RFC 9110 Trailer Fields](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.5)、
[RFC 9110 Trailer](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.6.2)、
[RFC 9112 Chunked Trailer Section](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.2)
以及 [RFC 9530 Digest Fields](https://www.rfc-editor.org/rfc/rfc9530.html)。
