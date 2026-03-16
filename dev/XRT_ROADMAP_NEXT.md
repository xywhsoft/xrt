# XRT 下一阶段蓝图

状态: Active  
语言: 中文  
最后更新: 2026-03-16

## 1. 目标定位

XRT 的长期目标是：

**互联网 + AI 时代的 C 语言基础设施库**

这个目标意味着，XRT 不是单纯的工具函数集合，也不是单纯的网络库，而是面向以下场景的基础运行时与通用基础设施：

- 通用应用程序开发
- 高并发服务开发
- 网络客户端与服务端开发
- AI Agent 与 LLM 应用开发
- 数据交换、流式处理与结构化信息处理

当前已有能力已经覆盖：

- 基础运行时
- 内存管理
- 常用数据结构
- 线程与协程
- 网络传输与网络上层协议
- `xvalue/json` 数据处理
- 模板与正则

下一阶段最重要的，不是继续零散增加功能点，而是补齐几条真正决定 XRT 上限的“基础主干”。


## 2. 当前能力图

### 2.1 已形成的基础层

- 基础功能: `base / os / string / charset / time / file / xid`
- 内存能力: `mempool / fixed-size mempool / memory trace / debug`
- 数据结构: `array / list / dict / avltree / stack`
- 并发基础: `thread / coroutine`
- 网络传输: `tcp / udp / pool / xnet-v2`
- 网络上层: `http / websocket / tls / httpd`
- 数据表达: `xvalue / json`
- 文本层: `template / regex`

### 2.2 当前缺少的主干

从“现代互联网 + AI 基础设施”的标准看，当前最明显的缺口主要集中在：

- 统一执行上下文模型
- 统一异步结果模型
- 统一互联网工具层
- 统一 AI 抽象层
- 统一日志、配置、进程与调试基础设施


## 3. 下一阶段最值得补的主干

## 3.1 统一执行上下文模型

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
- future/task
- HTTP 客户端
- LLM 请求
- Agent 工具调用

如果没有这层抽象，后续每个模块都会各自维护一套 timeout/cancel 语义，系统会逐渐失去一致性。


## 3.2 future / task / promise 正式化

当前 `future` 已经开始进入系统核心，但还更偏内部桥接与等待源。

建议将其提升为正式一等公民，并打通以下能力：

- 线程任务结果
- 协程等待结果
- 网络异步结果
- 定时器结果
- engine post 结果
- 组合等待能力

建议未来至少具备：

- `future create / resolve / reject / destroy`
- `wait / timeout / deadline`
- `wait source bridge`
- `result + status + error`
- 单等待者与多等待者策略明确


## 3.3 统一错误模型

当前已有线程级错误槽，但未来如果要支撑大型系统，建议建立三层错误语义：

- 快速路径: 简单返回值，例如 `bool/int/status`
- 线程级最近错误: `xrtGetError()`
- 结构化错误对象: 错误码、模块、系统错误、附加上下文

建议未来支持：

- 模块化错误码
- 系统错误透传
- 错误来源定位
- 结构化日志绑定

这会显著提高调试质量，也利于 HTTP、TLS、Agent、子进程等复杂系统组合。


## 3.4 统一资源生命周期模型

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

## 4.1 日志系统

建议增加正式日志模块，而不是继续依赖零散输出。

推荐最小能力：

- 级别: `trace/debug/info/warn/error/fatal`
- 模块标签
- 时间戳
- 线程 ID / 协程 ID
- sink: console / file / callback
- structured fields
- 滚动文件

这对服务程序、网络库、AI Agent、基准测试都非常重要。


## 4.2 配置系统

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


## 4.3 URL / Query / Header / Cookie / Form 工具层

当前已有 HTTP，但互联网应用中还有一层非常高频的辅助能力尚未独立出来。

建议补：

- URL parse / build
- query encode / decode
- header helper
- cookie parse / build
- `application/x-www-form-urlencoded`
- `multipart/form-data`

这会极大改善 HTTP 客户端、服务端、LLM API 调用与调试体验。


## 4.4 通用编码工具层

建议补齐这些高频基础件：

- `base64`
- `base64url`
- `hex`
- `percent-encode`
- `html escape/unescape`
- `csv`
- `sse parser`

其中 `SSE parser` 对 AI 应用价值很高，优先级应明显高于 `csv`。


## 4.5 子进程 / 工具执行层

AI Agent 场景极度依赖子进程能力。

建议增加：

- 启动子进程
- 管道读取 stdout/stderr
- 提供 stdin
- timeout
- exit code
- working dir
- env override

这是 Agent 与工具编排能力的关键基础件之一。


## 4.6 动态库 / 插件加载

建议提供最小跨平台动态库包装：

- load library
- get symbol
- unload

这项能力未来可用于：

- 动态扩展
- 可插拔模块
- 工具与 provider 接入


## 5. AI 时代特别重要的能力层

## 5.1 流式输出基础设施

LLM API 广泛依赖流式响应，因此建议把这部分提升到核心优先级。

