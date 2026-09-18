# Resolver Future 的真实拥有链

此合同对应 `xrtNetResolveAsync` 的实际实现，不是另一个仅供图测试调用的工厂。

## 物理拥有关系

一个 producer context 保存实际拥有的 Promise、ResolveOp，以及 bridge 内实际拥有
的 CancelWatch。Bridge 的 Promise 指针只是同一个端点的借用，不产生第二条边；
Promise 与 Future 也归一为同一个物理控制块。Future 在 pending 时拥有 producer，
请求与取消监听各自持有一个真实 context 引用。成功结果拥有一个不可变 AddrList。

创建者的活动引用、producer 引用、请求引用和监听引用各自显式 retain/transfer/drop，
不使用“预设总共有两个回调”的固定计数。默认、无缓存的单个排队请求形成 7 个
物理节点、11 条对象边；Resolver 创建者与 Future 观察者是两个真实外部拥有槽。
将这两个实际槽作为 internal slots 后得到 13 条边、0 个外部根，不凭空减 RC。

各参加者按其独立策略身份准入：Future producer/payload、Resolver request、
CancelWatch。即使内容相同的另一份策略表也不等价。自定义 Lookup 的认证仍须由
调用方独立提供，旧借用 callback 继续不透明。

## 创建、完成与取消

所有可能失败的 bridge 装配先完成，最后才提交 OwnedV1 解析请求；提交成功后只剩
操作槽发布和 Ready，不再存在“请求已接纳、监听装配又失败”的取消回滚。
成功接纳消费请求上下文引用，失败不消费。每条失败路径归还实际已接纳的槽，
保留原始诊断，且没有悄悄接纳的底层请求。

Done 必须先 acquire BridgeWait，再读取创建者发布的字段。取消回调在 context 锁内
取得一个独立 ResolveOp 引用，然后在锁和 mutation scope 外调用 Cancel。Done 的
Unwatch 等待、Promise 通知及所有用户语义回调均在自己的 scope 外；request 所持
context 引用一直保护到 Done 和 release tail 完成。

通用 bridge 仍为 32 字节，发布 Setup 之前先归还其修改标记。Setup 发布后不得再写
bridge 字段，因为唯一等待者可以立即注销。认证 Watch 的物理查询拒绝尚未发布、
正在注册/注销和旧借用监听；拒绝不修改输出或诊断，也不把借用 Promise 当拥有边。

## 准备与退出可观察性

Producer Prepare 不取消已接纳请求、不提前关闭 Future；它等待原语义完成，
Resolver Prepare 排空原工作。回调、运行查询及活动尾部仍拒绝图准入。

非阻塞 Resolver Prepare 开始关停后可能先返回 BUSY。旧 Count 永久要求 Joined，
而规划器在下一次 Prepare 前又必须先重新 Count，形成循环等待。现在退出 worker
通过 `xrtThreadStateTry` 只读、非阻塞地观察其真实 XRT 上下文完成状态：清理仍
活动或线程状态锁有争用就拒绝；所有退出线程的上下文完成后可以重新采集，随后
Prepare 才确认 Joined。Count 本身不执行 Join、不关闭句柄、不改 owning slots。

该查询从不阻塞取得线程状态锁，避免 ThreadFinish 持有状态锁并等待引用 mutation、
而图冻结反过来等待状态锁的反转。FINISHED 的含义与 ThreadState/ThreadWait 一致；
它不是额外的 OS 原生线程退出、外部 TLS 或 DLL 卸载证明。

## 验证边界

两个模块化和两个单头程序保留以下断言：

- 精确 pending 图、正反释放顺序、取消及丢弃观察者后仍完成已接纳工作；
- 独立结果引用、同内容错误策略拒绝、48 个分配预算的失败原子性；
- 完成通知执行时外线程取得 freeze，活动 context/服务仍拒绝采集；
- 真实阻塞查询下 Prepare 只返回 BUSY，不改变 pending 结果或调用前诊断；
- 真实线程键清理回调阻塞时拒绝采集，结束后无需先调用 Prepare 即可重新观察；
- 同步继承取消、重入拒绝、额外 Watch pin、256 次 Ready/Fail 与立即注销竞争；
- 旧 TCP/TLS dial Future、task network 和 bridge 正常/OOM 测试不删除。

这些是原生组件合同。Engine 图、生成模块准入、混合对象和真正 DLL 卸载，以及
同源语言/FFI、发布和平台验收需要各自的完整证据，不能从此合同直接推断完成。
