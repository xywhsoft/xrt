# 协程、Future 与 Task 入门

> 目标：理解为什么 XRT 不把线程、协程、future、task 做成互相割裂的几套体系，以及当前主线里推荐怎样组合它们。

[返回教学文档](README.md)

---

## 1. 先抓住 3 个角色

在 XRT 当前主线里，可以先把这三个概念分开理解：

- 协程：负责编排
- future：负责结果
- task：负责把一段执行逻辑变成 future

它们不是三套彼此竞争的模型，而是三层分工。


## 2. 为什么不能只学协程

很多人一看到协程，就会下意识地把所有异步问题都交给协程。

但协程真正擅长的是：

- 顺序表达等待链
- 在单线程里组织多步异步流程
- 让代码看起来像同步逻辑

它不擅长直接承担：

- CPU 密集执行
- 真正需要独立执行单元的工作
- 各种结果对象的统一表达

所以，协程本身不应该承载一切。


## 3. future 解决的是什么问题

future 的意义是：

> 不管结果来自线程、协程、engine worker、延迟任务还是网络等待，都统一表示成“一个未来会完成的结果”。

这会带来 3 个直接好处：

- 等待接口统一
- timeout / deadline / cancel 语义统一
- 组合等待变得容易

例如：

- `WhenAny`
- `WhenAll`
- `Race`
- `task group`

都建立在 future 之上。


## 4. task 解决的是什么问题

task 的意义是：

> 把“执行一段逻辑”标准化成“生成一个 future”。

所以 task 不只是“任务对象”这么简单，它更像 future 的生产入口。

当前主线已经支持：

- `xTaskRunThread()`
- `xTaskRunCo()`
- `xTaskRunEngine()`
- `xTaskRunDelayed()`

这意味着：

- 线程执行可以统一进 future
- 协程执行可以统一进 future
- engine worker 执行也可以统一进 future


## 5. 推荐的心智模型

初学时，建议把当前主线记成下面这张图：

- task：发起执行
- future：承接结果
- 协程：等待和编排

也就是：

1. 用 `xTaskRun*()` 发起工作
2. 得到 `xfuture`
3. 在协程里 `xFutureWaitCo*()`
4. 再决定是否继续 `Then / Catch / Finally`


## 6. 最基础的用法

### 6.1 启动一个线程任务

```c
xfuture pFuture = xTaskRunThread( procTask, pArg );
```

这时：

- 工作在线程里执行
- 结果会统一进入 future

### 6.2 在协程里等待它

```c
xFutureWaitCo( pFuture );
```

这样协程就能像写同步代码一样，把线程结果接回来。

### 6.3 用 continuation 继续处理

```c
xfuture pNext = xFutureThenCurrent( pFuture, procThen, NULL );
```

这时可以继续把结果往后传，而不是每一步都手工拼状态机。


## 7. 为什么这比“线程 + 回调”更好

如果没有 future / task 这条主线，程序通常会变成：

- 线程各自有一套回调协议
- 协程再自己接一套 event
- 网络层再维护一套 async result

这样后面会越来越碎。

而统一成 task -> future -> wait/continuation 之后：

- 线程和协程不再说两种语言
- 网络和非网络任务也更容易统一
- timeout / cancel / group join 都能往同一个核心上收


## 8. 当前主线推荐写法

### 推荐

- 执行层优先走 `xTaskRun*`
- 结果层优先走 `xfuture`
- 编排层优先走协程

### 不推荐

- 每个模块自己发明一套“异步完成通知”
- 线程任务直接把结果写进共享状态，再靠外部轮询
- 协程里手工拼一堆 event / flag / callback


## 9. task group 的意义

一旦多个 future 属于同一个逻辑 scope，推荐交给 `task group`：

- `xTaskGroupRun*`
- `xTaskGroupJoin*`
- `xTaskGroupClose`
- `xTaskGroupReapCompleted`

这样你会得到：

- scope 内任务统一管理
- close-aware dynamic join
- parent cancel -> child cancel 传播
- nested scope 的扩展能力


## 10. 建议继续阅读

- [Coroutine API](../api/api-coroutine.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)

---

**一句话总结：** 在 XRT 当前主线里，task 负责发起执行，future 负责承接结果，协程负责等待与编排；三者组合起来，才是推荐的现代异步开发方式。
