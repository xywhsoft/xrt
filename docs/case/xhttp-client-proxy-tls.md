# 用 XHTTP 走完 URL、代理与 TLS 客户端调用链

> 这个案例聚焦“解析 URL -> 配置代理 -> 发起 HTTPS 请求 -> 同步/异步等待 -> 释放连接与结果”这条主线，把 `xurl + xhttp + proxy + TLS + future` 串成一个完整可复用的客户端骨架。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“调用外部 HTTPS API 的客户端程序”，它有 6 个约束：

1. 需要先确认 URL 到底会连去哪
2. 某些环境要直连，某些环境要走代理
3. 大多数请求是 HTTPS，不只是明文 HTTP
4. 有些调用只想同步拿结果，有些调用要挂到异步链路里
5. 同一 origin 的多个请求，希望尽量复用 keep-alive 连接
6. 代码要能在 Windows / Linux 上保持同一套写法

如果没有一条清晰主线，代码很容易变成：

- URL 解析自己拼
- 代理配置散在每个连接点上
- TLS 被当成“额外补丁”，而不是主线一部分
- 同步和异步路径各自维护一套完全不同的调用方式

这个案例要解决的，正是“一个 HTTPS API 客户端怎样沿着 XRT 当前正式网络主线写出来”。


## 2. 为什么这次不用别的方案

### 2.1 为什么不是直接用 `xnet_stream`

因为这次的问题不是：

- 手工管理 socket、可写回调、可读回调
- 自己拼 HTTP 请求行和 header
- 自己驱动 chunked、keep-alive 和 response 解析

如果你只是要调用 HTTP API，直接站在 `xhttp` 之上更合适。


### 2.2 为什么不是只会写 `xhttp`

因为客户端真正的边界不只在 `ExecuteSync/Async` 上。

你还需要想清楚：

- URL 怎么拆
- `Host` 头会长什么样
- HTTPS 什么时候进入 TLS
- 代理对象的生命周期怎么管理
- sync hidden-engine 路径和 explicit engine 路径有什么区别

所以这页故意把 `xurl + xhttp + proxy + TLS + future` 放在一条链里讲。


### 2.3 为什么这里不直接讲 `xhttp_util`

因为大多数业务代码不会直接调用它。

但这并不代表它不重要。当前这条链里：

- `xurl` 负责 URL 视图与 `Host` 头
- `xhttp_util` 负责 header/token/chunked 等 HTTP 文本细节
- `xhttp` 把它们收成正式客户端入口

所以你可以把 `xhttp_util` 理解成：

- 这条链里你通常“不直接拿来写业务代码”，但 `xhttp` 又离不开的幕后层


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `xurl` | 解析 URL、拷贝 target、生成 `Host` 头 | 提前看清楚“这次请求到底会怎么连” |
| `xhttp_util` | `xhttp` 内部使用的 HTTP 文本辅助层 | header/token/chunked 细节不必手工重写 |
| `xnetproxy` | 共享代理对象 | 直连、HTTP CONNECT、SOCKS5 的统一入口 |
| `xtlssession` | `https` 路径下的 TLS 会话主线 | TLS 是网络主线一部分，不是临时外挂 |
| `xhttp` | 正式 HTTP 客户端入口 | 请求对象、响应对象、sync/async 执行 |
| `xnetengine / xnetfuture` | 显式 engine 与异步等待 | keep-alive 复用、future 等待、统一异步语义 |

可以先记一句话：

> `xurl` 负责把目标解释清楚，`xhttp` 负责把请求发出去，`proxy/TLS` 负责中间链路，而 `xnetfuture` 负责把 async 结果接进统一等待模型。


## 4. 代码目标

下面这段完整代码会做 5 件事：

