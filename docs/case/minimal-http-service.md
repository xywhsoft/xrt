# 用 XRT 写一个最小 HTTP 服务

> 这个案例不是“最短能跑”的炫技版，而是一个可以继续长成真实服务的起点：同步 handler 负责快路径，异步 handler 负责慢路径，数据统一先落到 `xvalue`，再序列化成 JSON。

[返回案例索引](README.md)

---

## 1. 场景

假设你现在要写一个本地 HTTP 服务，需求很简单：

- `GET /health` 返回健康检查 JSON
- `POST /inspect` 回显请求的结构化信息
- `GET /slow/future` 模拟一个慢任务，但最后仍然由 `xhttpd` 统一回包
- `GET /slow/manual` 模拟另一个慢任务，由 worker 线程稍后手工回包

这个需求看起来很小，但它正好能把服务端主线里最重要的边界讲清楚：

- 为什么 HTTP 服务应该直接站在 `xhttpd` 上
- 为什么响应最好先组织成 `xvalue`
- 什么叫同步 handler
- 什么叫异步 handler
- `OnRequestAsync` 返回 future 和手工 `xrtHttpdConnRespond()` 分别适合什么场景


## 2. 这次为什么选 `xhttpd + xvalue + json + future`

这次的目标已经不是“自己造 HTTP 协议栈”，而是“写一个结构正确的服务”。

所以更合理的分层是：

- `xrtInit() / xrtUnit()` 负责 runtime 生命周期
- `xnetengine` 负责网络线程与事件循环
- `xhttpd` 负责 HTTP/1.1 服务端收发、keep-alive、request / response 物化
- `xvalue + json` 负责结构化数据组织与 JSON 输出
- `future + thread task` 负责慢任务的异步收口

这里暂时不引入：

- 路由框架
- 模板系统
- 数据库
- `task group`

原因也很简单：

- 这个案例的重点是“服务端骨架”
- 每个请求最多只挂一个慢任务
- 还没有进入需要多子任务统一收口的阶段

所以这次用一个 plain future 就够了。


## 3. 完整骨架

下面这份代码刻意只使用当前 `xrt.h` 和 `test/test_xnet_httpd.h` 里已经存在的公开接口。

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define APP_TRACE_ID_CAP 64u

typedef struct {
	char sPath[XHTTPD_PATH_CAP];
	char sTraceId[APP_TRACE_ID_CAP];
} appfuturetask;

typedef struct {
	xhttpdconn* pConn;
	char sPath[XHTTPD_PATH_CAP];
} appmanualtask;


static bool procWriteJsonResponse(xhttpdresponse* pResp, uint32 iStatusCode, xvalue vPayload)
{
	str sJson = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( (pResp == NULL) || (vPayload == NULL) ) {
		return false;
	}

	sJson = xrtStringifyJSON(vPayload, FALSE, &iJsonSize);
	if ( sJson == NULL ) {
		return false;
	}

	xrtHttpdResponseSetStatus(pResp, iStatusCode, NULL);
	(void)xrtHttpdResponseSetHeader(pResp, "Cache-Control", "no-store");
	bOk = xrtHttpdResponseSetBodyCopy(pResp, sJson, iJsonSize, "application/json; charset=utf-8");
	xrtFree(sJson);
	return bOk;
}


static void procWriteFallbackError(xhttpdresponse* pResp)
{
	static const char sBody[] = "{\"ok\":false,\"error\":\"internal\"}";

	xrtHttpdResponseSetStatus(pResp, 500u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, sBody, sizeof(sBody) - 1u, "application/json; charset=utf-8");
}


