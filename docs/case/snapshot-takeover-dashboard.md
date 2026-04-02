# 把本地控制台服务升级成一个快照接管面板

> 这页要解决的不是“共识快照编排已经把 `snapshot_index / segment_seq / truncated_to / publish_index` 统一推进过一次，所以剩下的 snapshot install 自然就只是复制一个远端 segment”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / install_cursor / target_segment_seq / publish_index / takeover_state`，并且上一页已经能把 local inspect、multi-peer summary gather、publish 和 checkpoint 更新收成同一条 snapshot orchestration 主线之后，又开始需要把“远端 snapshot segment 到底该从哪一段开始正式 install”“如果本地上一轮只 install 到一半，下次启动应该从 `install_cursor` 还是从 `local_segment_seq` 继续”“为什么 `snapshot orchestrated` 不能直接等于 `snapshot takeover finished`”“segment rollover、installed summary 和 published snapshot 为什么仍然要进同一个 takeover scope”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 snapshot orchestration 之后靠“看见哪个 peer 更深就顺手覆盖本地 segment 文件”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识式 ownership 怎样稳定出 `committed_owner`
- 共识恢复编排怎样把 `replay / snapshot / publish` 收进同一个 orchestration scope
- 共识快照编排怎样把 `snapshot_index / segment_seq / truncated_to / publish_index` 稳定成正式 checkpoint

但真实系统再往前走一步，很快又会出现一个新的问题：

- 本地 checkpoint 已经知道当前 round 需要的 `snapshot_index`
- 但真正落在磁盘上的 local segment 可能还停在更旧的 `local_segment_seq`
- 远端 peer 可能已经有更完整的 snapshot segment，可以被 install 到本地
- 如果本地 install 到一半宕机，下次启动必须知道从哪里继续，而不是重新猜一遍

如果这时还把实现停在：

- `if ( remote_segment_seq > local_segment_seq ) copy remote file;`
- `if ( publish_index >= committed_round ) consider snapshot takeover done;`
- `if ( install once succeeds ) overwrite checkpoint;`

很快就会出现几个典型问题：

- checkpoint 说这轮 snapshot orchestration 已完成，但本地 segment 仍然缺段
- 某个 peer 的更深 segment 被提前写进本地文件，但 `install_cursor` 根本没有正式推进
- published snapshot 文件已经更新，local segment rollover 还没有收口
- 下次启动时只知道 `snapshot_index`，却不知道应该从哪一段继续 install

所以这页真正要补出的，不是“再补一次 snapshot 下载”，而是：

> 当共识快照编排已经稳定回答 snapshot 边界之后，怎样把 remote snapshot install、local segment rollover、published summary 和 checkpoint 更新统一进一条可恢复、可解释、可导出的正式快照接管主线。

## 2. 为什么这次不能只靠“看到更深 segment 就覆盖本地文件”

### 2.1 共识快照编排只回答“这一轮 snapshot 边界怎样统一推进”

上一页的共识快照编排已经很好地解决了：

- local snapshot state 怎样 inspect
- remote snapshot summary 怎样进入 future 主线
- `snapshot_index / segment_seq / truncated_to / publish_index` 怎样统一推进

但它不回答：

- 本地到底已经正式 install 到哪一段 `segment_seq`
- 远端更深 segment 到底该从哪一段开始 install
- install 一半失败之后，下次启动该怎样继续

### 2.2 `install_cursor` 不是旁注，而是正式 takeover 状态

到了这一层，系统真正要稳定回答的是：

- 当前 `local_segment_seq` 停在哪
- 当前 `install_cursor` 已经推进到哪
- 当前 `target_segment_seq` 是本地决定的，还是远端 summary 推出来的
- 当前 publish 文件回答的是“snapshot 已 orchestration”，还是“snapshot install 已真正接管完成”

这说明：

- `consensus snapshot orchestration checkpoint`
	- 回答“snapshot 边界和 publish 边界”
- `consensus snapshot takeover checkpoint`
	- 回答“remote install 和 local rollover 到底走到了哪一步”

### 2.3 这次真正新增的是“remote install + segment rollover”状态层

更稳的分工方式是：

- `consensus snapshot orchestration`
	- 负责把 snapshot boundary、truncate boundary 和 publish boundary 统一起来
- `snapshot takeover`
	- 负责把 remote segment install、local rollover 和 publish summary 做成长期可恢复主线
- 更重的 snapshot continuation
	- 再往下负责跨轮 install、segment rollover continuation 和更长生命周期的 snapshot persistence

一句话记住：

> 上一页补的是“snapshot 边界怎样统一编排”，这一页补的是“编排之后，本地怎样把真正的 snapshot segment install 和 rollover 正式接手并收口”。

## 3. 这条快照接管主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active takeover registry | 当前有哪些 key 正在做 snapshot takeover |
| `list` | recent takeover history | 页面和 JSON 展示最近 install / rollover 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 takeover |
| `queue + thread` | 后台消费 `TAKEOVER / SWEEP` | 请求线程不阻塞在 remote install 和 local rollover 上 |
| `task group` | 管住一次 takeover scope 里的 local inspect / remote fetch / install child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、install stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot takeover summary | 让远端 `target_segment_seq / remote_segment_seq` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local segment、写 install stage、读取本地 snapshot state | 让 takeover 进入正式文件主线 |
| `file_async` | 把 published takeover summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `local_segment_seq / install_cursor / target_segment_seq / publish_index` 有正式结构 |
| `time` | 记录 takeover 启动、join、install、rollover、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `snapshot_index / local_segment_seq / install_cursor / target_segment_seq / takeover_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 共识快照编排管“边界怎样统一回答”，快照接管管“真正要落在本地的 snapshot segment 怎样被正式 install 和 rollover”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot takeover 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-takeover-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-install/<round>-<seq>.json
runtime/consensus-snapshot-takeover-published.json
runtime/remote-consensus-snapshot-takeover-summary.json
```

其中：

- `config/console.json`
	- 保存 `takeover_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / install_cursor / target_segment_seq / publish_index / takeover_state`
- `runtime/console.log`
	- 记录 takeover 启动、remote summary merge、install 推进、rollover 和 publish
- `runtime/consensus-snapshot-takeover-checkpoint.json`
	- 保存最近一次正式 snapshot takeover 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-install/<round>-<seq>.json`
	- 保存这次 install stage 的中间文件
