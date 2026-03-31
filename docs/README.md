# XRT 文档中心

> XRT 当前正式文档入口。`api/` 已经可以承担源码主线的合同说明；`guide/` 与 `case/` 正在按“从最小 DEMO 到完整项目”的方式重建。

[English](README.en.md) | [项目简介](../README.md)

---

## 当前状态

- `docs/api/`：当前最适合直接查合同、类型和模块边界。
- `docs/guide/`：现有页面大多还是主题短讲，正在重建为循序渐进教程。
- `docs/case/`：现有页面大多还是专题说明，正在重建为完整案例拆解。
- [文档审计](DOCS_AUDIT.md)：整理哪些文档已过期、哪些链接有问题、哪些模块尚未覆盖。
- [教学重建路线图](guide/ROADMAP.md)：定义后续正式教学主线、模块覆盖范围和案例梯度。


## 文档结构

当前 `docs/` 目录分为 4 层：

### 1. 文档中心

- 当前页 [README.md](README.md)
- [文档审计](DOCS_AUDIT.md)

负责：

- 说明文档结构
- 标记当前状态
- 提供统一入口
- 跟踪重建进度

### 2. API 参考

- [API 索引](api/README.md)

负责：

- 类型与公共结构
- 基础运行时
- 线程、协程、队列与异步
- 内存、容器、文本与数据
- 当前网络主线 API

### 3. 教学文档

- [教学入口](guide/README.md)
- [教学重建路线图](guide/ROADMAP.md)

负责：

- 从零到一教学
- 概念解释
- 方案比较
- 从最小 DEMO 到组合示例的渐进式学习路线

### 4. 案例文档

- [案例入口](case/README.md)

负责：

- 完整问题拆解
- 多模块协作方式
- 从“能跑”走向“能落地”的组合示例


## 当前推荐入口

### 源码与定位

- 根目录 [README.md](../README.md)
- [架构设计](ARCHITECTURE.md)
- [裁剪宏定义说明](裁剪宏定义说明.md)

### API 主线

- [API 索引](api/README.md)
- [Base 基础运行时](api/api-base.md)
- [Value 动态类型系统](api/api-value.md)
- [JSON 处理库](api/api-json.md)
- [Thread 线程管理库](api/api-thread.md)
- [Coroutine 协程运行时](api/api-coroutine.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [XNet v2 网络主线](api/api-xnet-v2.md)
- [Network TLS](api/api-network-tls.md)
- [XHTTP](api/api-xhttp.md)
- [XHTTPD](api/api-xhttpd.md)
- [XWS](api/api-xws.md)

### 教学与案例主线

- [教学入口](guide/README.md)
- [教学重建路线图](guide/ROADMAP.md)
- [案例入口](case/README.md)
- [示例说明](EXAMPLES.md)


## 当前文档重建规则

- 中文版本优先完整和校准。
- 英文版本在中文稳定后统一同步。
- API 文档优先保证“和代码一致”。
- 教学文档优先保证“解释为什么、何时用、如何一步步写出来”。
- 案例文档优先保证“一个完整问题如何把多个模块串起来”。
- 已淘汰的旧主线或内部设计稿，不应继续作为正式入口文档保留在 `docs/`。


## 建议阅读顺序

如果你是第一次接触 XRT，建议按下面顺序阅读：

1. [项目简介](../README.md)
2. [架构设计](ARCHITECTURE.md)
3. [API 索引](api/README.md)
4. [文档审计](DOCS_AUDIT.md)
5. [教学重建路线图](guide/ROADMAP.md)

如果你准备直接写代码，建议优先阅读：

1. [Base](api/api-base.md)
2. [Value](api/api-value.md)
3. [JSON](api/api-json.md)
4. [Thread](api/api-thread.md)
5. [Coroutine](api/api-coroutine.md)
6. [Future / Task / Promise](api/api-future-task-promise.md)
7. [XNet v2](api/api-xnet-v2.md)

如果你想先看当前已有的快速教学页，可以继续阅读：

1. [从零开始写第一个 XRT 程序](guide/first-xrt-program.md)
2. [多任务总论：线程、队列、协程与 Future 怎么选](guide/multitask-overview.md)
3. [线程入门：什么时候该开线程，什么时候不该](guide/thread-intro.md)
4. [XRT 运行时与线程附加入门](guide/runtime-thread-attach.md)
5. [xvalue 与 JSON 入门](guide/xvalue-json-intro.md)
6. [协程、Future 与 Task 入门](guide/coroutine-future-task-intro.md)
7. [XRT 子进程与工具执行入门](guide/subprocess-intro.md)
8. [XRT 异步文件与目录操作入门](guide/file-async-intro.md)

如果你想直接看当前已有的组合案例，可以优先阅读：

1. [最小 HTTP 服务](case/minimal-http-service.md)
2. [线程、协程与 Future 协同模型](case/thread-coroutine-future.md)
3. [流式 LLM API](case/streaming-llm-api.md)
4. [HTTP + JSON + Template 完整链路](case/http-json-template-chain.md)
5. [xnet-v2 Stream Wait-Source 实战](case/xnet-stream-wait-source.md)

