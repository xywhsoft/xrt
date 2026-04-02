# 把本地控制台服务升级成一个持久化协调面板

> 这页要解决的不是“分布式编排结束后顺手写一个 JSON 文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了多 peer fan-out / quorum / publish 之后，又开始需要把“当前协调状态在进程重启后仍能恢复”“active orchestration、last quorum、last publish 和 defer 原因要进入正式 checkpoint 文件”“worker 启动时要先装载 checkpoint 再决定后续调度”“checkpoint 不是日志副产物，而是 coordinator 的正式状态面”放进同一条后台主线时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 串成一条正式主线，而不是继续让每次 fan-out 完了之后随手 `printf` 一下状态，下一次启动再重新猜。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 跨机器恢复怎样做 peer 选择、remote fetch 和 publish 回本地
- 分布式编排怎样做 multi-peer fan-out、quorum 和统一 publish

但真实服务再往前走一步，很快又会出现一个新的问题：

- 当前进程可能会重启
- 进程重启之后，dashboard 不能把上一轮协调状态直接丢掉
- 运维和脚本仍然需要知道“上次 fan-out 发给了谁”“上次是不是已经达到 quorum”“是不是已经 publish 完成”
- worker 启动时也需要有一份正式状态来决定是继续新请求、先展示旧状态，还是进入恢复模式

如果这时还把实现停在：

- `printf("quorum met\\n");`
- `write log and move on`

很快就会出现几个典型问题：

- active / quorum / publish 状态完全依赖内存，不可恢复
- 重启之后页面只能显示“空白当前状态”
- 脚本和运维无法判断上一轮编排是否真正 publish 完成
- 协调状态被拆成日志、局部变量和一堆不稳定输出，不能作为正式状态面

所以这页真正要补出的，不是“再写一个配置文件”，而是：

> 当分布式恢复已经成为长期运行主线时，怎样把协调状态 checkpoint 成正式文件，并在下一次启动时把它重新装回主线。


## 2. 为什么这次不能只靠“分布式编排 + 日志”

### 2.1 分布式编排只回答“这一轮怎么 fan-out / quorum”

上一页里的分布式编排已经很好地解决了：

- 当前这次恢复要 fan-out 给哪些 peer
- 当前 quorum 是多少
- 什么时候允许 publish

但它不回答：

- 这些状态重启之后怎么办
- 哪一份文件才是 coordinator 的正式状态面
- worker 启动时是从哪里恢复上次状态


### 2.2 日志不是正式协调状态

日志很重要，因为它能回答：

- 某次 fan-out 是什么时候开始的
- 某个 peer 当时回了什么

但日志不回答：

- 当前 checkpoint 里的 active orchestration 是多少
- 最后一次 quorum 是不是已经达成
- publish 是否已经作为正式状态写回


### 2.3 这次真正新增的是“checkpoint 层”

更稳的分工方式是：

- `task group + future`
	- 仍然管理一次分布式恢复的 child 和 join
- `xurl + xhttp`
	- 仍然管理每个 peer 子请求
- `value + json + file`
	- 负责把 coordinator 状态持久化成正式 checkpoint

一句话记住：

> 上一页补的是“这一轮怎么协调”，这一页补的是“这一轮协调完之后，状态怎么持久化并在下次启动时装回来”。


## 3. 这条持久化协调主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active coordination registry | 当前有哪些分布式恢复正在运行 |
| `list` | peer catalog | 哪些节点参与过 fan-out，健康状态和优先级如何 |
| `list` | recent orchestration history | 页面和 JSON 展示最近一次 fan-out / quorum / publish |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入协调 |
| `list` | recent publish history | 页面和 JSON 展示哪次 checkpoint 已经进入正式 published 状态 |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在协调和 checkpoint 上 |
| `task group` | 管住一次 orchestration 的所有 child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 inspect、每个 peer fetch、publish、checkpoint 的完成态 | 统一结果与 join 边界 |
| `xurl + xhttp` | fan-out 到多个 peer | 让远端请求进入正式网络主线 |
| `value + json` | 构造和解析 checkpoint | 让协调状态有正式结构，而不是散落字段 |
| `file + path` | 读写 checkpoint 文件 | 让状态能跨进程、跨重启继续存在 |
| `file_async` | 把最终 publish 结果异步写回本地 snapshot | publish 仍然走正式 future 主线 |
| `time` | 记录 checkpoint 时间、quorum 时间、publish 时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 一个稳定指纹 | 生成 checkpoint / snapshot 文件名与轻量标识 |
| `xhttpd + template` | 展示当前 checkpoint 与恢复状态 | 浏览器和脚本共享一个入口 |

