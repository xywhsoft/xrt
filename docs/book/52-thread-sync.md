---
num: 52
slug: thread-sync
title: 线程与同步原语
volume: 卷六 进程与并发
type: practice
lead: 线程创建与汇合、互斥/条件/信号量/读写锁四件套、once 可失败初始化——并发的地基。
api: thread, sync, atomic
---

## 导读

卷六从最底层开工：**线程**（`xrtThreadCreate/Wait`——创建、运行、汇合的三步生命周期）与**同步原语四件套**（Mutex 互斥、Condition 条件、Semaphore 信号量、RWLock 读写锁），外加第 5 章（金标准 ch）预告过的 **once 可失败一次初始化**。本章的立场是"原语层"——它们是并发体系的原材料，绝大多数业务代码不该直接碰原语（那是 Channel/Future/任务组的领地），但理解原语是理解上层封装的前提，也是诊断并发问题的最后落点。

## 引入

三个朴素问题开路。问题一：两个线程数同一个计数器各加一万次，结果为什么不是两万——`count++` 是读-改-写三步，两个线程的三步可以交错；Mutex 把这段变成"一次只能一个人进"。问题二：消费者线程怎么等生产者的数据——自旋轮询烧 CPU、sleep 轮询延迟高；Condition 是"睡到有人叫你"的机制。问题三：配置初始化耗时且可能失败，多个线程都要用——每个线程都试一遍初始化是浪费且竞态；once 保证"恰好一个线程执行、失败可重试、其他线程等结果"。

三个问题对应三组答案，也划出原语的分工：**Mutex 管互斥**（共享数据一次一人）、**Condition 管等待**（条件满足前睡觉）、**Semaphore 管配额**（同时最多 N 人）、**RWLock 管读写 asymmetry**（读共享写独占）。选错原语的代价是死锁或性能塌方——本章坑区有两类实例。

## 概念

### 线程生命周期

```diagram flow
- 创建：xrtThreadCreate(过程, 数据, 栈大小) → 句柄（栈大小 0 用默认）
- 运行：过程签名 xthreadproc——返回值即线程结果（int）
- 汇合：xrtThreadWait(句柄) → 取回结果并释放——不 Wait 则泄漏
- 辅助：Current/Yield/CurrentId——自省与让出（第 64 章 TCP 示例用过 Yield）
```

`Wait` 的纪律是本章第一条铁律：**创建的每个线程恰好 Wait 一次**——不 Wait 是资源泄漏，Wait 两次是未定义。线程的返回值（`exit: 42` 输出来源）经 Wait 取回，是线程间最原始的结果传递。

### 四件套原语

| 原语 | 语义 | 标准场景 |
| --- | --- | --- |
| Mutex | 一次一人；Lock/Unlock；TryLock 非阻塞试探 | 共享数据结构的任何修改 |
| Condition | Wait 睡到 Signal/Broadcast；**与 Mutex 配对** | 生产者-消费者的"有数据了" |
| Semaphore | 计数配额；Wait 扣减 Post 补充 | 连接池上限、并发数控制 |
| RWLock | 读共享写独占；多读单写 | 配置读多写少 |

四件套的家族形态统一：栈上初始化（Init/Unit）或堆创建（Create/Destroy）两种生命周期、Try 非阻塞变体、等待类支持 deadline（第 41 章 xtime 衔接——`WaitFor/WaitUntil` 带超时）。**Condition 的配对纪律**：Condition 必须与一个 Mutex 配合——Wait 前持有锁、Wait 内部原子地"放锁+入睡"、醒来重新拿锁（这是它能正确工作的机制核心，忘记配对是未定义行为）。

### once：可失败的一次初始化

第 5 章金标准讲过 once 的可失败语义（`bool (*xonceproc)(ptr)`——返回 false 可重试、`XERR_STATE` 防递归）。本章补上它在并发版图的定位：**once 是"初始化路径的 Mutex 替代"**——比 Mutex 更便宜（无锁快路径）且语义更准（"恰好一次"而不是"互斥访问"）。配置加载、全局资源构造、惰性单例全部是它的领地。

### 死锁四条件与预防

死锁的发生需要四条件同时成立：互斥、持有并等待、不可剥夺、循环等待。工程预防打断后两个中最容易的：**锁序**——全程序约定锁的获取顺序（如"先 A 后 B"），循环等待结构性消失；**TryLock 升级**——拿第二把锁用 Try，失败则放掉已持有的重来（打破持有并等待）。四件套层面的最后手段是**超时**——Wait 带 deadline，超时诊断（死锁检测的运行时形态）。

