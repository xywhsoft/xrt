# TLS 会话体系设计

## 目标

TLS 会话层负责把已经稳定的记录、握手消息、协商、密钥交换、密钥调度和 X.509 原语组合成可持续驱动的客户端与服务端。它必须同时满足以下要求：

- 保留旧版传输无关会话的优点，内存测试、TCP、代理和未来其他可靠字节流使用同一状态机。
- 客户端和服务端提供分开的创建入口与策略配置，不要求应用混用低级状态。
- 不在每个连接中预留固定收发缓冲，也不无条件保存所有命名组的密钥。
- 常见路径只需创建、驱动、读写和关闭；高级用户仍可控制记录搬运、身份选择、验证、恢复和内存池。
- 协议状态、传输状态和应用回调边界清晰，任何错误都能映射到结构化 `xerror` 和确定的 TLS Alert。

旧版 `lib/nettls.h` 与 `test/test_tls_boundary.h` 是本层的主要资产。旧测试已经证明了任意小分片握手、双向应用数据和 `close_notify` 泵送模型；新实现必须迁移并增强这些边界，而不是只保留相似的函数名。

## 分层

| 模块 | 职责 | 主要依赖 |
| --- | --- | --- |
| `tls_session` | 公共生命周期、状态、受限队列、记录分派、Alert 和关闭 | `tls`、`tls_record`、`net_buffer` |
| `tls_client` | ClientHello、HRR、服务端认证、Finished 和客户端后握手状态 | `tls_session`、Hello、消息、协商、密钥交换、调度 |
| `tls_server` | ClientHello 策略、SNI/ALPN、身份选择、服务端 flight 和客户端认证 | `tls_session`、Hello、消息、协商、密钥交换、调度 |
| `tls_verify` | 证书链、主机名、用途、时间与应用信任回调 | `tls_negotiate`、`x509` |
| `tls_resume` | TLS 1.3 ticket/PSK 的不可变资产对象；后续单独组合客户端交接与显式缓存 | `tls`、时间 |
| `tls_stream` | 在 TCP Stream 与客户端/服务端会话之间搬运数据，传播背压、超时和认证关闭 | `tcp`、`tls_client`、`tls_server` |
| `tls_stream_dial` | 组合 Resolver、TCP Dial 与客户端组合流，完成主机名到安全 Stream 的一次发布 | `tls_stream`、`net_tcp_dial` |

底层 TLS 协议原语不反向依赖网络。组合会话依赖公开的 `net_buffer`，因为它确实需要有界、自适应、可池化和多 span 的字节队列；这里不再复制一套 TLS 私有动态数组，也不引入函数表伪装解耦。

## 对象与线程

`xtlscontext` 当前是共享协议策略快照，只拥有深复制后的版本、套件、组和签名偏好。`xtlsidentity` 与 `xtlsverifier` 已经作为独立共享对象实现；前者深复制证书链并拥有签名器，后者深复制可选 trust store 并拥有自定义回调上下文。证书或信任轮换通过创建新对象和新角色配置快照完成，不在活动对象中原地修改。上下文、身份和验证器都可跨线程共享，并使用引用计数延长到最后一个会话结束。

`xtlssession` 是单连接对象，借用一个上下文引用。一个时刻只能由一个线程或 Worker 驱动；会话内部不加锁。网络层跨线程操作必须先投递到所属 Worker，这与 TCP Stream 的线程契约一致，避免旧版每次 API 调用都获取自旋锁。

客户端和服务端使用不同入口创建会话。公共 `xtlsstate` 只暴露 `NEW`、`HANDSHAKE`、`READY`、`CLOSING`、`CLOSED`、`FAILED`，内部握手步骤不进入 ABI。状态只能单向推进，`FAILED` 和 `CLOSED` 是终态。

## 内存与背压

会话创建只分配状态本体和实际启用的角色状态，不预分配 8 KiB 收发缓冲。cipher 输入、cipher 输出和未消费明文使用三个 `xnetbuf`：

- 第一次收到或生成数据时才从调用方指定的 `xnetbufpool` 取得合适块。
- 大记录和大握手消息按需使用更大块；消费后按池策略回收，不永久抬高单连接基线。
- `FeedLimit`、`SendLimit`、`PlainLimit` 和 `HandshakeLimit` 都是硬上限，达到上限返回 `XTLS_AGAIN`，不继续隐藏分配。
- `xrtTlsSessionFeed()` 提供复制路径；引用和接管路径复用 `xrtNetBufAppendRef()` / `xrtNetBufAppendTake()` 的所有权契约，`xrtTlsSessionFeedBuffer()` 则直接移动完整块链。
- cipher 输出直接暴露只读 `xnetbuf` span，并由调用方明确消费，TCP 适配器可直接提交 scatter/gather 发送。

