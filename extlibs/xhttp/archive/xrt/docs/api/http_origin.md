# HTTP Origin

`http_origin.h` 公开 [RFC 6454](https://www.rfc-editor.org/rfc/rfc6454)
Origin 字段和同源三元组能力，供 HTTP、CORS、WebSocket、代理和应用访问策略共同
复用。模块不依赖网络对象，也不替应用决定哪些 Origin 可以访问资源。

## 裁剪与依赖

- `XRT_FEATURE_HTTP_ORIGIN`：`null`、serialized-origin、历史 Origin 列表、唯一字段读取、URL 提取与同源比较。
- `XRT_FEATURE_HTTP_ORIGIN_WRITE`：单值、列表和拥有型规范写出；依赖 `HTTP_ORIGIN`。
- `HTTP_ORIGIN` 依赖 `HTTP_HOST`，因此复用 HTTP 字段、URL authority 和 Host 规则，不复制 URI 解析器。

只需要解析 WebSocket 或 CORS 请求时不必引入写出模块。

## 数据模型

`xhttporigin` 保存一个借用 Origin：

- `Text` 是原始线路元素；由 `xrtHttpOriginFromUrl` 或 `xrtHttpOriginNull` 建立的值可以为空。
- `Url` 保存 scheme、host、显式端口和 IP-literal 事实；路径、查询和片段不属于 Origin。
- `XHTTP_ORIGIN_NULL` 表示没有 scheme/host/port 三元组的 opaque Origin。

公开结构可以放在栈上和未对齐存储中，但合法非 null 描述符的 `Url` 只能保留
scheme、authority、host 和 port；混入 path、query 或 fragment 会被比较和 writer
作为非法参数拒绝。需要从完整 URL 建立 Origin 时应调用 `xrtHttpOriginFromUrl`。

`null` 不是一个所有调用方共享的真实来源身份。`xrtHttpOriginSame` 因此规定 null 与任何值都不同源，包括另一个 null；应用若有意允许线路值 `null`，必须作为单独策略明确处理。

## 解析

`xrtHttpOriginParse` 接受一个 `null` 或一个 serialized-origin。非 null 值必须具有
scheme、双斜线 authority 和非空 host，不允许 userinfo、path、query 或 fragment。
按照 [RFC 3986 port](https://www.rfc-editor.org/rfc/rfc3986#section-3.2.3)，端口
语法允许显式空值，因此 `https://example.test:` 会被接受，并按省略
端口参与同源比较；规范 writer 会去除空端口冒号。两端 OWS 可以存在，输出借用
去除 OWS 后的输入。

`xrtHttpOriginValid` 与 `xrtHttpOriginNext` 同时覆盖 RFC 6454 的历史列表形式：元素
之间只能使用一个 SP；`null` 必须单独出现。游标第一次返回条目前会验证完整值并
绑定输入地址和长度，因此合法前缀不能掩盖畸形后缀，迭代中也不能切换输入。

现代 Fetch/CORS 和 WebSocket 的常用路径通常只接受一个 Origin。`xrtHttpOriginFields` 读取唯一 `Origin` 字段并要求它只含单个值，以 `END`、`ITEM` 和 `ERROR` 区分缺失、成功和重复字段/列表/语法错误。

## 同源比较

`xrtHttpOriginSame` 比较 scheme、host 和有效端口：

- scheme 与 host 使用 ASCII 大小写不敏感比较；
- HTTP、HTTPS、WS 和 WSS 的省略端口与显式默认端口相等；
- 未知 scheme 的两个省略端口相等，两个显式数值端口按数值比较；未知默认端口不能与显式端口猜测为相等；
- path、query 和 fragment 不参与比较；
- null 始终返回不同源且不设置错误。

Host 比较不会执行 DNS，也不会把不同 IPv6 词法形式解析成同一地址。规范生成的 Origin 会得到稳定线路值；需要把任意非规范地址文本归并到安全策略键时，应用应先使用对应地址或 IDNA 规范化层。

## URL 提取

`xrtHttpOriginFromUrl` 接受完整绝对分层 URL，复用其 scheme、host 和 port，并从结果中去除 path、query 与 fragment。它不会分配，也不会改写输入 URL。

## 写出

`xrtHttpOriginWrite` 写一个值，`xrtHttpOriginListWrite` 写单空格分隔数组，`xrtHttpOriginBuild` 只为最终零结尾结果执行一次分配。

写出路径会：

- 把 scheme、DNS host 和 IP-literal 中的 ASCII 字母写为小写；
- 为 IP-literal 恢复方括号；
- 以最短十进制写端口，并省略 HTTP、HTTPS、WS 和 WSS 的默认端口；
- 拒绝把 null 与其他值组合；
- 拒绝相邻同源项，保证生成侧满足 RFC 6454 的去重要求。

直接 Writer 支持 `NULL/0` 精确计长，不分配内存。非法描述符、输入输出别名和大小输出别名在写入前失败；容量不足时返回所需大小且不修改输出。Build 分配失败时不修改调用方的长度值。

## 示例

```c
xhttporigin Request;
xhttporigin Allowed;

if ( xrtHttpOriginParse(
	XRT_STR_LITERAL("HTTPS://App.Example:443"), &Request
) && xrtHttpOriginParse(
	XRT_STR_LITERAL("https://app.example"), &Allowed
) && xrtHttpOriginSame(&Request, &Allowed) ) {
	/* 允许该来源。 */
}
```

完整示例见 `examples/http/origin/main.c`。
