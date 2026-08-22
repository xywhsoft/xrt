# 并发、协程与任务选择指南

XRT 把“在哪里执行”“怎样传递数据”“怎样取得结果”和“怎样结束作用域”拆成
可以独立裁剪的层。常见程序不需要直接组合全部原语；先确定工作性质，再选择最浅的
一层即可。

精确函数、返回值、所有权和线程规则以对应 API 文档为准。本指南只说明模块之间的
职责边界和组合方式。

## 先选择执行位置

| 工作 | 首选 | 原因 |
| --- | --- | --- |
| 一个独立的原生执行单元 | [Thread](../api/thread.md) | 明确栈、线程标识、退出码和协作停止 |
| 多个阻塞调用或 CPU 任务 | [TaskPool](../api/task.md#有界任务池) | 有界线程数和队列，统一返回 Future |
| 大量等待型流程 | [Coroutine](../api/coroutine.md) | 等待时挂起当前协程，不阻塞所属线程 |
| 网络事件循环上的短操作 | [Engine Task](../api/task.md#网络-engine-任务) | 保持 Worker 亲和，可直接使用网络上下文 |
| 已有外部执行器 | [Future/Promise](../api/future.md) | 只接入结果，不强迫更换执行器 |

协程不是并行计算单元。一个调度器归属一个线程；协程只有在让出、park、sleep 或
await 时才允许同调度器的其他协程继续运行。阻塞系统调用和长时间 CPU 工作应提交到
TaskPool，不能放进协程或网络 Worker 假装成异步操作。

## 再选择协调对象

### Channel：有界消息流

[Channel](../api/channel.md) 适合生产者和消费者之间连续传递指针消息。容量就是硬性
背压边界：有缓冲 Channel 最多保存指定数量的指针，容量为零时执行同步 rendezvous。

Channel 不拥有指针目标。发送成功后的所有权转移规则必须由上层协议确定；发送失败
时所有权仍在调用方。关闭后禁止新发送，但已经入队的消息仍可按 FIFO 排空。

需要无锁 SPSC、MPSC 或 MPMC 数据结构，而且调用方能够自行处理等待、关闭和对象
生命周期时，直接选择 [Queue](../api/queue.md)。应用层任务管线通常应优先使用
Channel，避免重新实现唤醒、deadline、取消和关闭。

### Future：一个不可变结果

[Future](../api/future.md) 适合一次性操作。它统一携带成功值、结构化错误、取消入口和
最终状态，可以由线程等待、协程 await、延续链、组合器或 TaskGroup 消费。

等待返回 `XWAIT_OK` 只说明 Future 已经结束，不代表操作成功。随后必须检查
`xrtFutureState()` 或 `xrtFutureResult()`。`xrtFutureCancel()` 只发出协作取消请求；
生产端确认取消之前，Future 仍可能成功或失败。

当前契约不再提供公共 `xwaitsrc`、网络专用 wait-source 或另一套任务结果句柄。
不同异步来源直接返回 `xfuture*`，业务层不需要在 bool、网络结果、等待源和 Future
之间反复转换。

### TaskGroup：结构化作用域

[TaskGroup](task-group.md) 跟踪一组 Future，不限制 Future 来自 TaskPool、
协程、网络 Engine 还是用户 Promise。它解决的是作用域，而不是执行：组本身不创建
线程，也不运行用户工作。

- `Close` 停止接纳新项，让已经登记的项自然结束。
- `Cancel` 同时关闭组，并向活动项发出协作取消请求。
- `Wait` 系列先关闭组，再等待活动项归零。
- 子组把父作用域传播到叶操作；父 Close 不等同于父 Cancel。
- `CancelOn` 可以在失败、取消或关闭后停止兄弟项，也可以保持默认的完整收集。

当“启动异步操作”和“登记 Future”不能出现竞态时，使用 `xrtTaskGroupStart()` 或
TaskGroup 与 TaskPool、Coroutine、Network 的组合入口，不要先启动后再手工 Add。

## 共用控制口径

### Deadline

[Wait](../api/wait.md) 定义所有模块共用的单调微秒 deadline：

- `For` 接收相对时长；
- `Until` 接收绝对 `xdeadline`；
- 多次重试时只在循环外调用一次 `xrtDeadlineAfter()`。

等待超时只停止当前等待者，不会自动取消线程、任务、Future、网络操作或任务组。
需要同时停止底层工作时，必须显式请求取消，并继续等待生产端发布真实终态。

### Cancel

[Cancel](../api/cancel.md) 是单向传播的协作令牌。父取消对子级可见，子取消不会反向
污染父作用域。取消回调由请求取消的线程同步执行，只能做短通知，不能在回调中执行
阻塞工作。

取消、超时和关闭是正常控制流，使用 `XWAIT_CANCELLED`、`XWAIT_TIMEOUT` 和
`XWAIT_CLOSED` 表达；只有 `XWAIT_ERROR` 才读取当前结构化错误。任务或 Future 的
失败则保存在自己的终态错误中，不能依赖另一个线程的 `xrtErrorGet()`。

## 推荐组合

### 有界工作队列

生产者通过 Channel 发送拥有明确所有权的任务指针，固定数量的消费者处理并释放。
Channel 容量限制在途工作；关闭后消费者排空队列并退出。需要返回独立结果时，每个
任务携带 Promise，或者直接提交到 TaskPool 取得 Future。

可运行的多生产者、单 worker、逐任务 Future 示例见
`examples/concurrency/worker`。该示例让 Future 直接拥有任务结果，避免任务和结果
分别分配，并明确区分发送前的生产者所有权与发送后的 worker 所有权。

### 阻塞工作与异步编排

把文件、数据库、外部库调用或 CPU 工作提交到 TaskPool。调用线程可以同步等待
Future；协程则使用 `xrtFutureAwait()`，让调度器继续运行其他流程。多个关联结果放入
TaskGroup，作用域结束时统一 Close/Wait 或 Cancel/Wait。

### 网络服务

Engine Worker 只做协议推进、状态转换和短回调。阻塞或重 CPU 工作进入 TaskPool，
结果通过 Future 返回；连接、请求或会话级生命周期由 TaskGroup 收束。持续消息流使用
Channel 时必须设置有意义的容量，不能用无界堆积掩盖慢消费者。

### 请求路径与后台编排

HTTP 请求回调只复制后台工作真正需要的输入，提交任务并绑定 Future；不要在网络
Worker 中同步执行冷数据恢复、外部进程、批量文件 I/O 或模板渲染。服务端可以用
`xrtHttpConnRespondFuture()` 接收普通任务 Future，也可以在固定响应路径直接提交
预封装响应，不要求业务先构造完整对象图。

复杂恢复流程按四层拆分：选择来源、生成不可变计划、执行计划、原子发布结果。来源
可能是内存快照、冷文件、资源包或外部工具，但来源选择只产生计划，不应同时修改线上
状态。执行阶段使用 TaskPool、File Async 或 Process；外部工具只写 staging 路径，
成功校验后再用文件原子写入或同卷替换发布，不能直接覆盖正在服务的快照。

页面、API、日志和缓存键应从同一份不可变结果模型派生。发布新模型时一次交换拥有型
引用，旧请求继续读取旧模型，避免多个展示路径分别维护容易漂移的可变状态。持续刷新
用有界 Channel 传递意图，一次性恢复用 Future 返回结果，整轮刷新由 TaskGroup 负责
取消和收口；不要再为这一组合发明专用队列、完成标志或错误通道。

热态、冷态和归档是应用状态，不是 Map、List 或文件 API 隐含的语义。状态迁移必须
失败原子：先建立完整的目标值，再提交目标索引，最后移除源索引；任何分配、复制或
校验失败都保留原状态。逻辑 key、路径和标题使用明确长度值，不能先截断进固定数组再
索引；非密码 Hash 只适合分片、查找或日志指纹，不能作为身份或授权凭据。

同一对象需要按 ID、名称或令牌建立多套索引时，只指定一个权威所有者；其他索引只
保存借用指针，不能各自释放对象。插入前先完成对象和全部拥有字段，再在短锁内逐项
提交索引，任一步失败都按相反顺序回滚。删除时先摘除全部二级索引，再移除权威入口并
释放对象。连续整数选择 Array，稀疏整数选择 IntMap 或对应类型容器，任意字节键选择
Map，需要稳定顺序或范围查询时选择 AVL；不能因为旧版某个容器曾承担整数映射，就把
它的历史语义套到当前侵入式 List 上。

如果删除、快照、迁移和有序遍历都天然以租户或分片为边界，把局部索引嵌入对应的
bucket，让 bucket 拥有其局部容器；这样整组资源可以一起发布和销毁。只有工作负载
主要是跨分片精确查询、没有局部生命周期时，全局复合键才可能更直接。两种布局都应
从真实访问与收口边界选择，不能把 Dict、Map 或 AVL 中任一种规定为所有层的默认答案。

对象来自 Pool 或 MemPool 时，正常删除仍使用显式 Remove 和 Free。只有导入、恢复或
批量构建可能留下未提交孤儿时，才从全部权威根出发对对象及其池内拥有字段调用 Mark，
最后执行 Sweep。池不会寻找根或遍历对象图，Sweep 也不是日常生命周期管理的替代品。

共享状态锁只保护引用、索引和版本交换，不能包住 JSON 序列化、模板渲染、外部进程
或文件 I/O。发布快照时，在短锁内取得不可变模型引用和 generation，解锁后生成候选
内容并用原子文件入口写入。写入失败时磁盘上的上一代快照仍然有效，并由后台策略记录
脏状态或重试；内存变更与文件替换不是一个跨介质原子事务。多个发布者必须在提交前
检查 generation，或者由单个有界 worker 串行发布，避免旧快照覆盖新快照。

后台完成必须由 Future、Channel 关闭或线程 Join 确认，不能用固定 Sleep 猜测任务已
完成。Future 成功值发布后可以立刻被其他线程释放，因此生产者不得再次访问已转移的
对象；失败路径则继续拥有对象，并负责回滚或释放。

各层可运行证据分别见 `examples/concurrency/worker`、`examples/process/capture`、
`examples/file/async_manage`、`examples/http/server_future` 和
`examples/template/compose`。原子快照和同一模型派生 JSON/HTML 的组合分别见
`examples/file/report` 与 `examples/http/reply_json_template`。这些示例刻意保持独立，
应用只组合所需层，不会因为采用后台编排而强制引入 HTTP、模板或外部进程。

### 外部异步来源

保留原执行器，通过 Promise 在完成回调中发布结果。成功值的借用或拥有语义必须在
创建点决定；回调线程不应把线程局部错误当作跨线程结果。需要取消时，把 Future 的
取消令牌接入外部执行器能够理解的停止机制。

## 生命周期顺序

稳定的关闭顺序是：停止新增工作，关闭输入，等待或取消活动工作，读取终态，最后释放
消费者和执行器。不要先销毁 Channel、TaskPool、调度器或网络 Engine，再期待尚未完成
的回调自行处理悬空对象。

Future 的生命周期独立于产生它的 TaskPool；已经持有的 Future 可以在池销毁后继续
读取。借用结果仍必须满足生产者声明的生命周期。TaskGroup 销毁会取消活动项，但不会
伪造它们已经完成；真实生产端仍要确认终态并释放资源。

## 常见错误

- 用强制终止线程代替协作停止，跳过锁、临时内存和资源清理。
- 在协程或网络 Worker 中调用阻塞 API，导致同线程全部连接停止推进。
- 把 Future 等待成功当成操作成功，不检查 Future 终态。
- 把等待超时当成底层操作已经取消，随后释放仍被使用的数据。
- 使用无界队列吸收过载，直到延迟和内存同时失控。
- Channel 发送成功后，发送端和接收端都释放同一个指针目标。
- 先启动异步操作再登记 TaskGroup，留下关闭或取消窗口。
- 在循环内反复把相对超时转换为新 deadline，导致总等待时间无限延长。

## 阅读和范例

- 基础线程和同步：`examples/concurrency/thread`、`sync`、`condition`、`semaphore`。
- 消息与多路等待：`examples/concurrency/channel`、`channel_select`、
  `channel_coroutine`。
- 一次性结果：`examples/concurrency/future`、`future_continue`、
  `future_combine`、`future_coroutine`。
- 协程生命周期：`examples/concurrency/coroutine_lifecycle`、
  `coroutine_scheduler`、`coroutine_event`。
- 任务作用域：`examples/concurrency/task_pool`、`task_group`、
  `task_group_scope`、`task_group_pool`、`task_group_coroutine`、`report`。
- 持续任务交接：`examples/concurrency/worker`。
- 网络组合：`examples/network/task`、`task_group`。

全部范例都由模块清单登记并参与对应编译器回归；入口索引见
[可运行示例](../EXAMPLES.md)。
