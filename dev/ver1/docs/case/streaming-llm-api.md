# 用 XRT 组织一个 LLM 调用链

> 这个案例聚焦“`xvalue + json` 负责请求模型，`xhttp` 负责整包 HTTP 调用，`future / coroutine` 负责异步编排，而 `xws` 负责实时双向会话升级”这条 AI 主线。它不是某家厂商 SDK 的翻译版，而是当前 XRT 公开 API 下最稳的一条 LLM 客户端写法。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“调用外部 LLM 服务的客户端程序”，它有 6 个约束：

1. 请求体字段很多，不能继续手拼大段 JSON 字符串
2. 大多数调用是 `POST JSON -> JSON response`
3. 上层流程已经开始需要 timeout、取消和多步编排
4. 未来还可能升级到 WebSocket realtime 会话
5. 代码要同时跑在 Windows / Linux
6. 文档必须严格站在当前 `xrt.h` 已公开的接口上

如果没有一条清晰主线，代码很容易变成：

- 请求 JSON 全靠字符串拼接
- HTTP、TLS、认证头、超时各写一份
- 想异步化时，再外面补一层共享状态和轮询
- 一提到“流式 LLM”，就误以为当前 `xhttp` 已经有完整 SSE 主路径

这个案例要解决的，正是“当前正式 public surface 下，LLM 客户端到底该怎么拆层”。


## 2. 为什么这次不能把所有“流式 LLM”写成同一条假主线

### 2.1 为什么不是直接套厂商 SDK 心智

因为 XRT 的定位不是：

- 某一家模型厂商的绑定层

而是：

- 互联网 + AI 时代的 C 语言基础设施库

所以更稳的做法是先把通用层拆出来：

- 请求数据模型
- JSON 交换格式
- HTTP / TLS
- WebSocket realtime
- future / coroutine 编排


### 2.2 为什么这里要把 HTTP 和 WebSocket 分开讲

因为它们解决的是两种不同的连接形状。

| 连接形状 | 当前更稳的正式主线 | 更适合什么场景 |
|----------|--------------------|----------------|
| `POST JSON -> 完整 JSON 响应` | `xhttp + future / coroutine` | chat completion、embedding、tool 结果提交 |
| 长连接双向实时会话 | `xws + queue + coroutine` | realtime session、持续事件流、双向音视频/文本控制 |

也就是说：

- 不是所有“流式”都等于 HTTP SSE
- 也不是所有 LLM 场景都必须落在 WebSocket


### 2.3 为什么这页不会假装 `xhttp` 已经有正式 SSE 主线

因为当前 `api-xhttp.md` 已经把边界写得很清楚：

- `xhttp` 当前正式 public 主线以 whole-body request / response 为主
- 大规模流式 request body / response body 仍属于后续增强方向

所以这页故意不写不存在的 `xhttp SSE callback` API，而是把当前最稳的路线讲清楚：

1. 先把整包 HTTP / JSON 调用链写稳
2. 真正需要实时双向流时，再升级到 `xws`


## 3. 这条 AI 主线里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `xvalue` | 请求与中间结果模型 | 字段扩展、厂商差异隔离、结构更清楚 |
| `json` | 请求/响应交换格式 | 序列化、反序列化、调试和日志落地 |
| `xhttp` | 当前正式 HTTP/HTTPS 客户端 | 请求对象、响应对象、sync/async 执行 |
| `xtlssession` | TLS 主线 | `https` 不是外挂，而是网络主线一部分 |
| `xnetfuture` | 承接异步 HTTP 结果 | 统一 timeout / wait / value |
| `xcosched` | 顺序编排整个调用流程 | 把“等结果再继续”写成直线代码 |
| `xws` | 实时双向会话升级路径 | WebSocket realtime、事件驱动、长连接会话 |

可以先记一句话：

