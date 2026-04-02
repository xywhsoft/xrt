# 把本地控制台服务升级成一个分布式接管编排面板

> 这页要解决的不是“仲裁已经选出 winner，剩下的节点按 winner 跑就行”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `owner / winner / term / round / local_priority / remote_priority`，并且上一页已经能把多个 contender 的 claim 正式仲裁成一个 winner 之后，又开始需要把“多个 peer 的接管计划怎样在同一轮里被看见”“为什么 winner 已经决出来，不代表 distributed takeover 已经编排完成”“loser 回退、winner publish、follow-up plan 下发和 checkpoint 更新为什么仍然要进同一条 orchestration 主线”“如果不同 peer 返回了不同阶段的 plan summary，本地应该怎样统一判断本轮 takeover orchestration 到底有没有正式收口”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在仲裁之后靠“winner 自己通知一下其他节点”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- unfinished work 怎样被正式接手
- 多个 contender 的 claim 怎样比较并决出 winner
- winner 文件和 arbitration checkpoint 为什么必须是正式状态

但真实系统再往前走一步，很快又会出现一个新的问题：

- winner 决出来，不代表各个 peer 都已经收到并接受同一份 takeover plan
- loser 可能需要回退到 follower / standby，而不是继续保留旧 owner 状态
- 不同 peer 返回的 plan summary 可能还停在不同阶段
- 如果没有正式的 distributed takeover orchestration 层，系统仍然会回到“winner 自己发个通知，其他节点看着办”

如果这时还把实现停在：

- `if ( winner == local ) publish once;`
- `if ( peer says ok ) count it as done;`
- `if ( majority acked ) treat orchestration finished;`

很快就会出现几个典型问题：

- arbitration checkpoint 说 winner 已经确定，但某些 peer 还保留旧 owner
- publish 文件已经更新，loser rollback 却还没真正进入同一轮计划
- 一部分 peer 接收的是 round 4，一部分 peer 接收的是 round 5
- 下次启动只能看到 winner，已经看不清本轮编排到底推进到了哪一阶段

所以这页真正要补出的，不是“winner 出来后的广播”，而是：

> 当 winner 已经正式仲裁出来之后，怎样把 plan gather、peer fan-out、ack 汇总、loser rollback 和 published orchestration summary 做成一条可恢复、可解释、可导出的正式分布式接管编排主线。


## 2. 为什么这次不能只靠“winner 决出来后通知一下”

### 2.1 `ownership_arbitrated` 只回答“这轮 winner 是谁”

上一页的 ownership 仲裁已经很好地解决了：

- 本地 claim 和远端 claim 怎样进入同一个比较范围
- winner 应该怎样正式写入 checkpoint
- published winner 文件怎样对外发布

但它不回答：

- loser 是否已经回退到正确状态
- 其余 peer 是否已经进入本轮 takeover plan
- 当前 orchestration 的 ack 到底够不够回答“本轮已经正式收口”


### 2.2 orchestration 结果不是通知副作用，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前 `round / winner / ack_count / required_ack` 是多少
- 当前有多少 peer 已经接受本轮计划
- 当前 loser rollback 有没有进到同一轮编排
- 当前 published orchestration summary 和 checkpoint 是否描述的是同一轮

这说明：

- `winner` 只是仲裁结果
- `distributed_takeover_checkpoint` 才是编排状态


### 2.3 这次真正新增的是“winner 之后的多节点协调状态层”

更稳的分工方式是：

- `ownership arbitration`
	- 负责多个 contender 怎样比较并选出 winner
- `distributed takeover orchestration`
	- 负责 winner 之后怎样把 plan fan-out 到多个 peer，并统一收口 ack / rollback
- `consensus-style ownership`
	- 再往下负责更正式的仲裁日志、跨轮稳定性和持久化投票

一句话记住：

> 上一页补的是“谁赢”，这一页补的是“winner 赢了之后，整组节点怎样进入同一轮 takeover plan 并正式收口”。


