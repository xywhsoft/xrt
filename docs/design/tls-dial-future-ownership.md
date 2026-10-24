# TLS Dial Future：非阻塞发布与拥有式 Proxy

## 范围

本批为 `xrtTlsDialAsyncOwnedV1`、`xrtTlsDialProxyOwnedV1` 和
`xrtTlsDialProxyAsyncOwnedV1` 提供真实拥有图、静止检查和 checked 退役。
复用已有 TCP／Proxy Dial 与 TLS Attach；不增加第二套网络或密码算法。
旧借用式入口仍保留原来的 Data／事件合同，不因共用实现而获得图认证。

## 发布与引用

TCP、Proxy 和 TLS Future 使用公共 Dial Future 发布器。提交者、producer、
Owned CancelWatch 和已接受 Done 分别持有真实引用。Promise 直接交付
各族的拥有 payload，不再调用 `xrtFutureBridgeWait`。TLS payload 的 Drop／
Trace 对应真正的 TLS Stream，不把 TLS 指针强转为 TCP Stream。

完成可能发生在 Submit 返回之前，包括另一个线程先完成并被 Submit
同步等待退出的情形。此时完成回调只把结果信用和错误保存到终态槽，
立即返回；提交者发布 Dial 后，或者后续完成回调，以同一把锁选择唯一
处理者。实际取消、Unwatch、销毁及 Promise 完成在锁和 Mutation 之外。
发布前的取消请求保留，发布后向实际 Dial 转发，不能因 Dial 槽尚为空而
丢失。Mutation 只排除 Freeze，不是线程互斥锁。

TLS 的配置／用户事件另由独立 owner record 持有。producer 的 Promise、
Watch 和 Dial 槽完成后可以清空，但已交付 Stream 仍能追踪和使用配置。
record 在构造期间有 wrapper 与暂定转移两个真实信用，防止已接受操作
在构造返回前完成并释放整图时发生悬空。构造成功消费一份用户 Data，
失败不消费；成功后 record 才提交 DataHeld，再释放 wrapper 信用。

永久 pull／push 策略分别保留 Read=NULL 与实际 Read 回调，继续把原始
用户 Data 交给用户事件；不增加 Observer，不在 Open 中替换事件表。
旧 `xrtTlsDialAsync` 同样使用非阻塞发布器，但继续调用旧借用 Dial，
不能把它的配置或 StreamData 语义暗中变成 owned 版本。

## Proxy 与期限

拥有式 Proxy 使用实际 `xrtNetProxyDialOwnedV1`；代理握手完成后，在同一
所属 Worker 上 Attach TLS。TCP／Proxy 阶段使用各自的拥有描述符，不做
句柄强转。嵌套 Proxy 关闭自己的阶段超时，由外层 TLS Dial 的单一总期限
覆盖连接、代理握手和 TLS 握手。Open 先于成功 Done。

原生保持相对毫秒合同；xlang 固定快照保持原来的绝对微秒 Schedule
合同。只同步指定命名段及必要特性依赖，不升级整个时间 ABI。

## 图与 checked 退役

图分别追踪 producer、Owned CancelWatch、实际 Dial、交付 TLS payload
以及独立 owner record。策略必须按永久地址逐项准入；相同内容的策略
副本不能冒充已批准策略。拒绝不改输出 preparation。

准备阶段等待原生源真实完成，不提前伪造 Future 终态。Clear 只断开逻辑
拥有边；Finish 在域外释放资源和用户 Data，保留首次清理错误与已有线程
错误，并支持重复检查。收集器必须让全部目标的 pin 保持到全部 Finish
结束，不能边 Finish 边释放其他目标。

## 验收与边界

证据：`F:/xlang-validation/five-groups-20261024/p6-tls-dial-future`。
正式 r1 冻结原生 2348、xlang 9014 输入。原生 Windows／Linux 分别
504／672 步、39／52 次实际 fixture 执行；固定快照分别 9／12 步、
6／8 次执行通过。新图夹具在上述正式执行中累计 1092 个闭合图／证明、
420 次拒绝、210 次持久清理错误、588 个分配预算中的 504 次失败，内存
台账平衡。覆盖模块化／单头、select／IOCP／epoll、GCC O0／O2、TCC 和
Linux ASan／UBSan。

独立发布夹具覆盖 inline Done、Submit 内等待其他线程 Done、内嵌 NUL
结果、发布前取消，以及真实 TLS 适配层构造返回前完成。普通回环夹具
覆盖直接与 HTTP CONNECT 后的真实 TLS 连接、pull／push、取消和总期限。
这不是畸形协议、密码攻击或安全专项测试。

两端全源码 O2 构建 1156／1155 TU，配置／IR／裁剪、产品回归和 51 个
精确 SDK 原型的实际 TCC 绑定，共 129／131 步通过；接受输入、对象、
产物和日志哈希独立复核。另有 16 项模块选择编译，确认 Dial Future
不再因本批实现引入阻塞 FutureBridge 依赖。

这证明本批原生接口和固定快照，不代表语言 TLS facade 已迁移、当前
整个工作区或发布 DLL 已验收，也不代表完整 TLS／P7 已完成。后续仍需
语言传输迁移、动态回调／ticket／恢复、旧后端清退及发布级跨组收尾。
