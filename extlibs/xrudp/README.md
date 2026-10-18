# xrudp

基于同一份 XRT 的可靠 UDP 扩展；库名固定为 xrudp，公共符号前缀为 xrtRudp。
xs 的 xrudp 服务 class 为独立能力；mdo 直接在进程内使用本库创建监听与打洞端点，
不要求通过 xs 跨服务通信。
运行时不增加第三方依赖。源码、模块清单、单头产物按 XRT 的统一扩展流程管理。

当前为开发中的基础设施，已能通过原生 UDP 建立已认证连接并可靠传输多条双向字节流和消息流。
会话核心和原生端点已有固定预算的多流/消息/游戏数据报接口、地址验证/端口重绑定及路径 MTU 探测/回退；公网穿透仍待开发，
尚未通过生产发布验收。
已实现组件：固定格式报文编解码、完整负载结构预校验、有界防重放窗口、
基于已认证方向密钥的包加密与密钥更新、固定 Noise IK 握手、无状态地址 cookie。
线上握手配置已组合 cookie、连接 ID、应用域、传输参数、有限缓存重传和最终 DATA 持钥证明；
原生端点已实现监听、每地址/总握手限速、半开/总连接/逻辑内存配额与有限关闭。
接收侧 ACK 历史、规范帧生成与解析、RTT、发送记录与包/时间丢失判定已实现；
发送侧 NewReno 与有界 pacing 已实现；多流共用包号、ACK、连接额度及拥塞窗口，按受限权重调度。
可靠流两半已有固定容量缓冲、重组、显式流额度、选择性确认/重传和 FIN/RESET 核算。
原生端点已接入流读写、OPEN、FIN/RESET、退休、进度事件和连接取消；
SessionShutdown/PeerShutdown 已接入双方可靠数据排空、可靠关闭确认和有限驻留；
不能将当前组件直接作为已通过完整生产验收的产品。

`xrudpsessionconfig.KeepAliveUs` 可选择空闲保活，默认零关闭；设为 1 秒至 1 小时，
实际间隔不超过协商空闲期限的三分之一。空闲 PING 使用原有加密、拥塞/pacing、
包记录和 ACK，不增加应用消息或扩大探测保留槽。有待完成的业务、PTO 和优雅关闭
继续使用各自原有的驱动与有限期限；保活不刷新业务进展，也不能让失联连接永久存活。

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
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_session_message_tests,rudp_endpoint_message_tests --no-examples --cflag=-O2 --jobs 2
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_datagram_tests --no-examples --cflag=-O2 --jobs 2
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_game_tests,rudp_game_native_tests --no-examples --cflag=-O2 --jobs 2
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_stop_tests,rudp_wire --no-examples --cflag=-O2 --jobs 2
    python tools/build.py --manifest extlibs/xrudp/config/modules.json --suite rudp_shutdown_tests,rudp_wire --no-examples --cflag=-O2 --jobs 2

Windows 另外组合 rudp_endpoint_iocp_tests，Linux 组合 rudp_endpoint_epoll_tests；
Linux 的优化构建同时运行 ASan/UBSan。无优化构建通过不能代替发布优化编译检查。

模块 rudp_wire 只依赖 core；rudp_crypto 增加 XRT ChaCha20-Poly1305。
模块 rudp_stun 提供固定预算的 STUN Binding 编解码，复用 XRT CRC32，不需要密码握手依赖；
rudp_discovery 增加安全随机事务及无 I/O 的发现状态机。它们只产生映射候选，不认证设备。
DiscoveryInit 后反复 Next，只有 UDP 真正受理输出时才 Sent；背压重复取得相同请求，
不能消耗次数或续期。Input 核对配置的精确来源和事务，State 为安全可公开的结果/期限快照。
默认最多 7 次、500ms 起指数退避和有限末次等待；调用方可设置更早的绝对截止。
Cancel/成功/终结错误擦除内部事务与请求。只读 State 不包含事务随机值。
不跟随重定向，不处理凭据挑战；远端错误保留数值，不转存任意文本。
原生 EndpointDiscover/EndpointDiscoveryCancel 已接入现有 UDP 套接字与定时器，
无连接时也可发现；四个内置任务不占握手/peer 槽。每个任务恰好一次延迟回调，关闭先
结算发现再通知端点 Close；回调可释放自有引用、关闭、取消或新增任务，不递归调用。
同一队列按实际受理交替给 peer/发现优先机会，发现仍至多每 50ms 一包，局部错误不关闭 peer。
双机打洞协调仍待接入，不能把单独获得映射当成直连已建立。

