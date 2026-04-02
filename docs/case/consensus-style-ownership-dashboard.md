# 把本地控制台服务升级成一个共识式 ownership 面板

> 这页要解决的不是“分布式接管编排已经收口，所以 winner 就稳定了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `winner / round / ack_count / required_ack / rollback_state`，并且上一页已经能把多节点 takeover plan 正式编排到同一轮之后，又开始需要把“这一轮 winner 什么时候才能算 committed owner”“为什么 ack 够了不等于 ownership 已经稳定”“本地投票日志、远端投票摘要、committed owner 发布为什么仍然要进同一个 consensus scope”“如果不同 peer 对同一轮给出了不同投票摘要，本地应该怎样把 `vote_log / remote grant / committed_round / published owner` 收成同一条稳定主线”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 orchestration 之后靠“多数 peer 看起来已经接受了”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 多个 contender 的 claim 怎样被正式仲裁成一个 winner
- winner 之后怎样把 takeover plan fan-out 到多个 peer，并统一收口 ack / rollback
- `winner / round / ack_count / rollback_state` 为什么必须进入正式 checkpoint

但真实系统再往前走一步，很快又会出现一个新的问题：

- orchestration 能回答“这一轮计划被发出去了”，但不自动回答“committed owner 已经稳定了”
- 某些 peer 可能已经进入 round 6，某些 peer 还只承认 round 5
- 本地如果没有正式 vote log，很难解释当前 committed owner 是怎么定下来的
- 如果没有专门的 consensus-style ownership 层，系统仍然会回到“看起来多数 peer 都差不多同意了”

如果这时还把实现停在：

- `if ( ack_count >= required_ack ) committed_owner = winner;`
- `if ( peer replied ok ) count it as grant;`
- `if ( published owner exists ) treat round as stable;`

很快就会出现几个典型问题：

- checkpoint 说本轮已 committed，但本地没有任何可回放的投票日志
- 一部分 peer 的 grant 属于旧 round，却被当前 round 误算成多数
- published owner 文件已经更新，某些 peer 却还没真正承认这轮 committed owner
- 下次启动时只能看见最终 owner，看不清它是通过哪一轮、多少 grant 稳定下来的

所以这页真正要补出的，不是“多数确认一下”，而是：

> 当 winner 和 orchestration 都已经成立之后，怎样把本地 vote log、远端 grant 摘要、committed round 判断、published committed owner 和 checkpoint 更新做成一条可恢复、可解释、可导出的正式共识式 ownership 主线。


## 2. 为什么这次不能只靠“ack 足够了就算 committed”

### 2.1 `distributed_takeover_orchestrated` 只回答“本轮 plan 已编排”

上一页的分布式接管编排已经很好地解决了：

- winner 之后怎样把同一轮 plan fan-out 到多个 peer
- loser rollback 怎样进入同一轮 orchestration scope
- `ack_count / required_ack / rollback_state` 怎样进入正式 checkpoint

但它不回答：

- 这些 ack 到底是不是当前 round 的正式 grant
- 当前 committed owner 到底是哪一轮稳定下来的
- 本地有没有足够的投票日志解释为什么是这个 committed owner


### 2.2 committed owner 不是一个推论，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前 `term / round / committed_round` 是多少
- 当前本地已经拿到多少个正式 grant
- 当前 committed owner 是谁，为什么是它
- 当前 published committed owner 和 checkpoint 是否描述的是同一轮

这说明：

- `winner` 只是候选结果
- `consensus_ownership_checkpoint` 才是稳定 ownership 状态


### 2.3 这次真正新增的是“投票日志和 committed round 状态层”

更稳的分工方式是：

- `distributed takeover orchestration`
	- 负责 winner 之后怎样把计划和 rollback 推到多个 peer
- `consensus-style ownership`
	- 负责本地 vote log、远端 grant、committed round 和 committed owner 的稳定判断
- `consensus-style takeover recovery`
	- 再往下负责更长链路的恢复、重放和跨轮接管

一句话记住：

> 上一页补的是“winner 之后怎样把 plan 编排出去”，这一页补的是“winner 什么时候才能变成正式 committed owner”。 