## 3. 这条分布式接管编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active orchestration registry | 当前有哪些 key 正在做 distributed takeover orchestration |
| `list` | recent orchestration history | 页面和 JSON 展示最近 plan fan-out / ack / rollback 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 orchestration |
| `queue + thread` | 后台消费 `ORCHESTRATE / SWEEP` | 请求线程不阻塞在 plan gather、ack 汇总和 rollback 上 |
| `task group` | 管住一次 orchestration scope 里的 local inspect / multi-peer fetch / local rollback child | `Close / Join / Cancel / Destroy` |
| `future` | 表达多 peer summary、publish orchestration 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向多个 peer 拉取或推送计划摘要 | 让多节点 plan gather 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取本地 plan、写 rollback / orchestration 文件 | 让编排进入正式文件主线 |
| `file_async` | 把 published orchestration summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 orchestration checkpoint、peer summary、publish JSON | 让 `ack_count / required_ack / winner / round` 有正式结构 |
| `time` | 记录 orchestration 启动、fan-out、ack 收口、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `winner / round / ack_count / required_ack / rollback_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `ownership arbitration` 管“谁赢”，`distributed takeover orchestration` 管“winner 之后怎样让整组节点正式进入同一轮计划并收口”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 distributed takeover orchestration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/distributed-takeover-checkpoint.json
runtime/distributed-takeover-plan/<round>.json
runtime/distributed-takeover-published.json
runtime/peer-orchestration-summary.json
```

其中：

- `config/console.json`
	- 保存 `orchestration_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `winner / round / ack_count / required_ack / rollback_state`
- `runtime/console.log`
	- 记录 orchestration 启动、peer summary join、loser rollback 和 publish
- `runtime/distributed-takeover-checkpoint.json`
	- 保存最近一次正式 distributed takeover orchestration 状态
- `runtime/distributed-takeover-plan/<round>.json`
	- 保存当前轮次的本地 takeover plan
- `runtime/distributed-takeover-published.json`
	- 保存异步发布后的 orchestration 输出
- `runtime/peer-orchestration-summary.json`
	- 保存这次多 peer orchestration summary 的本地 stage 文件


## 5. 先把“启动装载 orchestration checkpoint -> inspect local plan -> multi-peer gather -> loser rollback -> publish summary”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `distributed-takeover-checkpoint.json`
2. checkpoint 里明确记录 `winner / round / ack_count / required_ack / rollback_state`
3. `dict` 表示当前 active orchestration registry
4. `list` 表示 orchestration / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 orchestration scope
7. local plan inspect 用一个 child 表达
8. 多个 peer summary 走多条 `xhttp future`
9. loser rollback 先用一个本地 child 表达
10. join 完成本轮 gather 后，再统一用 `file_async future` 发布 orchestration summary

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`winner = node-b`、`round = 5`、`required_ack = 2`
- 本地会准备一份 round 5 的 takeover plan
- worker 会从 `peer-a`、`peer-b` 拉两份 orchestration summary
- 再把 local inspect、multi-peer gather、loser rollback 收进同一个 orchestration scope
- 最后异步发布 `runtime/distributed-takeover-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
	char sBaseURL[160];
	uint32 iLatencyMs;
	int iPriority;
	bool bHealthy;
} DemoPeerNode;

typedef struct
{
	uint32 iOrchestrationTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iRequiredAck;
} DemoOrchPolicy;

typedef struct
{
	xdict pActive;
	xlist pOrchestrationHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoOrchPolicy tPolicy;
	uint32 iActiveOrchestration;
	uint32 iTerm;
	uint32 iRound;
	uint32 iAckCount;
	uint32 iRequiredAck;
	char sWinner[32];
	char sRollbackState[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sPeerSummaryPath[260];
} DemoOrchCenter;

typedef struct
{
	char sStage[64];
	char sPath[260];
	uint32 iDelayMs;
	int iStatus;
} DemoStageTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPlanPath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/distributed-takeover-plan/%08u.json", (unsigned)iRound);
}

