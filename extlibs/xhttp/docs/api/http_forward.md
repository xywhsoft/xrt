# HTTP 消息转发

`xrt/http_forward.h` 提供 RFC 9110 消息转发策略：

- `XHTTP_FEATURE_HTTP_FORWARD`：固定逐跳字段、`Connection` 提名查询和
  `Max-Forwards`；
- 本层依赖 `XRT_FEATURE_HTTP_CONNECTION`，不依赖 Via、客户端、服务器、代理
  路由或网络连接。

Via 协议位于独立的 `xrt/http_via.h`，因此只需要代理链路解析的裁剪配置不会引入
Connection 转发策略。

## Connection 与逐跳字段

`Connection` 游标和单项查询位于 `xrt/http_connection.h`。客户端、服务端、缓存
和代理共享同一基础解析器；完整契约见 `docs/api/http_connection.md`。

`xrtHttpHopFieldKnown` 识别 RFC 9110 要求移除或替换的固定集合：
`Connection`、`Keep-Alive`、`Proxy-Connection`、`TE`、
`Transfer-Encoding` 和 `Upgrade`。`xrtHttpHopField` 还会识别任意被
`Connection` 动态提名的扩展字段。

~~~c
xhttpnext Hop = xrtHttpHopField(
	Fields, FieldCount, Fields[i].Name);

if ( Hop == XHTTP_NEXT_ITEM ) {
	/* 不把该字段转发到下一跳。 */
}
~~~

`Trailer` 并不是本 API 的固定逐跳字段。缓存或协议转换器仍可按自身的分帧与
存储策略单独移除它。

## Max-Forwards

`xrtHttpMaxForwardsParse` 接受完整 `uint64` 十进制范围并拒绝空值、空白、符号和
溢出。`xrtHttpMaxForwardsUpdate` 同时完成解析和 RFC 更新：

- 收到零时返回 `XHTTP_FORWARD_FINAL`，当前节点必须作为最终接收者处理；
- 非零时返回 `XHTTP_FORWARD_NEXT`，输出
  `min(received - 1, maximum)`；
- 非法输入返回 `XHTTP_FORWARD_ERROR`，并把输出保持为零。

`xrtHttpMaxForwardsWrite` 是零分配、失败原子的十进制写出函数。

## 内存契约

- 字段数组和输出描述符允许未对齐存储；
- 输出不得覆盖字段数组或任一借用输入；
- 解析、查询和直接写出均不分配；
- 后置畸形 `Connection` 不会被先前命中掩盖。

完整示例位于 `examples/http/forward/main.c`。
