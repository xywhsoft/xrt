# XRT Future / Task / Promise 阶段状态

版本: `0.1.0`
状态: `active`
更新时间: `2026-03-17`



## 1. 当前结论

`future / task / promise / wait-source` 这条正式异步主线已经从“设计稿阶段”进入“核心运行时已成型、具备正式回归”的状态。

当前已经不是实验性桥接代码，而是 XRT 运行时的一条正式基础主干。

从能力面来看，已经具备：

- 统一异步对象：`xfuture / xpromise / xtask / xwaitsrc`
- 统一执行模型：`thread / coroutine / engine / delayed`
- 统一 continuation：`inline / current-thread deferred / engine / coroutine scheduler`
- 统一组合模型：`WhenAny / WhenAll / Race`
- 统一 structured concurrency 雏形：`task group / close / reap / destroy / join / parent bind / nested scope`
- 统一 ownership 合同：`Get* = 持有引用`，`Peek* = 借用引用`
- 统一元数据：`create time / complete time / debug name / pending continuation count`



## 2. 已完成能力

### 2.1 核心对象

已完成：

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`
- `xfuture_state`
- `xfuture_result`
- `xfuture_all_value`


### 2.2 基础 future / promise API

已完成：

- future 创建、引用、释放、状态、结果、错误
- promise 创建、resolve、reject、cancel、close
- wait-source 与 future / stream / dgram / event 的桥接
- `Get* / Peek*` 双轨 ownership


### 2.3 执行模型

已完成：

- `xTaskRunThread`
- `xTaskRunCo`
- `xTaskRunEngine`
- `xTaskRunDelayed`

当前这 4 类执行模型都能统一返回 `xfuture`。


### 2.4 Continuation

已完成：

- `Then / Catch / Finally`
- `ThenInline / CatchInline / FinallyInline`
- `ThenCurrent / CatchCurrent / FinallyCurrent`
- `ThenEngine / CatchEngine / FinallyEngine`
- `ThenCo / CatchCo / FinallyCo`
- `xFuturePumpCurrentContinuations`

并且：

- `current-thread deferred` 已接入同步 `xFutureWait*` 自动 pump
- 调用方不再必须手工 pump 才能推进 child continuation


### 2.5 组合 future

已完成：

- `xFutureWhenAny`
- `xFutureWhenAll`
- `xFutureRace`

已完成的 richer result 合同：

- `WhenAny / Race` 暴露 `group source index / group source future`
- `WhenAll(first failure)` 暴露失败源 `index / source future`
- `WhenAll(success)` 返回 `xfuture_all_value`


### 2.6 Structured Concurrency

已完成：

- `xTaskGroupCreate / Destroy / Close`
- `xTaskGroupAddFuture / Count / ReapCompleted`
- `xTaskGroupWait / WaitTimeout / WaitUntil`
- `xTaskGroupJoinFuture / Join / JoinTimeout / JoinUntil`
- `xTaskGroupCancel`
- `xTaskGroupRunEngine / RunDelayed / RunThread / RunCo`
- `xTaskGroupBindParent`
- `xTaskGroupCreateChild`

当前已完成的 scope 语义：

- group destroy 时自动 `Close + Cancel pending children + Release hold`
- join 已升级为 `close-aware dynamic join`
- 支持多 parent bind
- nested child group 已支持：
	- 自动绑定 parent scope
	- child join 自动纳入 parent scope
	- parent `Close/Cancel/Destroy` 向 child 传播 `Close + Cancel`


### 2.7 元数据与可观测性

已完成：

- `xFutureSetDebugName`
- `xFutureGetDebugName`
- `xFutureGetCreateTimeMs`
- `xFutureGetCompleteTimeMs`
- `xFutureGetPendingContinuationCount`
- `xFutureGetGroupSourceIndex`
- `xFutureGetGroupSource`
- `xFuturePeekGroupSource`
- `xFuturePeekAllValue`
- `xFutureGetAllValueCount`
- `xFutureGetAllValueItem`



## 3. 已完成测试覆盖

### 3.1 独立 core harness

文件：

- [test_future_core.c](/D:/git/xrt/dev/test_future_core.c)

当前已覆盖：

- future / promise 基础合同
- thread task
- coroutine task
- continuation
- multi continuation / multi waiter
- combinators
- task group
- task group run
- task group scope
- dynamic join
- parent cancel
- nested scope
- continuation stress
- task group batch
- combinator race stress
- parent multi-cancel stress
- destroy pending child
- add after close
- ownership lifetime


### 3.2 主测试流嵌入

已完成：

- [test_future_core.h](/D:/git/xrt/test/test_future_core.h)
- [test.c](/D:/git/xrt/test.c) 已嵌入同一份 core harness
- `Test_FutureCore(xCore)` 已纳入默认主测试流执行

当前不再只是独立 harness，而是已经进入默认主测试基线。



## 4. 本轮已修掉的关键实现问题

### 4.1 组合 future 并发完成竞态

已修复：

- `WhenAny / Race` 在 source callback 并发完成时，`group source index/source future` 可能漂移
- group ctx 可能在并发 callback 间被过早释放

修复方式：

- 给组合 group ctx 增加内部锁
- 给组合 group ctx 增加 callback 级引用计数
- 改成“最后一个 source callback 才释放 group ctx”


### 4.2 current-thread continuation 自动推进

已修复：

- `ThenCurrent` child future 过去必须显式 `xFuturePumpCurrentContinuations()`

当前：

- 同步 `xFutureWait / WaitTimeout / WaitUntil` 主路径会自动 pump 当前线程 deferred queue


### 4.3 scope / ownership 历史包袱风险

已修复或已收口：

- `Get* / Peek*` 合同已明确
- `join future` 不再是一次性 snapshot
- parent bind 不再限制为单 parent
- nested scope 不再靠外部手工拼接



## 5. 当前仍未达到“完全生产级完成”的点

这部分不是“已经错误”，而是仍需继续收口的生产级事项。

### 5.1 更重的压力型与长时型验证还不够

当前已有压力回归，但还不够覆盖：

- 更大规模 source count 的 combinator 压力
- 更大规模 child count 的 task group 压力
- 长时间 soak test
- mixed engine/thread/co 混合编排长时运行


### 5.2 文档与公开使用指南尚未补齐

当前有设计稿和状态文档，但还没有形成最终 public 使用说明，例如：

- 推荐使用模式
- ownership 快速规则
- scope 推荐写法
- combinator 结果合同说明


### 5.3 `WhenAll(success)` 仍可继续深化

当前已经返回 `xfuture_all_value`，但仍未扩展为更丰富的聚合视图，例如：

- 每个 source 的 status 视图
- 每个 source 的 error 视图
- 完整 aggregate metadata



## 6. 当前生产级判断

### 6.1 可以这样判断

当前这条主线已经达到：

- `设计成型`
- `实现成型`
- `核心竞态已开始被正式回归覆盖`
- `可作为 XRT 后续线程/协程/xnet 统一异步基础继续使用`


### 6.2 还不能这样判断

当前还不建议直接宣称：

- “future/task/promise 已全部完成，不再需要收口”
- “默认主测试流已经全面覆盖”
- “所有高并发/长时间生产场景都已充分验证”



## 7. 下一阶段建议

建议按这个顺序继续：

1. 增加更大规模 combinator / task group / mixed-runtime 压力回归
2. 为 public 使用补一份简明指南
3. 视需要深化 `WhenAll(success)` 聚合结果合同



## 8. 推荐冻结项

以下合同建议现在就冻结，不再轻易改动：

- `Get* = 持有引用`
- `Peek* = 借用引用`
- `task group destroy => close + cancel + release hold`
- `join future = close-aware dynamic join`
- `current-thread deferred continuation` 通过 wait 主路径自动 pump
- `nested scope` 通过 `xTaskGroupCreateChild` 进入主线



## 9. 一句话总结

`future / task / promise / wait-source` 已经从“新设计”变成“XRT 正式异步运行时主干”，当前重点已经从“补功能”转向“并入默认测试基线、继续做生产级验证和文档收口”。
