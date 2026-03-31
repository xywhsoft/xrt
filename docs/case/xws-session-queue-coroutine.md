# 用 XWS + Queue + Coroutine 写一个双向会话服务骨架

> 这个案例聚焦“WebSocket 回调负责协议边界，Queue 负责消息交接，Coroutine 负责编排会话流程”这条主线，把 `xws + queue + coroutine` 串成一个当前公开 API 下可稳定落地的会话服务骨架。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“长期在线的 WebSocket 会话服务”，它有 6 个约束：

1. 连接建立、握手、ping/pong、close 都希望交给现成协议层
2. 收到一条消息后，不能在回调里直接做重活
3. 应用层希望把消息先交接出去，再按自己的节奏处理
4. 主流程希望按“等连接打开 -> 发消息 -> 等回包 -> 再决定下一步”的顺序来写
5. Windows / Linux 上都要沿同一套运行时语义工作
6. 文档必须严格站在当前 `xrt.h` 已公开的接口上

如果没有一条清晰主线，代码很容易变成：

- `OnText` 回调里直接做数据库、模板、磁盘、HTTP 调用
- 回调越写越长，后面调试时分不清是协议问题还是业务问题
- 应用层为了等一个会话事件，到处自己维护 flag、轮询和共享字符串
- 想引入协程时，又把 `xws` 错当成已经 future 化、wait-source 化的模块

这个案例要解决的，正是“在当前正式 public surface 下，WebSocket 会话服务应该怎样稳当地接上 queue 和 coroutine”。


## 2. 为什么这次不用别的方案

### 2.1 为什么不是“所有逻辑都塞进回调”

因为 `xws` 当前主线的公开合同是：

- 连接与协议管理走事件回调
- 上层消息组织需要你自己决定边界

回调真正适合承担的是：

- 复制输入
- 做最小校验
- 发快速 ACK / busy / closing 这类即时反馈

它不适合承担：

- 慢业务
- 阻塞等待
- 一整条会话编排逻辑


### 2.2 为什么这里要同时用 `queue` 和 `xcoevent`

因为这两个对象解决的不是一回事。

| 对象 | 这次承担的角色 | 不能替代什么 |
|------|----------------|--------------|
| `xmpscqwait` | 保存真正的消息对象 | 它本身不会唤醒协程 |
| `xcoevent` | 跨线程唤醒协程调度器 | 它本身不保存消息载荷 |

可以先记一句话：

> `queue` 负责“消息别丢”，`xcoevent` 负责“协程该醒了”。


### 2.3 为什么这次不把 `xws` 直接讲成 `future / wait-source`

因为当前 `xws` 正式 API 还不是那条主线。

`api-xws.md` 现在已经明确写了：

- `xws` 当前主路径仍是事件回调
- 更深的 wait-source / future 化扩展是自然演进方向，但不是当前正式主路径

所以这页故意不发明不存在的接口，而是沿着当前最稳的桥来写：

> `xws callback -> queue -> coroutine`


### 2.4 为什么示例里把服务端发送限制在回调里

因为当前公开文档并没有正式给出“`xwsconn*` 可以跨线程自由发送”的合同。

所以这页示例故意遵守一个保守且稳定的规则：

- 服务端快速回包留在 `OnText` 回调里
- 复杂业务通过 `queue` 交给协程侧继续处理

如果后续 `xws` 明确公开更强的线程归属 / post-to-engine 合同，再把“延迟推送”升级成下一层模式会更稳。


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `xwsserver` / `xwsclient` | 握手、收发、close、ping/pong | 正式 WebSocket 协议层 |
| `OnText` / `OnOpen` / `OnClose` | 最小协议边界和快速反馈 | 把 wire 事件接进应用层 |
| `xmpscqwait` | 跨线程消息交接 | 消息对象所有权明确、队列满/关闭可见 |
| `xcoevent` | 从回调线程唤醒协程 | 不阻塞线程、不轮询 |
| `xcosched` | 单线程编排主流程 | 顺序写会话脚本和审计逻辑 |

可以先记一句话：

> `xws` 负责连线，`queue` 负责交接，`coroutine` 负责把会话流程写成顺序代码。


## 4. 代码目标

下面这段完整代码会做 7 件事：

1. 创建本地 `xws` server 和 client
2. server 收到文本消息后，不做重活，只把消息复制进审计队列
3. server 在回调里立刻回一个 `queued:*` ACK
4. client 的回包回调把文本复制进 inbox 队列
5. 主线程上的协程先等 client open，再顺序发送两条消息
6. 另一个协程负责消费 server 侧审计队列，模拟“应用层会话处理”
7. 最后由协程主动关闭 client，并等待 close 回调收口