> `xvalue/json` 负责把 AI 请求说清楚，`xhttp` 负责把请求发出去，`future/coroutine` 负责把这条链编排清楚；如果提供方是 realtime 会话，再把传输层切到 `xws`。


## 4. 先做什么，后做什么

在当前正式 public surface 下，推荐演进顺序是：

1. 先把 `POST JSON -> JSON response` 的整包 HTTP 路径写稳
2. 再把 timeout、取消、future、coroutine 接进去
3. 最后如果服务方提供的是 WebSocket realtime，再切到 `xws + queue + coroutine`

不推荐一上来就把下面 4 件事揉在一起：

- 厂商特定 SSE 事件
- 大段手工 JSON
- 共享状态轮询
- 还没正式 public 的流式 body 假接口


## 5. 代码目标

下面这段完整代码只做当前最稳、最正式的一段：

1. 用 `xvalue` 构造一个 chat 请求对象
2. 用 `xrtStringifyJSON()` 生成请求体
3. 用 `xhttp` 发起异步 HTTPS POST
4. 在协程里用 `xrtNetFutureWaitCoTimeout()` 等待结果
5. 用 `xrtParseJSON()` 把响应再变回 `xvalue`
6. 尝试从 OpenAI-style `choices[0].message.content` 或 `choices[0].text` 中提取一段文本预览

这段代码故意把 `stream` 字段设为 `FALSE`，原因不是“LLM 不需要流式”，而是：

> 当前 `xhttp` 正式主线仍以 whole-body 响应为主，这条路才是今天应该写进正式案例的路径。


## 6. 完整代码

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_CHAT_URL		"https://api.example.com/v1/chat/completions"
#define DEMO_AUTH_HEADER	"Bearer replace-with-your-token"
#define DEMO_CHAT_MODEL		"your-model"
#define DEMO_CHAT_PROMPT	"请用三句话解释 xrt 的定位。"
#define DEMO_VERIFY_PEER	TRUE

typedef struct
{
	const char* sChatURL;
	const char* sAuthHeader;
	const char* sModel;
	const char* sPrompt;
	bool bVerifyPeer;
} DemoLlmHttpConfig;

typedef struct
{
	DemoLlmHttpConfig tCfg;
	xnetengine* pEngine;
} DemoLlmHttpCtx;


static xvalue procBuildChatRequest(const DemoLlmHttpConfig* pCfg)
{
	xvalue pReq = NULL;
	xvalue pMessages = NULL;
	xvalue pMessage = NULL;

	if ( (pCfg == NULL) || (pCfg->sModel == NULL) || (pCfg->sPrompt == NULL) ) {
		return NULL;
	}

	pReq = xvoCreateTable();
	if ( pReq == NULL ) {
		return NULL;
	}

	if ( !xvoTableSetText(
		pReq,
		"model",
		5u,
		(ptr)pCfg->sModel,
		(uint32)strlen(pCfg->sModel),
		FALSE
	) ) {
		goto fail;
	}

	if ( !xvoTableSetBool(pReq, "stream", 6u, FALSE) ) {
		goto fail;
	}

	if ( !xvoTableSetArray(pReq, "messages", 8u) ) {
		goto fail;
	}

	pMessages = xvoTableGetValue(pReq, "messages", 8u);
	if ( (pMessages == NULL) || !xvoArrayAppendTable(pMessages) ) {
		goto fail;
	}

	pMessage = xvoArrayGetValue(pMessages, 0u);
	if ( pMessage == NULL ) {
		goto fail;
	}

	if ( !xvoTableSetText(pMessage, "role", 4u, "user", 4u, FALSE) ) {
		goto fail;
	}

	if ( !xvoTableSetText(
		pMessage,
		"content",
		7u,
		(ptr)pCfg->sPrompt,
		(uint32)strlen(pCfg->sPrompt),
		FALSE
	) ) {
		goto fail;
	}

	return pReq;

fail:
	xvoUnref(pReq);
	return NULL;
}

