# 把本地控制台服务升级成一个跨机器恢复面板

> 这页要解决的不是“恢复配额满了之后再去别的机器拉一个文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 quota admission 和单机重链路恢复之后，又开始需要把“本机没有合适恢复来源时去远端 peer 拉取 snapshot”“远端 peer 的 URL、target、超时和健康状态要进入正式策略”“远端 HTTP fetch 完成后还要统一 publish 回本地快照”“当前 remote active、最近 peer 选择、最近 remote defer 原因要稳定交给页面、JSON、日志和快照”放进同一条后台主线时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是继续让每个 handler 自己拼 URL、自己挑 peer、自己决定超时和发布动作。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 重链路恢复怎样把 inspect / verify / hydrate / publish 收进一个 `task group` scope
- 恢复配额怎样决定 `run_now / queue_wait / defer`，以及 join 完成后怎样提升下一条 waiting 请求

但真实服务再往前走一步，很快又会出现一个新的问题：

- 本机 active slot 还空着
- 本地 warm、bundle、stage 都已经不够用了
- 真正可用的来源已经在另一台 peer 节点上
- 这次恢复不再只是本机文件系统或本机工具链
- 页面和 JSON 还要能回答“这次去了哪台 peer”“为什么没选另一台”“是 peer 太慢、peer 不健康，还是根本没有可用 remote source”

如果这时还把实现停在：

- `snprintf(sURL, ...);`
- `pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);`

很快就会出现几个典型问题：

- URL、peer 选择、超时、Host 头和远端健康策略散在各个 handler 里
- `waiting_remote`、`defer_remote` 和 `remote_done` 没有正式状态边界
- 远端 fetch 完成后没有统一 publish 回本地 snapshot 的收口点
- 页面为了说明“这次到底选了哪台 peer、为什么这样选”开始扫日志和局部变量

所以这页真正要补出的，不是“一个远端 HTTP 请求例子”，而是：

> 当恢复来源已经跨到其他机器时，怎样把 peer 选择、remote fetch、publish 和状态导出做成一条正式、可解释、可导出的跨机器恢复主线。


## 2. 为什么这次不能只靠“本机 quota + 单次 HTTP 请求”

### 2.1 quota 只能回答“能不能开始”，不能回答“去找谁”

上一页里的 quota admission 已经很好地解决了：

- 当前这条恢复链能不能拿到 slot
- slot 满了时是 waiting 还是 defer
- 哪条 join 完成后提升下一条 waiting

但它不回答：

- 应该向哪台 peer 发起远端恢复
- 这台 peer 的 URL、延迟、健康状态是不是可接受
- 这次为什么没走 remote A，而走了 remote B


### 2.2 单次 HTTP 请求也不等于完整远端恢复

到了这一层，系统真正要稳定回答的是：

- 远端 peer 是否健康
- 远端 URL 是否已经解析成正式 target
- 远端 fetch 是否完成
- 本地 publish 是否也已经完成
- 当前这条跨机器恢复链是否可以正式写成 done

这说明：

- `xhttp` 只是 remote fetch stage
- 它不能自动变成整条跨机器恢复链的最终完成点


### 2.3 这次真正新增的是“peer selection 层”

更稳的分工方式是：

- `quota`
	- 决定这条恢复链能不能开始
- `peer selection`
	- 决定如果要 remote fetch，该去找哪台 peer
- `task group`
	- 把 local inspect、remote fetch、publish 收进同一个 scope

一句话记住：

> 上一页补的是“多条本机重链路怎么排队”，这一页补的是“本机已经允许开始时，远端恢复应该去哪台机器拿、怎么正式收口”。


