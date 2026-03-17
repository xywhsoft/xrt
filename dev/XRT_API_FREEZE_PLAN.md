# XRT API 冻结计划

版本: `0.1.0`
状态: `active`
更新时间: `2026-03-17`



## 1. 目标

本文档用于回答一个核心问题：

> XRT 哪些模块的 API 已经足够强，可以开始冻结并承诺长期兼容；哪些模块仍处于快速演进期，不应现在锁死。

目标不是追求“所有模块立即冻结”，而是：

- 避免未来继续积累历史包袱
- 让已经成熟的抽象尽快稳定下来
- 给仍在演进的模块预留必要调整空间



## 2. 冻结原则

一个模块要进入 `API Frozen`，至少应满足：

- 核心抽象已经稳定
- 主要职责边界已经清晰
- 生命周期合同已经明确
- 测试和回归已经覆盖主路径
- 后续大概率只改内部实现，不再改 public surface

一个模块如果仍处于以下状态，则不应冻结：

- 命名和模块边界还在迁移
- 生命周期合同未完全收口
- 线程/协程/共享语义仍在补齐
- 仍存在明显的“历史包袱清理窗口”



## 3. 分类说明

本文档将模块分为 4 类：

- `A. 已建议冻结`
- `B. 可开始冻结`
- `C. 暂不冻结`
- `D. 明确不应冻结历史表面`



## 4. A 类：已建议冻结

这些模块或主线，已经具备较强的抽象稳定性。后续应该尽量只改实现，不改 public contract。


### 4.1 Runtime / Thread Attach 主线

状态：`建议冻结`

冻结范围：

- `xrtInit / xrtUnit`
- `xrtThreadAttachCurrent / xrtThreadDetachCurrent`
- 线程态 / 全局态分离模型
- `LastError / TempMemory / RNG` 的线程态归属

冻结理由：

- 这是整个 runtime 的根合同
- 后续线程、协程、future、内存池都会依赖这套模型
- 如果这层继续摇摆，后续所有模块都会被拖着反复返工

允许继续修改的部分：

- 内部实现
- 平台后端
- 诊断细节

不建议再改的部分：

- “线程必须先进入 XRT runtime 上下文” 这一根语义
- attach/detach 主模型


### 4.2 Future / Task / Promise / Wait-Source 主线

状态：`建议冻结`

