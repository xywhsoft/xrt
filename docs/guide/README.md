# XRT 教学文档

> 面向零基础、循序渐进和“从概念到代码”的教学入口。这里与 `docs/api/` 的区别是：`api/` 负责合同和边界，`guide/` 负责学习顺序、方案比较和写代码时的心智模型。

[返回文档中心](../README.md)

---

## 目录

- [教学文档的定位](#教学文档的定位)
- [当前状态](#当前状态)
- [正式教学路线](#正式教学路线)
- [现有页面如何使用](#现有页面如何使用)
- [当前空缺](#当前空缺)
- [与 API 文档的关系](#与-api-文档的关系)

---

## 教学文档的定位

`guide/` 目录后续主要承载：

- 面向第一次接触 XRT 的读者
- 面向希望快速搭起可运行程序的开发者
- 面向想理解“为什么这样设计、什么时候该换方案”的读者

因此，这里的文档不应该只是：

- 列 API
- 给一个片段
- 说一句“更多看源码”

而应该系统回答：

1. 这个模块解决什么问题
2. 如果只有一条主线程，会卡在哪里
3. 它和相邻方案的区别是什么
4. 最小 DEMO 该怎么写
5. 什么时候该升级到更完整的版本
6. 常见错误和边界是什么


## 当前状态

当前 `guide/` 已有若干可读页面，但整体仍处于重建中：

- 现有页面大多是“主题短讲”或“快速上手页”
- 适合先扫概念、确认主线方向
- 还不等于完整课程体系
- 尚未覆盖 XRT 全部公开模块

因此，当前入口分成两层理解：

- [ROADMAP.md](ROADMAP.md)：正式教学重建路线图
- 现有专题页：可作为过渡材料和草稿种子


## 正式教学路线

后续正式中文教学主线按下面顺序重建：

1. [教学重建路线图](ROADMAP.md)
2. [从零开始写第一个 XRT 程序](first-xrt-program.md)
3. 基础运行时与工具模块专题
4. 内存、容器与数据结构专题
5. `xvalue / json / xson / template / regex / crypto` 专题
6. 多任务专题：线程、队列、协程、future/task/promise、wait-source、task group
7. 系统能力专题：子进程、异步文件
8. 网络主线专题：`xurl / http util / xnet-v2 / TLS / xhttp / xhttpd / xws / proxy`
9. 完整案例梯度：从配置系统、HTTP 服务到流式 LLM API

如果你要先看“课程蓝图”，直接打开：

- [ROADMAP.md](ROADMAP.md)


## 现有页面如何使用

当前已经存在的页面，可以这样理解：

### 1. 入门种子页

- [从零开始写第一个 XRT 程序](first-xrt-program.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)
- [xvalue 与 JSON 入门](xvalue-json-intro.md)

### 2. 并发与异步短讲

- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
- [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)

### 3. 网络与应用页

- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [用 XRT 写最小 HTTP 服务入门](minimal-http-service-intro.md)
- [用 XRT 调用流式 LLM API 入门](streaming-llm-api-intro.md)
- [XRT HTTPD 异步处理入门](httpd-async-intro.md)

### 4. 系统能力页

- [XRT 子进程与工具执行入门](subprocess-intro.md)
- [XRT 异步文件与目录操作入门](file-async-intro.md)

这些页面当前适合：

- 快速了解一个模块是否已进入主线
- 先拿到最短路径的可运行片段
- 在等待正式课程页重写时作为过渡参考


## 当前空缺

目前还没有成体系教学的模块主要包括：

- `charset / path / os / math / hash / xid / network(local info)`
- `buffer / array / ptrarray / stack / dynstack / llist / avltree / dict / list`
- `bsmm / memunit / mempool_fs / mempool`
- `queue`
- `jnum / xson / template / regex / crypto`
- `xurl / xhttp_util / xws / proxy`
- 多任务专题中的“为什么需要多任务”和“线程、协程、future 的选择方法”

这也是后续优先补齐的部分。


## 与 API 文档的关系

建议把 `guide/` 和 `api/` 的关系理解成：

- `api/`：告诉你 API 是什么、合同是什么、边界是什么
- `guide/`：告诉你应该先学什么、为什么这样写、怎样一步步组合成可运行程序

如果你已经知道自己要找哪个函数，优先去：

- [API 索引](../api/README.md)

如果你还不确定应该先学哪条主线，优先从这里开始，并先打开：

- [ROADMAP.md](ROADMAP.md)