记录解密另使用一个惰性 `Scratch` 链，但它不是第四个常驻队列。只有完整受保护记录到达后才取得一个合适块；应用数据成功发布时，该块直接移动到明文队列，不复制负载；握手、Alert、认证失败和销毁路径会先安全擦除再归还池。明文队列背压期间，Scratch 与输入记录保持挂起，接收序列号不会再次递增。

密钥材料按协商结果分配或复用精确尺寸，不保存所有组。记录密钥只保留当前发送 epoch、当前接收 epoch 和协议切换期间确实需要的过渡 epoch；epoch 退休时立即清零。临时密钥、共享秘密、派生 secret 和 ticket key 在释放前全部安全擦除。

## 驱动契约

会话不直接调用 socket：

1. 调用方把收到的密文交给 `Feed`，TCP 适配器可用 `FeedBuffer` 直接接管 Read 缓冲。
2. `xrtTlsDrive()` 在不阻塞的前提下处理尽可能多的完整记录和握手消息。
3. 调用方从 cipher 输出队列发送数据，并按实际发送字节消费。
4. 应用通过 `Read` 取明文，通过 `Write` 生成受保护记录。

`XTLS_OK` 表示本次取得了可观察进展，`XTLS_AGAIN` 表示需要更多输入、输出空间或应用消费，`XTLS_CLOSED` 表示已完成受认证关闭，`XTLS_ERROR` 表示进入失败终态。会话额外公开等待原因位，网络适配器据此区分需要读、需要写、需要身份回调完成或需要应用消费，避免忙轮询。

同一次 `Drive` 有明确工作预算，限制处理的记录数和握手消息数。高吞吐连接可以连续调用，事件循环中单个连接不能无限占用 Worker。

## 记录、关闭与错误

- 握手阶段严格检查每个状态允许的消息、记录类型和加密 epoch；低级 parser 接受未知值不代表状态机会接受其位置。
- TLS 1.3 兼容性 CCS 只在标准允许的位置接受，格式必须是单字节 `1`。
- 收到 fatal Alert 后保存对端级别和描述，进入 `FAILED`；收到 `close_notify` 后停止接收新应用数据并进入受认证关闭流程。
- 本端 graceful close 只排队一次 `close_notify`，允许发送队列排空并等待对端关闭；abort 不生成 Alert。
- 底层 EOF 未伴随 `close_notify` 默认是截断错误。需要兼容历史对端时只能通过显式策略放宽，且状态不伪装成已认证关闭。
- 每个失败记录 `operation`、公开状态、内部握手步骤、记录/握手类型、输入偏移和底层 cause。生成 fatal Alert 失败不能覆盖最初原因。
- 对错误输入的解析、验证和密码操作保持失败原子性；只有完整步骤成功后才发布新状态、密钥 epoch 或协商结果。

## 身份、验证与回调

服务端身份选择发生在严格解析 ClientHello 和完成 SNI/ALPN 提取之后、生成 ServerHello 之前。回调接收借用视图并返回共享身份句柄，不直接改写会话内部字段。默认选择器覆盖单身份场景，SNI 多身份和外部证书管理使用同一扩展点。

客户端验证器默认验证链、时间、服务端用途和主机名；系统 trust store 的加载属于独立可裁剪组合层，不在核心验证调用中隐式执行。自定义验证回调接收已经解析的稳定链，可以接受、拒绝、回退到默认 store 或报告带 cause 的错误，但不能绕过协议签名与 Finished 校验。默认验证成功后，附加策略回调借用实际选中的叶到根路径和独立 trust anchor，可以直接组合已经公开的 CRL policy，也为 OCSP、CT、证书固定和应用规则保留同一层入口；核心本身不执行联网或文件加载。

回调在会话无内部锁时执行。回调期间禁止递归驱动同一会话；当前验证回调是同步决策，异步验证的挂起句柄、完成 API 和 `XTLS_WAIT_VERIFY` 恢复状态仍属于后续实现，不能从现有枚举推断已经可用。

## 客户端与服务端手感

