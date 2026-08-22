# HTTP Expect

`<xrt/http_expect.h>` 实现 RFC 9110 `Expect` 字段的传输无关协议层。它不绑定
HTTP 客户端、服务器或网络对象，直接路径不分配内存。

## 元素

`xhttpexpectation` 借用完整 `Element`、大小写不敏感的 `Name`、可选线路 `Value`
以及从首个分号开始的原始 `Parameters`。`Value` 保留 token 或包含双引号的
quoted-string，`XHTTP_EXPECT_VALUE_QUOTED` 明确区分两者；调用方可以原样转发，或在
启用 HTTP 参数层时用 quoted-string API 解码。

解析接受 `expectation = token [ "=" ( token / quoted-string ) parameters ]`。
等号两侧不允许空白；参数允许 RFC 定义的空分号成员，quoted-string 内的逗号、分号和
quoted-pair 不会被误当作外层分隔符。

## 列表与重复字段

`xrtHttpExpectNext` 迭代一个字段值，`xrtHttpExpectFieldNext` 按线路顺序迭代全部重复
`Expect` 字段。两种游标都必须由初始化函数建立，并在发布第一项前完成输入全集校验；
畸形后缀不会留下已消费的半份结果。输入在迭代期间必须保持不变。

`xrtHttpExpectValid` 和 `xrtHttpExpectCount` 验证单字段列表。HTTP `#list` 的空成员被
忽略，因此空值及首尾、连续逗号可形成零项或较短列表。描述符、游标和输出均支持未对齐
存储，输出不能覆盖输入或游标。

## 四态分类

`xrtHttpExpectFields` 完整验证全部重复字段并返回：

- `XHTTP_EXPECT_NONE`：没有非空 expectation；
- `XHTTP_EXPECT_CONTINUE`：全部元素都是无值、无参数的 `100-continue`；
- `XHTTP_EXPECT_UNSUPPORTED`：语法有效，但至少含一个扩展 expectation；
- `XHTTP_EXPECT_ERROR`：字段或语法错误。

服务器可把 `UNSUPPORTED` 映射为 417，同时仍允许应用在更低层迭代并实现扩展；客户端可
明确拒绝自己不会执行的 expectation。HTTP/1.0 服务器按 RFC 忽略标准
`100-continue`；HTTP/1.1 服务器只有在正文计划为非零定长或 chunked 时才发布
继续握手事实，零正文不会触发 100 响应。

## 范例

参见 `examples/http/expect/main.c`。