1. 先用 `xurl` 解析一个 HTTPS URL，并打印 `Host` 头和 target
2. 按配置创建一个可选共享代理对象
3. 走 `ExecuteSync(NULL, ...)` 的 hidden-engine 路径做一次同步探测
4. 再创建显式 engine，连续发两次异步请求，演示同 origin + 同代理的复用写法
5. 在最后统一释放 response、future、request、proxy 和 idle 连接

这段代码是一个“真实客户端骨架”，不是只讲单个函数的片段说明。

需要注意：

- 下面的 URL 和代理参数是模板值，写自己的程序时请替换成真实地址
- 如果你接的是本地自签名测试服务，可以把 `bVerifyPeer` 设为 `FALSE`


## 5. 完整代码

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_HEALTH_URL	"https://api.example.com/v1/health"
#define DEMO_LIST_URL	"https://api.example.com/v1/items?limit=10"
#define DEMO_DETAIL_URL	"https://api.example.com/v1/items/42"

#define DEMO_USE_PROXY	FALSE
#define DEMO_VERIFY_PEER	TRUE
#define DEMO_PROXY_HOST	"127.0.0.1"
#define DEMO_PROXY_PORT	7897u

typedef struct
{
	const char* sHealthURL;
	const char* sListURL;
	const char* sDetailURL;
	const char* sAuthHeader;
	bool bUseProxy;
	bool bVerifyPeer;
	const char* sProxyHost;
	uint16 iProxyPort;
} DemoHttpClientConfig;

static void procExplainURL(const char* sURL)
{
	xrturlview tURL;
	char sHostHeader[320];
	char sTarget[2048];

	if ( sURL == NULL ) {
		return;
	}

	if ( !xrtUrlParseView(sURL, &tURL) ) {
		printf("url parse failed: %s\n", sURL);
		return;
	}

	if ( !xrtUrlMakeHostHeader(&tURL, sHostHeader, sizeof(sHostHeader)) ) {
		strcpy(sHostHeader, "(make host header failed)");
	}

	if ( !xrtUrlViewCopyTargetTo(&tURL, sTarget, sizeof(sTarget)) ) {
		strcpy(sTarget, "(copy target failed)");
	}

	printf(
		"url info: scheme=%.*s host=%.*s port=%u secure=%d\n",
		(int)tURL.tScheme.iLen,
		tURL.tScheme.sPtr,
		(int)tURL.tHost.iLen,
		tURL.tHost.sPtr,
		(unsigned)tURL.iPort,
		(int)((tURL.iFlags & XRT_URL_F_SECURE) != 0u)
	);
	printf("url info: host-header=%s target=%s\n", sHostHeader, sTarget);
}

static xnetproxy* procCreateSharedProxy(const DemoHttpClientConfig* pCfg)
{
	xnetproxyconfig tProxyCfg;

	if ( pCfg == NULL || !pCfg->bUseProxy ) {
		return NULL;
	}

	xrtNetProxyConfigInit(&tProxyCfg);
	tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
	snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "%s", pCfg->sProxyHost);
	tProxyCfg.iPort = pCfg->iProxyPort;
	return xrtNetProxyCreate(&tProxyCfg);
}

static bool procInitCommonRequest(
	xhttprequest* pReq,
	const DemoHttpClientConfig* pCfg,
	const char* sMethod,
	const char* sURL,
	xnetproxy* pProxy
)
{
	if ( pReq == NULL || pCfg == NULL || sMethod == NULL || sURL == NULL ) {
		return FALSE;
	}

	xrtHttpRequestInit(pReq);

	if ( !xrtHttpRequestSetMethod(pReq, sMethod) ) {
		return FALSE;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return FALSE;
	}

	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	(void)xrtHttpRequestSetHeader(pReq, "User-Agent", "xrt-doc-case/1.0");

	if ( pCfg->sAuthHeader != NULL && pCfg->sAuthHeader[0] != '\0' ) {
		(void)xrtHttpRequestSetHeader(pReq, "Authorization", pCfg->sAuthHeader);
	}

	xrtHttpRequestSetTimeout(pReq, 10000u);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, pCfg->bVerifyPeer);
	pReq->pProxy = (pProxy != NULL) ? xrtNetProxyAddRef(pProxy) : NULL;
	return TRUE;
}

