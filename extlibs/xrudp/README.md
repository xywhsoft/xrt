# xrudp

基于同一份 XRT 的可靠 UDP 扩展；库名固定为 xrudp，公共符号前缀为 xrtRudp。
xs 的 xrudp 服务 class 为独立能力；mdo 直接在进程内使用本库创建监听与打洞端点，
不要求通过 xs 跨服务通信。
运行时不增加第三方依赖。源码、模块清单、单头产物按 XRT 的统一扩展流程管理。

当前为开发中的基础设施，尚不能建立网络连接，也尚未通过生产发布验收。
已实现组件：固定格式报文编解码、完整负载结构预校验、有界防重放窗口。
这些组件本身不提供认证；未经认证的输入不能提交到防重放状态或交给应用。

## 目标与边界

- 可靠有序字节流与可靠有序消息；不可靠有序消息与不可靠独立消息。
- 固定 Noise IK/X25519/ChaChaPoly/SHA256 安全配置，可信公钥绑定，关闭早期业务数据。
- ACK/RTT/丢包恢复、NewReno/pacing、连接及流流控、优先级和受限权重调度。
- 有界内存、重组片段、历史状态、握手、定时器、CPU 工作量与取消生命周期。
- IPv4/IPv6、路径 MTU、经认证并验证的新地址重绑定。
- xs 以 TCP 风格管理监听、连接、默认字节流及运行代；高级流/消息接口仍直接透传原生对象。
- 独立穿透模块对接已有账号协调通道；mdo 进程内监听优先双机直连，服务器中继兜底。

不实现跨协议大版本兼容、降级或转换；同一固定协议的不同平台与独立实现必须互通。
应用层保留账号权限、断点、业务收据和游戏同步算法。传输 ACK 不表示业务成功。

## 构建

在 XRT 仓库根目录使用统一工具：

    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_wire --no-examples
    python tools/amalgamate.py --manifest extlibs/xrudp/config/modules.json

当前模块 rudp_wire 只依赖 core。源入口 include/xrudp.h；生成产物在
single/extlibs/xrudp.h 与 single/extlibs/xrudp_decl.h。

协议草案见 docs/protocol.md；生产验收与跨仓库进度见 docs/implementation-status.md。
