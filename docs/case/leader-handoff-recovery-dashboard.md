# 把本地控制台服务升级成一个领导切换恢复面板

> 这页要解决的不是“lease 过期后把 `owner` 改成自己”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `lease_expires_at / owner / role / term / snapshot_index / truncated_to`，并且上一页已经能把 takeover 正式收口之后，又开始需要把“新 owner takeover 之后怎样补 replay gap”“compaction 开到一半时怎样继续接管”“为什么 `takeover_done` 不能直接等同于 `leader 已恢复完成`”“如果远端 leader 还留着更完整的 handoff checkpoint，新 owner 应该怎样把 `remote fetch / local replay / compaction resume / publish` 收进同一个 recovery scope”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 failover 之后靠人工判断“现在大概已经恢复得差不多了”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- lease 过期之后怎样正式探测 peer
- takeover 怎样稳定回答 `owner / role / term / lease_expires_at`
- replay gap 和 compaction 边界为什么不能提前混在 failover 页面里

但真实系统再往前走一步，很快又会出现一个新的问题：

- 新 owner takeover 成功，不代表它已经补齐自己的日志和状态
- 远端 peer 可能还持有更完整的 handoff checkpoint
- 本地 `replay_from / replay_to / replay_cursor` 可能还停在 takeover 前
- compaction 可能已经写出一半 snapshot，但 `truncated_to` 还没正式推进

如果这时还把实现停在：

- `if ( takeover_done ) role = leader_active;`
- `if ( remote looks good ) just fetch once;`
- `if ( replay gap exists ) fix it later;`

很快就会出现几个典型问题：

- dashboard 上看到 owner 已切过来，但服务实际还没补完本地 replay gap
- compaction 半途接管后没有正式 checkpoint，下次启动不知道该继续哪一段
- recovery worker 和请求线程都在争谁来决定“恢复是不是已经完成”
- 发布出去的 handoff summary 和本地 checkpoint 各说各话

所以这页真正要补出的，不是“takeover 之后顺手补几步”，而是：

> 当新的 owner 已经正式接管 lease 之后，怎样把远端 handoff 拉取、本地 replay 补齐、compaction resume 和 publish 做成一条可恢复、可解释、可导出的正式领导切换恢复主线。


## 2. 为什么这次不能只靠“takeover 成功后继续跑”

### 2.1 `takeover_done` 只回答“ownership 已切过来”

上一页的租约故障切换已经很好地解决了：

- lease 是否真的过期
- 哪个 peer 还健康
- 当前 node 是否应该正式 takeover

但它不回答：

- 新 owner 自己的 replay gap 还剩多少
- 之前未完成的 compaction 应该从哪里继续
- 哪一份 remote handoff summary 才能作为恢复输入


### 2.2 `leader_active` 不是“服务已经恢复完毕”的同义词

到了这一层，系统真正要稳定回答的是：

- 当前 `replay_from / replay_to / replay_cursor` 是多少
- 当前 `snapshot_index / truncated_to` 有没有在 handoff 之后继续推进
- 当前远端 handoff 摘要有没有被正式 join 进本次 recovery scope
- recovery 结束之后，对外发布的文件到底是哪一份

这说明：

- `owner / role` 只是 ownership 状态
- `leader recovery checkpoint` 才是恢复状态


### 2.3 这次真正新增的是“handoff recovery 状态层”

更稳的分工方式是：

- `lease failover`
	- 负责判断 ownership 是否应该切换
- `leader handoff recovery`
	- 负责把 remote handoff、local replay、compaction resume 收成一条恢复主线
- `compaction takeover`
	- 再往下负责更重的 compaction continuation 与 ownership 接管细节

一句话记住：

> 上一页补的是“ownership 怎么切过来”，这一页补的是“ownership 切过来之后，新 leader 怎样把自己的恢复链正式接上”。


## 3. 这条领导切换恢复主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active handoff recovery registry | 当前有哪些 key 正在做 leader recovery |
| `list` | recent recovery history | 页面和 JSON 展示最近 handoff 恢复结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式恢复 |
| `queue + thread` | 后台消费 `RECOVER / SWEEP` | 请求线程不阻塞在 replay 和 compaction 上 |
| `task group` | 管住一次 handoff recovery scope 里的 replay / remote fetch / compaction / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote fetch、local replay、compaction resume、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 handoff summary | 让 remote handoff 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取 replay gap、写 publish 文件 | 让恢复进入正式文件主线 |
| `file_async` | 把 recovery summary 异步发布成对外可读的状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 handoff checkpoint、remote summary 与 publish JSON | 让 `replay_cursor / snapshot_index / truncated_to` 有正式结构 |
| `time` | 记录 recovery 启动、join、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `owner / role / replay gap / compaction resume` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `lease failover` 管 ownership 是否切换，`leader handoff recovery` 管切换之后怎样把恢复链正式接起来。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成领导切换恢复语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/leader-handoff-checkpoint.json
runtime/replay-gap/<index>.json
runtime/leader-handoff-published.json
runtime/remote-handoff-stage.json
```

其中：

- `config/console.json`
	- 保存 `handoff_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `owner / role / replay_from / replay_to / replay_cursor / snapshot_index / truncated_to`
