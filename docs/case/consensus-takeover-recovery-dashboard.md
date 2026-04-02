# 把本地控制台服务升级成一个共识式接管恢复面板

> 这页要解决的不是“committed owner 已经稳定下来，所以恢复链自然会跟上”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `winner / committed_owner / round / committed_round / grant_count / required_grant`，并且上一页已经能把 committed owner 正式稳定下来之后，又开始需要把“committed owner 确定之后 replay 和 takeover continuation 应该从哪一轮继续”“为什么 `committed_owner` 不能直接等同于 `recovery 已完成`”“本地 replay inspect、远端恢复摘要、committed recovery publish 为什么仍然要进同一个 consensus recovery scope”“如果不同 peer 对同一 committed round 提供了不同恢复摘要，本地应该怎样把 `committed owner / replay gap / recovery summary / publish` 收成同一条稳定主线”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 committed owner 之后靠“winner 节点自己再补一补”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 多节点 plan 怎样被正式编排到同一轮
- committed owner 怎样通过 vote log 和 remote grant 稳定下来
- `winner / committed_owner / committed_round` 为什么必须进入正式 checkpoint

但真实系统再往前走一步，很快又会出现一个新的问题：

- committed owner 已稳定，不代表 replay gap 和 takeover continuation 已经补完
- 不同 peer 可能都承认同一个 committed owner，但它们手里的 recovery summary 深度不同
- 如果没有正式的 consensus recovery 层，系统仍然会回到“owner 稳定了之后自己继续补恢复”
- 下次启动时只能看到 committed owner，看不清当前恢复到底卡在哪一段

如果这时还把实现停在：

- `if ( committed_owner == local ) continue replay;`
- `if ( remote summary looks newer ) overwrite local recovery state;`
- `if ( publish recovery result succeeds ) consider committed recovery done;`

很快就会出现几个典型问题：

- checkpoint 说 committed owner 是 `node-b`，但 replay cursor 还停在旧 round
- 一部分 peer 提供的是旧 recovery summary，却被当前 round 误用成恢复依据
- published recovery 文件已经更新，checkpoint 还没记住 committed recovery 到哪一步
- 下次启动时知道谁是 owner，却不知道 recovery 应该从哪一段继续

所以这页真正要补出的，不是“owner 定下来后的后处理”，而是：

> 当 committed owner 已经正式稳定下来之后，怎样把本地 replay inspect、远端恢复摘要 gather、committed recovery 判定、published recovery summary 和 checkpoint 更新做成一条可恢复、可解释、可导出的正式共识式接管恢复主线。


## 2. 为什么这次不能只靠“owner 稳了就继续恢复”

### 2.1 `consensus_owned` 只回答“谁是 committed owner”

上一页的共识式 ownership 已经很好地解决了：

- 本地 vote log 和远端 grant 怎样进入同一个 consensus scope
- 当前 committed owner 应该怎样正式写入 checkpoint
- published committed owner 文件怎样对外发布

但它不回答：

- committed owner 确定之后 replay gap 应该从哪里继续
- 远端恢复摘要哪一份才应该作为本轮恢复依据
- 当前 recovery 结果有没有真正追上 committed round


### 2.2 committed recovery 不是 owner 的副作用，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前 `committed_round / replay_from / replay_to / replay_cursor` 是多少
- 当前有哪些 remote recovery summary 真正参与了本轮 committed recovery
- 当前 committed recovery 是否已经正式 publish
- 当前 published recovery 和 checkpoint 是否描述的是同一轮恢复

这说明：

- `committed_owner` 只是恢复前提
- `consensus_takeover_recovery_checkpoint` 才是恢复状态


### 2.3 这次真正新增的是“committed owner 之后的恢复状态层”

更稳的分工方式是：

- `consensus-style ownership`
	- 负责 committed owner 怎样稳定下来
- `consensus-style takeover recovery`
	- 负责 committed owner 确定之后 replay、takeover continuation 和 publish 怎样统一收口
- `consensus replay orchestration`
	- 再往下负责更长链路的日志重放与跨轮恢复编排

一句话记住：

> 上一页补的是“谁才是 committed owner”，这一页补的是“committed owner 定下来之后，恢复链怎样正式接上并收口”。 