这段代码不是“最短 hello world”，而是一个更接近真实程序边界的会话骨架。


## 5. 完整代码

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_AUDIT_EXPECTED	2

typedef struct
{
	char sText[256];
} DemoQueuedText;

typedef struct
{
	xmpscqwait hAuditQueue;
	xcoevent hAuditEvent;
	int32 iAuditExpected;
	int32 iAuditHandled;
} DemoWsServerCtx;

typedef struct
{
	xmpscqwait hInboxQueue;
	xcoevent hOpenEvent;
	xcoevent hInboxEvent;
	xcoevent hCloseEvent;
	xwsclient* pClient;
} DemoWsClientCtx;


static xqueue_result procQueueTextPush(
	xmpscqwait hQueue,
	xcoevent hEvent,
	const char* pData,
	size_t iLen
)
{
	DemoQueuedText* pMsg;
	size_t iCopy;
	xqueue_result iRet;

	if ( (hQueue == NULL) || (pData == NULL) ) {
		return XQUEUE_ERROR;
	}

	pMsg = (DemoQueuedText*)xrtMalloc(sizeof(DemoQueuedText));
	if ( pMsg == NULL ) {
		return XQUEUE_ERROR;
	}

	memset(pMsg, 0, sizeof(DemoQueuedText));
	iCopy = (iLen < (sizeof(pMsg->sText) - 1u)) ? iLen : (sizeof(pMsg->sText) - 1u);
	if ( iCopy > 0u ) {
		memcpy(pMsg->sText, pData, iCopy);
	}
	pMsg->sText[iCopy] = '\0';

	iRet = xrtMPSCQWaitTryPush(hQueue, pMsg);
	if ( iRet != XQUEUE_OK ) {
		xrtFree(pMsg);
		return iRet;
	}

	if ( hEvent != NULL ) {
		xrtCoEventSet(hEvent);
	}
	return XQUEUE_OK;
}

static DemoQueuedText* procQueueTextTryPop(xmpscqwait hQueue)
{
	ptr pItem = NULL;

	if ( hQueue == NULL ) {
		return NULL;
	}
	if ( xrtMPSCQWaitTryPop(hQueue, &pItem) != XQUEUE_OK ) {
		return NULL;
	}
	return (DemoQueuedText*)pItem;
}

static void procQueueTextFree(DemoQueuedText* pMsg)
{
	if ( pMsg != NULL ) {
		xrtFree(pMsg);
	}
}

static void procDrainResidualQueue(xmpscqwait hQueue)
{
	DemoQueuedText* pMsg;

	for ( ;; ) {
		pMsg = procQueueTextTryPop(hQueue);
		if ( pMsg == NULL ) {
			return;
		}
		procQueueTextFree(pMsg);
	}
}


static void procServerOnOpen(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	printf("server: session opened\n");
}

static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	DemoWsServerCtx* pCtx = (DemoWsServerCtx*)pOwner;
	xqueue_result iPushRet;
	char sAck[320];
	size_t iAckLen;

	(void)pServer;

	if ( (pCtx == NULL) || (pConn == NULL) || (pData == NULL) ) {
		return;
	}

	iPushRet = procQueueTextPush(pCtx->hAuditQueue, pCtx->hAuditEvent, pData, iLen);

	if ( iPushRet == XQUEUE_OK ) {
		snprintf(sAck, sizeof(sAck), "queued:%.*s", (int)iLen, pData);
	} else if ( iPushRet == XQUEUE_FULL ) {
		snprintf(sAck, sizeof(sAck), "busy");
	} else {
		snprintf(sAck, sizeof(sAck), "closing");
	}

	iAckLen = strlen(sAck);
	(void)xrtWsConnSendText(pConn, sAck, iAckLen);
}

static void procServerOnClose(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	printf("server: session closed, reason=%d\n", (int)iReason);
}

static void procServerOnError(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	printf("server: socket error=%d\n", iSysErr);
}


static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;

	(void)pClient;

	if ( (pCtx != NULL) && (pCtx->hOpenEvent != NULL) ) {
		xrtCoEventSet(pCtx->hOpenEvent);
	}
}

