# 把本地控制台服务升级成一个快照续传编排面板

> 这页要解决的不是“快照接管已经把 `local_segment_seq / continuation_cursor / target_segment_seq / publish_index` 推过一轮，所以剩下的工作只是多跑几次 install”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / publish_index / continuation_state`，并且上一页已经能把 remote install、local rollover、published summary 和 checkpoint 更新收成一条正式 snapshot takeover 主线之后，又开始需要把“如果一轮 install 只能推进一小段，下一轮到底从哪一个 cursor 继续”“如果 local install 已经追平，但 rollover 还没做完，为什么还不能把这轮状态说成 fully continued”“为什么 `continuation_cursor` 和 `rollover_cursor` 必须分开建模”“多轮 install、rollover stage、publish summary 和 checkpoint 为什么仍然要进同一个 continuation scope”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 snapshot takeover 之后靠“下次启动时再看看从哪儿补起”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local rollover 和 published summary 收进同一个 takeover scope
- takeover checkpoint 为什么不能被 published snapshot 文件替代

但真实系统再往前走一步，很快又会出现一个新的问题：

- 一轮 takeover 可能只把 install 推到 `continuation_cursor = 4`
- 当前 `target_segment_seq` 可能已经是 `7`
- 本地 `local_segment_seq` 也许已经跟上了 `4`
- 但 `rollover_cursor` 仍然只到 `3`
- 如果此时进程退出，下次启动必须知道是继续 install、继续 rollover，还是两者都继续

如果这时还把实现停在：

- `if ( continuation_cursor < target ) install next segment;`
- `if ( local_segment_seq == target ) publish success;`
- `if ( restart ) just read latest segment and guess next step;`

很快就会出现几个典型问题：

- checkpoint 说 takeover 已开始，但看不出这一轮到底卡在 continuation 还是 rollover
- install 已经推进到更深 segment，rollover 还停在旧值，却被误判成整体完成
- 某次 publish 文件写出成功后，下一轮 continuation 的真实 cursor 被旧 checkpoint 覆盖
- 下次启动时知道 target 很深，却不知道该先继续 install 还是先补 rollover gap

所以这页真正要补出的，不是“接着把剩下几段装完”，而是：

> 当 snapshot takeover 已经进入长期运行状态之后，怎样把多轮 install continuation、rollover continuation、published summary 和 checkpoint 更新统一进一条可恢复、可解释、可导出的正式快照续传编排主线。

## 2. 为什么这次不能只靠“接着跑几次 install 就算了”

### 2.1 快照接管只回答“这一轮 install / rollover 怎样正式接上”

上一页的快照接管已经很好地解决了：

- 本地 snapshot state 和 local segment 怎样 inspect
- remote summary 怎样进入正式 future 主线
- `continuation_cursor / local_segment_seq / target_segment_seq / publish_index` 怎样在同一轮里推进

但它不回答：

- install 一轮只推进一步时，下次要从哪继续
- rollover 如果落后于 install，一轮结束后应不应该立即判定完成
- 多轮 install / rollover 之间的 continuation gap 怎样正式记录

### 2.2 `continuation_cursor` 和 `rollover_cursor` 不是同一个字段

到了这一层，系统真正要稳定回答的是：

- 当前 `continuation_cursor` 已经推进到哪
- 当前 `rollover_cursor` 还落后多少
- 当前 `local_segment_seq` 是不是已经追上 install 结果
- 当前 publish 文件回答的是“这轮 continuation 还在跑”，还是“rollover 也已经正式收口”

这说明：

- `snapshot takeover checkpoint`
	- 回答“一轮 takeover 当前推进到了哪”
- `snapshot continuation checkpoint`
	- 回答“多轮 install / rollover continuation 当前到底卡在什么位置”

### 2.3 这次真正新增的是“多轮 continuation + rollover continuation”状态层

更稳的分工方式是：

- `snapshot takeover`
	- 负责把 remote install、local rollover 和 published summary 先接进正式主线
- `snapshot continuation orchestration`
	- 负责多轮 install continuation、rollover continuation 和 published continuation summary
- `snapshot persistence`
	- 再往下负责 durable manifest、archive、checkpoint version 和 published state 的长期收口
- 更重的 snapshot scheduler / quota
	- 负责更长生命周期的 admission、跨轮 persistence 和恢复调度

一句话记住：

> 上一页补的是“这一轮 takeover 怎么接住”，这一页补的是“接住之后，多轮 continuation 怎么持续推进并正式收口”。

## 3. 这条快照续传编排主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active continuation registry | 当前有哪些 key 正在做 snapshot continuation |
| `list` | recent continuation history | 页面和 JSON 展示最近 continuation / rollover 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 continuation |
| `queue + thread` | 后台消费 `CONTINUE / SWEEP` | 请求线程不阻塞在多轮 install continuation 和 rollover continuation 上 |
| `task group` | 管住一次 continuation scope 里的 local inspect / remote fetch / continuation child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、continuation stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot continuation summary | 让远端 `target_segment_seq / remote_rollover_cursor` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local segment、写 continuation stage、写 rollover stage、读取本地 snapshot state | 让 continuation 进入正式文件主线 |
| `file_async` | 把 published continuation summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / publish_index` 有正式结构 |
| `time` | 记录 continuation 启动、join、install、rollover、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `snapshot_index / local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / continuation_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照接管管“这一轮 install / rollover 怎样接住”，快照续传编排管“多轮 continuation 和 rollover continuation 怎样持续推进并正式收口”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot continuation 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-continuation-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-install/<round>-<seq>.json
runtime/consensus-snapshot-rollover/<round>-<seq>.json
runtime/consensus-snapshot-continuation-published.json
runtime/remote-consensus-snapshot-continuation-summary.json
```

