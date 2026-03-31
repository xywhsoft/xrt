# 一个完整的 HTTP + JSON + Template 服务链路

> 这个案例要讲的不是“会不会渲染模板”，而是一个更关键的问题：当同一个服务既要返回 JSON API，又要返回 HTML 页面时，怎样避免内部数据模型裂成两套。

[返回案例索引](README.md)

---

## 1. 场景

假设你正在写一个本地控制台服务，它同时需要提供两类出口：

- `GET /api/dashboard` 返回给脚本或前端调用的 JSON
- `GET /dashboard` 返回给浏览器直接打开的 HTML 页面

这时最常见的坏味道是：

- API 路由自己拼一套 JSON
- 页面路由再拼一套 HTML 字符串
- 两边字段名和业务逻辑慢慢分叉

当前 XRT 主线更推荐另一种组织方式：

- HTTP 入口统一走 `xhttpd`
- 内部数据统一先组织成 `xvalue`
- JSON 只是 `xvalue` 的一个出口
- HTML 模板只是同一份 `xvalue` 的另一个出口


## 2. 这次为什么选 `xhttpd + xvalue + json + template`

这条组合刚好把 4 层边界拉直：

- `xhttpd` 负责 request / response 生命周期
- `xvalue` 负责服务内部的数据模型
- `json` 负责交换格式
- `template` 负责展示格式

这里最关键的一点不是“模块更多了”，而是：

> JSON 和 HTML 不再是两套互不相干的产物，而是同一份 `xvalue` 在两个出口上的不同投影。

这会直接带来 3 个收益：

- 字段名天然一致
- 业务整理逻辑不会写两遍
- 后续加模板、配置、数据库或 AI 输出时，仍然能沿着同一份对象树继续长


## 3. 完整骨架

下面这份代码把整个链路做成一个最小但完整的服务：

- 启动时预编译 dashboard 模板
- 请求进来后先构造统一的 `xvalue`
- `/api/dashboard` 走 JSON 输出
- `/dashboard` 走 HTML 模板输出

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static const char g_sDashboardTemplate[] =
	"<!doctype html>\n"
	"<html>\n"
	"<head>\n"
	"\t<meta charset=\"utf-8\">\n"
	"\t<title>{$title}</title>\n"
	"</head>\n"
	"<body>\n"
	"\t<h1>{$title}</h1>\n"
	"\t<p>service={$service}</p>\n"
	"\t<p>route={$requested_path}</p>\n"
	"\t{#if:ok}<p>status={$status_text}</p>{#else}<p>status=degraded</p>{#end}\n"
	"\t<ul>\n"
	"\t{#foreach:cards}\n"
	"\t\t<li><strong>{$title}</strong>: {$value_text}</li>\n"
	"\t{#end}\n"
	"\t</ul>\n"
	"</body>\n"
	"</html>\n";

typedef struct {
	xteengine hTemplateEngine;
	xtetemplate hDashboardTemplate;
} appstate;


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


static bool procWriteHtmlResponse(xhttpdresponse* pResp, xtetemplate hTemplate, xvalue vPayload)
{
	char* sHtml = NULL;
	size_t iHtmlSize = 0;
	bool bOk = false;

	if ( (pResp == NULL) || (hTemplate == NULL) || (vPayload == NULL) ) {
		return false;
	}

	sHtml = xteMake(hTemplate, vPayload, NULL, NULL, &iHtmlSize);
	if ( sHtml == NULL ) {
		return false;
	}

	xrtHttpdResponseSetStatus(pResp, 200u, NULL);
	(void)xrtHttpdResponseSetHeader(pResp, "Cache-Control", "no-store");
	bOk = xrtHttpdResponseSetBodyCopy(pResp, sHtml, iHtmlSize, "text/html; charset=utf-8");
	xrtFree(sHtml);
	return bOk;
}


static void procWriteFallbackError(xhttpdresponse* pResp)
{
	static const char sBody[] = "{\"ok\":false,\"error\":\"internal\"}";

	xrtHttpdResponseSetStatus(pResp, 500u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, sBody, sizeof(sBody) - 1u, "application/json; charset=utf-8");
}


