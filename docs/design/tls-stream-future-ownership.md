# TLS Stream Future 实际激活图

此阶段取代上一阶段对 pending TLS Future 的保守拒绝；不等于 Listener、
Dial 或整个语言 TLS 接入已经完成。所有处理仍复用 XRT 协议与 Future。

## 引用与发布

每个异步操作是独立、真实引用计数的激活节点，构造／FIFO、Promise 的
producer 绑定、Owned CancelWatch 上下文各持有一个引用。Next 只是结构，
不能按每个引用再复制一份子图。节点实际拥有 Stream、Promise、Watch，
以及缓存分配时的一个 Engine 租约；Trace 分别列出这些物理槽。

节点与发送副本维持一次分配，元数据不超过 128 字节的缓存类别。
Promise 独立拥有。Publishing／Active 将半发布状态、取消调用帧、消费及
终结过程排除在图之外。失败先 Unwatch、归还预算和引用，再发布 Done；
保留原错误，不能调用节点析构来伪造正常取消。

发布只在完整初始化后转移构造引用给 FIFO。完成先同步退役 Watch，再
移除源与 Promise 槽，发布 Done 后释放激活引用。若收集 pin 仍保留外壳，
此时就脱离缓存和 Engine 操作租约，避免 Engine 退役等待惰性外壳。
无额外 pin 的普通完成仍归还同一小节点缓存。

## 图与语义完成

公开永久 producer、cancel、bytes payload、quiescence 策略和 Adapter。
身份白名单精确匹配；每个 Stream、Promise、Token、Watch 和 bytes 仍需
独立准入。接收结果通过标准 `ResolveOwnedPolicyV1` 发布，不再隐藏成
不可检查的裸析构回调。队列 walker 验证环、尾指针、类型、偏移、节点数、
读取数、发送数和完整字节预算，再逐个访问真实节点，不估算引用数。

节点 Prepare 等待实际源完成，不直接关闭中间 Future；Stream Prepare
通过真正的 TCP 生命周期停止并通知已受理操作。Claim 阻止新公开提交，
但允许已受理原生读写在正确 Worker 上继续。内部 accepted 入口仍检查
未 Clear 状态、线程、写入／关闭／取消门；不临时解除 Claim、不暴露到
语言 API、不绕过真实终态，也不改变任一时间 ABI。

## 验证范围

`tools/check_tls_stream_future_ownership.py` 只运行显式允许的普通固定密钥
本机回环。新夹具验证 Connect／Attach／Client／Accept 拥有工厂、待读
导致的 Attach 失败原子性、构造 OOM、真实 pending 节点的三份引用、拥有
bytes 结果、策略副本拒绝、Claim／Restore、原生停止、惰性 pin、持久清理
错误和完整内存台账。旧 Stream／交接正常用例同时回归。没有公网、DNS、
畸形密码材料、探测、扫描、模糊或安全专项测试。
