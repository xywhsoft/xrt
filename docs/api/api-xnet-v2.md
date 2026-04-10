# XNet V2 网络主线

> 当前正式网络主线总览。本文档不再描述旧网络模块族，而是围绕 `xnet-v2 + xtlssession + xhttp/xhttpd/xws` 组织。

[返回索引](README.md)

---

## 1. 定位

XNet V2 是 XRT 当前正式网络主线，目标是提供：

- 高性能
- 跨平台
- 线程、协程、future 一致的异步语义
- 共享代理对象与可选代理握手阶段
- 可继续向 HTTP / WebSocket / AI 场景上层扩展

它不是旧 TCP/UDP 客户端库的简单替代，而是新的统一网络底座。


## 2. 当前主线结构

### 2.1 URL 与 HTTP 辅助

- [xurl.h](../../lib/xurl.h)
- [xhttp_util.h](../../lib/xhttp_util.h)

负责：

- URL 解析与拆分
- HTTP 辅助数据处理

当前文档后续单独补齐。

### 2.2 网络底座

- [xnet_base.h](../../lib/xnet_base.h)
- [xnet_mem.h](../../lib/xnet_mem.h)
- [xnet_port.h](../../lib/xnet_port.h)
- [xnet_port_iocp.h](../../lib/xnet_port_iocp.h)
- [xnet_port_uring.h](../../lib/xnet_port_uring.h)

负责：

- 基础类型
- socket / addr / result
- 共享代理对象与代理配置
- 内存与链表辅助
- 平台端口层

### 2.3 编解码

- [xcodec.h](../../lib/xcodec.h)
- [xcodec_http1.h](../../lib/xcodec_http1.h)
- [xcodec_ws.h](../../lib/xcodec_ws.h)

负责：

- HTTP/1.1 编解码
- WebSocket 编解码

### 2.4 Engine

- [xnet_engine.h](../../lib/xnet_engine.h)

负责：

- worker 线程
- 任务投递
- delayed task
- 网络对象运行时归属

### 2.5 Stream 与 Dgram

- [xnet_stream.h](../../lib/xnet_stream.h)
- [xnet_dgram.h](../../lib/xnet_dgram.h)

负责：

- 面向连接的流式对象
- UDP / dgram 对象
- readable / writable / drain / close 等等待面

### 2.6 Sync / Future / Wait-Source

- [xnet_sync.h](../../lib/xnet_sync.h)

负责：

- `xnetfuture`
- `xnetwaitsrc`
- 线程等待
- 协程等待
- stream / dgram / listener / future 的统一等待桥接

### 2.7 TLS 与应用层

- [nettls.h](../../lib/nettls.h)
- [xhttp.h](../../lib/xhttp.h)
- [xhttpd.h](../../lib/xhttpd.h)
- [xws.h](../../lib/xws.h)

负责：

- TLS session
- HTTP client
- HTTP server
- WebSocket


## 3. 当前正式 API 面

### 3.1 Engine

```c
XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg);
XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);
XXAPI void xrtNetEngineDestroy(xnetengine* pEngine);
XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine);
XXAPI void xrtNetEngineStop(xnetengine* pEngine);
XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine);
XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg);
XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);
```

### 3.2 TLS Session

```c
XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
```

更完整说明见：

- [api-network-tls.md](api-network-tls.md)

### 3.3 Proxy

```c
XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg);
XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy);
XXAPI void xrtNetProxyRelease(xnetproxy* pProxy);
```

说明：

- `xnetproxy` 是共享、引用计数式的代理对象
- 连接运行态只保存 `xnetproxy*`，不会把整份代理配置拷入每个连接结构体
- 当前正式支持：
	- `SOCKS5 CONNECT`
	- `HTTP CONNECT`
- `xnetconnectconfig.pProxy == NULL` 表示直连

更完整的代理合同与生命周期说明见：

- [api-xnet-proxy.md](api-xnet-proxy.md)

### 3.4 Listener

```c
XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg, ...);
XXAPI void xrtNetListenerDestroy(xnetlistener* pListener);
XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener);
XXAPI void xrtNetListenerStop(xnetlistener* pListener);
```

### 3.5 Future / Wait-Source

```c
XXAPI xnetfuture* xrtNetFutureCreate(void);
XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture);
XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs);
XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture);
```

说明：

- `xnetfuture` 是当前正式 `xfuture` 主线在网络层的实现与桥接对象
- 线程侧、协程侧、wait-source 侧的等待语义都以它为统一结果载体
- 当前推荐优先理解 “network object -> wait-source -> future / coroutine wait” 这条链，而不是继续为每个对象记忆一套互不相同的等待协议

```c
XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture);
XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind);
XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock);
XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener);
```

### 3.5 统一等待

线程侧：

```c
XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc);
XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetWaitSourceWaitUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs);
```