static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;
	xqueue_result iPushRet;

	(void)pClient;

	if ( (pCtx == NULL) || (pData == NULL) ) {
		return;
	}

	iPushRet = procQueueTextPush(pCtx->hInboxQueue, pCtx->hInboxEvent, pData, iLen);
	if ( iPushRet != XQUEUE_OK ) {
		printf("client: inbox push failed=%d\n", (int)iPushRet);
	}
}

static void procClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;

	(void)pClient;
	printf("client: close callback, reason=%d\n", (int)iReason);

	if ( (pCtx != NULL) && (pCtx->hCloseEvent != NULL) ) {
		xrtCoEventSet(pCtx->hCloseEvent);
	}
}

static void procClientOnError(ptr pOwner, xwsclient* pClient, int iSysErr)
{
	(void)pOwner;
	(void)pClient;
	printf("client: socket error=%d\n", iSysErr);
}


static bool procWaitAndPrintInbox(DemoWsClientCtx* pCtx, const char* sStep)
{
	DemoQueuedText* pMsg;
	int32 iPrinted = 0;

	if ( (pCtx == NULL) || (sStep == NULL) ) {
		return false;
	}

	if ( !xrtCoWaitEventTimeout(pCtx->hInboxEvent, 3000u) ) {
		printf("%s: wait inbox timeout\n", sStep);
		return false;
	}

	for ( ;; ) {
		pMsg = procQueueTextTryPop(pCtx->hInboxQueue);
		if ( pMsg == NULL ) {
			break;
		}

		printf("%s: %s\n", sStep, pMsg->sText);
		procQueueTextFree(pMsg);
		iPrinted++;
	}

	if ( iPrinted == 0 ) {
		printf("%s: event arrived but queue empty\n", sStep);
	}

	return iPrinted > 0;
}

static void procServerAuditCo(ptr pArg)
{
	DemoWsServerCtx* pCtx = (DemoWsServerCtx*)pArg;
	int32 iIdleWait = 0;
	DemoQueuedText* pMsg;

	if ( pCtx == NULL ) {
		return;
	}

	while ( pCtx->iAuditHandled < pCtx->iAuditExpected ) {
		if ( !xrtCoWaitEventTimeout(pCtx->hAuditEvent, 3000u) ) {
			iIdleWait++;
			if ( iIdleWait >= 3 ) {
				break;
			}
			continue;
		}

		iIdleWait = 0;

		for ( ;; ) {
			pMsg = procQueueTextTryPop(pCtx->hAuditQueue);
			if ( pMsg == NULL ) {
				break;
			}

			pCtx->iAuditHandled++;
			printf("audit[%d]: %s\n", (int)pCtx->iAuditHandled, pMsg->sText);
			procQueueTextFree(pMsg);
		}
	}

	if ( pCtx->iAuditHandled < pCtx->iAuditExpected ) {
		printf(
			"audit incomplete: handled=%d expected=%d\n",
			(int)pCtx->iAuditHandled,
			(int)pCtx->iAuditExpected
		);
	}
}

static void procClientScriptCo(ptr pArg)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pArg;
	const char* sLogin = "login:user-1";
	const char* sChat = "say:hello-from-client";

	if ( (pCtx == NULL) || (pCtx->pClient == NULL) ) {
		return;
	}

	if ( !xrtCoWaitEventTimeout(pCtx->hOpenEvent, 3000u) ) {
		printf("client script: wait open timeout\n");
		return;
	}

	printf("client script: connection opened\n");

	if ( xrtWsClientSendText(pCtx->pClient, sLogin, strlen(sLogin)) != XRT_NET_OK ) {
		printf("client script: send login failed\n");
		return;
	}
	(void)procWaitAndPrintInbox(pCtx, "client inbox after login");

	if ( xrtWsClientSendText(pCtx->pClient, sChat, strlen(sChat)) != XRT_NET_OK ) {
		printf("client script: send chat failed\n");
		return;
	}
	(void)procWaitAndPrintInbox(pCtx, "client inbox after chat");

	if ( xrtWsClientClose(pCtx->pClient, XWS_CLOSE_NORMAL, "demo-done") != XRT_NET_OK ) {
		printf("client script: close request failed\n");
		return;
	}

	if ( !xrtCoWaitEventTimeout(pCtx->hCloseEvent, 3000u) ) {
		printf("client script: wait close timeout\n");
	}
}


