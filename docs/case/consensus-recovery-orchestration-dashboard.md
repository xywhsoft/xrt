# 把本地控制台服务升级成一个共识恢复编排面板

> 这页要解决的不是“committed owner 已经稳定，takeover recovery 也已经把 replay 补到目标 round，所以系统自然就完整了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / replay_from / replay_to / replay_cursor / snapshot_index / publish_index / recovery_state`，并且上一页已经能把 committed owner 之后的恢复链正式接上之后，又开始需要把“为什么一次 takeover recovery 还不等于整条恢复编排已经收口”“为什么 `replay inspect / snapshot inspect / multi-peer summary gather / publish` 仍然要进同一个 orchestration scope”“如果不同 peer 对同一 committed round 提供了不同的 replay depth 和 snapshot depth，本地应该怎样把 `committed_owner / replay / snapshot / publish / checkpoint` 收成同一条稳定主线”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 takeover recovery 之后靠“再补一个 snapshot 线程”和“再补一个 publish 回调”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- committed owner 怎样稳定下来
- takeover recovery 怎样把 replay gap 和 remote recovery summary 收进同一个 scope
- published committed recovery 和 checkpoint 为什么不能写成一回事

但真实系统再往前走一步，很快又会出现一个新的问题：

- takeover recovery 已经结束，不代表 snapshot inspect 和 publish index 已经追平
- 不同 peer 可能都承认同一个 committed owner，但它们手里的 replay depth、snapshot depth 和 published index 并不一致
- 如果没有正式的 recovery orchestration 层，系统仍然会回到“recover 完一段 replay，再零散补 snapshot 和 publish”
- 下次启动时只能看到某次 recovery 成功过，看不清当前编排到底卡在 replay、snapshot 还是 publish

如果这时还把实现停在：

- `if ( replay_cursor >= replay_to ) write publish file;`
- `if ( remote snapshot looks newer ) overwrite local snapshot state;`
- `if ( publish succeeds ) consider recovery done;`

很快就会出现几个典型问题：

- checkpoint 说 recovery 已完成，但 `snapshot_index` 仍然落后于 `committed_round - 1`
- published recovery 文件已经更新，checkpoint 里的 `publish_index` 还停在旧值
- 本地 replay 已经追平，但远端 peer 的更深 snapshot summary 没有进入本轮正式判断
- 下次启动时知道 committed owner 和 replay cursor，却不知道当前 orchestration 应该从哪一层继续

所以这页真正要补出的，不是“takeover recovery 之后再补一点状态”，而是：

> 当 committed owner 和 takeover recovery 都已经稳定之后，怎样把本地 replay inspect、snapshot inspect、多 peer summary gather、publish 和 checkpoint 更新统一进一条可恢复、可解释、可导出的正式共识恢复编排主线。

## 2. 为什么这次不能只靠“takeover recovery 之后继续补 snapshot”

### 2.1 takeover recovery 只回答“这一轮 replay gap 怎么补齐”

上一页的共识式接管恢复已经很好地解决了：

- committed owner 确定之后 replay gap 怎样推进
- 多个 peer recovery summary 怎样进入同一个 recovery scope
- published committed recovery 文件怎样异步写出

但它不回答：

- snapshot state 应该以哪一轮为准
- `publish_index` 应该怎样和 `replay_cursor`、`snapshot_index` 一起推进
- recovery 完成之后下一轮 orchestration 应该从哪里继续

### 2.2 recovery orchestration 不是副作用，而是正式状态层

到了这一层，系统真正要稳定回答的是：

- 当前 `replay_from / replay_to / replay_cursor` 到了哪里
- 当前 `snapshot_index` 是否已经追上当前 committed round 需要的边界
- 当前 `publish_index` 是否和 checkpoint 描述的是同一轮恢复
- 当前 remote summaries 到底有没有正式进入本轮 orchestration 判断

这说明：

- `committed_owner` 是恢复前提
- `takeover recovery` 是恢复阶段
- `consensus recovery orchestration checkpoint` 才是恢复编排状态

### 2.3 这次真正新增的是“replay + snapshot + publish”的统一编排层

更稳的分工方式是：

- `consensus-style ownership`
	- 负责 committed owner 怎样稳定下来
- `consensus-style takeover recovery`
	- 负责 committed owner 之后 replay 和 recovery summary 怎样接上
- `consensus recovery orchestration`
	- 负责 replay、snapshot、publish 怎样进入同一个正式 orchestration scope

一句话记住：

> 上一页补的是“恢复链已经正式接上”，这一页补的是“恢复链怎样继续编排成一条完整、可恢复、可发布的正式主线”。

## 3. 这条共识恢复编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active orchestration registry | 当前有哪些 key 正在做共识恢复编排 |
| `list` | recent orchestration history | 页面和 JSON 展示最近正式编排结果 |
| `list` | recent deferred history | 页面和 JSON 展示为什么这次没有进入正式 orchestration |
| `queue + thread` | 后台消费 `ORCHESTRATE / SWEEP` | 请求线程不阻塞在 replay、snapshot、publish 上 |
| `task group` | 管住一次 orchestration scope 里的 local replay inspect / local snapshot inspect / multi-peer fetch / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达多 peer summary、publish 和 child stage 的完成态 | 统一 join 与失败边界 |
| `xurl + xhttp` | 向多个 peer 拉取当前 committed round 的 orchestration summary | 让远端 replay / snapshot / publish 深度进入正式 future 主线 |
| `file + path` | 装载 checkpoint、本地 replay checkpoint 和 snapshot state | 让编排进入正式文件主线 |
| `file_async` | 把 committed orchestration 结果异步发布成对外可读状态文件 | 发布仍然是正式 future |
| `value + json` | 构造和解析 checkpoint、summary、publish JSON | 让 `replay_cursor / snapshot_index / publish_index` 有正式结构 |
| `time` | 记录 orchestration 启动、summary gather、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `committed_owner / replay / snapshot / publish / orchestration_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `takeover recovery` 管“replay 怎样接上”，`consensus recovery orchestration` 管“replay、snapshot 和 publish 怎样正式统一收口”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 recovery orchestration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-recovery-orchestration-checkpoint.json
runtime/consensus-replay-checkpoint/<round>.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-recovery-orchestration-published.json
runtime/remote-consensus-orchestration-summary.json
```

其中：

- `config/console.json`
	- 保存 `recovery_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / replay_cursor / snapshot_index / publish_index / orchestration_state`
- `runtime/console.log`
	- 记录 orchestration 启动、summary gather、snapshot 推进和 publish
- `runtime/consensus-recovery-orchestration-checkpoint.json`
	- 保存最近一次正式共识恢复编排状态
- `runtime/consensus-replay-checkpoint/<round>.json`
	- 保存当前 committed round 的本地 replay checkpoint
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 committed round 的本地 snapshot state
- `runtime/consensus-recovery-orchestration-published.json`
	- 保存异步发布后的 committed orchestration 输出
- `runtime/remote-consensus-orchestration-summary.json`
	- 保存这次多 peer orchestration summary 的本地 stage 文件

## 5. 先把“启动装载 orchestration checkpoint -> inspect replay -> inspect snapshot -> multi-peer summary gather -> advance publish index -> publish committed orchestration”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-recovery-orchestration-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / replay_from / replay_to / replay_cursor / snapshot_index / publish_index / orchestration_state`
3. `dict` 表示当前 active orchestration registry
4. `list` 表示 orchestration / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 orchestration scope
7. local replay checkpoint inspect 用一个 child 表达
8. local snapshot state inspect 再用一个 child 表达
9. 多个 peer orchestration summary 走多条 `xhttp future`
10. 统一推进 `replay_cursor / snapshot_index / publish_index`
11. 最后再用 `file_async future` 异步发布 committed orchestration 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 replay checkpoint 还停在 `replay_cursor = 61`
- 本地 snapshot state 还停在 `snapshot_index = 60`
- worker 会从 `peer-a`、`peer-b` 拉两份 orchestration summary
- 再把 local replay inspect、local snapshot inspect、multi-peer summary gather 和 publish 收进同一个 orchestration scope
- 最后异步发布 `runtime/consensus-recovery-orchestration-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_MAX_CHILD 10
#define DEMO_MAX_PEER 3

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
	char sStage[64];
	char sPath[260];
	uint32 iDelayMs;
	int iStatus;
} DemoStageTask;

typedef struct
{
	xdict pActive;
	xlist pHistory;
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
	uint32 iSnapshotIndex;
	uint32 iPublishIndex;
	char sCommittedOwner[32];
	char sRecoveryState[40];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoRecoveryCenter;


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

static void procBuildSnapshotStatePath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-state/%08u.json", (unsigned)iRound);
}

static bool procSaveConsensusRecoveryOrchestrationCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iReplayFrom,
	uint32 iReplayTo,
	uint32 iReplayCursor,
	uint32 iSnapshotIndex,
	uint32 iPublishIndex,
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
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
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

static xvalue procLoadConsensusRecoveryOrchestrationCheckpoint(const char* sCheckpointPath)
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

static bool procWriteSnapshotState(uint32 iRound, const char* sKey, uint32 iSnapshotIndex)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildSnapshotStatePath(sPath, sizeof(sPath), iRound);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
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

static bool procWriteRemoteSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iReplayCursor,
	uint32 iSnapshotIndex,
	uint32 iPublishIndex,
	uint32 iPeerCount)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( sPath == NULL ) {
		return false;
	}

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "peer_count", 0, (int32)iPeerCount);
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-orchestration/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Consensus-Orchestration-Key", sKey);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static int procSelectPeers(
	const DemoPeerNode arrPeer[],
	int iPeerCount,
	uint32 iMaxLatencyMs,
	const DemoPeerNode* arrOut[],
	int iOutCap)
{
	int iSelected = 0;
	int i;

	if ( (arrPeer == NULL) || (arrOut == NULL) || (iOutCap <= 0) ) {
		return 0;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( arrPeer[i].iLatencyMs > iMaxLatencyMs ) {
			continue;
		}
		if ( iSelected >= iOutCap ) {
			break;
		}

		arrOut[iSelected++] = &arrPeer[i];
	}

	return iSelected;
}

static const char* procChooseOrchestrationPlan(const DemoRecoveryCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActiveRecovery > 0u ) {
		return "queue_wait";
	}

	if ( (pCenter->iReplayCursor < pCenter->iReplayTo) || (pCenter->iPublishIndex < pCenter->iCommittedRound) ) {
		return "recover_now";
	}

	return "observe";
}

static bool procRunConsensusRecoveryOrchestration(
	DemoRecoveryCenter* pCenter,
	const DemoPeerNode* arrPeer[],
	int iPeerCount,
	const char* sKey)
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
	DemoStageTask tSnapshotInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sReplayPath[260];
	char sSnapshotPath[260];
	char sPublishJson[640];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tReplayInspect, 0, sizeof(tReplayInspect));
	memset(&tSnapshotInspect, 0, sizeof(tSnapshotInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iActiveRecovery++;
	(void)procSaveConsensusRecoveryOrchestrationCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_orchestration_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
		pCenter->iSnapshotIndex,
		pCenter->iPublishIndex,
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

	procBuildSnapshotStatePath(sSnapshotPath, sizeof(sSnapshotPath), pCenter->iCommittedRound);
	procCopyText(tSnapshotInspect.sStage, sizeof(tSnapshotInspect.sStage), "inspect_local_snapshot");
	procCopyText(tSnapshotInspect.sPath, sizeof(tSnapshotInspect.sPath), sSnapshotPath);
	tSnapshotInspect.iDelayMs = 25u;
	tSnapshotInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tSnapshotInspect, 0u);
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
			int32 iRemoteReplay = 0;
			int32 iRemoteSnapshot = 0;
			int32 iRemotePublish = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteReplay = xvoTableGetInt(vRemote, "remote_replay_cursor", 0);
				iRemoteSnapshot = xvoTableGetInt(vRemote, "remote_snapshot_index", 0);
				iRemotePublish = xvoTableGetInt(vRemote, "remote_publish_index", 0);

				if ( iRemoteReplay > (int32)pCenter->iReplayCursor ) {
					pCenter->iReplayCursor = (uint32)iRemoteReplay;
				}
				if ( iRemoteSnapshot > (int32)pCenter->iSnapshotIndex ) {
					pCenter->iSnapshotIndex = (uint32)iRemoteSnapshot;
				}
				if ( iRemotePublish > (int32)pCenter->iPublishIndex ) {
					pCenter->iPublishIndex = (uint32)iRemotePublish;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iReplayCursor < pCenter->iReplayTo ) {
		pCenter->iReplayCursor = pCenter->iReplayTo;
	}
	if ( (pCenter->iCommittedRound > 0u) && (pCenter->iSnapshotIndex < (pCenter->iCommittedRound - 1u)) ) {
		pCenter->iSnapshotIndex = pCenter->iCommittedRound - 1u;
	}
	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}

	(void)procWriteRemoteSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iReplayCursor,
		pCenter->iSnapshotIndex,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procCopyText(pCenter->sRecoveryState, sizeof(pCenter->sRecoveryState), "consensus_orchestrated");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"consensus-orchestrated\",\n\t\"committed_owner\": \"%s\",\n\t\"committed_round\": %u,\n\t\"replay_cursor\": %u,\n\t\"snapshot_index\": %u,\n\t\"publish_index\": %u,\n\t\"recovery_state\": \"%s\"\n}\n",
		pCenter->sCommittedOwner,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iReplayCursor,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iPublishIndex,
		pCenter->sRecoveryState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusRecoveryOrchestrationCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "consensus_orchestrated" : "consensus_orchestration_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
		pCenter->iSnapshotIndex,
		pCenter->iPublishIndex,
		bFinished ? 1u : 0u);

	if ( pCenter->iActiveRecovery > 0u ) {
		pCenter->iActiveRecovery--;
	}
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
	if ( vRemote != NULL ) {
		xvoUnref(vRemote);
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
	xvalue vLoaded = NULL;
	const char* sOwner = NULL;
	const char* sState = NULL;
	const char* sPlan = NULL;
	int iPeerCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iRecoveryTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 21u;
	tCenter.iCommittedRound = 8u;
	tCenter.iReplayFrom = 60u;
	tCenter.iReplayTo = 66u;
	tCenter.iReplayCursor = 61u;
	tCenter.iSnapshotIndex = 60u;
	tCenter.iPublishIndex = 6u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sRecoveryState, sizeof(tCenter.sRecoveryState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-recovery-orchestration-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-recovery-orchestration-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-orchestration-summary.json");

	(void)procSaveConsensusRecoveryOrchestrationCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iReplayFrom,
		tCenter.iReplayTo,
		tCenter.iReplayCursor,
		tCenter.iSnapshotIndex,
		tCenter.iPublishIndex,
		0u);

	(void)procWriteReplayCheckpoint(tCenter.iCommittedRound, "console-service", tCenter.iReplayCursor);
	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex);

	vLoaded = procLoadConsensusRecoveryOrchestrationCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded != NULL ) {
		sOwner = (const char*)xvoTableGetText(vLoaded, "committed_owner", 0);
		sState = (const char*)xvoTableGetText(vLoaded, "state", 0);

		if ( sOwner != NULL ) {
			procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), sOwner);
		}
		if ( sState != NULL ) {
			procCopyText(tCenter.sRecoveryState, sizeof(tCenter.sRecoveryState), sState);
		}

		tCenter.iCommittedRound = (uint32)xvoTableGetInt(vLoaded, "committed_round", 0);
		tCenter.iReplayFrom = (uint32)xvoTableGetInt(vLoaded, "replay_from", 0);
		tCenter.iReplayTo = (uint32)xvoTableGetInt(vLoaded, "replay_to", 0);
		tCenter.iReplayCursor = (uint32)xvoTableGetInt(vLoaded, "replay_cursor", 0);
		tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
		tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
		xvoUnref(vLoaded);
	}

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://10.0.0.21:8080");
	arrPeer[0].iLatencyMs = 24u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://10.0.0.22:8080");
	arrPeer[1].iLatencyMs = 39u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://10.0.0.23:8080");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	iPeerCount = procSelectPeers(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs, arrSelected, DEMO_MAX_PEER);
	sPlan = procChooseOrchestrationPlan(&tCenter, iPeerCount);

	printf("consensus orchestration plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u replay=%u/%u snapshot=%u publish=%u peers=%d\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iReplayTo,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iPublishIndex,
		iPeerCount);

	if ( strcmp(sPlan, "recover_now") == 0 ) {
		if ( !procRunConsensusRecoveryOrchestration(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus recovery orchestration failed\n");
			return 1;
		}
	}

	printf(
		"after orchestration: replay=%u snapshot=%u publish=%u state=%s\n",
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sRecoveryState);
	return 0;
}
```

