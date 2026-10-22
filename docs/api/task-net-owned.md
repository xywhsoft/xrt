# TaskNet 显式拥有型任务

`xrtTaskNetDataPolicyV1` 立即提交；`xrtTaskNetAfterDataPolicyV1` 和
`xrtTaskNetForDataPolicyV1` 按相对毫秒调度（-1 为无限等待）。
旧借用式接口保持原合同，不因新增接口而获得拥有图认证。

`xtasknetdataownershipv1` 描述唯一用户 Data 引用：Proc 借用当前 Worker、
取消令牌与 Data；Drop(Data,NULL) 消费该引用；Ops 描述同一物理对象的真实
引用数和强拥有槽。描述符和回调代码必须驻留到所有拥有图 pin 释放之后。
回调不得阻塞 Engine Worker，不新增线程或协程栈。

提交失败不消费 Data；成功后 Data 在 Future 发布终态之前恰好释放一次。
成功结果独立使用 `xfuturepayloadownershipv1`；非空借用结果不冒充拥有结果。
Engine 受理之前完成 Job、结果端点、取消 Watch 的可失败装配；成功后不再
出现“已接受工作但返回空”的装配失败。预先取消会在真实 Timer ID 发布后重放。

一个物理 Frame 由 Job Data、Post/Timer 和取消 Watch 分别真实持有。Frame
拥有 Job 的执行器引用、Engine 的计数外壳、用户 Data 和 Watch；Bridge 借用
Job 的 Promise，不重复计算端点。外壳引用不增加 LiveObjects，不阻止 Engine
关闭和排空。取消路径在锁内取得独立外壳引用，在锁外提交取消并释放。
用户 Proc/Drop、等待和 Promise 通知不在 Frame 锁或 mutation 区间内执行。

`xrtTaskNetOwnershipAdapterV1` 只接受显式登记的同一 Data policy 身份。
它不按同形描述符、名称或猜测授权用户 Count/Trace；未知策略、发布中或活动
记录拒绝。Prepare 只观察完成，不另造取消或停止行为。
Post、Timer、Job Data、取消 Watch 和 quiescence 的驻留 policy getter
供封闭拥有目录登记使用，不是语义结果，也不制造额外图节点。

`tests/network/test_task_net_owned.c` 登记于 task_net_oom_tests，覆盖 64 个
分配预算（12 个实际失败点）、16 次预先取消、16 次未启动 Engine 拒绝，
以及用户回调/析构中由另一线程成功取得 freeze。逐场景核对内存账本，
不创建应用 Socket、DNS、网络流量或网络安全测试。
xlang 另将同一底座定点适配到其固定微秒/Until 快照；不改变本库毫秒/For。
语言 generic profile、TaskGroup 拥有型组合和 NetFile 另行接入。