int main(void)
{
	xqueue_config tQueueCfg;
	xnetengineconfig tEngineCfg;
	xwsserverconfig tServerCfg;
	xwsclientconfig tClientCfg;
	xwsserverevents tServerEvents;
	xwsclientevents tClientEvents;
	DemoWsServerCtx tServerCtx;
	DemoWsClientCtx tClientCtx;
	xcosched* pSched = NULL;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xwsserver* pServer = NULL;
	xwsclient* pClient = NULL;
	char sURL[256];
	int iExitCode = 1;

	xrtInit();

	memset(&tQueueCfg, 0, sizeof(tQueueCfg));
	memset(&tEngineCfg, 0, sizeof(tEngineCfg));
	memset(&tServerCfg, 0, sizeof(tServerCfg));
	memset(&tClientCfg, 0, sizeof(tClientCfg));
	memset(&tServerEvents, 0, sizeof(tServerEvents));
	memset(&tClientEvents, 0, sizeof(tClientEvents));
	memset(&tServerCtx, 0, sizeof(tServerCtx));
	memset(&tClientCtx, 0, sizeof(tClientCtx));

	tQueueCfg.iCapacity = 32u;
	tServerCtx.hAuditQueue = xrtMPSCQWaitCreate(&tQueueCfg);
	tClientCtx.hInboxQueue = xrtMPSCQWaitCreate(&tQueueCfg);
	tServerCtx.hAuditEvent = xrtCoEventCreate(false, false);
	tClientCtx.hOpenEvent = xrtCoEventCreate(false, false);
	tClientCtx.hInboxEvent = xrtCoEventCreate(false, false);
	tClientCtx.hCloseEvent = xrtCoEventCreate(false, false);
	tServerCtx.iAuditExpected = DEMO_AUDIT_EXPECTED;

	if ( (tServerCtx.hAuditQueue == NULL) || (tClientCtx.hInboxQueue == NULL) ||
		(tServerCtx.hAuditEvent == NULL) || (tClientCtx.hOpenEvent == NULL) ||
		(tClientCtx.hInboxEvent == NULL) || (tClientCtx.hCloseEvent == NULL) ) {
		goto cleanup;
	}

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		goto cleanup;
	}

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( (pServerEngine == NULL) || (pClientEngine == NULL) ) {
		goto cleanup;
	}
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK ) {
		goto cleanup;
	}
	if ( xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) {
		goto cleanup;
	}

	xrtWsServerConfigInit(&tServerCfg);
	(void)xrtNetAddrParse(&tServerCfg.tBindAddr, "127.0.0.1", 0);

	tServerEvents.OnOpen = procServerOnOpen;
	tServerEvents.OnText = procServerOnText;
	tServerEvents.OnClose = procServerOnClose;
	tServerEvents.OnError = procServerOnError;

	pServer = xrtWsServerCreate(pServerEngine, &tServerCfg, &tServerEvents, &tServerCtx);
	if ( pServer == NULL ) {
		goto cleanup;
	}
	if ( xrtWsServerStart(pServer) != XRT_NET_OK ) {
		goto cleanup;
	}

	xrtWsClientConfigInit(&tClientCfg);
	snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/session", (unsigned)xrtWsServerBoundPort(pServer));
	snprintf(tClientCfg.sURL, sizeof(tClientCfg.sURL), "%s", sURL);

	tClientEvents.OnOpen = procClientOnOpen;
	tClientEvents.OnText = procClientOnText;
	tClientEvents.OnClose = procClientOnClose;
	tClientEvents.OnError = procClientOnError;

	pClient = xrtWsClientCreate(pClientEngine, &tClientCfg, &tClientEvents, &tClientCtx);
	if ( pClient == NULL ) {
		goto cleanup;
	}
	tClientCtx.pClient = pClient;

	if ( xrtCoSchedSpawn(pSched, procServerAuditCo, &tServerCtx, 0) == NULL ) {
		goto cleanup;
	}
	if ( xrtCoSchedSpawn(pSched, procClientScriptCo, &tClientCtx, 0) == NULL ) {
		goto cleanup;
	}

	if ( xrtWsClientStart(pClient) != XRT_NET_OK ) {
		goto cleanup;
	}

	xrtCoSchedRun(pSched);
	iExitCode = 0;

