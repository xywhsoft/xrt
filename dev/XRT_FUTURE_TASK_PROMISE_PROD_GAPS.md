# XRT Future / Task / Promise 生产级缺口清单

版本: `0.1.0`
状态: `active`
更新时间: `2026-03-17`



## 1. 当前判断

`future / task / promise / wait-source` 这条正式异步主线已经达到：

- 设计完成
- 核心实现完成
- 默认主测试流接入完成
- 独立 core harness 和一轮正式压力/竞态回归完成

但从“完全生产级收口”的标准看，当前仍有若干缺口需要继续推进。



## 2. P1 缺口

### P1-1 更大规模与长时间压力验证仍不足

当前已有压力回归，但规模仍偏“核心路径 smoke + 中等压力”。

尚缺：

- 更大规模 source count 的 `WhenAny / WhenAll / Race`
- 更大规模 child count 的 `task group`
- mixed `engine/thread/co` 混合编排长时运行
- 长时间 soak test

影响：

- 不能充分证明这条异步主线在高并发/长时间运行下没有新的引用计数、group ctx、scope settle 问题

建议：

- 增加 `128/256` 级 source 和 child 的压力用例
- 增加分钟级 soak harness
- 增加 mixed runtime 的长时组合测试


### P1-2 Public 使用指南仍缺

当前已有设计稿和状态文档，但还没有形成面向使用者的简明指南。

尚缺：

- `Get* / Peek*` 快速规则
- continuation 目标选择建议
- `task group` 推荐写法
- `join future / close / destroy` 语义说明
- combinator 结果合同说明

影响：

- 内部实现虽然成型，但上层调用者仍容易误用 ownership 和 scope 语义

建议：

- 单独补一份 `api-future-task-promise.md`
- 用少量完整示例讲清：
	- thread task
	- coroutine wait
	- engine task
	- task group + nested scope
	- `WhenAny / WhenAll / Race`


### P1-3 `WhenAll(success)` 聚合结果仍可继续深化

当前成功路径已经返回 `xfuture_all_value`，但仍缺更丰富的聚合视图。

尚缺：

- 每个 source 的 status
- 每个 source 的 error
- 完整 aggregate metadata

影响：

- 成功聚合路径对“纯值聚合”够用
- 但对更复杂编排和诊断仍不够强

建议：

- 后续增加并列状态/错误视图
- 保持与当前 `xfuture_all_value` 兼容扩展



## 3. P2 缺口

### P2-1 默认主测试流只接入了 core harness，没有分层执行开关

当前 `Test_FutureCore(xCore)` 已进入默认主测试流，但仍缺：

- 轻量模式 / 重压模式开关
- 单独运行 future core 的主测试参数

影响：

- 当前默认运行是好事
- 但后续压力回归变重后，需要分层运行策略

建议：

- 后续按 `smoke / stress / soak` 三层组织


### P2-2 更细的 destroy/cancel 交叉矩阵仍可继续补

当前已经覆盖：

- destroy pending child
- add after close
- multi parent cancel

但仍可继续补：

- `join future` 在 destroy 竞争下的 retained-ref 行为
- continuation child 与 source destroy 的交叉
- nested scope 多层 close/destroy 顺序矩阵



## 4. 当前不再视为缺口的事项

这些事项已经不再算当前生产阻塞项：

- 默认主测试流未接入
- single parent 限制
- snapshot join
- current-thread continuation 需要手工 pump
- combinator 并发完成下的 source index 漂移
- `Get* / Peek*` 生命周期不明确



## 5. 推荐下一阶段顺序

1. 增加大规模 combinator / task group / mixed-runtime 压力回归
2. 增加 soak harness
3. 补 public 使用指南
4. 再深化 `WhenAll(success)` 聚合结果合同



## 6. 一句话结论

这条正式异步主线现在已经不是“是否能用”的问题，而是“如何继续做更强的生产级验证与文档收口”的问题。