`rudp_punch` 已提供打洞出站预热核心：可信应用给定至多八个目的地址，每个至多六份独立
的 STUN Binding indication，全任务至少 50ms 发包间隔、默认 8 秒绝对期限。
无输入回应、地址学习、端口扫描或认证旁路；硬路由错误只撤销该候选，本地背压不消耗次数。
Init 之外无需随机源或分配，完成只表示本机 UDP 受理。原生 EndpointPunch/PunchCancel 使用
同套接字、固定双任务槽，与映射发现共用全局 50ms 门并公平服务 peer，关闭前异步结算回调。
设备直连必须通过 Noise 公钥/域/上下文与账户权限验证，授权协调与候选优选仍待接入。
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
CipherInspect 可对未验证地址的探测先验密而不提交防重放、接收密钥或发送密钥确认；
它不是受理成功，不能据此交付数据或生成传输 ACK，之后仍须由 Open 完成真实提交。
SentDiscardPath 供验证后的路径切换排空旧在途：可靠引用返回 LOST，纯 ACK 返回 EXPIRED，
全局包号/确认水位保留且账本可继续发送。驱动须先处理这些唯一事件，再重建新路径拥塞/RTT。
路径挑战与切换已接入会话和原生端点；握手期间仍固定源地址，确认后自动隔离并验证候选地址。
EnablePaths 在首次会话 I/O 前绑定握手已验证地址与地址化发送回调，之后使用 PathInput；
原生端点自动启用，可信协调提供的其他地址可用 PeerProbePath 主动探测。
一条候选、随机 16 字节证明、3 秒内最多三次挑战、失败后 3 秒冷却；重复密文不重复购买
反放大额度。业务仍发往 active 地址，验证成功后才切换并从保守拥塞/RTT 初值恢复。
Changed 提供 PATH_CHANGED/PATH_FAILED，SessionState.Path 提供无秘密的计数、地址和期限；
验证失败不撤销有效原路径。地址验证只证明可达性；MTU 另行探测，完整穿透协调仍待实现。
新包号可以立即触发刚离开地址的全新挑战，切回仍须验证；旧包不能反复开启尝试或购买额度。
每秒 32 次未知地址验密拆为 DATA 等普通报文 24 次和认证 PATH 控制 8 次，业务洪峰不占用响应额度。
模块 rudp_sent 提供有界发送记录、合法 ACK 校验、丢失事件与取消，固定分配后不再分配。
模块 rudp_congestion 提供 NewReno 与两数据报突发上限的 pacing；共享所有在途业务的预算。
模块 rudp_receive 提供固定数据环/位图重组，不为乱序片段另行分配元数据。
同包多个 STREAM 帧可整体预检和原子提交，连接累计偏移按各流最高值计费一次。
ReceiveStop 已提供本地停止交付的基础状态：清除未读明文和位图，仍校验迟到 STREAM 的
额度/偏移/真实 FIN/RESET，不伪造最终偏移，不继续给该流授予额度；逻辑消费量供连接回收额度。
停止后不能在真实 FIN/RESET 前复用存储。SessionStop/PeerStop 操作默认流 1，StreamStop
操作额外字节/消息流；STOP_SENDING 可靠通知对端停止对应发送半流，反向与其他流继续工作。
停止请求、RESET 和正常数据共用拥塞、账本和有界恢复；只在 UDP 真正受理后提交控制发送，
对端 RESET 的最终偏移为实际最高已发送值，未发缓存清除但不消耗对端额度。
WRITE_STOPPED 提示对端取消发送；READ_DONE 提示真实接收终结，不能把取消当成正常 READ_EOF。
默认流通过 Changed，所有流通过 StreamChanged 交付这些提示；取消后的发送容量提示会撤去。
StopComplete 表示通知被 ACK 或真实终结已到达，仍须真实最终偏移与所有包引用排空才允许退休。
等待最终偏移另有独立的 ProgressTimeoutUs 期限；ACK、其他流进展和重复 Stop 都不能续期。
正常收到真实终结即撤去该期限；超时会关闭不响应的连接并清理资源。
发送 RESET 也有独立的 ProgressTimeoutUs 确认期限，重复请求/重发和其他流进展不续期，
收到有效 ACK 才撤去。State.PendingSendResets 与流的 ResetDeadlineUs 可查询等待状态。
SessionShutdown/PeerShutdown 启动 1 秒至 10 分钟的有界可靠排空，停止新写入并给已受理
发送半流排入 FIN，读取仍可继续。原生 SHUTDOWN_REQUESTED 提示对端请求；已有异步回复
写完后须显式 Shutdown，不会自动截断。State.Shutdown 可查询双方声明、排空/确认及期限。
最终 CLOSED 且 Shutdown.Succeeded 才表示本地已证明双方可靠数据排空；取消/错误不置成功。
关闭请求会取消排队游戏数据报，不能将其传输确认当成可靠交付。有限驻留保留最终 ACK
恢复能力，不因重复控制延长；原有空闲/进度和独立取消期限仍可更早终止未排空的连接。
需要优雅结束时先关闭各 peer、等待 PeerClose，再关闭端点；EndpointClose 保持立即取消语义。
模块 rudp_transmit 保留未确认内容，以分层位图查找待发范围，重传副本共享逻辑确认状态。
TransmitMessageWrite 可将 u32 长度前缀和一条负载整体排入现有发送环，空间不足时完整拒收。
模块 rudp_message 提供有界的消息接收组装器，构造时一次分配 0..1MiB 固定空间；
逐段接收只组装一条消息，应用未取走时背压，不交付半条消息，短输出缓冲不消费消息。
非法长度或不完整 FIN 清空明文并保留错误，支持空消息、RESET 和终结后的存储复用。
消息流已接入会话和原生端点：Bind 前设置 Session.Local.MaxMessageSize（默认 0 关闭，
非零为最高接收负载，最多 1MiB），握手认证双方上限；StreamOpen 的 Kind 设置为 MESSAGES。
默认流 1 保持字节流，其他流可混合 BYTES/MESSAGES；字节读写接口拒绝操作消息流。
PeerMessageWrite 整条入队，允许空消息；State.SendMessageLimit 给出对端上限与本端发送环的较小值。
PeerMessagePeek/Read 只提供完整负载或 EOF/RESET，短缓冲返回 TRUNCATED 及所需大小，消息仍保留。
原有 StreamChanged 的 READ_READY 表示一条完整消息或终结状态；成功读取空消息也会调度后续维护。
组装空间按 2*N-2 个额外槽在会话构造时预留，并计入 AllocationSize 和端点连接准入预算；
组装时逐段释放字节额度，每次 Input/Poll 共处理至多 4096B，轮转维护并由下一截止继续推进。
消息被应用取走前不退休流槽；FIN/RESET、包引用排空和累计开流额度规则与字节流一致。
模块 rudp_datagram 已实现固定内存的数据报队列：独立消息、按序号去旧和最新快照替换，
本地发送期限、接收存活时间、受限权重调度、不可回绕的序号/ticket 与取消明文清理。
通道 ID/模式由应用在两端约定，不根据未知输入分配；最多 64 通道、双向各 256 条记录。
未知/模式不符/过旧/队满的数据报按不可靠策略丢弃；看到较新序号后，队满也不允许旧包复活。
发送 Peek 不消费，只有实际 UDP 受理后才能以匹配 ticket 提交；本地背压不丢数据，
已提交数据报在丢包后不回队重发。该模块不自带网络、加密或拥塞驱动。
DATAGRAM 已接入会话和原生端点。Bind 前设置 Session.Datagrams（全零默认关闭），
固定队列空间计入 AllocationSize 和端点的连接准入峰值；Confirmed 后双方配置同名通道。
PeerDatagramWrite 入队完整消息，LifetimeUs 为本地相对期限，0 表示不设单条期限；
超过 State.DatagramSendLimit 返回 LIMIT，不分片。该上限为本地负载容量与双方报文上限减 88B 的较小值。
数据报共用连接的加密、包号、ACK、发送账本、NewReno 和 pacing，实际 UDP 受理后才消费队列。
拥塞需求包含等待游戏记录的线路开销和当前在途字节，可靠流额度耗尽也不阻止游戏窗口增长。
丢失只反馈拥塞和统计，不重发负载、不占可靠流字节额度，也不使用 PTO 探测保留位。
持续积压时两类业务交替获得整包机会；轮到的一类暂时放不下时，保留窗口空隙以防小包长期抢占。
纯 ACK 可继续发送但不消耗轮次，PTO 保持既有恢复优先级与硬预算。
整包预检成功后才接纳游戏负载；未知/关闭/不符/队满只丢弃数据报，不拒绝同包合法可靠内容。
Changed 提供 DATAGRAM_READ_READY/WRITE_READY；PeerDatagramPeek/Read 读取完整记录（含空消息）。
短缓冲返回 TRUNCATED 和所需信息且不消费。每次成功读取后若还有记录，下一轮（至少 1ms）继续提示，
应用可以单次有界读取；回调内取消或关闭停止后续交付。带时钟操作发现连接终结时立即关闭 peer，
单条发送过期不关闭连接；期限维护不更新对端活性，已过期工作也不会误触发有用进度超时。
模块 rudp_session 是单所有者的双向多流会话，消费握手晋升后的唯一密码器，
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

原生端点默认启用路径 MTU 探测，使用 XRT 的 UDP PMTU_PROBE 禁止 IP 分片。
低层 Session 的 DiscoverMtu 默认关闭；只有传输适配器能保证不分片时才启用。
State.Mtu 区分不可变接收上限、当前发送尺寸、探测和回退；握手成功本身不证明大包可达。
从 512B 起步，通过完整加密报文的 ACK 提升尺寸；较大尺寸持续失败时保留当前可用尺寸，
大报文独立停滞时退回 512B，可靠数据缩包重发，未发送的超限游戏记录清除并计数。
DatagramSendLimit 随当前路径变化，应用应按该值组织游戏负载，并处理 MTU_CHANGED 提示。
此功能不替代 NAT 发现、协调打洞、xs 服务类型或 mdo 内嵌直连/中继集成，完整生产验收仍在进行。