- `runtime/consensus-snapshot-takeover-published.json`
	- 保存异步发布后的 takeover 输出
- `runtime/remote-consensus-snapshot-takeover-summary.json`
	- 保存这次 remote takeover summary 的本地 stage 文件

## 5. 先把“启动装载 takeover checkpoint -> inspect local state -> gather remote summary -> 推进 install_cursor -> rollover local segment -> publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-takeover-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / snapshot_index / local_segment_seq / install_cursor / target_segment_seq / publish_index / takeover_state`
3. `dict` 表示当前 active takeover registry
4. `list` 表示 takeover / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 takeover 意图”
6. `task group` 先管住本次 takeover scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer takeover summary 走多条 `xhttp future`
9. 统一推进 `install_cursor / local_segment_seq / publish_index`
10. install stage 和 remote summary 先落本地文件，再进入 publish
11. 最后再用 `file_async future` 异步发布 committed takeover 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 3`，install cursor 也只到 `3`
- worker 会从 `peer-a`、`peer-b` 拉两份 takeover summary
- 再把 local inspect、remote summary gather、install stage、segment rollover 和 publish 收进同一个 takeover scope
- 最后异步发布 `runtime/consensus-snapshot-takeover-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_MAX_CHILD 12
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
	uint32 iTakeoverTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoSnapshotTakeoverPolicy;

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
	DemoSnapshotTakeoverPolicy tPolicy;
	uint32 iActiveTakeover;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iSnapshotIndex;
	uint32 iLocalSegmentSeq;
	uint32 iInstallCursor;
	uint32 iTargetSegmentSeq;
	uint32 iPublishIndex;
	char sCommittedOwner[32];
	char sInstalledPeer[32];
	char sTakeoverState[48];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotTakeoverCenter;

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

