# 把本地控制台服务升级成一个快照调度配额面板

> 这页要解决的不是“快照持久化已经知道 `manifest_cursor / archive_cursor / checkpoint_version` 了，所以剩下的只是按顺序慢慢跑完”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state`，并且上一页已经能把 durable manifest、archive 和 publish 收成一条正式持久化主线之后，又开始需要回答“当前这一轮到底该立刻跑、进入等待队列，还是延后到下一个 wake 时间”“quota 已满时谁先排队，谁应该 defer”“为什么 `retry_budget` 不能只靠失败次数顺手减一减”“为什么 scheduler state 不能只靠 persistence checkpoint 反推”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在压力变高时靠“谁先请求到、谁就直接抢一个 persistence slot”。 

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local segment 落盘和 published summary 收进同一个 takeover scope
- 快照续传编排怎样把多轮 install、publish 和 checkpoint 更新收成长期主线
- 快照持久化怎样把 `manifest_cursor / archive_cursor / checkpoint_version / publish` 收成 durable baseline

但真实系统再往前走一步，很快又会出现一个新的问题：

- 当前 key-a 正在跑 manifest / archive
- key-b 也想进入 persistence
- 当前 `quota_limit = 1`
- `waiting_queue = 2`
- `retry_budget = 3`
- `next_wake_at` 还没到
- 如果此时再来一波 `POST /api/consensus-snapshot-scheduler`，系统必须知道是 `run_now`、`queue_wait`、`sleep_until_wake`，还是 `defer_retry_budget`

如果这时还把实现停在：

- `if ( archive_cursor < target ) run persistence now;`
- `if ( active < quota ) take next request;`
- `if ( fail ) retry later;`

很快就会出现几个典型问题：

- 请求线程把多个 key 同时推进进 persistence，导致 quota 形同虚设
- 某次失败之后只记得“失败过”，但看不出剩余 `retry_budget` 还够不够
- scheduler 明明应该等到下一轮 sweep，却因为没有正式 `next_wake_at` 而被每个请求都重新触发
- publish 已成功，但 active slot 没有正式释放，后面的 key 永远卡在 queue 里

所以这页真正要补出的，不是“再把 persistence 做重一点”，而是：

> 当 snapshot persistence 已经进入长期运行状态之后，怎样把 `active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` 和真正的 manifest / archive / publish 收口统一进一条可恢复、可解释、可导出的正式调度配额主线。

## 2. 为什么这次不能只靠“有 persistence 就够了”

### 2.1 快照持久化只回答“这一轮 durable 结果怎样正式落地”

上一页的快照持久化已经很好地解决了：

- 本地 snapshot state 和 local segment 怎样 inspect
- remote summary 怎样进入正式 future 主线
- `manifest_cursor / archive_cursor / checkpoint_version / publish_index` 怎样在同一轮里推进

但它不回答：

- 当前 active slot 已满时，新的 key 到底该排队还是直接 defer
- `retry_budget` 用完之前和用完之后，状态字段应该怎样正式区分
- `next_wake_at` 还没到时，请求线程是不是可以强行再拉起一轮 persistence
- 同一条 scheduler 主线里，什么叫“正在跑”，什么叫“等待重试”，什么叫“这轮已经正式结束”

### 2.2 `active_slot`、`waiting_queue`、`retry_budget` 和 `next_wake_at` 不是一个层级

到了这一层，系统真正要稳定回答的是：

- 当前 `active_slot` 还有没有空位
- 当前 `waiting_queue` 里还有多少 key 还没被 admission
- 当前 `retry_budget` 还剩多少
- 当前 `next_wake_at` 到底是不是正式调度边界
- 当前 publish 文件回答的是“这一轮 scheduler 还在跑”，还是“只是 persistence 已经 durable”

这说明：

- `snapshot takeover checkpoint`
	- 回答“一轮 takeover 当前推进到了哪”
- `snapshot persistence checkpoint`
	- 回答“manifest / archive / checkpoint version 当前到底推进到了哪”
- `snapshot scheduler checkpoint`
	- 回答“当前 active slot、waiting queue、retry budget 和 next wake 当前到底卡在哪”

### 2.3 这次真正新增的是“admission + queue + wake + retry budget”状态层

更稳的分工方式是：

- `snapshot takeover`
	- 负责把 remote install、local segment 落盘和 published summary 先接进正式主线
- `snapshot continuation orchestration`
	- 负责把 install gap、publish gap 和 continuation checkpoint 串起来
- `snapshot persistence`
	- 负责 durable manifest、archive、checkpoint version 和 publish 的长期收口
- `snapshot scheduler / quota`
	- 再往下负责 admission、active slot、waiting queue、retry budget 和 wake 时间的长期收口

一句话记住：

> 上一页补的是“结果怎样 durable”，这一页补的是“什么时候轮到你 durable、什么时候该等、什么时候该重试”。 

## 3. 这条快照调度配额主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active scheduler registry | 当前有哪些 key 正在占用 slot，哪些还在等待 |
| `list` | recent scheduler history | 页面和 JSON 展示最近 `run_now / queue_wait / sleep_until_wake / defer` 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有被 admission |
| `queue + thread` | 后台消费 `SCHEDULE / SWEEP` | 请求线程不阻塞在 admission、quota 和 wake 判断上 |
| `task group` | 管住一次 admitted persistence scope 里的 inspect / remote fetch / manifest child / archive child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、manifest stage、archive stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot scheduler summary | 让远端 `target_segment_seq / remote_archive_cursor / remote_publish_index` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local stage、写 manifest stage、写 archive stage、读取 scheduler checkpoint | 让 scheduler 进入正式文件主线 |
| `file_async` | 把 published scheduler summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` 有正式结构 |
| `time` | 记录 scheduler 启动、join、sleep、wake、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `manifest_cursor / archive_cursor / checkpoint_version / active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照持久化管“结果怎样 durable”，快照调度配额管“谁先跑、谁排队、谁等下一轮 wake”。 

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot scheduler 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-scheduler-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-manifest/<round>-<seq>.json
runtime/consensus-snapshot-archive/<round>-<seq>.json
runtime/consensus-snapshot-scheduler-published.json
runtime/remote-consensus-snapshot-scheduler-summary.json
```