公共 API 最终提供三层入口：

1. 原语层：现有 record、Hello、消息、协商、密钥交换、调度和 X.509 API。
2. 会话层：客户端/服务端会话、显式 cipher 搬运、明文读写、关闭和恢复。
3. 便捷层：TCP 客户端/服务端直接配置 TLS 上下文，连接打开事件只在 TLS `READY` 后发布。

便捷层只能组合会话层，不维护第二套握手实现。高级用户可以绕过 TCP 适配器，但不能绕过会话状态机后再声称获得同等协议保证。

## 迁移门禁

- 把旧 `test_tls_boundary.h` 的随机 1 到 53 字节分片、32 轮握手、双向数据和关闭迁移为无外部 socket 的确定性测试。
- 增加每个字节切分、同记录多消息、跨记录消息、队列上限、慢发送方、慢读取方、半关闭、无 `close_notify` EOF 和 fatal Alert 测试。
- TLS 1.2 与 TLS 1.3、四个命名组、所有启用 AEAD、RSA/ECDSA/Ed25519 身份和 ALPN/SNI 分别覆盖。
- 验证密码失败、证书失败、OOM、输出背压和回调暂停时状态与输出不变，错误 cause 不丢失。
- 测量空闲会话本体、首次握手峰值、稳定应用数据阶段和消费后回落；禁止用固定大缓冲换取测试通过。
- 主机名入口必须覆盖双栈回退、解析/连接/握手取消、全过程超时、Timer 拒绝、Open-before-Done、统计保留和失败不发布半成品。
- GCC、TinyCC、x86、单头文件、客户端裁剪、服务端裁剪、无 X.509 自定义认证裁剪和 TCP 适配裁剪分别通过。

## 当前落地状态

公共会话底座已经拆到 `<xrt/tls_session.h>`，并以 `XRT_FEATURE_TLS_SESSION` 独立裁剪。它依赖共享上下文、记录层、公开网络缓冲与安全擦除原语，但不依赖 TCP、Engine 或任何 socket API。客户端和服务端构造器继续保持分离，公共底座只提供稳定的生命周期、查询、输入所有权、密文输出和明文读取契约。

当前对象使用三个惰性持久队列和一个惰性瞬时 Scratch，会话创建时不分配任何固定 8 KiB 缓冲。硬上限在取得所有权或分配前检查；正常背压返回 `XTLS_AGAIN` 且不修改错误槽。密文发送与明文读取同时提供单 Span、Span 向量和严格消费路径，TCP 适配器可以直接使用 scatter/gather，而常规调用方可以使用复制式 `Read`。

公共会话底座已经能够独立切换收发 record epoch、排队初始明文记录与受保护记录、从任意分片输入中稳定挂起一条完整记录，并把解密后的应用块零拷贝发布到明文队列。TLS 1.2 与 TLS 1.3 使用同一记录组合路径；密钥已安装后的兼容 CCS 仍按明文记录处理。输出背压、明文背压、认证失败和 Scratch OOM 都保持输入、所有权与序列号原子；client/server 状态机仍负责决定当前位置允许的记录类型以及何时切换 epoch。

旧版 `lib/nettls.h` 的会话壳、密文喂入、发送队列与明文读取语义已经迁入本层；旧 `test/test_tls_boundary.h` 的随机小分片完整握手、双向应用数据和 `close_notify` 已由真实客户端/服务端集成测试迁移，并进一步覆盖输出背压、目标 OOM、KeyUpdate、fatal Alert 和恢复连接。

TCP 组合层已经落地到 `<xrt/tls_stream.h>` 与 `src/tls/stream.c`。客户端 Connect 和服务端 Accept 共用一个适配状态机，握手完成前不发布应用 Open；TCP 输入优先整体移动到 Feed，TLS 输出整体移动到 TCP，只有 Feed 剩余空间小于当前 TCP 前部 Span 时才执行有界前缀复制。未消费明文暂停 TCP，新消费恢复驱动；TLS 成功短写、TCP 高低水位、两级 Drain、握手 Timer、认证关闭 Timer、截断 EOF 和第一个结构化根因都由组合层统一发布。

