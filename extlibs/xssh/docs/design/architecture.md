# xssh 分层设计

## 目标

现代 `xssh` 只依赖 XRT 公共 API。它不复刻 socket、TLS、任务、取消、截止时间、随机数或
密码算法，也不隐藏每会话 Engine。历史单头只作为协议行为和测试向量来源，不再形成兼容
API。

迁移按以下层次推进：

1. `wire`：基础整数、string、name-list、mpint 与 identification。
2. `packet`：长度、padding、序列号、MAC/AEAD 边界和增量 framing。
3. `crypto/KEX`：算法清单、密钥交换、exchange hash 与 host key 验证。
4. `transport`：无缓冲 core、XRT TCP 动态链、rekey、断开与结构化错误。
5. `auth`：服务认证状态机与可扩展认证方法。
6. `channel`：流控窗口、exec/shell/subsystem、转发和多 channel 调度。
7. `session`：连接级协议组合、TCP 可靠提交以及后续同步/future/协程驱动。

每层公开足够的低层 API，高层 client 只组合这些能力。未知算法、认证方法和 channel request
应能通过公开 packet/transport 接口扩展，不能要求修改私有解析器。

## 当前收口

`ssh_wire` 已完成第一层，完全无分配，并独立裁剪到 XRT `core`。所有复合读取和写入都具有
事务语义，避免增量网络输入在错误或短包后发生状态错位。name-list 与 mpint 使用规范编码，
后续 KEX 不再各自复制列表扫描和大整数封装逻辑。

`ssh_packet` 把 plain framing、padding 注入和序列号保持在无密码底层；`ssh_packet_random` 只把
XRT 系统安全随机源适配为 padding 回调；`ssh_packet_aes_gcm` 只组合 XRT AES-GCM 与 packet。
这种分层允许协议测试使用确定性 padding，生产便利路径使用系统 CSPRNG，高吞吐 transport 使用
安全播种的会话级 PRNG，同时不复制任何密码实现。AES-GCM 直接在 writer 未提交区原位加密，
消除了历史实现的每包堆分配；解密缓冲由调用方复用，并通过认证后才发布视图和推进状态。

`ssh_packet_codec` 把 transport 必需的双向 mode、序列号和 cipher 生命周期组合起来，但仍不拥有
socket 或收发缓冲。NEWKEYS 可以分别切换读写方向；协商 strict-kex 后，初始 KEX 与后续每次
rekey 都在对应方向的 NEWKEYS 边界显式重置序列号。四字节长度头到达后即可探测完整线长和明文
工作区，运行时不再需要固定 8 KiB 数组。
系统 CSPRNG 便利写入位于独立 `ssh_packet_codec_random`，核心 codec 可由安全播种的会话 PRNG 驱动，
不会仅因状态编排而携带操作系统随机模块。

`ssh_kexinit` 保持纯协议：调用方显式提供 cookie 和 endpoint 角色，模块处理算法清单、首次/重协商
默认值、RFC 8308/strict-kex 标记、构建、解析、协商和 guessed packet 规则。扩展标记不会成为真实
算法，也不会泄漏到重协商清单。`ssh_kexinit_random` 是独立的生产便利层，只增加系统安全随机依赖。
这样 KEX 的消息格式与后续 X25519、exchange hash、host-key 验证保持分层，扩展算法不需要修改
KEXINIT 解析器。

ECDH init/reply 仍是纯 wire 层；SHA-256 transcript 流式调用 XRT 摘要，不再分配或拼接完整 hash
输入；Curve25519 确定性原语与安全随机临时密钥对分开裁剪。transport 将这些公开底层组合为一次
KEX，未来增加新的曲线或 hash 时不会改动 KEXINIT、packet 或网络层。

主机密钥同样拆成两层：`ssh_hostkey` 只公开算法无关的 key/signature blob 与 Ed25519 格式，
`ssh_hostkey_ed25519` 才引入 XRT 严格签名验证。密码学认证与主机信任策略明确分开；known-hosts、
SSH 证书和应用回调可以复用同一格式层，而不会被固定到某个存储方案。

