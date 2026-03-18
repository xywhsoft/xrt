# XRT 范例解析

> 面向“完整链路、组合能力、真实工程片段”的案例入口。这里与 `docs/guide/` 的区别是：`guide/` 偏教学路径，`case/` 偏成体系实战拆解。

[返回文档中心](../README.md)

---

## 目录

- [范例解析的定位](#范例解析的定位)
- [推荐案例方向](#推荐案例方向)
- [适合优先补齐的案例](#适合优先补齐的案例)
- [与 API / 教学文档的关系](#与-api--教学文档的关系)

---

## 范例解析的定位

`case/` 目录后续主要承载：

- 一个完整问题如何用 XRT 组合解决
- 多个子系统怎样协同工作
- 从基础运行时一路走到网络、并发、模板、JSON、AI 场景的“成体系用法”

因此，这里的文档更适合写成：

- 场景背景
- 设计思路
- 模块选型
- 关键代码片段
- 常见错误点
- 为什么这样做

---

## 推荐案例方向

当前最值得做案例解析的方向有：

### 1. JSON / Value 组合案例

例如：

- 配置加载
- 动态对象组装
- 请求/响应数据建模

### 2. 模板渲染案例

例如：

- 用 `xvalue + template` 渲染 HTML
- 报表输出
- 代码生成

### 3. 线程、协程、future 协同案例

例如：

- 线程负责并行
- 协程负责单线程编排
- future / task / promise 负责统一异步结果

### 4. `xnet-v2` 案例

例如：

- stream readable / writable / drain / close
- dgram recv
- listener accept
- wait-source 统一等待

### 5. 网络应用层案例

例如：

- HTTP client 调用第三方 API
- HTTP server 暴露服务
- WebSocket 双向通信
- TLS session 作为统一安全层

### 6. 互联网 + AI 场景案例

例如：

- 调用 LLM API
- 流式读取响应
- 用 future / coroutine 编排多步异步流程
- 用 JSON / template 构造请求和展示结果

---

## 当前已完成的案例页

当前已经落地的正式案例包括：

1. [用 xvalue + json 构造配置系统](json-config-system.md)
2. [用 Template 渲染一个 HTML 页面](template-render-html.md)
3. [线程、协程与 Future 协同模型](thread-coroutine-future.md)
4. [xnet-v2 Stream Wait-Source 实战](xnet-stream-wait-source.md)
5. [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
6. [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)
7. [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)

这些案例已经覆盖了：

- `xvalue + json`
- `template`
- 线程 / 协程 / future
- `xnet-v2` wait-source
- HTTP 服务
- 流式 LLM API
- `HTTP + JSON + template` 完整链路

后续仍可继续补：

- `xhttp` 客户端深度案例
- `xws` 双向会话案例
- `dgram recv` 与 `listener accept` 的正式案例

---

## 与 API / 教学文档的关系

建议理解为：

- `api/`：模块合同和能力边界
- `guide/`：从零到一教学路线
- `case/`：多模块协同的完整问题拆解

如果你已经知道每个模块的 API，但还不知道怎么把它们串起来，就应该优先看这里。

---

**当前状态：案例入口、案例方向与第一批正式案例页已经建立，当前可作为中文主线案例入口使用。**
