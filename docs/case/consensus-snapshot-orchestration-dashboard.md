# 把本地控制台服务升级成一个共识快照编排面板

> 这页要解决的不是“recovery orchestration 已经把 replay、snapshot index 和 publish index 推到一条线上，所以再写一个 snapshot 文件就结束了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / replay_cursor / snapshot_index / truncated_to / segment_seq / publish_index / snapshot_state`，并且上一页已经能把 replay、snapshot 和 publish 统一编排之后，又开始需要把“为什么 `snapshot_index` 和真正的 snapshot segment 仍然不是一回事”“为什么 `segment inspect / remote snapshot summary gather / truncated_to 推进 / published snapshot` 仍然要进同一个 orchestration scope”“如果不同 peer 对同一 committed round 提供了不同的 snapshot segment depth 和 truncate 位置，本地应该怎样把 `snapshot_index / segment_seq / truncated_to / publish_index / checkpoint` 收成同一条稳定主线”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 recovery orchestration 之后靠“再补一个 segment 文件”和“再补一个 truncate 脚本”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- committed owner 怎样稳定下来
- takeover recovery 怎样把 replay gap 正式接上
- recovery orchestration 怎样把 replay、snapshot、publish 推进到同一条主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- `snapshot_index` 已推进，不代表新的 snapshot segment 已经正式写出
- segment 文件已经写出，不代表 `truncated_to` 已经追上这一轮应清理的边界
- 不同 peer 可能都承认同一个 committed round，但它们手里的 snapshot segment 序列和 truncate 位置并不一致
- 如果没有正式的 snapshot orchestration 层，系统仍然会回到“编排做完后，再零散补 segment 和 truncate”

如果这时还把实现停在：

- `if ( snapshot_index >= committed_round ) write_snapshot_segment();`
- `if ( segment_written ) truncated_to = snapshot_index - 1;`
- `if ( publish succeeds ) consider snapshot stable;`

很快就会出现几个典型问题：

- checkpoint 说 snapshot 已更新，但 segment 文件还是旧版本
- published snapshot 文件已经更新，`truncated_to` 还停在旧位置
- 某个 peer 的 segment 更深，系统却在本地没有正式记录这一轮 truncate 应该推进到哪里
- 下次启动时知道 `snapshot_index`，却不知道当前 snapshot orchestration 应该从 segment、truncate 还是 publish 继续

所以这页真正要补出的，不是“再补一个 snapshot 文件”，而是：

> 当 recovery orchestration 已经稳定之后，怎样把 local snapshot inspect、segment inspect、多 peer snapshot summary gather、truncate 推进、publish 和 checkpoint 更新统一进一条可恢复、可解释、可导出的正式共识快照编排主线。

## 2. 为什么这次不能只靠“orchestration 之后顺手写一个 snapshot 文件”

### 2.1 recovery orchestration 只回答“replay、snapshot、publish 怎样统一推进”

上一页的共识恢复编排已经很好地解决了：

- replay、snapshot、publish 怎样进入同一个 orchestration scope
- 多 peer summary 怎样正式影响 `replay_cursor / snapshot_index / publish_index`
- published orchestration 文件怎样异步写出

但它不回答：

- 当前 snapshot segment 文件到底是哪一份
- `truncated_to` 应该怎样和 `snapshot_index / publish_index` 一起推进
- snapshot publish 成功之后，日志或 segment 边界到底推进到了哪里

### 2.2 snapshot orchestration 不是文件收尾，而是正式状态层

到了这一层，系统真正要稳定回答的是：

- 当前 `snapshot_index / segment_seq / truncated_to / publish_index` 到了哪里
- 当前 local snapshot segment 是否已经和 committed round 对齐
- 当前 remote snapshot summaries 到底有没有正式进入本轮 snapshot orchestration 判断
- 当前 published snapshot 和 checkpoint 描述的是不是同一轮 segment / truncate 结果

这说明：

- `snapshot_index` 只是恢复编排结果之一
- `snapshot segment` 是正式阶段产物
- `consensus snapshot orchestration checkpoint` 才是当前快照编排状态

### 2.3 这次真正新增的是“snapshot segment + truncate + publish”的统一编排层

更稳的分工方式是：

- `consensus recovery orchestration`
	- 负责 replay、snapshot index、publish index 怎样统一推进
- `consensus snapshot orchestration`
	- 负责 snapshot segment、truncate、publish 怎样正式收口
- `snapshot takeover`
	- 再往下负责远端 snapshot install、segment 迁移和更重的跨节点恢复

一句话记住：

> 上一页补的是“snapshot index 已进入正式编排”，这一页补的是“snapshot segment 和 truncate 怎样进入同一条正式主线”。

## 3. 这条共识快照编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active snapshot orchestration registry | 当前有哪些 key 正在做共识快照编排 |
| `list` | recent snapshot orchestration history | 页面和 JSON 展示最近正式快照编排结果 |
| `list` | recent snapshot deferred history | 页面和 JSON 展示为什么这次没有进入正式 snapshot orchestration |
| `queue + thread` | 后台消费 `SNAPSHOT_ORCHESTRATE / SWEEP` | 请求线程不阻塞在 segment、truncate、publish 上 |
| `task group` | 管住一次 snapshot orchestration scope 里的 local snapshot inspect / local segment inspect / multi-peer summary fetch / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达多 peer snapshot summary、publish 和 child stage 的完成态 | 统一 join 与失败边界 |
| `xurl + xhttp` | 向多个 peer 拉取当前 committed round 的 snapshot summary | 让远端 segment / truncate 深度进入正式 future 主线 |
| `file + path` | 装载 checkpoint、本地 snapshot state、写 snapshot segment | 让快照编排进入正式文件主线 |
| `file_async` | 把 committed snapshot 结果异步发布成对外可读状态文件 | 发布仍然是正式 future |
| `value + json` | 构造和解析 checkpoint、segment、summary、publish JSON | 让 `snapshot_index / segment_seq / truncated_to / publish_index` 有正式结构 |
| `time` | 记录 snapshot orchestration 启动、summary gather、segment 写出和 publish 时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `snapshot_index / segment_seq / truncated_to / publish_index / snapshot_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `recovery orchestration` 管“snapshot index 怎样推进”，`consensus snapshot orchestration` 管“segment、truncate、publish 怎样正式统一收口”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot orchestration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-orchestration-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<segment>.json
runtime/consensus-snapshot-orchestration-published.json
runtime/remote-consensus-snapshot-summary.json
```

其中：

- `config/console.json`
	- 保存 `recovery_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / segment_seq / truncated_to / publish_index / snapshot_state`
- `runtime/console.log`
	- 记录 snapshot orchestration 启动、summary gather、segment 写出和 publish
- `runtime/consensus-snapshot-orchestration-checkpoint.json`
	- 保存最近一次正式共识快照编排状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 committed round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<segment>.json`
	- 保存当前 committed round 的本地 snapshot segment