冻结范围：

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`
- `Wait / WaitTimeout / WaitUntil`
- `Get* / Peek*`
- `Then / Catch / Finally`
- `WhenAny / WhenAll / Race`
- `task group / join / close / destroy / create child`

冻结理由：

- 这是最近按最高标准专门设计和实现的一条新主线
- 抽象层级是对的
- 生命周期和 ownership 已经过一轮正式收口
- 已进入默认主测试流

允许继续修改的部分：

- 内部实现
- 聚合结果增强
- 压力/长时验证
- public 使用文档

不建议再改的部分：

- 核心对象名称与职责
- `Get* / Peek*` 语义
- `Wait / Timeout / Until` 三件套
- `task group` 的根 scope 语义


### 4.3 结构化并发核心语义

状态：`建议冻结`

冻结范围：

- `task group` 是 scope，不只是 future 容器
- `destroy => close + cancel pending children + release hold`
- `join future = close-aware dynamic join`
- `nested scope => xTaskGroupCreateChild`
- `parent cancel/close/destroy` 传播到 child

冻结理由：

- 这是 future 主线里最容易积累历史包袱的部分
- 现在已经有明确合同和正式回归

允许继续修改的部分：

- 更丰富的父子 scope 关系
- 更深的嵌套优化

不建议再改的部分：

- scope 的根语义
- destroy/join/close 的基本合同



## 5. B 类：可开始冻结

这些模块已经有较强的方向稳定性，可以开始“冻结大方向”，但不建议一次性把所有细枝末节全部锁死。


### 5.1 协程高层运行时 API

状态：`可开始冻结`

建议冻结的部分：

- 协程是线程绑定 runtime
- scheduler 是正式调度入口
- `Create / Resume / Yield / Sleep / Join / Cancel / Exit`
- wait-source / future / xnet 与协程的结合方向

不建议现在冻结的部分：

- 各 backend 的平台细节
- 某些可能受真机验证影响的后端辅助接口

原因：

- 协程高层语义已经成型
- 但 backend 级别的生产验证还没彻底完成


### 5.2 xnet-v2 的 wait-source / future 集成方向

状态：`可开始冻结`

建议冻结的部分：

- `future` 是正式异步结果模型
- `wait-source` 是统一等待桥
- `stream/dgram/future` 向统一等待模型收口
- timeout/deadline/cancel 的统一方向

不建议现在冻结的部分：

- 所有专名 surface
- listener / accept 的最终 public shape
- 最终命名和模块分布

原因：

- 方向已经明确
- 但网络 public surface 还在历史包袱清理窗口


### 5.3 Shared-Mode 的根方向

状态：`可开始冻结`

建议冻结的部分：

- `local` 是默认
- `shared` 必须显式 opt-in
- 共享访问必须有明确同步合同

不建议现在冻结的部分：

- 所有容器/内存池/shared root 的完整 API 细节

原因：

- 设计原则已经正确
- 但第二阶段仍在持续收口



## 6. C 类：暂不冻结

这些模块当前不适合承诺 API 长期不变。


### 6.1 网络应用层

包括：

- HTTP
- WebSocket
- HTTPD
- TLS 上层表面

状态：`暂不冻结`

原因：

- 命名、模块拆分、文档、迁移策略还在继续清理
- 新网络栈和旧网络栈的边界还在收口
- 这层离用户最近，一旦过早冻结，后续清理历史包袱代价会更大


### 6.2 内存管理 shared / thread-bound 上层 API

包括：

- mempool
- memunit
- fixed-size mempool
- shared-mode 下的容器与池管理

状态：`暂不冻结`

原因：

- 这部分本来就在第二阶段继续收口
- 如果现在锁死 API，后面线程安全和线程绑定优化容易被绑住


### 6.3 某些协程 backend 辅助表面

状态：`暂不冻结`

原因：

- 仍受未完成平台真机验证影响
- 当前更适合冻结高层语义，不适合冻结 backend 级工具表面



## 7. D 类：明确不应冻结历史表面

这些内容如果还存在，应优先清理，而不是保留兼容承诺。


### 7.1 旧网络库历史表面

状态：`不应冻结`

原因：

- 新网络主线已经成型
- 旧表面越早清理，越能避免双轨并行造成长期混乱


### 7.2 误导性版本命名 / 历史命名

例如：

- 名称与真实协议/能力不匹配的模块名

状态：`不应冻结`

原因：

- 这类命名一旦冻结，会直接把历史包袱固化到长期 public contract



## 8. 当前建议的冻结清单

### 8.1 现在就应开始视为稳定合同的

- runtime attach 模型
- global state / thread state 分层
- future / task / promise / wait-source 核心对象
- `Wait / Timeout / Until`
- `Get* / Peek*`
- continuation 的 4 类执行目标
- combinator 的根语义
- task group 的 scope 根语义


### 8.2 现在只冻结方向，不冻结细枝末节的

- 协程高层 API
- xnet wait-source / future 集成方向
- shared-mode 根方向


### 8.3 现在明确不要冻结的

- 旧网络表面
- 网络应用层最终 public surface
- 内存管理 shared 细节 API
- backend 级辅助细节接口



## 9. 冻结前缺口

### 9.1 对 A 类

仍建议继续做：

- 更大规模压力验证
- 更长时间 soak test
- public 使用指南补齐

但这些不会阻止 API 冻结方向。


### 9.2 对 B 类

冻结前还要继续收口：

- 协程跨平台真机验证
- xnet public surface 清理
- shared-mode 第二阶段补完



## 10. 一句话结论

XRT 现在不适合“一刀切宣布所有模块 API 全部冻结”，但已经完全可以开始做 **分模块 API 冻结**。  

最应该率先冻结的，就是：

- runtime / thread attach 主线
- future / task / promise / wait-source 主线
- 结构化并发核心语义