这段骨架的重点不是把所有分布式恢复细节一次写完，而是先把边界稳住：

- committed owner 只是恢复编排前提
- local replay 和 local snapshot 都要进同一个 orchestration scope
- remote summary 进入 future 主线之后，才能正式影响 replay / snapshot / publish 三个边界
- published orchestration 文件只是对外结果，不是替代 checkpoint

## 6. 这一页最容易写错的边界

### 6.1 remote summary 不是“看起来更新一点就直接覆盖本地状态”

这页里最容易偷懒的写法是：

- 看到某个 peer 的 replay 更深，就立刻覆盖本地 replay state
- 看到某个 peer 的 snapshot 更深，就立刻覆盖本地 snapshot state

但更稳的模型应该是：

- 先把多 peer summary 收进同一轮 orchestration scope
- 再由统一推进逻辑决定本轮 `replay_cursor / snapshot_index / publish_index` 到哪里

否则系统会出现“某个 peer 的 summary 已经写进本地状态，但这轮 orchestration 其实还没真正 join 完”的拆裂状态。

### 6.2 publish 文件和 orchestration checkpoint 仍然不是同一份文件

这页也很容易写混：

- 以为 published orchestration 文件已经足够回答当前编排是否稳定

但更稳的边界应该是：

- `runtime/consensus-recovery-orchestration-published.json`
	- 回答“这次 committed orchestration 对外发布了什么”
