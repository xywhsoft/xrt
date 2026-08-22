# HTTP CORS

`http_cors.h` 按当前
[Fetch CORS protocol](https://fetch.spec.whatwg.org/#cors-protocol) 提供传输无关、
零分配的 CORS 字段读取能力。它依赖
`http_origin`，但不依赖 HTTP 客户端、服务器、路由器或响应对象。

## 分层

- `xrtHttpCorsMethodParse`、`xrtHttpCorsAllowOriginParse`、
  `xrtHttpCorsAllowCredentialsParse`、`xrtHttpCorsMaxAgeParse` 读取单个字段值。
- `xrtHttpCors*Fields` 读取必须唯一的字段，并区分缺失、条目和错误。
- `xrtHttpCors*Next` 跨重复同名字段迭代方法或字段名列表。
- `xrtHttpCorsRequestRead` 与 `xrtHttpCorsResponseRead` 一次性验证字段组合并建立借用视图。
- `xrtHttpCorsPolicyCheck` 用静态数组策略处理常见 Origin、方法和字段授权。
- `xrtHttpCorsDecisionWrite` 直接写出响应字段行，不依赖响应对象。

协议层不执行应用授权，也不强迫调用方构建响应对象。应用可以直接写固定响应，
也可以在更高层策略模块中依据这里的请求事实生成响应字段。

## 请求

`xrtHttpCorsRequestRead` 接收实际 HTTP 方法和字段数组。普通 CORS 请求只需要
`Origin`。预检请求必须使用区分大小写的 `OPTIONS`，并携带唯一的
`Access-Control-Request-Method`。`Access-Control-Request-Headers` 出现时必须
至少包含一个合法字段名，并且只能用于完整预检。

请求头列表可以分布在多个同名字段中。游标在发布第一个条目前先验证所有重复
字段，因此后续坏字段不会让应用先消费半个可信列表。按照 Fetch 的
`1#field-name` 语法，`Access-Control-Request-Headers` 字段一旦出现，全部重复字段
合并后必须至少包含一个名称；高层读取和低层游标执行相同约束。

## 响应

`xrtHttpCorsResponseRead` 覆盖以下字段：

- `Access-Control-Allow-Origin`
- `Access-Control-Allow-Credentials`
- `Access-Control-Allow-Methods`
- `Access-Control-Allow-Headers`
- `Access-Control-Expose-Headers`
- `Access-Control-Max-Age`

Allow-Origin 只接受星号、`null` 或单一 serialized-origin。Credentials 只接受
区分大小写的 `true`。Max-Age 使用完整 `uint64` 范围并拒绝溢出。列表字段保留
“字段存在但列表为空”的事实，便于更高层按 Fetch 凭据模式解释星号。

## 所有权

所有文本和 Origin 都借用输入字段。字段数组与借用文本在视图和游标使用结束前
必须保持有效且不可修改。模块不分配内存，描述符数组、游标和结构输出支持未对齐
存储。

## 策略

`xhttpcorspolicy` 借用预解析的 Origin 数组，以及方法、允许请求头和暴露响应头
token 数组。零值策略拒绝所有跨源请求，但不影响没有 Origin 的普通请求。
`xrtHttpCorsPolicyCheck` 返回 `false` 只表示参数、协议或策略描述错误；策略拒绝
返回 `true`，并通过 `XHTTP_CORS_REJECT_ORIGIN`、`METHOD` 或 `HEADER` 表达原因。

`ANY_ORIGIN` 在不允许凭据时生成 `*`。与 `CREDENTIALS` 同时使用时会回显请求
Origin 并生成 `Vary: Origin`，避免产生浏览器不会接受的凭据通配响应。显式 Origin
同样回显并生成 Vary。预检只回显已经批准的请求方法和请求头，并额外把
`Access-Control-Request-Method` 与 `Access-Control-Request-Headers` 写入 `Vary`，
避免共享 HTTP 缓存复用另一组预检条件的响应。Max-Age 只写入预检响应；
Expose-Headers 只写入普通 CORS 响应。

方法和允许请求字段数组中的 `*` 会被拒绝；调用方必须使用
`XHTTP_CORS_POLICY_ANY_METHOD` 或 `XHTTP_CORS_POLICY_ANY_HEADER` 明确表达通配策略。
Expose-Headers 中的 `*` 保留 Fetch 定义的凭据相关语义。

## 直接写出

`xrtHttpCorsDecisionWrite` 输出以 CRLF 结束的字段行，但不输出字段块最后的空行。
调用方可以把结果直接拼入预封装 HTTP 响应，也可以交给任意服务器 Header API。
`NULL/0` 查询精确长度，短缓冲只返回所需长度，不写部分结果。拒绝决策和非 CORS
决策成功写出零字节。

## 裁剪

启用 `XRT_MODULE_HTTP_CORS` 会自动带入 HTTP Origin、Host、URL 和 HTTP 基础层。
`XRT_MODULE_HTTP_CORS_POLICY` 增加数组策略，`XRT_MODULE_HTTP_CORS_WRITE` 增加直接
字段写出。只使用基础 HTTP 或 Origin 时不需要启用 CORS。

浏览器式 Fetch safelist、客户端预检编排和预检缓存不属于通用 HTTP 客户端，已移出
XRT 主线。独立扩展仍可复用 `xhttpfield`、Origin 与本模块的 CORS 字段解析能力，
无需复制线缆解析器。
