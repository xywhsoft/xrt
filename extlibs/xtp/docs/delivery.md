# 可选的恢复投递

`xtp_delivery` 在 Session 上增加有界的内存恢复能力，普通 `xtp_session` 可以单独裁剪。完整 `xtp` 包含此模块，但 `config.Delivery.Enabled` 默认 false。以 `xtpDeliveryConfigInit(&config.Delivery)` 启用，再逐条设置 `options.Replay=true`。仅 REQUEST/PUSH 合法；可恢复 REQUEST 还要求本地配置和双方证明都允许 `AllowRequests`。NOTIFY 不保留、不确认、不重放。

## 身份与续接

宿主在自己的已认证控制面交换随机 128 位 SessionID、应用域、双方规范主体 ID、恢复期限、双方保留状态和去重下界。生产环境用 `xrtSecureRandom` 生成 SessionID。`xtpdeliveryproof` 的 Identity 固定为 SessionID[16]、Domain[16]、Local[32]、Peer[32]；每段至少一字节非零，存公开身份，不存凭据。双方各自的 Local/Peer 顺序相反。

Bind 只借用证明，不负责核验网络证书或控制面。`Authenticated=true` 是宿主对真实认证结果的声明，不能由未认证网络数据直接填写。Delivery 不接受 AllowPlaintext 例外，也不增加登录、恢复命令或第六种线消息。`examples/delivery/main.c` 使用固定测试身份说明生命周期；生产中必须替换为真实认证结果。

首次证明 Resume=false、StateRetained=true。后续先停止并隔离旧承载，再 Unbind；Unbind/Drive 返回 AGAIN 要继续维护到完成。两端保留原 Session 实例。重新 Bind 要有 Resume、StateRetained、OldBindingDetached，并保持完整身份、约定 LifetimeUs、请求能力和本地 lane 顺序/模式/可靠性/记录上限不变。重建接收 Reader 后，从完整规范帧起点恢复，旧半包与旧 Ticket 全部失效。Reader 创建 OOM 可重试，不损坏保留状态。

PeerReceiveFloor/PeerResultFloor 从对端 `xtpInfo` 的 ReceiveFloor/ResultFloor 经认证控制面传递。低于对端接收下界的未完成 PUSH 不能恢复；精确保留的活跃 REQUEST 不因滚动窗口前移而误判。ResultFloor 证明本地没有仍活跃且 ID 低于此值的可恢复 REQUEST；恢复时可回收对端低于该值的完成缓存，不将回收解释成结果已消费。身份、状态证明或期限丢失会关闭旧逻辑会话，已发送操作以 UNKNOWN 完成；不自动创建同名新会话重跑业务。

UntilUs 是本地单调时钟上的有限认证到期时间，不能是 UINT64_MAX；双方分别映射自己的时钟，不能比较绝对单调时钟值。重绑定只能缩短原认证期限。LifetimeUs 是双方协商的恢复保留时长，必须相同且不超过本地 MaxLifetimeUs（最多一小时），不能续期延长旧承诺。Replay 的原始操作截止时间必须落在提交时刻加 LifetimeUs 以内且早于认证期限，否则明确返回 E_LIMIT。不会暗中修改调用者期限。

## 保留、回执与完成

发件箱只保存规范 XTP 帧，不保存 TLS 密文。每条占用原 Tx 槽位/字节预算；不另建无界队列。健康可靠链路没有 ACK 超时重传，重发只发生在明确 BUSY 退避或通过验证的续接。BUSY 表示尚未受理，采用有界指数退避和轻量抖动，始终保持原 ID、内容、操作令牌和截止时间。尝试次数只在本轮第一次实际接受字节时增加；零短写不计数。重试上限不允许截断正在发送的帧。

可靠 PUSH 的 ACK 表示业务回调已经 ACCEPT（或放入应用自己的有界队列），不证明持久化或业务执行成功。BUSY 不写已受理历史。重复的已受理 PUSH 仅重复 ACK。历史窗口外的旧 ID 返回 STATE_LOST，不再次执行业务。

接收可恢复 REQUEST 前，先预留完整最大响应缓冲、槽位及保留预算，再调用业务。活跃请求不重复调用；已完成请求共享原响应发送缓冲缓存结果。令牌跨有效续接存活，允许暂停期间异步 Reply；收到 BUSY 必须通过 OnMessage 返回 xtpBusy，已受理的异步 Reply 不能返回 BUSY。等待业务到期产生 DEADLINE_EXCEEDED；整个缓存到期后仅保留旧 ID 拒绝保护。

请求方收到有效匹配 RESPONSE 时，先预留 RESULT_ACK，再写入固定滚动完成记录并调用 OnComplete。结果视图借用到回调返回，需要长期使用时由回调复制。成功回执 Status=0，只有确实消费过的结果才使用；回调返回后才能发送。窗口内重复结果重复成功回执，不再次回调。

已退休或超时后到达的旧结果使用 RESULT_ACK + STATE_LOST，明确表示结果已不再保留，不证明业务结果曾被消费。它只让接收方释放完成缓存，不触发成功完成、不重新执行业务。未曾签发的 ID、仍有不匹配 pending，或仍保有完成记录但 lane 不符时，不获这一回执。完成记录滚出窗口后不再保存原类型/lane；对于已签发且无活跃记录的 ID，可在当前可靠 lane 明确退休，但不能据此推断原路由或成功消费。对端仍校验其缓存 ID/lane 后才释放。RESULT_ACK 不再要求 ACK。

若回执/响应在重发半包中途到达，逻辑操作只完成一次，但保留物理缓冲与配额直到该帧余部发送完成。不会为了尽早释放缓存而破坏同一流的下一帧。原操作令牌、异步请求令牌和逐轮输出 Ticket 各自独立；旧轮次的 offset=0 Ticket 也不能作用于重试。

## 有限容量及成本

所有表、接收 Reader、缓冲和 realloc 峰值都计入 MaxSessionBytes。MaxReplayMessages 默认 256，包含发件箱和接收方结果缓存，共用 MaxTxFrames/MaxTxBytes；BUSY/关闭/到期清理和 Reader 回调受每轮预算限制。普通 RPC 不增加结果 ACK、回执表占位或长期结果缓存。

可恢复 RPC 使用 DedupWindow 个固定紧凑完成记录（默认 1024，最多 65536），按 ID 取模 O(1) 定位，最近结果覆盖旧完成记录，不为完成记录保留一个按时间计费的入场槽。每个活跃请求继续使用精确 pending/规范帧，滚动窗口不得淘汰它。ResultFloor 会钳制在最老的未到期活跃请求，保护乱序和长请求。

因此吞吐不再受 1024/60s 的约 17 次/秒上限限制。内存仍受窗口、MaxPending、MaxReplayMessages、Tx 配额及 MaxSessionBytes 的硬限制；Receiver 的未确认结果在约定期限内不会被新业务挤掉。遇到缓存/发送预算不足仍需处理 BUSY/E_LIMIT。完成记录没有独立过期扫描，Drive 不再遍历它来回收时间槽。

这是进程内、有限期限的受控恢复，不保证进程崩溃后的业务恰好执行一次。交易仍需要业务幂等与事务。状态丢失、过期或超出窗口都会明确失败，不自动重新执行。