其中：

- `config/console.json`
	- 保存 `scheduler_timeout_ms`、`publish_timeout_ms`、`quota_limit`、`retry_budget`、`sweep_interval_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state`
- `runtime/console.log`
	- 记录 scheduler admission、remote summary merge、manifest 推进、archive 推进、wake 判断和 publish
- `runtime/consensus-snapshot-scheduler-checkpoint.json`
	- 保存最近一次正式 snapshot scheduler 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-manifest/<round>-<seq>.json`
	- 保存这轮 manifest 推进的中间文件
- `runtime/consensus-snapshot-archive/<round>-<seq>.json`
	- 保存这轮 archive 推进的中间文件
- `runtime/consensus-snapshot-scheduler-published.json`
	- 保存异步发布后的 scheduler 输出
- `runtime/remote-consensus-snapshot-scheduler-summary.json`
	- 保存这次 remote scheduler summary 的本地 stage 文件

## 5. 先把“启动装载 scheduler checkpoint -> 判断 slot / queue / wake -> admitted 后再推进 manifest / archive / publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-scheduler-checkpoint.json`
2. checkpoint 里明确记录 `manifest_cursor / archive_cursor / checkpoint_version / active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state`
3. `dict` 表示当前 active scheduler registry
4. `list` 表示 scheduler / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 scheduler 意图”
6. `task group` 先管住本次 admitted persistence scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer scheduler summary 走多条 `xhttp future`
9. admission 逻辑先决定 `run_now / queue_wait / sleep_until_wake / defer_retry_budget`
10. admitted 后再统一推进 `manifest_cursor / archive_cursor / local_segment_seq / publish_index / checkpoint_version`
11. 最后再用 `file_async future` 异步发布 committed scheduler 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`manifest_cursor = 4`，`archive_cursor = 3`
- 当前 `quota_limit = 1`、`waiting_queue = 2`、`retry_budget = 3`
- worker 会从 `peer-a`、`peer-b` 拉两份 scheduler summary
- admission 通过后，再把 local inspect、remote summary gather、manifest stage、archive stage 和 publish 收进同一个 scheduler scope
- 最后异步发布 `runtime/consensus-snapshot-scheduler-published.json`

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
	uint32 iSchedulerTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iQuotaLimit;
	uint32 iRetryBudget;
	uint32 iSweepIntervalMs;
} DemoSnapshotSchedulerPolicy;

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
	DemoSnapshotSchedulerPolicy tPolicy;
	uint32 iActiveSlot;
	uint32 iWaitingQueue;
	uint32 iRetryBudget;
	uint32 iNextWakeAt;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iSnapshotIndex;
	uint32 iLocalSegmentSeq;
	uint32 iManifestCursor;
	uint32 iArchiveCursor;
	uint32 iTargetSegmentSeq;
	uint32 iPublishIndex;
	uint32 iCheckpointVersion;
	char sCommittedOwner[32];
	char sInstalledPeer[32];
	char sSchedulerState[48];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotSchedulerCenter;

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