static xvalue procCreateHealthPayload(const xhttpdrequest* pReq, const char* sMode)
{
	xvalue vRes = NULL;
	xvalue vModules = NULL;
	xvalue vItem = NULL;

	vRes = xvoCreateTable();
	if ( vRes == NULL ) {
		return NULL;
	}

	xvoTableSetBool(vRes, "ok", 0, TRUE);
	xvoTableSetText(vRes, "service", 0, "minimal-http-service", 0, FALSE);
	xvoTableSetText(vRes, "method", 0, pReq->sMethod, 0, FALSE);
	xvoTableSetText(vRes, "path", 0, pReq->sPath, 0, FALSE);
	xvoTableSetText(vRes, "mode", 0, sMode, 0, FALSE);
	xvoTableSetInt(vRes, "body_len", 0, (int)pReq->iBodyLen);

	xvoTableSetArray(vRes, "modules", 0);
	vModules = xvoTableGetValue(vRes, "modules", 0);
	if ( vModules != NULL ) {
		vItem = xvoCreateTable();
		if ( vItem != NULL ) {
			xvoTableSetText(vItem, "name", 0, "xhttpd", 0, FALSE);
			xvoTableSetText(vItem, "role", 0, "http-server", 0, FALSE);
			if ( !xvoArrayAppendValue(vModules, vItem, TRUE) ) {
				xvoUnref(vItem);
			}
		}

		vItem = xvoCreateTable();
		if ( vItem != NULL ) {
			xvoTableSetText(vItem, "name", 0, "xvalue", 0, FALSE);
			xvoTableSetText(vItem, "role", 0, "payload-model", 0, FALSE);
			if ( !xvoArrayAppendValue(vModules, vItem, TRUE) ) {
				xvoUnref(vItem);
			}
		}

		vItem = xvoCreateTable();
		if ( vItem != NULL ) {
			xvoTableSetText(vItem, "name", 0, "future", 0, FALSE);
			xvoTableSetText(vItem, "role", 0, "slow-path", 0, FALSE);
			if ( !xvoArrayAppendValue(vModules, vItem, TRUE) ) {
				xvoUnref(vItem);
			}
		}
	}

	return vRes;
}


static xvalue procCreateInspectPayload(const xhttpdrequest* pReq)
{
	xvalue vRes = NULL;
	const char* sContentType = NULL;

	vRes = xvoCreateTable();
	if ( vRes == NULL ) {
		return NULL;
	}

	xvoTableSetBool(vRes, "ok", 0, TRUE);
	xvoTableSetText(vRes, "method", 0, pReq->sMethod, 0, FALSE);
	xvoTableSetText(vRes, "target", 0, pReq->sTarget, 0, FALSE);
	xvoTableSetText(vRes, "path", 0, pReq->sPath, 0, FALSE);
	xvoTableSetText(vRes, "query", 0, pReq->sQuery, 0, FALSE);
	xvoTableSetText(vRes, "version", 0, pReq->sVersion, 0, FALSE);
	xvoTableSetInt(vRes, "body_len", 0, (int)pReq->iBodyLen);

	sContentType = xrtHttpdRequestHeader(pReq, "Content-Type");
	if ( sContentType != NULL ) {
		xvoTableSetText(vRes, "content_type", 0, sContentType, 0, FALSE);
	}

	if ( (pReq->pBody != NULL) && (pReq->iBodyLen > 0u) ) {
		xvoTableSetText(vRes, "body_preview", 0, pReq->pBody, pReq->iBodyLen, TRUE);
	}

	return vRes;
}


static int32 procBuildSlowFuture(ptr pArg, xfuture_result* pOut)
{
	appfuturetask* pTask = (appfuturetask*)pArg;
	xhttpdresponse* pResp = NULL;
	xvalue vRes = NULL;
	bool bOk = false;

	pOut->pValue = NULL;
	xrtSleep(150u);

	if ( pTask == NULL ) {
		return XRT_NET_ERROR;
	}

	pResp = xrtHttpdResponseCreate();
	vRes = xvoCreateTable();
	if ( (pResp == NULL) || (vRes == NULL) ) {
		goto cleanup;
	}

	xvoTableSetBool(vRes, "ok", 0, TRUE);
	xvoTableSetText(vRes, "mode", 0, "async-future", 0, FALSE);
	xvoTableSetText(vRes, "path", 0, pTask->sPath, 0, FALSE);
	xvoTableSetText(vRes, "note", 0, "worker thread returned xhttpdresponse*", 0, FALSE);
	if ( pTask->sTraceId[0] != '\0' ) {
		xvoTableSetText(vRes, "trace_id", 0, pTask->sTraceId, 0, FALSE);
	}

	if ( !procWriteJsonResponse(pResp, 200u, vRes) ) {
		goto cleanup;
	}

	if ( pTask->sTraceId[0] != '\0' ) {
		(void)xrtHttpdResponseSetHeader(pResp, "X-Trace-Id", pTask->sTraceId);
	}

	pOut->pValue = pResp;
	pResp = NULL;
	bOk = true;

cleanup:
	if ( vRes != NULL ) {
		xvoUnref(vRes);
	}
	if ( pResp != NULL ) {
		xrtHttpdResponseDestroy(pResp);
	}
	xrtFree(pTask);
	return bOk ? XRT_NET_OK : XRT_NET_ERROR;
}