static bool procInitChatHttpRequest(
	xhttprequest* pReq,
	const DemoLlmHttpConfig* pCfg,
	const char* sJSON,
	size_t iJSONLen
)
{
	if ( (pReq == NULL) || (pCfg == NULL) || (sJSON == NULL) ) {
		return FALSE;
	}

	xrtHttpRequestInit(pReq);

	if ( !xrtHttpRequestSetMethod(pReq, "POST") ) {
		return FALSE;
	}

	if ( !xrtHttpRequestSetURL(pReq, pCfg->sChatURL) ) {
		return FALSE;
	}

	if ( !xrtHttpRequestSetHeader(pReq, "Accept", "application/json") ) {
		return FALSE;
	}

	if ( !xrtHttpRequestSetHeader(pReq, "User-Agent", "xrt-doc-llm/1.0") ) {
		return FALSE;
	}

	if ( (pCfg->sAuthHeader != NULL) && (pCfg->sAuthHeader[0] != '\0') ) {
		if ( !xrtHttpRequestSetHeader(pReq, "Authorization", pCfg->sAuthHeader) ) {
			return FALSE;
		}
	}

	if ( !xrtHttpRequestSetBodyCopy(pReq, sJSON, iJSONLen, "application/json") ) {
		return FALSE;
	}

	xrtHttpRequestSetTimeout(pReq, 30000u);
	xrtHttpRequestSetIdleTimeout(pReq, 5000u);
	xrtHttpRequestSetVerifyPeer(pReq, pCfg->bVerifyPeer);
	return TRUE;
}

static void procPrintBodyPreview(const char* pBody, size_t iBodyLen)
{
	size_t iPreviewLen;

	if ( (pBody == NULL) || (iBodyLen == 0u) ) {
		printf("response preview: (empty)\n");
		return;
	}

	iPreviewLen = (iBodyLen < 160u) ? iBodyLen : 160u;
	printf("response preview: %.*s\n", (int)iPreviewLen, pBody);
}

static const char* procTryAssistantPreview(xvalue pRoot)
{
	xvalue pChoices = NULL;
	xvalue pFirst = NULL;
	xvalue pMessage = NULL;
	const char* sText = NULL;

	if ( (pRoot == NULL) || (xvoType(pRoot) != XVO_DT_TABLE) ) {
		return NULL;
	}

	pChoices = xvoTableGetValue(pRoot, "choices", 7u);
	if ( (pChoices == NULL) || (xvoType(pChoices) != XVO_DT_ARRAY) ) {
		return NULL;
	}

	if ( xvoArrayItemCount(pChoices) == 0u ) {
		return NULL;
	}

	pFirst = xvoArrayGetValue(pChoices, 0u);
	if ( (pFirst == NULL) || (xvoType(pFirst) != XVO_DT_TABLE) ) {
		return NULL;
	}

	pMessage = xvoTableGetValue(pFirst, "message", 7u);
	if ( (pMessage != NULL) && (xvoType(pMessage) == XVO_DT_TABLE) ) {
		sText = xvoTableGetText(pMessage, "content", 7u);
		if ( (sText != NULL) && (sText[0] != '\0') ) {
			return sText;
		}
	}

	sText = xvoTableGetText(pFirst, "text", 4u);
	if ( (sText != NULL) && (sText[0] != '\0') ) {
		return sText;
	}

	return NULL;
}

