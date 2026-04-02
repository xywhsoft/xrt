# 把本地控制台服务升级成一个分布式编排面板

> 这页要解决的不是“跨机器恢复再多挑几台 peer”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了跨机器恢复之后，又开始需要把“同一次恢复请求要同时下发到多个 peer 节点”“某些 peer 成功、某些 peer 超时、某些 peer 返回旧版本时怎样统一汇总”“什么时候算达到 quorum，什么时候必须整体取消”“本地 publish 只能发生在正式汇总完成之后”放进同一条后台主线，还要把当前 active orchestration、最近 fan-out 历史、最近 quorum 结果和最近 defer 原因稳定交给页面、JSON、日志和快照时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file_async + time + path + file + hash + xhttpd + template` 串成一条正式主线，而不是继续让每个 handler 自己拼几条 peer 请求、自己数成功数、自己临时决定什么时候 publish。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 恢复配额怎样决定 `run_now / queue_wait / defer`
- 跨机器恢复怎样做 peer 选择、remote fetch 和 publish 回本地

但真实服务再往前走一步，很快又会出现一个新的问题：

- 这次恢复不再是“选一台 peer 去拿一份结果”
- 而是要同时向多台 peer 发起恢复子请求
- 某些 peer 可能成功
- 某些 peer 可能超时
- 某些 peer 可能仍在返回旧版本
- 页面和 JSON 还要能回答“这次发给了哪几台 peer”“现在是不是已经达到 quorum”“为什么这次虽然有一个 peer 成功了，但整体还没有 publish”

如果这时还把实现停在：

- `for ( peer in peers ) send_http();`
- `if ( success_count >= 2 ) publish();`

很快就会出现几个典型问题：

- fan-out 请求、quorum 判断、失败收口和 publish 没有正式边界
- 请求线程开始背上多 peer 协调和成功计数
- 某个 peer 超时之后，没有统一地方决定是否继续等、立即取消还是正式 defer
- 页面为了说明“这次分布式恢复为什么成功/失败”开始扫日志和局部变量

所以这页真正要补出的，不是“多个 HTTP 请求的例子”，而是：

> 当一次恢复请求要同时协调多个远端 peer 时，怎样把 fan-out、quorum、join 和 publish 做成一条正式、可解释、可导出的分布式编排主线。


## 2. 为什么这次不能只靠“跨机器恢复 + 多发几次请求”

### 2.1 跨机器恢复只回答“找一台 peer 去拿”

上一页里的跨机器恢复已经很好地解决了：

- 当前这次 remote fetch 要不要开始
- 如果开始，应该选哪台 peer
- fetch 完成后怎样 publish 回本地

但它不回答：

- 这次是不是应该同时联系多台 peer
- 多台 peer 的结果怎样统一汇总
- quorum 到底是什么时候达成
- 哪些 peer 要继续等，哪些 peer 要被统一取消


### 2.2 多个 HTTP future 也不等于正式分布式编排

到了这一层，系统真正要稳定回答的是：

- 这次 coordinator 一共 fan-out 给了哪些 peer
- 当前成功数、失败数、超时数分别是多少
- 有没有达到 quorum
- 如果已经达到 quorum，哪些未完成 peer 要被视为可放弃
- 本地 publish 是否已经进入正式完成态

这说明：

- `xhttp` future 仍然只是一个 peer 子任务
- 它不能自动变成整次分布式恢复的最终完成点


### 2.3 这次真正新增的是“orchestration 汇总层”

更稳的分工方式是：

- `peer selection`
	- 决定这次 fan-out 给哪些 peer
- `task group`
	- 管住 local inspect、多个 remote fetch 和 publish child
- `orchestration summary`
	- 决定 quorum、整体状态和最终 publish 时机

一句话记住：

> 上一页补的是“找一台 peer”，这一页补的是“协调很多台 peer，并且把 fan-out/fan-in 的结果收成一个正式恢复结论”。


## 3. 这条分布式编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active orchestration registry | 当前有哪些分布式恢复正在运行 |
| `list` | peer catalog | 哪些节点可参与 fan-out，健康状态和优先级如何 |
| `list` | recent orchestration history | 页面和 JSON 展示“发给了谁、谁成功了、谁超时了” |
| `list` | recent defer history | 页面和 JSON 展示“为什么这次没有进入 fan-out” |
| `list` | recent publish history | 页面和 JSON 展示“哪次 quorum 达成后真正 publish 了” |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在分布式协调上 |
| `task group` | 管住一次 orchestrated recovery 的所有 child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local inspect、每个 peer fetch、publish 的完成态 | 统一结果和 join 边界 |
| `xurl` | 解析 peer base URL、target 和 `Host` 头 | 让远端节点目标进入正式状态 |
| `xhttp` | 发起每个 peer 的 remote fetch | 正式控制平面请求入口 |
| `file_async` | 把 quorum 结果 publish 到本地 snapshot | publish 进入正式 future 主线 |
| `time` | 记录 fan-out / quorum / publish 时间 | 页面和策略使用正式时间边界 |
| `path + file` | 组织 stage / published 路径 | 把结果落回本地文件系统 |
| `hash` | 给恢复 key 一个稳定指纹 | 生成快照路径和轻量标识 |
| `xvalue + json` | 导出 active / peer / quorum / defer 摘要 | 页面、脚本、运维共享同一份状态模型 |
| `xhttpd + template` | 展示当前分布式恢复状态 | 浏览器和脚本共享一个入口 |

一句话记住：

> `xhttp` 管每个 peer 子请求，`task group` 管这批子请求属于同一个恢复 scope，`orchestration summary` 管什么时候达成 quorum、什么时候允许 publish。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成分布式编排语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/distributed-orchestration.json
runtime/stage/<hash>.json
runtime/published/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `max_active_orchestrations`、`fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 active orchestration、peer catalog、recent orchestration history、recent defer history
- `runtime/console.log`
	- 记录 hydrate signal、fan-out、peer 完成、quorum 达成、publish 完成和 defer 原因