## 3. 这条跨机器恢复主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active remote registry | 当前有哪些跨机器恢复正在占用 slot |
| `list` | peer catalog | 哪些远端节点可选、健康状态如何、延迟和优先级是多少 |
| `list` | recent remote history | 页面和 JSON 展示“这次选了哪台 peer、远端请求结果怎样” |
| `list` | recent defer history | 页面和 JSON 展示“为什么这次没走 remote” |
| `list` | recent publish history | 页面和 JSON 展示“哪次 remote fetch 真正落回本地 snapshot” |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在 peer 选择与远端恢复上 |
| `task group` | 管住一条跨机器恢复链的 child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local inspect、remote fetch、publish 的完成态 | 统一结果和 join 边界 |
| `xurl` | 解析 peer base URL、解析 target、生成 `Host` 头 | 远端连接目标进入正式状态 |
| `xhttp` | 发起 remote fetch | 正式 HTTP/HTTPS 远端拉取入口 |
| `file_async` | 把远端结果发布到本地 snapshot | publish 进入正式 future 主线 |
| `time` | 记录 queued / remote started / published 时间 | 页面和策略使用正式时间边界 |
| `path + file` | 组织 stage / published 路径 | 把 remote 结果落回本地文件系统 |
| `hash` | 给 key 一个稳定指纹 | 生成 snapshot 路径和轻量标识 |
| `xvalue + json` | 导出 active / peer / defer / publish 摘要 | 页面、脚本、运维共享同一份状态模型 |
| `xhttpd + template` | 展示当前 peer 状态与 remote 恢复结果 | 浏览器和脚本共享一个入口 |

一句话记住：

> `quota` 决定能不能开始，`xurl + xhttp` 决定远端 fetch 到底去哪，`task group` 决定整条跨机器恢复链什么时候真正完成。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成跨机器恢复语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/cross-machine-recovery.json
runtime/stage/<hash>.json
runtime/published/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `max_active_remote`、`max_peer_latency_ms`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 active remote、peer catalog、recent remote history、recent defer history
- `runtime/console.log`
	- 记录 hydrate signal、peer 选择、remote fetch、publish 完成和 defer 原因
- `runtime/cross-machine-recovery.json`
	- 保存最近一次跨机器恢复摘要
- `runtime/stage/<hash>.json`
	- 保存 remote fetch 后的中间结果
- `runtime/published/<hash>.json`
	- 保存发布完成后的正式本地 snapshot


## 5. 先把“hydrate signal -> 选 peer -> remote fetch -> publish”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. `dict` 表示当前 active remote registry
2. `list` 表示 peer catalog
3. `list` 表示 remote / defer / publish history
4. `queue + thread` 仍然只承接 `HYDRATE / SWEEP`
5. `task group` 仍然只管理一条跨机器恢复链
6. local inspect 用线程 child 表示
7. remote fetch 用真实 `xurl + xhttp` 异步 future 表示
8. publish 用 `file_async future` 表示
9. peer 选择只决定 `run_remote / defer_remote`
10. 整条链路仍然只靠一个 join future 收口

这个骨架会展示这些事：

- `beta` 仍有 warm shadow，不进入 remote 恢复
- `theta` 选择 `peer-a`，执行一条 remote fetch + publish 链
- `gamma` 只有 `peer-b` 可选，但延迟超预算，这次直接 defer
- `omega` 当前没有健康 peer，这次直接 defer

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_HYDRATE 1
#define DEMO_SIGNAL_SWEEP 2
#define DEMO_MAX_CHILD 8

typedef struct
{
	xtime tTouchedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoWarmItem;

typedef struct
{
	char sName[32];
	char sBaseURL[256];
	bool bHealthy;
	uint32 iLatencyMs;
	uint32 iPriority;
} DemoPeerNode;

typedef struct
{
	xtime tAt;
	uint64 iKeyHash;
	char sKey[64];
	char sPeer[32];
	char sState[32];
	char sReason[64];
} DemoRemoteEvent;

typedef struct
{
	uint32 iMaxActiveRemote;
	uint32 iMaxPeerLatencyMs;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
} DemoRemotePolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pRemoteHistory;
	xlist pDeferred;
	xlist pPublished;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoRemotePolicy tPolicy;
	uint32 iActiveRemote;
	char sSnapshotPath[260];
} DemoRemoteCenter;

