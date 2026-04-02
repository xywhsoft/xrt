# 把本地控制台服务升级成一个快照多队列调度面板

> 这页要解决的不是“快照调度已经有 quota、有 wake 时间，所以再多加几个计数器就行了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state`，并且上一页已经能把“单队列 admission + wake + retry budget”收成一条正式调度主线之后，又开始需要回答“高优先级 key 为什么可以抢先 admission，普通队列为什么必须排后”“为什么 high queue 和 normal queue 不能共用同一个 retry budget”“为什么 high wake 和 normal wake 不能偷懒合并成一个时间戳”“为什么 queue state 不能只靠 active slot 反推”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在高优先级流量进来之后靠“谁先抢到 worker 就先跑”。 

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local segment 落盘和 published summary 收进同一个 takeover scope
- 快照续传编排怎样把多轮 install、publish 和 checkpoint 更新收成长期主线
- 快照持久化怎样把 `manifest_cursor / archive_cursor / checkpoint_version / publish` 收成 durable baseline
- 快照调度配额怎样把 `active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` 收成单队列 admission 主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- 当前 key-a 进入高优先级队列，应该尽快 admission
- key-b、key-c 只是普通刷新，不该和 key-a 抢同一条 fast lane
- 当前 `high_quota_limit = 1`、`normal_quota_limit = 1`
- `high_queue = 1`、`normal_queue = 2`
- `next_high_wake_at` 和 `next_normal_wake_at` 不一样
- 如果此时再来一波 `POST /api/consensus-snapshot-multi-queue`，系统必须知道是 `run_high_now`、`high_queue_wait`、`sleep_high_wake`、`run_normal_now`、`normal_queue_wait`，还是 `defer_normal_budget`

如果这时还把实现停在：

- `if ( queue > 0 ) run next;`
- `if ( active < quota ) admit;`
- `if ( fail ) put back to queue;`

很快就会出现几个典型问题：

- 高优先级 key 无法稳定抢先，普通流量一多就把关键恢复挤掉
- 所有队列共用一套 retry budget，导致 high queue 和 normal queue 互相污染
- high wake 和 normal wake 混成一个字段，结果高优先级流量被普通队列的冷却时间拖住
- publish 已成功，但没有明确写出本次 admitted 的到底是哪条队列，排障时只能猜

所以这页真正要补出的，不是“继续把 quota 再加重一点”，而是：

> 当 snapshot scheduler 已经进入长期运行状态之后，怎样把 `active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state` 和真正的 manifest / archive / publish 收口统一进一条可恢复、可解释、可导出的正式多队列调度主线。

## 2. 为什么这次不能只靠“有 persistence 就够了”

### 2.1 单队列调度只回答“这一条 admission 线怎样收口”

上一页的快照调度配额已经很好地解决了：

- 当前 admitted scope 怎样拿到 slot
- 单条 waiting queue 怎样和 wake 时间一起收口
- `retry_budget / next_wake_at / scheduler_state` 怎样写回 checkpoint

但它不回答：

- 高优先级恢复和普通刷新为什么不能塞进同一条 waiting queue
- high queue 满了时，normal queue 到底能不能先跑
- 为什么 high queue 和 normal queue 的 wake 时间不能共用一个字段
- 为什么 admission 结果里必须明确写出“本次是 high 还是 normal”

### 2.2 `active_high_slot / active_normal_slot / high_queue / normal_queue` 不是同一类字段

到了这一层，系统真正要稳定回答的是：

- 当前 `active_high_slot` 还有没有空位
- 当前 `active_normal_slot` 还有没有空位
- 当前 `high_queue` 里还有多少关键 key 没被 admission
- 当前 `normal_queue` 里还有多少普通 key 还在排队
- 当前 `defer_queue` 到底是预算耗尽，还是本轮故意不 admission

这说明：

- `snapshot persistence checkpoint`
	- 回答 manifest / archive / checkpoint version 当前推进到了哪
- `snapshot scheduler checkpoint`
	- 回答单条 admission / retry / wake 当前推进到了哪
- `snapshot multi-queue checkpoint`
	- 回答 high queue、normal queue、defer queue 和双 slot / 双 wake 当前到底卡在哪

### 2.3 这次真正新增的是“双队列 admission + 双 wake + 双 retry budget”状态层

更稳的分工方式是：

- `snapshot persistence`
	- 负责 durable manifest、archive、checkpoint version 和 publish 的长期收口
- `snapshot scheduler / quota`
	- 负责单条 waiting queue、单条 slot、单条 wake 的 admission 收口
- `snapshot multi-queue scheduler`
	- 再往下负责 high queue / normal queue / defer queue、双 quota、双 wake 和双 retry budget 的长期收口

一句话记住：

> 上一页补的是“这一条 admission 线怎样跑”，这一页补的是“多条 admission 线怎样稳定分流，不互相踩踏”。 

## 3. 这条快照多队列调度主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active multi-queue registry | 当前有哪些 key 正在 high slot、normal slot、defer queue 上 |
| `list` | recent scheduler history | 页面和 JSON 展示最近 `run_high_now / high_queue_wait / run_normal_now / normal_queue_wait / defer` 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有被 admission |
| `queue + thread` | 后台消费 `ENQUEUE / SWEEP` | 请求线程不阻塞在多队列 admission、quota 和 wake 判断上 |
| `task group` | 管住一次 admitted persistence scope 里的 inspect / remote fetch / manifest child / archive child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、manifest stage、archive stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot multi-queue summary | 让远端 `target_segment_seq / remote_archive_cursor / remote_publish_index` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local stage、写 manifest stage、写 archive stage、读取 multi-queue checkpoint | 让 scheduler 进入正式文件主线 |
| `file_async` | 把 published multi-queue summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state` 有正式结构 |
| `time` | 记录 high queue、normal queue、sleep、wake、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `manifest_cursor / archive_cursor / checkpoint_version / active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / next_high_wake_at / next_normal_wake_at / queue_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照调度配额管“单队列 admission 怎样收口”，快照多队列调度管“高优先级和普通流量怎样稳定分层，不互相拖累”。 

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot multi-queue 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-multi-queue-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-manifest/<round>-<seq>.json
runtime/consensus-snapshot-archive/<round>-<seq>.json
runtime/consensus-snapshot-multi-queue-published.json
runtime/remote-consensus-snapshot-multi-queue-summary.json
```

