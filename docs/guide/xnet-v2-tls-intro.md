# xnet-v2 与 TLS Session 入门

> 目标：理解 XRT 当前网络主线为什么是 `xnet-v2`，以及 TLS 为什么统一收口到 `xtlssession`。

[返回教学文档](README.md)

---

## 1. 为什么要先理解 xnet-v2

如果你要用 XRT 做：

- TCP / TLS 客户端
- HTTP client
- HTTP 服务
- WebSocket
- 高并发网络程序

那么你真正站在的网络主线，就是 `xnet-v2`。

它不是“一个单独的网络模块”，而是当前网络基础设施的统一底座。


## 2. xnet-v2 在解决什么问题

传统网络代码往往会越来越碎：

- socket 一套
- 线程一套
- 回调一套
- 定时器一套
- TLS 再单独一套

而 `xnet-v2` 当前主线的目标，是把这些东西往统一模型上收：

- engine
- stream
- dgram
- future
- wait-source
- coroutine wait

这样后续不管是写 HTTP 还是写 WebSocket，都能建立在同一套运行时语义之上。


## 3. 可以怎样理解 xnet-v2

初学时，可以先这样记：

- `engine`：执行和调度底座
- `stream`：面向连接、字节流的通信模型
- `dgram`：面向数据包的通信模型
- `sync / future / wait-source`：等待与结果模型

也就是说：

- 网络不只是“收发数据”
- 网络也必须进入统一异步模型

这正是当前主线和旧式“回调拼装网络代码”的根本区别。


## 4. 为什么 TLS 现在统一成 Session

TLS 在当前主线里，不再推荐作为一组分散的低层公开接口来直接操作。

现在更推荐把它理解成：

- 一个和 `stream` 主线配合的 session 对象

也就是：

- `xtlssession`
- `xrtNetTlsSession*`

这样做有几个直接好处：

- 对外 public surface 只有一条
- 更容易和 `xnet_stream` 对接
- 更适合被 `xhttp / xhttpd / xws` 这些上层模块复用
- 不会把 TLS 核心内部状态直接暴露给外部代码


## 5. Session 模型的好处

把 TLS 看成 session，而不是零散函数集合，意味着：

- handshake 是 session 的状态推进
- cipher 输入/输出是 session 的缓冲协同
- plain read/write 也是 session 的职责
- resume / SNI / cert 设置都围绕 session 收口

这比“到处拿着内部 TLS 上下文指针做操作”更利于主线稳定。


## 6. 学这条线时，先学什么

建议按下面顺序学：

1. 先理解 `xnet-v2` 的角色
2. 再理解 `stream / dgram / wait-source`
3. 再理解 `xtlssession`
4. 最后再去看 `xhttp / xhttpd / xws`

不要一上来就直接跳到 HTTP 服务层，否则很容易看不清底层结构。


## 7. 推荐心智模型

可以先把这条线想成下面 3 层：

- 底层通信：`engine / stream / dgram`
- 安全层：`xtlssession`
- 应用层：`xhttp / xhttpd / xws`

然后等待与异步模型贯穿这 3 层：

- future
- wait-source
- coroutine wait

这就是为什么当前网络主线更适合长期演进：  
它不是一堆分散模块，而是一套能彼此配合的运行时体系。


## 8. 常见误区

### 误区一：TLS 只是网络层外面再包一层

不准确。

在当前主线里，TLS 是网络运行时主线的一部分，必须和 `stream`、wait-source、上层 HTTP / WebSocket 一起考虑。

### 误区二：HTTP 比 xnet-v2 更重要，先学 HTTP 就够了

不建议。

如果不理解 `xnet-v2`，后面会看不清：

- 为什么会有 stream wait-source
- 为什么 TLS 是 session
- 为什么协程/future 可以自然接到网络等待上

### 误区三：TLS 核心内部接口就是推荐 public API

不对。

当前正式 public 主线是 session，而不是内部核心上下文。


## 9. 建议继续阅读

- [XNet V2 API](../api/api-xnet-v2.md)
- [Network TLS API](../api/api-network-tls.md)
- [XHTTP API](../api/api-xhttp.md)
- [XHTTPD API](../api/api-xhttpd.md)
- [XWS API](../api/api-xws.md)

---

**一句话总结：** 在当前 XRT 主线里，`xnet-v2` 负责统一网络底座，`xtlssession` 负责统一 TLS public surface，HTTP / WebSocket 等上层能力都建立在这条单一网络主线之上。