- `runtime/console.log`
	- 记录 recovery 启动、remote join、replay 补齐、compaction resume 和 publish
- `runtime/leader-handoff-checkpoint.json`
	- 保存最近一次正式 handoff recovery 状态
- `runtime/replay-gap/<index>.json`
	- 保存 takeover 后需要继续补齐的 replay gap
- `runtime/leader-handoff-published.json`
	- 保存异步发布后的 recovery 输出
- `runtime/remote-handoff-stage.json`
	- 保存这次 remote handoff 的 stage 摘要


## 5. 先把“启动装载 handoff checkpoint -> 选择 peer -> remote fetch -> local replay -> compaction resume -> publish”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `leader-handoff-checkpoint.json`
2. checkpoint 里明确记录 `owner / role / replay_from / replay_to / replay_cursor / snapshot_index / truncated_to`
3. `dict` 表示当前 active recovery registry
4. `list` 表示 recovery / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 recovery scope
7. remote handoff summary 走 `xhttp future`
8. local replay 和 compaction resume 先各用一个 child thread 表达
9. publish 仍然走 `file_async future`
10. join 完成之后再统一把 checkpoint 更新成 `leader_recovered` 或 `leader_recovery_incomplete`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`owner = node-b`、`role = leader_recovering`
- 本地还留着 `49 -> 52` 这段 replay gap
- worker 会先从 `peer-a` 拉一份 handoff summary
- 再把 replay 和 compaction resume 收进同一个 recovery scope
- 最后异步发布 `runtime/leader-handoff-published.json`

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
	uint32 iHandoffTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoHandoffPolicy;

typedef struct
{
	xdict pActive;
	xlist pRecoveryHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoHandoffPolicy tPolicy;
	uint32 iActiveRecovery;
	uint32 iTerm;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	uint32 iReplayFrom;
	uint32 iReplayTo;
	uint32 iReplayCursor;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	char sOwner[32];
	char sRole[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteStagePath[260];
} DemoLeaderCenter;

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

static void procBuildReplayPath(char* sDst, size_t iCap, uint32 iLogIndex)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/replay-gap/%08u.json", (unsigned)iLogIndex);
}

static bool procSaveLeaderCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sOwner,
	const char* sRole,
	uint32 iTerm,
	uint32 iCommitIndex,
	uint32 iLastApplied,
	uint32 iReplayFrom,
	uint32 iReplayTo,
	uint32 iReplayCursor,
	uint32 iSnapshotIndex,
	uint32 iTruncatedTo,
	uint32 iRecoveryDone)
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
	xvoTableSetText(vRoot, "owner", 0, (uint8*)((sOwner != NULL) ? sOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "role", 0, (uint8*)((sRole != NULL) ? sRole : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)iCommitIndex);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)iLastApplied);
	xvoTableSetInt(vRoot, "replay_from", 0, (int32)iReplayFrom);
	xvoTableSetInt(vRoot, "replay_to", 0, (int32)iReplayTo);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "truncated_to", 0, (int32)iTruncatedTo);
	xvoTableSetInt(vRoot, "recovery_done", 0, (int32)iRecoveryDone);
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

static xvalue procLoadLeaderCheckpoint(const char* sCheckpointPath)
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

static bool procWriteReplayGap(uint32 iLogIndex, const char* sKey)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildReplayPath(sPath, sizeof(sPath), iLogIndex);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"replay_gap", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "log_index", 0, (int32)iLogIndex);
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

static int32 procLocalStageTask(ptr pArg, xfuture_result* pOut)
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

static bool procPrepareLeaderRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[192];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/leader-handoff/%s", sKey);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Handoff-Key", sKey);
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

static const char* procChooseLeaderRecoveryPlan(
	const DemoWarmItem* pWarm,
	const DemoPeerNode* pPeer,
	const DemoLeaderCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveRecovery > 0u) ) {
		return "defer_recovery_busy";
	}
	if ( pPeer == NULL ) {
		return "defer_no_healthy_peer";
	}
	if ( pCenter->iLastApplied >= pCenter->iCommitIndex ) {
		return "already_recovered";
	}
	return "leader_recover_now";
}

