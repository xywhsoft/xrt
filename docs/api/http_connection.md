# HTTP Connection

`xrt/http_connection.h` 提供 HTTP `Connection` 字段的解析和 HTTP/1 持久性判断。
协议层不依赖客户端、服务器、代理或网络对象，可直接处理借用的 `xhttpfield` 数组。

## 裁剪

- 模块：`http_connection`
- 功能宏：`XRT_FEATURE_HTTP_CONNECTION`
- 直接依赖：`http`

## 选项迭代

`Connection` 使用 RFC 9110 的 `#connection-option` 语法。空字段、仅含 OWS 的字段和仅含空
列表成员的字段都表示零个选项；非空成员必须是 HTTP token。

`xrtHttpConnectionCursorInit` 初始化通用的 `xhttpfieldtokencursor`，
`xrtHttpConnectionNext` 按线路顺序遍历全部重复字段。第一次发布选项前会验证所有
`Connection` 字段。游标会绑定原字段数组、字段数量和字段名称，不能在迭代中切换输入；
输入存储在游标结束前必须保持有效且不变。

返回的 `xstrview` 借用原字段值。迭代不分配内存，返回值为：

- `XHTTP_NEXT_ITEM`：发布一个选项；
- `XHTTP_NEXT_END`：正常结束，同时清空输出视图；
- `XHTTP_NEXT_ERROR`：参数、游标或字段语法错误。

`xrtHttpConnectionCount` 完整验证并统计选项。
`xrtHttpConnectionFind` 完整验证后执行大小写不敏感的单项查询。
底层通用能力分别是 `xrtHttpFieldTokenCount`、`xrtHttpFieldTokenFind` 和
`xrtHttpFieldTokenNext`。

```c
static const xhttpfield fields[] = {
	{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive") },
	{ XRT_STR_INIT("Connection"), XRT_STR_INIT("TE") }
};
size_t count;

if ( !xrtHttpConnectionCount(fields, 2u, &count) ) {
	return false;
}
if ( xrtHttpConnectionFind(
	fields, 2u, XRT_STR_LITERAL("te")
) == XHTTP_NEXT_ITEM ) {
	/* 对端声明了 TE 逐跳选项。 */
}
```

## 持久连接

`xrtHttpConnectionPersistence` 实现 RFC 9112 的 HTTP/1 持久性判断：

- `close` 始终返回 `XHTTP_CONNECTION_CLOSE`；
- 没有 `close` 的 HTTP/1.1 默认返回 `XHTTP_CONNECTION_PERSIST`；
- HTTP/1.0 默认关闭；
- HTTP/1.0 只有设置 `XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE`、消息包含
  `keep-alive`，且接收方不是代理或消息是响应时才持久；
- 非法版本、未知标志或非法字段返回 `XHTTP_CONNECTION_ERROR`。

消息为响应时设置 `XHTTP_CONNECTION_RESPONSE`，接收方为代理时设置
`XHTTP_CONNECTION_PROXY`。允许 HTTP/1.0 Keep-Alive 是显式本地策略，库不会默认开启。

```c
xhttpconnectionstatus status = xrtHttpConnectionPersistence(
	XHTTP_VERSION_1_1, fields, 2u, 0);

if ( status == XHTTP_CONNECTION_ERROR ) {
	return false;
}
if ( status == XHTTP_CONNECTION_CLOSE ) {
	/* 完成当前响应后优雅关闭。 */
}
```

连接持久还要求消息具有自描述长度并被完整消费；该函数只判断版本和 `Connection` 字段，
不会替代 HTTP/1 消息分帧或传输状态检查。

## 分层复用

- 规范写出选项数组使用 `xrtHttpTokenListWrite` 或 `xrtHttpTokenListBuild`；
- 逐跳字段移除策略使用 `xrtHttpHopField`；
- `TE`、`Upgrade` 等具体协议模块负责校验对应选项与字段是否配套；
- 传输层负责半关闭、排空和最终关闭，不由本协议层操作套接字。

## 内存与错误

- 字段数组、字段值、游标和输出描述符支持未对齐存储；
- 游标和输出不得覆盖字段描述符或任一借用视图；
- Count 失败时输出为零，Next 失败时不推进游标并清空输出视图；
- 所有解析、统计、查找和持久性判断都不分配内存；
- 详细错误由线程错误槽提供。

协议依据：[RFC 9110 Section 7.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.6.1)
和 [RFC 9112 Section 9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3)。

## 验证资产

- `tests/http/test_http_connection.c`
- `tests/http/test_http_connection_noalloc.c`
- `tests/single/test_single_http_connection.c`
- `examples/http/connection/main.c`
