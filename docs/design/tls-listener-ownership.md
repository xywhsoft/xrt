# TLS Listener 的实际组合拥有图

`StartOwnedV1` 使用永久 `xtlslistenerownershipv1`。成功消费一个 Data 引用，
失败消费零个。Data 覆盖 Listener、TLS 配置及所有子连接借用的回调上下文。
发布前不接入新子连接；失败前已经发出的启动错误回调必须先退栈，才可
交还 Data 信用。Context／Identity 独立保留，ALPN 仍深复制。

Listener 分别持有 Engine 操作与 storage 两个实际引用；Engine 目录登记
原有 RuntimeHeld 信用，底层 owned TCP Listener 另持一个 Listener 引用。
普通关闭释放运行时／操作信用，但 storage 及不可变配置保留到实际退役。

## 子连接记录

每条记录实际持有一个 Listener 引用，尚未交付时还持有一个 Stream 创建
引用。TLS Stream 上下文拥有一份记录引用；握手链或完成队列各拥有另外
一份记录引用，链中的 Next／Previous 本身不算信用。交付时只转移已有
Stream 创建信用，不能把结构借用误算成新引用。

子 Stream 复用既有不可变拥有上下文和永久事件表，不增加 Observer 体系，
也不在交接时改写事件。pull 与 push Read 分别使用两个明确的永久策略，
不能让空转的 Read 包装函数把 pull 错判成 push。事件回调收到原始 Data；
低层 `xrtTlsStreamData` 指向组合记录，不是原始 Data，应用不可强转它。

Open／Close 及转发回调持有真实帧引用并标 Busy。Close 先退役握手／队列
信用和未交付 Stream 信用，记录仍由 Stream 上下文拥有，直到 Session
销毁后释放。清理的所有目标必须保持 pin，直到全部 Finish 完成。

## accept Future 与退役

每个等待激活有真实的 FIFO／producer／Owned CancelWatch 三份引用，
拥有 Listener、Promise、Watch。完成握手的 Stream 用拥有 payload 策略
交付；复制策略地址不算身份认证，每个传递节点仍需独立准入。

Claim 拒绝新公开 Accept／AcceptAsync／Close。Prepare 请求底层 Listener
真正停止；Native Close 在锁外退役未发布握手、排队连接与等待者，不
提前伪造 Future 终态。已经交付的连接不随 Listener 普通 Close 被关闭。
在闭合全图中，子 Stream 的各自准备职责负责其正常退役。

Clear 不执行用户回调；Finish 在 Freeze／Mutation 外释放原生配置和
Data，保留首次清理错误并恢复已有线程错误，重复检查不重复析构。
Engine storage 与记录 pin 防止静止后元数据悬空。

## 普通验收

`check_tls_listener_ownership.py` 仅执行显式允许的本机回环：完整固定
密钥握手、正在准备握手的合法 TCP 连接、pull／push／Future、NUL 载荷、
构造 OOM 与实际循环图退役。所有监听和拨号都显式使用 loopback；监听
通配地址不是可直接用于拨号的地址。不会运行公网／DNS、畸形记录、
扫描、攻击、模糊或安全专项。本批不代表 Dial、语言层迁移或 P7 完成。
