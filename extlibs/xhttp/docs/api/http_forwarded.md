# Forwarded

`http_forwarded` 实现 RFC 7239 请求字段 `Forwarded`。它用于表达代理处理过程中
丢失或改变的客户端节点、代理入口、原始 Host 和协议；它不建立代理信任策略，
也不会把字段内容自动当作可信客户端地址。

## 裁剪与依赖

- `XHTTP_FEATURE_HTTP_FORWARDED`：解析、重复字段组合、标准值校验和扩展参数遍历；
  依赖 XRT 的 `http_param_host`，Host、IPv4 与 IPv6 语义复用同一套核心语法。
- `XHTTP_FEATURE_HTTP_FORWARDED_WRITE`：规范写出单个元素或完整链路，以及分配型
  Build；只增加 writer 实现。

解析、字段迭代、节点/Host/proto 校验和直接写出均不分配堆内存。只需要读取
代理链路的程序不会携带 writer，协议层也不依赖网络 Engine、TCP 或 HTTP
Client/Server 运行时。

转义参数按语义字节流验证，任意长度 reg-name 与 IPvFuture 不分配临时字符串，
也不维护第二套地址语法。

## 解析链路

```c
xhttpforwardedcursor Cursor;
xhttpforwarded Forwarded;

xrtHttpForwardedCursorInit(&Cursor);
while ( xrtHttpForwardedNext(
	Value, &Cursor, &Forwarded
) == XHTTP_NEXT_ITEM ) {
	if ( (Forwarded.Flags & XHTTP_FORWARDED_HAS_FOR) != 0 ) {
		/* Forwarded.For 借用字段；按需用 xrtHttpParamValueWrite 解码。 */
	}
}
```

`xrtHttpForwardedNext` 处理一个字段值，
`xrtHttpForwardedFieldNext` 按线路顺序组合全部同名字段行。第一次迭代会预校验
完整链路，因此后续非法元素不会造成部分发布；失败时游标和输出保持不变。
游标同时绑定首次调用的字段值地址与长度，或字段数组地址与成员数；输入在游标
生命周期内必须保持不变，切换来源会原子失败并返回 `XERR_ARGUMENT`。

`xrtHttpForwardedCount` 和 `xrtHttpForwardedFieldCount` 在完整验证后返回单字段或
全部同名字段中的元素数。逗号产生的空列表成员按 HTTP 接收方规则忽略；RFC 7239
允许 `forwarded-element` 的分号项省略，因此 `;for=...;;proto=https;` 与仅含
分号的空元素都可读取，空项不会伪造参数，`PairCount` 可以为零。

`xhttpforwarded` 类型化保存四个标准参数，并保留原始 `Element` 和
`PairCount`。`xrtHttpForwardedPairNext` 可遍历所有标准或扩展参数，避免注册表扩展
迫使库升级。参数名称按 ASCII 大小写不敏感规则去重，重复参数会使整个元素失败。

参数结果使用 `xhttpparam`：token 值直接借用，quoted-string 值不含外层引号但
保留线路反斜线。`xrtHttpParamValueWrite` 可把两种形式统一解码到调用方缓冲。

## 标准值

- `for`、`by`：接受严格 IPv4、方括号 IPv6、`unknown` 或 `_` 开头的混淆节点，
  可带 1 到 5 位数字端口或混淆端口。IPv6 和带端口节点必须位于
  quoted-string。
- `host`：严格采用 Host ABNF，接受空 `uri-host`、空端口、任意长度十进制端口、
  reg-name、IPv4、IPv6 与 IPvFuture；协议层不把端口提前压缩为 `uint16`。
- `proto`：接受 RFC 3986 URI scheme。

`xrtHttpForwardedNodeValid`、`xrtHttpForwardedHostValid` 和
`xrtHttpForwardedProtoValid` 对已经解码的语义值执行同样校验，适合构建策略或
处理自定义来源。quoted-pair 通过流式语义读取校验，不会因为字段很长而分配临时
缓冲或施加隐藏长度上限。

## 写出

```c
xhttpforwardedvalue Element = { 0 };
char Value[128];
size_t Size;

Element.For = XRT_STR_LITERAL("[2001:db8::1]:443");
Element.Proto = XRT_STR_LITERAL("https");
Element.Flags = XHTTP_FORWARDED_HAS_FOR |
	XHTTP_FORWARDED_HAS_PROTO;

xrtHttpForwardedElementWrite(
	&Element, Value, sizeof(Value), &Size
);
```

writer 输入全部是已经解码的语义值。合法 token 直接写出；包含冒号、方括号、
逗号或其他不能进入 token 的值自动写成 quoted-string 并转义。标准参数按
`for;by;host;proto` 顺序写出，扩展参数随后保持调用方顺序。扩展名称不得重复，
也不得占用四个标准名称。writer 要求每个元素至少有一个参数，不主动生成接收方
语法允许的空元素。

`xrtHttpForwardedElementWrite` 用于常见单代理节点，`xrtHttpForwardedWrite` 写出
完整数组，`xrtHttpForwardedBuild` 返回由 `xrtFree` 释放的零结尾字符串。空输出
可查询精确长度；容量不足不写部分结果并返回所需长度。writer 先预检全部顶层与
嵌套描述符、借用视图、输出范围和长度槽，再验证语义和测量；无效或回绕的扩展
数组不会被解引用，输入、输出与长度槽之间的别名会以 `XERR_ARGUMENT` 原子拒绝。

解析器与 writer 都不设置参数数量上限。为保持零分配并验证任意扩展名唯一性，
单元素未知扩展的最坏去重复杂度为 O(n^2)；常见四个标准参数使用存在位做 O(1)
去重。HTTP 客户端和服务器仍应通过自己的字段总量限制控制不可信输入成本。

## 错误与所有权

- 返回的元素、参数和名称视图全部借用调用方输入。
- 非法指针、回绕范围、损坏游标或别名返回 `XERR_ARGUMENT`。
- RFC 语法、标准参数语义、重复名称或 writer 描述符口径错误返回 `XERR_VALUE`。
- 长度加法溢出返回 `XERR_RANGE`；短缓冲同样返回 `XERR_RANGE` 并发布所需长度。
- 除短缓冲的长度查询外，失败不会修改游标、输出对象、输出字节或长度槽。

## 信任边界

`Forwarded` 可以由客户端或任意中间节点修改。服务器只能在传输层对端属于明确
配置的可信代理时，按部署策略消费对应链路；协议解析成功不代表来源可信。字段
只用于请求，不应复制进响应。库保持该策略在 HTTP Server 中间件之外，避免一个
固定信任模型限制反向代理、服务网格或自定义网关。

## 示例与测试

- `examples/http/forwarded/main.c`
- `tests/http/test_http_forwarded.c`
- `tests/http/test_http_forwarded_noalloc.c`
- `tests/http/test_http_forwarded_write.c`
- `tests/http/test_http_forwarded_write_noalloc.c`
- `tests/http/test_http_forwarded_write_oom.c`
- `tests/http/test_http_forwarded_mutation.c`
- `tests/single/test_single_http_forwarded.c`
- `tests/single/test_single_http_forwarded_write.c`

实现遵循 [RFC 7239](https://www.rfc-editor.org/rfc/rfc7239.html)、
[RFC 7230 Host 语法](https://www.rfc-editor.org/rfc/rfc7230.html#section-5.4) 与
[RFC 3986](https://www.rfc-editor.org/rfc/rfc3986.html)。
