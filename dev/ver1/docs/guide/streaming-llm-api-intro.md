# 用 XRT 调用流式 LLM API 入门

> 目标：理解为什么“调用 LLM API”在 XRT 里不应被看成一个特殊孤岛，而应建立在 `xhttp + TLS session + xvalue + json + future / coroutine` 这条统一主线上。

[返回教学文档](README.md)

---

## 1. 为什么这是 XRT 的主线能力之一

XRT 的目标是：

> 互联网 + AI 时代的 C 语言基础设施库

这意味着：

- 它不仅要能写网络程序
- 还要能自然支撑 AI 时代常见的调用链

而“调用流式 LLM API”正是最典型的交叉场景之一：

- 结构化请求
- HTTPS
- 流式响应
- 异步编排


## 2. 这件事不要拆成很多孤立问题

很多项目一提到 LLM API，会立刻想到：

- 某家厂商的字段格式
- 某种特定 SSE 事件
- 某个专用 SDK

但在 XRT 当前主线里，更推荐先把问题拆成基础设施层：

1. 请求数据建模
2. JSON 序列化
3. HTTP / TLS 访问
4. 流式读取
5. future / coroutine 编排

也就是说：

- 厂商协议是上层差异
- 主线基础设施应尽量统一


## 3. 推荐主线

当前更推荐先把 AI 调用拆成两条正式路径：

- `xvalue`：构造请求和中间数据
- `json`：做交换格式
- `xhttp`：承担当前 whole-body HTTP/HTTPS 主线
- `xws`：承担 realtime 双向会话主线
- `xtlssession`：提供统一 TLS public surface
- `future / coroutine`：组织异步流程

这里要特别记住一个边界：

- `xhttp` 当前正式 public 主线仍以 whole-body response 为主
- 如果提供方是 WebSocket realtime，直接走 `xws`
- 如果提供方只有 HTTP SSE / chunked 长流，当前还不应写成“`xhttp` 已经完全 formal 的推荐主线”


## 4. 为什么请求对象建议先做成 xvalue

一个典型 LLM 请求本质上也是结构化数据，例如：

- model
- messages
- stream
- temperature
- tools

这些字段非常适合先组织成 `xvalue`，再序列化成 JSON。

这样做的好处是：

- 字段扩展容易
- 结构更清晰
- 多模型供应商更容易共享中间数据模型


## 5. 为什么 future / coroutine 在这里很重要

如果只是一个阻塞式单请求，直接顺序代码也能写。  
但一旦进入更真实的 AI 场景，通常会出现：

- timeout
- cancel
- 多请求并发
- 用户中断
- 工具调用后的多步编排

这时如果没有统一异步主线，代码会很快变得混乱。

future / coroutine 的作用就是：

- future：统一承接异步结果
- coroutine：统一表达多步等待流程


## 6. 流式响应要怎样理解

流式 LLM 调用的重点，不是“一次收到完整结果”，而是：

- 一段时间内不断收到增量
- 每次只拿到部分输出
- 需要边接收边处理，或最终再拼成完整结果

这就说明：

- 网络读取必须是连续的
- 解析和组装逻辑必须清晰
- timeout / cancel 不能是事后补丁

但在当前正式 public surface 下，要把这件事再拆细一层理解：

- 普通 `POST JSON -> JSON response`，直接走 `xhttp`
- realtime 双向流，直接走 `xws + queue + coroutine`
- HTTP SSE / chunked 长流，暂时不要写成 `xhttp` 已经 fully formal 的文档主线


## 7. 为什么不要过早把某个厂商 SDK 当主线

因为 XRT 要做的是基础设施，而不是某一个平台的绑定层。

更稳的做法是：

- 先把通用基础设施做好
- 再在案例层或上层封装厂商差异

这样：

- 核心库不会被某一家接口绑定
- 文档也不会快速过期
- 你的 AI 时代支持能力会更持久


## 8. 推荐学习顺序

如果你要学这条线，建议按下面顺序：

1. 先看 `xvalue + json`
2. 再看 `xhttp + TLS session`
3. 再看 future / coroutine
4. 最后再看流式 LLM API 场景

不要反过来直接从“厂商协议细节”开始。


## 9. 常见误区

### 误区一：LLM API 是特殊场景，不需要复用现有网络主线

不对。

它本质上仍然是：

- 结构化数据
- HTTPS
- 流式网络交互
- 异步编排

### 误区二：只要能发请求就够了

不对。

AI 场景真正的复杂度往往在：

- 流式处理
- timeout
- cancel
- 多步流程编排

### 误区三：必须写成某家厂商 SDK 的样子

不对。

XRT 更适合提供跨厂商、跨场景可复用的基础能力。


## 10. 建议继续阅读

- [XHTTP API](../api/api-xhttp.md)
- [Network TLS API](../api/api-network-tls.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [xvalue 与 JSON 入门](xvalue-json-intro.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)
- [用 XRT 调用一个流式 LLM API](../case/streaming-llm-api.md)

---

**一句话总结：** 在当前 XRT 主线里，流式 LLM API 调用不是一个孤立特例，而是 `xvalue + json + xhttp + TLS session + future / coroutine` 共同组成的一条现代互联网与 AI 基础设施链路。