## 示例

### 完整程序：线程创建与汇合

来自仓库范例 `examples/concurrency/thread/main.c`：

```embed path="examples/concurrency/thread/main.c" title="examples/concurrency/thread/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/thread/main.c -lws2_32 -liphlpapi
worker alpha: 13300
exit: 42
```

**刚才发生了什么。** ① ① `xrtThreadCreate` 的参数三件：过程（`xthreadproc` 签名——`int32(ptr)` 返回）、数据（`ptr` 透传——本例是名字字符串）、栈大小（0 用平台默认；需要深递归时显式给）。② 工作线程打印名字与线程 ID（`13300`——随运行变化）；③ 主线程 `xrtThreadWait` 汇合——`xwaitresult` 报告等待结果；线程的返回值（本例 42）经线程局部约定或 Future（第 57 章）回传，`exit: 42` 是范例自己从原子变量取的——**Wait 负责汇合不负责取值**；④ Wait 后句柄已释放，不需要（也不能）再 Destroy。

### 完整程序：互斥保护临界区

来自 `examples/concurrency/sync/main.c`：

```embed path="examples/concurrency/sync/main.c" title="examples/concurrency/sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/concurrency/sync/main.c -lws2_32 -liphlpapi
protected section
```

**刚才发生了什么。** ① Mutex Init 栈上初始化（零分配——与堆版 Create/Destroy 的选择同容器约定）。② Lock/Unlock 之间是临界区——两个线程对同一 Mutex 的 Lock 互斥，临界区代码如入无人之境（`protected section` 恰好打印一次，未被撕碎）。③ sync_tour 范例是四件套的全接口巡礼：Mutex 的 Try（试探拿锁）、Condition 的 Wait/WaitFor（带超时等待与 Signal 配对）、Semaphore 的配额语义（`post-many=3` 后三次 Wait 全过）——四行 ok 对应四件套。condition 与 rwlock 范例分别展开条件等待与读写锁的完整用例。

## 契约

- **线程配平**：每线程恰好一次 Wait（返回 xwaitresult）；不 Wait 泄漏、双 Wait 未定义；返回值经原子或 Future 回传，Wait 只管汇合。
- **临界区最小**：Lock 与 Unlock 之间只放必须互斥的代码——IO、日志、分配尽量出区。
- **Condition 配对**：必须与 Mutex 配合；Wait 前持锁；Signal 用 Broadcast 的场合（多消费者）显式选择。
- **锁序纪律**：多锁程序全程序约定获取顺序；或 TryLock 升级策略——二选一并写进模块文档。
- **once 语义**：恰好一次 + 失败可重试 + `XERR_STATE` 防递归（第 5 章契约原样生效）。
- **超时诊断**：等待类原语带 deadline——超时是死锁的运行时信号，不该被静默重试。

### 原语层定位：什么时候不用四件套

开讲原语就要讲清"什么时候别用它"——业务代码直接碰原语的三个信号。**信号一：你在用 Mutex 保护"任务交接"**——两个线程传数据用锁+条件变量手工拼，这正是 Channel（第 56 章）的领地：队列+唤醒+背压一步到位。**信号二：你在用线程+Wait 拼"异步结果"**——创建线程只为算个值再 Wait 取回，这是 Future/任务池（第 57/58 章）的领地：提交即返回 Future、池化复用线程。**信号三：你在用 Semaphore 控制"并发度"**——限流场景用信号量能跑，但任务池的队列深度天然就是并发上限（还带背压）。三个信号的共同判据：**原语解决"怎么同步"，上层解决"怎么协作"**——写协作逻辑时手握原语，说明正在重造上层。原语的正当直用场景：保护纯数据结构（缓存、计数器）、性能关键路径（锁的开销可见）、与外部线程库对接的边界。

### 与第 21 章队列、第 9 章原子的分工

并发数据访问的三层工具箱容易混淆，一张对照讲清。**原子操作**（第 9 章）：单变量读写——计数器、标志位、指针；无锁但只覆盖单变量。**Mutex/四件套**（本章）：多变量的不变式维护——"两个字段必须一起改"这类约束原子做不了，锁来。**无锁队列**（第 21 章）：SPSC/MPSC/MPMC 的指针传递——队列内部无锁，你不用加锁。三层的选型由"共享的形状"决定：单变量→原子、不变式→锁、传指针→队列。混层的典型错误是给队列外面再包 Mutex（队列已经线程安全）或给计数器上锁（原子更便宜）——层级意识与第 23 章容器选型同构。

