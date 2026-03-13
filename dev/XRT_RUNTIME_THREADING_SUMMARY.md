# XRT Runtime Threading Refactor Summary

## 1. 文档信息

- 文档路径: `dev/XRT_RUNTIME_THREADING_SUMMARY.md`
- 状态: 已整理
- 版本: `1.0.0`
- 最后更新: `2026-03-13`
- 覆盖范围: `phase-1 runtime/threading` + `phase-2 memory/container sharing`
- 相关规格:
- `dev/XRT_RUNTIME_PHASE1_SPEC.md`
- `dev/XRT_RUNTIME_PHASE2_SPEC.md`


## 2. 背景

`xrt` 最初是便捷功能集合库，后续逐步扩展到网络基础设施和更完整的运行时能力。

随着网络编程、服务器模型、AI Agent 支撑能力的推进，旧有实现暴露出两个根本问题：

- 运行时全局状态和线程状态混杂在 `xCore` 中
- 内存、容器、`xvalue` 等关键模块缺少统一的并发语义

这轮改造的目标不是“到处补锁”，而是先把 `xrt` 的并发模型定型，再为后续模块重构建立稳定基线。


## 3. 本轮工作的总体结论

本轮多线程相关工作已经完成了两层基础重构：

- `phase-1` 完成了 runtime 执行模型重构
- `phase-2` 完成了内存系统、核心容器、`xvalue` 的共享语义重构

当前 `xrt` 的并发基线已经从“隐式单线程假设”切换为“显式线程上下文 + 默认 owner-thread + 显式 shared mode”。


## 4. 当前并发模型

### 4.1 两份状态

当前运行时已经明确拆成两份状态：

- 全局状态: 保存进程级、静态只读或全局唯一的数据
- 线程状态: 保存当前线程独有的运行时数据

全局状态主要承担以下职责：

- `AppFile` / `AppPath`
- `sNull`
- `vNull` / `vTrue` / `vFalse`
- 分配器函数表
- 高频率时钟等运行时环境信息
- 运行时初始化与退出引用计数

线程状态主要承担以下职责：

- `LastError`
- 临时内存
- 默认随机数种子与状态
- 当前线程对象指针
- 线程级 cleanup 栈
- 为后续线程绑定内存上下文预留的 `tMem`


### 4.2 runtime 执行域

当前 `xrt` 采用“线程必须先进入 runtime 执行域”这一模型。

运行规则如下：

- `xrtInit()` 会自动 attach 当前线程
- `xrtThreadCreate()` 创建的线程会自动 attach
- 线程函数结束时，`xrt` 会自动 detach 并清理线程级资源
- 宿主线程或外部线程如果要调用 runtime-bound API，必须显式 attach

这条规则替代了旧有的“默认所有线程都能直接碰全局状态”的隐式模型。


### 4.3 线程生命周期

线程生命周期已经收紧为更明确的语义：

- `Create` 负责创建线程和运行时接线
- `Wait` 负责 join
- `Destroy` 负责释放线程管理对象
- 运行中的线程禁止直接 `Destroy`

线程退出时会自动执行：

- cleanup 栈回调
- `LastError` 释放
- 临时内存释放
- 线程内存状态释放
- TLS 清空
- 退出码写回线程对象

这解决了旧模型下“线程结束但线程级资源残留”以及“Destroy 和 detach 语义混杂”的问题。


## 5. phase-1 主要改动

### 5.1 线程态迁移

以下运行时能力已经迁入线程状态：

- `LastError`
- 临时内存
- 默认 RNG

这意味着以下高频便捷接口已经从“进程共享可变状态”改成“线程隔离状态”：

- `xrtTempMemory()`
- `xrtSetError()`
- `xrtGetError()`
- `xrtRand32()`
- `xrtRand64()`
- `xrtRandRange()`


### 5.2 便捷 API 体验保持不变

虽然 runtime 内部语义已经变化，但外部常用体验仍尽量保持简单：

- 主线程通常只需要 `xrtInit()` / `xrtUnit()`
- `xrtThreadCreate()` 创建的线程内部可以直接调用 `xrt` API
- 像 `xvoGetValueText()` 这类“脚本式取值”接口暂时保留原行为

也就是说，这轮改造主要改变底层运行时支撑，不主动把高层 API 强制拆成更复杂的显式生命周期风格。


### 5.3 线程库行为收紧

线程库语义比旧版本更严格：

- `xrtThreadDestroy()` 不再隐式 detach
- 运行中的线程销毁会报错
- `Wait` 和 `Destroy` 的职责分离
- 自动 attach / detach 成为标准线程路径

这会让以前一些“能跑但语义不严谨”的线程用法尽早暴露出来。


## 6. phase-2 主要改动

### 6.1 默认模式: owner-thread local

`xrt` 当前的默认对象模型是 owner-thread local。

含义是：

- 默认对象属于创建它的线程
- 本线程修改走快路径
- 其他线程误用会被拒绝

这条规则已经扩展到以下层次：

- `xbsmm`
- `xmemunit`
- `xfsmempool`
- `xmempool`
- `xparray`
- `xarray`
- `xavltree`
- `xdict`
- `xlist`


### 6.2 显式 shared mode