其中：

- `config/console.json`
	- 保存 `continuation_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / publish_index / continuation_state`
- `runtime/console.log`
	- 记录 continuation 启动、remote summary merge、install 续传、rollover 续传和 publish
- `runtime/consensus-snapshot-continuation-checkpoint.json`
	- 保存最近一次正式 snapshot continuation 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-install/<round>-<seq>.json`
	- 保存这轮 continuation install stage 的中间文件
- `runtime/consensus-snapshot-rollover/<round>-<seq>.json`
	- 保存这轮 rollover continuation 的中间文件
- `runtime/consensus-snapshot-continuation-published.json`
	- 保存异步发布后的 continuation 输出
- `runtime/remote-consensus-snapshot-continuation-summary.json`
	- 保存这次 remote continuation summary 的本地 stage 文件

## 5. 先把“启动装载 continuation checkpoint -> inspect local state -> gather remote summary -> 推进 continuation_cursor -> 推进 rollover_cursor -> publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-continuation-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / snapshot_index / local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / publish_index / continuation_state`
3. `dict` 表示当前 active continuation registry
4. `list` 表示 continuation / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 continuation 意图”
6. `task group` 先管住本次 continuation scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer continuation summary 走多条 `xhttp future`
9. 统一推进 `continuation_cursor / rollover_cursor / local_segment_seq / publish_index`
10. install stage、rollover stage 和 remote summary 先落本地文件，再进入 publish
11. 最后再用 `file_async future` 异步发布 committed continuation 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`continuation_cursor = 4`，`rollover_cursor = 3`
- worker 会从 `peer-a`、`peer-b` 拉两份 continuation summary
- 再把 local inspect、remote summary gather、install stage、rollover stage 和 publish 收进同一个 continuation scope
- 最后异步发布 `runtime/consensus-snapshot-continuation-published.json`

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
	uint32 iContinuationTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoSnapshotContinuationPolicy;

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
	DemoSnapshotContinuationPolicy tPolicy;
	uint32 iActiveContinuation;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iSnapshotIndex;
	uint32 iLocalSegmentSeq;
	uint32 iContinuationCursor;
	uint32 iRolloverCursor;
	uint32 iTargetSegmentSeq;
	uint32 iPublishIndex;
	char sCommittedOwner[32];
	char sInstalledPeer[32];
	char sContinuationState[48];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotContinuationCenter;

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

static void procBuildSnapshotRolloverPath(char* sDst, size_t iCap, uint32 iRound, uint32 iSegmentSeq)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-rollover/%08u-%04u.json", (unsigned)iRound, (unsigned)iSegmentSeq);
}

static bool procSaveConsensusSnapshotContinuationCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledPeer,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iLocalSegmentSeq,
	uint32 iContinuationCursor,
	uint32 iRolloverCursor,
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
	xvoTableSetInt(vRoot, "continuation_cursor", 0, (int32)iContinuationCursor);
	xvoTableSetInt(vRoot, "rollover_cursor", 0, (int32)iRolloverCursor);
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

static xvalue procLoadConsensusSnapshotContinuationCheckpoint(const char* sCheckpointPath)
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

static bool procWriteSnapshotRolloverStage(
	uint32 iRound,
	uint32 iSegmentSeq,
	const char* sKey,
	uint32 iSnapshotIndex)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildSnapshotRolloverPath(sPath, sizeof(sPath), iRound, iSegmentSeq);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_rollover_ready", 0, FALSE);
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

static bool procWriteRemoteSnapshotContinuationSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iSnapshotIndex,
	uint32 iRemoteSegmentSeq,
	uint32 iRemoteRolloverCursor,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_continuation_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_segment_seq", 0, (int32)iRemoteSegmentSeq);
	xvoTableSetInt(vRoot, "remote_rollover_cursor", 0, (int32)iRemoteRolloverCursor);
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

static bool procPrepareSnapshotContinuationRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-continuation/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotContinuationPlan(const DemoSnapshotContinuationCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActiveContinuation > 0u ) {
		return "queue_wait";
	}

	if ( (pCenter->iContinuationCursor < pCenter->iTargetSegmentSeq) || (pCenter->iRolloverCursor < pCenter->iContinuationCursor) ) {
		return "continuation_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotContinuation(
	DemoSnapshotContinuationCenter* pCenter,
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
	DemoStageTask tRolloverStage;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sStatePath[260];
	char sSegmentPath[260];
	char sInstallPath[260];
	char sRolloverPath[260];
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
	memset(&tRolloverStage, 0, sizeof(tRolloverStage));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iActiveContinuation++;
	(void)procSaveConsensusSnapshotContinuationCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_continuation_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iContinuationCursor,
		pCenter->iRolloverCursor,
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
		if ( !procPrepareSnapshotContinuationRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			pCenter->tPolicy.iContinuationTimeoutMs) ) {
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
			int32 iRemoteRollover = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteSnapshot = xvoTableGetInt(vRemote, "remote_snapshot_index", 0);
				iRemoteSegment = xvoTableGetInt(vRemote, "remote_segment_seq", 0);
				iRemoteTarget = xvoTableGetInt(vRemote, "target_segment_seq", 0);
				iRemotePublish = xvoTableGetInt(vRemote, "remote_publish_index", 0);
				iRemoteRollover = xvoTableGetInt(vRemote, "remote_rollover_cursor", 0);

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
				if ( iRemoteRollover > (int32)pCenter->iRolloverCursor ) {
					pCenter->iRolloverCursor = (uint32)iRemoteRollover;
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

	if ( pCenter->iContinuationCursor < pCenter->iTargetSegmentSeq ) {
		pCenter->iContinuationCursor++;
	} else if ( pCenter->iContinuationCursor < pCenter->iLocalSegmentSeq ) {
		pCenter->iContinuationCursor = pCenter->iLocalSegmentSeq;
	}

	if ( pCenter->iContinuationCursor > pCenter->iLocalSegmentSeq ) {
		pCenter->iLocalSegmentSeq = pCenter->iContinuationCursor;
	}

	if ( pCenter->iRolloverCursor < pCenter->iContinuationCursor ) {
		pCenter->iRolloverCursor++;
	}

	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}

	(void)procWriteSnapshotState(pCenter->iCommittedRound, sKey, pCenter->iSnapshotIndex, pCenter->iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(pCenter->iCommittedRound, pCenter->iLocalSegmentSeq, sKey, "snapshot_segment_installed");
	(void)procWriteSnapshotInstallStage(
		pCenter->iCommittedRound,
		pCenter->iContinuationCursor,
		sKey,
		pCenter->sInstalledPeer,
		pCenter->iSnapshotIndex);
	(void)procWriteSnapshotRolloverStage(
		pCenter->iCommittedRound,
		pCenter->iRolloverCursor,
		sKey,
		pCenter->iSnapshotIndex);
	(void)procWriteRemoteSnapshotContinuationSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iRolloverCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procBuildSnapshotInstallPath(sInstallPath, sizeof(sInstallPath), pCenter->iCommittedRound, pCenter->iContinuationCursor);
	procCopyText(tInstallStage.sStage, sizeof(tInstallStage.sStage), "inspect_install_stage");
	procCopyText(tInstallStage.sPath, sizeof(tInstallStage.sPath), sInstallPath);
	tInstallStage.iDelayMs = 15u;
	tInstallStage.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tInstallStage, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	procBuildSnapshotRolloverPath(sRolloverPath, sizeof(sRolloverPath), pCenter->iCommittedRound, pCenter->iRolloverCursor);
	procCopyText(tRolloverStage.sStage, sizeof(tRolloverStage.sStage), "inspect_rollover_stage");
	procCopyText(tRolloverStage.sPath, sizeof(tRolloverStage.sPath), sRolloverPath);
	tRolloverStage.iDelayMs = 10u;
	tRolloverStage.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tRolloverStage, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	procCopyText(
		pCenter->sContinuationState,
		sizeof(pCenter->sContinuationState),
		(pCenter->iRolloverCursor >= pCenter->iTargetSegmentSeq) ? "snapshot_continuation_finished" : "snapshot_continuation_running");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"continuation_cursor\": %u,\n\t\"rollover_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"continuation_state\": \"%s\"\n}\n",
		pCenter->sContinuationState,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iLocalSegmentSeq,
		(unsigned)pCenter->iContinuationCursor,
		(unsigned)pCenter->iRolloverCursor,
		(unsigned)pCenter->iTargetSegmentSeq,
		(unsigned)pCenter->iPublishIndex,
		pCenter->sContinuationState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotContinuationCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sContinuationState : "consensus_snapshot_continuation_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iContinuationCursor,
		pCenter->iRolloverCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		bFinished ? 1u : 0u);

	if ( pCenter->iActiveContinuation > 0u ) {
		pCenter->iActiveContinuation--;
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
	DemoSnapshotContinuationCenter tCenter;
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

	tCenter.tPolicy.iContinuationTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 4u;
	tCenter.iContinuationCursor = 4u;
	tCenter.iRolloverCursor = 3u;
	tCenter.iTargetSegmentSeq = 7u;
	tCenter.iPublishIndex = 8u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sContinuationState, sizeof(tCenter.sContinuationState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-continuation-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-continuation-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-continuation-summary.json");

	(void)procSaveConsensusSnapshotContinuationCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledPeer,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iLocalSegmentSeq,
		tCenter.iContinuationCursor,
		tCenter.iRolloverCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotContinuationCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledPeer = xvoTableGetText(vLoaded, "installed_peer", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iContinuationCursor = (uint32)xvoTableGetInt(vLoaded, "continuation_cursor", 0);
	tCenter.iRolloverCursor = (uint32)xvoTableGetInt(vLoaded, "rollover_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sContinuationState, sizeof(tCenter.sContinuationState), (const char*)sState);
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

	sPlan = procChooseSnapshotContinuationPlan(&tCenter, iPeerCount);

	printf("snapshot continuation plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u continuation_cursor=%u rollover_cursor=%u target_segment=%u publish=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iContinuationCursor,
		(unsigned)tCenter.iRolloverCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sInstalledPeer);

	if ( strcmp(sPlan, "continuation_now") == 0 ) {
		if ( !procRunConsensusSnapshotContinuation(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot continuation failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot continuation: snapshot=%u local_segment=%u continuation_cursor=%u rollover_cursor=%u target_segment=%u publish=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iContinuationCursor,
		(unsigned)tCenter.iRolloverCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		tCenter.sContinuationState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot takeover finished` 不等于 `snapshot continuation finished`

这页最容易偷懒的写法是：

- 看到 `snapshot_index >= committed_round` 就认为 continuation 已完成
- 看到 published snapshot 文件已经写出，就认为 install 和 rollover 都已经追平
- 看到某个 peer 给出更深 segment，就直接把它当成本地 install 完成

但更稳的模型应该是：

- `snapshot orchestrated`
	- 回答这轮 snapshot 边界怎样统一推进
- `snapshot continuation finished`
	- 回答多轮 install 和 rollover continuation 是否都已经正式收口

### 6.2 `continuation_cursor` 和 `rollover_cursor` 不是一个字段

这页也很容易写混：

- 把 `local_segment_seq` 直接等同于“当前准备 install 的 segment”
- 或者把 `continuation_cursor` 直接当成“rollover 也已经一起完成了”

但更稳的边界应该是：

- `local_segment_seq`
	- 回答当前本地已经正式落盘到哪一段
- `continuation_cursor`
	- 回答当前 install continuation 已经推进到了哪一段
- `rollover_cursor`
	- 回答当前 rollover continuation 真正追到了哪一段

### 6.3 remote summary 只能更新 takeover 目标，不能偷偷越过本地 rollover

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地马上把 checkpoint 也写成 `local_segment_seq = 6`

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`
- 真正的 `local_segment_seq` 还得等 install stage 和 local rollover 正式走完

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输和校验协议，应继续补更重的 snapshot transport / continuation 主线
- 如果需要把 segment rollover、publish 和更长生命周期的 persistence state 合并进同一条长期主线，应继续补更重的 snapshot persistence / continuation 主线
- 如果需要把 continuation 主线扩展到更完整的多轮 install retry 和 quota 控制，应继续补更重的 snapshot scheduler 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / continuation_cursor / rollover_cursor / target_segment_seq / publish_index / continuation_state`
2. `POST /api/consensus-snapshot-continuation`
	- 只提交 continuation 意图，让 worker 决定是否进入 inspect / gather / install continuation / rollover continuation / publish 主线
3. `GET /api/consensus-snapshot-continuation`
	- 直接返回最近一次 snapshot continuation checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 continuation 的重试和 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 continuation 进度、local segment、target segment、publish 和 recent continuation history

## 9. 下一步阅读

如果你准备继续把 snapshot continuation 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	先回看为什么上一页只回答 snapshot 边界，而这一页开始回答真正的 install 和 rollover
2. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
	对比“snapshot segment takeover”和“unfinished compaction takeover”到底各自接管的是什么
3. [把本地控制台服务升级成一个快照持久化面板](snapshot-persistence-dashboard.md)
	继续把 durable manifest、archive、checkpoint version 和 publish 收成同一个正式 persistence scope
