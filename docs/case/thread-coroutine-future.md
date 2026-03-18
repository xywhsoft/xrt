# 线程、协程与 Future 协同模型

> 这个案例不追求“最短代码”，而是解释 XRT 当前主线里三种并发能力应该如何分工，以及为什么这样组合更稳。

[返回案例索引](README.md)

---

## 1. 场景

一个现实中的程序往往同时需要三类能力：

- 线程：把 CPU 密集型任务或独立阻塞任务分散到多个执行单元
- 协程：在单线程内把大量异步步骤写成顺序代码
- future / task / promise：统一表达“一个未来会完成的结果”

如果三者没有统一关系，程序就会变成：

- 有些任务靠回调
- 有些任务靠线程等待
- 有些任务靠手工 event
- 有些任务根本不知道该怎么组合

XRT 当前主线的设计目标，就是让这三者说同一种异步语言。


## 2. 推荐分工

### 线程负责什么

适合线程处理的工作：

- CPU 密集计算
- 独立阻塞调用
- 不适合塞进单线程事件循环的任务

在线程层，推荐用：

- `xTaskRunThread()`

这样线程执行结果会自动进入 `xfuture`，后续不需要再单独发明回调协议。

### 协程负责什么

适合协程处理的工作：

- 单线程内的多步骤异步编排
- 需要顺序表达的等待链
- `wait-source / future / timer / event` 组合等待

在线程已经附加到 runtime 的前提下，协程里可以自然写：

```c
xfuture pFuture = xTaskRunThread( procTask, pArg );
	xFutureWaitCo( pFuture );
```

### Future 负责什么

future 负责统一这三种结果表达：

- 线程任务结果
- 协程任务结果
- engine / delayed task 结果

因此 future 不是“附属工具”，而是三者协同的中轴。


## 3. 一个典型链路

下面是当前主线里很推荐的一种思路：

1. 主线程进入 XRT runtime
2. 启动一个协程做编排
3. 协程里发起多个 task
4. task 可能跑在线程、engine worker、或另一个协程
5. 它们统一返回 future
6. 协程等待这些 future，最后拼装结果

对应的思路大致是：

```c
/* 主线程 */
xrtInit();

/* 协程负责编排 */
/* 线程或 engine 负责执行 */
/* future 负责承接结果 */

xrtUnit();
```

这比“线程直接互相回调 + 协程手工收 event + 网络层自己发状态”要清晰得多。


## 4. 为什么不建议乱用线程

很多程序一开始会倾向于：

- 每个任务都开线程
- 每个网络连接都开线程
- 每个等待都用条件变量

这种做法在小程序里能跑，但随着逻辑变复杂，会很快遇到：

- 线程数量难控
- 生命周期变乱
- 结果传播不统一
- timeout / cancel 难以组合

因此在 XRT 当前主线里，更推荐：

- 线程只承担确实需要独立执行的工作
- 编排层尽量交给协程
- 结果统一交给 future


## 5. 为什么不建议让协程直接承担一切

协程很强，但它不适合替代一切并发模型。

不适合协程独自承担的场景包括：

- 真正的 CPU 密集计算
- 必须脱离当前调度线程的工作
- 长时间阻塞且无法 cooperative 的外部调用

这时，最自然的做法是：

- 把实际执行交给 thread task 或 engine task
- 让协程只负责等待和组合结果


## 6. 当前主线推荐写法

### 6.1 线程任务统一走 `xTaskRunThread()`

这样做的好处是：

- 自动返回 `xfuture`
- 后续可以给线程、协程、组合 future 统一等待
- 结果与错误模型保持一致

### 6.2 协程里统一等待 future / wait-source

当前主线已经支持：

- `xFutureWaitCo*`
- event wait
- deadline wait
- 与 `xnet` wait-source 的桥接

因此协程最适合承担“流程 orchestration”。

### 6.3 多个子任务统一进 task group

如果一批子任务属于同一个逻辑 scope，推荐：

- `xTaskGroupRunThread()`
- `xTaskGroupRunEngine()`
- `xTaskGroupRunCo()`
- `xTaskGroupJoin*()`

这样可以得到：

- close-aware dynamic join
- close / reap / destroy 的统一合同
- parent cancel -> child cancel 的传播


## 7. 一个推荐的心智模型

可以把它们理解成三层：

- 线程：执行层
- 协程：编排层
- future：结果层

当你不确定该用哪一个时，通常可以先问自己：

1. 这段工作是“真的要独立执行”，还是“只是要等待结果”？
2. 我现在缺的是“执行能力”，还是“等待和组合能力”？
3. 这个结果后面是否还要被其他模块继续等待、组合、取消？

如果答案更偏“结果和组合”，那通常就应该优先考虑 future，而不是直接堆线程或手工事件。


## 8. 常见错误

### 错误一：线程函数直接向外部写一堆共享状态

更推荐：

- 让线程返回 future 结果
- 由等待者统一读取

这样能减少锁和生命周期混乱。

### 错误二：协程直接承担 CPU 密集型工作

协程擅长的是等待与编排，不是替代线程。

### 错误三：每个子任务都各写一套 callback 协议

如果任务最终都能归结成“将来完成”，就应该优先统一进 future。


## 9. 建议继续阅读

- [Thread API](../api/api-thread.md)
- [Coroutine API](../api/api-coroutine.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [从零开始写第一个 XRT 程序](../guide/first-xrt-program.md)
- [XRT 运行时与线程附加入门](../guide/runtime-thread-attach.md)

---

**一句话总结：** 在线程、协程、future 三者里，线程负责执行，协程负责编排，future 负责统一结果；一旦这三层分工清楚，程序结构会比“线程到处乱飞、回调到处乱散”稳定很多。
