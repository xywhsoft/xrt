# XWS WebSocket 主线

> 基于 `xnet-v2 + xnet_stream + xhttp upgrade + xtlssession` 的当前 WebSocket 主线。

[返回索引](README.md)

---

## 1. 定位

`xws` 是当前主线 WebSocket 层。

它提供：

- WebSocket client
- WebSocket server
- HTTP upgrade 握手
- WebSocket client 代理接入
- text / binary 消息
- ping / pong
- close
- 分片消息重组

它的目标是：

- 提供足够完整的 WebSocket 基础设施
- 保持轻量
- 与当前 `xnet-v2` 主线和 TLS session 主线自然衔接


## 2. 核心类型

### `xwsclientconfig`

主要字段：

- `sURL`
- `sOrigin`
- `sProtocol`
- `iConnectTimeoutMs`
- `iRecvLimit`
- `bVerifyPeer`
- `pProxy`


### `xwsserverconfig`

主要字段：

- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`
- `sProtocol`


### 事件对象

客户端事件：

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xwsclient* pClient);
	void (*OnText)(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsclient* pClient, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsclient* pClient, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
} xwsclientevents;
```

服务端事件：

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xwsserver* pServer, xwsconn* pConn);
	void (*OnText)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
} xwsserverevents;
```


## 3. 当前正式 API

### 配置初始化

```c
XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg);
XXAPI void xrtWsClientConfigUnit(xwsclientconfig* pCfg);
XXAPI bool xrtWsClientConfigSetURL(xwsclientconfig* pCfg, const char* sURL);
XXAPI bool xrtWsClientConfigSetOrigin(xwsclientconfig* pCfg, const char* sOrigin);
XXAPI bool xrtWsClientConfigSetProtocols(xwsclientconfig* pCfg, const char* sProtocols);
XXAPI const char* xrtWsClientConfigURL(const xwsclientconfig* pCfg);
XXAPI const char* xrtWsClientConfigOrigin(const xwsclientconfig* pCfg);
XXAPI const char* xrtWsClientConfigProtocols(const xwsclientconfig* pCfg);
XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg);
XXAPI void xrtWsServerConfigUnit(xwsserverconfig* pCfg);
XXAPI bool xrtWsServerConfigSetProtocol(xwsserverconfig* pCfg, const char* sProtocol);
XXAPI const char* xrtWsServerConfigProtocol(const xwsserverconfig* pCfg);
```

配置 setter 按实际长度保存 URL、Origin 和协议字符串，旧版 `XWS_*_CAP` 宏只是内联存储阈值，不是协议长度限制。使用 setter 后应调用对应的 `ConfigUnit`；client/server 创建时会深拷贝配置字符串，因此创建完成后可以立即释放源配置。


### 客户端生命周期

```c
XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData);
XXAPI xnet_result xrtWsClientStart(xwsclient* pClient);
XXAPI void xrtWsClientStop(xwsclient* pClient);
XXAPI void xrtWsClientDestroy(xwsclient* pClient);
XXAPI bool xrtWsClientIsOpen(const xwsclient* pClient);
```

说明：

- `xwsclientconfig.pProxy` 为可选共享代理对象
- client create 时会为该对象增加引用；client destroy 时释放


### 客户端发送控制

```c
XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen);
XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason);
XXAPI const char* xrtWsClientProtocol(const xwsclient* pClient);
```


### 服务端生命周期

```c
XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData);
XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer);
XXAPI xnet_result xrtWsServerStart(xwsserver* pServer);
XXAPI void xrtWsServerStop(xwsserver* pServer);
XXAPI void xrtWsServerDestroy(xwsserver* pServer);
```


### 服务端连接发送控制

```c
XXAPI bool xrtWsConnIsOpen(const xwsconn* pConn);
XXAPI xnet_result xrtWsConnSendText(xwsconn* pConn, const char* sText, size_t iLen);
XXAPI xnet_result xrtWsConnSendBinary(xwsconn* pConn, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsConnPing(xwsconn* pConn, const void* pData, size_t iLen);
XXAPI xnet_result xrtWsConnClose(xwsconn* pConn, uint16 iCode, const char* sReason);
XXAPI const char* xrtWsConnProtocol(const xwsconn* pConn);
```

协商后的 subprotocol 由 client 或 connection 对象持有，到对象销毁前可通过对应 accessor 读取。


## 4. 协议行为

当前主线提供的协议能力包括：

- HTTP upgrade 握手
- client 侧代理握手
- `Sec-WebSocket-Key / Accept` 处理
- text / binary frame 收发
- ping / pong
- close frame
- 分片消息重组
- text 消息与 close reason 的严格 UTF-8 校验
- close code、控制帧、mask、RSV、opcode 与最短长度编码校验

客户端 URL 必须是绝对 `ws://` 或 `wss://` URL，userinfo 与 fragment 会被拒绝；长 Host 和请求目标使用动态存储。握手严格校验唯一且合法的 upgrade 字段、key、version 和选中的 subprotocol。服务端要求客户端帧带 mask，客户端要求服务端帧不带 mask。

当前明确未实现或暂不覆盖：

- extension
- `permessage-deflate`
- 复杂的 subprotocol 协商策略

客户端建连时序为：

- `TCP connect`
- 如果配置了 `pProxy`，先完成代理握手
- 如果是 `wss://`，再完成 TLS 握手
- 最后完成 HTTP upgrade，并进入 WebSocket open


## 5. 常见用法

### 5.1 WebSocket 客户端

```c
xwsclientconfig tCfg;

xrtWsClientConfigInit(&tCfg);
xrtWsClientConfigSetURL(&tCfg, "wss://example.com/ws");
tCfg.iConnectTimeoutMs = 10000u;
tCfg.bVerifyPeer = true;

pClient = xrtWsClientCreate(pEngine, &tCfg, &tEvents, pUserData);
xrtWsClientConfigUnit(&tCfg);
xrtWsClientStart(pClient);
```


### 5.2 通过代理建立 WebSocket 客户端

```c
xnetproxyconfig tProxyCfg;
xnetproxy* pProxy;

xrtNetProxyConfigInit(&tProxyCfg);
tProxyCfg.iType = XNET_PROXY_SOCKS5;
strcpy(tProxyCfg.sHost, "127.0.0.1");
tProxyCfg.iPort = 7897u;

pProxy = xrtNetProxyCreate(&tProxyCfg);

xrtWsClientConfigInit(&tCfg);
xrtWsClientConfigSetURL(&tCfg, "wss://example.com/ws");
tCfg.pProxy = pProxy;

pClient = xrtWsClientCreate(pEngine, &tCfg, &tEvents, pUserData);
xrtWsClientConfigUnit(&tCfg);
xrtNetProxyRelease(pProxy);
xrtWsClientStart(pClient);
```

### 5.3 客户端发送文本

```c
xrtWsClientSendText(pClient, sText, strlen(sText));
```


### 5.4 WebSocket 服务端

```c
xwsserverconfig tCfg;

xrtWsServerConfigInit(&tCfg);
xrtNetAddrInitAny(&tCfg.tBindAddr, AF_INET, 8080u);

pServer = xrtWsServerCreate(pEngine, &tCfg, &tEvents, pUserData);
xrtWsServerStart(pServer);
```


### 5.5 服务端连接发消息

```c
xrtWsConnSendText(pConn, sText, strlen(sText));
```


## 6. 与其他模块的关系

### 与 `xhttp`

- 握手阶段本质上复用了 HTTP upgrade 语义

### 与 `xnet_stream`

- WebSocket 连接建立在 stream 之上
- 关闭、代理、TLS、收发链都来自 stream 主线

### 与 `xtlssession`

- `wss://` 通过当前 TLS session 主线工作

### 与协程 / future

- `xws` 目前主路径仍是事件回调
- 但它建立在当前异步主线和网络主线上，后续扩到 wait-source 时阻力较小

也就是说，当前 WebSocket 主线的推荐理解方式是：

- 协议和连接管理：已经进入当前网络主线
- 上层消息组织：仍以事件回调为主
- 更深的 wait-source / future 化扩展：属于自然可演进方向，但不是当前正式主路径

## 7. 当前最推荐的使用场景

当前 `xws` 最适合：

- 需要稳定 WebSocket client / server 能力的基础设施程序
- 需要 text / binary / ping / pong / close 的常规服务
- 需要建立在 `xnet-v2 + xtlssession` 主线上的双向通信程序

如果你的目标是：

- 浏览器级扩展支持
- 复杂 extension 协商
- 重量级协议栈模拟

当前主线并不是朝这个方向设计的。

## 8. 当前边界

当前 `xws` 文档覆盖的是：

- 当前 WebSocket client / server 主线
- 生命周期
- 收发与握手

它的定位是：

- 现代基础设施中的 WebSocket 组件
- 不是浏览器端协议栈模拟器

## 9. 建议继续阅读

- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)
- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [XHTTP](api-xhttp.md)