static bool procRunLeaderRecovery(DemoLeaderCenter* pCenter, const DemoPeerNode* pPeer, const char* sKey)
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
	DemoStageTask tReplay;
	DemoStageTask tResume;
	xvalue vRemote = NULL;
	char sURL[512];
	char sHostHeader[320];
	const char* sPublishJson =
		"{\n"
		"\t\"state\": \"leader-recovered\",\n"
		"\t\"publish\": \"handoff\"\n"
		"}\n";
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(&tReq, 0, sizeof(tReq));
	memset(&tReplay, 0, sizeof(tReplay));
	memset(&tResume, 0, sizeof(tResume));

	if ( (pCenter == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveLeaderCheckpoint(
		pCenter->sCheckpointPath,
		"leader_recovery_opened",
		sKey,
		pCenter->sOwner,
		pCenter->sRole,
		pCenter->iTerm,
		pCenter->iCommitIndex,
		pCenter->iLastApplied,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
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

	procCopyText(tReplay.sStage, sizeof(tReplay.sStage), "replay_gap");
	procBuildReplayPath(tReplay.sPath, sizeof(tReplay.sPath), pCenter->iReplayFrom);
	tReplay.iDelayMs = 30u;
	tReplay.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tReplay, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tResume.sStage, sizeof(tResume.sStage), "resume_compaction");
	procCopyText(tResume.sPath, sizeof(tResume.sPath), pCenter->sCheckpointPath);
	tResume.iDelayMs = 50u;
	tResume.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalStageTask, &tResume, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	if ( !procPrepareLeaderRequest(
		&tReq,
		sURL,
		sizeof(sURL),
		sHostHeader,
		sizeof(sHostHeader),
		pPeer,
		sKey,
		pCenter->tPolicy.iHandoffTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pRemoteFetch = (xfuture*)xrtHttpExecuteAsync(pEngine, &tReq);
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

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
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
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pResp = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)pRemoteFetch);
	if ( (pResp == NULL) || (pResp->iStatusCode >= 500u) ) {
		goto cleanup;
	}

	if ( (pResp->pBody != NULL) && (pResp->iBodyLen > 0u) ) {
		vRemote = xrtParseJSON((str)pResp->pBody, pResp->iBodyLen);
		if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
			int32 iRemoteApplied = xvoTableGetInt(vRemote, "remote_last_applied", 0);
			if ( iRemoteApplied > (int32)pCenter->iLastApplied ) {
				pCenter->iLastApplied = (uint32)iRemoteApplied;
			}
		}
	}

	pCenter->iReplayCursor = pCenter->iReplayTo;
	pCenter->iLastApplied = pCenter->iCommitIndex;
	pCenter->iSnapshotIndex = 46u;
	pCenter->iTruncatedTo = 46u;
	procCopyText(pCenter->sOwner, sizeof(pCenter->sOwner), "node-b");
	procCopyText(pCenter->sRole, sizeof(pCenter->sRole), "leader_active");
	bFinished = true;

cleanup:
	(void)procSaveLeaderCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "leader_recovered" : "leader_recovery_incomplete",
		sKey,
		pCenter->sOwner,
		pCenter->sRole,
		pCenter->iTerm,
		pCenter->iCommitIndex,
		pCenter->iLastApplied,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		bFinished ? 1u : 0u);

	if ( vRemote != NULL ) {
		xvoUnref(vRemote);
	}
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
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}
	xrtHttpRequestUnit(&tReq);
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}

	return bFinished;
}