组合层为同步低水位回调设置独立发送重入门。TCP 接管密文时即使立即完成发送，也只能把 `Writable`/`Drain` 延迟为内部 Worker 命令，在外层 `xrtTlsStreamSend()` 或 `xrtTlsStreamSendVec()` 返回后发布，不能让应用递归写回正在移动的队列或在 `iWritten` 提交前重发同一前缀。向量入口先验证全部 Span 与长度溢出，再逐片写记录、统一冲刷密文，不用连续临时副本换取便利 API。真实证书回环测试使用 32 KiB 双层硬上限强制 256 KiB 明文短写，并覆盖 select、IOCP、TCC x86、延迟消费者和不回应 `close_notify` 的原始会话对端；故障测试拒绝同步组合对象分配并耗尽 Worker Timer 槽位，验证根因、状态和引用回滚。Engine Timer 扩容 OOM 由网络底座的独立故障测试负责，避免用共享 heap span 尺寸制造不可靠的定向证据。

主机名客户端已经落地为独立 `tls_stream_dial`，而不是重新把 DNS、代理和 TLS 塞回 TCP Stream。它直接组合公开 Resolver、`xrtNetDial()` 和 TLS Stream：TCP Dial 负责候选并发与回退，TLS Stream 负责握手与认证关闭，可选总 Timer 只负责覆盖 DNS、TCP 与 TLS 的完整截止时间。操作在 TLS READY 前独占组合对象，成功按用户 Stream `Open`、Dial `Done` 的顺序一次转移调用方引用；解析、TCP 或握手失败只从 Dial 发布，不暴露半初始化 Stream。旧版 `lib/xnet_stream.h` 中“主机解析、连接总超时、TLS 挂接”的有效契约因此被保留，但混在同一函数中的代理状态机仍属于后续独立组合层，不能计入当前完成范围。

客户端首航已经落地到独立 `<xrt/tls_client.h>`、`src/tls/client.c` 和 `src/tls/client_handshake.c`。构造器深复制 SNI 与 ALPN，按当前裁剪构建的记录、调度和密钥交换后端过滤上下文策略，只生成一份首选组 key share，并在一次精确角色尾部分配内保留实际线路 offer、ClientHello 和后续所需的精确摘要容量。初始记录直接进入公共有界 Send 队列，不再使用旧版 1024 字节栈缓冲，也不会预先生成所有组密钥。

公开 `xrtTlsClientDrive()` 已经支持普通 TLS 1.3 ServerHello 跨记录重组、严格线路 offer 校验、兼容 CCS、transcript 初始化、ECDHE 握手调度和双向 record epoch 原子切换。派生过程先写入栈上临时结果，只有协商、密码和记录完成全部成功才提交；客户端临时私钥在成功切换后立即擦除。测试独立复算全部握手秘密，并使用反向服务端密钥生成真实受保护记录验证读写方向，而不是只检查状态值。

TLS 1.3 HelloRetryRequest 使用同一客户端和服务端状态机。客户端只接受一次 HRR，要求回显 session ID、选择已提供但尚未发送 key share 的可用组，并拒绝未知或禁止扩展；服务端保留首个 ClientHello，只允许第二个 ClientHello 改变 RFC 8446 明确允许变化的 key_share、cookie、PSK age 和 binder 字段。双方都以 synthetic `message_hash` 重建 transcript，第二个 ClientHello 的恢复 binder 也以该前缀计算。当前内置服务端发送无 cookie HRR；底层 Cookie 解析和写出 API 已公开，应用可以在自定义协议层构建带 Cookie 的线路消息。

客户端随后严格处理受保护的 TLS 1.3 EncryptedExtensions。扩展必须既允许出现在该消息中，又确实由客户端发出；ALPN 只发布客户端尾部快照中的精确匹配项，因此查询结果不借用输入记录。握手 reader 保留跨记录分片，角色状态额外保存同记录偏移，使 EE 与 Certificate 聚合时只提交第一条完整消息。验证失败不会提交 transcript、ALPN 或下一握手步骤。

客户端认证已经形成从 Certificate 到 Finished 的初始 TLS 1.3 闭环。对端链先精确测量，再以一次分配保存证书视图和独立 DER，不继承旧版固定证书张数；默认 verifier 使用不可变 trust-store 快照验证路径、时间、用途和主机名，自定义回调只能替换信任决策。CertificateVerify 使用加入当前消息之前的 transcript 摘要，严格匹配真实 offer 和证书密钥，并通过公共 X.509 密码分派完成 RSA-PSS、ECDSA 或 Ed25519 验签。