一句话记住：

> `distributed orchestration` 决定这一轮怎么协调，`persistent coordination` 决定这一轮协调完之后怎么留下正式状态并在下一次启动时恢复出来。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成持久化协调语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/coordination-checkpoint.json
runtime/published/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 checkpoint、recent orchestration history、recent defer history
- `runtime/console.log`
	- 记录 hydrate signal、fan-out、quorum、checkpoint 写入和 publish 完成
- `runtime/coordination-checkpoint.json`
	- 保存最近一次正式协调状态
- `runtime/published/<hash>.json`
	- 保存 quorum 达成后的正式本地 snapshot


## 5. 先把“启动时装载 checkpoint -> fan-out -> publish -> checkpoint 更新”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先从 checkpoint 文件装载最近状态
2. `dict` 表示当前 active coordination registry
3. `list` 表示 peer catalog 和 recent histories
4. `queue + thread` 仍然只承接 `HYDRATE / SWEEP`
5. `task group` 仍然只管理一次 orchestration
6. local inspect 用线程 child 表示
7. 多个 peer fetch 仍然用真实 `xurl + xhttp` future 表示
8. publish 仍然用 `file_async future` 表示
9. checkpoint 用 `value + json + file` 写成正式状态
10. join 完成后再把最终状态写回 checkpoint

这个骨架会展示这些事：

- 启动时先写入一个 boot checkpoint，然后立刻按正式方式再读回来
- `beta` 仍有 warm shadow，不进入持久化协调
- `theta` fan-out 给 `peer-a` 和 `peer-b`，达到 quorum 后 publish，并写回 checkpoint
- `gamma` 只有一个健康 peer，不满足 quorum，这次直接 defer，并写回 checkpoint

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
} DemoPersistEvent;

typedef struct
{
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
} DemoPersistPolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pHistory;
	xlist pDeferred;
	xlist pPublished;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoPersistPolicy tPolicy;
	uint32 iActiveOrchestrations;
	char sCheckpointPath[260];
	char sSnapshotPath[260];
} DemoPersistCenter;

typedef struct
{
	char sStage[32];
	int iDelayMs;
	int iStatus;
} DemoStageTask;

typedef struct
{
	char sPath[260];
	char sState[32];
	char sKey[64];
	uint32 iActive;
	uint32 iQuorumDone;
	int iStatus;
} DemoCheckpointTask;


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
		(sDir != NULL) ? sDir : "state",
		(unsigned long long)xrtHash64((ptr)sSafeKey, strlen(sSafeKey)),
		(sExt != NULL) ? sExt : ".json");
}

static bool procSaveCheckpointSummary(const char* sCheckpointPath, const char* sState, const char* sKey, uint32 iActive, uint32 iQuorumDone)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( sCheckpointPath == NULL ) {
		return false;
	}

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)((sState != NULL) ? sState : ""), 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "active_orchestrations", 0, (int32)iActive);
	xvoTableSetInt(vRoot, "quorum_done", 0, (int32)iQuorumDone);
	xvoTableSetInt(vRoot, "updated_at", 0, (int32)xrtNow());

	sDir = xrtPathGetDir((str)sCheckpointPath, 0u);
	if ( (sDir != NULL) && !xrtDirCreateAll(sDir) ) {
		goto cleanup;
	}

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	bOk = xrtFileWriteAll((str)sCheckpointPath, sJson, iJsonSize, XRT_CP_UTF8) > 0;

cleanup:
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}
	if ( sDir != NULL ) {
		xrtFree(sDir);
	}
	if ( vRoot != NULL ) {
		xvoUnref(vRoot);
	}

	return bOk;
}

