# HTTP/1 运行时分层

## 边界

HTTP/1 运行时只负责把 xhttp 的协议对象绑定到 XRT TCP/TLS 传输。协议解析、响应编码、正文背压和连接复用语义由底层对象保持一致；路由、中间件、静态文件、缓存与重试属于独立策略模块，不进入运行时核心。

## 服务器路径

`xrtHttpServerStart` 与 `xrtHttpServerStartTls` 建立监听器，每个连接固定归属于一个网络 Worker。请求头、流式正文、完整请求、错误和关闭回调都在该 Worker 串行发布，应用不需要为单连接协议状态额外加锁。

响应可以直接用 `xrtHttpConnWrite` 和 `xrtHttpConnWriteRef` 写入已编码字节，也可以提交 `xhttp1serverresponse`。直接写路径不会创建 Header、Cookie 或 JSON 对象，是框架和高负载服务的基础路径。

## 内存与背压

连接没有固定 8K 缓冲。读取、解析和发送复用 XRT 网络块池；`WriteSize` 仅限制一次借出和发送的窗口。TCP/TLS 的队列上限和 drain 事件形成硬背压，运行时不会建立第二套无界队列。

跨线程恢复正文使用连接内嵌的 `xnetpost`，不分配任务节点。投递失败会撤销门状态并释放投递引用，保留底层结构化错误。

## 生命周期

服务器状态单向经过 `RUNNING`、`DRAINING` 或 `ABORTING`，最终进入 `CLOSED`。优雅停止不再接受新连接，并等待当前 HTTP 连接结束；中止停止关闭全部传输。`Shutdown` 只在监听器、普通连接以及已接管 Upgrade 生命周期全部退出后发布一次。

TLS 配置在启动时快照化。Context 与 Identity 使用引用所有权，ALPN 字节深复制；调用方的选择器和恢复回调上下文必须保持到服务器完全关闭。
