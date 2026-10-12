# xws-group WebSocket API

持有成员引用、生成稳定快照、提交有界广播并报告逐连接结果。分组不管理监听、HTTP、鉴权、房间、业务消息或持久化。

本库模块与构建方式见 [README](../../README.md)。逐接口声明、参数和所有权契约见 [API 参考](reference.md)。

连接和快照均按引用计数管理生命周期。预算耗尽返回背压，由调用方等待 Writable、Drain 或对应 Future 后重试。