## 3. 这条共识式接管恢复主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active consensus recovery registry | 当前有哪些 key 正在做 consensus-style takeover recovery |
| `list` | recent recovery history | 页面和 JSON 展示最近 committed recovery 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 committed recovery |
| `queue + thread` | 后台消费 `CONSENSUS_RECOVER / SWEEP` | 请求线程不阻塞在 replay inspect、remote recovery gather 和 publish 上 |
| `task group` | 管住一次 consensus recovery scope 里的 local replay inspect / multi-peer recovery fetch / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达多 peer recovery summary、publish committed recovery 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向多个 peer 拉取当前 committed round 的 recovery 摘要 | 让远端恢复摘要进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取本地 replay checkpoint、写 committed recovery checkpoint | 让恢复进入正式文件主线 |
| `file_async` | 把 published committed recovery 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 recovery checkpoint、remote summary、publish JSON | 让 `committed_round / replay_from / replay_to / replay_cursor / committed_owner` 有正式结构 |
| `time` | 记录 consensus recovery 启动、summary gather、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `committed_owner / committed_round / replay_cursor / recovery_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `consensus-style ownership` 管“committed owner 怎么稳定”，`consensus-style takeover recovery` 管“committed owner 稳定后，恢复链怎么正式接上”。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 consensus takeover recovery 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-takeover-recovery-checkpoint.json
runtime/consensus-replay-checkpoint/<round>.json
runtime/consensus-takeover-recovery-published.json
runtime/remote-consensus-recovery-summary.json
```

其中：

- `config/console.json`
	- 保存 `recovery_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / replay_from / replay_to / replay_cursor / recovery_state`
- `runtime/console.log`
	- 记录 recovery 启动、remote recovery gather、replay 推进和 publish
- `runtime/consensus-takeover-recovery-checkpoint.json`
	- 保存最近一次正式 consensus takeover recovery 状态
- `runtime/consensus-replay-checkpoint/<round>.json`
	- 保存当前 committed round 的本地 replay checkpoint
- `runtime/consensus-takeover-recovery-published.json`
	- 保存异步发布后的 committed recovery 输出
- `runtime/remote-consensus-recovery-summary.json`
	- 保存这次多 peer recovery summary 的本地 stage 文件