static int32 procBuildSlowManual(ptr pArg, xfuture_result* pOut)
{
	appmanualtask* pTask = (appmanualtask*)pArg;
	xhttpdresponse* pResp = NULL;
	xvalue vRes = NULL;
	int32 iRet = XRT_NET_ERROR;

	pOut->pValue = NULL;
	xrtSleep(250u);

	if ( pTask == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( !xrtHttpdConnIsOpen(pTask->pConn) ) {
		iRet = XRT_NET_CLOSED;
		goto cleanup;
	}

	pResp = xrtHttpdResponseCreate();
	vRes = xvoCreateTable();
	if ( (pResp == NULL) || (vRes == NULL) ) {
		goto cleanup;
	}

	xvoTableSetBool(vRes, "ok", 0, TRUE);
	xvoTableSetText(vRes, "mode", 0, "async-manual", 0, FALSE);
	xvoTableSetText(vRes, "path", 0, pTask->sPath, 0, FALSE);
	xvoTableSetText(vRes, "note", 0, "worker thread called xrtHttpdConnRespond()", 0, FALSE);

	if ( !procWriteJsonResponse(pResp, 202u, vRes) ) {
		goto cleanup;
	}

	iRet = xrtHttpdConnRespond(pTask->pConn, pResp);

cleanup:
	if ( vRes != NULL ) {
		xvoUnref(vRes);
	}
	if ( pResp != NULL ) {
		xrtHttpdResponseDestroy(pResp);
	}
	xrtFree(pTask);
	return iRet;
}


static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	xvalue vRes = NULL;
	const char* sTraceId = NULL;
	bool bOk = false;

	(void)pOwner;
	(void)pServer;
	(void)pConn;

	if ( strcmp(pReq->sPath, "/health") == 0 ) {
		vRes = procCreateHealthPayload(pReq, "sync");
		if ( vRes == NULL ) {
			procWriteFallbackError(pResp);
			return true;
		}

		sTraceId = xrtHttpdRequestHeader(pReq, "X-Trace-Id");
		if ( sTraceId != NULL ) {
			(void)xrtHttpdResponseSetHeader(pResp, "X-Trace-Id", sTraceId);
		}

		bOk = procWriteJsonResponse(pResp, 200u, vRes);
		xvoUnref(vRes);
		if ( !bOk ) {
			procWriteFallbackError(pResp);
		}
		return true;
	}

	if ( strcmp(pReq->sPath, "/inspect") == 0 ) {
		vRes = procCreateInspectPayload(pReq);
		if ( vRes == NULL ) {
			procWriteFallbackError(pResp);
			return true;
		}

		bOk = procWriteJsonResponse(pResp, 200u, vRes);
		xvoUnref(vRes);
		if ( !bOk ) {
			procWriteFallbackError(pResp);
		}
		return true;
	}

	return false;
}


static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	xfuture* pFuture = NULL;
	appfuturetask* pFutureTask = NULL;
	appmanualtask* pManualTask = NULL;
	const char* sTraceId = NULL;

	(void)pOwner;
	(void)pServer;

	if ( strcmp(pReq->sPath, "/slow/future") == 0 ) {
		pFutureTask = (appfuturetask*)xrtCalloc(1u, sizeof(appfuturetask));
		if ( pFutureTask == NULL ) {
			return NULL;
		}

		snprintf(pFutureTask->sPath, sizeof(pFutureTask->sPath), "%s", pReq->sPath);
		sTraceId = xrtHttpdRequestHeader(pReq, "X-Trace-Id");
		if ( sTraceId != NULL ) {
			snprintf(pFutureTask->sTraceId, sizeof(pFutureTask->sTraceId), "%s", sTraceId);
		}

		pFuture = xTaskRunThread(procBuildSlowFuture, pFutureTask, 0u);
		if ( pFuture == NULL ) {
			xrtFree(pFutureTask);
		}
		return pFuture;
	}

	if ( strcmp(pReq->sPath, "/slow/manual") == 0 ) {
		pManualTask = (appmanualtask*)xrtCalloc(1u, sizeof(appmanualtask));
		if ( pManualTask == NULL ) {
			return NULL;
		}

		pManualTask->pConn = pConn;
		snprintf(pManualTask->sPath, sizeof(pManualTask->sPath), "%s", pReq->sPath);

		pFuture = xTaskRunThread(procBuildSlowManual, pManualTask, 0u);
		if ( pFuture == NULL ) {
			xrtFree(pManualTask);
		}
		return pFuture;
	}

	return NULL;
}