static void procPrintResponseSummary(const char* sStep, const xhttpresponse* pResp)
{
	const char* sType;

	if ( sStep == NULL || pResp == NULL ) {
		return;
	}

	sType = xrtHttpResponseHeader(pResp, "Content-Type");
	printf(
		"%s: code=%u body-bytes=%u content-type=%s\n",
		sStep,
		(unsigned)pResp->iStatusCode,
		(unsigned)pResp->iBodyLen,
		(sType != NULL) ? sType : "(missing)"
	);
}

static bool procRunSyncProbe(const DemoHttpClientConfig* pCfg, xnetproxy* pProxy)
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	xnetengine* pHidden = NULL;
	bool bOk = FALSE;

	memset(&tReq, 0, sizeof(tReq));

	if ( !procInitCommonRequest(&tReq, pCfg, "GET", pCfg->sHealthURL, pProxy) ) {
		printf("sync probe init failed\n");
		goto cleanup;
	}

	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	if ( iStatus != XRT_NET_OK || pResp == NULL ) {
		printf("sync probe failed: status=%d\n", (int)iStatus);
		goto cleanup;
	}

	procPrintResponseSummary("sync probe", pResp);
	bOk = TRUE;

cleanup:
	if ( pResp != NULL ) {
		xrtHttpResponseDestroy(pResp);
	}
	xrtHttpRequestUnit(&tReq);

	pHidden = xrtNetSyncGetHiddenEngine();
	if ( pHidden != NULL ) {
		xrtHttpCloseIdleConnections(pHidden);
	}
	xrtNetSyncShutdownHiddenEngine();
	return bOk;
}

static xhttpresponse* procWaitHttpFuture(const char* sStep, xnetfuture* pFuture, xnet_result* pStatus)
{
	if ( pStatus == NULL ) {
		return NULL;
	}

	*pStatus = XRT_NET_ERROR;

	if ( pFuture == NULL ) {
		printf("%s failed: future missing\n", (sStep != NULL) ? sStep : "http async");
		return NULL;
	}

	*pStatus = xrtNetFutureWait(pFuture, 10000u);
	if ( *pStatus != XRT_NET_OK ) {
		printf("%s failed: future status=%d\n", (sStep != NULL) ? sStep : "http async", (int)*pStatus);
		return NULL;
	}

	return (xhttpresponse*)xrtNetFutureValue(pFuture);
}

static bool procRunAsyncPair(const DemoHttpClientConfig* pCfg, xnetproxy* pProxy)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xhttprequest tReqA;
	xhttprequest tReqB;
	xnetfuture* pFutureA = NULL;
	xnetfuture* pFutureB = NULL;
	xhttpresponse* pRespA = NULL;
	xhttpresponse* pRespB = NULL;
	xnet_result iStatusA = XRT_NET_ERROR;
	xnet_result iStatusB = XRT_NET_ERROR;
	bool bOk = FALSE;

	memset(&tReqA, 0, sizeof(tReqA));
	memset(&tReqB, 0, sizeof(tReqB));

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		printf("client engine create failed\n");
		goto cleanup;
	}
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		printf("client engine start failed\n");
		goto cleanup;
	}

	if ( !procInitCommonRequest(&tReqA, pCfg, "GET", pCfg->sListURL, pProxy) ) {
		printf("async request A init failed\n");
		goto cleanup;
	}
	if ( !procInitCommonRequest(&tReqB, pCfg, "GET", pCfg->sDetailURL, pProxy) ) {
		printf("async request B init failed\n");
		goto cleanup;
	}

	pFutureA = xrtHttpExecuteAsync(pEngine, &tReqA);
	pRespA = procWaitHttpFuture("async request A", pFutureA, &iStatusA);
	if ( pRespA == NULL ) {
		goto cleanup;
	}
	procPrintResponseSummary("async request A", pRespA);

	/*
	 * 这里故意把第二个请求放在第一个请求完成之后再发，
	 * 这样最容易沿着“同 origin + 同代理 => keep-alive 可复用”的主线理解。
	 */
	pFutureB = xrtHttpExecuteAsync(pEngine, &tReqB);
	pRespB = procWaitHttpFuture("async request B", pFutureB, &iStatusB);
	if ( pRespB == NULL ) {
		goto cleanup;
	}
	procPrintResponseSummary("async request B", pRespB);

	bOk = TRUE;