旧网络 TLS 壳只公开叶证书 DER 指针与长度，新客户端状态机已经为验证和 CertificateVerify 深复制并解析整条链。公共查询直接借用这份唯一资产，以数量和 `xx509cert` 索引视图替代重复 DER 包装；完整握手视图稳定到会话销毁，PSK 恢复如实报告零张本次线路证书。

服务端 Finished 使用加入该消息之前的 transcript 校验。状态机在临时对象中纳入服务端 Finished、派生主秘密和双向应用流量秘密、构造客户端 Finished、初始化新 record epoch，并先以旧握手写 epoch 把客户端 Finished 完整排队；只有这些步骤全部成功才一次发布 transcript、应用 epoch 和 READY。SendLimit 不足会在握手 reader 消费输入前返回背压，重试不会重复解密、递增序号或污染 transcript。客户端 Finished 排队后清除握手秘密，保留应用流量秘密供后续 KeyUpdate 使用。

READY 状态继续使用同一非阻塞驱动器处理应用记录、Alert、NewSessionTicket 和 KeyUpdate。应用记录解密后直接把 Scratch 链移入 Plain；明文硬上限暂停在挂起记录，应用消费后继续完成，不重复解密。认证关闭自动应答一次 `close_notify`，本地读侧可以继续等待对端关闭，收到关闭后到达的应用数据只消费不发布。

KeyUpdate 保留当前应用流量秘密作为代际根。收到不请求应答的更新只替换读取 epoch；收到请求型更新时，响应先以旧写 epoch 原子入队，然后同时替换读取和写入 epoch。公开 `xrtTlsClientKeyUpdate()` 对主动消息执行相同的“旧 epoch 入队、新 epoch 提交”顺序。两条路径都在栈上临时派生下一代 secret/key，发送背压、参数错误或分配失败不会发布部分状态。KeyUpdate 必须独占记录，跨记录分片和同记录拼接均按协议错误拒绝。

NewSessionTicket 已接入后握手 Reader并执行公共严格 parser。独立 `tls_resume` 已实现一次分配的不可变 ticket/PSK 快照、版本/套件/SNI/ALPN/对端身份域绑定、墙钟有效期、票据年龄、原子引用和整块敏感内存清零。它不反向依赖会话、客户端或缓存，所以低层用户可以直接构造和保存恢复资产。

客户端票据发布层已经实现：包含客户端 Finished 的 transcript 派生 resumption master secret，每张 ticket 再按 nonce 派生独立 PSK，并通过容量 0 到 64 的有界环显式交接 `xtlsresume`。环槽位进入客户端角色的单块尾部分配，不产生队列节点；满载时淘汰最旧票据，SNI、实际协商 ALPN 和已验证叶证书摘要随对象一起深拷贝。缓存继续由调用方拥有，不继承旧版 `lib/nettls.h` 的进程全局自旋锁链表。旧版 TLS 1.2 恢复实现提供的身份/SNI 绑定、显式导出、恢复查询和敏感材料清零继续作为契约资产，但旧 master-secret 对象和全局缓存实现不会直接迁移。

下一连接恢复也已经进入同一客户端状态机。配置只借用一张恢复对象并在成功创建后持有引用；省略的 SNI/ALPN 从对象推导，显式路由必须精确匹配。ClientHello 保持正常 X25519 key share，只提供 `psk_dhe_ke` 和一个末尾 PSK identity，并从最终线路编码计算 binder；因此当前只有 PSK+DHE，没有纯 PSK 和 0-RTT。服务端未选择 PSK 时继续证书航班，选择 identity `0` 时严格检查套件摘要、key share 与票据 ALPN，再跳过 Certificate/CertificateVerify 直接完成 Finished。恢复成功后的新票据继承旧对象的已认证对端身份，不重新把“恢复”误当作匿名认证。

票据缓存是 READY 连接的附属能力。对象分配 OOM 只增加 `ResumeDropped` 并清理临时错误，不使已经可用的连接进入失败状态；解析、派生、协议字段或状态错误仍然终止会话。该边界使用超过堆池化阈值的大票据定向注入后端 OOM，避免测试依赖尺寸类缓存和编译器偶然分配顺序。

服务端状态机已经落地到独立 `<xrt/tls_server.h>`、`src/tls/server.c` 与 `src/tls/server_handshake.c`。它在严格解析完整 ClientHello 后才执行同步身份选择，深复制 SNI 与 ALPN 配置，使用公共协商、writer、身份签名、transcript 和记录保护原语构造 TLS 1.3 证书航班。整个首航先在临时对象中完成选择、ECDHE、密钥调度、签名、精确编码和密文保护，只有完整输出被有界 Send 队列接管后才一次提交身份、路由、transcript 与握手 epoch。

