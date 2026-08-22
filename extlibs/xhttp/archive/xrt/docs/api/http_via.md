# HTTP Via

`xrt/http_via.h` 提供独立、零分配的 Via 协议层。协议语义以
[RFC 9110 Via](https://www.rfc-editor.org/rfc/rfc9110#name-via) 为准：

- `XRT_FEATURE_HTTP_VIA`：元素解析、完整列表验证、重复字段迭代和 comment 解码；
- `XRT_FEATURE_HTTP_VIA_WRITE`：规范写出和单次分配 Build。

本层只依赖 HTTP 字段基础，不依赖 Connection、转发策略、客户端、服务器或网络。

## 解析

`xrtHttpViaElementParse` 实现 RFC 9110 的单元素语法：

~~~text
received-protocol RWS received-by [ RWS comment ]
~~~

HTTP 协议名可以省略，例如 `1.1 edge`；显式形式可以写为
`HTTP/1.1 edge:8080`。`received-by` 使用 token pseudonym 和 URI `port`。
按照 [RFC 3986 port](https://www.rfc-editor.org/rfc/rfc3986#section-3.2.3)，
URI 端口语法允许零个数字，因此 `edge:` 是有效的显式空端口；解析结果通过
`XHTTP_VIA_HAS_PORT` 区分它与省略端口的 `edge`。

完整 Via 值使用 HTTP `#list` 语法，因此空值、仅 OWS 和仅空列表成员都是有效的
零元素列表。`xrtHttpViaElementParse` 仍只接受一个非空元素。解析器支持嵌套
comment、quoted-pair 和 comment 内逗号，拒绝裸控制字符、残缺转义和尾随垃圾。

`xhttpvia` 的所有视图借用输入。`Comment` 保留包含最外层括号的线路形式；
`xrtHttpViaCommentDecode` 去掉最外层括号和 quoted-pair 反斜线，同时保留嵌套
括号正文。解析、迭代和直接解码均不分配。

~~~c
xhttpviacursor Cursor;
xhttpvia Via;

xrtHttpViaCursorInit(&Cursor);
while ( xrtHttpViaNext(Value, &Cursor, &Via) == XHTTP_NEXT_ITEM ) {
	/* Via.ProtocolVersion 和 Via.ReceivedBy 借用 Value。 */
}
~~~

单字段游标在第一次调用时绑定值的地址和长度；`xrtHttpViaFieldNext` 的重复字段
游标绑定字段数组地址和数量。迭代结束前切换输入会失败，不推进游标，并清空元素
输出。重复字段游标在发布第一项前验证全部 Via 字段。

## 写出

`xhttpviavalue` 使用存在位控制可选协议名、端口和注释。Comment 是已经解码的
普通文本；writer 自动转义括号和反斜线。`XHTTP_VIA_HAS_PORT` 可以明确写出空端口
`:`，`XHTTP_VIA_HAS_COMMENT` 可以明确写出空注释 `()`；未设置存在位时，对应
视图必须为空。

`xrtHttpViaElementWrite` 和 `xrtHttpViaWrite` 支持空输出长度查询，不分配且容量
失败不写入半个值。`xrtHttpViaBuild` 只为最终零结尾结果分配一次内存，返回值由
`xrtFree` 释放。

规范化生产端通常应省略空端口的冒号；存在位保留该线路形式，是为了协议转换和
需要精确保真时不丢失信息。

## 内存契约

- 字段数组、Via 描述符和游标允许未对齐存储；
- 公开结果描述符不得覆盖借用输入；
- writer 输出和长度指针不得覆盖描述符或任一输入视图；
- 解析器与直接 writer 零分配；
- Build 失败时不修改调用方长度输出。

## 验证

- RFC 语法：重复字段、空列表、嵌套 comment、quoted-pair 和空端口；
- 游标边界：等长换源、改变长度、字段数组换源和失败原子性；
- 内存边界：未对齐描述符、输入输出别名和短缓冲失败原子性；
- 无分配、OOM、随机 comment 写出解析闭环、单头和独立裁剪测试。

完整示例位于 `examples/http/via/main.c`。