cleanup:
	if ( pRespA != NULL ) {
		xrtHttpResponseDestroy(pRespA);
	}
	if ( pRespB != NULL ) {
		xrtHttpResponseDestroy(pRespB);
	}
	if ( pFutureA != NULL ) {
		xrtNetFutureDestroy(pFutureA);
	}
	if ( pFutureB != NULL ) {
		xrtNetFutureDestroy(pFutureB);
	}
	xrtHttpRequestUnit(&tReqA);
	xrtHttpRequestUnit(&tReqB);

	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	return bOk;
}

int main(void)
{
	DemoHttpClientConfig tCfg;
	xnetproxy* pProxy;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.sHealthURL = DEMO_HEALTH_URL;
	tCfg.sListURL = DEMO_LIST_URL;
	tCfg.sDetailURL = DEMO_DETAIL_URL;
	tCfg.sAuthHeader = "";
	tCfg.bUseProxy = DEMO_USE_PROXY;
	tCfg.bVerifyPeer = DEMO_VERIFY_PEER;
	tCfg.sProxyHost = DEMO_PROXY_HOST;
	tCfg.iProxyPort = DEMO_PROXY_PORT;

	procExplainURL(tCfg.sHealthURL);

	pProxy = procCreateSharedProxy(&tCfg);
	if ( tCfg.bUseProxy && pProxy == NULL ) {
		printf("proxy create failed\n");
		xrtUnit();
		return 1;
	}

	if ( !procRunSyncProbe(&tCfg, pProxy) ) {
		xrtNetProxyRelease(pProxy);
		xrtUnit();
		return 1;
	}

	if ( !procRunAsyncPair(&tCfg, pProxy) ) {
		xrtNetProxyRelease(pProxy);
		xrtUnit();
		return 1;
	}

	xrtNetProxyRelease(pProxy);
	xrtUnit();
	return 0;
}
```


## 6. 这段代码怎么读

### 6.1 先看 `xurl`

程序一开始不是急着发请求，而是先：

- `xrtUrlParseView()`
- `xrtUrlMakeHostHeader()`
- `xrtUrlViewCopyTargetTo()`

这样你能提前看到：

- scheme 是不是 `https`
- `Host` 头会不会带端口
- target 究竟是 `/path?query`

这一步看似“可有可无”，但在代理、重定向和 API 签名场景里非常值钱。


### 6.2 再看代理对象

代理不是写在每个 socket 上的零散配置，而是一个共享对象：

- `xnetproxy`

请求真正使用时，再把一份引用挂到：

- `xhttprequest.pProxy`

也就是说：

- 代理配置创建一次
- 每个请求按需 `AddRef`
- `RequestUnit()` 释放请求持有的那一份引用
- 外层创建者最后再 `xrtNetProxyRelease(pProxy)`

这条生命周期一定要记住。


### 6.3 `https` 路径为什么不需要你手工驱动 TLS

因为这条链里：

- `xhttp` 站在 `xnet-v2 + xtlssession` 主线上

也就是说，当 URL 是 `https://...` 时：

- TLS session 会作为网络主线的一部分参与预打开阶段

你不需要自己去写：