## 3. 这条共识式 ownership 主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active consensus registry | 当前有哪些 key 正在做 consensus-style ownership |
| `list` | recent committed history | 页面和 JSON 展示最近 committed owner 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 committed 状态 |
| `queue + thread` | 后台消费 `CONSENSUS / SWEEP` | 请求线程不阻塞在 vote log、grant 汇总和 committed publish 上 |
| `task group` | 管住一次 consensus scope 里的 local log inspect / multi-peer grant fetch / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达多 peer grant、publish committed owner 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向多个 peer 拉取当前 round grant 摘要 | 让多节点 grant gather 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取本地 vote log、写 committed checkpoint | 让共识进入正式文件主线 |
| `file_async` | 把 published committed owner 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 vote log、checkpoint、grant summary、publish JSON | 让 `round / committed_round / grant_count / committed_owner` 有正式结构 |
| `time` | 记录 consensus 启动、grant gather、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `winner / committed_owner / round / committed_round / grant_count` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `distributed takeover orchestration` 管“这轮计划怎样发出去”，`consensus-style ownership` 管“这一轮什么时候才能稳定成 committed owner”。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 consensus-style ownership 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-ownership-checkpoint.json
runtime/ownership-vote-log/<round>.json
runtime/consensus-ownership-published.json
runtime/remote-grant-summary.json
```

其中：

- `config/console.json`
	- 保存 `consensus_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `winner / committed_owner / round / committed_round / grant_count / required_grant`
- `runtime/console.log`
	- 记录 consensus 启动、grant gather、committed 判定和 publish
- `runtime/consensus-ownership-checkpoint.json`
	- 保存最近一次正式 consensus-style ownership 状态
- `runtime/ownership-vote-log/<round>.json`
	- 保存当前 round 的本地投票日志
- `runtime/consensus-ownership-published.json`
	- 保存异步发布后的 committed owner 输出
- `runtime/remote-grant-summary.json`
	- 保存这次多 peer grant 的本地 stage 文件


## 5. 先把“启动装载 consensus checkpoint -> inspect local vote log -> multi-peer grant gather -> decide committed owner -> publish committed owner”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `consensus-ownership-checkpoint.json`
2. checkpoint 里明确记录 `winner / committed_owner / round / committed_round / grant_count / required_grant`
3. `dict` 表示当前 active consensus registry
4. `list` 表示 committed / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 consensus scope
7. local vote log inspect 用一个 child 表达
8. 多个 peer grant summary 走多条 `xhttp future`
9. 统一统计当前 round 的 grant，再决定 `committed_owner`
10. 最后再用 `file_async future` 异步发布 committed owner

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`winner = node-b`、`round = 6`、`committed_round = 5`
- 本地会准备一份 round 6 的 vote log
- worker 会从 `peer-a`、`peer-b` 拉两份 grant summary
- 再把 local inspect、multi-peer grant gather 和 publish 收进同一个 consensus scope
- 最后异步发布 `runtime/consensus-ownership-published.json`

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
	uint32 iConsensusTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iRequiredGrant;
} DemoConsensusPolicy;

typedef struct
{
	xdict pActive;
	xlist pCommittedHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoConsensusPolicy tPolicy;
	uint32 iActiveConsensus;
	uint32 iTerm;
	uint32 iRound;
	uint32 iCommittedRound;
	uint32 iGrantCount;
	uint32 iRequiredGrant;
	char sWinner[32];
	char sCommittedOwner[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sGrantSummaryPath[260];
} DemoConsensusCenter;

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

static void procBuildVoteLogPath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/ownership-vote-log/%08u.json", (unsigned)iRound);
}

static bool procSaveConsensusCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sWinner,
	const char* sCommittedOwner,
	uint32 iTerm,
	uint32 iRound,
	uint32 iCommittedRound,
	uint32 iGrantCount,
	uint32 iRequiredGrant,
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
	xvoTableSetText(vRoot, "committed_owner", 0, (uint8*)((sCommittedOwner != NULL) ? sCommittedOwner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "grant_count", 0, (int32)iGrantCount);
	xvoTableSetInt(vRoot, "required_grant", 0, (int32)iRequiredGrant);
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

static xvalue procLoadConsensusCheckpoint(const char* sCheckpointPath)
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

static bool procWriteVoteLog(uint32 iRound, const char* sKey, const char* sWinner)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildVoteLogPath(sPath, sizeof(sPath), iRound);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"vote_open", 0, FALSE);
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

