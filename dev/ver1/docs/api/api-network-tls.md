# Network TLS 会话层

> 当前正式 TLS 主线。对外 public surface 统一为 `xtlssession / xrtNetTlsSession*`，旧 `xtlsctx / xrtTls*` 已不再属于正式公共接口。

[返回索引](README.md)

---

## 1. 定位

XRT 当前 TLS 主线采用：

- 内部核心实现：TLS 引擎核心仍在 [nettls.h](../../lib/nettls.h)
- 对外正式表面：`xtlssession` 与 `xrtNetTlsSession*`

这意味着：

- 新网络主线
- HTTP client
- HTTP server
- WebSocket

都围绕 session 层工作，而不是直接暴露 TLS 核心上下文。


## 2. 核心类型

### `xtlssession`

```c
typedef struct xrt_tls_session xtlssession;
```

这是当前正式的 TLS 会话对象。

它负责：

- 握手驱动
- 密文输入
- 密文输出缓冲
- 明文写入
- 明文读取
- close 队列
- session resume 导出
- SNI 查询

### `xtlsconfig`

TLS 配置仍通过 `xtlsconfig` 提供，但当前使用方式已经围绕 session 层组织。

其中值得注意的一点是：

- `OnSNI` 当前收到的是 `xtlssession*`

而不是旧的 TLS core 上下文类型。


## 3. 当前正式 API

### 创建与销毁

```c
XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
```

说明：

- `bIsServer = TRUE` 表示服务端会话
- `bIsServer = FALSE` 表示客户端会话


### 握手与状态

```c
XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession);
```

说明：

- 会话握手是“驱动式”的
- 当握手尚未完成时，应继续喂入新的密文数据并重复 drive
- `xrtNetTlsSessionIsReady()` 为 `TRUE` 后，表示会话已进入可传输明文阶段


### 密文输入与输出

```c
XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen);
XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen);
```

说明：

- `FeedCipher`：把网络收到的 TLS 密文喂给 session
- `PendingCipher`：查看当前待发送密文长度
- `PeekCipher`：读取待发送密文
- `ConsumeCipher`：消费已发送的密文


### 明文输入与输出

```c
XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten);
XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
```

说明：

- `WritePlain`：把应用明文写入 session，由 session 负责加密并进入待发送密文队列
- `ReadPlain`：从 session 中读取已解密的应用明文
- `PendingRecv`：查看当前可读明文长度


### 关闭与恢复

```c
XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession);
XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession);
XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume);
XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession);
```

说明：

- `QueueClose`：把 TLS close 流程排入发送路径
- `ExportResume`：导出 resume 数据
- `WasResumed`：判断当前会话是否通过 resume 恢复


### SNI 与证书配置

```c
XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile);
XXAPI void xrtNetTlsSessionSetAllowTLS12Ed25519(xtlssession* pSession, bool bAllow);
```

说明：

- `GetSNI`：读取当前会话中的 SNI
- `SetCert`：给当前会话设置证书/私钥
- `SetAllowTLS12Ed25519`：控制 TLS 1.2 下的 Ed25519 兼容开关


## 4. 推荐使用方式

### 4.1 独立驱动

适用：

- 自己直接管理 socket/chain/buffer
- 做 TLS 协议边界测试

推荐流程：

1. `xrtNetTlsSessionCreate`
2. 网络收到密文后 `xrtNetTlsSessionFeedCipher`
3. 调用 `xrtNetTlsSessionDriveHandshake`
4. 检查 `xrtNetTlsSessionIsReady`
5. 握手完成后使用：
	- `xrtNetTlsSessionWritePlain`
	- `xrtNetTlsSessionReadPlain`
6. 网络层发送时：
	- `xrtNetTlsSessionPeekCipher`
	- `xrtNetTlsSessionConsumeCipher`

### 4.2 通过 `xnet_stream` 使用

这是当前更推荐的方式。

`xnet_stream` 已经把 TLS session 和网络主线接好了，因此：

- `xhttp`
- `xhttpd`
- `xws`

都会自然复用 session 主线，而不需要直接操作 TLS 核心状态机。

如果你当前的目标是：

- 写 HTTPS 客户端
- 写 HTTPS 服务
- 写 WSS client / server
- 调用 LLM API

那么更推荐站在：

- `xnet_stream`
- `xhttp`
- `xhttpd`
- `xws`

这些上层主线上使用 TLS，而不是直接手工驱动 session。


## 5. 为什么不再公开 `xtlsctx / xrtTls*`

原因不是这些实现失效了，而是：

- 它们更适合作为 TLS core 内部层
- 新网络主线已经统一站在 session 抽象上
- 继续公开两套 TLS public surface 会制造长期历史包袱

因此当前正式文档只保留 session 主线。


## 6. 与其他模块的关系

### 与 `xnet_stream`

- `xnet_stream` 是 session 最重要的上层承载者
- stream 内部可把 TLS session 与明文读写、事件回调、close 语义接在一起

### 与 `xhttp / xhttpd / xws`

- 这些模块都建立在当前网络主线上
- 因而都会间接复用 `xtlssession`

### 与 future / coroutine / wait-source

- TLS 自身不是一套独立异步体系
- 它是当前网络主线的一部分
- 因此会自然参与 stream wait、HTTP wait、future / coroutine 编排等更高层流程

### 与旧 TLS 文档

- 旧 `api-nettls*` 文档已退出正式主线
- 如需查历史资料，应看 `dev/` 归档，而不是把旧表面视为推荐 API


## 7. 当前正式边界

当前 TLS 主线文档只覆盖：

- session 层公共 API
- 与当前网络主线的关系

不再覆盖：

- 旧 `xrtTls*` 公共表面
- 旧 TCP/UDP/HTTP 客户端时代的 TLS 叙述
- “多后端抽象层”式的历史结构说明

## 8. 建议继续阅读

- [XNet V2](api-xnet-v2.md)
- [XHTTP](api-xhttp.md)
- [XHTTPD](api-xhttpd.md)
- [XWS](api-xws.md)