static xvalue procBuildDashboardModel(const xhttpdrequest* pReq)
{
	xvalue vPage = NULL;
	xvalue vCards = NULL;
	xvalue vCard = NULL;

	vPage = xvoCreateTable();
	if ( vPage == NULL ) {
		return NULL;
	}

	xvoTableSetBool(vPage, "ok", 0, TRUE);
	xvoTableSetText(vPage, "title", 0, "XRT Service Dashboard", 0, FALSE);
	xvoTableSetText(vPage, "service", 0, "http-json-template-chain", 0, FALSE);
	xvoTableSetText(vPage, "status_text", 0, "running", 0, FALSE);
	xvoTableSetText(vPage, "requested_path", 0, pReq->sPath, 0, FALSE);

	xvoTableSetArray(vPage, "cards", 0);
	vCards = xvoTableGetValue(vPage, "cards", 0);
	if ( vCards != NULL ) {
		vCard = xvoArrayAppendTable(vCards);
		xvoTableSetText(vCard, "title", 0, "workers", 0, FALSE);
		xvoTableSetText(vCard, "value_text", 0, "1", 0, FALSE);

		vCard = xvoArrayAppendTable(vCards);
		xvoTableSetText(vCard, "title", 0, "routes", 0, FALSE);
		xvoTableSetText(vCard, "value_text", 0, "2", 0, FALSE);

		vCard = xvoArrayAppendTable(vCards);
		xvoTableSetText(vCard, "title", 0, "template", 0, FALSE);
		xvoTableSetText(vCard, "value_text", 0, "precompiled", 0, FALSE);
	}

	return vPage;
}


static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	appstate* pState = (appstate*)pOwner;
	xvalue vPage = NULL;
	bool bOk = false;

	(void)pServer;
	(void)pConn;

	if ( pState == NULL ) {
		procWriteFallbackError(pResp);
		return true;
	}

	if ( (strcmp(pReq->sPath, "/api/dashboard") != 0) && (strcmp(pReq->sPath, "/dashboard") != 0) ) {
		return false;
	}

	vPage = procBuildDashboardModel(pReq);
	if ( vPage == NULL ) {
		procWriteFallbackError(pResp);
		return true;
	}

	if ( strcmp(pReq->sPath, "/api/dashboard") == 0 ) {
		bOk = procWriteJsonResponse(pResp, 200u, vPage);
	} else {
		bOk = procWriteHtmlResponse(pResp, pState->hDashboardTemplate, vPage);
	}

	xvoUnref(vPage);
	if ( !bOk ) {
		procWriteFallbackError(pResp);
	}
	return true;
}