## 5. 先把“启动装载 committed recovery checkpoint -> inspect local replay -> multi-peer recovery gather -> advance replay cursor -> publish committed recovery”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `consensus-takeover-recovery-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / replay_from / replay_to / replay_cursor / recovery_state`
3. `dict` 表示当前 active consensus recovery registry
4. `list` 表示 recovery / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 consensus recovery scope
7. local replay checkpoint inspect 用一个 child 表达
8. 多个 peer recovery summary 走多条 `xhttp future`
9. 统一推进 `replay_cursor` 到当前 committed round 的目标
10. 最后再用 `file_async future` 异步发布 committed recovery 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 6`
- 本地 replay checkpoint 还停在 `replay_cursor = 52`
- worker 会从 `peer-a`、`peer-b` 拉两份 recovery summary
- 再把 local replay inspect、multi-peer recovery gather 和 publish 收进同一个 recovery scope
- 最后异步发布 `runtime/consensus-takeover-recovery-published.json`

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
	uint32 iRecoveryTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoRecoveryPolicy;

typedef struct
{
	xdict pActive;
	xlist pRecoveryHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoRecoveryPolicy tPolicy;
	uint32 iActiveRecovery;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iReplayFrom;
	uint32 iReplayTo;
	uint32 iReplayCursor;
	char sCommittedOwner[32];
	char sRecoveryState[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoRecoveryCenter;

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

static void procBuildReplayCheckpointPath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-replay-checkpoint/%08u.json", (unsigned)iRound);
}

static bool procSaveConsensusRecoveryCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iReplayFrom,
	uint32 iReplayTo,
	uint32 iReplayCursor,
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
	xvoTableSetText(vRoot, "committed_owner", 0, (uint8*)((sCommittedOwner != NULL) ? sCommittedOwner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "replay_from", 0, (int32)iReplayFrom);
	xvoTableSetInt(vRoot, "replay_to", 0, (int32)iReplayTo);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
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

static xvalue procLoadConsensusRecoveryCheckpoint(const char* sCheckpointPath)
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

static bool procWriteReplayCheckpoint(uint32 iRound, const char* sKey, uint32 iReplayCursor)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildReplayCheckpointPath(sPath, sizeof(sPath), iRound);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"replay_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
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

static bool procPrepareRecoveryRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-recovery/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Consensus-Recovery-Key", sKey);
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

static const char* procChooseRecoveryPlan(
	const DemoWarmItem* pWarm,
	int iPeerCount,
	const DemoRecoveryCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveRecovery > 0u) ) {
		return "defer_recovery_busy";
	}
	if ( iPeerCount <= 0 ) {
		return "defer_no_recovery_peer";
	}
	return "recover_now";
}

static bool procRunConsensusRecovery(DemoRecoveryCenter* pCenter, const DemoPeerNode** arrPeer, int iPeerCount, const char* sKey)
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
	DemoStageTask tReplayInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sReplayPath[260];
	char sPublishJson[512];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tReplayInspect, 0, sizeof(tReplayInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveConsensusRecoveryCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_recovery_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
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

	procBuildReplayCheckpointPath(sReplayPath, sizeof(sReplayPath), pCenter->iCommittedRound);
	procCopyText(tReplayInspect.sStage, sizeof(tReplayInspect.sStage), "inspect_local_replay");
	procCopyText(tReplayInspect.sPath, sizeof(tReplayInspect.sPath), sReplayPath);
	tReplayInspect.iDelayMs = 20u;
	tReplayInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tReplayInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPrepareRecoveryRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			pCenter->tPolicy.iRecoveryTimeoutMs) ) {
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

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrRemoteFetch[i]);
		if ( (arrResp[i] == NULL) || (arrResp[i]->iStatusCode >= 500u) ) {
			continue;
		}

		if ( (arrResp[i]->pBody != NULL) && (arrResp[i]->iBodyLen > 0u) ) {
			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				int32 iRemoteCursor = xvoTableGetInt(vRemote, "remote_replay_cursor", 0);
				if ( iRemoteCursor > (int32)pCenter->iReplayCursor ) {
					pCenter->iReplayCursor = (uint32)iRemoteCursor;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iReplayCursor < pCenter->iReplayTo ) {
		pCenter->iReplayCursor = pCenter->iReplayTo;
	}
	procCopyText(pCenter->sRecoveryState, sizeof(pCenter->sRecoveryState), "committed_recovered");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"consensus-recovered\",\n\t\"committed_owner\": \"%s\",\n\t\"committed_round\": %u,\n\t\"replay_cursor\": %u,\n\t\"replay_to\": %u,\n\t\"recovery_state\": \"%s\"\n}\n",
		pCenter->sCommittedOwner,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iReplayCursor,
		(unsigned)pCenter->iReplayTo,
		pCenter->sRecoveryState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusRecoveryCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "consensus_recovered" : "consensus_recovery_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
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
	DemoRecoveryCenter tCenter;
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

	tCenter.tPolicy.iRecoveryTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 19u;
	tCenter.iCommittedRound = 6u;
	tCenter.iReplayFrom = 53u;
	tCenter.iReplayTo = 58u;
	tCenter.iReplayCursor = 52u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sRecoveryState, sizeof(tCenter.sRecoveryState), "recovery_open");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-takeover-recovery-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-takeover-recovery-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-recovery-summary.json");

	(void)procSaveConsensusRecoveryCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sCommittedOwner,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iReplayFrom,
		tCenter.iReplayTo,
		tCenter.iReplayCursor,
		0u);
	(void)procWriteReplayCheckpoint(tCenter.iCommittedRound, "theta", tCenter.iReplayCursor);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 18u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 26u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadConsensusRecoveryCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: committed_owner=%s committed_round=%d replay=%d->%d cursor=%d state=%s\n",
			xvoTableGetText(vBoot, "committed_owner", 0),
			(int)xvoTableGetInt(vBoot, "committed_round", 0),
			(int)xvoTableGetInt(vBoot, "replay_from", 0),
			(int)xvoTableGetInt(vBoot, "replay_to", 0),
			(int)xvoTableGetInt(vBoot, "replay_cursor", 0),
			xvoTableGetText(vBoot, "state", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 60;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 3;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	iPeerCount = procSelectPeers(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs, arrSelected, DEMO_MAX_PEER);
	sPlan = procChooseRecoveryPlan(NULL, iPeerCount, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "recover_now") == 0 ) {
		tCenter.iActiveRecovery = 1u;
		if ( procRunConsensusRecovery(&tCenter, arrSelected, iPeerCount, "theta") ) {
			tCenter.iActiveRecovery = 0u;
		}
	}

	printf("checkpoint=%s published=%s committed_owner=%s committed_round=%u replay_cursor=%u replay_to=%u state=%s\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iReplayTo,
		tCenter.sRecoveryState);
	return 0;
}
```