static void procBuildSnapshotInstallPath(char* sDst, size_t iCap, uint32 iRound, uint32 iSegmentSeq)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-install/%08u-%04u.json", (unsigned)iRound, (unsigned)iSegmentSeq);
}

static bool procSaveConsensusSnapshotTakeoverCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledPeer,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iLocalSegmentSeq,
	uint32 iInstallCursor,
	uint32 iTargetSegmentSeq,
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
	xvoTableSetText(vRoot, "installed_peer", 0, (uint8*)((sInstalledPeer != NULL) ? sInstalledPeer : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "local_segment_seq", 0, (int32)iLocalSegmentSeq);
	xvoTableSetInt(vRoot, "install_cursor", 0, (int32)iInstallCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
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

static xvalue procLoadConsensusSnapshotTakeoverCheckpoint(const char* sCheckpointPath)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_state_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "local_segment_seq", 0, (int32)iSegmentSeq);
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

static bool procWriteSnapshotSegment(uint32 iRound, uint32 iSegmentSeq, const char* sKey, const char* sState)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)((sState != NULL) ? sState : "snapshot_segment_ready"), 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
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

static bool procWriteSnapshotInstallStage(
	uint32 iRound,
	uint32 iSegmentSeq,
	const char* sKey,
	const char* sInstalledPeer,
	uint32 iSnapshotIndex)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildSnapshotInstallPath(sPath, sizeof(sPath), iRound, iSegmentSeq);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_install_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "installed_peer", 0, (uint8*)((sInstalledPeer != NULL) ? sInstalledPeer : ""), 0, FALSE);
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

static bool procWriteRemoteSnapshotTakeoverSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iSnapshotIndex,
	uint32 iRemoteSegmentSeq,
	uint32 iTargetSegmentSeq,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_takeover_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_segment_seq", 0, (int32)iRemoteSegmentSeq);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
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

static bool procPrepareSnapshotTakeoverRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-takeover/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotTakeoverPlan(const DemoSnapshotTakeoverCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActiveTakeover > 0u ) {
		return "queue_wait";
	}

	if ( pCenter->iInstallCursor < pCenter->iTargetSegmentSeq ) {
		return "takeover_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotTakeover(
	DemoSnapshotTakeoverCenter* pCenter,
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
	DemoStageTask tStateInspect;
	DemoStageTask tSegmentInspect;
	DemoStageTask tInstallStage;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sStatePath[260];
	char sSegmentPath[260];
	char sInstallPath[260];
	char sPublishJson[896];
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tStateInspect, 0, sizeof(tStateInspect));
	memset(&tSegmentInspect, 0, sizeof(tSegmentInspect));
	memset(&tInstallStage, 0, sizeof(tInstallStage));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iActiveTakeover++;
	(void)procSaveConsensusSnapshotTakeoverCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_takeover_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iInstallCursor,
		pCenter->iTargetSegmentSeq,
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

	procBuildSnapshotStatePath(sStatePath, sizeof(sStatePath), pCenter->iCommittedRound);
	procCopyText(tStateInspect.sStage, sizeof(tStateInspect.sStage), "inspect_local_snapshot_state");
	procCopyText(tStateInspect.sPath, sizeof(tStateInspect.sPath), sStatePath);
	tStateInspect.iDelayMs = 20u;
	tStateInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tStateInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procBuildSnapshotSegmentPath(sSegmentPath, sizeof(sSegmentPath), pCenter->iCommittedRound, pCenter->iLocalSegmentSeq);
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
		if ( !procPrepareSnapshotTakeoverRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			pCenter->tPolicy.iTakeoverTimeoutMs) ) {
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
			int32 iRemoteTarget = 0;
			int32 iRemotePublish = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteSnapshot = xvoTableGetInt(vRemote, "remote_snapshot_index", 0);
				iRemoteSegment = xvoTableGetInt(vRemote, "remote_segment_seq", 0);
				iRemoteTarget = xvoTableGetInt(vRemote, "target_segment_seq", 0);
				iRemotePublish = xvoTableGetInt(vRemote, "remote_publish_index", 0);

				if ( iRemoteSnapshot > (int32)pCenter->iSnapshotIndex ) {
					pCenter->iSnapshotIndex = (uint32)iRemoteSnapshot;
				}
				if ( iRemoteTarget > (int32)pCenter->iTargetSegmentSeq ) {
					pCenter->iTargetSegmentSeq = (uint32)iRemoteTarget;
					procCopyText(pCenter->sInstalledPeer, sizeof(pCenter->sInstalledPeer), arrPeer[i]->sName);
				}
				if ( iRemoteSegment > (int32)pCenter->iLocalSegmentSeq ) {
					pCenter->iLocalSegmentSeq = (uint32)iRemoteSegment;
				}
				if ( iRemotePublish > (int32)pCenter->iPublishIndex ) {
					pCenter->iPublishIndex = (uint32)iRemotePublish;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iTargetSegmentSeq < pCenter->iLocalSegmentSeq ) {
		pCenter->iTargetSegmentSeq = pCenter->iLocalSegmentSeq;
	}

	if ( pCenter->iInstallCursor < pCenter->iTargetSegmentSeq ) {
		pCenter->iInstallCursor++;
	} else if ( pCenter->iInstallCursor < pCenter->iLocalSegmentSeq ) {
		pCenter->iInstallCursor = pCenter->iLocalSegmentSeq;
	}

	if ( pCenter->iInstallCursor > pCenter->iLocalSegmentSeq ) {
		pCenter->iLocalSegmentSeq = pCenter->iInstallCursor;
	}

	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}

	(void)procWriteSnapshotState(pCenter->iCommittedRound, sKey, pCenter->iSnapshotIndex, pCenter->iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(pCenter->iCommittedRound, pCenter->iLocalSegmentSeq, sKey, "snapshot_segment_installed");
	(void)procWriteSnapshotInstallStage(
		pCenter->iCommittedRound,
		pCenter->iInstallCursor,
		sKey,
		pCenter->sInstalledPeer,
		pCenter->iSnapshotIndex);
	(void)procWriteRemoteSnapshotTakeoverSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procBuildSnapshotInstallPath(sInstallPath, sizeof(sInstallPath), pCenter->iCommittedRound, pCenter->iInstallCursor);
	procCopyText(tInstallStage.sStage, sizeof(tInstallStage.sStage), "inspect_install_stage");
	procCopyText(tInstallStage.sPath, sizeof(tInstallStage.sPath), sInstallPath);
	tInstallStage.iDelayMs = 15u;
	tInstallStage.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tInstallStage, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	procCopyText(
		pCenter->sTakeoverState,
		sizeof(pCenter->sTakeoverState),
		(pCenter->iInstallCursor >= pCenter->iTargetSegmentSeq) ? "snapshot_takeover_finished" : "snapshot_takeover_installing");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"install_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"takeover_state\": \"%s\"\n}\n",
		pCenter->sTakeoverState,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iLocalSegmentSeq,
		(unsigned)pCenter->iInstallCursor,
		(unsigned)pCenter->iTargetSegmentSeq,
		(unsigned)pCenter->iPublishIndex,
		pCenter->sTakeoverState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotTakeoverCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sTakeoverState : "consensus_snapshot_takeover_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iInstallCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		bFinished ? 1u : 0u);

	if ( pCenter->iActiveTakeover > 0u ) {
		pCenter->iActiveTakeover--;
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
	DemoSnapshotTakeoverCenter tCenter;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	const DemoPeerNode* arrSelected[DEMO_MAX_PEER];
	xvalue vLoaded = NULL;
	str sOwner = NULL;
	str sInstalledPeer = NULL;
	str sState = NULL;
	const char* sPlan = NULL;
	int iPeerCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iTakeoverTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 3u;
	tCenter.iInstallCursor = 3u;
	tCenter.iTargetSegmentSeq = 5u;
	tCenter.iPublishIndex = 8u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sTakeoverState, sizeof(tCenter.sTakeoverState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-takeover-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-takeover-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-takeover-summary.json");

	(void)procSaveConsensusSnapshotTakeoverCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledPeer,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iLocalSegmentSeq,
		tCenter.iInstallCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotTakeoverCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledPeer = xvoTableGetText(vLoaded, "installed_peer", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iInstallCursor = (uint32)xvoTableGetInt(vLoaded, "install_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sTakeoverState, sizeof(tCenter.sTakeoverState), (const char*)sState);
	xvoUnref(vLoaded);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:9101");
	arrPeer[0].iLatencyMs = 25u;
	arrPeer[0].iPriority = 10;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:9102");
	arrPeer[1].iLatencyMs = 32u;
	arrPeer[1].iPriority = 9;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:9103");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 5;
	arrPeer[2].bHealthy = false;

	iPeerCount = procSelectPeers(
		arrPeer,
		DEMO_MAX_PEER,
		tCenter.tPolicy.iMaxPeerLatencyMs,
		arrSelected,
		DEMO_MAX_PEER);

	sPlan = procChooseSnapshotTakeoverPlan(&tCenter, iPeerCount);

	printf("snapshot takeover plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u install_cursor=%u target_segment=%u publish=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iInstallCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sInstalledPeer);

	if ( strcmp(sPlan, "takeover_now") == 0 ) {
		if ( !procRunConsensusSnapshotTakeover(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot takeover failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot takeover: snapshot=%u local_segment=%u install_cursor=%u target_segment=%u publish=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iInstallCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sTakeoverState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot orchestrated` 不等于 `snapshot takeover finished`

这页最容易偷懒的写法是：

- 看到 `snapshot_index >= committed_round` 就认为 takeover 已完成
- 看到 published snapshot 文件已经写出，就认为 local segment 也已经追平
- 看到某个 peer 给出更深 segment，就直接把它当成本地 install 完成

但更稳的模型应该是：

- `snapshot orchestrated`
	- 回答这轮 snapshot 边界怎样统一推进
- `snapshot takeover finished`
	- 回答真正需要 install 的 remote segment 是否已经被本地正式接住

### 6.2 `local_segment_seq` 和 `install_cursor` 不是一个字段

这页也很容易写混：

- 把 `local_segment_seq` 直接等同于“当前准备 install 的 segment”
- 或者把 `install_cursor` 直接当成“本地已经完全落盘的 segment”

但更稳的边界应该是：

- `local_segment_seq`
	- 回答当前本地已经正式落盘到哪一段
- `install_cursor`
	- 回答这轮 takeover 当前推进到了哪一段

### 6.3 remote summary 只能更新 takeover 目标，不能偷偷越过本地 rollover

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地马上把 checkpoint 也写成 `local_segment_seq = 6`

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`
- 真正的 `local_segment_seq` 还得等 install stage 和 local rollover 正式走完

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输和校验协议，应继续补更重的 snapshot continuation 主线
- 如果需要把 segment rollover、publish 和更长生命周期的 persistence state 合并进同一条长期主线，应继续补更重的 snapshot persistence / continuation 主线
- 如果需要把 takeover 主线扩展到更完整的多轮 install retry 和 quota 控制，应继续补更重的 snapshot scheduler 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / install_cursor / target_segment_seq / publish_index / takeover_state`
2. `POST /api/consensus-snapshot-takeover`
	- 只提交 takeover 意图，让 worker 决定是否进入 inspect / gather / install / rollover / publish 主线
3. `GET /api/consensus-snapshot-takeover`
	- 直接返回最近一次 snapshot takeover checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 takeover 的重试和 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 takeover 进度、local segment、target segment、publish 和 recent takeover history

## 9. 下一步阅读

如果你准备继续把 snapshot install 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	先回看为什么上一页只回答 snapshot 边界，而这一页开始回答真正的 install 和 rollover
2. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
	对比“snapshot segment takeover”和“unfinished compaction takeover”到底各自接管的是什么
3. [把本地控制台服务升级成一个快照续传编排面板](snapshot-continuation-dashboard.md)
	把多轮 install、segment rollover continuation 和更长生命周期的 snapshot persistence 正式补出来