int main(void)
{
	xnetengineconfig tEngineCfg;
	xhttpdconfig tHttpdCfg;
	xhttpdevents tEvents = {0};
	xnetengine* pEngine = NULL;
	xhttpdserver* pServer = NULL;

	xrtInit();

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( (pEngine == NULL) || (xrtNetEngineStart(pEngine) != XRT_NET_OK) ) {
		fprintf(stderr, "start network engine failed\n");
		goto cleanup;
	}

	xrtHttpdConfigInit(&tHttpdCfg);
	(void)xrtNetAddrParse(&tHttpdCfg.tBindAddr, "127.0.0.1", 8080u);

	tEvents.OnRequest = procOnRequest;
	tEvents.OnRequestAsync = procOnRequestAsync;

	pServer = xrtHttpdCreate(pEngine, &tHttpdCfg, &tEvents, NULL);
	if ( (pServer == NULL) || (xrtHttpdStart(pServer) != XRT_NET_OK) ) {
		fprintf(stderr, "start http server failed\n");
		goto cleanup;
	}

	printf("listen on http://127.0.0.1:%u\n", (unsigned)xrtHttpdBoundPort(pServer));
	printf("GET  /health\n");
	printf("POST /inspect\n");
	printf("GET  /slow/future\n");
	printf("GET  /slow/manual\n");
	printf("press Enter to stop...\n");
	(void)getchar();

cleanup:
	if ( pServer != NULL ) {
		xrtHttpdStop(pServer);
		xrtHttpdDestroy(pServer);
	}
	if ( pEngine != NULL ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}

	xrtUnit();
	return 0;
}
```


## 4. 这份骨架里最关键的 4 个点

### 4.1 `/health` 走同步 handler

这个路径的特点是：

- 计算很快
- 不需要跨线程等待
- 当前回调里就能把响应组好

所以它应该直接在 `OnRequest` 里完成：

- 读 request
- 组织 `xvalue`
- `xrtStringifyJSON()`
- 填充 `xhttpdresponse`

这就是最标准的快路径。


### 4.2 `/inspect` 说明 request 已经被物化好了

这条路主要是为了让读者看到：到了 `OnRequest` 这一层，你拿到的已经不是原始 socket 字节流了，而是结构化 request：

- `pReq->sMethod`
- `pReq->sTarget`
- `pReq->sPath`
- `pReq->sQuery`
- `pReq->sVersion`
- `pReq->pBody / pReq->iBodyLen`
- `xrtHttpdRequestHeader()`

这就是为什么写 HTTP 服务时，不应该再往下退回“自己手工解析协议”。


### 4.3 `/slow/future` 说明“异步但仍由 xhttpd 统一回包”

这条路的流程是：

1. `OnRequestAsync` 识别到慢路径
2. 用 `xTaskRunThread()` 开一个 worker 任务
3. worker 构造 `xhttpdresponse*`
4. 把 `pOut->pValue = pResp`
5. future resolve 后，由 `xhttpd` 统一把这个响应发出去

这种方式适合：

- 慢任务最后仍然只是“产出一个完整响应”
- 你希望连接生命周期和回包时机继续交给 `xhttpd`
- 你不想自己手工在 worker 里操作连接


### 4.4 `/slow/manual` 说明“异步任务自己决定何时回包”

这条路和上一条的区别是：

- worker 最后不返回 `xhttpdresponse*`
- 而是自己调用 `xrtHttpdConnRespond()`

这种方式适合：

- 你已经拿着 `xhttpdconn*`
- 你想在更具体的时刻手工触发回包
- 你后面可能还要把同样的连接交给别的异步链路

但代价也更明显：

- 你要自己处理“连接还开着吗”
- 你要自己负责本地响应对象的销毁
- 你要更清楚地管理 worker 里的所有权

所以最小服务里，优先学会 future 直接返回响应对象；只有确实需要时，再切到 `xrtHttpdConnRespond()`。


## 5. 为什么响应先落到 `xvalue`

这个案例看起来完全可以手工拼字符串，但这里故意不这么写，因为真实服务很快就会遇到下面这些需求：

- 增字段
- 加错误对象
- 返回数组
- 后续接模板、配置、日志或 AI 响应

如果一开始就让 handler 直接拼字符串，后面每长一步都要重构。  
如果一开始就让数据先变成 `xvalue`，增长路径会自然很多。

这个案例里你已经能看到两个收益：

- `/health` 很容易塞进 `modules` 数组
- `/inspect` 很容易把 request 元信息整块转成 JSON


## 6. 运行与验证

服务启动后，可以直接用下面几条请求验证：

```bash
curl http://127.0.0.1:8080/health
curl -X POST http://127.0.0.1:8080/inspect -H "Content-Type: application/json" -d "{\"name\":\"demo\"}"
curl -H "X-Trace-Id: demo-100" http://127.0.0.1:8080/slow/future
curl http://127.0.0.1:8080/slow/manual
```

如果你还想顺手验证客户端主线，可以再加一个最小探测片段：

```c
xhttprequest tReq;
xhttpresponse* pResp = NULL;
xnet_result iStatus = XRT_NET_ERROR;
char sURL[128];
xvalue vJson = NULL;