static void procBuildSnapshotManifestPath(char* sDst, size_t iCap, uint32 iRound, uint32 iSegmentSeq)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-manifest/%08u-%04u.json", (unsigned)iRound, (unsigned)iSegmentSeq);
}

static void procBuildSnapshotArchivePath(char* sDst, size_t iCap, uint32 iRound, uint32 iSegmentSeq)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/consensus-snapshot-archive/%08u-%04u.json", (unsigned)iRound, (unsigned)iSegmentSeq);
}

static bool procSaveConsensusSnapshotSchedulerCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledPeer,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iLocalSegmentSeq,
	uint32 iManifestCursor,
	uint32 iArchiveCursor,
	uint32 iTargetSegmentSeq,
	uint32 iPublishIndex,
	uint32 iCheckpointVersion,
	uint32 iActiveSlot,
	uint32 iWaitingQueue,
	uint32 iRetryBudget,
	uint32 iQuotaLimit,
	uint32 iNextWakeAt,
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
	xvoTableSetInt(vRoot, "manifest_cursor", 0, (int32)iManifestCursor);
	xvoTableSetInt(vRoot, "archive_cursor", 0, (int32)iArchiveCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "checkpoint_version", 0, (int32)iCheckpointVersion);
	xvoTableSetInt(vRoot, "active_slot", 0, (int32)iActiveSlot);
	xvoTableSetInt(vRoot, "waiting_queue", 0, (int32)iWaitingQueue);
	xvoTableSetInt(vRoot, "retry_budget", 0, (int32)iRetryBudget);
	xvoTableSetInt(vRoot, "quota_limit", 0, (int32)iQuotaLimit);
	xvoTableSetInt(vRoot, "next_wake_at", 0, (int32)iNextWakeAt);
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

static xvalue procLoadConsensusSnapshotSchedulerCheckpoint(const char* sCheckpointPath)
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

