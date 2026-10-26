# TLS Verifier 的原生上下文拥有权

`xrtTlsVerifierCreateOwnedV1(config, limits, policy)` 是显式的物理拥有合同，
不是新的信任算法。复用原有默认路径、名称、用途、时钟和资源预算，不改变
`xtlsverifierconfig` 的字段或既有函数签名。独立 XRT 的时间 ABI 不变。

## 接管及驻留条件

成功接管 **一个实际 Context 引用**；失败不接管，也不执行任何回调／Drop。
config／limits／policy 都必需，Context 非空，policy 的四个回调槽必须与
config 完全一致。Ops 必须描述同一个物理 Context 节点的真实计数和每一条
强引用槽，不得把语言值扁平展开成虚构引用。独占的信任快照由 Verifier 自己释放。

policy、Ops 和回调代码不可变且持续驻留，直到原生消费者和 collector pin
全部释放。接纳比较的是已注册的 **policy 地址身份**；字段相同的副本不是
权限。创建时传入的合法 policy 不会因此自动进入任意语言运行时的目录。

普通 Create／CreateLimited 保持旧合同，其 opaque context 不会因为回调
恰好相同而自动得到拥有图／退役资格。

## 调用与退役

拥有式 Verifier 的每次真实 Verify 都持有实际临时引用，并标记整次调用为
busy，包括 Time、Verify、默认验证和附加 Policy。回调释放原始引用不能让
执行中的 Verifier／Context 消失。检查不能把活动回调看成静止的可退役节点。

`xrtTlsVerifierOwnership` 报告实际 Verifier 节点；Trace 报告唯一被接管的
Context 引用。Count／Trace、adapter 接纳与 Busy 观察遵循 core 的完整图
freeze 合同，既不提供独立 collector，也不代替真实强引用和代码拥有者。

adapter 必须按以下顺序使用：实际 Hold → 全图 freeze 下 Claim → freeze 外
Prepare → 重新核对全图／语义终结 → freeze 下 Clear → freeze 外 Finish。
Claim 阻止新的验证调用，Restore 可撤销尚未 Clear 的 claim。不同 token 不能
夺取 claim。Clear 不执行析构；Finish 在域外释放 Context 和信任快照。
清理失败保留首因，重试报告该失败，不重复已经执行的 Drop。

## 明确边界

每一个消费者都须报告自己实际持有的 Verifier 引用；语言包装不能把带闭包
的 Verifier 当作 native leaf。`xrtTlsSessionOwnershipTrace` 在四个缓冲队列
之外，报告内建 verified-client 实际持有的 Verifier 槽。Stream 与尚持有
Session 的 Dial 组合这套遍历，不能继续只遍历缓冲区。

此接口要求 Session 被唯一拥有、静止并处于完整图 freeze 中。先核对实际
内建角色的清理函数身份，再读取角色尾部；内部无角色尾部的 base Session
不会被强制解释为 client/server 尾部。原生参数、内建密钥和 DER／resume
字节不含回调拥有边；不代表这些原生资源只能有一个引用。

自定义 signer 身份的外部支持、签名、清理或上下文槽只要任一个存在，
Session 的完整图遍历就拒绝，Listener 的图接纳也拒绝；不能从 identity
类型或 NULL Context 推断它是叶对象。普通 C 构造、签名及销毁不受影响。
借用的 server selector／resume 数据仍由嵌入者自己的拥有图和代码驻留合同
覆盖，不把借用指针虚构成 Session 的强引用，也不接纳未知 Verifier 子节点。

语言层附加验证和 CRL 提供者的接线、公开拥有快照、异常传播、恢复凭据／
策略绑定属于语言接入工作。不得仅因本接口
存在就宣称语言 CRL、完整 TLS 或 shipping 已通过。CRL 默认关闭，显式开启
后严格拒绝未知结果的用户决策不被此原生接口改变。

`tests/tls/test_tls_verify_ownership.c` 仅使用未修改的正常固定证书链，默认
验签／名称检查照常执行；测试真实 context 环、活动时钟、回调内释放原始
引用、分配回滚、claim／restore 和 checked 清理失败重试。没有 socket、畸形
密码材料、信任绕过或安全性质专项。