- `FeedCipher`
- `DriveHandshake`
- `ReadPlain`

除非你故意退回到更底层的 `xnet_stream` / session 手工驱动路径。


### 6.4 sync hidden-engine 路径和 explicit engine 路径的区别

同步探测这段故意写成：

- `xrtHttpExecuteSync(NULL, &tReq, &iStatus)`

这表示：

- 让 XRT 走 hidden-engine 路径

适合：

- 一次性同步调用
- 小工具
- 启动时配置探测

而异步两连发那段，则故意写成：

- 显式 `xrtNetEngineCreate()`
- 显式 `xrtHttpExecuteAsync()`

适合：

- 需要 keep-alive 复用
- 需要多请求编排
- 需要继续挂 future / wait-source / coroutine 主线


### 6.5 为什么异步两连发是“顺序发起”

这里不是为了炫并发，而是为了把这条主线讲清楚：

- 同 origin
- 同代理对象
- 同一个 client engine

第二个请求最自然地就能落在 keep-alive 复用的讨论上。

如果你一上来就并发扔很多请求，初学者反而更难分清：

- 连接复用
- 并行发起
- future 等待

这三件事的边界。


## 7. 如果你要升级成 POST JSON

这条客户端骨架升级成 JSON API 调用，最直接的变化通常只有两步：

1. 改方法为 `POST`
2. 用 `xrtHttpRequestSetBodyCopy()` 设置请求体

最小升级片段：

```c
static const char sBody[] = "{\"demo\":true}";

(void)xrtHttpRequestSetMethod(&tReq, "POST");
(void)xrtHttpRequestSetHeader(&tReq, "Accept", "application/json");

if ( !xrtHttpRequestSetBodyCopy(
	&tReq,
	sBody,
	sizeof(sBody) - 1u,
	"application/json"
) ) {
	return FALSE;
}
```

如果你后面再接：

- `xvalue`
- `json`

那就能继续把“字符串请求体”升级成“结构化请求对象”。


## 8. 这条模型最适合什么项目

这条写法最适合：

- REST / JSON API 客户端
- 内部服务调用
- 需要可选代理的企业网络环境
- 需要 keep-alive 复用的中频请求程序
- 站在 future / coroutine 主线之上的上层业务

也就是说，它不是浏览器，也不是超重型 HTTP 框架。

它更像：

- 一个足够轻、又已经把 URL / proxy / TLS / sync / async 主线接好的正式客户端入口


## 9. 常见错误

### 9.1 错误一：URL 只当字符串，不先解析

这样一遇到默认端口、省略端口、target 或代理排障，就容易糊。


### 9.2 错误二：代理对象创建完就直接塞裸指针

请求拿到的是自己的代理引用，不是“随便借一下外面的对象”。


### 9.3 错误三：本地自签名证书调试时忘了显式设置 `VerifyPeer`

这会让你把“证书校验失败”和“请求逻辑失败”混在一起看。


### 9.4 错误四：sync 路径用完 hidden engine，却不做 idle/hidden 清理

短程序里问题不明显，长生命周期工具里会越来越乱。


### 9.5 错误五：响应对象和 future 对象都忘记释放

`xhttpresponse` 和 `xnetfuture` 都是正式对象，不是临时借用值。


## 10. 建议继续阅读

- [xnet-v2 与 TLS Session 入门](../guide/xnet-v2-tls-intro.md)
- [XHTTP API](../api/api-xhttp.md)
- [XURL API](../api/api-xurl.md)
- [Network TLS API](../api/api-network-tls.md)
- [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md)
- [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)

---

**一句话总结：** 这条案例主线的关键不在“发出一个 GET 请求”，而在“先把 URL 解释清楚，再把代理和 TLS 放回网络主线里，最后根据场景选择 hidden-engine sync 或 explicit-engine async”；这才是 XRT 当前 HTTP 客户端最稳的一条写法。