### 一个测试观：并发的确定性测试

并发代码"跑一万次才偶现一次"的性质让测试变成玄学。三个工程化手段。**压到位**：竞态窗口在压力下才张开——测试用满线程数 + 人为让步（Yield 注入交错）+ 长时间跑（CI 夜间任务）。**降维断言**：不定时的中间态不可断言，断言只落在汇合点之后（全部 Wait 完再做全局检查）——"最终一致"比"每步一致"可测得多。**工具加持**：TSan 类消毒器在 CI 特定 lane 跑（代价慢但抓真竞态）；第 6 章故障注入给锁路径的 OOM 场景补位。三手段合起来的现实期望：**并发测试证明"没发现问题"而不是"没有问题"**——定位依然靠第 49 章的观测管线（时间戳微秒级的日志重排竞争现场）。

## 避坑

### 坑 1：忘 Wait 或双 Wait

症状：前者——线程资源泄漏（第 6 章统计可见）；后者——偶发崩溃或返回值错乱。

原因：线程句柄是"必须汇合的承诺"——创建即欠一次 Wait；Wait 是承诺的兑现，第二次是重复兑现。

```c bad
xthread* Worker = xrtThreadCreate(work, NULL, &Config);
/* ... 主线程退出，没有 Wait——Worker 资源泄漏；更糟：进程退出时线程还在跑 */
```

```c good
xthread* Worker = xrtThreadCreate(work, NULL, 0);
if ( xrtThreadWait(Worker) != XWAIT_OK ) {   /* 恰好一次 */
	return false;
}
/* 线程返回值经原子/Future/队列回传——Wait 只管汇合 */
```

### 坑 2：锁序颠倒致死锁

症状：偶发双双卡死——线程 1 持 A 等 B、线程 2 持 B 等 A；低并发不现、压力测试偶现、生产必现。

原因：两处代码以相反顺序拿同一对锁——循环等待的四条件凑齐。

```c bad
/* 线程 1 的代码 */          /* 线程 2 的代码 */
xrtMutexLock(&A);            xrtMutexLock(&B);
xrtMutexLock(&B);            xrtMutexLock(&A);   /* 顺序相反——死锁窗口 */
Transfer(A, B);              Transfer(B, A);
xrtMutexUnlock(&B);          xrtMutexUnlock(&A);
xrtMutexUnlock(&A);          xrtMutexUnlock(&B);
```

```c good
/* 全程序约定：同时需要 A、B 时，永远先 A 后 B（写进模块文档与评审清单） */
/* 线程 1 与线程 2 都改为： */
xrtMutexLock(&A);            /* 先 A——约定 */
xrtMutexLock(&B);            /* 后 B */
Transfer(A, B);
xrtMutexUnlock(&B);
xrtMutexUnlock(&A);
```

## 练习

### 基础：计数器竞态实证

两线程各加一百万次的无锁计数器 vs Mutex 保护版——结果对比（无锁版每次运行不同且小于两百万）；再加原子操作版（第 9 章）三方对照。

### 进阶：生产者消费者

Condition + Mutex 实现有界队列的生产消费：生产者在满时等待、消费者在空时等待、各自唤醒对方——双条件变量的标准练习（用第 21 章队列做底座）。

### 挑战：读写锁基准

同一份读多写少的数据（读 90%/写 10%）分别用 Mutex 与 RWLock 保护，四线程并发跑十万次操作——对比吞吐。验收标准：RWLock 版吞吐显著高于 Mutex 版（读共享的收益）；写饥饿不出现（或用写优先策略消除）；数字写进对照表。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 线程三步 | Create（过程+数据+配置）→ 运行 → Wait（取结果+释放，恰好一次） |
| 四件套 | Mutex 互斥 / Condition 配对等待 / Semaphore 配额 / RWLock 读共享写独占 |
| 生命周期 | Init/Unit 栈上零分配 或 Create/Destroy 堆上——与容器约定一致 |
| 超时族 | WaitFor/WaitUntil 带 deadline——超时是死锁信号不是重试信号 |
| once | 恰好一次+可失败重试+防递归——初始化路径的 Mutex 替代 |
| 死锁预防 | 锁序约定 或 TryLock 升级——二选一写进文档 |
| 临界区纪律 | 最小化——IO/日志/分配出区 |