int main(void)
{
	appstate tApp = {0};
	xnetengineconfig tEngineCfg;
	xhttpdconfig tHttpdCfg;
	xhttpdevents tEvents = {0};
	XTE_Error tTemplateError = {0};
	xnetengine* pEngine = NULL;
	xhttpdserver* pServer = NULL;

	xrtInit();

	tApp.hTemplateEngine = xteCreateEngine();
	if ( tApp.hTemplateEngine == NULL ) {
		fprintf(stderr, "create template engine failed\n");
		goto cleanup;
	}

	tApp.hDashboardTemplate = xteParseEx(
		tApp.hTemplateEngine,
		g_sDashboardTemplate,
		0u,
		NULL,
		&tTemplateError
	);
	if ( tApp.hDashboardTemplate == NULL ) {
		fprintf(
			stderr,
			"parse dashboard template failed: %s (%u:%u)\n",
			tTemplateError.sDesc ? tTemplateError.sDesc : "unknown",
			(unsigned)tTemplateError.iLine,
			(unsigned)tTemplateError.iColumn
		);
		goto cleanup;
	}

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( (pEngine == NULL) || (xrtNetEngineStart(pEngine) != XRT_NET_OK) ) {
		fprintf(stderr, "start network engine failed\n");
		goto cleanup;
	}

	xrtHttpdConfigInit(&tHttpdCfg);
	(void)xrtNetAddrParse(&tHttpdCfg.tBindAddr, "127.0.0.1", 8081u);

	tEvents.OnRequest = procOnRequest;

	pServer = xrtHttpdCreate(pEngine, &tHttpdCfg, &tEvents, &tApp);
	if ( (pServer == NULL) || (xrtHttpdStart(pServer) != XRT_NET_OK) ) {
		fprintf(stderr, "start http server failed\n");
		goto cleanup;
	}

	printf("listen on http://127.0.0.1:%u\n", (unsigned)xrtHttpdBoundPort(pServer));
	printf("GET /api/dashboard\n");
	printf("GET /dashboard\n");
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
	if ( tApp.hDashboardTemplate != NULL ) {
		xteDestroyTemplate(tApp.hDashboardTemplate);
	}
	if ( tApp.hTemplateEngine != NULL ) {
		xteDestroyEngine(tApp.hTemplateEngine);
	}

	xrtUnit();
	return 0;
}
```


## 4. 这份代码真正想教会你的事

### 4.1 同一份 `xvalue` 同时服务两个出口

`procBuildDashboardModel()` 是这页最核心的函数。  
它做完以后，服务已经拿到了一份完整的结构化对象树。

接下来：

- `/api/dashboard` 只是把这份对象树序列化成 JSON
- `/dashboard` 只是把同一份对象树送进模板渲染

也就是说，JSON 和 HTML 在出口处分叉，但在数据层没有分叉。


### 4.2 模板应该在启动时预编译，而不是每个请求里现解析

这就是为什么例子里把：

- `xteCreateEngine()`
- `xteParseEx()`

都放在 `main()` 的启动阶段。

原因很直接：

- 模板结构本身是稳定资产
- request 变化的是数据，不是模板源码
- 预编译后反复 `xteMake()` 才符合长期服务的热路径组织


### 4.3 `xhttpd` 只管服务边界，不负责替你发明页面模型

这页里 `xhttpd` 做的事情非常纯粹：

- 接请求
- 产出响应
- 维持 HTTP 生命周期

而真正的页面模型、JSON 模型、展示模型，全都收敛到 `xvalue` 和模板层。  
这才是模块职责清楚的状态。


### 4.4 这页故意不引入异步

和 [最小 HTTP 服务](minimal-http-service.md) 不同，这一页故意不讲 `OnRequestAsync`。  
不是因为它不重要，而是因为这页真正要讲的是：

- 数据主线不要裂开
- 模板不要在每个请求里重新解析
- 页面和 API 不要各养一套对象模型

把这些边界先讲稳，再去叠加 future / task / coroutine，结构才不会乱。


## 5. 为什么这比“API 一套，页面一套”更稳

如果 `/api/dashboard` 和 `/dashboard` 各自维护一套内部数据结构，马上就会遇到这些问题：

- 字段名不一致
- 同一块业务整理逻辑写两遍
- 一个改了另一个忘记改
- 页面上有的字段 API 没有，API 有的字段页面没用上

而统一到 `xvalue` 之后，变化路径会清楚很多：

- 加字段时，先改 `procBuildDashboardModel()`
- 然后决定 JSON 是否输出它
- 再决定模板是否展示它

这就把“数据准备”和“数据出口”明确分成了两个阶段。


## 6. 运行与验证

服务启动后，可以直接用下面两条请求验证：

```bash
curl http://127.0.0.1:8081/api/dashboard
curl http://127.0.0.1:8081/dashboard
```

你应该看到：

- 第一条返回 JSON
- 第二条返回 HTML
- 两边都带着同一组字段：`title`、`service`、`status_text`、`cards`

如果你改动 `procBuildDashboardModel()` 里的字段内容，两条输出会同时受影响。  
这正是这页想证明的点。


## 7. 这个案例故意没有做什么

为了把主线讲清楚，这里故意没加下面这些东西：

- 路由表
- 模板 include
- 数据库
- 用户会话
- 异步任务
- TLS

它们当然都可以继续接，但不是这页的重点。  
这页只负责把一个关键认知讲稳：

> HTTP 入口、JSON 输出和 HTML 输出，应该围绕同一份 `xvalue` 主线展开，而不是从一开始就裂成三块。


## 8. 常见错误

### 错误一：API 直接拼 JSON，页面直接拼 HTML

这样做短期很快，长期会越来越难统一字段和逻辑。

### 错误二：每个请求都重新 `xteParseEx()`

模板源码不该成为热路径上的重复成本。  
稳定模板应在启动时编译好，request 期间只喂数据。

### 错误三：模板上下文不是完整对象，而是一堆零散变量

如果模板层拿到的不是一棵完整的 `xvalue` 对象树，而是东一块西一块的数据，后面一旦页面增长，模型还是会碎掉。

### 错误四：把页面层和接口层看成两件完全独立的事情

它们的出口当然不同，但它们的数据准备层，很多时候本来就应该是同一份。


## 9. 建议继续阅读

- [Template API](../api/api-template.md)
- [XHTTPD API](../api/api-xhttpd.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [用 Template 渲染一个 HTML 页面](template-render-html.md)
- [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)

---

**一句话总结：** 这个案例真正要建立的习惯不是“会写模板”，而是“先用 `xvalue` 组织完整数据，再让 JSON 和 HTML 各走各的出口”。这才是 HTTP + JSON + Template 能长期稳定协作的关键。
