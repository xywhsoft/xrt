# XRT 性能说明

> 面向当前主线的性能说明。本文不再维护旧年代零散 benchmark 叙述，而是说明 XRT 当前在性能设计上追求什么、已经有哪些基线、应该如何理解这些结果。

[English Version](PERFORMANCE.en.md) | [返回索引](README.md)

---

## 目录

- [性能设计目标](#性能设计目标)
- [当前主线的性能策略](#当前主线的性能策略)
- [已经建立的性能基线](#已经建立的性能基线)
- [各子系统的性能理解方式](#各子系统的性能理解方式)
- [如何正确做性能测试](#如何正确做性能测试)

---

## 性能设计目标

XRT 追求的不是“单个函数跑分漂亮”，而是：

- 高性能
- 轻量化
- 跨平台
- 成体系
- 在真实基础设施场景里维持稳定吞吐、延迟和资源利用率

因此，当前主线的性能目标包括：

1. 低层运行时和常用工具函数足够轻
2. 内存与容器在当前线程本地路径上保持快速
3. 协程、future、wait-source 不引入多余的执行层分裂
4. `xnet-v2` 在真实网络场景中显著优于旧主线
5. 网络应用层能够建立在统一网络底座之上，而不是每层重复造轮子

---

## 当前主线的性能策略

### 1. 线程本地快路径优先

当前 runtime、默认 RNG、临时内存、很多轻量能力都已经迁到线程状态。

这条设计的核心目的就是：

- 让最常见路径尽量不走全局锁
- 为后续线程绑定内存上下文和 shared-mode 奠定基础

### 2. local / shared 显式分离

当前 phase-2 之后的内存/容器模型，不再假设“所有对象都天然线程安全”。

而是明确区分：

- local root：本线程快路径
- shared root：显式共享路径

这让本地性能不会被“为了全局线程安全而处处加锁”拖垮。

### 3. 统一异步主线

当前已经建立：

- future
- task
- promise
- wait-source
- coroutine wait
- thread wait

这条统一异步主线的意义不只是“API 更整齐”，也包括性能：

- 避免每个模块都维护一套独立的异步结果与等待模型
- 减少中间包装和无效桥接

### 4. 网络主线统一到底层

当前 `xnet-v2` 的 stream / dgram / listener / future / wait-source / TLS session 已经开始收口成单主线。

这使得：

- HTTP
- HTTPD
- WebSocket
- 协程等待
- future 等待

都能复用同一底层，而不是每层自己再叠加一套状态机。

---

## 已经建立的性能基线

### 1. 网络新旧主线对比基线

当前已经有正式 bench 资产，位于：

- [XNET_COMPARE_20260314.md](../../dev/bench/XNET_COMPARE_20260314.md)
- [XRT_FOUNDATION_BENCHMARK_METHOD.md](../../dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md)
- [TLS_BENCH_20260315.md](../../dev/bench/TLS_BENCH_20260315.md)

这些文档已经建立了：

- Windows / Linux 平台对比
- TCP echo
- TLS echo
- UDP burst
- queue pressure

### 2. 已记录的代表性结果

以之前建立的基线为例：

- Windows 上，`xnet-v2` 相比旧主线：
	- TCP total：`91.7 -> 83178.1 msg/s`
	- TLS total：`99.9 -> 59343.7 msg/s`
	- UDP send：`322825.4 -> 1718877.6 pps`
- Debian 上，`xnet-v2` 相比旧主线：
	- TCP total：`36628.1 -> 60240.8 msg/s`
	- TLS：旧主线当时不稳定，`xnet-v2` 可稳定完成，约 `39332.4 msg/s`
	- UDP send：`340122.5 -> 1355287.2 pps`

这说明“新网络主线明显优于旧主线”不是口号，而是已经有基线支撑。

### 3. 这些数据的正确理解方式

这些数据能证明：

- `xnet-v2` 方向是对的
- 新主线已经不是“架构更漂亮但跑不快”

但这些数据不意味着：

- 所有平台所有场景已经全部收口到最终生产级

因此，性能结论应当和稳定性、竞态验证一起看。

---

## 各子系统的性能理解方式

### 1. 内存与容器

当前重点不是孤立地追求“单个容器 micro-benchmark 最大化”，而是：

- local root 快路径保持轻
- shared root 明确边界
- 错线程访问及时拒绝
- 为未来线程绑定内存上下文留出空间

### 2. 协程

当前协程主线已经做了：

- 线程绑定 runtime
- scheduler core
- stack allocator
- future / wait-source 接线

性能上更重要的是：

- 减少等待模型分裂
- 减少无意义轮询
- 让网络等待和协程等待走同一条主线

### 3. future / task / promise

这条主线的性能价值在于：

- 统一执行与结果模型
- 减少模块间重复包装
- 让 continuation / combinator / task group 建立在一个核心上

后续性能重点应放在：

- 压力回归
- 长时间运行
- destroy / cancel / close 竞争

继续深入时，还应特别关注：

- combinator（`WhenAny / WhenAll / Race`）在并发完成下的稳定性
- `task group / nested scope` 在批量 child 场景下的 join / reap / cancel 开销
- current-thread deferred continuation 在真实业务等待链路里的推进成本

而不是只看单个 `future` 的微基准。

### 4. 网络与网络应用层

当前网络底层性能主线已经比较清楚：

- engine / worker
- stream / dgram / listener
- future / wait-source
- TLS session

上层 HTTP / WebSocket 的性能评估，应该建立在这个统一底座上，而不是单独拆开孤立看。

---

## 如何正确做性能测试

### 1. 明确测试目标

先说明你测的是哪一类：

- 吞吐
- 延迟
- 内存占用
- 抖动
- 长时间稳定性

不要把这些混成一个“快不快”的模糊结论。

### 2. 固定测试条件

每次 benchmark 最好固定：

- 平台
- 编译器
- 编译选项
- 核心数
- 是否绑核
- 测试时长
- 数据规模

### 3. 同时记录吞吐与延迟

只记录吞吐不够，至少还应记录：

- 平均延迟
- p50 / p95 / p99（如果场景适合）

### 4. 网络 benchmark 要分层看

建议分开看：

- TLS on / off
- localhost / real network
- client / server
- queue pressure / normal path

### 5. 压力测试和正确性一起做

对于基础设施库，“跑得快但 destroy/cancel 竞争会炸”不能算真正完成。

因此 benchmark 最好和这些测试一起做：

- timeout / deadline
- cancel / close
- destroy / release
- 多等待者 / 多 continuation
- listener / stream / dgram 的竞争回归

对于当前异步主线，还建议把这些也纳入长期基线：

- `WhenAny / Race` 的并发完成压力
- `task group` 的批量 child、close / destroy 边界
- 多 parent cancel 传播
- nested scope 的 join 完成时机

---

## 相关文档

- [项目简介](../../README.md)
- [架构设计](ARCHITECTURE.md)
- [最佳实践](BEST_PRACTICES.md)
- [XNet V2](api/api-xnet-v2.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [性能基线方法](../../dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md)
- [XNet 对比基线](../../dev/bench/XNET_COMPARE_20260314.md)
- [TLS 基线](../../dev/bench/TLS_BENCH_20260315.md)

---

**XRT 性能文档版本：当前主线重建版**
