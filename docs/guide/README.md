# XRT 教学文档

> 面向零基础、循序渐进和“从概念到代码”的教学入口。这里与 `docs/api/` 的区别是：`api/` 负责 API 合同，`guide/` 负责教学路线。

[返回文档中心](../README.md)

---

## 目录

- [教学文档的定位](#教学文档的定位)
- [推荐教学路线](#推荐教学路线)
- [当前已完成的教学页](#当前已完成的教学页)
- [与 API 文档的关系](#与-api-文档的关系)

---

## 教学文档的定位

`guide/` 目录后续主要承载：

- 面向第一次接触 XRT 的读者
- 面向希望快速搭起可运行程序的开发者
- 面向想理解“为什么这样设计”的读者

因此，这里的文档应更关注：

- 概念解释
- 入门路径
- 从小示例到完整主线的学习顺序
- 常见误区与推荐写法

而不是逐条罗列 API。

---

## 推荐教学路线

如果从零开始学习 XRT，推荐按下面顺序阅读和练习：

### 1. 认识 XRT

目标：

- 理解 XRT 的定位
- 理解“互联网 + AI 时代的 C 语言基础设施库”是什么意思
- 理解源码树主线、单头分发、文档结构之间的关系

建议入口：

- [项目简介](../../../README.md)
- [文档中心](../README.md)
- [架构设计](../ARCHITECTURE.md)

### 2. 运行时与基础能力

目标：

- 学会 `xrtInit()` / `xrtUnit()`
- 理解 `xrtGetError()`
- 学会字符串、时间、文件等基础能力

建议入口：

- [Base](../api/api-base.md)
- [String](../api/api-string.md)
- [Time](../api/api-time.md)
- [File](../api/api-file.md)

### 3. 动态数据与 JSON

目标：

- 学会 `xvalue`
- 学会 array / list / coll / table
- 学会 JSON 与 `xvalue` 的互转

建议入口：

- [Value](../api/api-value.md)
- [JSON](../api/api-json.md)

### 4. 并发与异步

目标：

- 理解线程附加模型
- 理解协程与调度器
- 理解 future / task / promise / wait-source

建议入口：

- [Thread](../api/api-thread.md)
- [Coroutine](../api/api-coroutine.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)

### 5. 网络主线

目标：

- 理解 `xnet-v2`
- 理解 TLS session
- 理解 HTTP / HTTPD / WebSocket

建议入口：

- [XNet V2](../api/api-xnet-v2.md)
- [Network TLS](../api/api-network-tls.md)
- [XHTTP](../api/api-xhttp.md)
- [XHTTPD](../api/api-xhttpd.md)
- [XWS](../api/api-xws.md)

---

## 当前已完成的教学页

当前已经落地的正式教学页包括：

1. [从零开始写第一个 XRT 程序](first-xrt-program.md)
2. [XRT 运行时与线程附加入门](runtime-thread-attach.md)
3. [xvalue 与 JSON 入门](xvalue-json-intro.md)
4. [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
5. [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
6. [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)
7. [用 XRT 写最小 HTTP 服务入门](minimal-http-service-intro.md)
8. [用 XRT 调用流式 LLM API 入门](streaming-llm-api-intro.md)
9. [XRT 子进程与工具执行入门](subprocess-intro.md)

这些教学页已经足够支撑“从运行时、数据、并发一直到网络与 AI 场景”的第一轮学习。

后续仍可继续补：

- 更细的 `xhttp / xhttpd / xws` 专题教学
- 更细的 `task group / nested scope` 教学
- 更细的 `template + xvalue + json` 组合教学

---

## 与 API 文档的关系

建议把 `guide/` 和 `api/` 的关系理解成：

- `api/`：告诉你 API 是什么、合同是什么、边界是什么
- `guide/`：告诉你应该先学什么、怎么组合、为什么这样写

如果你已经知道自己要找哪个函数，优先去：

- [API 索引](../api/README.md)

如果你还不确定应该先学哪条主线，优先从这里开始。

---

**当前状态：目录、学习路线与第一批正式教学页已经建立，当前可作为中文主线教学入口使用。**
