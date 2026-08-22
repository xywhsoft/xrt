# XURL 入门：什么时候先把 URL 拆对，比直接发请求更重要

> 目标：把 `xrtUrlParseView()`、`xrtUrlParseFixedTo()`、`xrtUrlMakeHostHeader()`、`xrtUrlNormalizePathTo()`、`xrtUrlResolveTo()` 讲成第 6 阶段网络主线里“地址解析与构建”这条正式起点。读完这页后，你应该明确知道：URL、authority、target 不是一回事，为什么客户端连不上、请求发错主机、`Host` 头不对，很多时候不是 `xhttp` 的错，而是 URL 在进入网络层之前就已经拆坏了。

[返回教学文档](README.md)

---

## 1. 为什么 `xurl` 要先学，而不是直接跳到 `xhttp`

很多网络 bug 表面看起来像：

- 连不上
- TLS 握手失败
- 代理转发失败
- 服务器返回 400

但真正的根因常常更早：

- `scheme` 没认对
- `host` 和 `target` 混了
- 默认端口没处理好
- `Host` 头和连接目标不一致
- 相对路径在 base URL 上解析错了

也就是说，在你真正创建连接之前，往往已经先有一层更基础的问题：

- 这串 URL 到底代表什么

这就是 `xurl` 的位置。


## 2. 先把 8 个边界分清楚

### 2.1 完整 URL、authority、target 不是一回事

推荐你先把这三层硬拆开：

| 名称 | 典型内容 |
|------|----------|
| 完整 URL | `https://user@host:8443/path?q=1#frag` |
| authority | `user@host:8443` |
| target | `/path?q=1#frag` |

所以当前 API 才会分成：

- `xrtUrlParseAuthority*()`
- `xrtUrlParseTarget*()`
- `xrtUrlParseView*()`

如果你只需要 HTTP 请求行里的 target，就不要先把它当完整 URL 解析。  
如果你在做代理、Host 头或连接目标，authority 才是重点。


### 2.2 `xrturlview` 默认借用原始输入，不替你复制内存

当前 `xurl` 的核心返回类型是：

- `xrturlview`

它内部是多段：

- `xrtstrview`

也就是说，这组 API 更像：

- “解析视图”

不是：

- “帮你造一份新的字符串对象”

所以如果你后面要长期保存字段，应该自己复制，而不是把 view 当成独立堆对象。


### 2.3 `scheme` 不只是前缀，它决定默认端口和是否进入 TLS

当前主线里和 `scheme` 相关的几个关键判断是：

- `xrtUrlDefaultPort()`
- `xrtUrlIsSecureScheme()`
- `xrtUrlIsDefaultPort()`
- `xrtUrlViewIsScheme()`
- `xrtUrlViewMatchesScheme2()`

它们直接影响：

- 默认端口是否补成 `80 / 443`
- 当前连接是不是安全 scheme
- 构造 `Host` 头时该不该省略端口

所以 `scheme` 不是“给日志看的一截字符串”，它会真正影响连接路径。


### 2.4 `Host` 头不应该手搓，尤其是默认端口和 IPv6

这类代码很常见，也很容易写错：

```c
sprintf(sHostHeader, "%s:%u", sHost, iPort);
```

问题在于：

- 默认端口不该总是带上
- IPv6 host 需要方括号
- `http` / `https` / `ws` / `wss` 的默认端口判断不同

当前更稳的入口是：

- `xrtUrlMakeHostHeader()`
- `xrtUrlMakeHostHeaderFixed()`


### 2.5 规范化路径不是“字符串清洗”，而是请求语义的一部分

当前 `xrtUrlNormalizePathTo()` 解决的是：

- 多余分隔符
- `.` / `..`
- 相对段收口

这很重要，因为：

- 路由匹配会受影响
- cache key 会受影响
- base URL 解析结果会受影响

它不是单纯“把字符串变好看”。


### 2.6 相对引用要基于 base URL 解析，不要手搓路径拼接

如果你拿到：

- base URL
- 相对引用

当前更稳的入口是：

- `xrtUrlResolveTo()`
- `xrtUrlResolve()`

不要自己做：

- 截掉最后一个 `/`
- 再手拼

因为：

- query / fragment
- 目录级别
- 根路径语义

都比表面复杂。