static bool procWriteSnapshotManifestStage(
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

	procBuildSnapshotManifestPath(sPath, sizeof(sPath), iRound, iSegmentSeq);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_manifest_ready", 0, FALSE);
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

static bool procWriteSnapshotArchiveStage(
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

	procBuildSnapshotArchivePath(sPath, sizeof(sPath), iRound, iSegmentSeq);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshot_archive_ready", 0, FALSE);
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

static bool procWriteRemoteSnapshotSchedulerSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iSnapshotIndex,
	uint32 iRemoteSegmentSeq,
	uint32 iRemoteArchiveCursor,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_scheduler_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_segment_seq", 0, (int32)iRemoteSegmentSeq);
	xvoTableSetInt(vRoot, "remote_archive_cursor", 0, (int32)iRemoteArchiveCursor);
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

static bool procPrepareSnapshotSchedulerRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-scheduler/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotSchedulerPlan(const DemoSnapshotSchedulerCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActiveSlot >= pCenter->tPolicy.iQuotaLimit ) {
		return "queue_wait";
	}

	if ( (pCenter->iRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
		return "defer_retry_budget";
	}

	if ( (pCenter->iWaitingQueue > 0u) && (pCenter->iNextWakeAt > (uint32)xrtNow()) ) {
		return "sleep_until_wake";
	}

	if ( (pCenter->iWaitingQueue > 0u) || (pCenter->iManifestCursor < pCenter->iTargetSegmentSeq) || (pCenter->iArchiveCursor < pCenter->iManifestCursor) ) {
		return "run_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotScheduler(
	DemoSnapshotSchedulerCenter* pCenter,
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
	DemoStageTask tArchiveStage;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sStatePath[260];
	char sSegmentPath[260];
	char sInstallPath[260];
	char sArchivePath[260];
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
	memset(&tArchiveStage, 0, sizeof(tArchiveStage));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iActiveSlot++;
	if ( pCenter->iWaitingQueue > 0u ) {
		pCenter->iWaitingQueue--;
	}

	(void)procSaveConsensusSnapshotSchedulerCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_scheduler_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveSlot,
		pCenter->iWaitingQueue,
		pCenter->iRetryBudget,
		pCenter->tPolicy.iQuotaLimit,
		pCenter->iNextWakeAt,
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
		if ( !procPrepareSnapshotSchedulerRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			pCenter->tPolicy.iSchedulerTimeoutMs) ) {
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
			int32 iRemoteArchive = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteSnapshot = xvoTableGetInt(vRemote, "remote_snapshot_index", 0);
				iRemoteSegment = xvoTableGetInt(vRemote, "remote_segment_seq", 0);
				iRemoteTarget = xvoTableGetInt(vRemote, "target_segment_seq", 0);
				iRemotePublish = xvoTableGetInt(vRemote, "remote_publish_index", 0);
				iRemoteArchive = xvoTableGetInt(vRemote, "remote_archive_cursor", 0);

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
				if ( iRemoteArchive > (int32)pCenter->iArchiveCursor ) {
					pCenter->iArchiveCursor = (uint32)iRemoteArchive;
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

	if ( pCenter->iManifestCursor < pCenter->iTargetSegmentSeq ) {
		pCenter->iManifestCursor++;
	} else if ( pCenter->iManifestCursor < pCenter->iLocalSegmentSeq ) {
		pCenter->iManifestCursor = pCenter->iLocalSegmentSeq;
	}

	if ( pCenter->iManifestCursor > pCenter->iLocalSegmentSeq ) {
		pCenter->iLocalSegmentSeq = pCenter->iManifestCursor;
	}

	if ( pCenter->iArchiveCursor < pCenter->iManifestCursor ) {
		pCenter->iArchiveCursor++;
	}

	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}

	pCenter->iCheckpointVersion++;

	(void)procWriteSnapshotState(pCenter->iCommittedRound, sKey, pCenter->iSnapshotIndex, pCenter->iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(pCenter->iCommittedRound, pCenter->iLocalSegmentSeq, sKey, "snapshot_segment_installed");
	(void)procWriteSnapshotManifestStage(
		pCenter->iCommittedRound,
		pCenter->iManifestCursor,
		sKey,
		pCenter->sInstalledPeer,
		pCenter->iSnapshotIndex);
	(void)procWriteSnapshotArchiveStage(
		pCenter->iCommittedRound,
		pCenter->iArchiveCursor,
		sKey,
		pCenter->iSnapshotIndex);
	(void)procWriteRemoteSnapshotSchedulerSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procBuildSnapshotManifestPath(sInstallPath, sizeof(sInstallPath), pCenter->iCommittedRound, pCenter->iManifestCursor);
	procCopyText(tInstallStage.sStage, sizeof(tInstallStage.sStage), "inspect_manifest_stage");
	procCopyText(tInstallStage.sPath, sizeof(tInstallStage.sPath), sInstallPath);
	tInstallStage.iDelayMs = 15u;
	tInstallStage.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tInstallStage, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	procBuildSnapshotArchivePath(sArchivePath, sizeof(sArchivePath), pCenter->iCommittedRound, pCenter->iArchiveCursor);
	procCopyText(tArchiveStage.sStage, sizeof(tArchiveStage.sStage), "inspect_archive_stage");
	procCopyText(tArchiveStage.sPath, sizeof(tArchiveStage.sPath), sArchivePath);
	tArchiveStage.iDelayMs = 10u;
	tArchiveStage.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tArchiveStage, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	if ( pCenter->iArchiveCursor >= pCenter->iTargetSegmentSeq ) {
		pCenter->iRetryBudget = pCenter->tPolicy.iRetryBudget;
		pCenter->iNextWakeAt = 0u;
		procCopyText(pCenter->sSchedulerState, sizeof(pCenter->sSchedulerState), "snapshot_scheduler_finished");
	} else {
		if ( pCenter->iRetryBudget > 0u ) {
			pCenter->iRetryBudget--;
		}
		pCenter->iWaitingQueue++;
		pCenter->iNextWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iSweepIntervalMs;
		procCopyText(pCenter->sSchedulerState, sizeof(pCenter->sSchedulerState), "snapshot_scheduler_running");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"manifest_cursor\": %u,\n\t\"archive_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u,\n\t\"active_slot\": %u,\n\t\"waiting_queue\": %u,\n\t\"retry_budget\": %u,\n\t\"quota_limit\": %u,\n\t\"next_wake_at\": %u,\n\t\"scheduler_state\": \"%s\"\n}\n",
		pCenter->sSchedulerState,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iLocalSegmentSeq,
		(unsigned)pCenter->iManifestCursor,
		(unsigned)pCenter->iArchiveCursor,
		(unsigned)pCenter->iTargetSegmentSeq,
		(unsigned)pCenter->iPublishIndex,
		(unsigned)pCenter->iCheckpointVersion,
		(unsigned)pCenter->iActiveSlot,
		(unsigned)pCenter->iWaitingQueue,
		(unsigned)pCenter->iRetryBudget,
		(unsigned)pCenter->tPolicy.iQuotaLimit,
		(unsigned)pCenter->iNextWakeAt,
		pCenter->sSchedulerState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotSchedulerCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sSchedulerState : "consensus_snapshot_scheduler_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveSlot,
		pCenter->iWaitingQueue,
		pCenter->iRetryBudget,
		pCenter->tPolicy.iQuotaLimit,
		pCenter->iNextWakeAt,
		bFinished ? 1u : 0u);

	if ( pCenter->iActiveSlot > 0u ) {
		pCenter->iActiveSlot--;
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
	DemoSnapshotSchedulerCenter tCenter;
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

	tCenter.tPolicy.iSchedulerTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iQuotaLimit = 1u;
	tCenter.tPolicy.iRetryBudget = 3u;
	tCenter.tPolicy.iSweepIntervalMs = 15u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 4u;
	tCenter.iManifestCursor = 4u;
	tCenter.iArchiveCursor = 3u;
	tCenter.iTargetSegmentSeq = 7u;
	tCenter.iPublishIndex = 8u;
	tCenter.iCheckpointVersion = 11u;
	tCenter.iWaitingQueue = 2u;
	tCenter.iRetryBudget = tCenter.tPolicy.iRetryBudget;
	tCenter.iNextWakeAt = (uint32)xrtNow() - 1u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sSchedulerState, sizeof(tCenter.sSchedulerState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-scheduler-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-scheduler-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-scheduler-summary.json");

	(void)procSaveConsensusSnapshotSchedulerCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledPeer,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iLocalSegmentSeq,
		tCenter.iManifestCursor,
		tCenter.iArchiveCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		tCenter.iCheckpointVersion,
		tCenter.iActiveSlot,
		tCenter.iWaitingQueue,
		tCenter.iRetryBudget,
		tCenter.tPolicy.iQuotaLimit,
		tCenter.iNextWakeAt,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotSchedulerCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledPeer = xvoTableGetText(vLoaded, "installed_peer", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iManifestCursor = (uint32)xvoTableGetInt(vLoaded, "manifest_cursor", 0);
	tCenter.iArchiveCursor = (uint32)xvoTableGetInt(vLoaded, "archive_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iActiveSlot = (uint32)xvoTableGetInt(vLoaded, "active_slot", 0);
	tCenter.iWaitingQueue = (uint32)xvoTableGetInt(vLoaded, "waiting_queue", 0);
	tCenter.iRetryBudget = (uint32)xvoTableGetInt(vLoaded, "retry_budget", 0);
	tCenter.tPolicy.iQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "quota_limit", (int32)tCenter.tPolicy.iQuotaLimit);
	tCenter.iNextWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_wake_at", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sSchedulerState, sizeof(tCenter.sSchedulerState), (const char*)sState);
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

	sPlan = procChooseSnapshotSchedulerPlan(&tCenter, iPeerCount);

	printf("snapshot scheduler plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u active_slot=%u waiting_queue=%u retry_budget=%u next_wake_at=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		(unsigned)tCenter.iActiveSlot,
		(unsigned)tCenter.iWaitingQueue,
		(unsigned)tCenter.iRetryBudget,
		(unsigned)tCenter.iNextWakeAt,
		tCenter.sInstalledPeer);

	if ( strcmp(sPlan, "run_now") == 0 ) {
		if ( !procRunConsensusSnapshotScheduler(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot scheduler failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot scheduler: snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u active_slot=%u waiting_queue=%u retry_budget=%u next_wake_at=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		(unsigned)tCenter.iActiveSlot,
		(unsigned)tCenter.iWaitingQueue,
		(unsigned)tCenter.iRetryBudget,
		(unsigned)tCenter.iNextWakeAt,
		tCenter.sSchedulerState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot persistence finished` 不等于 `snapshot scheduler finished`

这页最容易偷懒的写法是：

- 看到 `archive_cursor >= target_segment_seq` 就认为 scheduler 已完成
- 看到 publish 文件已经写出，就认为 active slot 一定已经释放
- 看到某一轮刚失败过，就直接把整个 key 标成永久 defer

但更稳的模型应该是：

- `snapshot persistence finished`
	- 回答 manifest、archive、checkpoint version 和 publish 是否都已经正式 durable
- `snapshot scheduler finished`
	- 回答当前 active slot、waiting queue、retry budget 和 wake 时间是不是也已经正式收口

### 6.2 `active_slot`、`waiting_queue`、`retry_budget` 和 `next_wake_at` 不是一个字段

这页也很容易写混：

- 把 `active_slot = 0` 直接等同于“没有任何调度工作了”
- 或者把 `waiting_queue > 0` 直接当成“下一轮一定立刻跑”
- 或者把 `retry_budget > 0` 直接当成“现在就能重试”
- 或者把 `next_wake_at` 直接当成“这一轮一定成功”的时间点

但更稳的边界应该是：

- `active_slot`
	- 回答当前还有几个 admitted persistence 正在真正占用资源
- `waiting_queue`
	- 回答当前还有多少 key 已经排队但还没真正进入 persistence scope
- `retry_budget`
	- 回答当前这条 scheduler 主线还允许失败后再被拉起几次
- `next_wake_at`
	- 回答最早什么时候可以合法再看一眼这条主线，而不是被每个请求线程随手打断

### 6.3 remote summary 可以提高优先级，但不能直接绕过 admission

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地就直接把 plan 改成 `run_now`
- 即使 quota 已满、wake 还没到，也照样强行抢 slot

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`、peer 选择和当前优先级判断
- 真正的 `run_now` 还得等 quota、waiting queue 和 next wake 一起通过
- 真正的 slot 释放还得等 admitted scope 正式 close / join / publish 结束
- 真正的下一轮重试还得等 retry budget 和 wake 时间一起满足

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输、校验和断点续拉协议，应继续补更重的 snapshot transport 主线
- 如果需要把 scheduler 扩展到多类 key、多级优先级和不同 quota domain，应继续补更重的 snapshot multi-queue scheduler 主线
- 如果需要把 scheduler 主线扩展到跨节点 admission、共享 retry budget 和全局配额控制，应继续补更重的 distributed scheduler / quota 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state`
2. `POST /api/consensus-snapshot-scheduler`
	- 只提交 scheduler 意图，让 worker 决定是否进入 `run_now / queue_wait / sleep_until_wake / defer_retry_budget`
3. `GET /api/consensus-snapshot-scheduler`
	- 直接返回最近一次 snapshot scheduler checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 waiting queue、wake 判断和 retry budget 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 manifest 进度、archive 进度、checkpoint version、quota、queue、wake 和 recent scheduler history

## 9. 下一步阅读

如果你准备继续把 snapshot scheduler / quota 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	先回看为什么更早的页面只回答 snapshot 边界，而这一页已经开始回答 slot、queue 和 wake
2. [把本地控制台服务升级成一个快照续传编排面板](snapshot-continuation-dashboard.md)
	对比“install / publish 继续推进”和“谁可以先跑、谁必须等下一轮 wake”到底差在哪里
3. [把本地控制台服务升级成一个快照持久化面板](snapshot-persistence-dashboard.md)
	先对比“durable baseline 已经成立”和“scheduler state 已经允许下一轮 admission”到底差在哪里