- `runtime/consensus-snapshot-orchestration-published.json`
	- 保存异步发布后的 committed snapshot 输出
- `runtime/remote-consensus-snapshot-summary.json`
	- 保存这次多 peer snapshot summary 的本地 stage 文件

## 5. 先把“启动装载 snapshot checkpoint -> inspect local snapshot state -> inspect local segment -> multi-peer snapshot summary gather -> advance truncated_to -> publish snapshot”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-orchestration-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / snapshot_index / segment_seq / truncated_to / publish_index / snapshot_state`
3. `dict` 表示当前 active snapshot orchestration registry
4. `list` 表示 snapshot orchestration / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 snapshot orchestration scope
7. local snapshot state inspect 用一个 child 表达
8. local snapshot segment inspect 再用一个 child 表达
9. 多个 peer snapshot summary 走多条 `xhttp future`
10. 统一推进 `snapshot_index / segment_seq / truncated_to / publish_index`
11. 最后再用 `file_async future` 异步发布 committed snapshot 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 还停在 `snapshot_index = 7`
- 本地 segment 还停在 `segment_seq = 2`
- worker 会从 `peer-a`、`peer-b` 拉两份 snapshot summary
- 再把 local snapshot inspect、local segment inspect、multi-peer summary gather 和 publish 收进同一个 snapshot orchestration scope
- 最后异步发布 `runtime/consensus-snapshot-orchestration-published.json`

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
} DemoSnapshotPolicy;

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
	DemoSnapshotPolicy tPolicy;
	uint32 iActiveSnapshot;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iSnapshotIndex;
	uint32 iSegmentSeq;
	uint32 iTruncatedTo;
	uint32 iPublishIndex;
	char sCommittedOwner[32];
	char sSnapshotState[40];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotCenter;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildSnapshotStatePath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-state/%08u.json", (unsigned)iRound);
}