`ssh_key_text` 在 blob 格式层之上处理 OpenSSH public-key 与 authorized_keys 文本。options 只按
quoted 字段边界保留，不在格式层解释授权策略；算法发现不绑定白名单，Base64 和 blob 解析分别
复用 XRT codec 与 `ssh_hostkey`。解析阶段直接从 Base64 校验 blob 内算法前缀，快速匹配再逐组比较
原始字节，因此既不隐藏畸形记录，也不需要完整临时副本。known_hosts、authorized_keys 策略和
私钥容器可以共享同一文本入口，而不会复制 Base64 或公钥算法解析。

`ssh_known_host` 在同一文本底座上补齐 known_hosts marker 与明文 host pattern。主机匹配使用虚拟
`[host]:port` 视图，不为长主机名分配或使用固定栈缓冲；否定项优先、ASCII 大小写折叠和通配符
规则与 OpenSSH 一致。hashed-host 的 SHA-1 兼容负担留在独立层，普通明文数据库不会因此带入
遗留摘要算法。

`ssh_known_host_hash` 单独实现 OpenSSH `|1|` HMAC-SHA1 格式。salt 由调用方提供，因此安全随机、
确定性测试与数据库策略不耦合；主机名小写折叠和 `[host]:port` 也通过流式摘要完成。只有选择该
兼容模块时才引入 XRT SHA-1，未来替换数据库表示或完全禁用 hashed-host 不影响普通信任路径。

`ssh_known_host_db` 把明文与哈希格式组合成借用文本游标和常见信任判定。扫描时直接逐组比较
Base64 与握手得到的原始 key blob，不建立与 key 等大的临时缓冲；坏行策略可选择宽松或严格，
撤销记录始终优先。CA 只作为候选返回，调用方仍可枚举全部匹配行并实现证书、DNS 或自定义
marker 策略，数据库层不会把复杂信任模型固化为一个布尔值。

`ssh_fingerprint` 直接对完整 host-key blob 计算 OpenSSH SHA-256 指纹，不依赖某个公钥算法或信任
存储。MD5 仅为旧界面显示兼容，不进入默认闭包；需要它的应用仍可显式组合 XRT MD5 与 hex
codec，而不会污染主机验证的安全默认值。

OpenSSH 私钥按二进制容器、PEM 和具体算法拆成三层。`ssh_private_key` 预验证全部公开 key blob，
但保留未知 cipher/KDF 与密文，未来扩展解密不会改变容器 API；`ssh_private_key_pem` 只复用 XRT
PEM 并写入调用方缓冲；`ssh_private_key_ed25519` 才解释未加密 seed、公钥、checkint 和 padding。
秘密不复制进 session，解码缓冲由调用方持有并负责清零，历史固定 8 KiB 临时数组不再存在。

`ssh_transport_message` 将 transport 双方共用的 RFC 4253 消息和 RFC 8308 扩展消息保持在纯 wire
层。EXT_INFO 使用“先完整验证、后借用迭代”，既不设置固定数量上限，也不为未知扩展分配对象；
NEWCOMPRESS 只暴露报文，启用时机和压缩状态重置由后续 transport 状态层控制。

`ssh_transport_rekey` 不读取系统时钟，也不接触 cipher；它只按调用方传入的单调时间、wire 字节、
包数和密码块数维护每代双向预算。预留 API 在密码处理前阻止硬额度之外的包，软阈值则让同步、
future 与协程驱动用同一决策启动非阻塞 rekey。

`ssh_transport_state` 在 packet codec 与会话驱动之间收口协议顺序，但仍不拥有网络、密钥或报文。
KEX 方法以调用方配置的方向性精确额度扩展，错误 guessed packet 只推进线路序列而不消费真实额度；
NEWKEYS 分方向返回密钥激活和 strict 序列重置动作。状态层同时保留 rekey 期间的在途应用数据语义、
首次 KEX 的追溯检查以及 RFC 8308 两次 EXT_INFO 机会，不把这些安全边界分散到同步、future 和协程
三套驱动中。

`ssh_transport_core` 进一步把 packet codec、顺序状态和 rekey 预算组合为无缓冲事务。发送线路包在
网络队列可靠接收前不消费 sequence、nonce 或协议状态，背压取消也不会留下缺口；接收包认证后由
上层解析决定提交或关闭。两个 NEWKEYS 方向分别锁住并激活 cipher、strict sequence 与 rekey 新代，
因此不需要固定接收数组，也不会因双向完成时间不同而抹掉先切换方向的新代统计。