static xvalue procLoadCheckpointSummary(const char* sCheckpointPath)
{
	str sJson = NULL;
	size_t iJsonSize = 0;
	xvalue vRoot = NULL;

	if ( (sCheckpointPath == NULL) || !xrtPathExists((str)sCheckpointPath) ) {
		return NULL;
	}

	sJson = xrtFileReadAll((str)sCheckpointPath, XRT_CP_UTF8, &iJsonSize);
	if ( sJson == NULL ) {
		return NULL;
	}

	vRoot = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);

	if ( (vRoot == NULL) || xvoIsNull(vRoot) || (xvoType(vRoot) != XVO_DT_TABLE) ) {
		if ( vRoot != NULL ) {
			xvoUnref(vRoot);
		}
		return NULL;
	}

	return vRoot;
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

static int32 procCheckpointTask(ptr pArg, xfuture_result* pOut)
{
	DemoCheckpointTask* pTask = (DemoCheckpointTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pTask->iStatus = procSaveCheckpointSummary(
		pTask->sPath,
		pTask->sState,
		pTask->sKey,
		pTask->iActive,
		pTask->iQuorumDone) ? XRT_NET_OK : XRT_NET_ERROR;

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

	snprintf(sRef, sizeof(sRef), "/api/persist/%s", sKey);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Persist-Key", sKey);
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

static const char* procChoosePersistPlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoPersistCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "warm_shadow";
	}
	if ( iSelectedPeerCount < (int)pCenter->tPolicy.iQuorumCount ) {
		return "defer_quorum_not_met";
	}
	return "persist_now";
}

static bool procRunPersistedOrchestration(DemoPersistCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoStageTask tInspect;
	DemoCheckpointTask tCheckpointStart;
	xfuture* pPublish = NULL;
	xfuture* pJoin = NULL;
	char arrURL[DEMO_MAX_PEER][512];
	char arrHost[DEMO_MAX_PEER][320];
	char sPublishPath[260];
	const char* sPublishJson = "{\n\t\"state\": \"persisted-published\"\n}\n";
	int iChildCount = 0;
	int iSuccessCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tInspect, 0, sizeof(tInspect));
	memset(&tCheckpointStart, 0, sizeof(tCheckpointStart));

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

	procCopyText(tInspect.sStage, sizeof(tInspect.sStage), "inspect_persistent_plan");
	tInspect.iDelayMs = 40;
	tInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tCheckpointStart.sPath, sizeof(tCheckpointStart.sPath), pCenter->sCheckpointPath);
	procCopyText(tCheckpointStart.sState, sizeof(tCheckpointStart.sState), "fanout_started");
	procCopyText(tCheckpointStart.sKey, sizeof(tCheckpointStart.sKey), sKey);
	tCheckpointStart.iActive = 1u;
	tCheckpointStart.iQuorumDone = 0u;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procCheckpointTask, &tCheckpointStart, 0u);
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

		arrChild[iChildCount] = xrtHttpExecuteAsync(pEngine, &arrReq[i]);
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
	pPublish = xrtFileWriteAllAsync((str)sPublishPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
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
		printf("%s -> persisted_timeout\n", sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrChild[i + 2]);
		if ( (arrResp[i] != NULL) && (arrResp[i]->iStatusCode >= 200u) && (arrResp[i]->iStatusCode < 300u) ) {
			iSuccessCount++;
		}
		printf("peer[%d]=%s status=%u\n",
			i,
			arrPeer[i].sName,
			(unsigned)((arrResp[i] != NULL) ? arrResp[i]->iStatusCode : 0u));
	}

	bFinished = (iSuccessCount >= (int)pCenter->tPolicy.iQuorumCount);
	(void)procSaveCheckpointSummary(
		pCenter->sCheckpointPath,
		bFinished ? "published" : "quorum_not_met",
		sKey,
		0u,
		(uint32)iSuccessCount);

	printf("%s -> checkpoint=%s quorum=%d/%u publish=%s\n",
		sKey,
		pCenter->sCheckpointPath,
		iSuccessCount,
		(unsigned)pCenter->tPolicy.iQuorumCount,
		sPublishPath);

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