static void procRunChatHttpCo(ptr pArg)
{
	DemoLlmHttpCtx* pCtx = (DemoLlmHttpCtx*)pArg;
	xvalue pReqModel = NULL;
	char* sReqJSON = NULL;
	size_t iReqJSONLen = 0u;
	xhttprequest tReq;
	xnetfuture* pFuture = NULL;
	xhttpresponse* pResp = NULL;
	xvalue pRespModel = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	const char* sEnvelopeId = NULL;
	const char* sEnvelopeModel = NULL;
	const char* sAssistantText = NULL;
	const char* sContentType = NULL;

	memset(&tReq, 0, sizeof(tReq));

	if ( (pCtx == NULL) || (pCtx->pEngine == NULL) ) {
		return;
	}

	pReqModel = procBuildChatRequest(&pCtx->tCfg);
	if ( pReqModel == NULL ) {
		printf("build request model failed\n");
		goto cleanup;
	}

	sReqJSON = xrtStringifyJSON(pReqModel, FALSE, &iReqJSONLen);
	if ( sReqJSON == NULL ) {
		printf("stringify request failed\n");
		goto cleanup;
	}

	if ( !procInitChatHttpRequest(&tReq, &pCtx->tCfg, sReqJSON, iReqJSONLen) ) {
		printf("init http request failed\n");
		goto cleanup;
	}

	pFuture = xrtHttpExecuteAsync(pCtx->pEngine, &tReq);
	if ( pFuture == NULL ) {
		printf("http async execute failed\n");
		goto cleanup;
	}

	iStatus = xrtNetFutureWaitCoTimeout(pFuture, 30000u);
	if ( iStatus != XRT_NET_OK ) {
		printf("http wait failed: status=%d\n", (int)iStatus);
		goto cleanup;
	}

	pResp = (xhttpresponse*)xrtNetFutureValue(pFuture);
	if ( pResp == NULL ) {
		printf("http response missing\n");
		goto cleanup;
	}

	sContentType = xrtHttpResponseHeader(pResp, "Content-Type");
	printf(
		"http result: code=%u body-bytes=%u content-type=%s\n",
		(unsigned)pResp->iStatusCode,
		(unsigned)pResp->iBodyLen,
		(sContentType != NULL) ? sContentType : "(missing)"
	);

	if ( (pResp->pBody == NULL) || (pResp->iBodyLen == 0u) ) {
		printf("response body empty\n");
		goto cleanup;
	}

	pRespModel = xrtParseJSON(pResp->pBody, pResp->iBodyLen);
	if ( pRespModel == NULL ) {
		printf("response json parse failed\n");
		procPrintBodyPreview(pResp->pBody, pResp->iBodyLen);
		goto cleanup;
	}

	sEnvelopeId = xvoTableGetText(pRespModel, "id", 2u);
	sEnvelopeModel = xvoTableGetText(pRespModel, "model", 5u);

	printf(
		"json envelope: id=%s model=%s\n",
		(sEnvelopeId != NULL) ? sEnvelopeId : "(missing)",
		(sEnvelopeModel != NULL) ? sEnvelopeModel : "(missing)"
	);

	sAssistantText = procTryAssistantPreview(pRespModel);
	if ( sAssistantText != NULL ) {
		printf("assistant preview: %s\n", sAssistantText);
	} else {
		printf("assistant preview unavailable: provider schema differs from OpenAI-style choices envelope\n");
		procPrintBodyPreview(pResp->pBody, pResp->iBodyLen);
	}

cleanup:
	if ( pRespModel != NULL ) {
		xvoUnref(pRespModel);
	}
	if ( pResp != NULL ) {
		xrtHttpResponseDestroy(pResp);
	}
	if ( pFuture != NULL ) {
		xrtNetFutureDestroy(pFuture);
	}
	xrtHttpRequestUnit(&tReq);
	if ( sReqJSON != NULL ) {
		xrtFree(sReqJSON);
	}
	if ( pReqModel != NULL ) {
		xvoUnref(pReqModel);
	}
}