static bool procSaveOrchCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sWinner,
	const char* sRollbackState,
	uint32 iTerm,
	uint32 iRound,
	uint32 iAckCount,
	uint32 iRequiredAck,
	uint32 iDone)
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
	xvoTableSetText(vRoot, "winner", 0, (uint8*)((sWinner != NULL) ? sWinner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "rollback_state", 0, (uint8*)((sRollbackState != NULL) ? sRollbackState : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "ack_count", 0, (int32)iAckCount);
	xvoTableSetInt(vRoot, "required_ack", 0, (int32)iRequiredAck);
	xvoTableSetInt(vRoot, "done", 0, (int32)iDone);
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

static xvalue procLoadOrchCheckpoint(const char* sCheckpointPath)
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

static bool procWritePlanFile(uint32 iRound, const char* sKey, const char* sWinner)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildPlanPath(sPath, sizeof(sPath), iRound);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"plan_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "winner", 0, (uint8*)((sWinner != NULL) ? sWinner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "created_at", 0, (int32)xrtNow());

	sDir = xrtPathGetDir((str)sPath, 0u);
	if ( (sDir != NULL) && !xrtDirCreateAll(sDir) ) {
		goto cleanup;
	}

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	bOk = xrtFileWriteAll((str)sPath, sJson, iJsonSize, XRT_CP_UTF8) > 0;

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

static int32 procStageTask(ptr pArg, xfuture_result* pOut)
{
	DemoStageTask* pTask = (DemoStageTask*)pArg;
	str sJson = NULL;
	size_t iJsonSize = 0;
	xvalue vRoot = NULL;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	if ( pTask->sPath[0] != '\0' ) {
		sJson = xrtFileReadAll((str)pTask->sPath, XRT_CP_UTF8, &iJsonSize);
		if ( sJson == NULL ) {
			pTask->iStatus = XRT_NET_ERROR;
			goto finish;
		}

		vRoot = xrtParseJSON(sJson, iJsonSize);
		if ( (vRoot == NULL) || xvoIsNull(vRoot) ) {
			pTask->iStatus = XRT_NET_ERROR;
			goto finish;
		}
	}

	xrtSleep(pTask->iDelayMs);
	pTask->iStatus = XRT_NET_OK;

finish:
	if ( vRoot != NULL ) {
		xvoUnref(vRoot);
	}
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}

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
	uint32 iRound,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[224];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/distributed-takeover/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Takeover-Key", sKey);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static int procSelectPeers(const DemoPeerNode* arrPeer, int iCount, uint32 iMaxPeerLatencyMs, const DemoPeerNode** arrOut, int iCap)
{
	int iOut = 0;
	int i;

	if ( (arrPeer == NULL) || (arrOut == NULL) || (iCap <= 0) ) {
		return 0;
	}

	for ( i = 0; i < iCount; i++ ) {
		if ( iOut >= iCap ) {
			break;
		}
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( arrPeer[i].iLatencyMs > iMaxPeerLatencyMs ) {
			continue;
		}
		arrOut[iOut++] = &arrPeer[i];
	}

	return iOut;
}

static const char* procChooseOrchestrationPlan(
	const DemoWarmItem* pWarm,
	int iPeerCount,
	const DemoOrchCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveOrchestration > 0u) ) {
		return "defer_orchestration_busy";
	}
	if ( iPeerCount <= 0 ) {
		return "defer_no_peer_plan";
	}
	return "orchestrate_now";
}