其中：

- `config/console.json`
	- 保存 `high_queue_timeout_ms`、`normal_queue_timeout_ms`、`high_quota_limit`、`normal_quota_limit`、`high_retry_budget`、`normal_retry_budget`、`high_sweep_interval_ms`、`normal_sweep_interval_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state`
- `runtime/console.log`
	- 记录 high / normal admission、remote summary merge、manifest 推进、archive 推进、双 wake 判断和 publish
- `runtime/consensus-snapshot-multi-queue-checkpoint.json`
	- 保存最近一次正式 snapshot multi-queue 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-manifest/<round>-<seq>.json`
	- 保存这轮 manifest 推进的中间文件
- `runtime/consensus-snapshot-archive/<round>-<seq>.json`
	- 保存这轮 archive 推进的中间文件
- `runtime/consensus-snapshot-multi-queue-published.json`
	- 保存异步发布后的 multi-queue 输出
- `runtime/remote-consensus-snapshot-multi-queue-summary.json`
	- 保存这次 remote multi-queue summary 的本地 stage 文件

## 5. 先把“启动装载 multi-queue checkpoint -> 判断 high / normal admission -> admitted 后再推进 manifest / archive / publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-multi-queue-checkpoint.json`
2. checkpoint 里明确记录 `manifest_cursor / archive_cursor / checkpoint_version / active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state`
3. `dict` 表示当前 active multi-queue registry
4. `list` 表示 multi-queue / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 multi-queue admission 意图”
6. `task group` 先管住本次 admitted persistence scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer multi-queue summary 走多条 `xhttp future`
9. admission 逻辑先决定 `run_high_now / high_queue_wait / sleep_high_wake / run_normal_now / normal_queue_wait / sleep_normal_wake / defer_high_budget / defer_normal_budget`
10. admitted 后再统一推进 `manifest_cursor / archive_cursor / local_segment_seq / publish_index / checkpoint_version`
11. 最后再用 `file_async future` 异步发布 committed multi-queue 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`manifest_cursor = 4`，`archive_cursor = 3`
- 当前 `high_quota_limit = 1`、`normal_quota_limit = 1`
- 当前 `high_queue = 1`、`normal_queue = 2`
- worker 会从 `peer-a`、`peer-b` 拉两份 multi-queue summary
- admission 通过后，再把 local inspect、remote summary gather、manifest stage、archive stage 和 publish 收进同一个 multi-queue scope
- 最后异步发布 `runtime/consensus-snapshot-multi-queue-published.json`

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
	uint32 iHighQueueTimeoutMs;
	uint32 iNormalQueueTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iHighQuotaLimit;
	uint32 iNormalQuotaLimit;
	uint32 iHighRetryBudget;
	uint32 iNormalRetryBudget;
	uint32 iHighSweepIntervalMs;
	uint32 iNormalSweepIntervalMs;
} DemoSnapshotMultiQueuePolicy;

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
	DemoSnapshotMultiQueuePolicy tPolicy;
	uint32 iActiveHighSlot;
	uint32 iActiveNormalSlot;
	uint32 iHighQueue;
	uint32 iNormalQueue;
	uint32 iDeferQueue;
	uint32 iHighRetryBudget;
	uint32 iNormalRetryBudget;
	uint32 iNextHighWakeAt;
	uint32 iNextNormalWakeAt;
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
	char sQueueState[48];
	char sSelectedQueue[16];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotMultiQueueCenter;

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

