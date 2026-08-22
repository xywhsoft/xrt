# XNet Proxy 共享代理对象

> 当前 XRT 网络主线里的代理能力不是散落在 `xhttp`、`xws` 和底层 stream 的一堆临时字段，而是统一收口成共享、引用计数式的 `xnetproxy` 对象。

[返回索引](README.md)

---

## 1. 定位

当前 `xnet_proxy` 主线主要解决：

- 统一表达代理配置
- 在不同网络模块之间共享代理对象
- 让连接层沿同一条“直连 / 代理”时序工作

它服务于：

- `xnet-v2` 连接主线
- `xhttp`
- `xws`

当前正式支持的代理类型只有两类：

- `SOCKS5`
- `HTTP CONNECT`

也就是说，当前这层负责的是：

- 面向连接的代理通路

不是：

- 浏览器 PAC
- 自动发现
- 复杂代理策略编排


## 2. 使用边界

先记住下面 8 条：

1. `xnetproxy` 是共享对象，不是每个请求都重新拷一份 `host/port/user/pass`。
2. 当前正式支持 `XNET_PROXY_SOCKS5` 和 `XNET_PROXY_HTTP_CONNECT`；`XNET_PROXY_NONE` 只应作为“未启用代理”的初始化值。
3. 代理对象本身不发起连接，也不参与等待；它只是被挂到连接配置上，由连接主线在建连时使用。
4. 代理会真实插入连接时序：`TCP -> proxy handshake -> TLS -> application protocol`。
5. 目标站点仍然来自 `xnetconnectconfig` 或上层 URL 解析结果，不来自代理配置本身。
6. `xrtNetProxyCreate()` 会校验配置；无效类型、空 host、无效端口这类情况都会导致创建失败并返回 `NULL`。
7. `xrtNetProxyAddRef(NULL)` 当前会返回 `NULL`；`xrtNetProxyRelease(NULL)` 是 no-op。
8. 上层模块对代理引用的持有和释放各有自己的边界，不能只 `Release()` 一次就默认全部收干净。


## 3. 常量与类型

### 3.1 代理类型

```c
#define XNET_PROXY_NONE          0
#define XNET_PROXY_SOCKS5        1
#define XNET_PROXY_HTTP_CONNECT  2
```

推荐理解：

- `NONE`
	- 默认初始化值，不应拿去创建正式代理对象
- `SOCKS5`
	- SOCKS5 CONNECT 风格代理
- `HTTP_CONNECT`
	- HTTP CONNECT 隧道代理


### 3.2 固定容量

```c
#define XNET_PROXY_HOST_CAP  256u
#define XNET_PROXY_USER_CAP  128u
#define XNET_PROXY_PASS_CAP  128u
```

这意味着：

- 代理配置是固定缓冲区风格
- 适合作为稳定的运行时配置对象
- 不需要为每个字段额外做堆分配


### 3.3 `xnetproxyconfig`

```c
typedef struct {
	int iType;
	char sHost[XNET_PROXY_HOST_CAP];
	uint16 iPort;
	char sUser[XNET_PROXY_USER_CAP];
	char sPass[XNET_PROXY_PASS_CAP];
} xnetproxyconfig;
```

字段职责：

- `iType`
	- 代理类型
- `sHost`
	- 代理服务器地址
- `iPort`
	- 代理服务器端口
- `sUser` / `sPass`
	- 可选认证信息


### 3.4 `xnetproxy`

这是不透明共享对象。  
当前实现内部持有：

- 引用计数
- 一份固定拷贝后的 `xnetproxyconfig`

调用方不直接改内部字段，而是通过：

- `Create`
- `AddRef`
- `Release`

管理它。


## 4. 公开 API

```c
XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy);
XXAPI void xrtNetProxyRelease(xnetproxy* pProxy);
```


### 4.1 `xrtNetProxyConfigInit()`

用途：

- 初始化代理配置对象

当前实现会：

- `memset(0)`
- 并把 `iType` 设成 `XNET_PROXY_NONE`

所以推荐总是先：

