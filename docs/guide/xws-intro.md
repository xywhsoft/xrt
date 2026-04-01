# XWS 入门：什么时候该让 WebSocket 回调负责协议边界，什么时候该把业务交给 Queue 和 Coroutine

> 目标：把 `xrtWsClientCreate()/Start()`、`xrtWsServerCreate()/Start()`、`xrtWsClientSendText()`、`xrtWsConnSendText()` 讲成第 6 阶段网络主线里“WebSocket 会话层”这条正式第一页。读完这页后，你应该明确知道：当前 `xws` 的正式主路径为什么仍是事件回调，客户端和服务端各自负责什么，什么时候一条 WebSocket 会话已经不该继续把业务逻辑塞在 `OnText` 里。

[返回教学文档](README.md)

---

## 1. 为什么 `xws` 要单独讲

很多人一看到 WebSocket，就会本能地把它理解成：

- “就是一个能双向收发字符串的长连接”

然后代码就很容易直接滑向：

- `OnText` 里做所有业务
- 回调里直接查库、写文件、发 HTTP
- 客户端回包和服务端事件都混在一起

短期能跑，长期一定会变得难以维护。

因为 WebSocket 这层真正要解决的事情，不只是：

- 连接着没断

而是：

- HTTP upgrade
- `ws://` / `wss://` 的连接路径
- ping / pong
- close
- 分片重组
- 客户端和服务端的会话边界

这就是 `xws` 的位置。


## 2. 先把 9 个边界分清楚

### 2.1 `xws` 是协议会话层，不是底层网络替代品

你可以先把当前网络主线拆成 4 层：

| 层 | 负责什么 |
|----|----------|
| `xurl` | URL、authority、target |
| `xnet-v2` | engine、stream、连接运行时 |
| `xtlssession` | `wss://` 的 TLS 会话 |
| `xws` | WebSocket 握手、帧、消息和会话 |

所以 `xws` 不是：

- 重新造一个 socket 层

它是：

- 建在当前网络主线之上的 WebSocket 协议层


### 2.2 当前 `xws` 正式主路径仍是事件回调

这是这页最重要的前提。

当前公开 API 面里，`xws` 的主路径是：

- `OnOpen`
- `OnText`
- `OnBinary`
- `OnClose`
- `OnError`
- `OnPing`
- `OnPong`

也就是说，当前正式文档不应该把它讲成：

- 已经有完整 `future` 化发送/接收
- 已经有正式 `wait-source` 会话接口

这些可能是后续演进方向，但不是当前正式主路径。


### 2.3 客户端和服务端各自有自己的对象边界

当前公开面里：

- 客户端对象
	- `xwsclient`
- 服务端对象
	- `xwsserver`
- 服务端单个连接
	- `xwsconn`

推荐这样理解：

| 对象 | 角色 |
|------|------|
| `xwsclient` | 主动发起连接的会话对象 |
| `xwsserver` | 监听和管理整体服务 |
| `xwsconn` | 服务端接受到的单个客户端连接 |

这意味着服务端发消息不是对着 `xwsserver` 发，而是对着：

- `xwsconn`


### 2.4 客户端建连不是一步，而是一条时序链

如果你走的是：

- `ws://`
- `wss://`

当前更稳的时序理解是：

1. 先解析 URL
2. 如果配置了代理，先连代理并做代理握手
3. 如果是 `wss://`，再做 TLS
4. 最后做 HTTP upgrade
5. 进入 WebSocket open

也就是：

- `TCP -> proxy handshake -> TLS -> HTTP upgrade -> WS open`

这也是为什么前面要先学：

- `xurl`
- `proxy`
- `xnet-v2 / TLS`


### 2.5 `xws` 负责协议边界，不适合在回调里做重业务

回调更适合承担的是：

- 收到消息后的最小解析
- 快速 ACK
- 复制输入
- 推入队列
- 更新轻量状态

它不适合长期承担：

- 重 CPU
- 阻塞 I/O
- 长事务
- 一整条会话编排

这时就该把业务交给：

- `queue`
- `coroutine`
- `future/task`

这也是为什么案例页单独用了：

- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)


### 2.6 `pProxy` 只在客户端配置里出现

当前公开配置上：

- `xwsclientconfig`
	- 有 `pProxy`
- `xwsserverconfig`
	- 没有 `pProxy`

所以代理这条线当前主要是：

- WebSocket client 经代理接出

不是：

- 服务端监听本地时也走 `pProxy`


### 2.7 `wss://` 的证书校验边界在客户端配置里

当前客户端配置里有：

- `bVerifyPeer`

这意味着：

- 你可以决定是否严格校验对端证书

正式场景里更推荐：

- `bVerifyPeer = true`

只有本地调试、自签证书演练这类场景，才有理由临时放宽。