## 6. 共识式接管恢复里真正要稳定下来的边界

### 6.1 `committed_round / replay_from / replay_to / replay_cursor / committed_owner` 必须进入正式 checkpoint

这页真正稳定下来的，不是“committed owner 看起来已经在继续工作了”，而是：

- 当前 committed_round 是多少
- 当前 replay gap 从哪里开始、到哪里结束
- 当前 replay_cursor 已经推进到了哪里
- 当前 committed_owner 是谁

如果这些字段不进入正式 checkpoint，下次启动就无法判断当前恢复应该从哪一段继续。


### 6.2 local replay inspect、multi-peer recovery gather、publish committed recovery 必须进入同一个 recovery scope

这页最容易写错的一点是：

- 先读本地 replay checkpoint
- 再分散地去各个 peer 拉 recovery summary
- 最后再另起一个线程发布 recovery 结果

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 local replay inspect、multi-peer recovery gather、publish 都挂进同一个 scope
- 最后只让统一推进逻辑决定这轮 committed recovery 是否真正完成

否则系统会出现“published recovery 已更新，但 checkpoint 还没推进 replay_cursor”的拆裂状态。


### 6.3 published committed recovery 文件和 consensus recovery checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published committed recovery 文件已经足够回答当前恢复是否稳定

但更稳的边界应该是：

- `runtime/consensus-takeover-recovery-published.json`
	- 回答“这次 committed recovery 对外发布了什么”
- `runtime/consensus-takeover-recovery-checkpoint.json`
	- 回答“这轮 replay inspect 和 recovery gather 当前走到了哪一步”


### 6.4 这页先停在“committed owner 的恢复链已正式接上”，不提前把它说成“更重的 consensus replay orchestration 已经讲完”

这页故意把边界停在：

- local replay checkpoint 已 inspect
- 多个 peer recovery summary 已 gather
- replay_cursor 已经推进到目标
- published committed recovery 已经异步写出

但它还没有回答：

- 更长链路的 replay orchestration 怎样跨轮统一推进
- consensus replay log 怎样和 takeover continuation 彻底合并
- consensus recovery orchestration 怎样把 committed owner、replay、snapshot 和 publish 全部统一到一条更重主线

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要更长链路的 replay orchestration，应继续补 consensus replay 主线
- 如果需要把 snapshot / compaction continuation 并入同一轮恢复，应继续补 consensus recovery orchestration 主线
- 如果需要跨轮日志和恢复摘要统一回放，应继续补更重的 committed replay orchestration 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / replay_from / replay_to / replay_cursor / recovery_state`
2. `POST /api/consensus-recover`
	- 只提交 recovery 意图，让 worker 决定是否进入 replay inspect / recovery gather / publish 主线
3. `GET /api/consensus-recovery`
	- 直接返回最近一次 consensus takeover recovery checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 committed recovery 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 committed owner、replay 进度、published recovery 和 recent recovery history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识式 ownership 面板](consensus-style-ownership-dashboard.md)
	先对比“committed owner 已稳定”与“committed owner 的恢复链已正式接上”到底差在哪
2. [把本地控制台服务升级成一个分布式接管编排面板](distributed-takeover-orchestration-dashboard.md)
	回看 `winner / committed_owner / publish` 为什么在 committed recovery 之后仍然是恢复链的底层边界
3. [把本地控制台服务升级成一个共识恢复编排面板](consensus-recovery-orchestration-dashboard.md)
	把 committed owner、replay、snapshot、publish 和统一收口主线正式补出来
