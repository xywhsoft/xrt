# 用 XRT 调用一个流式 LLM API

> 这个案例的目标不是绑定某个具体厂商，而是说明：在当前主线里，如何用 `xhttp + xvalue + json + future / coroutine` 组织一个流式 LLM 调用链路。

[返回案例索引](README.md)

---

## 1. 场景

一个典型的 LLM 调用流程通常包括：

- 构造请求 JSON
- 发起 HTTPS 请求
- 持续读取流式响应
- 把增量结果拼装成最终内容
- 在需要时接入 future / coroutine 进行异步编排

这正好覆盖了 XRT 当前主线的几个关键能力：

- `xvalue`
- `json`
- `xhttp`
- `xtlssession`
- future / coroutine


## 2. 推荐主线

如果站在当前 XRT 主线看，一个流式 LLM API 调用更适合这样组织：

1. 用 `xvalue` 构造请求对象
2. 用 JSON 序列化请求体
3. 用 `xhttp` 发起 HTTPS 请求
4. 流式读取响应片段
5. 用 `xvalue` 或文本缓冲维护增量状态
6. 如果需要更复杂的编排，再接入 future / coroutine

重点在于：

- 结构化数据统一走 `xvalue`
- 网络访问统一走当前 HTTP/TLS 主线
- 异步组织统一走 future / coroutine


## 3. 为什么不建议直接手工拼请求和回调

如果直接：

- 手工拼 JSON 字符串
- 手工处理 TLS 细节
- 一层层套回调解析流片段

程序会很快变得难维护。

而按 XRT 当前主线写：

- 请求结构用 `xvalue`
- 对外交换格式用 JSON
- 流式网络能力建立在 HTTP/TLS 主线之上
- 更上层的编排交给协程 / future

这样“互联网 + AI”场景会自然落在一条统一链路上，而不是很多碎片能力的拼凑。


## 4. 请求数据应该怎样组织

最推荐的写法，不是直接拼大段 JSON 字符串，而是先构造 `xvalue`。

例如，一个典型请求模型可以这样理解：

```c
xvalue vReq = xvoCreateTable();
xvalue vMessages;
xvalue vMsg;

xvoTableSetArray( vReq, "messages", 0 );
vMessages = xvoTableGetValue( vReq, "messages", 0 );
xvoArrayAppendTable( vMessages );
vMsg = xvoArrayGetValue( vMessages, 0 );

xvoTableSetText( vMsg, "role", 0, "user", 0, FALSE );
xvoTableSetText( vMsg, "content", 0, "介绍一下 xrt", 0, FALSE );

xvoTableSetText( vReq, "model", 0, "your-model", 0, FALSE );
xvoTableSetBool( vReq, "stream", 0, TRUE );
```

然后再统一序列化成 JSON。

这样做的好处是：

- 请求结构清晰
- 后续加字段容易
- 多厂商 API 之间更容易共享中间模型


## 5. 流式响应如何理解

流式 LLM API 的重点不是“拿到一个完整 JSON”，而是：

- 在一段时间里不断收到增量片段
- 每个片段都可能只包含部分内容
- 最终需要把这些片段拼成完整输出

所以更推荐的处理思路是：

- 网络层：持续读取响应流
- 解析层：识别每个增量片段
- 组装层：把文本或结构化增量拼起来

当前 XRT 的价值是：

- 底层网络和 TLS 主线已经成型
- 上层可以继续站在 `xvalue + json` 和 future / coroutine 上组织这条链


## 6. 协程和 future 在这里起什么作用

如果只是一个同步调用，直接按顺序写就够了。  
但一旦进入更真实的 AI 场景，通常会出现：

- 同时发多个请求
- 超时控制
- 用户取消
- 多步骤 tool / model / post-process 编排

这时 future / coroutine 的作用就出来了：

- future：统一承接异步结果
- 协程：把多步等待写成顺序流程

也就是说，网络层负责“拿到流”，future / coroutine 负责“把整条流程组织清楚”。


## 7. 推荐的增长路径

如果你现在只是想做一个最小可用的 LLM 调用，可以按这个顺序演进：

1. 先做非流式请求
2. 再做流式文本输出
3. 再做错误处理和 timeout
4. 再接入 coroutine / future 编排
5. 最后再扩到多模型、多请求或 tool 调用

不要一上来就把：

- SSE
- 多请求并发
- 工具调用
- 响应缓存

全部揉在一起。


## 8. 当前主线适合承担哪些工作

当前 XRT 主线比较适合承担：

- 请求对象建模
- JSON 编解码
- HTTP/TLS 请求发送
- future / coroutine 编排
- 结构化响应和模板输出

而厂商特定协议细节，例如：

- 特定事件格式
- 特定字段命名
- 特定鉴权头

更适合作为案例层或上层封装处理，而不是写死到 XRT 核心里。


## 9. 为什么这符合 XRT 的定位

XRT 的目标不是做“某一家 LLM API SDK”，而是做：

> 互联网 + AI 时代的 C 语言基础设施库

所以更合理的路线是：

- 提供统一的 runtime
- 提供统一的网络与 TLS 主线
- 提供统一的结构化数据模型
- 提供统一的异步编排模型

然后让不同 AI 场景建立在这条主线上。


## 10. 建议继续阅读

- [XHTTP API](../api/api-xhttp.md)
- [Network TLS API](../api/api-network-tls.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [xvalue 与 JSON 入门](../guide/xvalue-json-intro.md)
- [协程、Future 与 Task 入门](../guide/coroutine-future-task-intro.md)

---

**一句话总结：** 在当前 XRT 主线里，流式 LLM API 调用不应被看成“某家厂商的特殊接口”，而应被看成 `xvalue + json + xhttp + TLS session + future / coroutine` 共同组成的一条现代异步网络链路。