- `runtime/distributed-orchestration.json`
	- 保存最近一次分布式编排摘要
- `runtime/stage/<hash>.json`
	- 保存 fan-in 前的本地中间结果
- `runtime/published/<hash>.json`
	- 保存 quorum 达成后的正式本地 snapshot


## 5. 先把“hydrate signal -> fan-out 到多个 peer -> quorum -> publish”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. `dict` 表示当前 active orchestration registry
2. `list` 表示 peer catalog
3. `list` 表示 orchestration / defer / publish history
4. `queue + thread` 仍然只承接 `HYDRATE / SWEEP`
5. `task group` 仍然只管理一次 orchestrated recovery
6. local inspect 用线程 child 表示
7. 多个 peer fetch 用真实 `xurl + xhttp` 异步 future 表示
8. publish 用 `file_async future` 表示
9. quorum 只决定 `orchestrate_now / defer_orchestration`
10. 整条链路仍然只靠一个 join future 收口

这个骨架会展示这些事：

- `beta` 仍有 warm shadow，不进入分布式编排
- `theta` 会同时 fan-out 给 `peer-a` 和 `peer-b`，quorum 设为 2
- `gamma` 只有一个健康 peer，不满足 quorum，这次直接 defer
- `omega` 当前 active orchestration 已满，这次直接 defer

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_HYDRATE 1
#define DEMO_SIGNAL_SWEEP 2
#define DEMO_MAX_CHILD 8
#define DEMO_MAX_PEER 3

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
} DemoOrchEvent;

typedef struct
{
	uint32 iMaxActiveOrchestrations;
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
} DemoOrchPolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pOrchHistory;
	xlist pDeferred;
	xlist pPublished;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoOrchPolicy tPolicy;
	uint32 iActiveOrchestrations;
	char sSnapshotPath[260];
} DemoOrchCenter;

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

static bool procPreparePeerRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/orchestrate/%s", sKey);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Orchestration-Key", sKey);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static int procSelectPeers(const DemoPeerNode* arrPeer, int iPeerCount, DemoPeerNode* arrSelected, int iCap, uint32 iFanoutWidth)
{
	int i;
	int iCount = 0;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( iCount >= iCap ) {
			break;
		}
		arrSelected[iCount++] = arrPeer[i];
		if ( (uint32)iCount >= iFanoutWidth ) {
			break;
		}
	}

	return iCount;
}

