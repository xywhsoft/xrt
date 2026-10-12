# xws-client WebSocket API

构造和验证 HTTP 握手，协商子协议与压缩，成功后把连接交给 xws。HTTP 请求执行、代理和 TLS 由 xhttp-client 提供。

本库模块与构建方式见 [README](../../README.md)。逐接口声明、参数和所有权契约见 [API 参考](reference.md)。

升级成功前，HTTP 保持传输驱动权；成功时停止 HTTP 消费，再向 xws 转交一个拥有引用。输入缓冲中的未消费字节仍留在同一 Stream，由连接在下一次 Worker 循环处理。同步失败不转移传输所有权；异步失败按原 HTTP 完成契约回收。