建议能力包括：

- stream body reader
- SSE parser
- chunked body helper
- incremental message merge
- partial JSON / partial text 处理

这会直接影响：

- 聊天补全流式输出
- 工具调用流式结果
- 长响应可视化与中断


## 5.2 结构化输出与校验

`xvalue + json` 已经提供了不错的数据表达能力，下一步建议补齐：

- schema-like validation
- required / optional / default
- path-based error report
- typed binding helper

这样会显著改善：

- 配置校验
- HTTP body 校验
- LLM structured output
- tool call arguments 校验


## 5.3 AI 消息模型

建议构建“模型无关”的消息与工具调用抽象，而不是过早绑定某个厂商 SDK。

建议抽象：

- message list
- role
- content part
- tool call
- tool result
- delta merge
- usage / token stats

这样以后接 OpenAI、Anthropic、Gemini、私有模型都会更顺。


## 5.4 向量与基础相似度工具

如果希望 XRT 更贴近 AI 基础设施，可以加入轻量数值能力：

- float vector
- cosine similarity
- dot product
- L2 distance
- top-k helper

这里不建议一开始就做重量级向量数据库，但基础向量运算非常有价值。


## 5.5 文本切分与清洗

建议补一层轻量文本处理：

- sentence split
- paragraph split
- chunk by size
- overlap chunk
- whitespace normalize
- markdown/plaintext extract helper

这对 RAG、prompt 构造、LLM 前处理都很实用。


## 5.6 工具调用协议层

建议构建与模型无关的 tool-call 协议封装：

- tool descriptor
- argument schema
- dispatch helper
- result envelope
- error envelope

这会让 Agent 系统不必在每个项目里重新造一套工具调用框架。


## 6. 网络层还能继续增强的能力

## 6.1 客户端基础设施

建议逐步补齐：

- connection pool
- keepalive pool
- DNS cache
- retry policy
- backoff
- circuit breaker
- rate limiter
- proxy support

这些能力对互联网客户端、LLM API client 都非常重要。


## 6.2 SSE client

这项能力值得单独列出，因为它对 AI 的重要性极高。

建议具备：

- 连接 SSE endpoint
- event parse
- message assemble
- reconnect policy
- callback / coroutine wait / future bridge


## 6.3 WebSocket client 健壮化

当前已有 websocket，但未来建议继续增强：

- 自动 ping/pong
- reconnect policy
- close reason
- backpressure contract
- coroutine/future waiting


## 6.4 HTTP/2 与后续协议

当前命名和实现应坚持“没有历史包袱”的原则：

- HTTP/1.1 就明确叫 HTTP/1.1
- HTTP/2 只有在真实实现后再引入
- QUIC / HTTP/3 可放更后阶段


## 7. 工程质量与可观测性层

## 7.1 trace / metrics / profiling hooks

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


## 7.2 fuzzing 友好入口

建议逐步为以下模块准备独立 fuzz harness：

- json
- http parser
- websocket frame
- url parse
- template
- regex


## 7.3 sanitizer / 测试矩阵

建议未来稳定建设：

- ASan
- UBSan
- TSan
- Windows / Linux
- GCC / Clang / TCC

这对基础设施级项目非常关键。


## 7.4 benchmark policy

当前已经开始形成 bench 体系，建议继续固定并制度化：

- baseline
- hardware note
- repeat count
- throughput
- latency
- p50 / p95
- regression threshold


## 8. 建议的优先级排序

## 8.1 P0: 最优先

- 统一执行上下文: `context / cancel / deadline`
- `future / task` 正式化
- 统一错误模型
- 统一资源生命周期模型

这些能力会影响后面几乎所有模块。


## 8.2 P1: 高优先级

- logging
- config + env + cli
- URL/query/header/cookie/form
- SSE / 流式 HTTP body
- subprocess
- JSON validation / structured output

这批能力会显著提升 XRT 的可用性和现代感。


## 8.3 P2: AI 方向强化

- AI message model
- tool-call 协议层
- 轻量向量与相似度工具
- 文本切分与清洗

这批能力能让 XRT 更像“AI 时代基础设施库”，但不必先于 P0/P1。


## 8.4 P3: 后续扩展

- HTTP/2
- QUIC / HTTP/3
- 更完整的插件体系
- 更大规模的 observability 方案


## 9. 明确不建议过早扩张的范围

为了避免 XRT 失控，以下方向不建议过早投入过深：

- 重量级数据库
- ORM
- 完整向量数据库
- 厂商绑定的 LLM SDK 大合集
- 浏览器级 JS 引擎集成

这些方向会迅速稀释 XRT 作为“基础设施库”的边界。


## 10. 一句话结论

XRT 下一阶段最需要补的，不是再增加零散功能，而是补齐 3 条主干：

- 统一执行上下文主干
- 统一互联网工具层主干
- 统一 AI 抽象层主干

只要这三条线补起来，XRT 就会从“功能很多的 C 库”真正升级为：

**互联网 + AI 时代的 C 语言基础设施库**