```c
xnetproxyconfig tCfg;
xrtNetProxyConfigInit(&tCfg);
```


### 4.2 `xrtNetProxyCreate()`

用途：

- 从配置创建共享代理对象

使用边界：

- 返回 `NULL` 表示创建失败
- 成功时初始引用计数为 1
- 当前会把配置内容复制进对象内部

也就是说，创建成功后：

- 外部原始 `xnetproxyconfig` 可以离开作用域


### 4.3 `xrtNetProxyAddRef()`

用途：

- 给共享代理对象增加一份引用

典型场景：

- 外部持有者保留一份
- 同时把另一份挂到 `xhttprequest` 或 `xwsclientconfig`


### 4.4 `xrtNetProxyRelease()`

用途：

- 释放一份引用

当前行为：

- 引用计数减到 0 时销毁对象


## 5. 典型接入点

### 5.1 `xnetconnectconfig`

当前底层连接配置里有：

```c
typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iFlags;
	uint32 iConnectTimeoutMs;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	xnetproxy* pProxy;
} xnetconnectconfig;
```

这里要记住：

- `sHost / iPort`
	- 仍然是目标站点
- `pProxy`
	- 只是决定这次连目标时要不要先过代理


### 5.2 `xhttprequest`

`xhttp` 当前通过：

- `xhttprequest.pProxy`

接入代理。

真实生命周期边界：

- 你通常先 `xrtNetProxyAddRef(pProxy)` 再赋给请求对象
- `xrtHttpRequestUnit()` 会释放请求对象持有的那份引用


### 5.3 `xwsclientconfig`

`xws` 当前通过：

- `xwsclientconfig.pProxy`

接入代理。

真实生命周期边界：

- `xrtWsClientCreate()` 当前会为配置里的 `pProxy` 增加引用
- `xrtWsClientDestroy()` 时释放


## 6. 最小用法

### 6.1 创建共享代理对象

```c
xnetproxyconfig tCfg;
xnetproxy* pProxy;

xrtNetProxyConfigInit(&tCfg);
tCfg.iType = XNET_PROXY_HTTP_CONNECT;
strcpy(tCfg.sHost, "127.0.0.1");
tCfg.iPort = 7897u;

pProxy = xrtNetProxyCreate(&tCfg);
if ( pProxy == NULL ) {
	return 1;
}

xrtNetProxyRelease(pProxy);
```


### 6.2 挂到 HTTP 请求

```c
xrtHttpRequestInit(&tReq);
tReq.pProxy = xrtNetProxyAddRef(pProxy);
```

说明：

- `RequestUnit()` 会释放 `tReq.pProxy`
- 外部持有者最后还要再 `xrtNetProxyRelease(pProxy)`


### 6.3 挂到 WebSocket client

```c
xrtWsClientConfigInit(&tCfg);
tCfg.pProxy = pProxy;

pClient = xrtWsClientCreate(pEngine, &tCfg, &tEvents, NULL);
```

说明：

- `Create()` 当前会自己加引用
- 外部持有者仍应在合适时机 release 自己那份


## 7. 推荐心智模型

把代理理解成下面这条链最稳：

1. 先解析目标 URL
2. 再决定是否给这次连接挂共享代理对象
3. 连接层先连代理服务器
4. 完成代理握手
5. 如果目标是安全 scheme，再做 TLS
6. 最后才进入 HTTP / WebSocket / 其他应用协议

也就是：

- `URL -> connect config + proxy -> stream connect -> proxy handshake -> TLS -> app protocol`


## 8. 常见错误

1. 直接把 `XNET_PROXY_NONE` 当正式代理类型去创建对象。
2. 以为代理 host/port 就是最终目标 host/port。
3. 把代理看成请求层临时 header，而不是连接层共享对象。
4. 给请求或 client 配置挂上代理后，忘记外部自己那份引用还要 release。
5. 把不同代理出口的连接混用成一条连接池策略。


## 9. 相关阅读

- [XNet V2](api-xnet-v2.md)
- [XHTTP](api-xhttp.md)
- [XWS](api-xws.md)
- [Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序](../guide/proxy-intro.md)
