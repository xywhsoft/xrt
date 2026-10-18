# xrudp

基于同一份 XRT 的可靠 UDP 扩展；库名固定为 xrudp，公共符号前缀为 xrtRudp。
xs 的 xrudp 服务 class 为独立能力；mdo 直接在进程内使用本库创建监听与打洞端点，
不要求通过 xs 跨服务通信。
运行时不增加第三方依赖。源码、模块清单、单头产物按 XRT 的统一扩展流程管理。

当前为开发中的基础设施，已能通过原生 UDP 建立已认证连接并可靠传输多条双向字节流。
会话核心和原生端点已有固定预算的多流接口；消息、游戏和公网路径管理仍待开发，
尚未通过生产发布验收。
已实现组件：固定格式报文编解码、完整负载结构预校验、有界防重放窗口、
基于已认证方向密钥的包加密与密钥更新、固定 Noise IK 握手、无状态地址 cookie。
线上握手配置已组合 cookie、连接 ID、应用域、传输参数、有限缓存重传和最终 DATA 持钥证明；
原生端点已实现监听、每地址/总握手限速、半开/总连接/逻辑内存配额与有限关闭。
接收侧 ACK 历史、规范帧生成与解析、RTT、发送记录与包/时间丢失判定已实现；
发送侧 NewReno 与有界 pacing 已实现；多流共用包号、ACK、连接额度及拥塞窗口，按受限权重调度。
可靠流两半已有固定容量缓冲、重组、显式流额度、选择性确认/重传和 FIN/RESET 核算。
原生端点已接入流读写、OPEN、FIN/RESET、退休、进度事件和连接取消；
不能将当前组件直接作为已通过完整生产验收的产品。

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
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_transport_tests --no-examples
    python tools/amalgamate.py --manifest extlibs/xrudp/config/modules.json

发布前还须使用优化编译保持严格警告门禁，例如：

    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_endpoint,rudp_endpoint_fault_tests,rudp_multi_tests,rudp_multi_fault_tests --no-examples --cflag=-O2 --jobs 2

Windows 另外组合 rudp_endpoint_iocp_tests，Linux 组合 rudp_endpoint_epoll_tests；
Linux 的优化构建同时运行 ASan/UBSan。无优化构建通过不能代替发布优化编译检查。

模块 rudp_wire 只依赖 core；rudp_crypto 增加 XRT ChaCha20-Poly1305。
模块 rudp_noise 增加 X25519、HKDF-SHA256 与系统安全随机源。
模块 rudp_handshake 提供固定协议的完整握手状态机；不自行创建套接字或执行应用请求。
模块 rudp_endpoint 在调用方现有 XRT Engine Worker 上创建 IPv4 或 IPv6 UDP 端点，
管理握手缓存、来源固定、密钥晋升及定时器/引用生命周期，不创建私有 Engine 或线程。
配置使用 Session.Local 公布参数；通过 Session 设置固定环、包记录、拥塞与截止策略。
监听/拨号在握手前预留握手和完整数据会话的峰值预算，最终证明排队前完成会话构造；
Confirmed 后可 PeerWrite/PeerRead/PeerFinish，Changed 提供容量、可读、EOF 和 FIN 确认提示。
Changed 是边缘通知，读取应排空到 BACKPRESSURE/EOF/RESET；回调内变更的后续提示延后交付。
PeerStream* 操作额外字节流，StreamChanged 以相同接口报告全部流（含 ID 1）的提示；
每次派发预先提取最多 8 条合并事件，回调中新建的提示至少延后 1ms，关闭后停止其余回调。
RETIRE_READY 提示两半结束且旧包引用已排空，STREAMS_READY 提示对端增加开流额度；
回收本地流槽或收到新额度后可重试 Open，不需要用定时轮询等待流资源。
多连接共用有界 UDP 输出队列。成功受理后轮转起点，背压失败保留等待顺序；
收到新输入也先给先前受阻的连接一次重试机会，防止高频回包绕开维护轮次抢占额度。
无可发送内容时撤去等待登记，关闭连接同时移除登记；不绕过各连接的拥塞/pacing 和有限截止。
先请求 EndpointClose 并等待终结回调，再 Stop/Destroy Engine；关闭后的外部句柄可晚于 Engine 释放。
模块 rudp_cookie 只增加 HMAC-SHA256，不需要分配或公钥运算。
模块 rudp_ack 提供固定上限的接收确认历史，依赖 rudp_wire，无堆分配。
模块 rudp_timing 提供 RTT、PTO 退避和丢失时间阈值；时间由外部注入，不访问全局时钟。
模块 rudp_sent 提供有界发送记录、合法 ACK 校验、丢失事件与取消，固定分配后不再分配。
模块 rudp_congestion 提供 NewReno 与两数据报突发上限的 pacing；共享所有在途业务的预算。
模块 rudp_receive 提供固定数据环/位图重组，不为乱序片段另行分配元数据。
同包多个 STREAM 帧可整体预检和原子提交，连接累计偏移按各流最高值计费一次。
模块 rudp_transmit 保留未确认内容，以分层位图查找待发范围，重传副本共享逻辑确认状态。
模块 rudp_session 是单所有者的双向多字节流会话，消费握手晋升后的唯一密码器，
组合整包 ACK/STREAM/RESET/额度预检、实际输出受理后的发送提交、恢复/流控/换钥与有限截止。
固定数据环、引用和事件空间预先分配，Poll 最多输出 8 包，通过回调复制交给 UDP；
不自行创建套接字、线程或 Engine，由 rudp_endpoint 驱动并映射字节流的业务读写接口。
StreamCapacity=N 预留 2*N-1 对固定环（默认 N=1），OPEN/读写/FIN/RESET/退休均不新增分配。
额外流使用单调奇偶 ID；退休且所有包副本排空后复用存储，不复用 ID。
SessionStreamNextEvent 提供远端开流、读写及退休边缘提示；原生 peer 将其映射到 StreamChanged。
测试套件 rudp_transport_tests 组合加密、恢复、拥塞与双向字节流，注入确定性网络故障；
其中的测试驱动不作为产品连接端点导出。
源入口 include/xrudp.h；生成产物在
single/extlibs/xrudp.h 与 single/extlibs/xrudp_decl.h。

协议草案见 docs/protocol.md；生产验收与跨仓库进度见 docs/implementation-status.md。