cleanup:
	if ( pClient != NULL ) {
		xrtWsClientDestroy(pClient);
		pClient = NULL;
	}
	if ( pServer != NULL ) {
		xrtWsServerDestroy(pServer);
		pServer = NULL;
	}
	if ( pClientEngine != NULL ) {
		xrtNetEngineStop(pClientEngine);
		xrtNetEngineDestroy(pClientEngine);
		pClientEngine = NULL;
	}
	if ( pServerEngine != NULL ) {
		xrtNetEngineStop(pServerEngine);
		xrtNetEngineDestroy(pServerEngine);
		pServerEngine = NULL;
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
		pSched = NULL;
	}

	if ( tClientCtx.hCloseEvent != NULL ) {
		xrtCoEventDestroy(tClientCtx.hCloseEvent);
		tClientCtx.hCloseEvent = NULL;
	}
	if ( tClientCtx.hInboxEvent != NULL ) {
		xrtCoEventDestroy(tClientCtx.hInboxEvent);
		tClientCtx.hInboxEvent = NULL;
	}
	if ( tClientCtx.hOpenEvent != NULL ) {
		xrtCoEventDestroy(tClientCtx.hOpenEvent);
		tClientCtx.hOpenEvent = NULL;
	}
	if ( tServerCtx.hAuditEvent != NULL ) {
		xrtCoEventDestroy(tServerCtx.hAuditEvent);
		tServerCtx.hAuditEvent = NULL;
	}

	if ( tClientCtx.hInboxQueue != NULL ) {
		xrtMPSCQWaitClose(tClientCtx.hInboxQueue);
		procDrainResidualQueue(tClientCtx.hInboxQueue);
		xrtMPSCQWaitDestroy(tClientCtx.hInboxQueue);
		tClientCtx.hInboxQueue = NULL;
	}
	if ( tServerCtx.hAuditQueue != NULL ) {
		xrtMPSCQWaitClose(tServerCtx.hAuditQueue);
		procDrainResidualQueue(tServerCtx.hAuditQueue);
		xrtMPSCQWaitDestroy(tServerCtx.hAuditQueue);
		tServerCtx.hAuditQueue = NULL;
	}

	xrtUnit();
	return iExitCode;
}
```


## 6. 这段代码怎么读

### 6.1 先看 server 的 `OnText`

这段回调故意只做 3 件事：

1. 把文本复制进堆对象
2. 推进审计队列
3. 立刻回一个 `queued:*` / `busy` / `closing`

这里真正想讲的是：

- 回调负责协议边界
- 队列负责把业务消息交出去
- 重逻辑不直接堵在 WebSocket 回调里


### 6.2 再看 `queue + event`

这页最重要的边界不是“有没有协程”，而是下面这组配合：

- `xmpscqwait` 保存消息对象
- `xcoevent` 让协程在消息到来时被唤醒

如果你只有 `queue` 没有 `event`，协程侧就容易退回忙轮询。
如果你只有 `event` 没有 `queue`，唤醒信号到了也没有正式载荷可读。


### 6.3 `procServerAuditCo()` 才是应用层处理点

这个协程并不参与握手，也不直接收 socket 数据。

它做的是：

- 等 `hAuditEvent`
- drain `hAuditQueue`
- 按顺序消费每条会话消息

真实项目里，你通常会在这个位置继续做：

- 路由
- 权限校验
- 模板渲染
- `json / value / xson` 解析
- 子任务分发


### 6.4 `procClientScriptCo()` 展示的是“顺序写会话脚本”

这里故意把 client 侧流程写成：

1. 先等 `OnOpen`
2. 再发 `login`
3. 再等 inbox
4. 再发 `chat`
5. 最后主动 close

这就是协程在这条链里的真正价值：

- 不是替代 WebSocket 协议层
- 而是把“等事件再继续”的会话流程写成顺序代码


### 6.5 为什么这页刻意同时起本地 server 和 client

因为这样最容易把边界讲清楚：

- server 代表真实服务端协议入口
- client 代表一个最小可运行的会话脚本
- 两边都只用公开 API

也就是说，这不是“测试文件翻译版”，而是一个能把服务边界、消息交接和协程编排同时讲明白的教学骨架。


## 7. 如果你要升级成“慢业务处理”

这页的主线先停在：

- `callback -> queue -> coroutine`

如果你的审计协程后面要接慢业务，例如：

- 写磁盘
- 访问数据库
- 调 HTTP API

最稳的升级方式不是把这些慢操作塞回回调，而是让协程继续往下挂 `future`：

```c
static int32 procSlowWork(ptr pArg, xfuture_result* pOut)
{
	DemoQueuedText* pMsg = (DemoQueuedText*)pArg;
	DemoQueuedText* pCopy;

	pCopy = (DemoQueuedText*)xrtMalloc(sizeof(DemoQueuedText));
	if ( pCopy == NULL ) {
		return XRT_NET_ERROR;
	}

	memcpy(pCopy, pMsg, sizeof(DemoQueuedText));
	xrtSleep(50u);
	pOut->pValue = pCopy;
	return XRT_NET_OK;
}

