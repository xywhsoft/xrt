# Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序

> 目标：把 `xrtNetProxyConfigInit()`、`xrtNetProxyCreate()`、`xrtNetProxyAddRef()`、`xrtNetProxyRelease()` 讲成第 6 阶段里“共享代理对象与连接路径”这条正式主线。读完这页后，你应该明确知道：在当前 XRT 主线里，代理不是散落在每个模块里的特判，而是一个共享、引用计数式的网络对象；它会真正插入 `TCP -> proxy handshake -> TLS -> application protocol` 这条连接时序。

[返回教学文档](README.md)

---

## 1. 为什么 `proxy` 要单独讲

很多项目里，代理都会被写成：

- 某个 HTTP client 里塞几个代理字段
- WebSocket 再复制一份
- 底层 TCP 连接再补一套特判

结果就是：

- client 和 ws 行为不一致
- 代理对象生命周期混乱
- 直连和代理两套时序混在一起

当前 XRT 主线不是这么组织的。  
它把代理收口成：

- `xnetproxy`

也就是说，代理先是一个共享对象，然后才被挂到：

- `xnetconnectconfig`
- `xhttprequest`
- `xwsclientconfig`


## 2. 先把 7 个边界分清楚

### 2.1 当前代理主线只支持两类 CONNECT 风格代理

当前正式支持：

- `XNET_PROXY_SOCKS5`
- `XNET_PROXY_HTTP_CONNECT`

这意味着当前主线的重点是：

- 让面向连接的 stream 在代理后建立到目标端的通路

而不是：

- 任意 HTTP 级反向代理语法
- 浏览器 PAC / 自动发现
- 高层代理策略系统


### 2.2 `xnetproxy` 是共享对象，不是每次请求都重新抄一份配置

当前 API 设计里：

- `xrtNetProxyCreate()`
	- 创建共享代理对象
- `xrtNetProxyAddRef()`
	- 增加引用
- `xrtNetProxyRelease()`
	- 释放引用

更稳的理解是：

- 代理配置先收口成一份对象
- 连接或请求再持有它的一份引用

这比“每层都复制一套 host/port/user/pass”更稳定。


### 2.3 直连和代理不是两套不同 API，只是 `pProxy` 是否为空

当前主线里：

- `pProxy == NULL`
	- 直连
- `pProxy != NULL`
	- 进入代理握手路径

这很重要，因为它意味着：

- 你不需要学两套客户端接口
- 你只是在连接配置里多挂了一层共享对象


### 2.4 代理会真正插入连接时序，不是“请求发出去前再补一行 header”

如果配置了 `pProxy`，当前主线应该这样理解：

1. 先连代理服务器
2. 完成代理握手
3. 如果目标是安全 scheme，再继续 TLS
4. 最后才进入 HTTP / WebSocket / 其他应用协议

也就是：

- `TCP -> proxy handshake -> TLS -> HTTP/WS`

这正是为什么 `proxy` 不能只在应用层靠手拼 header 糊过去。


### 2.5 代理对象的生命周期要和请求/连接引用分开

这是当前最容易写错的一点。

例如：

```c
pProxy = xrtNetProxyCreate(&tCfg);
tReq.pProxy = xrtNetProxyAddRef(pProxy);
```

这里其实有两份引用：

- 外部持有者那份
- 请求对象那份

所以结束时应该分别收口，而不是只 `Release()` 一次就以为够了。


### 2.6 `proxy` 不负责 URL 解析，目标地址仍要先过 `xurl`

代理要解决的是：

- 连接路径怎么走

但目标本身仍然要先知道：

- host
- port
- scheme

所以这条主线的更自然顺序是：

1. 先 `xurl`
2. 再决定是否挂 `proxy`
3. 再进入 `xhttp / xws / xnet-v2`


### 2.7 用户名密码是代理配置的一部分，不是请求 header 的临时拼装

当前 `xnetproxyconfig` 本身就包含：

- `sUser`
- `sPass`

这说明代理认证应优先放在代理对象配置层，而不是业务请求层零散拼接。


## 3. 当前配置对象长什么样

当前公开配置里最重要的是：

```c
#define XNET_PROXY_SOCKS5       1
#define XNET_PROXY_HTTP_CONNECT 2

typedef struct {
	int iType;
	char sHost[XNET_PROXY_HOST_CAP];
	uint16 iPort;
	char sUser[XNET_PROXY_USER_CAP];
	char sPass[XNET_PROXY_PASS_CAP];
} xnetproxyconfig;
```

这意味着一份代理配置的核心就是：

1. 类型
2. 代理主机
3. 代理端口
4. 可选认证信息


## 4. 最小 DEMO：先把共享代理对象建出来

第一步不要急着发请求。  
先学会正确创建、加引用、释放：

```c
xnetproxyconfig tProxyCfg;
xnetproxy* pProxy;

xrtNetProxyConfigInit(&tProxyCfg);
tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
strcpy(tProxyCfg.sHost, "127.0.0.1");
tProxyCfg.iPort = 7897u;

pProxy = xrtNetProxyCreate(&tProxyCfg);
if ( pProxy == NULL ) {
	return 1;
}

/* 后续把 pProxy 挂到请求或连接上 */

xrtNetProxyRelease(pProxy);
```

这一步重点不是“代理连没连通”，而是先建立生命周期意识。


## 5. 升级 DEMO：挂到 `xhttp` / `xws` 客户端主线里

第二步再把它接到真正使用层：

### 5.1 `xhttp`

```c
tReq.pProxy = xrtNetProxyAddRef(pProxy);
```

### 5.2 `xws`

```c
tCfg.pProxy = xrtNetProxyAddRef(pProxy);
```

当前更稳的习惯是：

1. 外部统一创建代理对象
2. 每个请求 / 客户端配置拿自己的一份引用
3. 请求或连接对象在 unit/destroy 时释放自己的那份
4. 外部持有者最后再 release


## 6. 什么时候需要 `proxy`

| 场景 | 是否该先考虑 `proxy` |
|------|----------------------|
| 企业出口代理访问外网 API | 是 |
| 把 HTTPS client 走 HTTP CONNECT | 是 |
| WebSocket client 经 SOCKS5 出口 | 是 |
| 服务端监听本地端口 | 否 |
| 只做 URL 解析、header 处理 | 否 |


## 7. 常见错误

### 7.1 只在应用层手拼代理 header

这会把真正的连接时序问题掩盖掉。


### 7.2 只 `Release()` 一次

如果你额外 `AddRef()` 过，请求对象和外部持有者通常不是同一份引用。


### 7.3 还没解析 URL，就先决定代理目标

代理之前，仍要先知道：

- scheme
- host
- port


### 7.4 把直连和代理当成两套完全不同的客户端代码

当前主线的优势就在于：

- 连接对象一致
- 只是 `pProxy` 是否为空不同


## 8. 下一步阅读

建议按这条线继续：

- [XURL 入门：什么时候先把 URL 拆对，比直接发请求更重要](xurl-intro.md)
- [HTTP Util 入门：Header、Query、Cookie 和 multipart 不是一堆字符串](http-util-intro.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](../case/xhttp-client-proxy-tls.md)
- [XNet V2 API](../api/api-xnet-v2.md)