static bool procRunDistributedOrchestration(DemoOrchCenter* pCenter, const DemoPeerNode** arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xfuture* arrRemoteFetch[DEMO_MAX_PEER];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	char arrURL[DEMO_MAX_PEER][512];
	char arrHostHeader[DEMO_MAX_PEER][320];
	DemoStageTask tPlanInspect;
	DemoStageTask tRollback;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sPlanPath[260];
	char sPublishJson[512];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tPlanInspect, 0, sizeof(tPlanInspect));
	memset(&tRollback, 0, sizeof(tRollback));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveOrchCheckpoint(
		pCenter->sCheckpointPath,
		"distributed_takeover_opened",
		sKey,
		pCenter->sWinner,
		pCenter->sRollbackState,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iAckCount,
		pCenter->iRequiredAck,
		0u);

	xrtNetEngineConfigInit(&tEngineCfg);
	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		goto cleanup;
	}
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		goto cleanup;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		goto cleanup;
	}

	procBuildPlanPath(sPlanPath, sizeof(sPlanPath), pCenter->iRound);
	procCopyText(tPlanInspect.sStage, sizeof(tPlanInspect.sStage), "inspect_local_plan");
	procCopyText(tPlanInspect.sPath, sizeof(tPlanInspect.sPath), sPlanPath);
	tPlanInspect.iDelayMs = 20u;
	tPlanInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tPlanInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tRollback.sStage, sizeof(tRollback.sStage), "rollback_loser");
	procCopyText(tRollback.sPath, sizeof(tRollback.sPath), pCenter->sCheckpointPath);
	tRollback.iDelayMs = 40u;
	tRollback.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tRollback, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPreparePeerRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iRound,
			pCenter->tPolicy.iOrchestrationTimeoutMs) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}

		arrRemoteFetch[i] = (xfuture*)xrtHttpExecuteAsync(pEngine, &arrReq[i]);
		if ( arrRemoteFetch[i] == NULL ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		if ( !xTaskGroupAddFuture(pGroup, arrRemoteFetch[i]) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		arrChild[iChildCount++] = arrRemoteFetch[i];
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pCenter->iAckCount = 1u;
	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrRemoteFetch[i]);
		if ( (arrResp[i] == NULL) || (arrResp[i]->iStatusCode >= 500u) ) {
			continue;
		}

		if ( (arrResp[i]->pBody != NULL) && (arrResp[i]->iBodyLen > 0u) ) {
			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				if ( xvoTableGetInt(vRemote, "ack_round", 0) == (int32)pCenter->iRound ) {
					pCenter->iAckCount += 1u;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iAckCount >= pCenter->iRequiredAck ) {
		procCopyText(pCenter->sRollbackState, sizeof(pCenter->sRollbackState), "rollback_applied");
	} else {
		procCopyText(pCenter->sRollbackState, sizeof(pCenter->sRollbackState), "rollback_pending");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"distributed-orchestrated\",\n\t\"winner\": \"%s\",\n\t\"ack_count\": %u,\n\t\"required_ack\": %u,\n\t\"rollback_state\": \"%s\"\n}\n",
		pCenter->sWinner,
		(unsigned)pCenter->iAckCount,
		(unsigned)pCenter->iRequiredAck,
		pCenter->sRollbackState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveOrchCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "distributed_takeover_orchestrated" : "distributed_takeover_incomplete",
		sKey,
		pCenter->sWinner,
		pCenter->sRollbackState,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iAckCount,
		pCenter->iRequiredAck,
		bFinished ? 1u : 0u);

	if ( pPublish != NULL ) {
		xFutureRelease(pPublish);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iPeerCount; i++ ) {
		if ( arrResp[i] != NULL ) {
			xrtHttpResponseDestroy(arrResp[i]);
		}
		if ( arrReq[i].sURL[0] != '\0' ) {
			xrtHttpRequestUnit(&arrReq[i]);
		}
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}

	return bFinished;
}

int main(void)
{
	DemoOrchCenter tCenter;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	const DemoPeerNode* arrSelected[DEMO_MAX_PEER];
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const char* sPlan = NULL;
	int iPeerCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iOrchestrationTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iRequiredAck = 2u;
	tCenter.iTerm = 18u;
	tCenter.iRound = 5u;
	tCenter.iAckCount = 1u;
	tCenter.iRequiredAck = 2u;
	procCopyText(tCenter.sWinner, sizeof(tCenter.sWinner), "node-b");
	procCopyText(tCenter.sRollbackState, sizeof(tCenter.sRollbackState), "rollback_pending");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/distributed-takeover-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/distributed-takeover-published.json");
	procCopyText(tCenter.sPeerSummaryPath, sizeof(tCenter.sPeerSummaryPath), "runtime/peer-orchestration-summary.json");

	(void)procSaveOrchCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sWinner,
		tCenter.sRollbackState,
		tCenter.iTerm,
		tCenter.iRound,
		tCenter.iAckCount,
		tCenter.iRequiredAck,
		0u);
	(void)procWritePlanFile(tCenter.iRound, "theta", tCenter.sWinner);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 20u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 34u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadOrchCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: winner=%s round=%d ack=%d/%d rollback=%s\n",
			xvoTableGetText(vBoot, "winner", 0),
			(int)xvoTableGetInt(vBoot, "round", 0),
			(int)xvoTableGetInt(vBoot, "ack_count", 0),
			(int)xvoTableGetInt(vBoot, "required_ack", 0),
			xvoTableGetText(vBoot, "rollback_state", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 70;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 4;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	iPeerCount = procSelectPeers(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs, arrSelected, DEMO_MAX_PEER);
	sPlan = procChooseOrchestrationPlan(NULL, iPeerCount, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "orchestrate_now") == 0 ) {
		tCenter.iActiveOrchestration = 1u;
		if ( procRunDistributedOrchestration(&tCenter, arrSelected, iPeerCount, "theta") ) {
			tCenter.iActiveOrchestration = 0u;
		}
	}

	printf("checkpoint=%s published=%s winner=%s ack=%u/%u rollback=%s\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		tCenter.sWinner,
		(unsigned)tCenter.iAckCount,
		(unsigned)tCenter.iRequiredAck,
		tCenter.sRollbackState);
	return 0;
}
```


## 6. 分布式接管编排里真正要稳定下来的边界

### 6.1 `winner / round / ack_count / required_ack / rollback_state` 必须进入正式 checkpoint

这页真正稳定下来的，不是“winner 看起来已经广播出去了”，而是：

- 当前本轮 orchestration 的 round 是多少
- 当前到底有多少 peer 已经进入这轮计划
- 当前 loser rollback 走到了哪一步
- 当前 published summary 和 checkpoint 是否描述的是同一轮

如果这些字段不进入正式 checkpoint，下次启动就无法判断本轮分布式接管到底完成到了哪一步。


### 6.2 local inspect、multi-peer gather、loser rollback 必须进入同一个 orchestration scope

这页最容易写错的一点是：

- 先读本地 plan
- 再分散地去各个 peer 拉 summary
- 最后 winner 线程顺手把 loser 状态改了

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 local inspect、multi-peer gather、loser rollback 都挂进同一个 scope
- 最后再统一汇总 ack，并发布 orchestration summary

否则系统会出现“本地计划是 round 5，但某个 peer 还在 round 4，published 文件却已经说 orchestration 完成”的拆裂状态。


### 6.3 published orchestration 文件和 distributed checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published orchestration 文件已经足够回答当前分布式接管状态

但更稳的边界应该是：

- `runtime/distributed-takeover-published.json`
	- 回答“这次编排对外发布了什么”
- `runtime/distributed-takeover-checkpoint.json`
	- 回答“这轮 fan-out / ack / rollback 当前走到了哪一步”


### 6.4 这页先停在“多节点 takeover 已被正式编排”，不提前把它说成“更重的共识式 ownership 已经讲完”

这页故意把边界停在：

- local plan 已 inspect
- 多个 peer summary 已 join
- loser rollback 已进入同一轮
- published orchestration summary 已经异步写出

但它还没有回答：

- 更正式的仲裁日志和持久化投票怎样接进来
- 跨轮 winner 稳定性怎样防止抖动
- consensus-style ownership 怎样统一多轮 arbitration 和 orchestration

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要更正式的投票日志和持久化轮次，应继续补 consensus-style ownership 主线
- 如果需要跨轮 winner 稳定性保证，应继续补更重的 distributed arbitration 主线
- 如果需要把 arbitration、orchestration、rollback 统一成更长链路，应继续补 consensus-style takeover recovery 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `winner / round / ack_count / required_ack / rollback_state`
2. `POST /api/orchestrate`
	- 只提交 orchestration 意图，让 worker 决定是否进入 local inspect / multi-peer gather / rollback 主线
3. `GET /api/distributed-takeover`
	- 直接返回最近一次 distributed takeover checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 orchestration 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 winner、ack 进度、rollback 状态、published summary 和 recent orchestration history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
	先对比“winner 已经被选出来”与“多节点已经进入同一轮 takeover plan”到底差在哪
2. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
	回看 `winner / cursor / publish` 为什么在 orchestration 之后仍然是恢复链的底层边界
3. [把本地控制台服务升级成一个共识式 ownership 面板](consensus-style-ownership-dashboard.md)
	把投票日志、持久化轮次、跨轮 winner 稳定性和统一收口主线正式补出来