static void procBuildSnapshotSegmentPath(char* sDst, size_t iCap, uint32 iRound, uint32 iSegmentSeq)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-segment/%08u-%04u.json", (unsigned)iRound, (unsigned)iSegmentSeq);
}

static bool procSaveConsensusSnapshotCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iSegmentSeq,
	uint32 iTruncatedTo,
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
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "segment_seq", 0, (int32)iSegmentSeq);
	xvoTableSetInt(vRoot, "truncated_to", 0, (int32)iTruncatedTo);
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

static xvalue procLoadConsensusSnapshotCheckpoint(const char* sCheckpointPath)
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

static bool procWriteSnapshotState(uint32 iRound, const char* sKey, uint32 iSnapshotIndex, uint32 iSegmentSeq)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_state_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "segment_seq", 0, (int32)iSegmentSeq);
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

static bool procWriteSnapshotSegment(uint32 iRound, uint32 iSegmentSeq, const char* sKey, uint32 iSnapshotIndex)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildSnapshotSegmentPath(sPath, sizeof(sPath), iRound, iSegmentSeq);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_segment_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "segment_seq", 0, (int32)iSegmentSeq);
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

static bool procWriteRemoteSnapshotSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iSnapshotIndex,
	uint32 iSegmentSeq,
	uint32 iTruncatedTo,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_segment_seq", 0, (int32)iSegmentSeq);
	xvoTableSetInt(vRoot, "remote_truncated_to", 0, (int32)iTruncatedTo);
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

static bool procPrepareSnapshotRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Consensus-Snapshot-Key", sKey);
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

