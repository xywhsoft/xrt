# TLS Stream 显式拥有图（V1，核心阶段）

此阶段补充 Stream 的显式拥有工厂与 checked 退役底座，不宣称
Listener、Dial 或尚有待决 Future 的组合图已经完成。

## 创建与引用合同

`xrtTlsStreamConnectOwnedV1`、`AttachOwnedV1`、`ClientOwnedV1`、
`AcceptOwnedV1` 接收永久 `xtlsstreamownershipv1` 策略。成功恰好接管
一个 Data 引用，失败接管零个。Attach 失败保留调用方 TCP 与 Session
引用及原有缓冲池；成功沿用原 Attach 的 TCP／Session 接管合同。

策略的 Events 与 Data 不可通过 SetEvents 修改。Data 必须真实持有事件
以及 Session 配置中所有借用回调上下文，至少活到 Session 销毁之后。
共享回调拥有记录是独立图节点，禁止将其引用扁平化重复计算。
原有借用工厂仍可使用，但不能因为恰好拥有引用计数而通过此认证。

TCP 自启动前就持有真实的 TLS 事件拥有引用；运行时目录登记现有的
TLS RuntimeHeld 引用，不凭空创造引用。TLS 另持一个 Engine 操作引用
和一个 Engine storage 引用，Trace 必须分别列出这两条真实边。
操作引用在协议停止后释放；storage 引用让静止后仍可安全检查外壳，
不会阻止 Engine 停止。

## 发现、静止与退役

`xrtTlsStreamOwnership` 返回引用；Quiescence、Runtime、Transport
策略 getter 返回永久身份。`OwnershipAdapterV1` 只接纳显式白名单中的
同一策略地址，不接受内容相同的副本，拒绝时不修改输出。

Trace 包含真实 Engine 引用、TCP 引用、Data 拥有记录、Session 四个
缓冲队列的实际块／池边及错误根因。操作中、未知 Listener 管理回调、
尚有旧式异步节点时必须拒绝图发现；此阶段不替未知节点猜测边。

Claim 隔离新 Close／Abort／异步提交及 Worker 上的公开协议修改；
已有原生生命周期操作仍能推进实际关闭。Prepare 通过 TCP 生命周期
命令停止协议，等待运行时、定时器、内部命令与回调真正结束。
停止后先分离四个缓冲队列，在 Freeze 外释放，再以 READY 报告边变化，
要求调用方重新发现全图，避免缓冲池等待租约而造成循环准备依赖。

Clear 仅在最终重新验证后标记逻辑清空，不执行用户回调。
Finish 在 Freeze／Mutation 外销毁 Session、TCP 和 Data（顺序固定），
保留首次清理错误；重复 Finish 不重复析构。进入时已有线程错误优先
恢复，之后可以再次检查持久清理错误。

## 验证边界

`tools/check_tls_stream_ownership.py` 仅运行显式允许的本地固定密钥
正常握手、已有普通传输用例及循环图退役。覆盖 Select 与平台原生
后端、模块化／单头、两种优化和 TCC；Linux 可附加 ASan／UBSan。
不执行外部网络、DNS、畸形报文、扫描或密码学安全测试。

新的循环图用例保留未读的 `a\0b`，验证池租约依赖、策略身份拒绝、
pending Future 保守拒绝、Claim 隔离、重复 Finish、首错恢复和完整
内存台账。此验证不等于全部 TLS 公开接口或 xlang 发布产物已验收。