typedef struct
{
	char sStage[32];
	int iDelayMs;
	int iStatus;
} DemoStageTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPath(char* sDst, size_t iCap, const char* sDir, const char* sKey, const char* sExt)
{
	const char* sSafeKey = (sKey != NULL) ? sKey : "";

	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(
		sDst,
		iCap,
		"runtime/%s/%016llx%s",
		(sDir != NULL) ? sDir : "stage",
		(unsigned long long)xrtHash64((ptr)sSafeKey, strlen(sSafeKey)),
		(sExt != NULL) ? sExt : ".json");
}

static int32 procLocalStageTask(ptr pArg, xfuture_result* pOut)
{
	DemoStageTask* pTask = (DemoStageTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pTask->iDelayMs);
	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static bool procPrepareRemoteRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	uint32 iTimeoutMs
)
{
	xrturlview tBase;
	char sRef[160];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/recovery/%s", sKey);
	if ( !xrtUrlResolve(&tBase, sRef, sURL, iURLCap, NULL) ) {
		return false;
	}
	if ( !xrtUrlMakeHostHeader(&tBase, sHostHeader, iHostCap) ) {
		return false;
	}

	xrtHttpRequestInit(pReq);
	if ( !xrtHttpRequestSetMethod(pReq, "GET") ) {
		return false;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return false;
	}
	(void)xrtHttpRequestSetHeader(pReq, "Host", sHostHeader);
	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	(void)xrtHttpRequestSetHeader(pReq, "X-Recovery-Key", sKey);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static const DemoPeerNode* procChoosePeer(const DemoPeerNode* arrPeer, int iCount, uint32 iMaxPeerLatencyMs)
{
	const DemoPeerNode* pBest = NULL;
	int i;

	for ( i = 0; i < iCount; i++ ) {
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( arrPeer[i].iLatencyMs > iMaxPeerLatencyMs ) {
			continue;
		}
		if ( (pBest == NULL) || (arrPeer[i].iPriority < pBest->iPriority) ) {
			pBest = &arrPeer[i];
		}
	}

	return pBest;
}

static const char* procChooseRemotePlan(
	const DemoWarmItem* pWarm,
	const DemoPeerNode* pPeer,
	const DemoRemoteCenter* pCenter,
	xtime tNow
)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "warm_shadow";
	}
	if ( pCenter->iActiveRemote >= pCenter->tPolicy.iMaxActiveRemote ) {
		return "defer_remote_busy";
	}
	if ( pPeer == NULL ) {
		return "defer_no_healthy_peer";
	}
	if ( pPeer->iLatencyMs > pCenter->tPolicy.iMaxPeerLatencyMs ) {
		return "defer_peer_too_slow";
	}
	return "run_remote";
}

