# HTTP/1 运行时分层

职责、依赖与迁移见 [HTTP 分层](http-layers.md)。xhttp 保留无 IO 的消息对象和 Exchange；连接、Listener、Timer 由 xhttp-client/xhttp-server 持有，Cookie/缓存/认证会话和决策由 xhttp-policy 持有。

结构化 Reply、准备好的响应计划和原始报文输出继续使用同一响应接纳与引用转移契约。网络 Runtime 决定暂停/恢复、排空、取消和关闭；消息层只处理交付的字节和输出计划。

Upgrade 的传输所有权仅在成功 Take/Detach 时转移，剩余接收字节保留。Engine 的所有者不因扩展构建而变化。具体生命周期见 [客户端](../../../xhttp-client/docs/api/http_client.md) 与 [服务器](../../../xhttp-server/docs/api/http_server.md)。
