# xws WebSocket API

接管已经开放的 TCP/TLS Stream，处理消息、控制帧、关闭、发送预算与异步操作。Writer、引用发送、Future、TLS 和压缩仍为可选模块，共享连接生命周期。

本库模块与构建方式见 [README](../../README.md)。逐接口声明、参数和所有权契约见 [API 参考](reference.md)。

连接和快照均按引用计数管理生命周期。预算耗尽返回背压，由调用方等待 Writable、Drain 或对应 Future 后重试。
