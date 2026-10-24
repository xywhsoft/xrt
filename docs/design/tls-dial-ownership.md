# TLS Dial 的拥有阶段与不可变交付

`xrtTlsDialOwnedV1` 接受永久 `xtlsdialownershipv1`。一次成功只接管一个
Data 引用；失败接管零个。Data 同时覆盖 Done、最终 Stream 事件和 TLS
Session 借用的所有配置回调。Context／Verifier 等既有原生保留规则不变。

## 实际信用与阶段交接

调用方与 Engine 目录各有一个真实 Dial 引用。Dial 分别保留 Engine 的
操作及 storage 信用；已受理总期限 Timer 和 TCP Done 上下文各另持一个
真实引用。Timer 使用 owned policy，而不是把借用指针伪装为拥有引用。

TCP 阶段只使用已有 `xrtNetDialOwnedV1`，不安装借用式 TCP Stream 事件。
其 Done 在实际 Worker 上把真实 TCP 创建信用和预先创建的 Session 交给
`xrtTlsStreamAttachOwnedV1`。Attach 的类型正确的输出槽在可能回调前发布；
同步关闭可以清空该槽，不强转原子存储，不在返回后重新发布陈旧指针。

TLS Stream 的永久上下文持有一个 Dial 引用。握手时 Dial 持有 Stream
创建信用；交付时把这一信用转移给 Done，并清空自己的槽。Stream 使用
永久 pull／push 两种策略，Open 先于成功 Done，不在交接时 SetEvents。
事件收到原始用户 Data；低层 StreamData 是私有 Dial 上下文，不能强转。
Session 始终先于其借用上下文销毁。

## 线程、期限与终态

Stream 阶段槽只由所属 Worker 访问。公开取消先原子争取终态门，再将
阶段取消投递到 Worker；不跨线程借用一个可能正被交付／释放的指针。
嵌入停止命令借用原有 runtime 信用，不凭空增加队列引用。

ownership Mutation 只排除 Freeze，不提供线程互斥。构造线程和 Timer／
TCP Worker 共享的发布、runtime 转移和停止投递，用独立自旋锁串行。
实际 runtime 信用只有在发布／放弃构造、TCP Done、唯一终态、总期限
和停止命令都已收敛时转移一次；操作租约同时释放，storage 延至物理清理。
回调帧各保留真实引用并标 Active，不能仅凭 Active 计数延长生命。

两套时间合同不互换数值：原生 public Schedule 接收相对毫秒，固定快照
以 `XRT_DEADLINE_NEVER` 标识既有绝对微秒 deadline 合同，先用该版本的
`xrtDeadlineAfter` 构造期限再提交。没有 ABI 强转，不升级其他时间 API。

## 静止与 checked 退役

Count／Trace 仅认证拥有工厂对象；未知策略副本拒绝且不改变输出。图中
逐项枚举实际 Engine、TCP Dial、独占 Session 缓冲、未交付 Stream、用户
Data 和错误信用。Timer、TCP Done、Stream context 都有明确的永久身份，
每个传递目标仍需独立准入。

Claim 拒绝新公开取消；Prepare 在 Freeze 内只请求实际 Worker 停止，
不会调用用户代码或假造 Future 终态。已交付 Stream 由自身参与者准备。
全图 Ready 后 Clear 只作逻辑断开；全部目标保留 pin，直到所有 Finish
在 Freeze／Mutation 外释放真实资源。首次清理错误被保留，已有线程错误
优先，重复 Finish 不重复析构。

## 验证与未完成范围

普通夹具覆盖双 Worker、成功交付／Open 顺序、pull／push／NUL、合法但
尚未完成的 TLS 握手、总期限、显式取消、图退役、分配失败、永久策略
身份拒绝和持久清理错误；所有监听与连接都是显式 loopback。固定快照
另用同一夹具和显式测试单位适配实测，不以 SDK 指针绑定代替时序行为。
没有公网、DNS 查询、畸形输入、扫描、攻击、模糊或安全专项测试。

本批不宣称旧借用 Dial、Proxy Dial 或旧 FutureBridgeWait 已拥有化。
Future producer／Owned CancelWatch／拥有结果、Proxy 拥有图和语言传输
迁移仍需后续完成，不能将本批记作完整 TLS 或 P7 发布验收。
