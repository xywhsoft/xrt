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
- `http_cors_safelist.h` 提供 Fetch 客户端侧的预检与响应可见性判定。
- `http_cors_client.h` 提供预检规划以及实际、预检响应校验。
- `http_cors_cache.h` 提供可选、有界且线程安全的 Fetch 预检缓存。

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

## Fetch Safelist

`http_cors_safelist` 依赖 CORS 与 MIME 协议层；Content-Type 复用 MIME 模块的 token
规则，但按 MIME Sniff 算法只提取 Fetch 所需的 essence。参数尾不会决定解析成败，
因此 `text/plain;` 和含有不可识别参数的 `text/plain` 仍按其 essence 分类；通用
`xrtHttpMediaTypeParse` 仍保持严格线缆语法，不与此宽容判定混用。实现按当前
[Fetch Standard](https://fetch.spec.whatwg.org/) 覆盖：

- GET、HEAD、POST 三种安全方法；
- Accept、Accept-Language、Content-Language、Content-Type、Range 五类请求字段；
- 单字段值 128 字节和安全字段总值 1024 字节上限；
- 三种安全 Content-Type essence 与带首位置的单字节 Range；
- header-list 按项累计 value；重复 Content-Type 或 Range 直接判为非安全；
- Authorization 非通配规则；
- 七个默认可见响应字段、Expose-Headers 和凭据模式星号语义；
- Set-Cookie 与 Set-Cookie2 永不暴露。

`xrtHttpCorsPreflightPlan` 接收即将发送的 header-list，返回触发预检的方法、字段和
强制原因。`xrtHttpCorsClientCheck` 只检查实际响应所需的 Allow-Origin 与凭据字段，
不会因为无关 CORS 字段格式错误而误拒绝响应。`xrtHttpCorsPreflightCheck` 另外检查
2xx 状态、允许方法、唯一非安全字段名和 Max-Age；策略拒绝通过 `Reject` 返回，输入
或协议语法错误返回 `false`。

`xrtHttpCorsPreflightHeaderNamesWrite` 直接生成 Fetch 要求的排序、去重、小写字段名
集合，逗号后不添加空格。`xrtHttpCorsPreflightFieldsWrite` 写出 CORS 专用字段行，不添加
`Accept: */*` 或最终空行，便于 HTTP 客户端与调用方自己的字段块直接组合。

## 预检缓存

`xrtHttpCorsCachePlan` 把 safelist 规划和缓存查询合并为一次请求侧操作。原始
`METHOD`、`HEADERS`、`FORCED` 和 `REQUIRED` 原因始终保留；只有方法权限和全部
非安全字段权限都命中时才增加 `XHTTP_CORS_PREFLIGHT_CACHED`。因此发送条件固定为：

```c
bool sendPreflight =
	(Plan.Flags & XHTTP_CORS_PREFLIGHT_REQUIRED) != 0 &&
	(Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0;
```

`xrtHttpCorsCacheUpdate` 只接收已经由 `xrtHttpCorsPreflightCheck` 校验成功的
`xhttpcorsclientresult`，并在硬容量范围内缓存响应列出的方法和字段权限。单个响应
不会先为超过 `MaxEntries` 的权限分配节点再执行 LRU 淘汰；超出部分可以在后续请求
中重新预检。强制预检且响应没有
Allow-Methods 时，当前请求方法会按 Fetch 规则进入缓存。Max-Age 先采用 Fetch 的
5 秒默认语义，再由缓存配置的实现上限钳制；零秒通过无分配路径立即撤销响应
列出的全部匹配权限，即使内存分配已经失败也不会保留旧授权。

缓存键由调用方提供的网络分区、请求 Origin 和规范目标 URL 组成。URL 按字节精确
比较，调用方应在建键前完成规范化并删除片段。凭据权限遵循 Fetch 的单向复用规则：
凭据项可以覆盖无凭据请求，无凭据项不能覆盖凭据请求；字段星号永不覆盖
Authorization。

默认缓存最多保存 512 个权限项，Max-Age 上限为 86400 秒，并使用包含网络分区、
规范 URL 与 Origin 三元组的哈希桶、单调时钟和 LRU 提前淘汰。所有操作线程安全；
`Remove` 用于预检网络失败后的同键清理，
`Purge` 主动回收过期项，`Clear` 保留桶容量，`Stats` 提供命中、未命中、刷新、淘汰
与删除计数。缓存是优化层，协议规划和校验不依赖它。

## 裁剪

启用 `XRT_MODULE_HTTP_CORS` 会自动带入 HTTP Origin、Host、URL 和 HTTP 基础层。
`XRT_MODULE_HTTP_CORS_POLICY` 增加数组策略，`XRT_MODULE_HTTP_CORS_WRITE` 增加直接
字段写出，`XRT_MODULE_HTTP_CORS_SAFELIST` 增加 Fetch safelist，
`XRT_MODULE_HTTP_CORS_CLIENT` 再增加客户端预检和响应校验。只使用基础 HTTP 或
Origin 时不需要启用 CORS。`XRT_MODULE_HTTP_CORS_CLIENT_WRITE` 单独增加客户端预检
字段写出。`XRT_MODULE_HTTP_CORS_CACHE` 单独增加 Map、Mutex、Time 与有界预检缓存，
不会带入 HTTP 网络客户端或服务器。

仍处于孵化或被后续方案替换的私网/本地网络访问扩展不固化进稳定 CORS 核心。
调用方仍可通过基础 `xhttpfield` 和直接字段 writer 处理扩展头；扩展成熟后应进入
独立裁剪模块，而不是改变本层已经冻结的 Fetch CORS 数据结构。
