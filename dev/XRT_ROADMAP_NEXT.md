# XRT 下一阶段蓝图

状态: Active  
语言: 中文  
最后更新: 2026-03-20

## 1. 目标定位

XRT 的长期目标是：

**互联网时代的 C 语言基础设施库**

当前需要进一步明确职责边界：

- XRT 聚焦基础运行时、内存、并发、网络、HTTP、文本处理与工程基础设施
- AI、LLM、SSE 流式消息、模型协议适配等高层能力由单独的 `xllm` 库承担
- XRT 只保留那些对上层系统普遍有价值、且不绑定上层模型协议的底层能力

这意味着，XRT 不是单纯的工具函数集合，也不是单纯的网络库，而是面向以下场景的基础运行时与通用基础设施：

- 通用应用程序开发
- 高并发服务开发
- 网络客户端与服务端开发
- 数据交换、流式 I/O 与结构化信息处理

下一阶段最重要的，不是继续零散增加功能点，而是补齐几条真正决定 XRT 上限的“基础主干”。


## 2. 当前能力图

### 2.1 已形成的基础层

- 基础功能: `base / os / string / charset / time / file / xid`
- 内存能力: `mempool / fixed-size mempool / memory trace / debug`
- 数据结构: `array / list / dict / avltree / stack`
- 并发基础: `thread / coroutine`
- 异步结果模型: `future / promise / task / wait-source / task group`
- 网络传输: `tcp / udp / pool / xnet-v2 / proxy`
- 网络上层: `http / websocket / tls / httpd`
- 互联网工具: `xurl / xhttp_util`
- 通用编码: `hex / base64 / percent-encode / percent-decode`
- 数据表达: `xvalue / json`
- 文本层: `template / regex`
- 工程资产: focused test、HTTP util fuzz harness、bench baseline

### 2.2 当前仍缺少的主干

从“现代互联网基础设施”的标准看，当前最明显的缺口主要集中在：

- 统一执行上下文模型
- 统一错误模型
- 统一资源生命周期模型
- 统一日志、配置、进程与调试基础设施


## 3. 下一阶段最值得补的主干

### 3.1 统一执行上下文模型

建议引入一套正式的执行上下文抽象，例如：

- `xctx`
- `xcancel`
- `xdeadline`

它们应当统一承载：

- 取消请求
- deadline / timeout
- 上下文级附加数据
- 与线程、协程、future、网络等待的一致语义

这个能力非常关键，因为它会成为以下模块的共同基础：

- 协程等待
- 网络等待
- HTTP 客户端
- 子进程 / 工具执行

如果没有这层抽象，后续每个模块都会各自维护一套 timeout/cancel 语义，系统会逐渐失去一致性。


### 3.2 统一错误模型

当前已有线程级错误槽，但未来如果要支撑大型系统，建议建立三层错误语义：

- 快速路径: 简单返回值，例如 `bool/int/status`
- 线程级最近错误: `xrtGetError()`
- 结构化错误对象: 错误码、模块、系统错误、附加上下文

建议未来支持：

- 模块化错误码
- 系统错误透传
- 错误来源定位
- 结构化上下文绑定

这会显著提高调试质量，也利于 HTTP、TLS、子进程等复杂系统组合。


### 3.3 统一资源生命周期模型

这项工作不会直接产生新功能，但会显著影响整个库的长期可维护性。

建议尽快冻结并统一以下语义：

- `Init / Unit`
- `Create / Destroy`
- `Open / Close`
- `Borrowed / Owned`
- `Local / Shared`
- `Sync / Async`

尤其是以下模块必须逐步统一：

- 并发与协程
- 内存池
- 网络对象
- `xvalue`
- wait source / future

如果这套模型不尽早冻结，后续越往上做 API 越容易失控。


## 4. 互联网时代非常实用的能力层

### 4.1 日志系统

建议增加正式日志模块，而不是继续依赖零散输出。

推荐最小能力：

- 级别: `trace/debug/info/warn/error/fatal`
- 模块标签
- 时间戳
- 线程 ID / 协程 ID
- sink: `console / file / callback`
- structured fields
- 滚动文件

这对服务程序、网络库、基准测试与线上问题定位都非常重要。


### 4.2 配置系统

建议补齐以下配置基础设施：

- 命令行参数解析
- 环境变量读取
- 配置文件加载
- 多来源覆盖规则

建议优先考虑：