static void procAuditCo(ptr pArg)
{
	DemoQueuedText* pMsg = (DemoQueuedText*)pArg;
	xfuture* pFuture;
	DemoQueuedText* pDone;

	pFuture = xTaskRunThread(procSlowWork, pMsg, 0);
	if ( pFuture == NULL ) {
		return;
	}

	pDone = (DemoQueuedText*)xFutureWaitCoValueTimeout(pFuture, 3000);
	if ( pDone != NULL ) {
		printf("slow done: %s\n", pDone->sText);
		xrtFree(pDone);
	}

	xFutureRelease(pFuture);
}
```

这里的意思是：

- `xws` 仍然负责协议事件
- `queue` 仍然负责交接消息
- 协程继续负责顺序编排
- 真正慢的执行单元，再交给 `future/task`

这才是和前面多任务主线能接起来的升级方式。


## 8. 如果你要升级成 TLS / 代理 / 真实会话服务

这页只是最小骨架，继续升级时通常会走下面 3 步：

### 8.1 升级成 `wss://`

服务端：

- `tServerCfg.pTlsConfig = &tTlsCfg`

客户端：

- `snprintf(tClientCfg.sURL, ..., "wss://...")`
- 本地自签名调试时，可临时 `tClientCfg.bVerifyPeer = false`


### 8.2 客户端接代理

如果 client 侧需要穿过代理再建连，直接使用：

- `xrtNetProxyConfigInit()`
- `xrtNetProxyCreate()`
- `tClientCfg.pProxy = pProxy`

这和 HTTP client 那条主线是一致的。


### 8.3 做成真正的多人会话服务

这时你通常还会补：

- 连接注册表
- 房间 / topic 路由
- 用户态 session 状态
- 心跳与超时踢线
- `json/xson` 协议对象

但这些都应该建立在今天这条边界上，而不是把回调重新写成“大一统业务函数”。


## 9. Windows / Linux 跨平台注意点

### 9.1 不要把回调线程和协程调度线程混成同一个概念

这页示例里：

- `OnOpen / OnText / OnClose` 在 `xws` 事件线程里推进
- 协程在主线程自己的 scheduler 上运行

两边通过：

- `xcoevent`
- `queue`

做跨线程交接。


### 9.2 `queue` 里的对象必须是正式所有权

示例里 push 进队列的是：

- `DemoQueuedText*`

也就是说，回调不会把原始 `pData` 指针直接扔给后面的协程。

这是因为回调参数的生命周期只保证在回调当下可用，后面继续持有就不稳了。


### 9.3 不要在协程侧退回成轮询 flag

既然已经有：

- `xrtCoWaitEventTimeout()`

就不要再写：

```c
while ( !bOpen ) {
	xrtCoYield();
}
```

这种写法短期能跑，但会把等待语义重新打散。


## 10. 常见错误

### 10.1 错误一：在 `OnText` 里直接做慢操作

这样最容易把协议层和业务层缠在一起。


### 10.2 错误二：只有事件，没有正式消息队列

被唤醒以后没有载荷，最后又会回到共享字符串和全局变量。


### 10.3 错误三：把回调参数里的 `pData` 直接跨线程保存

更稳的做法是像示例一样先复制进自己的消息对象。


### 10.4 错误四：把 `xws` 误写成已经 wait-source 化的模块

当前正式 public 主线不是这样。

这页特意用 `queue + xcoevent`，就是为了站在当前真实合同上写教学。


### 10.5 错误五：`Close` 之后立刻销毁还没 drain 的消息队列

协议关闭和应用层消息排空，不是同一件事。


## 11. 建议继续阅读

- [XWS API](../api/api-xws.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](../guide/queue-intro.md)
- [协程、Future 与 Task 入门](../guide/coroutine-future-task-intro.md)
- [xnet-v2 与 TLS Session 入门](../guide/xnet-v2-tls-intro.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](xhttp-client-proxy-tls.md)
- [用 XRT 调用一个流式 LLM API](streaming-llm-api.md)

---

**一句话总结：** 这条案例主线的关键不在“把 WebSocket 连上了”，而在“让 `xws` 只负责协议边界，让 `queue` 正式接走消息，让 `coroutine` 把会话流程写成顺序代码”；这才是当前 XRT 公开 API 下最稳的一条会话服务写法。