协程侧：

```c
XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
XXAPI xnet_result xrtNetWaitSourceWaitCoTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetWaitSourceWaitCoUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs);
```

带值版本：

```c
XXAPI xnet_result xrtNetWaitSourceWaitValue(const xnetwaitsrc* pSrc, ptr* ppValue);
XXAPI xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue);
```


## 4. 当前等待面

### 4.1 Future

- 普通 future 等待
- timeout / deadline
- 协程等待

### 4.2 Stream

当前已进入统一等待主线的 stream wait-kind：

- readable
- writable
- drain
- close

这些等待面同时有：

- future bridge
- sync-thread wait
- coroutine wait
- timeout / deadline 版本

当前更推荐优先从统一入口理解这条线：

- `xrtNetStreamFutureEx`
- `xrtNetStreamWaitEx / WaitTimeoutEx / WaitUntilEx`
- `xrtNetStreamWaitCoEx / WaitCoTimeoutEx / WaitCoUntilEx`

而把 `Readable / Writable / Drain / Close` 这些专名 helper 看成更易读的薄封装。

当前 stream 的预打开时序为：

- `TCP connect`
- 如果配置了 `pProxy`，先完成代理握手
- 如果配置了 TLS，再完成 TLS 握手
- 最后才触发 `OnOpen`

这带来几个重要语义：

- `OnOpen` 之前，`xrtNetStreamSend / SendVec / SendRef` 统一返回 `XRT_NET_AGAIN`
- `iConnectTimeoutMs` 覆盖整个预打开阶段，而不只是裸 `connect`
- `xrtNetStreamRemoteAddr()` 保持返回实际 socket 对端地址；启用代理时这里是代理地址，不是目标地址

### 4.3 Dgram

当前已进入统一等待主线的 dgram 能力：

- recv future
- sync recv
- coroutine recv
- timeout / deadline 版本

### 4.4 Listener

当前已有：

- `xrtNetListenerAcceptFuture`
- `xrtNetListenerAccept`
- `xrtNetListenerAcceptTimeout`
- `xrtNetListenerAcceptUntil`
- `xrtNetListenerAcceptCo`
- `xrtNetListenerAcceptCoTimeout`
- `xrtNetListenerAcceptCoUntil`

但 listener accept 链路仍然属于近期重构重点区域，后续还要继续做更重压力和竞态验证。

### 4.5 统一 wait-source 当前覆盖范围

当前 `xnetwaitsrc` 已正式覆盖：

- future
- stream wait-kind
- dgram recv
- listener accept

并且同时具备：

- sync-thread wait
- coroutine wait
- timeout / deadline
- value 返回路径


## 5. 与协程 / future 主线的关系

XNet V2 不再只是回调式网络层。

它已经和当前正式异步主线形成接线：

- `future`
- `wait-source`
- coroutine wait
- thread wait
- engine post future

这使得网络异步对象不再需要各自发明一套等待模型。

当前更推荐把 `xnet-v2` 理解成：

- engine：执行与归属
- stream / dgram / listener：网络对象
- future：异步结果
- wait-source：统一等待桥
- coroutine / thread wait：两套消费入口

这样 HTTP、WebSocket、TLS、AI 场景中的流式等待，都能沿同一条主线继续扩展。

## 6. 当前最推荐的理解顺序

如果第一次接触当前网络主线，建议按下面顺序阅读和实践：

1. 先理解 `engine`
2. 再理解 `stream / dgram / listener`
3. 再理解 `wait-source / future / coroutine wait`
4. 再理解 `xtlssession`
5. 最后回到 `xhttp / xhttpd / xws`

不要一开始就只记大量专名 API，而忽略统一运行时语义。

## 7. 与旧网络主线的边界

当前正式文档不再推荐：

- 旧网络模块族
- 旧 `nettls` public surface

原因：

- 它们已不再是当前主线
- 新主线已经围绕 `xnet-v2 + xtlssession` 收口

历史文档后续统一归档到 `dev/`，不再作为正式入口。


## 8. 当前主线优势

相对于旧网络层，XNet V2 的核心优势是：

- 统一 engine/worker 模型
- 统一 future/wait-source 桥接
- 统一协程等待入口
- stream / dgram / listener 都能接入相同等待语义
- 代理已重新并入主线，而且以共享对象方式接入，不放大每连接常驻内存
- TLS 已收成 session 主线
- 更适合继续承载 HTTP / WebSocket / AI 场景的流式网络能力


## 9. 当前文档边界

本文档是总览，不试图替代后续模块级 API 文档。

后续需要继续补齐：

- `api-xurl.md`
- `api-http-util.md`
- `api-xhttp.md`
- `api-xhttpd.md`
- `api-xws.md`

这些模块文档应建立在本文档的主线定位之上，而不是再回到旧网络表面。