### 2.7 `ParseFixedTo()` 适合快路径，`ParseView()` 适合完整处理

推荐这样理解：

| 入口 | 适合场景 |
|------|----------|
| `xrtUrlParseView*()` | 需要完整字段、后续还要做 build / normalize / resolve |
| `xrtUrlParseFixedTo()` | 只想快速拿 `host / port / target` |
| `xrtUrlParse()` | 直接落到固定 `xurl` 结构 |

如果你的目标只是：

- 我确认它是 `http/https`
- 我拿 host / port / target 去连

那 `xrtUrlParseFixedTo()` 会很省事。


### 2.8 `xurl` 负责地址层，不负责 header/query/cookie 逐项文本语义

这页最容易混到一起的，是 `xurl` 和 `xhttp_util`。

你可以先这样分工：

| 模块 | 负责什么 |
|------|----------|
| `xurl` | URL / authority / target 的拆分、规范化、重建 |
| `xhttp_util` | header / query / cookie / multipart 等 HTTP 文本块 |

也就是说：

- `?a=1&b=2`
	- `xurl` 负责把 query 这一段切出来
- query 里每一对 `a=1`
	- 再交给 `xhttp_util`


## 3. 最小 DEMO：先把完整 URL 拆成连接需要的 4 段

第一步先不要急着发请求。  
先学会拿到：

1. `scheme`
2. `host`
3. `port`
4. `target`

```c
xrturlview tURL;
char sHost[256];
char sTarget[1024];

if ( xrtUrlParseView("https://api.example.com:8443/v1/chat?q=1", &tURL) ) {
	xrtUrlViewCopyHostTo(&tURL, sHost, sizeof(sHost));
	xrtUrlViewCopyTargetTo(&tURL, sTarget, sizeof(sTarget));

	printf("secure = %s\n", xrtUrlIsSecureScheme(tURL.tScheme) ? "true" : "false");
	printf("host = %s\n", sHost);
	printf("port = %u\n", tURL.iPort);
	printf("target = %s\n", sTarget);
}
```

这一步的重点是先把“连接目标”和“请求 target”拆开。


## 4. 升级 DEMO：`Host` 头、路径归一化和相对 URL 解析

第二步再进入真正的客户端预处理主线：

1. 解析 base URL
2. 归一化 path
3. 解析相对引用
4. 构造最终 `Host` 头

```c
xrturlview tBase;
char sResolved[1024];
char sNormalized[1024];
char sHostHeader[256];
size_t iLen = 0u;

if ( xrtUrlParseView("https://api.example.com/v1/models/", &tBase) ) {
	xrtUrlNormalizePathTo("/v1/../v1/chat/./completions", 0, sNormalized, sizeof(sNormalized), &iLen);
	xrtUrlResolve(&tBase, "../files?id=7", sResolved, sizeof(sResolved), &iLen);
	xrtUrlMakeHostHeader(&tBase, sHostHeader, sizeof(sHostHeader));
}
```

这条链很适合接：

- `xhttp`
- 代理 CONNECT 目标
- WebSocket client URL 预处理


## 5. 什么时候该用 `xurl`

| 场景 | 是否应该先过 `xurl` |
|------|---------------------|
| HTTP client 发请求 | 是 |
| WebSocket client 连接 URL | 是 |
| 代理目标地址与 `Host` 头构造 | 是 |
| HTTP server 处理请求 target | 通常只需 target/authority 那部分 |
| 只处理 query 中每个键值对 | 否，交给 `xhttp_util` |


## 6. 常见错误

### 6.1 把 `host` 和 `target` 混成一串

这会直接导致：

- 连错地址
- 请求行错误
- 代理目标错误


### 6.2 手搓 `Host` 头

尤其是：

- 默认端口
- IPv6

最容易出错。


### 6.3 把 view 当成独立持有的数据

`xrturlview` 默认借原文。  
原始缓冲区失效后，view 也就不再可靠。


### 6.4 用字符串拼接代替 `Resolve`

相对 URL 解析比看起来复杂，不值得自己重写一遍。


## 7. 下一步阅读

建议按这条线继续：

- [HTTP Util 入门：Header、Query、Cookie 和 multipart 不是一堆字符串](http-util-intro.md)
- [Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序](proxy-intro.md)
- [XURL API](../api/api-xurl.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](../case/xhttp-client-proxy-tls.md)