static bool procSaveConsensusSnapshotMultiQueueCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledPeer,
	const char* sSelectedQueue,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iLocalSegmentSeq,
	uint32 iManifestCursor,
	uint32 iArchiveCursor,
	uint32 iTargetSegmentSeq,
	uint32 iPublishIndex,
	uint32 iCheckpointVersion,
	uint32 iActiveHighSlot,
	uint32 iActiveNormalSlot,
	uint32 iHighQueue,
	uint32 iNormalQueue,
	uint32 iDeferQueue,
	uint32 iHighRetryBudget,
	uint32 iNormalRetryBudget,
	uint32 iHighQuotaLimit,
	uint32 iNormalQuotaLimit,
	uint32 iNextHighWakeAt,
	uint32 iNextNormalWakeAt,
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
	xvoTableSetText(vRoot, "selected_queue", 0, (uint8*)((sSelectedQueue != NULL) ? sSelectedQueue : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "local_segment_seq", 0, (int32)iLocalSegmentSeq);
	xvoTableSetInt(vRoot, "manifest_cursor", 0, (int32)iManifestCursor);
	xvoTableSetInt(vRoot, "archive_cursor", 0, (int32)iArchiveCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "checkpoint_version", 0, (int32)iCheckpointVersion);
	xvoTableSetInt(vRoot, "active_high_slot", 0, (int32)iActiveHighSlot);
	xvoTableSetInt(vRoot, "active_normal_slot", 0, (int32)iActiveNormalSlot);
	xvoTableSetInt(vRoot, "high_queue", 0, (int32)iHighQueue);
	xvoTableSetInt(vRoot, "normal_queue", 0, (int32)iNormalQueue);
	xvoTableSetInt(vRoot, "defer_queue", 0, (int32)iDeferQueue);
	xvoTableSetInt(vRoot, "high_retry_budget", 0, (int32)iHighRetryBudget);
	xvoTableSetInt(vRoot, "normal_retry_budget", 0, (int32)iNormalRetryBudget);
	xvoTableSetInt(vRoot, "high_quota_limit", 0, (int32)iHighQuotaLimit);
	xvoTableSetInt(vRoot, "normal_quota_limit", 0, (int32)iNormalQuotaLimit);
	xvoTableSetInt(vRoot, "next_high_wake_at", 0, (int32)iNextHighWakeAt);
	xvoTableSetInt(vRoot, "next_normal_wake_at", 0, (int32)iNextNormalWakeAt);
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

static xvalue procLoadConsensusSnapshotMultiQueueCheckpoint(const char* sCheckpointPath)
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

static bool procWriteRemoteSnapshotMultiQueueSummaryStage(
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_multi_queue_summary_ready", 0, FALSE);
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

static bool procPrepareSnapshotMultiQueueRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-multi-queue/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotMultiQueuePlan(const DemoSnapshotMultiQueueCenter* pCenter, int iPeerCount)
{
	uint32 iNow = (uint32)xrtNow();

	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iHighQueue > 0u ) {
		if ( pCenter->iActiveHighSlot >= pCenter->tPolicy.iHighQuotaLimit ) {
			return "high_queue_wait";
		}

		if ( (pCenter->iHighRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_high_budget";
		}

		if ( pCenter->iNextHighWakeAt > iNow ) {
			return "sleep_high_wake";
		}

		return "run_high_now";
	}

	if ( pCenter->iNormalQueue > 0u ) {
		if ( pCenter->iActiveNormalSlot >= pCenter->tPolicy.iNormalQuotaLimit ) {
			return "normal_queue_wait";
		}

		if ( (pCenter->iNormalRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_normal_budget";
		}

		if ( pCenter->iNextNormalWakeAt > iNow ) {
			return "sleep_normal_wake";
		}

		return "run_normal_now";
	}

	if ( (pCenter->iManifestCursor < pCenter->iTargetSegmentSeq) || (pCenter->iArchiveCursor < pCenter->iManifestCursor) ) {
		return "run_normal_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotMultiQueue(
	DemoSnapshotMultiQueueCenter* pCenter,
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
	char sPublishJson[1200];
	int iChildCount = 0;
	int i;
	bool bHighQueue = false;
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

	bHighQueue = (pCenter->iHighQueue > 0u);
	procCopyText(pCenter->sSelectedQueue, sizeof(pCenter->sSelectedQueue), bHighQueue ? "high" : "normal");

	if ( bHighQueue ) {
		pCenter->iActiveHighSlot++;
		if ( pCenter->iHighQueue > 0u ) {
			pCenter->iHighQueue--;
		}
	} else {
		pCenter->iActiveNormalSlot++;
		if ( pCenter->iNormalQueue > 0u ) {
			pCenter->iNormalQueue--;
		}
	}

	(void)procSaveConsensusSnapshotMultiQueueCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_multi_queue_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->sSelectedQueue,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveHighSlot,
		pCenter->iActiveNormalSlot,
		pCenter->iHighQueue,
		pCenter->iNormalQueue,
		pCenter->iDeferQueue,
		pCenter->iHighRetryBudget,
		pCenter->iNormalRetryBudget,
		pCenter->tPolicy.iHighQuotaLimit,
		pCenter->tPolicy.iNormalQuotaLimit,
		pCenter->iNextHighWakeAt,
		pCenter->iNextNormalWakeAt,
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
		if ( !procPrepareSnapshotMultiQueueRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			bHighQueue ? pCenter->tPolicy.iHighQueueTimeoutMs : pCenter->tPolicy.iNormalQueueTimeoutMs) ) {
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
	(void)procWriteRemoteSnapshotMultiQueueSummaryStage(
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
		pCenter->iHighRetryBudget = pCenter->tPolicy.iHighRetryBudget;
		pCenter->iNormalRetryBudget = pCenter->tPolicy.iNormalRetryBudget;
		pCenter->iNextHighWakeAt = 0u;
		pCenter->iNextNormalWakeAt = 0u;
		procCopyText(pCenter->sQueueState, sizeof(pCenter->sQueueState), "snapshot_multi_queue_finished");
	} else {
		if ( bHighQueue ) {
			if ( pCenter->iHighRetryBudget > 0u ) {
				pCenter->iHighRetryBudget--;
			}
			pCenter->iHighQueue++;
			pCenter->iNextHighWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iHighSweepIntervalMs;
		} else {
			if ( pCenter->iNormalRetryBudget > 0u ) {
				pCenter->iNormalRetryBudget--;
			}
			pCenter->iNormalQueue++;
			pCenter->iNextNormalWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iNormalSweepIntervalMs;
		}
		pCenter->iDeferQueue++;
		procCopyText(pCenter->sQueueState, sizeof(pCenter->sQueueState), "snapshot_multi_queue_running");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"selected_queue\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"manifest_cursor\": %u,\n\t\"archive_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u,\n\t\"active_high_slot\": %u,\n\t\"active_normal_slot\": %u,\n\t\"high_queue\": %u,\n\t\"normal_queue\": %u,\n\t\"defer_queue\": %u,\n\t\"high_retry_budget\": %u,\n\t\"normal_retry_budget\": %u,\n\t\"high_quota_limit\": %u,\n\t\"normal_quota_limit\": %u,\n\t\"next_high_wake_at\": %u,\n\t\"next_normal_wake_at\": %u,\n\t\"queue_state\": \"%s\"\n}\n",
		pCenter->sQueueState,
		pCenter->sSelectedQueue,
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
		(unsigned)pCenter->iActiveHighSlot,
		(unsigned)pCenter->iActiveNormalSlot,
		(unsigned)pCenter->iHighQueue,
		(unsigned)pCenter->iNormalQueue,
		(unsigned)pCenter->iDeferQueue,
		(unsigned)pCenter->iHighRetryBudget,
		(unsigned)pCenter->iNormalRetryBudget,
		(unsigned)pCenter->tPolicy.iHighQuotaLimit,
		(unsigned)pCenter->tPolicy.iNormalQuotaLimit,
		(unsigned)pCenter->iNextHighWakeAt,
		(unsigned)pCenter->iNextNormalWakeAt,
		pCenter->sQueueState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotMultiQueueCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sQueueState : "consensus_snapshot_multi_queue_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->sSelectedQueue,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveHighSlot,
		pCenter->iActiveNormalSlot,
		pCenter->iHighQueue,
		pCenter->iNormalQueue,
		pCenter->iDeferQueue,
		pCenter->iHighRetryBudget,
		pCenter->iNormalRetryBudget,
		pCenter->tPolicy.iHighQuotaLimit,
		pCenter->tPolicy.iNormalQuotaLimit,
		pCenter->iNextHighWakeAt,
		pCenter->iNextNormalWakeAt,
		bFinished ? 1u : 0u);

	if ( bHighQueue ) {
		if ( pCenter->iActiveHighSlot > 0u ) {
			pCenter->iActiveHighSlot--;
		}
	} else {
		if ( pCenter->iActiveNormalSlot > 0u ) {
			pCenter->iActiveNormalSlot--;
		}
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
	DemoSnapshotMultiQueueCenter tCenter;
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

	tCenter.tPolicy.iHighQueueTimeoutMs = 3000u;
	tCenter.tPolicy.iNormalQueueTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iHighQuotaLimit = 1u;
	tCenter.tPolicy.iNormalQuotaLimit = 1u;
	tCenter.tPolicy.iHighRetryBudget = 2u;
	tCenter.tPolicy.iNormalRetryBudget = 3u;
	tCenter.tPolicy.iHighSweepIntervalMs = 10u;
	tCenter.tPolicy.iNormalSweepIntervalMs = 20u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 4u;
	tCenter.iManifestCursor = 4u;
	tCenter.iArchiveCursor = 3u;
	tCenter.iTargetSegmentSeq = 7u;
	tCenter.iPublishIndex = 8u;
	tCenter.iCheckpointVersion = 11u;
	tCenter.iHighQueue = 1u;
	tCenter.iNormalQueue = 2u;
	tCenter.iHighRetryBudget = tCenter.tPolicy.iHighRetryBudget;
	tCenter.iNormalRetryBudget = tCenter.tPolicy.iNormalRetryBudget;
	tCenter.iNextHighWakeAt = (uint32)xrtNow() - 1u;
	tCenter.iNextNormalWakeAt = (uint32)xrtNow() + 30u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sQueueState, sizeof(tCenter.sQueueState), "boot");
	procCopyText(tCenter.sSelectedQueue, sizeof(tCenter.sSelectedQueue), "high");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-multi-queue-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-multi-queue-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-multi-queue-summary.json");

	(void)procSaveConsensusSnapshotMultiQueueCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledPeer,
		tCenter.sSelectedQueue,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iLocalSegmentSeq,
		tCenter.iManifestCursor,
		tCenter.iArchiveCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		tCenter.iCheckpointVersion,
		tCenter.iActiveHighSlot,
		tCenter.iActiveNormalSlot,
		tCenter.iHighQueue,
		tCenter.iNormalQueue,
		tCenter.iDeferQueue,
		tCenter.iHighRetryBudget,
		tCenter.iNormalRetryBudget,
		tCenter.tPolicy.iHighQuotaLimit,
		tCenter.tPolicy.iNormalQuotaLimit,
		tCenter.iNextHighWakeAt,
		tCenter.iNextNormalWakeAt,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotMultiQueueCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledPeer = xvoTableGetText(vLoaded, "installed_peer", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	procCopyText(tCenter.sSelectedQueue, sizeof(tCenter.sSelectedQueue), (const char*)xvoTableGetText(vLoaded, "selected_queue", 0));
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iManifestCursor = (uint32)xvoTableGetInt(vLoaded, "manifest_cursor", 0);
	tCenter.iArchiveCursor = (uint32)xvoTableGetInt(vLoaded, "archive_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iActiveHighSlot = (uint32)xvoTableGetInt(vLoaded, "active_high_slot", 0);
	tCenter.iActiveNormalSlot = (uint32)xvoTableGetInt(vLoaded, "active_normal_slot", 0);
	tCenter.iHighQueue = (uint32)xvoTableGetInt(vLoaded, "high_queue", 0);
	tCenter.iNormalQueue = (uint32)xvoTableGetInt(vLoaded, "normal_queue", 0);
	tCenter.iDeferQueue = (uint32)xvoTableGetInt(vLoaded, "defer_queue", 0);
	tCenter.iHighRetryBudget = (uint32)xvoTableGetInt(vLoaded, "high_retry_budget", 0);
	tCenter.iNormalRetryBudget = (uint32)xvoTableGetInt(vLoaded, "normal_retry_budget", 0);
	tCenter.tPolicy.iHighQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "high_quota_limit", (int32)tCenter.tPolicy.iHighQuotaLimit);
	tCenter.tPolicy.iNormalQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "normal_quota_limit", (int32)tCenter.tPolicy.iNormalQuotaLimit);
	tCenter.iNextHighWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_high_wake_at", 0);
	tCenter.iNextNormalWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_normal_wake_at", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sQueueState, sizeof(tCenter.sQueueState), (const char*)sState);
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

	sPlan = procChooseSnapshotMultiQueuePlan(&tCenter, iPeerCount);

	printf("snapshot multi-queue plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_queue=%s active_high=%u active_normal=%u high_queue=%u normal_queue=%u defer_queue=%u high_retry=%u normal_retry=%u next_high=%u next_normal=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sSelectedQueue,
		(unsigned)tCenter.iActiveHighSlot,
		(unsigned)tCenter.iActiveNormalSlot,
		(unsigned)tCenter.iHighQueue,
		(unsigned)tCenter.iNormalQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iHighRetryBudget,
		(unsigned)tCenter.iNormalRetryBudget,
		(unsigned)tCenter.iNextHighWakeAt,
		(unsigned)tCenter.iNextNormalWakeAt,
		tCenter.sInstalledPeer);

	if ( (strcmp(sPlan, "run_high_now") == 0) || (strcmp(sPlan, "run_normal_now") == 0) ) {
		if ( !procRunConsensusSnapshotMultiQueue(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot multi-queue failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot multi-queue: snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_queue=%s active_high=%u active_normal=%u high_queue=%u normal_queue=%u defer_queue=%u high_retry=%u normal_retry=%u next_high=%u next_normal=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sSelectedQueue,
		(unsigned)tCenter.iActiveHighSlot,
		(unsigned)tCenter.iActiveNormalSlot,
		(unsigned)tCenter.iHighQueue,
		(unsigned)tCenter.iNormalQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iHighRetryBudget,
		(unsigned)tCenter.iNormalRetryBudget,
		(unsigned)tCenter.iNextHighWakeAt,
		(unsigned)tCenter.iNextNormalWakeAt,
		tCenter.sQueueState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot scheduler finished` 不等于 `snapshot multi-queue finished`

这页最容易偷懒的写法是：

- 看到 `archive_cursor >= target_segment_seq` 就认为 multi-queue 已完成
- 看到 publish 文件已经写出，就认为 high / normal slot 一定都已经释放
- 看到某一轮刚失败过，就直接把整个 key 标成永久 defer

但更稳的模型应该是：

- `snapshot persistence finished`
	- 回答 manifest、archive、checkpoint version 和 publish 是否都已经正式 durable
- `snapshot scheduler finished`
	- 回答单队列 admission、wake 和 retry budget 是不是已经正式收口
- `snapshot multi-queue finished`
	- 回答当前 high queue / normal queue / defer queue 以及双 slot / 双 wake 是不是也已经正式收口

### 6.2 `active_high_slot / active_normal_slot / high_queue / normal_queue` 不是一个字段

这页也很容易写混：

- 把 `active_high_slot = 0` 直接等同于“没有任何高优先级压力了”
- 或者把 `normal_queue > 0` 直接当成“普通流量一定马上能跑”
- 或者把 `high_retry_budget > 0` 直接当成“normal queue 也能跟着重试”
- 或者把 `next_high_wake_at` 和 `next_normal_wake_at` 直接当成同一个冷却时间

但更稳的边界应该是：

- `active_high_slot`
	- 回答当前 high queue 还有几个 admitted scope 正在真正占用资源
- `active_normal_slot`
	- 回答当前 normal queue 还有几个 admitted scope 正在真正占用资源
- `high_queue / normal_queue`
	- 回答不同优先级队列里到底还有多少 key 等待 admission
- `high_retry_budget / normal_retry_budget`
	- 回答不同优先级队列当前还允许失败后再被拉起几次
- `next_high_wake_at / next_normal_wake_at`
	- 回答不同优先级队列最早什么时候可以合法再看一眼，而不是被彼此的冷却时间拖住

### 6.3 remote summary 可以提高优先级，但不能直接绕过 admission

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地就直接把 plan 改成 `run_high_now`
- 即使 high queue quota 已满、normal queue 还排着，也照样强行抢 slot

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`、peer 选择和当前优先级判断
- 真正的 `run_high_now / run_normal_now` 还得等对应队列的 quota、queue 长度和 wake 时间一起通过
- 真正的 slot 释放还得等 admitted scope 正式 close / join / publish 结束
- 真正的下一轮重试还得等对应队列的 retry budget 和 wake 时间一起满足

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输、校验和断点续拉协议，应继续补更重的 snapshot transport 主线
- 如果需要把 scheduler 扩展到多类 key、多级优先级和不同 quota domain，应继续补更重的 snapshot multi-queue scheduler 主线
- 如果需要把 scheduler 主线扩展到跨节点 admission、共享 retry budget 和全局配额控制，应继续补更重的 distributed scheduler / quota 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_high_slot / active_normal_slot / high_queue / normal_queue / defer_queue / high_retry_budget / normal_retry_budget / next_high_wake_at / next_normal_wake_at / queue_state`
2. `POST /api/consensus-snapshot-multi-queue`
	- 只提交 multi-queue admission 意图，让 worker 决定是否进入 `run_high_now / high_queue_wait / sleep_high_wake / run_normal_now / normal_queue_wait / sleep_normal_wake / defer_high_budget / defer_normal_budget`
3. `GET /api/consensus-snapshot-multi-queue`
	- 直接返回最近一次 snapshot multi-queue checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 high / normal queue 的 wake 判断、retry budget 刷新和 defer 迁移
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 manifest 进度、archive 进度、checkpoint version、high / normal quota、双队列长度、双 wake 和 recent scheduler history

## 9. 下一步阅读

如果你准备继续把 snapshot multi-queue scheduler 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	先回看为什么更早的页面只回答 snapshot 边界，而这一页已经开始回答 high / normal admission
2. [把本地控制台服务升级成一个快照续传编排面板](snapshot-continuation-dashboard.md)
	对比“install / publish 继续推进”和“高优先级 / 普通队列怎样分层 admission”到底差在哪里
3. [把本地控制台服务升级成一个快照分布式调度配额面板](snapshot-distributed-scheduler-quota-dashboard.md)
	先对比“本机双域 admission 已经成立”和“跨节点 shared quota / peer budget 已经允许下一轮 admission”到底差在哪里