int main(void)
{
	DemoPersistCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoPeerNode arrSelected[DEMO_MAX_PEER];
	xvalue vBoot = NULL;
	const char* sPlan;
	int iSelected = 0;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iFanoutWidth = 2u;
	tCenter.tPolicy.iQuorumCount = 2u;
	tCenter.tPolicy.iRemoteTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/coordination-checkpoint.json");
	procBuildPath(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "published", "coordination", ".json");

	(void)procSaveCheckpointSummary(tCenter.sCheckpointPath, "boot", "none", 0u, 0u);
	vBoot = procLoadCheckpointSummary(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s key=%s active=%d quorum_done=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			xvoTableGetText(vBoot, "key", 0),
			(int)xvoTableGetInt(vBoot, "active_orchestrations", 0),
			(int)xvoTableGetInt(vBoot, "quorum_done", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = tNow - 120;
	tBetaWarm.iKeyHash = 6001u;
	tBetaWarm.iHeat = 18;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "https://peer-a.example.com/");
	arrPeer[0].bHealthy = true;
	arrPeer[0].iLatencyMs = 70u;
	arrPeer[0].iPriority = 1u;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "https://peer-b.example.com/");
	arrPeer[1].bHealthy = true;
	arrPeer[1].iLatencyMs = 90u;
	arrPeer[1].iPriority = 2u;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "https://peer-c.example.com/");
	arrPeer[2].bHealthy = false;
	arrPeer[2].iLatencyMs = 60u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChoosePersistPlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChoosePersistPlan(NULL, iSelected, &tCenter, tNow);
	if ( strcmp(sPlan, "persist_now") == 0 ) {
		tCenter.iActiveOrchestrations = 1u;
		if ( procRunPersistedOrchestration(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveOrchestrations = 0u;
		}
	}

	arrPeer[1].bHealthy = false;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChoosePersistPlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		(void)procSaveCheckpointSummary(tCenter.sCheckpointPath, "defer_quorum_not_met", "gamma", 0u, 1u);
		printf("gamma -> %s\n", sPlan);
	}

	printf("checkpoint=%s snapshot=%s\n", tCenter.sCheckpointPath, tCenter.sSnapshotPath);
	return 0;
}
```


## 6. 持久化协调里真正要稳定下来的边界

### 6.1 checkpoint 是正式状态面，不是日志副产物

这页真正稳定下来的，不是“顺手写了个 JSON”，而是：

- checkpoint 文件是 coordinator 的正式状态面
- worker 启动时会先装载它
- dashboard 和脚本也会把它当成正式状态来读


### 6.2 `value + json` 回答的是“状态怎么持久化”，不是“请求怎么协调”

这页里 `xvalue + json` 真正回答的是：

- 当前 active / quorum / publish / defer 怎么写成正式结构
- 下次启动时怎样再读回来

它不回答：

- peer 子请求怎么发
- quorum 怎么算
- 哪个 peer 该取消

那一层仍然应该交给 distributed orchestrator。


### 6.3 checkpoint 更新时机必须是正式策略

如果没有把 checkpoint 更新时机做成正式策略，系统很快就会退化成：

- 想起来就写一次
- 忙的时候就不写

但更稳的做法应该是：

- fan-out 开始前写一次
- quorum / publish 完成后再写一次
- defer 发生时也要写正式状态


### 6.4 publish 和 checkpoint 是两条不同职责

这页里最容易写混的地方是：

- 以为 publish 文件就是 checkpoint 文件

但更稳的边界应该是：

- publish 文件
	- 回答“业务恢复结果是什么”
- checkpoint 文件
	- 回答“协调状态走到了哪一步”


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果 checkpoint 需要跨节点共享，不该继续只用本地文件
- 如果恢复状态要支持强一致恢复，应继续补仲裁或租约层
- 如果状态机需要事务和回滚，应继续补数据库或正式 WAL 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 checkpoint、recent orchestration history、recent defer history
2. `POST /api/hydrate`
	- 只提交恢复意图，让 worker 决定 fan-out / quorum / checkpoint 更新
3. `GET /api/checkpoint`
	- 直接返回最近一次 checkpoint 的结构化状态
4. `POST /api/sweep`
	- 手工或定时触发超时 coordination 清理与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 checkpoint、peer 状态、quorum 结果和 publish 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
	对比“瞬时 fan-out / quorum”与“持久化协调状态”到底差在哪
2. [把本地控制台服务升级成一个跨机器恢复面板](cross-machine-recovery-dashboard.md)
	回看单 peer remote fetch 在持久化协调场景里为什么仍然是基础块
3. [把本地控制台服务升级成一个一致性仲裁面板](consensus-arbitration-dashboard.md)
	把持久化协调继续升级成真正带 `term / majority / commit index` 的正式裁决主线
