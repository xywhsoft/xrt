# xws-server WebSocket API

验证请求、发送 101、协商子协议与压缩，并把传输和已缓冲的早到帧交给 xws。监听、HTTP 请求和响应由 xhttp-server 提供。Router 是可选模块，直接 Upgrade 不依赖路由。

本库模块与构建方式见 [README](../../README.md)。逐接口声明、参数和所有权契约见 [API 参考](reference.md)。

升级成功前，HTTP 保持传输驱动权；成功时停止 HTTP 消费，再向 xws 转交一个拥有引用。输入缓冲中的未消费字节仍留在同一 Stream，由连接在下一次 Worker 循环处理。同步失败不转移传输所有权；异步失败按原 HTTP 完成契约回收。
