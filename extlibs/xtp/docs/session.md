# 基础 Session 接入契约

选择 `XTP_MODULE_XTP_SESSION`。`xtpCreate(NULL, &events, user, &session)` 复制默认有限配置和回调表；同一实例所有调用由一个执行者串行，不同实例可以并行。XTP 不创建网络对象、线程、定时器或路由表。完整可运行例程见 [session](../examples/session/main.c)。

宿主以 `xtpBind` 提供 lane：非零唯一 ID、STREAM/RECORD、可靠性和 RECORD 长度限制。每条 lane 有独立 Reader，不能混合不同流的半帧。LaneID=0 在 Submit 选择首条 lane，在 Output 按成功 Advance 后的轮转位置选择非空、未过期 lane 头；需要独立处理背压时按具体 LaneID 排出。同一 lane 保持 FIFO，不承诺不同 lane 的全局顺序，单条 STREAM 内帧不会交错。可靠 RECORD 的长度限制至少为实例 MaxResponseBytes；不可靠 RECORD 只允许 NOTIFY，超限不自动切换到可靠 lane。

Bind 的 Authenticated 是宿主对真实 TLS/Noise/其他安全承载已经完成认证的声明，不是 XTP 认证过程。默认拒绝未认证绑定；AllowPlaintext 仅用于明确选择的可信本地或测试连接。裸 UDP 的被动解析可只用 Reader，不能因此声称具有可靠或安全投递。

数据到达时调用 `xtpInput(session, binding, lane, bytes, record_end, now, &progress)`，消费恰好 progress.Used。未消费后缀由宿主保留；MORE 等网络，AGAIN 按预算继续，PAUSED 显式 Resume 后再交后缀。完整借用帧 PAUSE 时 Used 不包含该帧；分片组装帧可能已消费全部输入并由 Reader 保留。RecordEnd 随未消费后缀重交；空 Input 不是 EOF，接收半关闭用 End。非法参数/旧绑定失败不修改 Progress。

只使用一个 `xtpSubmit` 或 `xtpSubmitBatch`；用 CmdID/CmdName 与 Array/Table 表达四种组合。成功前完整预检并复制，失败不接管数据或 Tag，不发布部分操作、不消耗线路 ID、不改输出令牌。REQUEST/PUSH 共用本方向 UInt64 递增 ID，NOTIFY 的线路 ID 固定为 0。成功提交各自最多一次 OnComplete；Operation 可省略，但不省略完成生命周期。

OnMessage 只交付 REQUEST/PUSH/NOTIFY，Command.Kind 与 Params.Kind 独立。array/table 均可 ParamAt，table 另可 ParamFind。入站视图只在回调内有效，异步业务必须复制所需数据；复制请求令牌不保留 Body。REQUEST 返回 Accept 表示已经 Reply 或承担异步 Reply 责任。Reply 使用本次新鲜时钟、有效请求令牌和无 Command 的 response，可换结果参数模式；令牌在 Reply、超时、Unbind 或 Close 后失效。

处理 REQUEST 前分配并计费完整 MaxResponseBytes 物理缓冲及发送槽，因此 Reply 不再分配；同时受 TxBytes、槽数、IncomingRequests 和总内存限制，可能早于条数上限触发 BUSY/背压。处理 PUSH 前预留嵌入式 32 字节 ACK。普通发送不能使用 ResponseReserveBytes/Frames。回调接受后不能再因 ACK 无空间而撤销接受。PUSH 的成功 ACK 只证明接收端接管，不证明磁盘或业务完成；Notify 无回包。

回调可同步 Submit/Reply/请求 Close；它们不递归派发完成回调。成功 Reply 后必须返回 Accept；其他返回值或非法 Action/Status 是致命的本地契约错误。Destroy、Bind、Input、Advance、Drive 不允许在回调中重入。分配器必须满足普通 realloc 对齐及失败保留语义，不重入实例；Size=0 释放。User、分配器上下文和回调代码须保持到 Destroy；Config/events 描述本身已复制，可以释放其原存储。

宿主以 Output 获取借用的不可变数据和 Ticket，先让下层真正复制/接管，再以 Advance 报告受理字节。STREAM 可短写并跨越多个帧边界，RECORD 只能完整受理当前输出或零受理。零受理返回 AGAIN，不移动游标。成功 Advance 后旧 Ticket 失效；绑定代、槽代、偏移或聚合快照任一不符都拒绝。Output 借用不能跨越会改变输出状态的调用，发送与 Advance 之间不得递归进入 XTP；异步保留发送须由宿主先复制。

默认 BatchBytes=16384，立即组合同一可靠 lane 已排队的帧，不等待攒包；Output.Frames 是当前输出包含的帧后缀数。聚合受 BatchBytes、Codec/Reader 帧数预算、RECORD 长度和 Drain 的 Bytes/Frames/ChunkBytes 共同限制；不可靠 RECORD 不自动组合，大帧直接输出。BatchBytes=0 关闭聚合。规范帧独立保留，聚合额外复制到一个惰性分配、重复利用且计入 MaxSessionBytes 的缓冲，分配失败退回逐帧发送。重复获取未变化的输出复用快照；完成回调可提交新操作或请求关闭，Advance 先记完实际受理的全部前缀再执行延迟关闭。Info.Batches/BatchedFrames/BatchCopiedBytes 统计实际构建的聚合，包括构建后未发送的快照，不能将此路径宣称为零复制。

宿主在业务提交、可写事件和 next_due 到期时调用 Drive 并排出数据。时钟为 UInt64 单调微秒，操作期限从 API 提交起计算，包含排队；零 DeadlineUs 使用有限默认值。维护和关闭每轮受 Reader.MaxFrames 限制，接收维护共享 MaxWorkBytes；返回 AGAIN 要再安排一轮。接收、输入和调用者拥有的网络缓冲之外，所有自有容量、表、响应预留及 realloc 的旧块加新块都计入 MaxSessionBytes。已知过期的队列头不再由 Output 提供，交给 Drive 退休；部分 STREAM 帧超时会报告 Fatal 并关闭，避免下一帧被拼到残帧中。

RESPONSE/ACK 只通过 OnComplete 关联原 pending，同时校验 ID、原类型、lane 和已发送状态。NOTIFY 下层整条受理即本地完成，不证明远端收到。结果、完成原因、本地错误、远端 Status 和 Knowledge 分开；STATE_LOST 始终 UNKNOWN。迟到、重复或未知结果只计数，不能完成另一个操作。OnComplete 的结果也只借用到回调返回。

Close 可以在回调内请求；回调返回后分轮清空操作，完成结果保持 NOT_SENT/UNKNOWN。反复 Close/Drive 到 CLOSED 后才能 Destroy。Unbind 使绑定立即失效，分轮终结基础操作、清除接收状态和请求令牌，随后可以再次 Bind；旧网络事件和输出 Ticket 不影响新绑定。主机必须先停止输入并加入所有异步业务，再释放 Session。

单独选择 xtp_session 时没有恢复能力，Replay=true 明确拒绝。完整 xtp 包可启用 [Delivery](delivery.md)，只有显式 Replay 的操作和请求令牌跨有效续接保留。基础去重窗口仍有退休下界；它不构成持久化承诺。网络适配见 [transport](transport.md) 与 [xrudp](xrudp.md)。