`ssh_kex_session` 在该事务边界上组合双方 KEXINIT、Curve25519、exchange hash、Ed25519 主机签名
验证、外部主机信任和方向性 NEWKEYS 激活。它借用调用方保存的 transcript，服务端签名可来自本地
密钥或 HSM；所有读写仍采用 Prepare/Commit/Abort，只有 transport 可靠提交后才推进会话。系统
安全随机临时密钥入口独立位于 `ssh_kex_session_random`，确定性核心不携带随机或私钥签名实现。

`ssh_kex_exchange` 在会话之上只补连接级 transcript 所有权和可靠提交关联。identification 持续到
连接结束；KEXINIT 使用“当前代/下一代”两组动态链，在新一代会话成功改借新视图后才释放旧代，
因此 rekey 第一包、背压重试或协议拒绝都不会制造悬空协商结果。它用 transport 的方向、KEX 代数
和 packet ordinal 校验 Commit，但不绑定 TCP、等待或任务。安全随机 Begin 仍拆在独立的
`ssh_kex_exchange_random`，确定性闭包只增加 XRT `net_buffer`。

`ssh_transport_tcp` 已把该事务边界直接接到 XRT 公共 `xnetbuf` 与有界 TCP 队列。输出按精确线长
写入 Worker 缓冲块，`SendBuffer` 成功接管后才提交；接收只复制四字节长度头，并仅对当前完整包
按需连续化。该层仍不持有 Stream、Engine 或时钟，后续 future 和协程驱动只负责等待、deadline
和 cancel，不再复制 SSH 状态机或另设每会话固定数组。系统安全随机 padding 便利入口位于独立
`ssh_transport_tcp_random`；确定性核心不会仅因 TCP 适配而携带 `random_secure`。

认证按公共消息和具体方法继续拆层。`ssh_auth_message` 只实现 RFC 4252 请求前缀、none 探测、
failure/success/banner，并把未知方法字段作为原始视图公开；它不保存凭据、不选择认证顺序，也不
绑定客户端或服务端。password、publickey 和 keyboard-interactive 在该层之上独立裁剪。

`ssh_auth_password` 只编码和解析普通密码、旧/新密码以及服务端更改提示。它不把凭据复制到
会话对象，transport 状态机负责在受保护链路上发送并及时清理承载明文的调用方缓冲。

`ssh_auth_publickey` 复用通用 host-key/signature blob，保持 signer 与私钥存储在协议层之外。
签名原文直接写入调用方缓冲；请求算法与 signature blob 算法严格一致，但公钥 blob 格式仍可
不同，以正确支持 RSA SHA-2 等算法。

`ssh_auth_keyboard` 对 RFC 4256 的请求、challenge 和 response 做完整预验证与借用迭代。提示和
响应数量没有库内固定上限，协议层也不保存秘密答案；多轮交互、单个未完成 challenge、总轮次与
截止时间由后续认证状态机统一约束。

`ssh_auth_hostbased` 复用算法无关 host-key 格式并公开完整签名原文。它只严格约束协议中的 DNS
主机名和客户端用户名，不绑定 known-hosts、DNS 反查或授权数据库；主机所有权、网络端点匹配和
用户授权均由服务端策略层决定。

`ssh_auth_guard` 只维护整个认证会话的单调时间、尝试、轮次、消息和字节预算。它不保存用户名或
方法状态，因此身份切换可以清理上层部分认证结果，却不能绕过会话总预算；调用方时钟注入让同步、
future、协程和测试共用同一决策逻辑。

`ssh_auth_session` 在 transport 事务之上收口 `ssh-userauth` service 与双端 USERAUTH 顺序，但不
选择认证方法，也不保存凭据或 payload。方法模块直接构建最终 payload；会话只分类、检查角色和
阶段并事务性提交 guard 预算。60..79 的方法扩展包保持完整原文，因此 publickey、keyboard 及未来
方法都能组合进同一状态机，而无需给同步、future、协程分别重写认证流程。