int main(void)
{
	DemoLeaderCenter tCenter;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const DemoPeerNode* pPeer = NULL;
	const char* sPlan = NULL;
	uint32 iIndex = 0u;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iHandoffTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 18u;
	tCenter.iCommitIndex = 52u;
	tCenter.iLastApplied = 48u;
	tCenter.iReplayFrom = 49u;
	tCenter.iReplayTo = 52u;
	tCenter.iReplayCursor = 48u;
	tCenter.iSnapshotIndex = 44u;
	tCenter.iTruncatedTo = 44u;
	procCopyText(tCenter.sOwner, sizeof(tCenter.sOwner), "node-b");
	procCopyText(tCenter.sRole, sizeof(tCenter.sRole), "leader_recovering");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/leader-handoff-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/leader-handoff-published.json");
	procCopyText(tCenter.sRemoteStagePath, sizeof(tCenter.sRemoteStagePath), "runtime/remote-handoff-stage.json");

	(void)procSaveLeaderCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sOwner,
		tCenter.sRole,
		tCenter.iTerm,
		tCenter.iCommitIndex,
		tCenter.iLastApplied,
		tCenter.iReplayFrom,
		tCenter.iReplayTo,
		tCenter.iReplayCursor,
		tCenter.iSnapshotIndex,
		tCenter.iTruncatedTo,
		0u);

	for ( iIndex = tCenter.iReplayFrom; iIndex <= tCenter.iReplayTo; iIndex++ ) {
		(void)procWriteReplayGap(iIndex, "theta");
	}

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 28u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 66u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadLeaderCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: owner=%s role=%s replay=%d->%d cursor=%d snapshot=%d truncated=%d\n",
			xvoTableGetText(vBoot, "owner", 0),
			xvoTableGetText(vBoot, "role", 0),
			(int)xvoTableGetInt(vBoot, "replay_from", 0),
			(int)xvoTableGetInt(vBoot, "replay_to", 0),
			(int)xvoTableGetInt(vBoot, "replay_cursor", 0),
			(int)xvoTableGetInt(vBoot, "snapshot_index", 0),
			(int)xvoTableGetInt(vBoot, "truncated_to", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 90;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 7;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	pPeer = procChoosePeer(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs);
	sPlan = procChooseLeaderRecoveryPlan(NULL, pPeer, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( (strcmp(sPlan, "leader_recover_now") == 0) && (pPeer != NULL) ) {
		tCenter.iActiveRecovery = 1u;
		if ( procRunLeaderRecovery(&tCenter, pPeer, "theta") ) {
			tCenter.iActiveRecovery = 0u;
		}
	}

	printf("checkpoint=%s published=%s replay_cursor=%u snapshot_index=%u truncated_to=%u role=%s\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iTruncatedTo,
		tCenter.sRole);
	return 0;
}
```


## 6. 领导切换恢复里真正要稳定下来的边界

### 6.1 `replay_cursor / snapshot_index / truncated_to` 必须进入正式 handoff checkpoint

这页真正稳定下来的，不是“owner 看起来已经能继续对外服务”，而是：

- replay gap 当前已经补到了哪里
- compaction resume 当前推进到了哪里
- publish 是否已经在正式 recovery scope 里完成

如果这些字段不进入正式 checkpoint，下次启动就无法稳定继续收尾。


### 6.2 remote handoff、local replay、compaction resume 必须进入同一个 recovery scope

这页最容易写错的一点是：

- 先随手拉一个 remote summary
- 再在别的线程里各自补 replay 和 compaction

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 remote handoff、local replay、compaction resume、publish 都挂进这个 scope
- 最后只让 join 结果决定这次 recovery 是否正式完成

否则系统会在不同 child 上各自判断“我这一步已经好了”，却没人真正回答“整条恢复链是否已经收口”。


### 6.3 published handoff 文件和 leader checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published handoff 文件已经足够回答当前 leader recovery 状态

但更稳的边界应该是：

- `runtime/leader-handoff-published.json`
	- 回答“这次恢复对外发布了什么”
- `runtime/leader-handoff-checkpoint.json`
	- 回答“replay / compaction / publish 当前走到了哪一步”


### 6.4 这页先停在“新 leader 恢复链已接上”，不提前把它说成“更重的 compaction takeover 已经讲完”

这页故意把边界停在：

- remote handoff 已经 join
- replay gap 已经补到 `replay_to`
- compaction resume 已经推进了 `snapshot_index / truncated_to`
- publish summary 已经异步写出

但它还没有回答：

- compaction 半途断电后更细的 cursor continuation 应该怎样接管
- 多个 contender 竞争接管 compaction 时怎样仲裁
- leader handoff 和 ownership arbitration 怎样统一成更重的一条主线

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要更细的 compaction cursor continuation，应继续补 compaction takeover 主线
- 如果需要多 contender 竞争接手 unfinished work，应继续补 ownership arbitration 主线
- 如果需要更正式的 peer 健康评分和多阶段回退，应继续补 distributed recovery orchestration 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `owner / role / replay_from / replay_to / replay_cursor / snapshot_index / truncated_to`
2. `POST /api/leader-recover`
	- 只提交 handoff recovery 意图，让 worker 决定是否进入 remote fetch / replay / compaction 主线
3. `GET /api/handoff`
	- 直接返回最近一次 leader handoff checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 recovery 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 owner、replay gap、compaction resume、published summary 和 recent recovery history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个租约故障切换面板](lease-failover-dashboard.md)
	先对比“ownership 已切换”与“新 leader 的恢复链已经正式接上”到底差在哪
2. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	回看 `snapshot_index / truncated_to` 为什么在 handoff 之后仍然是恢复链的底层边界
3. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
	把未完成 compaction 的 cursor continuation、接管仲裁和恢复收口主线正式补出来