static bool procRunRemoteChain(DemoRemoteCenter* pCenter, const DemoPeerNode* pPeer, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xfuture* pRemoteFetch = NULL;
	xfuture* pPublish = NULL;
	xfuture* pJoin = NULL;
	xhttpresponse* pResp = NULL;
	xhttprequest tReq;
	DemoStageTask tInspect;
	char sURL[512];
	char sHostHeader[320];
	char sStagePath[260];
	char sPublishPath[260];
	const char* sPublishJson = "{\n\t\"state\": \"remote-published\"\n}\n";
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(&tReq, 0, sizeof(tReq));
	memset(&tInspect, 0, sizeof(tInspect));

	if ( (pCenter == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	xrtNetEngineConfigInit(&tEngineCfg);
	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		return false;
	}
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		goto cleanup;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		goto cleanup;
	}

	procCopyText(tInspect.sStage, sizeof(tInspect.sStage), "inspect_remote_plan");
	tInspect.iDelayMs = 40;
	tInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	if ( !procPrepareRemoteRequest(&tReq, sURL, sizeof(sURL), sHostHeader, sizeof(sHostHeader), pPeer, sKey, pCenter->tPolicy.iRemoteTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pRemoteFetch = xrtHttpExecuteAsync(pEngine, &tReq);
	if ( pRemoteFetch == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pGroup, pRemoteFetch) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	arrChild[iChildCount] = pRemoteFetch;
	iChildCount++;

	procBuildPath(sStagePath, sizeof(sStagePath), "stage", sKey, ".json");
	procBuildPath(sPublishPath, sizeof(sPublishPath), "published", sKey, ".json");

	pPublish = xrtFileWriteAllAsync(sPublishPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( pPublish == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pGroup, pPublish) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	arrChild[iChildCount] = pPublish;
	iChildCount++;

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		printf("%s -> remote_chain_timeout\n", sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pResp = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)pRemoteFetch);
	printf("%s -> remote_chain_joined via %s\n", sKey, pPeer->sName);
	printf("remote-url: %s\n", sURL);
	printf("host-header: %s\n", sHostHeader);
	if ( pResp != NULL ) {
		printf("remote-status: %u\n", (unsigned)pResp->iStatusCode);
	}
	printf("stage-path: %s\n", sStagePath);
	printf("publish-path: %s\n", sPublishPath);

	for ( i = 0; i < iChildCount; i++ ) {
		printf("child[%d] status = %d\n", i, (int)xFutureStatus(arrChild[i]));
	}

	bFinished = true;

cleanup:
	if ( pResp != NULL ) {
		xrtHttpResponseDestroy(pResp);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
	xrtHttpRequestUnit(&tReq);
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	return bFinished;
}

static void procRecordEvent(DemoRemoteEvent* pEvent, const char* sKey, const char* sPeer, const char* sState, const char* sReason, xtime tNow, uint64 iKeyHash)
{
	if ( pEvent == NULL ) {
		return;
	}

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->tAt = tNow;
	pEvent->iKeyHash = iKeyHash;
	procCopyText(pEvent->sKey, sizeof(pEvent->sKey), sKey);
	procCopyText(pEvent->sPeer, sizeof(pEvent->sPeer), sPeer);
	procCopyText(pEvent->sState, sizeof(pEvent->sState), sState);
	procCopyText(pEvent->sReason, sizeof(pEvent->sReason), sReason);
}

int main(void)
{
	DemoRemoteCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPeerNode arrPeer[2];
	DemoRemoteEvent tEvent;
	const DemoPeerNode* pPeer = NULL;
	const char* sPlan;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(&tEvent, 0, sizeof(tEvent));

	tCenter.tPolicy.iMaxActiveRemote = 1u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 200u;
	tCenter.tPolicy.iRemoteTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	procBuildPath(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), ".", "cross-machine-recovery", ".json");

	tBetaWarm.tTouchedAt = tNow - 180;
	tBetaWarm.iKeyHash = 4001u;
	tBetaWarm.iHeat = 21;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "https://peer-a.example.com/");
	arrPeer[0].bHealthy = true;
	arrPeer[0].iLatencyMs = 80u;
	arrPeer[0].iPriority = 1u;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "https://peer-b.example.com/");
	arrPeer[1].bHealthy = true;
	arrPeer[1].iLatencyMs = 260u;
	arrPeer[1].iPriority = 2u;

	sPlan = procChooseRemotePlan(&tBetaWarm, NULL, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	pPeer = procChoosePeer(arrPeer, 2, tCenter.tPolicy.iMaxPeerLatencyMs);
	sPlan = procChooseRemotePlan(NULL, pPeer, &tCenter, tNow);
	if ( strcmp(sPlan, "run_remote") == 0 ) {
		tCenter.iActiveRemote++;
		procRecordEvent(&tEvent, "theta", pPeer->sName, "remote_start", "peer_selected", tNow, 4002u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sPeer);

		if ( procRunRemoteChain(&tCenter, pPeer, "theta") ) {
			tCenter.iActiveRemote--;
			procRecordEvent(&tEvent, "theta", pPeer->sName, "remote_done", "publish_completed", tNow + 1, 4002u);
			printf("%s -> %s (%s)\n", tEvent.sKey, tEvent.sState, tEvent.sReason);
		}
	}

	pPeer = &arrPeer[1];
	sPlan = procChooseRemotePlan(NULL, pPeer, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		procRecordEvent(&tEvent, "gamma", pPeer->sName, "defer_remote", "peer_too_slow", tNow + 2, 4003u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	arrPeer[0].bHealthy = false;
	arrPeer[1].bHealthy = false;
	pPeer = procChoosePeer(arrPeer, 2, tCenter.tPolicy.iMaxPeerLatencyMs);
	sPlan = procChooseRemotePlan(NULL, pPeer, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		procRecordEvent(&tEvent, "omega", "(none)", "defer_remote", "no_healthy_peer", tNow + 3, 4004u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	printf("active_remote=%u snapshot=%s\n", (unsigned)tCenter.iActiveRemote, tCenter.sSnapshotPath);
	return 0;
}
```


## 6. 跨机器恢复里真正要稳定下来的边界

### 6.1 请求线程只提交恢复意图，不自己挑 peer

这页真正稳定下来的，不是“哪个 handler 拼了哪个 URL”，而是：

- 请求线程只提交 `HYDRATE`
- worker 才决定 remote 选哪台 peer、是否超预算、是否直接 defer


### 6.2 `peer selection` 是正式策略层，不是字符串拼接层

这页里真正容易写错的地方是：

- 把 peer 选择写成几行 `snprintf` + `if ( host == ... )`

但更稳的做法应该是：

- peer catalog 明确记录健康状态、延迟和优先级
- `xurl` 明确解析 base URL、target 和 `Host` 头
- 远端请求的 URL、target、超时都进入正式状态


### 6.3 remote fetch 是 child future，不是整条链路的最终完成点

这页里 `xhttp` 真正回答的是：

- 某次 remote fetch 有没有结束
- 远端响应有没有回到当前 runtime

它不直接回答：

- 整条跨机器恢复链是不是已经 publish 完成

那一层仍然应该交给 `task group + join future`。


### 6.4 publish 仍然是本地正式动作

即使恢复来源已经跨机器，真正要稳定下来的仍然是：

- 远端 fetch 完成之后，如何统一 publish 回本地 snapshot

如果没有这层，本机 dashboard 和后续请求就仍然拿不到正式恢复结果。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果 peer 状态需要多进程或多节点共享，不该继续只用单机内存状态
- 如果 remote fetch 需要双向流、增量同步或 lease 协商，应继续补 `xws` 或更正式的复制协议层
- 如果恢复结果还要跨集群仲裁，应该继续补分布式编排或持久化协调层


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 active remote、peer catalog、recent remote history、recent defer history
2. `POST /api/hydrate`
	- 只提交恢复意图，让 worker 决定本地恢复、remote fetch，还是 defer
3. `GET /api/peers`
	- 查询 peer 健康状态、延迟、最近一次 remote 选择结果
4. `POST /api/sweep`
	- 手工或定时触发超时 remote scope 清理
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 active remote、peer 状态、defer 原因和 publish 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个恢复配额面板](recovery-quota-dashboard.md)
	对比“单机 quota admission”和“跨机器 peer selection”到底差在哪
2. [把本地控制台服务升级成一个重链路恢复面板](heavy-recovery-chain-dashboard.md)
	回看单条恢复链 scope 在跨机器场景里仍然为什么重要
3. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
	把单机 peer 选择继续升级成“多 peer fan-out / quorum / publish”的正式协调模型