服务端恢复继续组合同一状态机，而不是另建握手实现。外部查找回调只负责把不透明 ticket 映射到不可变恢复对象；核心严格验证有效期、票据年龄、套件摘要、SNI、ALPN、`psk_dhe_ke`、key share、末尾 PSK 扩展与 binder。缓存未命中或绑定不匹配回退证书航班，匹配资产的坏 binder 必须失败。READY 后票据签发从最终 transcript 派生 resumption master 与每票据 PSK，发送预检位于随机数、派生和分配之前，成功后由调用方同时接管缓存对象。

本端协议失败现在由公共会话层唯一映射并尽力排队 fatal Alert。握手 epoch 尚未建立时发送明文 Alert，建立后使用当前写 epoch；Alert 的 OOM 或发送上限失败不会覆盖原始 `xerror`。角色状态机只设置根错误并进入共享失败路径，避免客户端和服务端重复维护 Alert、序列号及队列原子性。

当前门禁覆盖 ServerHello 缺失扩展、错误 session ID、未 offer 套件、错误组、单次 HRR、非法或重复 HRR、第二个 ClientHello 稳定字段、非法 CCS、意外记录、分片 reader OOM、EE 加密 epoch、跨记录 EE、聚合消息后缀、未请求或禁止扩展、ALPN 稳定生命周期、证书深复制、空链、未请求 CertificateEntry 扩展、缺失 verifier、坏签名、未 offer 签名方案、精确 transcript、服务端/客户端 Finished、双向应用 epoch、认证关闭、主动与被动 KeyUpdate、旧/新 epoch 顺序、KeyUpdate 输出背压和记录边界、票据分片/聚合/畸形正文、有界淘汰、ClientHello binder、客户端与服务端 PSK 接受和安全回退、错误 identity、缺失 key share、恢复 SNI/ALPN/年龄绑定、坏 binder、PlainLimit 恢复、明文与受保护 fatal Alert、目标 OOM、默认路径后的 CRL 策略组合、TCP 组合层的 select/IOCP、短写恢复与回调非重入、慢消费者、截断与超时、组合对象 OOM、Timer 调度拒绝、真实 TLS 1.3 重复恢复、主机名双栈回退、解析期取消、全过程超时、TLS Dial Timer 拒绝、GCC、TCC x86、单头和直接依赖裁剪。HRR 真实双端测试使用客户端首发 X25519、服务端偏好 P-256 的策略完成 synthetic `message_hash`、第二份 key share、证书握手、应用数据和认证关闭。CertificateRequest、客户端认证、0-RTT、自动吊销数据获取、异步验证和异步身份选择仍不得标记完成。

## 身份层

`xtlsidentity` 把不可变证书链与签名能力组合成独立共享对象，不依赖 TCP、Engine、客户端或服务端状态机。公共核心深复制全部 DER 证书，严格解析叶 SPKI，并以一块紧凑分配保存对象、证书视图、证书内容和内置私钥尾部；最后一个引用释放前清除整块存储。

内置后端按算法独立裁剪：RSA、P-256、P-384 和 Ed25519。EC 的 SEC1/PKCS#8 解析、TLS 版本兼容规则和线路摘要选择都只实现一份共享底座，两个曲线后端只保留各自公钥派生与签名调用。TLS 1.2 保留 ECDSA 与 SHA-256/384/512 的合法交叉组合，TLS 1.3 才强制方案曲线匹配。RSA-PSS 同时解析证书 SPKI 与 PKCS#8 私钥限制，只有摘要、MGF1、盐长和 trailer 共同允许的 TLS 方案才会发布。外部签名回调只服务 HSM、系统密钥库和异步签名代理等真实扩展点，不承担内部模块之间的伪解耦。

旧 `lib/nettls.h` 的 RSA/EC/Ed25519 私钥格式识别和 `examples/example_tls_fixture.h` 的真实 RSA 资产被保留并增强。旧实现只保存 RSA 私有指数、按 EC 长度猜曲线且不派生公钥；新实现验证完整 RSA CRT、严格校验算法与曲线 OID，并要求私钥派生公钥同时匹配 SEC1 可选公钥和叶证书。