xrtHttpRequestInit(&tReq);
(void)xrtHttpRequestSetMethod(&tReq, "GET");
snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/health", (unsigned)xrtHttpdBoundPort(pServer));
(void)xrtHttpRequestSetURL(&tReq, sURL);

pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
if ( (iStatus == XRT_NET_OK) && (pResp != NULL) ) {
	printf("status=%u\n", (unsigned)pResp->iStatusCode);
	printf("body=%.*s\n", (int)pResp->iBodyLen, pResp->pBody ? pResp->pBody : "");

	vJson = xrtParseJSON(pResp->pBody ? pResp->pBody : "{}", pResp->iBodyLen);
	if ( vJson != NULL ) {
		printf("service=%s\n", xvoTableGetText(vJson, "service", 0));
		xvoUnref(vJson);
	}
}

if ( pResp != NULL ) {
	xrtHttpResponseDestroy(pResp);
}
```


## 7. 这个案例故意没有做什么

为了把边界讲清楚，这里故意没加下面这些东西：

- 路由表
- 模板渲染
- 数据库
- `task group`
- TLS

不是它们不重要，而是它们会把问题混在一起。

当前这页只想让你先把 3 个基础动作学稳：

- 用 `xhttpd` 接住 HTTP request
- 用 `xvalue + json` 产出结构化响应
- 用 future 把慢路径从同步 handler 里拆出去

等这 3 件事稳了，再往上接模板、配置、数据库、TLS，主线会清晰很多。


## 8. 常见错误

### 错误一：慢任务还写在 `OnRequest` 里

这样会把 handler 线程直接卡住。  
如果这条路径会等待外部资源、sleep、跑重计算，就应该考虑 `OnRequestAsync`。

### 错误二：异步 worker 里直接长期依赖 request 指针

当前合同里，请求会保持到异步请求结束，但如果你还要跨更长的边界，最稳的做法仍然是把自己需要的字段复制出来。这个案例里复制了 `sPath` 和 `X-Trace-Id`，就是为了把边界写清楚。

### 错误三：手工 `xrtHttpdConnRespond()` 以后忘了销毁本地响应对象

`xrtHttpdConnRespond()` 不是“把对象所有权转走后你就不用管了”的写法。  
测试代码和这个案例都按“发送后本地销毁”的方式组织。

### 错误四：最小服务一开始就引入过多框架层

如果你的目标只是先站稳服务端主线，那么：

- 一个 `xnetengine`
- 一个 `xhttpdserver`
- 一组 handler

就够了。  
先把边界讲清楚，比先把功能堆满更重要。


## 9. 建议继续阅读

- [XHTTPD API](../api/api-xhttpd.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [最小 HTTP 服务入门](../guide/minimal-http-service-intro.md)
- [HTTP + JSON + Template 完整链路](http-json-template-chain.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](xhttp-client-proxy-tls.md)

---

**一句话总结：** 最小 HTTP 服务真正该学的不是“怎么把 200 OK 发出去”，而是“快路径留在同步 handler，慢路径交给 future，响应统一先组织成 `xvalue`，再交给 `xhttpd` 发出去”。