connection 协议从与 channel 无关的全局 envelope 开始。`ssh_connection_message` 保留未知请求和
成功响应的原始字段，不内置请求类型注册表；需要回复的请求 FIFO、策略和具体端口转发语义由更高
层组合，因此第三方扩展不会被私有 parser 封死。

`ssh_channel_message` 紧接着完成 RFC 4254 的 channel 公共 envelope。Open、confirmation 和
request 的类型专用字段都原样借出，data 路径不分配、不复制到中间容器；EOF、close 与请求响应
采用严格固定报文。窗口扣减、最大包分片、半关闭和回复队列仍归独立状态层，避免协议格式层隐藏
负载策略或固定 channel 数量上限。

`ssh_channel_window` 只维护每个 channel 的双向额度和消费确认。它没有历史实现中的固定 channel
接收数组或 8 KiB 对象缓冲；应用可以按全局内存预算决定何时消费、返还或新授予额度。发送入队和
WINDOW_ADJUST 都使用两阶段提交边界，网络背压不会让本地计数先于真实线路状态推进。

`ssh_channel_request` 把高频 session request 做成直接写入最终 payload 的便利层，同时让命令、
子系统和环境字段保持底层二进制 string 语义。PTY 和 terminal modes 继续独立裁剪，普通 exec、
subsystem 或转发程序不会被迫携带终端模式表及其解析代码。

`ssh_channel_pty` 对 terminal modes 采用调用方缓冲和完整预验证迭代，不恢复历史固定数组。
遇到未来保留 opcode 时按 RFC 停止解释，原始 mode stream 仍由 PTY 视图保留，扩展实现可以继续
处理而不要求核心 parser 先认识新 opcode。

`ssh_channel_state` 独立维护双向 EOF/CLOSE。EOF 只关闭单向数据，CLOSE 必须双向完成后才允许
回收 channel id；本端先 close 时仍容许远端先前已发出的在途数据到达。状态提交发生在消息可靠
入队之后，不把网络发送成功与协议意图混为一体。

`ssh_channel_core` 把单个 channel 的 open、window 和 EOF/CLOSE 状态组合为无缓冲对象。它不拥有
channel 表、data 队列或 request token；调用方可以按本地 id 使用数组、哈希表或 slab，并给活跃
channel 单独绑定 `ssh_reply_queue`。这种组合提供开箱即用的正确状态边界，同时不会恢复历史每会话
固定 channel 数和每 channel 固定收发缓存。

`ssh_channel_io` 是 channel core 之上的可选动态数据面。普通数据与 stderr 分流，但接收和发送分别
共享硬预算；复制、借用、接管、引用和整条 `xnetbuf` 移动共用相同背压口径。接收先在 staging 链
完成分配，发送先借用受窗口约束的队首，只有外层可靠提交 channel 状态后才发布或消费动态块。
未知 extended-data 可以绕过便利缓冲走 connection 借用快路径，因此该层不会把扩展类型封死，也
不会让不需要缓冲的代理或数据泵承担额外复制。

`ssh_forward_message` 复用 global request 与 channel open 的同一套私有前缀构建器，直接输出
tcpip-forward、direct-tcpip 和 forwarded-tcpip payload。它不创建 listener 或连接，也不把地址
提前压缩为平台 socket 结构；转发策略和 XRT 网络数据泵可以在其上独立扩展。

`ssh_reply_queue` 处理 SSH 请求回复没有 request id 的顺序约束。Global 和每个 channel 分别使用
调用方 token ring，取消等待不会删除中间协议位置；存储可以在保留顺序时迁移扩容，因此运行时不
需要历史固定 pending-request 数组。

`ssh_connection_session` 在已认证 transport 上统一分类 RFC 4254 消息，并把 channel core 与 reply
FIFO 的改变延迟到 transport commit 之后。现有 recipient 通过调用方 resolver 映射，新 channel open
保持为借用视图交给策略层选择存储和本地 id；因此底座既能直接使用数组，也能扩展到哈希表、slot map
或 slab，而不会在每个连接中预留固定 channel/event 数组。未知非 connection 消息可以交给外层 KEX
或扩展驱动，已识别畸形消息则关闭同一协议路径。