### 2.8 当前主线覆盖常规 WebSocket，不覆盖浏览器级扩展生态

当前文档和 API 明确覆盖的是：

- text / binary
- ping / pong
- close
- 分片消息重组

当前不应假设已经正式覆盖：

- `permessage-deflate`
- 复杂 extension 协商
- 浏览器协议栈级的全部能力


### 2.9 `xws` 和 `xhttpd` 的关系是“借 upgrade 语义”，不是互相替代

`xws` 的握手阶段本质上借用了：

- HTTP upgrade

但一旦进入 WebSocket open，语义就已经和普通 HTTP 请求不同了。

所以不要把它理解成：

- “就是 `xhttpd` 多了个长连接”


## 3. 最小 DEMO：先把 WebSocket client 跑起来

第一步先不要同时写 server。  
先学会最小客户端主线：

1. 配置 URL
2. 填事件回调
3. `Create -> Start`
4. 在 `OnOpen` 之后发消息

```c
static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	(void)pOwner;
	(void)xrtWsClientSendText(pClient, "hello", 5u);
}

static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	(void)pOwner;
	(void)pClient;
	printf("recv: %.*s\n", (int)iLen, pData);
}

int main(void)
{
	xwsclientconfig tCfg;
	xwsclientevents tEvents;
	xwsclient* pClient;

	xrtInit();
	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnOpen = procClientOnOpen;
	tEvents.OnText = procClientOnText;

	xrtWsClientConfigInit(&tCfg);
	strcpy(tCfg.sURL, "wss://example.com/ws");
	tCfg.iConnectTimeoutMs = 10000u;
	tCfg.bVerifyPeer = true;

	pClient = xrtWsClientCreate(pEngine, &tCfg, &tEvents, NULL);
	xrtWsClientStart(pClient);
}
```

这一步的重点不是“业务怎么写”，而是先掌握客户端生命周期。


## 4. 升级 DEMO：再把服务端边界接上

第二步再进入服务端：

1. 绑定地址
2. 创建 server
3. 在 `OnOpen / OnText / OnClose` 里做最小协议处理
4. 真正业务通过队列移交

```c
static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	(void)pOwner;
	(void)pServer;
	printf("server recv: %.*s\n", (int)iLen, pData);
	(void)xrtWsConnSendText(pConn, "queued", 6u);
}

int main(void)
{
	xwsserverconfig tCfg;
	xwsserverevents tEvents;
	xwsserver* pServer;

	xrtInit();
	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnText = procServerOnText;

	xrtWsServerConfigInit(&tCfg);
	xrtNetAddrInitAny(&tCfg.tBindAddr, AF_INET, 8080u);

	pServer = xrtWsServerCreate(pEngine, &tCfg, &tEvents, NULL);
	xrtWsServerStart(pServer);
}
```

这里最该记住的是：

- 服务端发消息是对着 `xwsconn`
- 不是对着 `xwsserver`


## 5. 什么时候该把业务移出回调

一旦你出现下面这些需求，就不该继续把所有逻辑塞在 `OnText`：

- 要排队处理消息
- 要顺序编排多个会话动作
- 要调用磁盘、HTTP、数据库、模板
- 要把“连接打开 -> 发消息 -> 等回包 -> 再决定下一步”写成顺序流程

这时当前更稳的主线是：

- `xws callback -> queue -> coroutine`

直接看案例页会更顺：

- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)


## 6. 一张选型表：这次该停在哪一层

| 场景 | 优先入口 |
|------|----------|
| 只要稳定 WS client / server 基础设施 | `xws` 回调主线 |
| 需要代理接出 WebSocket client | `xwsclientconfig.pProxy` |
| 需要安全连接 | `wss://` + `bVerifyPeer` |
| 需要慢业务和消息交接 | `xws + queue` |
| 需要顺序编排会话流程 | `xws + queue + coroutine` |


## 7. 常见错误

### 7.1 把 `xws` 讲成已经正式 `future / wait-source` 化

当前不是这条主线。  
正式公开主路径仍是事件回调。


### 7.2 在 `OnText` 里做所有重活

这会让协议层和业务层彻底粘死。


### 7.3 服务端发消息时找错对象

服务端发送控制入口是：

- `xrtWsConnSendText()`
- `xrtWsConnSendBinary()`
- `xrtWsConnPing()`
- `xrtWsConnClose()`

不是对 `xwsserver` 本身发。


### 7.4 忽略 `wss://` 的证书校验边界

正式环境里不该默认关闭 `bVerifyPeer`。


### 7.5 把代理理解成“再补一层 HTTP header”

代理会真实插入连接时序，不只是应用层多拼一段文本。


## 8. 下一步阅读

建议按这条线继续：

- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)
- [XWS API](../api/api-xws.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序](proxy-intro.md)