static const char* procChooseOrchPlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoOrchCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "warm_shadow";
	}
	if ( pCenter->iActiveOrchestrations >= pCenter->tPolicy.iMaxActiveOrchestrations ) {
		return "defer_orchestration_busy";
	}
	if ( iSelectedPeerCount < (int)pCenter->tPolicy.iQuorumCount ) {
		return "defer_quorum_not_met";
	}
	return "orchestrate_now";
}

static bool procRunDistributedOrchestration(DemoOrchCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoStageTask tInspect;
	xfuture* pPublish = NULL;
	xfuture* pJoin = NULL;
	char arrURL[DEMO_MAX_PEER][512];
	char arrHost[DEMO_MAX_PEER][320];
	char sPublishPath[260];
	const char* sPublishJson = "{\n\t\"state\": \"distributed-published\"\n}\n";
	int iChildCount = 0;
	int iSuccessCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tInspect, 0, sizeof(tInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (sKey == NULL) ) {
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

	procCopyText(tInspect.sStage, sizeof(tInspect.sStage), "inspect_orchestration_plan");
	tInspect.iDelayMs = 40;
	tInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPreparePeerRequest(&arrReq[i], arrURL[i], sizeof(arrURL[i]), arrHost[i], sizeof(arrHost[i]), &arrPeer[i], sKey, pCenter->tPolicy.iRemoteTimeoutMs) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}

		arrChild[iChildCount] = (xfuture*)xrtHttpExecuteAsync(pEngine, &arrReq[i]);
		if ( arrChild[iChildCount] == NULL ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		if ( !xTaskGroupAddFuture(pGroup, arrChild[iChildCount]) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		iChildCount++;
	}

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
		printf("%s -> distributed_timeout\n", sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrChild[i + 1]);
		if ( (arrResp[i] != NULL) && (arrResp[i]->iStatusCode >= 200u) && (arrResp[i]->iStatusCode < 300u) ) {
			iSuccessCount++;
		}
		printf("peer[%d]=%s url=%s status=%u\n",
			i,
			arrPeer[i].sName,
			arrURL[i],
			(unsigned)((arrResp[i] != NULL) ? arrResp[i]->iStatusCode : 0u));
	}

	printf("%s -> quorum=%d/%u publish=%s\n",
		sKey,
		iSuccessCount,
		(unsigned)pCenter->tPolicy.iQuorumCount,
		sPublishPath);

	for ( i = 0; i < iChildCount; i++ ) {
		printf("child[%d] status = %d\n", i, (int)xFutureStatus(arrChild[i]));
	}

	bFinished = (iSuccessCount >= (int)pCenter->tPolicy.iQuorumCount);

cleanup:
	for ( i = 0; i < iPeerCount; i++ ) {
		if ( arrResp[i] != NULL ) {
			xrtHttpResponseDestroy(arrResp[i]);
		}
		xrtHttpRequestUnit(&arrReq[i]);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
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

static void procRecordEvent(DemoOrchEvent* pEvent, const char* sKey, const char* sPeer, const char* sState, const char* sReason, xtime tNow, uint64 iKeyHash)
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
	DemoOrchCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoPeerNode arrSelected[DEMO_MAX_PEER];
	DemoOrchEvent tEvent;
	const char* sPlan;
	int iSelected;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));
	memset(&tEvent, 0, sizeof(tEvent));

	tCenter.tPolicy.iMaxActiveOrchestrations = 1u;
	tCenter.tPolicy.iFanoutWidth = 2u;
	tCenter.tPolicy.iQuorumCount = 2u;
	tCenter.tPolicy.iRemoteTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	procBuildPath(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), ".", "distributed-orchestration", ".json");

	tBetaWarm.tTouchedAt = tNow - 150;
	tBetaWarm.iKeyHash = 5001u;
	tBetaWarm.iHeat = 11;
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
	arrPeer[1].iLatencyMs = 95u;
	arrPeer[1].iPriority = 2u;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "https://peer-c.example.com/");
	arrPeer[2].bHealthy = false;
	arrPeer[2].iLatencyMs = 70u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChooseOrchPlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseOrchPlan(NULL, iSelected, &tCenter, tNow);
	if ( strcmp(sPlan, "orchestrate_now") == 0 ) {
		tCenter.iActiveOrchestrations++;
		procRecordEvent(&tEvent, "theta", "peer-a,peer-b", "fanout_start", "quorum_target_2", tNow, 5002u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);

		if ( procRunDistributedOrchestration(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveOrchestrations--;
			procRecordEvent(&tEvent, "theta", "peer-a,peer-b", "quorum_met", "publish_completed", tNow + 1, 5002u);
			printf("%s -> %s (%s)\n", tEvent.sKey, tEvent.sState, tEvent.sReason);
		}
	}

	arrPeer[1].bHealthy = false;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseOrchPlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		procRecordEvent(&tEvent, "gamma", "peer-a", "defer_orchestration", "quorum_not_met", tNow + 2, 5003u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	tCenter.iActiveOrchestrations = tCenter.tPolicy.iMaxActiveOrchestrations;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseOrchPlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		procRecordEvent(&tEvent, "omega", "peer-a", "defer_orchestration", "active_orchestration_busy", tNow + 3, 5004u);
		printf("%s -> %s (%s)\n", tEvent.sKey, sPlan, tEvent.sReason);
	}

	printf("active_orchestrations=%u snapshot=%s\n", (unsigned)tCenter.iActiveOrchestrations, tCenter.sSnapshotPath);
	return 0;
}
```


## 6. 分布式编排里真正要稳定下来的边界

### 6.1 请求线程只提交恢复意图，不自己数成功数

这页真正稳定下来的，不是“哪个 handler 在本地数了几个 200 响应”，而是：

- 请求线程只提交 `HYDRATE`
- worker 才决定 fan-out、quorum、取消和 publish


### 6.2 peer 子请求是 child future，不是最终恢复结论

这页里 `xhttp` 真正回答的是：

- 某个 peer 子请求有没有结束
- 这台 peer 返回了什么状态

它不直接回答：

- 整次分布式恢复是不是已经达到 quorum
- 整次恢复是不是已经允许 publish

那一层仍然应该交给 orchestrator 自己的 summary 和 `task group + join future`。


### 6.3 quorum 是正式状态，不是日志里的经验值

如果没有把 quorum 做成正式状态，系统很快就会退化成：

- 有人说“差不多够了”
- 然后随手 publish

但更稳的做法应该是：

- fan-out 宽度是正式配置
- quorum 数是正式配置
- publish 只在正式 quorum 达成后发生


### 6.4 publish 仍然只能发生在本地正式收口之后

即使多个 peer 已经都返回了结果，真正要稳定下来的仍然是：

- 这些远端结果如何汇总成一个正式结论
- 然后再 publish 回本地 snapshot

否则 dashboard 和后续请求依然拿不到统一结果。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果 peer 之间要做租约、主从选举或版本仲裁，不该继续只用这个轻量 HTTP fan-out 模型
- 如果协调状态需要跨节点持久化，应继续补数据库或一致性协调层
- 如果子请求需要双向流式交互，应继续补 `xws` 或更正式的复制协议


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 active orchestration、peer catalog、recent orchestration history、recent defer history
2. `POST /api/hydrate`
	- 只提交恢复意图，让 worker 决定 fan-out、quorum 或 defer
3. `GET /api/orchestrations/:key`
	- 查询某个恢复请求当前发给了哪些 peer、现在成功数是多少、是否达到 quorum
4. `POST /api/sweep`
	- 手工或定时触发超时 orchestration 清理
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 active orchestration、peer 状态、quorum 结果和 publish 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个跨机器恢复面板](cross-machine-recovery-dashboard.md)
	对比“单 peer remote fetch”和“多 peer distributed orchestration”到底差在哪
2. [把本地控制台服务升级成一个恢复配额面板](recovery-quota-dashboard.md)
	回看 quota admission 在分布式场景里为什么仍然不能省掉
3. [把本地控制台服务升级成一个持久化协调面板](persistent-coordination-dashboard.md)
	把分布式编排继续升级成“checkpoint 可恢复、启动时可装载”的正式协调主线
