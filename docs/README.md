# XRT 文档中心

> 面向使用者的正式入口。先用 `guide/` 建立使用心智，再用 `api/` 查函数与类型细节，最后用 `case/` 看完整组合案例。

[English](README.en.md) | [项目简介](../README.md)

---

## 快速入口

- [项目简介](../README.md)
- [API 索引](api/README.md)
- [教学入口](guide/README.md)
- [范例入口](case/README.md)
- [示例说明](EXAMPLES.md)
- [架构设计](ARCHITECTURE.md)
- [功能裁剪宏](FEATURE_TRIMMING_MACROS.md)
- [最佳实践](BEST_PRACTICES.md)
- [常见问题](FAQ.md)
- [迁移说明](MIGRATION.md)
- [性能说明](PERFORMANCE.md)


## 按目标阅读

### 第一次接触 XRT

1. [项目简介](../README.md)
2. [架构设计](ARCHITECTURE.md)
3. [教学入口](guide/README.md)
4. [从零开始写第一个 XRT 程序](guide/first-xrt-program.md)
5. [范例入口](case/README.md)

### 准备直接写代码

1. [API 索引](api/README.md)
2. [Base 基础运行时](api/api-base.md)
3. [Value 动态类型系统](api/api-value.md)
4. [JSON 处理库](api/api-json.md)
5. [Thread 线程管理库](api/api-thread.md)
6. [Queue 队列模块](api/api-queue.md)
7. [Coroutine 协程运行时](api/api-coroutine.md)
8. [Future / Task / Promise](api/api-future-task-promise.md)
9. [XURL](api/api-xurl.md)
10. [HTTP Util](api/api-http-util.md)
11. [XNet v2 网络主线](api/api-xnet-v2.md)
12. [把配置、日志、任务、网络和模板串成一个本地控制台服务](case/local-console-service.md)

### 想先跑一个小程序

1. [示例说明](EXAMPLES.md)
2. [从零开始写第一个 XRT 程序](guide/first-xrt-program.md)
3. [用 XRT 写一个最小 HTTP 服务](case/minimal-http-service.md)


## 文档分区

- `api/`
	函数、结构、返回值、模块边界。
- `guide/`
	什么时候该用什么能力，为什么这样组合，以及从最小程序到可运行服务的学习路线。
- `case/`
	多个模块如何串成一个完整问题的解决方案。


## 常用专题

- 基础运行时与数据
	- [Base](api/api-base.md)
	- [xvalue、JSON 与 XSON 入门](guide/xvalue-json-intro.md)
- 并发与异步
	- [多任务总论：线程、队列、协程与 Future 怎么选](guide/multitask-overview.md)
	- [Queue 入门：什么时候该用消息交接，而不是共享状态](guide/queue-intro.md)
	- [协程、Future 与 Task 入门](guide/coroutine-future-task-intro.md)
- 网络与服务
	- [XURL 入门](guide/xurl-intro.md)
	- [HTTP Util 入门](guide/http-util-intro.md)
	- [XWS 入门](guide/xws-intro.md)
- 完整案例
	- [一个完整的 HTTP + JSON + Template 服务链路](case/http-json-template-chain.md)
	- [把配置、日志、任务、网络和模板串成一个本地控制台服务](case/local-console-service.md)


## 使用建议

- 先看 `guide/`，能更快建立“什么时候该换模块、什么时候该拆层”的判断。
- 写代码时随时回 `api/` 查函数细节，不需要在教学页里反复翻找签名。
- 需要完整工程组合方式时，直接进入 `case/`，优先顺着案例主线阅读。

