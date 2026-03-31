# XRT 范例解析

> 面向“完整链路、组合能力、真实工程片段”的案例入口。这里与 `docs/guide/` 的区别是：`guide/` 偏教学路径，`case/` 偏完整问题拆解与工程组合。

[返回文档中心](../README.md)

---

## 目录

- [范例解析的定位](#范例解析的定位)
- [当前状态](#当前状态)
- [目标案例梯度](#目标案例梯度)
- [现有页面如何使用](#现有页面如何使用)
- [当前空缺](#当前空缺)
- [与 API / 教学文档的关系](#与-api--教学文档的关系)

---

## 范例解析的定位

`case/` 目录后续主要承载：

- 一个完整问题如何用 XRT 组合解决
- 多个子系统怎样协同工作
- 从基础运行时一路走到网络、并发、模板、JSON、AI 场景的整条链路

正式案例页应至少覆盖：

- 场景背景
- 目标与约束
- 模块选型和为什么不用别的方案
- 目录结构与关键代码
- 运行步骤
- 常见错误点
- 可以继续扩展的方向


## 当前状态

当前 `case/` 目录已经有一批可读页面，但整体仍处于重建中：

- 现有页面更像“专题说明”而不是“完整教学案例”
- 适合快速理解模块如何组合
- 还没有形成从简单到复杂的案例阶梯
- 还不能覆盖 XRT 全部公开模块

如果你想看后续正式教学顺序，建议同时阅读：

- [教学重建路线图](../guide/ROADMAP.md)


## 目标案例梯度

后续正式案例建议按下面顺序重建：

1. 配置系统：`file + path + value + json`
2. 模板渲染页面：`value + template + file`
3. 多任务 worker：`thread + queue + future`
4. 子进程与异步文件流水线：`subprocess + file_async + future`
5. HTTP 客户端调用链：`xurl + http util + xhttp + TLS + proxy`
6. HTTP 服务：`xhttpd + json + template + value`
7. WebSocket 会话服务：`xws + coroutine + queue`
8. 流式 LLM API：`xhttp/xws + future + coroutine + json`
9. 完整小项目：把配置、日志、任务、网络和模板串成一个可运行程序

这条梯度的目标不是只展示“能跑”，而是展示：

- 为什么这样拆模块
- 哪一层适合线程，哪一层适合协程
- 哪一层应该统一进 future / wait-source
- 如何把基础模块一路升级成完整功能


## 现有页面如何使用

当前已有页面可以作为重建前的过渡参考：

1. [用 xvalue + json 构造配置系统](json-config-system.md)
2. [用 Template 渲染一个 HTML 页面](template-render-html.md)
3. [线程、协程与 Future 协同模型](thread-coroutine-future.md)
4. [xnet-v2 Stream Wait-Source 实战](xnet-stream-wait-source.md)
5. [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
6. [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)
7. [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)

这些页面当前适合：

- 快速确认一个组合方向是否已经进入主线
- 先拿到模块串联方式
- 在正式案例重写前作为结构草稿使用


## 当前空缺

当前仍缺少正式案例的方向包括：

- `queue` 驱动的生产者/消费者案例
- `subprocess + file_async` 工具链案例
- `xhttp` 客户端与代理/TLS 案例
- `xws` 双向会话案例
- `crypto / regex / charset` 在实际项目中的组合案例
- 内存池、容器与数据结构在真实业务中的用法案例


## 与 API / 教学文档的关系

建议理解为：

- `api/`：模块合同和能力边界
- `guide/`：从零到一教学路线
- `case/`：多个模块如何协同解决一个完整问题

如果你已经知道每个模块的大致 API，但还不知道怎么把它们串起来，就应该优先看这里。