static const char* procChooseSnapshotPlan(const DemoSnapshotCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActiveSnapshot > 0u ) {
		return "queue_wait";
	}

	if ( (pCenter->iSnapshotIndex < pCenter->iCommittedRound) || (pCenter->iPublishIndex < pCenter->iCommittedRound) ) {
		return "snapshot_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotOrchestration(
	DemoSnapshotCenter* pCenter,
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
	DemoStageTask tSnapshotInspect;
	DemoStageTask tSegmentInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sSnapshotPath[260];
	char sSegmentPath[260];
	char sPublishJson[768];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tSnapshotInspect, 0, sizeof(tSnapshotInspect));
	memset(&tSegmentInspect, 0, sizeof(tSegmentInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iActiveSnapshot++;
	(void)procSaveConsensusSnapshotCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iSegmentSeq,
		pCenter->iTruncatedTo,
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

	procBuildSnapshotStatePath(sSnapshotPath, sizeof(sSnapshotPath), pCenter->iCommittedRound);
	procCopyText(tSnapshotInspect.sStage, sizeof(tSnapshotInspect.sStage), "inspect_local_snapshot_state");
	procCopyText(tSnapshotInspect.sPath, sizeof(tSnapshotInspect.sPath), sSnapshotPath);
	tSnapshotInspect.iDelayMs = 20u;
	tSnapshotInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tSnapshotInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procBuildSnapshotSegmentPath(sSegmentPath, sizeof(sSegmentPath), pCenter->iCommittedRound, pCenter->iSegmentSeq);
	procCopyText(tSegmentInspect.sStage, sizeof(tSegmentInspect.sStage), "inspect_local_snapshot_segment");
	procCopyText(tSegmentInspect.sPath, sizeof(tSegmentInspect.sPath), sSegmentPath);
	tSegmentInspect.iDelayMs = 25u;
	tSegmentInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tSegmentInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPrepareSnapshotRequest(
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
			int32 iRemoteSnapshot = 0;
			int32 iRemoteSegment = 0;
			int32 iRemoteTruncated = 0;
			int32 iRemotePublish = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteSnapshot = xvoTableGetInt(vRemote, "remote_snapshot_index", 0);
				iRemoteSegment = xvoTableGetInt(vRemote, "remote_segment_seq", 0);
				iRemoteTruncated = xvoTableGetInt(vRemote, "remote_truncated_to", 0);
				iRemotePublish = xvoTableGetInt(vRemote, "remote_publish_index", 0);

				if ( iRemoteSnapshot > (int32)pCenter->iSnapshotIndex ) {
					pCenter->iSnapshotIndex = (uint32)iRemoteSnapshot;
				}
				if ( iRemoteSegment > (int32)pCenter->iSegmentSeq ) {
					pCenter->iSegmentSeq = (uint32)iRemoteSegment;
				}
				if ( iRemoteTruncated > (int32)pCenter->iTruncatedTo ) {
					pCenter->iTruncatedTo = (uint32)iRemoteTruncated;
				}
				if ( iRemotePublish > (int32)pCenter->iPublishIndex ) {
					pCenter->iPublishIndex = (uint32)iRemotePublish;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iSnapshotIndex < pCenter->iCommittedRound ) {
		pCenter->iSnapshotIndex = pCenter->iCommittedRound;
	}
	pCenter->iSegmentSeq++;
	if ( pCenter->iTruncatedTo < pCenter->iSnapshotIndex ) {
		pCenter->iTruncatedTo = pCenter->iSnapshotIndex;
	}
	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}

	(void)procWriteSnapshotState(pCenter->iCommittedRound, sKey, pCenter->iSnapshotIndex, pCenter->iSegmentSeq);
	(void)procWriteSnapshotSegment(pCenter->iCommittedRound, pCenter->iSegmentSeq, sKey, pCenter->iSnapshotIndex);
	(void)procWriteRemoteSnapshotSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iSegmentSeq,
		pCenter->iTruncatedTo,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procCopyText(pCenter->sSnapshotState, sizeof(pCenter->sSnapshotState), "consensus_snapshot_orchestrated");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"consensus-snapshot-orchestrated\",\n\t\"committed_owner\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"segment_seq\": %u,\n\t\"truncated_to\": %u,\n\t\"publish_index\": %u,\n\t\"snapshot_state\": \"%s\"\n}\n",
		pCenter->sCommittedOwner,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iSegmentSeq,
		(unsigned)pCenter->iTruncatedTo,
		(unsigned)pCenter->iPublishIndex,
		pCenter->sSnapshotState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "consensus_snapshot_orchestrated" : "consensus_snapshot_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iSegmentSeq,
		pCenter->iTruncatedTo,
		pCenter->iPublishIndex,
		bFinished ? 1u : 0u);

	if ( pCenter->iActiveSnapshot > 0u ) {
		pCenter->iActiveSnapshot--;
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
	DemoSnapshotCenter tCenter;
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
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 7u;
	tCenter.iSegmentSeq = 2u;
	tCenter.iTruncatedTo = 6u;
	tCenter.iPublishIndex = 7u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sSnapshotState, sizeof(tCenter.sSnapshotState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-orchestration-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-orchestration-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-summary.json");

	(void)procSaveConsensusSnapshotCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iSegmentSeq,
		tCenter.iTruncatedTo,
		tCenter.iPublishIndex,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iSegmentSeq, "console-service", tCenter.iSnapshotIndex);

	vLoaded = procLoadConsensusSnapshotCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded != NULL ) {
		sOwner = (const char*)xvoTableGetText(vLoaded, "committed_owner", 0);
		sState = (const char*)xvoTableGetText(vLoaded, "state", 0);

		if ( sOwner != NULL ) {
			procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), sOwner);
		}
		if ( sState != NULL ) {
			procCopyText(tCenter.sSnapshotState, sizeof(tCenter.sSnapshotState), sState);
		}

		tCenter.iCommittedRound = (uint32)xvoTableGetInt(vLoaded, "committed_round", 0);
		tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
		tCenter.iSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "segment_seq", 0);
		tCenter.iTruncatedTo = (uint32)xvoTableGetInt(vLoaded, "truncated_to", 0);
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
	sPlan = procChooseSnapshotPlan(&tCenter, iPeerCount);

	printf("consensus snapshot plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u segment=%u truncated_to=%u publish=%u peers=%d\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iSegmentSeq,
		(unsigned)tCenter.iTruncatedTo,
		(unsigned)tCenter.iPublishIndex,
		iPeerCount);

	if ( strcmp(sPlan, "snapshot_now") == 0 ) {
		if ( !procRunConsensusSnapshotOrchestration(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot orchestration failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot orchestration: snapshot=%u segment=%u truncated_to=%u publish=%u state=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iSegmentSeq,
		(unsigned)tCenter.iTruncatedTo,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sSnapshotState);
	return 0;
}
```

这段骨架的重点不是把所有远端 snapshot 安装细节一次写完，而是先把边界稳住：

- `snapshot_index` 只是快照编排前提，不等于 segment 已经正式收口
- local snapshot state 和 local snapshot segment 都要进同一个 orchestration scope
- remote snapshot summary 进入 future 主线之后，才能正式影响 `segment_seq / truncated_to / publish_index`
- published snapshot 文件只是对外结果，不是替代 checkpoint
```

## 6. 这一页最容易写错的边界

### 6.1 remote snapshot summary 不是“看起来更深就立刻覆盖本地 segment 状态”

这页里最容易偷懒的写法是：

- 看到某个 peer 的 `remote_snapshot_index` 更深，就立刻覆盖本地 snapshot state
- 看到某个 peer 的 `remote_segment_seq` 更大，就立刻把本地 segment 标成最新
- 看到某个 peer 的 `remote_truncated_to` 更靠后，就立刻推进 truncate

但更稳的模型应该是：

- 先把多 peer snapshot summaries 收进同一轮 snapshot orchestration scope
- 再由统一推进逻辑决定本轮 `snapshot_index / segment_seq / truncated_to / publish_index` 到哪里

否则系统会出现“某个 peer 的 segment 状态已经写进本地 checkpoint，但这轮 snapshot orchestration 其实还没真正 join 完”的拆裂状态。

### 6.2 published snapshot 文件和 snapshot orchestration checkpoint 仍然不是同一份文件

这页也很容易写混：

- 以为 published snapshot 文件已经足够回答当前快照是否稳定

但更稳的边界应该是：

- `runtime/consensus-snapshot-orchestration-published.json`
	- 回答“这次 committed snapshot 对外发布了什么”
- `runtime/consensus-snapshot-orchestration-checkpoint.json`
	- 回答“这轮 segment / truncate / publish 当前走到了哪一步”

### 6.3 这页先停在“snapshot segment + truncate + publish 已统一编排”，不提前把它说成“更重的 snapshot takeover 已经讲完”

这页故意把边界停在：

- local snapshot state 已 inspect
- local snapshot segment 已 inspect
- 多个 peer snapshot summary 已 gather
- `snapshot_index / segment_seq / truncated_to / publish_index` 已统一推进
- published snapshot 文件已经异步写出

但它还没有回答：

- 远端 snapshot segment 怎样正式安装到本地
- compaction cursor 和 snapshot segment 切换怎样跨轮继续
- 更重的 snapshot takeover 怎样把 snapshot install、segment rollover 和 publish 全部统一成长期主线

那是下一页应该专门解决的问题。

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正把远端 snapshot segment install 收进同一轮主线，应继续补 snapshot takeover 主线
- 如果需要把 compaction cursor 和 segment rollover 并入同一轮编排，应继续补更重的 snapshot continuation 主线
- 如果需要把 snapshot 之外的更重持久化状态一并纳入长期恢复模型，应继续补更重的 consensus persistence snapshot 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / segment_seq / truncated_to / publish_index / snapshot_state`
2. `POST /api/consensus-snapshot`
	- 只提交 snapshot orchestration 意图，让 worker 决定是否进入 snapshot inspect / segment inspect / summary gather / publish 主线
3. `GET /api/consensus-snapshot`
	- 直接返回最近一次共识快照编排 checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 snapshot orchestration 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 snapshot 进度、segment 序号、truncate 进度、publish 进度和 recent snapshot history

## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识恢复编排面板](consensus-recovery-orchestration-dashboard.md)
	先对比“snapshot index 已统一编排”与“snapshot segment + truncate 已统一编排”到底差在哪
2. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	回看 `snapshot_index / truncated_to / compaction_cursor` 为什么在 snapshot orchestration 之后仍然是更底层的文件状态边界
3. 继续往下补“快照接管面板”这条更重的业务变体
	把远端 snapshot install、segment rollover、publish 和统一收口主线正式补出来