static bool procPrepareGrantRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/ownership-grant/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Consensus-Key", sKey);
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

static const char* procChooseConsensusPlan(
	const DemoWarmItem* pWarm,
	int iPeerCount,
	const DemoConsensusCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveConsensus > 0u) ) {
		return "defer_consensus_busy";
	}
	if ( iPeerCount <= 0 ) {
		return "defer_no_grant_peer";
	}
	return "consensus_now";
}

static bool procRunConsensusOwnership(DemoConsensusCenter* pCenter, const DemoPeerNode** arrPeer, int iPeerCount, const char* sKey)
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
	DemoStageTask tVoteInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sVoteLogPath[260];
	char sPublishJson[512];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tVoteInspect, 0, sizeof(tVoteInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveConsensusCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_opened",
		sKey,
		pCenter->sWinner,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iCommittedRound,
		pCenter->iGrantCount,
		pCenter->iRequiredGrant,
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

	procBuildVoteLogPath(sVoteLogPath, sizeof(sVoteLogPath), pCenter->iRound);
	procCopyText(tVoteInspect.sStage, sizeof(tVoteInspect.sStage), "inspect_vote_log");
	procCopyText(tVoteInspect.sPath, sizeof(tVoteInspect.sPath), sVoteLogPath);
	tVoteInspect.iDelayMs = 20u;
	tVoteInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tVoteInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPrepareGrantRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iRound,
			pCenter->tPolicy.iConsensusTimeoutMs) ) {
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

	pCenter->iGrantCount = 1u;
	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrRemoteFetch[i]);
		if ( (arrResp[i] == NULL) || (arrResp[i]->iStatusCode >= 500u) ) {
			continue;
		}

		if ( (arrResp[i]->pBody != NULL) && (arrResp[i]->iBodyLen > 0u) ) {
			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				if ( (xvoTableGetInt(vRemote, "grant_round", 0) == (int32)pCenter->iRound) &&
					(xvoTableGetInt(vRemote, "granted", 0) == 1) ) {
					pCenter->iGrantCount += 1u;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iGrantCount >= pCenter->iRequiredGrant ) {
		pCenter->iCommittedRound = pCenter->iRound;
		procCopyText(pCenter->sCommittedOwner, sizeof(pCenter->sCommittedOwner), pCenter->sWinner);
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"consensus-owned\",\n\t\"winner\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"grant_count\": %u,\n\t\"required_grant\": %u,\n\t\"committed_round\": %u\n}\n",
		pCenter->sWinner,
		pCenter->sCommittedOwner,
		(unsigned)pCenter->iGrantCount,
		(unsigned)pCenter->iRequiredGrant,
		(unsigned)pCenter->iCommittedRound);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "consensus_owned" : "consensus_incomplete",
		sKey,
		pCenter->sWinner,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iCommittedRound,
		pCenter->iGrantCount,
		pCenter->iRequiredGrant,
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
	DemoConsensusCenter tCenter;
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

	tCenter.tPolicy.iConsensusTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iRequiredGrant = 2u;
	tCenter.iTerm = 19u;
	tCenter.iRound = 6u;
	tCenter.iCommittedRound = 5u;
	tCenter.iGrantCount = 1u;
	tCenter.iRequiredGrant = 2u;
	procCopyText(tCenter.sWinner, sizeof(tCenter.sWinner), "node-b");
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-a");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-ownership-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-ownership-published.json");
	procCopyText(tCenter.sGrantSummaryPath, sizeof(tCenter.sGrantSummaryPath), "runtime/remote-grant-summary.json");

	(void)procSaveConsensusCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sWinner,
		tCenter.sCommittedOwner,
		tCenter.iTerm,
		tCenter.iRound,
		tCenter.iCommittedRound,
		tCenter.iGrantCount,
		tCenter.iRequiredGrant,
		0u);
	(void)procWriteVoteLog(tCenter.iRound, "theta", tCenter.sWinner);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 18u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 30u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadConsensusCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: winner=%s committed_owner=%s round=%d committed_round=%d grant=%d/%d\n",
			xvoTableGetText(vBoot, "winner", 0),
			xvoTableGetText(vBoot, "committed_owner", 0),
			(int)xvoTableGetInt(vBoot, "round", 0),
			(int)xvoTableGetInt(vBoot, "committed_round", 0),
			(int)xvoTableGetInt(vBoot, "grant_count", 0),
			(int)xvoTableGetInt(vBoot, "required_grant", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 65;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 4;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	iPeerCount = procSelectPeers(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs, arrSelected, DEMO_MAX_PEER);
	sPlan = procChooseConsensusPlan(NULL, iPeerCount, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "consensus_now") == 0 ) {
		tCenter.iActiveConsensus = 1u;
		if ( procRunConsensusOwnership(&tCenter, arrSelected, iPeerCount, "theta") ) {
			tCenter.iActiveConsensus = 0u;
		}
	}

	printf("checkpoint=%s published=%s winner=%s committed_owner=%s grant=%u/%u committed_round=%u\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		tCenter.sWinner,
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iGrantCount,
		(unsigned)tCenter.iRequiredGrant,
		(unsigned)tCenter.iCommittedRound);
	return 0;
}
```


## 6. 共识式 ownership 里真正要稳定下来的边界

### 6.1 `round / committed_round / grant_count / required_grant / committed_owner` 必须进入正式 checkpoint

这页真正稳定下来的，不是“多数 peer 看起来已经同意了”，而是：

- 当前 round 是多少
- 当前 committed_round 是多少
- 当前到底拿到了多少个正式 grant
- 当前 committed_owner 是谁

如果这些字段不进入正式 checkpoint，下次启动就无法判断当前 owner 到底是“候选 winner”还是“已经 committed 的稳定 owner”。


### 6.2 local vote log、multi-peer grant gather、publish committed owner 必须进入同一个 consensus scope

这页最容易写错的一点是：

- 先读本地 vote log
- 再分散地去各个 peer 拉 grant
- 最后再另起一个线程发布 committed owner

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 local vote log、multi-peer grant gather、publish 都挂进同一个 scope
- 最后只让统一统计逻辑决定这轮是否真正 committed

否则系统会出现“published owner 已经更新，但 checkpoint 还没记住 committed_round”的拆裂状态。


### 6.3 published committed owner 文件和 consensus checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published committed owner 文件已经足够回答当前 ownership 是否稳定

但更稳的边界应该是：

- `runtime/consensus-ownership-published.json`
	- 回答“这次共识式 ownership 对外发布了什么”
- `runtime/consensus-ownership-checkpoint.json`
	- 回答“这轮 grant gather 和 committed 判定当前走到了哪一步”


### 6.4 这页先停在“committed owner 已经稳定下来”，不提前把它说成“更重的 consensus-style takeover recovery 已经讲完”

这页故意把边界停在：

- local vote log 已 inspect
- 多个 peer grant 已 gather
- committed owner 已经决定
- published committed owner 已经异步写出

但它还没有回答：

- 更长链路的 recovery 和 replay 怎样基于 committed owner 继续推进
- 跨轮回放和 takeover continuation 怎样进入同一条恢复链
- consensus-style takeover recovery 怎样统一 arbitration、orchestration 和 committed replay

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要更长链路的恢复和重放，应继续补 consensus-style takeover recovery 主线
- 如果需要跨轮日志和重放游标统一管理，应继续补更重的 consensus replay 主线
- 如果需要把 committed owner 和恢复链彻底统一，应继续补 consensus recovery orchestration 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `winner / committed_owner / round / committed_round / grant_count / required_grant`
2. `POST /api/consensus`
	- 只提交 consensus 意图，让 worker 决定是否进入 vote log / grant gather / publish 主线
3. `GET /api/consensus-ownership`
	- 直接返回最近一次 consensus ownership checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 consensus 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 winner、committed owner、grant 进度、published owner 和 recent consensus history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个分布式接管编排面板](distributed-takeover-orchestration-dashboard.md)
	先对比“takeover plan 已编排”与“committed owner 已经稳定下来”到底差在哪
2. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
	回看 `winner / round / publish` 为什么在 committed owner 之后仍然是恢复链的底层边界
3. [把本地控制台服务升级成一个共识式接管恢复面板](consensus-takeover-recovery-dashboard.md)
	把 committed owner、恢复链、回放和统一收口主线正式补出来