- `runtime/consensus-recovery-orchestration-checkpoint.json`
	- 回答“这轮 replay / snapshot / publish 当前走到了哪一步”

### 6.3 这页先停在“replay + snapshot + publish 已统一编排”，不提前把它说成“更重的 consensus snapshot orchestration 已经讲完”

这页故意把边界停在：

- local replay checkpoint 已 inspect
- local snapshot state 已 inspect
- 多个 peer orchestration summary 已 gather
- `replay_cursor / snapshot_index / publish_index` 已统一推进
- published orchestration 文件已经异步写出

但它还没有回答：

- snapshot segment 真正怎样跨轮切换和裁剪
- compaction checkpoint 怎样进入同一轮 consensus orchestration
- 更重的 consensus snapshot orchestration 怎样把 replay、snapshot、segment truncate 和 publish 全部统一成长期主线

那是下一页应该专门解决的问题。

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正把 snapshot segment 切换和 truncate 收进同一轮主线，应继续补 consensus snapshot orchestration 主线
- 如果需要把 compaction checkpoint 和 remote snapshot install 并入同一轮编排，应继续补更重的 snapshot takeover 主线
- 如果需要把 replay、snapshot、publish 之外的仲裁状态一并纳入长期恢复模型，应继续补更重的 consensus persistence orchestration 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / replay_cursor / snapshot_index / publish_index / orchestration_state`
2. `POST /api/consensus-orchestrate`
	- 只提交 orchestration 意图，让 worker 决定是否进入 replay inspect / snapshot inspect / summary gather / publish 主线
3. `GET /api/consensus-orchestration`
	- 直接返回最近一次共识恢复编排 checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 orchestration 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 committed owner、replay 进度、snapshot 进度、publish 进度和 recent orchestration history

## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识式接管恢复面板](consensus-takeover-recovery-dashboard.md)
	先对比“takeover recovery 已经接上”与“replay + snapshot + publish 已统一编排”到底差在哪
2. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	回看 `snapshot_index / truncated_to / compaction_cursor` 为什么在 orchestration 之后仍然是下一层边界
3. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	把 snapshot segment、truncate、publish 和统一收口主线正式补出来