跨线程共享现在必须显式 opt-in。

shared mode 的入口是显式的 `CreateEx(..., XRT_OBJMODE_SHARED)` 或 `xvoCreate*Ex(XRT_OBJMODE_SHARED)`。

这条规则的意义是：

- 不对默认对象引入共享开销
- 跨线程共享必须由调用者明确表达
- 共享语义只在真正需要时启用


### 6.3 shared root lock 模型

对于会暴露内部指针、walk、iterator 的容器根对象，当前采用显式 `Lock/Unlock` 模型。

已提供显式锁接口的根对象包括：

- `xparray`
- `xarray`
- `xavltree`
- `xdict`
- `xlist`

这让旧有 pointer-view / iterator 风格 API 可以继续保留，而不必在 phase-2 引入第二套“只读视图 API 家族”。


### 6.4 `xvalue` shared 语义

`xvalue` 的共享路径已不再依赖普通非原子引用计数。

当前已完成的关键变化：

- 顶层 value header 已支持原始 header + CAS 更新路径
- array/list/table/coll-backed `xvalue` 已可进入 real shared
- 共享容器写入时会自动提升 share-compatible 子值
- 本地嵌套容器写入共享根时会被拒绝

这部分是 `xvalue` 从“理论可跨线程传递”变成“具备明确共享合同”的关键改造。


## 7. 已解决的问题

这轮改造已经实质解决了以下历史问题：

- `LastError` 不再是多线程共享错误槽
- 临时内存不再是进程级共享临时环
- 默认 RNG 不再是进程共享状态
- 线程结束时线程级资源可以自动回收
- 默认 local-mode 对象的错线程修改会被显式拒绝
- 共享容器和共享 `xvalue` 不再依赖普通数据竞争下的“碰运气”
- Windows TinyCC 下 TLS 和部分原子路径的兼容性已经补齐


## 8. 对上层代码的影响

### 8.1 正向影响

- 多线程代码的运行时行为更可预测
- 跨线程误用更容易在开发阶段暴露
- 默认本线程快路径仍然保留，性能模型更清晰
- 后续内存池和数据结构继续重构时，有稳定的线程语义可以依赖


### 8.2 兼容性影响

旧代码中如果存在以下模式，行为已经发生变化：

- 宿主线程未 attach 就直接调用 runtime-bound API
- 跨线程直接修改默认 local-mode 容器
- 把本地嵌套容器直接塞进共享容器
- 运行中的线程被直接 `Destroy`

这些行为现在会明确报错或被拒绝，而不是继续静默执行。


### 8.3 对“脚本式易用性”的影响

这轮改造刻意没有强制把常用便捷接口改造成复杂显式生命周期风格。

目前保持的使用体验是：

- 主线程像以前一样初始化后直接用
- `xrt` 创建的线程像单线程代码一样直接用
- 常用值访问接口继续维持简洁调用方式

也就是说，内部线程语义已经重构，但高层 API 体验仍尽量保持“像脚本一样易用”。


## 9. 当前仍然保留的边界

以下内容尚未在这轮工作中彻底完成或刻意推迟：

- `xrtThreadData.tMem` 目前已预留，但还没有把所有 allocator 热路径真正路由到线程内存上下文
- 临时内存仍然保留现有便捷语义，尚未切换为更彻底的 scratch arena 模型
- raw `xrtAVLTB_*` 仍然属于内部 no-lock 基元，不构成公开 shared 合同
- shared pointer-view / iterator 虽然已能通过显式 `Lock/Unlock` 使用，但长期是否演化为更独立的 view/session 抽象仍未定稿


## 10. 对后续模块重构的意义

这轮多线程改造已经为后续模块重构提供了稳定基础。

后续其他模块在设计上可以直接依赖以下事实：

- runtime 已具备明确的线程执行域模型
- 线程级状态和全局状态已经分离
- 默认 owner-thread 模式已经成立
- 显式 shared mode 已有统一词汇和入口
- shared root 锁语义已经有现成模式
- shared `xvalue` 和 shared 容器已经建立基本合同

因此，后续模块重构可以直接围绕“local fast path + explicit shared path”继续推进，而不必重新讨论线程模型本身。


## 11. 推荐的后续工作方式

后续切换到其他模块时，建议遵守以下规则：

- 如果改动涉及 runtime 线程语义，优先回看 `phase-1 spec`
- 如果改动涉及内存、容器、共享对象语义，优先回看 `phase-2 spec`
- 新模块默认沿用 owner-thread local 语义
- 只有明确存在跨线程共享需求时，才引入显式 shared mode
- 不要重新回到“默认共享可变全局状态”的设计路线


## 12. 总结

这轮多线程改造已经把 `xrt` 从“单线程默认、并发靠自觉”的旧状态，推进到了“线程执行域明确、owner-thread 默认、shared 明确 opt-in”的新状态。

从工程角度看，这不是简单修 bug，而是 runtime 基线已经变了：

- `phase-1` 解决了 runtime 和线程生命周期
- `phase-2` 解决了内存/容器/`xvalue` 的基础并发语义

因此，这部分工作可以视为已经完成阶段性收口，后续其他模块重构可以直接建立在这套模型之上继续推进。