- `env`
- `ini`
- `json`
- 后续再考虑 `toml`


### 4.3 JSON 校验与绑定

`xvalue + json` 已经提供了不错的数据表达能力，下一步建议补齐：

- schema-like validation
- `required / optional / default`
- path-based error report
- typed binding helper

这样会显著改善：

- 配置校验
- HTTP body 校验
- 内部结构化数据绑定


### 4.4 通用编码工具层

以下通用编码能力仍值得补齐：

- `base64url`
- `html escape/unescape`
- `csv`

`hex / base64 / percent-encode / percent-decode` 已经从路线图移出，不再属于下一阶段待办。


### 4.5 子进程 / 工具执行层

当前 `os` 层已经有基础的启动与等待能力，但还缺少“可编排”的正式子进程模型。

建议增加：

- 启动子进程
- 管道读取 `stdout/stderr`
- 提供 `stdin`
- timeout
- exit code
- working dir
- env override

如果这层不补齐，XRT 仍然只能覆盖“启动程序”，很难覆盖真正的工具编排场景。


### 4.6 动态库 / 插件加载

建议提供最小跨平台动态库包装：

- load library
- get symbol
- unload

这项能力未来可用于：

- 动态扩展
- 可插拔模块
- 工具与 provider 接入


## 5. 网络层还能继续增强的能力

### 5.1 客户端基础设施

建议逐步补齐：

- connection pool
- keepalive pool
- DNS cache
- retry policy
- backoff
- circuit breaker
- rate limiter

`proxy support` 已经从路线图移出，不再作为下一阶段待办。


### 5.2 WebSocket client 健壮化

当前已有 websocket，但未来建议继续增强：

- 自动 ping/pong
- reconnect policy
- close reason
- backpressure contract
- coroutine / future waiting


### 5.3 HTTP/2 与后续协议

当前命名和实现应坚持“没有历史包袱”的原则：

- HTTP/1.1 就明确叫 HTTP/1.1
- HTTP/2 只有在真实实现后再引入
- QUIC / HTTP/3 可放更后阶段


## 6. 工程质量与可观测性层

### 6.1 trace / metrics / profiling hooks

建议增加统一的轻量观测能力：

- span begin / end
- counter
- gauge
- histogram
- callback sink

重点覆盖：

- thread
- coroutine
- xnet
- http
- tls


### 6.2 fuzzing 友好入口

HTTP util 的 fuzz harness 已经从路线图移出。下一阶段建议继续为以下模块准备独立 fuzz harness：

- json
- http parser
- websocket frame
- template
- regex


### 6.3 sanitizer / 测试矩阵

建议未来稳定建设：

- ASan
- UBSan
- TSan
- Windows / Linux
- GCC / Clang / TCC

这对基础设施级项目非常关键。


### 6.4 benchmark policy

当前已经形成了基础 bench 体系，下一阶段建议继续固定并制度化：

- baseline
- hardware note
- repeat count
- throughput
- latency
- `p50 / p95`
- regression threshold


## 7. 建议的优先级排序

### 7.1 P0: 最优先

- 统一执行上下文: `context / cancel / deadline`
- 统一错误模型
- 统一资源生命周期模型

这些能力会影响后面几乎所有模块。


### 7.2 P1: 高优先级

- logging
- config + env + cli
- JSON validation / binding
- subprocess

这批能力会显著提升 XRT 的可用性和工程完整度。


### 7.3 P2: 中优先级

- dynamic library / plugin wrapper
- client infra
- WebSocket client 健壮化
- 通用编码工具层剩余能力


### 7.4 P3: 后续扩展

- HTTP/2
- QUIC / HTTP/3
- 更大规模的 observability 方案


## 8. 明确不建议过早扩张的范围

为了避免 XRT 失控，以下方向不建议过早投入过深：

- 重量级数据库
- ORM
- 浏览器级 JS 引擎集成
- 与 `xllm` 职责重叠的高层模型协议能力

这些方向会迅速稀释 XRT 作为“基础设施库”的边界。


## 9. 一句话结论

XRT 下一阶段最需要补的，不是再增加零散功能，而是补齐 3 条主干：

- 统一执行上下文主干
- 统一错误与生命周期主干
- 统一工程基础设施主干

只要这三条线补起来，XRT 就会从“功能很多的 C 库”进一步收敛为：

**互联网时代的 C 语言基础设施库**