int main(void)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xcosched* pSched = NULL;
	DemoLlmHttpCtx tCtx;
	int iExitCode = 1;

	xrtInit();
	memset(&tCtx, 0, sizeof(tCtx));

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		goto cleanup;
	}

	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		goto cleanup;
	}

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		goto cleanup;
	}

	tCtx.pEngine = pEngine;
	tCtx.tCfg.sChatURL = DEMO_CHAT_URL;
	tCtx.tCfg.sAuthHeader = DEMO_AUTH_HEADER;
	tCtx.tCfg.sModel = DEMO_CHAT_MODEL;
	tCtx.tCfg.sPrompt = DEMO_CHAT_PROMPT;
	tCtx.tCfg.bVerifyPeer = DEMO_VERIFY_PEER;

	if ( xrtCoSchedSpawn(pSched, procRunChatHttpCo, &tCtx, 0) == NULL ) {
		goto cleanup;
	}

	xrtCoSchedRun(pSched);
	iExitCode = 0;

cleanup:
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
		pSched = NULL;
	}
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		pEngine = NULL;
	}
	xrtUnit();
	return iExitCode;
}
```


## 7. 这段代码怎么读

### 7.1 先看 `xvalue -> json`

这页真正想先讲透的是：

- 不要手拼大段请求 JSON

而应该先：

1. `xvoCreateTable()`
2. `xvoTableSetText()`
3. `xvoTableSetArray()`
4. `xvoArrayAppendTable()`
5. `xrtStringifyJSON()`

这样请求对象的扩展会稳定很多：

- 加 `temperature`
- 加 `tools`
- 加 `response_format`
- 加多条 messages

都不需要回到字符串拼接地狱。


### 7.2 再看 `xhttp + xnetfuture`

这段代码没有走同步阻塞，而是故意写成：

- `xrtHttpExecuteAsync()`
- `xrtNetFutureWaitCoTimeout()`

原因是 AI 场景里你迟早会需要：

- timeout
- cancel
- 多步编排
- 多请求并发

也就是说，这页不是为了炫“异步”，而是为了让这条链从第一天起就站在将来能扩展的主线上。


### 7.3 为什么这里把 `stream` 设成 `FALSE`

因为当前正式 public 主线要讲真实能力边界。

这页不是说：

- LLM 不需要流式

而是说：

- 当前 `xhttp` 正式文档还没有把大规模 HTTP 流式 response body 做成推荐主路径

所以正式案例必须先把：

- whole-body HTTP JSON

这条今天已经稳定的路径写清楚。


### 7.4 响应解析为什么只做“OpenAI-style 尝试性预览”

示例里会尝试读取：

- `choices[0].message.content`
- `choices[0].text`

这是为了演示怎样把：

- 响应 JSON

重新拉回：

- `xvalue`

但代码也明确写了 fallback：

- 如果提供方 schema 不同，就只打印 envelope 和 body preview

这样文档既有教学价值，又不会把某一家 schema 写成 XRT 的核心合同。


## 8. 如果你的提供方是 WebSocket realtime

这时传输层就不该继续勉强塞在 `xhttp` 里，而应该直接切到：

- `xws + queue + coroutine`

也就是上一页刚补完的正式会话主线：

- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](xws-session-queue-coroutine.md)

在 realtime 场景里，最自然的写法通常是：

1. 仍然用 `xvalue + json` 构造要发送的事件帧
2. 在 `OnOpen` 里 `xrtWsClientSendText(...)`
3. 在 `OnText` 里 `xrtParseJSON(...)`
4. 把解析后的业务消息推进 `queue`
5. 用 `xcoevent` 唤醒协程编排层

最小发送片段可以写成：

```c
static void procRealtimeOnOpen(ptr pOwner, xwsclient* pClient)
{
	xvalue pFrame = NULL;
	xvalue pInput = NULL;
	xvalue pMsg = NULL;
	char* sJSON = NULL;
	size_t iJSONLen = 0u;

	(void)pOwner;

	pFrame = xvoCreateTable();
	if ( pFrame == NULL ) {
		return;
	}

	if ( !xvoTableSetText(pFrame, "type", 4u, "response.create", 15u, FALSE) ) {
		goto cleanup;
	}

	if ( !xvoTableSetArray(pFrame, "input", 5u) ) {
		goto cleanup;
	}

	pInput = xvoTableGetValue(pFrame, "input", 5u);
	if ( (pInput == NULL) || !xvoArrayAppendTable(pInput) ) {
		goto cleanup;
	}

	pMsg = xvoArrayGetValue(pInput, 0u);
	if ( pMsg == NULL ) {
		goto cleanup;
	}

	(void)xvoTableSetText(pMsg, "role", 4u, "user", 4u, FALSE);
	(void)xvoTableSetText(pMsg, "content", 7u, "hello realtime", 14u, FALSE);

	sJSON = xrtStringifyJSON(pFrame, FALSE, &iJSONLen);
	if ( sJSON != NULL ) {
		(void)xrtWsClientSendText(pClient, sJSON, iJSONLen);
	}

cleanup:
	if ( sJSON != NULL ) {
		xrtFree(sJSON);
	}
	if ( pFrame != NULL ) {
		xvoUnref(pFrame);
	}
}
```

接收侧则不要在 `OnText` 里直接做重活，而应像 `xws-session-queue-coroutine.md` 那样：

- 复制或解析消息
- push 进 `xmpscqwait`
- `xrtCoEventSet(...)`

这条边界比“在回调里直接做完整 AI 编排”稳得多。


## 9. 如果你的提供方只有 HTTP SSE / chunked 长流

这里要特别说清楚，因为这正是最容易把文档写错的地方。

在当前正式 public surface 下：

- 你不应该把它写成“`xhttp` 已经有成熟的 SSE 主路径”

更稳的理解是：

- `xhttp` 当前正式主线仍以 whole-body response 为主
- HTTP 长流 / SSE 仍属于更底层的网络探索或后续增强方向

所以正式文档当前推荐：

1. 普通 completion / JSON API 先走 `xhttp`
2. 真正的实时双向流先走 `xws`
3. HTTP SSE 长流不要伪装成已经完成 formalization 的主路径


## 10. 常见错误

### 10.1 错误一：请求 JSON 还是靠字符串手拼

这会让 schema 一复杂就难维护。


### 10.2 错误二：明明只是整包 HTTP 请求，却不接 `future / coroutine`

短期能跑，后面一加 timeout / cancel / 多请求编排就会开始碎裂。


### 10.3 错误三：把“流式 LLM”自动等同成“当前 `xhttp` 已支持 SSE 正式主线”

这正是这页要避免的误导。


### 10.4 错误四：一进入 realtime，就在 `OnText` 回调里直接做慢业务

更稳的写法是：

- `OnText` 只做协议边界和最小交接
- 业务编排回到 `queue + coroutine`


### 10.5 错误五：把厂商 schema 写死成核心运行时合同

案例层可以演示某种 envelope。

但 XRT 主线真正应该固定的是：

- `xvalue`
- `json`
- `xhttp`
- `xws`
- `future / coroutine`

而不是某一家字段名。


## 11. 建议继续阅读

- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](xhttp-client-proxy-tls.md)
- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](xws-session-queue-coroutine.md)
- [用 Queue + Future 写一个多生产者 Worker](queue-worker-future.md)
- [xvalue 与 JSON 入门](../guide/xvalue-json-intro.md)
- [协程、Future 与 Task 入门](../guide/coroutine-future-task-intro.md)
- [XHTTP API](../api/api-xhttp.md)
- [XWS API](../api/api-xws.md)

---

**一句话总结：** 在当前 XRT 公开 API 下，LLM 客户端最稳的写法不是发明一套“AI 专用异步模型”，而是把问题拆成 `xvalue + json + xhttp + future / coroutine` 的整包 HTTP 主线，以及 `xws + queue + coroutine` 的 realtime 会话主线；这两条路合起来，才是今天真正正式、可维护、不会误导读者的 AI 案例体系。