`ssh_channels` 是 resolver 之上的可选拥有式组合。它用 XRT 整数映射按需保存稳定地址的 channel，
每个节点组合 core、动态 I/O 和回复 FIFO，但空节点不预留 DATA、stderr、发送或 token 数组。活动
channel 数、接收窗口、packet、收发数据和待回复请求都具有独立硬上限；回复 token 只在应用发送
`want-reply` 请求前增长。正常 Remove 会拒绝丢弃未消费数据或回复，连接终止和策略拒绝则走显式
Discard。经典 exec、shell 和 forward 将复用这一层，但自定义代理和固定资源程序仍可直接提供自有
resolver，不被标准存储模型锁定。

`ssh_session_core` 最后只组合 KEX transcript 所有权、认证和 connection 生命周期，并统一分类
transport 控制、KEX、USERAUTH、connection 与未知扩展消息。写事务在上层 Prepare 后与 transport
当前 packet 显式 Bind，可靠提交后才推进子状态；NEWKEYS 自动激活对应 cipher 方向，认证成功自动
开放 connection。对象仍不拥有 socket、等待、凭据、主机信任、channel 表或数据队列，因此 TCP、
同步、future 和协程驱动都能复用同一协议核心。安全随机 KEX 入口继续独立裁剪。

`ssh_session_tcp` 把上述协议事务与 `ssh_transport_tcp` 的真实 Stream 接管边界闭合。identification 与
packet 都只在 `SendBuffer` 成功接管动态链后按 transport、session 顺序提交；AGAIN 保留两层事务，
中止则按逆序回滚。读取返回 packet 线路元数据和连接级轻量解析的同一借用 payload，主机密钥存储
不足时可以在不消费输入的前提下扩容重试。该对象仍不持有 Stream、等待、时钟或任务；确定性闭包
不依赖系统随机，`ssh_session_tcp_random` 只增加安全随机 KEX 和 padding。

连接级 `Action` 将 identification、KEX 和 USERAUTH 的现有子事件映射为同一稳定枚举，并显式暴露
未决读写提交边界。它不保存事件、payload 或回调，因此不会增加每会话内存；后续 callback、future、
同步和协程驱动只解释这一动作并调用公开构建器，仍可随时下钻到 KEX、认证或 connection 子对象。

`ssh_session_reader` 是 TCP 会话上的可选动态读取工作区。明文 packet 直接借用输入链，不分配；
AES-GCM packet 仅在完整线路到齐后按本包明文最小尺寸申请连续尾部。客户端 ECDH_REPLY 先探测
主机公钥精确长度，再在同一未消费 packet 上扩容重试，并只保存最近一次已提交公钥。Reader 不拥有
Stream、Engine、Worker、凭据、信任数据库、channel 表、等待或任务，因此 callback 驱动可以直接
使用，同步、future 和协程只需复用同一事务，不必复制协议状态机。

Reader 还区分 host-key 容量请求和其他内部 `XSSH_ERROR_SPACE`。后者保持为通用 `RETRY`，不会错误
调用 host-key 路径或终止会话；这使 KEX transcript、连接路由及未来扩展的临时分配都能遵守同一套
失败原子性与重试契约。

`ssh_session_stream` 是该协议核心上的最薄 callback 网络驱动。它在 Stream 所属 Worker 上拥有唯一
TCP 会话和 Reader，自动提交已经 Prepare 的输出，并把 HOLD、动态内存重试及 TCP 高低水位统一成
同一串行暂停状态。驱动不构造 KEX、认证或 channel 消息，也不创建 Engine、任务、future 或协程；
客户端直连和服务端 accepted Stream 只共享事件适配，不复制协议状态机。应用仍可通过公开访问器
下钻全部 transport/session API，因此便利入口不会封死代理、自定义认证或扩展消息的低层路径。

旧实现把协议、密码、网络、运行时、known_hosts 和 private key 全部放在一个 267 KiB 头中，
并带有固定 channel/event/receive 缓冲。现代实现不会把这些固定上限带回产品目录；容量预算
将在对应层显式配置，发送和接收由 XRT 的动态 buffer、背压与等待契约承载。
