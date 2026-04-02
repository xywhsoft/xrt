# 把本地控制台服务升级成一个 lease journal compaction 与 ownership migration replay 面板

> 这页要解决的，不再是“当前 owner 迁移有没有被 durable journal 正式记住”这么简单，而是更贴近长期运行控制面的难题：当一个本地控制台服务已经同时积累了 `current_owner / target_owner / lease_epoch / ownership_epoch / journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / replay_ack_count / replay_reject_count / active_compaction_slot / active_replay_slot / shared_lease_in_use / granted_lease_window / journal_rotate_queue / replay_queue / audit_queue / compaction_retry_budget / replay_retry_budget / next_compaction_wake_at / next_replay_wake_at / compaction_state / witness_cluster`，并且上一页已经能把“owner 迁移怎样写进 durable journal 并完成审计”收成正式状态之后，又开始需要回答“旧 journal 什么时候能正式裁剪”“owner replay 为什么不能只看 publish 文件”“为什么 compaction 和 replay 不能共用同一个 cursor”“为什么 replay cursor 没补齐时不能直接宣布新的 current owner 已经稳定”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续靠“重启时顺着日志试试看”来赌系统能不能恢复到一致状态。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 快照租约协调怎样回答“当前 lease 怎样续租或接管”
- lease journal 与 ownership migration 怎样回答“owner 迁移怎样写进 durable journal、怎样被审计和发布”

但真实系统再往前走一步，很快又会出现一个新的问题：

- 当前 `current_owner = node-c`
- 这一轮仍然保留着 `target_owner = node-c`，说明迁移已经发生过
- `lease_epoch = 15`、`ownership_epoch = 10`
- `journal_segment_seq = 36`
- `compacted_to_seq = 24`
- `replay_from_seq = 25`、`replay_to_seq = 36`
- `replay_cursor = 29`
- `replay_ticket = 44`
- `replay_ack_count = 0`、`replay_reject_count = 0`
- `active_compaction_slot = 1`、`active_replay_slot = 0`
- `shared_lease_in_use = 1`、`granted_lease_window = 2`
- `journal_rotate_queue = 1`、`replay_queue = 2`、`audit_queue = 1`
- `compaction_retry_budget = 3`、`replay_retry_budget = 2`
- `next_compaction_wake_at` 和 `next_replay_wake_at` 并不相同

如果这时再来一波 `POST /api/lease-journal-compaction-migration-replay`，系统必须知道这次应该是：

- `append_compaction_marker_now`
- `wait_replay_quorum`
- `queue_granted_lease_window`
- `queue_compaction_window`
- `run_compaction_now`
- `queue_replay_window`
- `run_replay_now`
- `sleep_compaction_wake`
- `sleep_replay_wake`
- `defer_compaction_budget`
- `defer_replay_budget`
- 还是 `publish_compacted_journal_now`

如果这时还把实现停在：

- `if ( file_size > threshold ) rotate;`
- `if ( replay_cursor < tail ) keep replaying;`
- `if ( publish ok ) assume replay done;`

很快就会出现几个典型问题：

- journal segment 已经切了新文件，但 `compacted_to_seq` 根本没有正式写回 checkpoint
- replay cursor 还没追平，系统却已经对外宣布新的 current owner 完全稳定
- `replay_ack_count / replay_reject_count` 和 `replay_retry_budget` 混成一种失败语义，结果“票没过”和“预算没了”被写成同一类 defer
- published summary 看起来已经很新，但 replay 真正从哪一条 journal entry 开始补、补到了哪一条，根本没人知道

所以这页真正要补出的，不是“再多写一层 replay 日志”，而是：

> 当 lease journal 已经进入分段、裁剪、回放和 published summary 长期并存的状态之后，怎样把 compaction、replay、audit 和 owner 稳态统一收成一条可恢复、可追溯、可裁剪、可发布的正式主线。

## 2. 为什么这次不能只靠“journal 已存在”就够了

### 2.1 durable journal 不等于 durable replay

上一页的 lease journal 与 ownership migration 已经很好地解决了：

- owner 迁移怎样写进 durable journal
- migration quorum 怎样被正式记录
- audit export 怎样解释这一轮迁移到底发生了什么

但它不回答：

- 哪些旧 journal entry 已经被 compaction 正式吸收
- 当前 replay 到底该从哪一条开始
- replay cursor 到底已经补到哪一条
- 为什么 `current_owner == target_owner` 也不代表 replay 已经收口

### 2.2 `journal_segment_seq / compacted_to_seq / replay_cursor / ownership_epoch` 不是一类字段

到了这一层，系统真正要稳定回答的是：

- 当前 `journal_segment_seq` 已经切到了哪个 segment
- 当前 `compacted_to_seq` 到底已经正式吸收到了哪一条
- 当前 `replay_from_seq / replay_to_seq / replay_cursor` 到底已经推进到了哪
- 当前 `replay_ticket` 属于哪一轮 replay 协调
- 当前 `ownership_epoch` 到底已经在哪一轮 owner 语义上稳定
- 当前 `journal_rotate_queue / replay_queue / audit_queue` 分别还积压了多少工作
- 当前 `compaction_retry_budget / replay_retry_budget` 和 `next_compaction_wake_at / next_replay_wake_at` 到底怎么影响下一轮动作

这说明：

- `lease journal checkpoint`
	- 回答 owner 迁移和 journal append 当前推进到了哪
- `lease journal compaction checkpoint`
	- 回答旧 journal 已经被正式吸收到哪一条
- `ownership migration replay checkpoint`
	- 回答 replay 该从哪开始、补到哪、哪一轮 replay 才算真正收口

### 2.3 这次真正新增的是“compaction window + replay cursor + replay publish”状态层

更稳的分工方式是：

- `lease journal + ownership migration`
	- 负责 durable owner 迁移、migration audit 和 published owner summary 的长期收口
- `lease journal compaction + ownership migration replay`
	- 再往下负责 `journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / replay_ack_count / replay_reject_count / compaction_retry_budget / replay_retry_budget`，以及 compaction、replay 和 replay publish 的正式收口

一句话记住：

> 上一页补的是“owner 迁移怎样被 durable journal 正式接住”，这一页补的是“旧 journal 怎样被正式裁剪，以及新 owner 怎样把历史 journal replay 到稳定状态”。

## 3. 这条 lease journal compaction 与 ownership migration replay 主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active replay registry | 当前有哪些 key 正在 compaction、replay、audit queue 和 owner 稳态上 |
| `list` | recent compaction history | 页面和 JSON 展示最近 `append_compaction_marker_now / run_compaction_now / publish_compacted_journal_now` 结果 |
| `list` | recent replay history | 页面和 JSON 展示最近 `wait_replay_quorum / run_replay_now / defer_replay_budget` 结果 |
| `queue + thread` | 后台消费 `ROTATE / COMPACT / REPLAY / SWEEP` | 请求线程不阻塞在 journal 裁剪、replay gather 和 wake 判断上 |
| `task group` | 管住一次 admitted compaction scope 里的 inspect / remote fetch / compact child / replay child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote replay summary、compaction stage、replay stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 ownership migration replay summary | 让远端 `remote_replay_to_seq / remote_replay_cursor / remote_publish_index / remote_ownership_epoch` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 current journal、写 segment journal、写 replay audit、写 published stage | 让 compaction 与 replay 进入正式文件主线 |
| `file_async` | 把 published replay summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、published JSON | 让 `journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / replay_ack_count / replay_reject_count` 有正式结构 |
| `time` | 记录 compaction wake、replay wake、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 segment、compacted range、replay range、replay cursor、retry budget 和 published summary | 浏览器和脚本共享同一份状态面 |

一句话记住：

> lease journal 与 ownership migration 管“迁移怎样被正式记住”，lease journal compaction 与 ownership migration replay 管“这些历史怎样被正式裁剪、正式回放，并且还能被重新证明为一致状态”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 compaction / replay 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/lease-journal-compaction-replay-checkpoint.json
runtime/lease-journal-segment-current.json
runtime/lease-journal-segment-<seq>.json
runtime/ownership-migration-replay-<ticket>.json
runtime/remote-ownership-migration-replay-summary.json
runtime/lease-journal-compaction-replay-published.json
```

其中：

- `config/console.json`
	- 保存 `compaction_timeout_ms`、`replay_timeout_ms`、`publish_timeout_ms`、`shared_lease_limit`、`replay_quorum_required`、`compaction_threshold`、`compaction_retry_budget`、`replay_retry_budget`、`compaction_wake_interval_ms`、`replay_wake_interval_ms`
- `runtime/lease-journal-compaction-replay-checkpoint.json`
	- 保存最近一次正式 compaction / replay 协调状态
- `runtime/lease-journal-segment-current.json`
	- 保存当前 journal current 摘要
- `runtime/lease-journal-segment-<seq>.json`
	- 保存某个 segment 的正式 journal 摘要
- `runtime/ownership-migration-replay-<ticket>.json`
	- 保存某一轮 replay 的 audit 结果
- `runtime/remote-ownership-migration-replay-summary.json`
	- 保存这轮远端 replay summary 的本地 stage 文件
- `runtime/lease-journal-compaction-replay-published.json`
	- 保存异步发布后的 replay 输出

## 5. 先把“启动装载 checkpoint -> decide compaction / replay -> admitted 后再 publish replay summary”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `lease-journal-compaction-replay-checkpoint.json`
2. checkpoint 里明确记录 `journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / replay_ack_count / replay_reject_count / journal_rotate_queue / replay_queue / audit_queue / compaction_retry_budget / replay_retry_budget / next_compaction_wake_at / next_replay_wake_at`
3. `dict` 表示当前 active replay registry
4. `list` 表示 recent compaction history 和 recent replay history
5. `queue + thread` 的边界先体现在“请求线程只提交 compaction / replay 意图”
6. `task group` 先管住本次 admitted compaction scope
7. local journal current inspect 和 checkpoint inspect 先各用一个 child 表达
8. 多个 peer 的 replay summary 走多条 `xhttp future`
9. admission 逻辑先决定 `append_compaction_marker_now / wait_replay_quorum / queue_granted_lease_window / queue_compaction_window / run_compaction_now / queue_replay_window / run_replay_now / sleep_compaction_wake / sleep_replay_wake / defer_compaction_budget / defer_replay_budget / publish_compacted_journal_now`
10. admitted 后再统一推进 `journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / ownership_epoch / publish_index / checkpoint_version`

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
	bool bHealthy;
} DemoLeaseReplayPeer;

typedef struct
{
	uint32 iCompactionTimeoutMs;
	uint32 iReplayTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iLocalQuotaLimit;
	uint32 iRemoteQuotaLimit;
	uint32 iSharedLeaseLimit;
	uint32 iReplayQuorumRequired;
	uint32 iCompactionThreshold;
	uint32 iCompactionRetryBudget;
	uint32 iReplayRetryBudget;
	uint32 iCompactionWakeIntervalMs;
	uint32 iReplayWakeIntervalMs;
} DemoLeaseReplayPolicy;

typedef struct
{
	char sStage[64];
	char sPath[260];
	uint32 iDelayMs;
	int32 iStatus;
} DemoLeaseReplayTask;

typedef struct
{
	DemoLeaseReplayPolicy tPolicy;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iPublishIndex;
	uint32 iCheckpointVersion;
	uint32 iLeaseEpoch;
	uint32 iOwnershipEpoch;
	uint32 iJournalSegmentSeq;
	uint32 iCompactedToSeq;
	uint32 iReplayFromSeq;
	uint32 iReplayToSeq;
	uint32 iReplayCursor;
	uint32 iReplayTicket;
	uint32 iReplayAckCount;
	uint32 iReplayRejectCount;
	uint32 iActiveCompactionSlot;
	uint32 iActiveReplaySlot;
	uint32 iSharedLeaseInUse;
	uint32 iGrantedLeaseWindow;
	uint32 iJournalRotateQueue;
	uint32 iReplayQueue;
	uint32 iAuditQueue;
	uint32 iCompactionRetryBudget;
	uint32 iReplayRetryBudget;
	uint32 iNextCompactionWakeAt;
	uint32 iNextReplayWakeAt;
	char sCommittedOwner[48];
	char sCurrentOwner[48];
	char sTargetOwner[48];
	char sWitnessCluster[48];
	char sCompactionState[64];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
	char sJournalCurrentPath[260];
} DemoLeaseReplayCenter;

static void procCopyText(char* sDst, size_t iDstCap, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstCap == 0u) ) {
		return;
	}

	if ( sSrc == NULL ) {
		sDst[0] = '\0';
		return;
	}

	snprintf(sDst, iDstCap, "%s", sSrc);
}

static void procBuildJournalSegmentPath(char* sBuf, size_t iCap, uint32 iSeq)
{
	snprintf(sBuf, iCap, "runtime/lease-journal-segment-%u.json", (unsigned)iSeq);
}

static void procBuildReplayAuditPath(char* sBuf, size_t iCap, uint32 iTicket)
{
	snprintf(sBuf, iCap, "runtime/ownership-migration-replay-%u.json", (unsigned)iTicket);
}

static bool procWriteJournalSegment(
	const char* sPath,
	const char* sKey,
	const char* sState,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	uint32 iJournalSegmentSeq,
	uint32 iCompactedToSeq,
	uint32 iReplayFromSeq,
	uint32 iReplayToSeq,
	uint32 iReplayCursor,
	uint32 iOwnershipEpoch)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)((sState != NULL) ? sState : ""), 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "current_owner", 0, (uint8*)((sCurrentOwner != NULL) ? sCurrentOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "target_owner", 0, (uint8*)((sTargetOwner != NULL) ? sTargetOwner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "journal_segment_seq", 0, (int32)iJournalSegmentSeq);
	xvoTableSetInt(vRoot, "compacted_to_seq", 0, (int32)iCompactedToSeq);
	xvoTableSetInt(vRoot, "replay_from_seq", 0, (int32)iReplayFromSeq);
	xvoTableSetInt(vRoot, "replay_to_seq", 0, (int32)iReplayToSeq);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "ownership_epoch", 0, (int32)iOwnershipEpoch);
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

static bool procWriteReplayAudit(
	const char* sPath,
	const char* sKey,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	const char* sWitnessCluster,
	uint32 iReplayTicket,
	uint32 iReplayAckCount,
	uint32 iReplayRejectCount,
	uint32 iReplayCursor,
	uint32 iReplayToSeq)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"ownership_migration_replay_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "current_owner", 0, (uint8*)((sCurrentOwner != NULL) ? sCurrentOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "target_owner", 0, (uint8*)((sTargetOwner != NULL) ? sTargetOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "witness_cluster", 0, (uint8*)((sWitnessCluster != NULL) ? sWitnessCluster : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "replay_ticket", 0, (int32)iReplayTicket);
	xvoTableSetInt(vRoot, "replay_ack_count", 0, (int32)iReplayAckCount);
	xvoTableSetInt(vRoot, "replay_reject_count", 0, (int32)iReplayRejectCount);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "replay_to_seq", 0, (int32)iReplayToSeq);
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

static bool procSaveLeaseJournalCompactionCheckpoint(
	const char* sPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	const char* sWitnessCluster,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iPublishIndex,
	uint32 iCheckpointVersion,
	uint32 iLeaseEpoch,
	uint32 iOwnershipEpoch,
	uint32 iJournalSegmentSeq,
	uint32 iCompactedToSeq,
	uint32 iReplayFromSeq,
	uint32 iReplayToSeq,
	uint32 iReplayCursor,
	uint32 iReplayTicket,
	uint32 iReplayAckCount,
	uint32 iReplayRejectCount,
	uint32 iActiveCompactionSlot,
	uint32 iActiveReplaySlot,
	uint32 iSharedLeaseInUse,
	uint32 iGrantedLeaseWindow,
	uint32 iJournalRotateQueue,
	uint32 iReplayQueue,
	uint32 iAuditQueue,
	uint32 iCompactionRetryBudget,
	uint32 iReplayRetryBudget,
	uint32 iLocalQuotaLimit,
	uint32 iRemoteQuotaLimit,
	uint32 iSharedLeaseLimit,
	uint32 iReplayQuorumRequired,
	uint32 iNextCompactionWakeAt,
	uint32 iNextReplayWakeAt,
	uint32 iCompactionThreshold,
	uint32 iFinished)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)((sState != NULL) ? sState : ""), 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "committed_owner", 0, (uint8*)((sCommittedOwner != NULL) ? sCommittedOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "current_owner", 0, (uint8*)((sCurrentOwner != NULL) ? sCurrentOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "target_owner", 0, (uint8*)((sTargetOwner != NULL) ? sTargetOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "witness_cluster", 0, (uint8*)((sWitnessCluster != NULL) ? sWitnessCluster : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "checkpoint_version", 0, (int32)iCheckpointVersion);
	xvoTableSetInt(vRoot, "lease_epoch", 0, (int32)iLeaseEpoch);
	xvoTableSetInt(vRoot, "ownership_epoch", 0, (int32)iOwnershipEpoch);
	xvoTableSetInt(vRoot, "journal_segment_seq", 0, (int32)iJournalSegmentSeq);
	xvoTableSetInt(vRoot, "compacted_to_seq", 0, (int32)iCompactedToSeq);
	xvoTableSetInt(vRoot, "replay_from_seq", 0, (int32)iReplayFromSeq);
	xvoTableSetInt(vRoot, "replay_to_seq", 0, (int32)iReplayToSeq);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "replay_ticket", 0, (int32)iReplayTicket);
	xvoTableSetInt(vRoot, "replay_ack_count", 0, (int32)iReplayAckCount);
	xvoTableSetInt(vRoot, "replay_reject_count", 0, (int32)iReplayRejectCount);
	xvoTableSetInt(vRoot, "active_compaction_slot", 0, (int32)iActiveCompactionSlot);
	xvoTableSetInt(vRoot, "active_replay_slot", 0, (int32)iActiveReplaySlot);
	xvoTableSetInt(vRoot, "shared_lease_in_use", 0, (int32)iSharedLeaseInUse);
	xvoTableSetInt(vRoot, "granted_lease_window", 0, (int32)iGrantedLeaseWindow);
	xvoTableSetInt(vRoot, "journal_rotate_queue", 0, (int32)iJournalRotateQueue);
	xvoTableSetInt(vRoot, "replay_queue", 0, (int32)iReplayQueue);
	xvoTableSetInt(vRoot, "audit_queue", 0, (int32)iAuditQueue);
	xvoTableSetInt(vRoot, "compaction_retry_budget", 0, (int32)iCompactionRetryBudget);
	xvoTableSetInt(vRoot, "replay_retry_budget", 0, (int32)iReplayRetryBudget);
	xvoTableSetInt(vRoot, "local_quota_limit", 0, (int32)iLocalQuotaLimit);
	xvoTableSetInt(vRoot, "remote_quota_limit", 0, (int32)iRemoteQuotaLimit);
	xvoTableSetInt(vRoot, "shared_lease_limit", 0, (int32)iSharedLeaseLimit);
	xvoTableSetInt(vRoot, "replay_quorum_required", 0, (int32)iReplayQuorumRequired);
	xvoTableSetInt(vRoot, "next_compaction_wake_at", 0, (int32)iNextCompactionWakeAt);
	xvoTableSetInt(vRoot, "next_replay_wake_at", 0, (int32)iNextReplayWakeAt);
	xvoTableSetInt(vRoot, "compaction_threshold", 0, (int32)iCompactionThreshold);
	xvoTableSetInt(vRoot, "finished", 0, (int32)iFinished);
	xvoTableSetInt(vRoot, "updated_at", 0, (int32)xrtNow());

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

static xvalue procLoadLeaseJournalCompactionCheckpoint(const char* sPath)
{
	str sJson = NULL;
	size_t iJsonSize = 0;
	xvalue vRoot = NULL;

	if ( sPath == NULL ) {
		return NULL;
	}

	sJson = xrtFileReadAll((str)sPath, XRT_CP_UTF8, &iJsonSize);
	if ( sJson == NULL ) {
		return NULL;
	}

	vRoot = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);

	if ( (vRoot == NULL) || xvoIsNull(vRoot) ) {
		if ( vRoot != NULL ) {
			xvoUnref(vRoot);
		}
		return NULL;
	}

	return vRoot;
}

static bool procWriteRemoteOwnershipMigrationReplaySummary(
	const char* sPath,
	const char* sKey,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	uint32 iReplayToSeq,
	uint32 iReplayCursor,
	uint32 iOwnershipEpoch,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_ownership_migration_replay_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "current_owner", 0, (uint8*)((sCurrentOwner != NULL) ? sCurrentOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "target_owner", 0, (uint8*)((sTargetOwner != NULL) ? sTargetOwner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "replay_to_seq", 0, (int32)iReplayToSeq);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "ownership_epoch", 0, (int32)iOwnershipEpoch);
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
	DemoLeaseReplayTask* pTask = (DemoLeaseReplayTask*)pArg;
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

static bool procPrepareReplayRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoLeaseReplayPeer* pPeer,
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

	snprintf(sRef, sizeof(sRef), "/api/lease-journal-compaction/%s?round=%u", sKey, (unsigned)iRound);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Ownership-Key", sKey);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static int procSelectPeers(
	const DemoLeaseReplayPeer arrPeer[],
	int iPeerCount,
	uint32 iMaxLatencyMs,
	const DemoLeaseReplayPeer* arrOut[],
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

static const char* procChooseLeaseReplayPlan(const DemoLeaseReplayCenter* pCenter, int iPeerCount)
{
	uint32 iNow = (uint32)xrtNow();

	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iJournalSegmentSeq >= pCenter->tPolicy.iCompactionThreshold ) {
		if ( pCenter->iCompactionRetryBudget == 0u ) {
			return "defer_compaction_budget";
		}

		if ( pCenter->iNextCompactionWakeAt > iNow ) {
			return "sleep_compaction_wake";
		}
	}

	if ( pCenter->iGrantedLeaseWindow == 0u ) {
		if ( pCenter->iReplayRetryBudget == 0u ) {
			return "defer_replay_budget";
		}

		if ( pCenter->iReplayAckCount < pCenter->tPolicy.iReplayQuorumRequired ) {
			return "append_compaction_marker_now";
		}

		return "wait_replay_quorum";
	}

	if ( pCenter->iJournalRotateQueue > 0u ) {
		if ( pCenter->iSharedLeaseInUse >= pCenter->iGrantedLeaseWindow ) {
			return "queue_granted_lease_window";
		}
		if ( pCenter->iActiveCompactionSlot >= pCenter->tPolicy.iLocalQuotaLimit ) {
			return "queue_compaction_window";
		}
		if ( pCenter->iNextCompactionWakeAt > iNow ) {
			return "sleep_compaction_wake";
		}
		return "run_compaction_now";
	}

	if ( pCenter->iReplayQueue > 0u ) {
		if ( pCenter->iSharedLeaseInUse >= pCenter->iGrantedLeaseWindow ) {
			return "queue_granted_lease_window";
		}
		if ( pCenter->iActiveReplaySlot >= pCenter->tPolicy.iRemoteQuotaLimit ) {
			return "queue_replay_window";
		}
		if ( pCenter->iNextReplayWakeAt > iNow ) {
			return "sleep_replay_wake";
		}
		return "run_replay_now";
	}

	if ( (pCenter->iCompactedToSeq >= pCenter->iReplayToSeq) && (pCenter->iReplayCursor >= pCenter->iReplayToSeq) ) {
		return "publish_compacted_journal_now";
	}

	return "observe";
}

static bool procRunLeaseJournalCompactionReplay(
	DemoLeaseReplayCenter* pCenter,
	const DemoLeaseReplayPeer* arrPeer[],
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
	DemoLeaseReplayTask tCurrentInspect;
	DemoLeaseReplayTask tCheckpointInspect;
	DemoLeaseReplayTask tReplayInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sJournalSegmentPath[260];
	char sReplayAuditPath[260];
	char sPublishJson[1600];
	int iChildCount = 0;
	int i;
	bool bCompactionDomain = false;
	bool bFinished = false;
	bool bReplayReady = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tCurrentInspect, 0, sizeof(tCurrentInspect));
	memset(&tCheckpointInspect, 0, sizeof(tCheckpointInspect));
	memset(&tReplayInspect, 0, sizeof(tReplayInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	bCompactionDomain = (pCenter->iGrantedLeaseWindow > 0u) ? (pCenter->iJournalRotateQueue > 0u) : true;
	pCenter->iSharedLeaseInUse++;

	if ( bCompactionDomain ) {
		pCenter->iActiveCompactionSlot++;
		if ( pCenter->iJournalRotateQueue > 0u ) {
			pCenter->iJournalRotateQueue--;
		}
	} else {
		pCenter->iActiveReplaySlot++;
		if ( pCenter->iReplayQueue > 0u ) {
			pCenter->iReplayQueue--;
		}
	}

	(void)procSaveLeaseJournalCompactionCheckpoint(
		pCenter->sCheckpointPath,
		"lease_journal_compaction_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iLeaseEpoch,
		pCenter->iOwnershipEpoch,
		pCenter->iJournalSegmentSeq,
		pCenter->iCompactedToSeq,
		pCenter->iReplayFromSeq,
		pCenter->iReplayToSeq,
		pCenter->iReplayCursor,
		pCenter->iReplayTicket,
		pCenter->iReplayAckCount,
		pCenter->iReplayRejectCount,
		pCenter->iActiveCompactionSlot,
		pCenter->iActiveReplaySlot,
		pCenter->iSharedLeaseInUse,
		pCenter->iGrantedLeaseWindow,
		pCenter->iJournalRotateQueue,
		pCenter->iReplayQueue,
		pCenter->iAuditQueue,
		pCenter->iCompactionRetryBudget,
		pCenter->iReplayRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedLeaseLimit,
		pCenter->tPolicy.iReplayQuorumRequired,
		pCenter->iNextCompactionWakeAt,
		pCenter->iNextReplayWakeAt,
		pCenter->tPolicy.iCompactionThreshold,
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

	procCopyText(tCurrentInspect.sStage, sizeof(tCurrentInspect.sStage), "inspect_journal_current");
	procCopyText(tCurrentInspect.sPath, sizeof(tCurrentInspect.sPath), pCenter->sJournalCurrentPath);
	tCurrentInspect.iDelayMs = 15u;
	tCurrentInspect.iStatus = XRT_NET_OK;
	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tCurrentInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tCheckpointInspect.sStage, sizeof(tCheckpointInspect.sStage), "inspect_compaction_checkpoint");
	procCopyText(tCheckpointInspect.sPath, sizeof(tCheckpointInspect.sPath), pCenter->sCheckpointPath);
	tCheckpointInspect.iDelayMs = 20u;
	tCheckpointInspect.iStatus = XRT_NET_OK;
	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tCheckpointInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPrepareReplayRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			bCompactionDomain ? pCenter->tPolicy.iCompactionTimeoutMs : pCenter->tPolicy.iReplayTimeoutMs) ) {
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
			int32 iRemoteReplayToSeq = 0;
			int32 iRemoteReplayCursor = 0;
			int32 iRemoteOwnershipEpoch = 0;
			int32 iRemotePublishIndex = 0;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteReplayToSeq = xvoTableGetInt(vRemote, "remote_replay_to_seq", 0);
				iRemoteReplayCursor = xvoTableGetInt(vRemote, "remote_replay_cursor", 0);
				iRemoteOwnershipEpoch = xvoTableGetInt(vRemote, "remote_ownership_epoch", 0);
				iRemotePublishIndex = xvoTableGetInt(vRemote, "remote_publish_index", 0);

				if ( iRemoteReplayToSeq > (int32)pCenter->iReplayToSeq ) {
					pCenter->iReplayToSeq = (uint32)iRemoteReplayToSeq;
					procCopyText(pCenter->sWitnessCluster, sizeof(pCenter->sWitnessCluster), arrPeer[i]->sName);
				}
				if ( iRemoteReplayCursor > (int32)pCenter->iReplayCursor ) {
					pCenter->iReplayCursor = (uint32)iRemoteReplayCursor;
				}

				if ( (iRemoteOwnershipEpoch >= (int32)pCenter->iOwnershipEpoch) && (iRemotePublishIndex >= (int32)(pCenter->iCommittedRound - 1u)) ) {
					pCenter->iReplayAckCount++;
				} else {
					pCenter->iReplayRejectCount++;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	pCenter->iReplayTicket++;
	if ( pCenter->iReplayFromSeq == 0u ) {
		pCenter->iReplayFromSeq = pCenter->iCompactedToSeq + 1u;
	}
	if ( pCenter->iReplayToSeq < pCenter->iJournalSegmentSeq ) {
		pCenter->iReplayToSeq = pCenter->iJournalSegmentSeq;
	}

	if ( pCenter->iReplayAckCount >= pCenter->tPolicy.iReplayQuorumRequired ) {
		pCenter->iGrantedLeaseWindow = pCenter->tPolicy.iSharedLeaseLimit;
		pCenter->iLeaseEpoch++;
		bReplayReady = true;
	}

	if ( pCenter->iCompactedToSeq < pCenter->iJournalSegmentSeq ) {
		pCenter->iCompactedToSeq++;
	}
	if ( pCenter->iReplayCursor < pCenter->iReplayToSeq ) {
		pCenter->iReplayCursor++;
	}
	if ( pCenter->iReplayCursor >= pCenter->iReplayToSeq ) {
		pCenter->iOwnershipEpoch++;
		procCopyText(pCenter->sCurrentOwner, sizeof(pCenter->sCurrentOwner), pCenter->sTargetOwner);
	}

	pCenter->iPublishIndex = pCenter->iCommittedRound;
	pCenter->iCheckpointVersion++;

	(void)procWriteJournalSegment(
		pCenter->sJournalCurrentPath,
		sKey,
		"lease_journal_segment_current_ready",
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iJournalSegmentSeq,
		pCenter->iCompactedToSeq,
		pCenter->iReplayFromSeq,
		pCenter->iReplayToSeq,
		pCenter->iReplayCursor,
		pCenter->iOwnershipEpoch);

	procBuildJournalSegmentPath(sJournalSegmentPath, sizeof(sJournalSegmentPath), pCenter->iJournalSegmentSeq);
	(void)procWriteJournalSegment(
		sJournalSegmentPath,
		sKey,
		"lease_journal_segment_ready",
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iJournalSegmentSeq,
		pCenter->iCompactedToSeq,
		pCenter->iReplayFromSeq,
		pCenter->iReplayToSeq,
		pCenter->iReplayCursor,
		pCenter->iOwnershipEpoch);

	procBuildReplayAuditPath(sReplayAuditPath, sizeof(sReplayAuditPath), pCenter->iReplayTicket);
	(void)procWriteReplayAudit(
		sReplayAuditPath,
		sKey,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iReplayTicket,
		pCenter->iReplayAckCount,
		pCenter->iReplayRejectCount,
		pCenter->iReplayCursor,
		pCenter->iReplayToSeq);

	(void)procWriteRemoteOwnershipMigrationReplaySummary(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iReplayToSeq,
		pCenter->iReplayCursor,
		pCenter->iOwnershipEpoch,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procCopyText(tReplayInspect.sStage, sizeof(tReplayInspect.sStage), "inspect_replay_audit");
	procCopyText(tReplayInspect.sPath, sizeof(tReplayInspect.sPath), sReplayAuditPath);
	tReplayInspect.iDelayMs = 10u;
	tReplayInspect.iStatus = XRT_NET_OK;
	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tReplayInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	if ( !bReplayReady && (pCenter->iGrantedLeaseWindow == 0u) ) {
		pCenter->iAuditQueue++;
		pCenter->iSharedLeaseInUse = 0u;
		pCenter->iNextCompactionWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iCompactionWakeIntervalMs;
		procCopyText(pCenter->sCompactionState, sizeof(pCenter->sCompactionState), "lease_journal_compaction_wait_replay_quorum");
	} else if ( pCenter->iReplayCursor >= pCenter->iReplayToSeq ) {
		pCenter->iCompactionRetryBudget = pCenter->tPolicy.iCompactionRetryBudget;
		pCenter->iReplayRetryBudget = pCenter->tPolicy.iReplayRetryBudget;
		pCenter->iNextCompactionWakeAt = 0u;
		pCenter->iNextReplayWakeAt = 0u;
		procCopyText(pCenter->sCompactionState, sizeof(pCenter->sCompactionState), "lease_journal_compaction_replay_finished");
	} else {
		if ( bCompactionDomain ) {
			if ( pCenter->iCompactionRetryBudget > 0u ) {
				pCenter->iCompactionRetryBudget--;
			}
			pCenter->iReplayQueue++;
			pCenter->iNextCompactionWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iCompactionWakeIntervalMs;
			pCenter->iNextReplayWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iReplayWakeIntervalMs;
			procCopyText(pCenter->sCompactionState, sizeof(pCenter->sCompactionState), "lease_journal_compaction_running");
		} else {
			if ( pCenter->iReplayRetryBudget > 0u ) {
				pCenter->iReplayRetryBudget--;
			}
			pCenter->iAuditQueue++;
			pCenter->iNextReplayWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iReplayWakeIntervalMs;
			procCopyText(pCenter->sCompactionState, sizeof(pCenter->sCompactionState), "ownership_migration_replay_running");
		}
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"current_owner\": \"%s\",\n\t\"target_owner\": \"%s\",\n\t\"witness_cluster\": \"%s\",\n\t\"lease_epoch\": %u,\n\t\"ownership_epoch\": %u,\n\t\"journal_segment_seq\": %u,\n\t\"compacted_to_seq\": %u,\n\t\"replay_from_seq\": %u,\n\t\"replay_to_seq\": %u,\n\t\"replay_cursor\": %u,\n\t\"replay_ticket\": %u,\n\t\"replay_ack_count\": %u,\n\t\"replay_reject_count\": %u,\n\t\"journal_rotate_queue\": %u,\n\t\"replay_queue\": %u,\n\t\"audit_queue\": %u,\n\t\"compaction_retry_budget\": %u,\n\t\"replay_retry_budget\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u\n}\n",
		pCenter->sCompactionState,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		(unsigned)pCenter->iLeaseEpoch,
		(unsigned)pCenter->iOwnershipEpoch,
		(unsigned)pCenter->iJournalSegmentSeq,
		(unsigned)pCenter->iCompactedToSeq,
		(unsigned)pCenter->iReplayFromSeq,
		(unsigned)pCenter->iReplayToSeq,
		(unsigned)pCenter->iReplayCursor,
		(unsigned)pCenter->iReplayTicket,
		(unsigned)pCenter->iReplayAckCount,
		(unsigned)pCenter->iReplayRejectCount,
		(unsigned)pCenter->iJournalRotateQueue,
		(unsigned)pCenter->iReplayQueue,
		(unsigned)pCenter->iAuditQueue,
		(unsigned)pCenter->iCompactionRetryBudget,
		(unsigned)pCenter->iReplayRetryBudget,
		(unsigned)pCenter->iPublishIndex,
		(unsigned)pCenter->iCheckpointVersion);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveLeaseJournalCompactionCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sCompactionState : "lease_journal_compaction_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iLeaseEpoch,
		pCenter->iOwnershipEpoch,
		pCenter->iJournalSegmentSeq,
		pCenter->iCompactedToSeq,
		pCenter->iReplayFromSeq,
		pCenter->iReplayToSeq,
		pCenter->iReplayCursor,
		pCenter->iReplayTicket,
		pCenter->iReplayAckCount,
		pCenter->iReplayRejectCount,
		pCenter->iActiveCompactionSlot,
		pCenter->iActiveReplaySlot,
		pCenter->iSharedLeaseInUse,
		pCenter->iGrantedLeaseWindow,
		pCenter->iJournalRotateQueue,
		pCenter->iReplayQueue,
		pCenter->iAuditQueue,
		pCenter->iCompactionRetryBudget,
		pCenter->iReplayRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedLeaseLimit,
		pCenter->tPolicy.iReplayQuorumRequired,
		pCenter->iNextCompactionWakeAt,
		pCenter->iNextReplayWakeAt,
		pCenter->tPolicy.iCompactionThreshold,
		bFinished ? 1u : 0u);

	if ( pCenter->iSharedLeaseInUse > 0u ) {
		pCenter->iSharedLeaseInUse--;
	}
	if ( bCompactionDomain ) {
		if ( pCenter->iActiveCompactionSlot > 0u ) {
			pCenter->iActiveCompactionSlot--;
		}
	} else {
		if ( pCenter->iActiveReplaySlot > 0u ) {
			pCenter->iActiveReplaySlot--;
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
	DemoLeaseReplayCenter tCenter;
	DemoLeaseReplayPeer arrPeer[DEMO_MAX_PEER];
	const DemoLeaseReplayPeer* arrSelected[DEMO_MAX_PEER];
	xvalue vLoaded = NULL;
	str sCommittedOwner = NULL;
	str sCurrentOwner = NULL;
	str sTargetOwner = NULL;
	str sCompactionState = NULL;
	const char* sPlan = NULL;
	int iPeerCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iCompactionTimeoutMs = 3000u;
	tCenter.tPolicy.iReplayTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iLocalQuotaLimit = 1u;
	tCenter.tPolicy.iRemoteQuotaLimit = 1u;
	tCenter.tPolicy.iSharedLeaseLimit = 2u;
	tCenter.tPolicy.iReplayQuorumRequired = 2u;
	tCenter.tPolicy.iCompactionThreshold = 32u;
	tCenter.tPolicy.iCompactionRetryBudget = 3u;
	tCenter.tPolicy.iReplayRetryBudget = 2u;
	tCenter.tPolicy.iCompactionWakeIntervalMs = 10u;
	tCenter.tPolicy.iReplayWakeIntervalMs = 20u;
	tCenter.iTerm = 25u;
	tCenter.iCommittedRound = 10u;
	tCenter.iPublishIndex = 10u;
	tCenter.iCheckpointVersion = 15u;
	tCenter.iLeaseEpoch = 16u;
	tCenter.iOwnershipEpoch = 11u;
	tCenter.iJournalSegmentSeq = 36u;
	tCenter.iCompactedToSeq = 24u;
	tCenter.iReplayFromSeq = 25u;
	tCenter.iReplayToSeq = 36u;
	tCenter.iReplayCursor = 29u;
	tCenter.iReplayTicket = 47u;
	tCenter.iJournalRotateQueue = 1u;
	tCenter.iReplayQueue = 2u;
	tCenter.iAuditQueue = 1u;
	tCenter.iCompactionRetryBudget = tCenter.tPolicy.iCompactionRetryBudget;
	tCenter.iReplayRetryBudget = tCenter.tPolicy.iReplayRetryBudget;
	tCenter.iNextCompactionWakeAt = (uint32)xrtNow() - 1u;
	tCenter.iNextReplayWakeAt = (uint32)xrtNow() + 12u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-c");
	procCopyText(tCenter.sCurrentOwner, sizeof(tCenter.sCurrentOwner), "node-c");
	procCopyText(tCenter.sTargetOwner, sizeof(tCenter.sTargetOwner), "node-c");
	procCopyText(tCenter.sWitnessCluster, sizeof(tCenter.sWitnessCluster), "cluster-b");
	procCopyText(tCenter.sCompactionState, sizeof(tCenter.sCompactionState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/lease-journal-compaction-replay-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/lease-journal-compaction-replay-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-ownership-migration-replay-summary.json");
	procCopyText(tCenter.sJournalCurrentPath, sizeof(tCenter.sJournalCurrentPath), "runtime/lease-journal-segment-current.json");

	(void)procSaveLeaseJournalCompactionCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		tCenter.sWitnessCluster,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iPublishIndex,
		tCenter.iCheckpointVersion,
		tCenter.iLeaseEpoch,
		tCenter.iOwnershipEpoch,
		tCenter.iJournalSegmentSeq,
		tCenter.iCompactedToSeq,
		tCenter.iReplayFromSeq,
		tCenter.iReplayToSeq,
		tCenter.iReplayCursor,
		tCenter.iReplayTicket,
		tCenter.iReplayAckCount,
		tCenter.iReplayRejectCount,
		tCenter.iActiveCompactionSlot,
		tCenter.iActiveReplaySlot,
		tCenter.iSharedLeaseInUse,
		tCenter.iGrantedLeaseWindow,
		tCenter.iJournalRotateQueue,
		tCenter.iReplayQueue,
		tCenter.iAuditQueue,
		tCenter.iCompactionRetryBudget,
		tCenter.iReplayRetryBudget,
		tCenter.tPolicy.iLocalQuotaLimit,
		tCenter.tPolicy.iRemoteQuotaLimit,
		tCenter.tPolicy.iSharedLeaseLimit,
		tCenter.tPolicy.iReplayQuorumRequired,
		tCenter.iNextCompactionWakeAt,
		tCenter.iNextReplayWakeAt,
		tCenter.tPolicy.iCompactionThreshold,
		0u);

	(void)procWriteJournalSegment(
		tCenter.sJournalCurrentPath,
		"console-service",
		"lease_journal_segment_current_boot",
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		tCenter.iJournalSegmentSeq,
		tCenter.iCompactedToSeq,
		tCenter.iReplayFromSeq,
		tCenter.iReplayToSeq,
		tCenter.iReplayCursor,
		tCenter.iOwnershipEpoch);

	vLoaded = procLoadLeaseJournalCompactionCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sCommittedOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sCurrentOwner = xvoTableGetText(vLoaded, "current_owner", 0);
	sTargetOwner = xvoTableGetText(vLoaded, "target_owner", 0);
	sCompactionState = xvoTableGetText(vLoaded, "state", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iLeaseEpoch = (uint32)xvoTableGetInt(vLoaded, "lease_epoch", 0);
	tCenter.iOwnershipEpoch = (uint32)xvoTableGetInt(vLoaded, "ownership_epoch", 0);
	tCenter.iJournalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "journal_segment_seq", 0);
	tCenter.iCompactedToSeq = (uint32)xvoTableGetInt(vLoaded, "compacted_to_seq", 0);
	tCenter.iReplayFromSeq = (uint32)xvoTableGetInt(vLoaded, "replay_from_seq", 0);
	tCenter.iReplayToSeq = (uint32)xvoTableGetInt(vLoaded, "replay_to_seq", 0);
	tCenter.iReplayCursor = (uint32)xvoTableGetInt(vLoaded, "replay_cursor", 0);
	tCenter.iReplayTicket = (uint32)xvoTableGetInt(vLoaded, "replay_ticket", 0);
	tCenter.iReplayAckCount = (uint32)xvoTableGetInt(vLoaded, "replay_ack_count", 0);
	tCenter.iReplayRejectCount = (uint32)xvoTableGetInt(vLoaded, "replay_reject_count", 0);
	tCenter.iActiveCompactionSlot = (uint32)xvoTableGetInt(vLoaded, "active_compaction_slot", 0);
	tCenter.iActiveReplaySlot = (uint32)xvoTableGetInt(vLoaded, "active_replay_slot", 0);
	tCenter.iSharedLeaseInUse = (uint32)xvoTableGetInt(vLoaded, "shared_lease_in_use", 0);
	tCenter.iGrantedLeaseWindow = (uint32)xvoTableGetInt(vLoaded, "granted_lease_window", 0);
	tCenter.iJournalRotateQueue = (uint32)xvoTableGetInt(vLoaded, "journal_rotate_queue", 0);
	tCenter.iReplayQueue = (uint32)xvoTableGetInt(vLoaded, "replay_queue", 0);
	tCenter.iAuditQueue = (uint32)xvoTableGetInt(vLoaded, "audit_queue", 0);
	tCenter.iCompactionRetryBudget = (uint32)xvoTableGetInt(vLoaded, "compaction_retry_budget", 0);
	tCenter.iReplayRetryBudget = (uint32)xvoTableGetInt(vLoaded, "replay_retry_budget", 0);
	tCenter.tPolicy.iLocalQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "local_quota_limit", (int32)tCenter.tPolicy.iLocalQuotaLimit);
	tCenter.tPolicy.iRemoteQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "remote_quota_limit", (int32)tCenter.tPolicy.iRemoteQuotaLimit);
	tCenter.tPolicy.iSharedLeaseLimit = (uint32)xvoTableGetInt(vLoaded, "shared_lease_limit", (int32)tCenter.tPolicy.iSharedLeaseLimit);
	tCenter.tPolicy.iReplayQuorumRequired = (uint32)xvoTableGetInt(vLoaded, "replay_quorum_required", (int32)tCenter.tPolicy.iReplayQuorumRequired);
	tCenter.iNextCompactionWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_compaction_wake_at", 0);
	tCenter.iNextReplayWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_replay_wake_at", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sCommittedOwner);
	procCopyText(tCenter.sCurrentOwner, sizeof(tCenter.sCurrentOwner), (const char*)sCurrentOwner);
	procCopyText(tCenter.sTargetOwner, sizeof(tCenter.sTargetOwner), (const char*)sTargetOwner);
	procCopyText(tCenter.sCompactionState, sizeof(tCenter.sCompactionState), (const char*)sCompactionState);
	xvoUnref(vLoaded);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "cluster-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:9301");
	arrPeer[0].iLatencyMs = 24u;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "cluster-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:9302");
	arrPeer[1].iLatencyMs = 30u;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "cluster-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:9303");
	arrPeer[2].iLatencyMs = 110u;
	arrPeer[2].bHealthy = false;

	iPeerCount = procSelectPeers(
		arrPeer,
		DEMO_MAX_PEER,
		tCenter.tPolicy.iMaxPeerLatencyMs,
		arrSelected,
		DEMO_MAX_PEER);

	sPlan = procChooseLeaseReplayPlan(&tCenter, iPeerCount);

	printf("lease journal compaction replay plan = %s\n", sPlan);
	printf(
		"current_owner=%s target_owner=%s lease_epoch=%u ownership_epoch=%u journal_segment_seq=%u compacted_to_seq=%u replay_from_seq=%u replay_to_seq=%u replay_cursor=%u replay_ticket=%u replay_ack=%u replay_reject=%u journal_rotate_queue=%u replay_queue=%u audit_queue=%u compaction_retry_budget=%u replay_retry_budget=%u next_compaction_wake=%u next_replay_wake=%u compaction_state=%s witness_cluster=%s\n",
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iOwnershipEpoch,
		(unsigned)tCenter.iJournalSegmentSeq,
		(unsigned)tCenter.iCompactedToSeq,
		(unsigned)tCenter.iReplayFromSeq,
		(unsigned)tCenter.iReplayToSeq,
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iReplayTicket,
		(unsigned)tCenter.iReplayAckCount,
		(unsigned)tCenter.iReplayRejectCount,
		(unsigned)tCenter.iJournalRotateQueue,
		(unsigned)tCenter.iReplayQueue,
		(unsigned)tCenter.iAuditQueue,
		(unsigned)tCenter.iCompactionRetryBudget,
		(unsigned)tCenter.iReplayRetryBudget,
		(unsigned)tCenter.iNextCompactionWakeAt,
		(unsigned)tCenter.iNextReplayWakeAt,
		tCenter.sCompactionState,
		tCenter.sWitnessCluster);

	if ( (strcmp(sPlan, "append_compaction_marker_now") == 0) || (strcmp(sPlan, "run_compaction_now") == 0) || (strcmp(sPlan, "run_replay_now") == 0) || (strcmp(sPlan, "publish_compacted_journal_now") == 0) ) {
		if ( !procRunLeaseJournalCompactionReplay(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "lease journal compaction replay failed\n");
			return 1;
		}
	}

	printf(
		"after lease journal compaction replay: current_owner=%s target_owner=%s compacted_to_seq=%u replay_to_seq=%u replay_cursor=%u replay_ticket=%u replay_ack=%u replay_reject=%u publish=%u version=%u compaction_state=%s witness_cluster=%s\n",
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		(unsigned)tCenter.iCompactedToSeq,
		(unsigned)tCenter.iReplayToSeq,
		(unsigned)tCenter.iReplayCursor,
		(unsigned)tCenter.iReplayTicket,
		(unsigned)tCenter.iReplayAckCount,
		(unsigned)tCenter.iReplayRejectCount,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sCompactionState,
		tCenter.sWitnessCluster);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `journal_segment_seq` 增长了，不等于旧 journal 已经正式被 compaction 吸收

这页最容易偷懒的写法是：

- 看到已经切到新的 `journal_segment_seq`
- 就认为旧 journal 可以直接删掉
- 看到 published summary 很新，就认为 replay 已经一定追平

但更稳的模型应该是：

- `lease journal finished`
	- 回答 owner 迁移怎样被 durable journal 正式记住
- `lease journal compaction finished`
	- 回答旧 journal 到底已经被正式吸收到哪一条
- `ownership migration replay finished`
	- 回答 replay cursor 到底有没有正式追平 replay tail，并且新的 current owner 是否已经真正稳定

### 6.2 `compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor` 不是一个字段

这页也很容易写混：

- 把 `compacted_to_seq` 直接当成 replay cursor
- 把 `replay_to_seq` 直接当成当前已经补齐的位置
- 把 `replay_from_seq` 忘掉，导致重启后根本不知道从哪段 journal 重新补

但更稳的边界应该是：

- `compacted_to_seq`
	- 回答旧 journal 已经被正式吸收到了哪一条
- `replay_from_seq`
	- 回答这一轮 replay 必须从哪一条 journal 开始补
- `replay_to_seq`
	- 回答这一轮 replay 目标必须补到哪一条
- `replay_cursor`
	- 回答当前实际已经补到了哪一条，而不是应该补到哪一条

### 6.3 published replay summary 可以提高可观测性，但不能替代 journal replay

这页还很容易偷懒成：

- published 文件已经有 `current_owner / replay_cursor`
- 所以就不再保留 segment journal
- 或者直接把 replay cursor 当成“历史已经安全可删”的证据

但更稳的写法应该是：

- segment journal 负责 durable replay 输入
- replay audit 负责解释这一轮 replay 为什么成功、失败或 defer
- published replay summary 负责对外读取，不负责替代 replay input
- journal compaction 只能在 `compacted_to_seq` 正式推进后发生，而不是只看文件大小或最新 publish

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要把 replay 扩展到跨租户、多资源类型和多阶段状态机补齐，应继续补更重的 `ownership migration replay orchestration` 主线
- 如果需要把 compaction、replay、仲裁、handoff 和 publish 统一进一个更长生命周期的控制面，应继续补更重的 `lease control-plane orchestration` 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `journal_segment_seq / compacted_to_seq / replay_from_seq / replay_to_seq / replay_cursor / replay_ticket / replay_ack_count / replay_reject_count / journal_rotate_queue / replay_queue / audit_queue / next_compaction_wake_at / next_replay_wake_at / compaction_state`
2. `POST /api/lease-journal-compaction-migration-replay`
	- 只提交 compaction / replay 意图，让 worker 决定是否进入 `append_compaction_marker_now / wait_replay_quorum / queue_granted_lease_window / queue_compaction_window / run_compaction_now / queue_replay_window / run_replay_now / sleep_compaction_wake / sleep_replay_wake / defer_compaction_budget / defer_replay_budget / publish_compacted_journal_now`
3. `GET /api/lease-journal-compaction-migration-replay`
	- 直接返回最近一次 compaction / replay checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 compaction wake、replay wake、replay quorum 复核、replay audit 和 published summary 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 segment、compacted range、replay range、replay cursor、双预算、双 wake、recent compaction history 和 recent replay history

## 9. 下一步阅读

如果你准备继续把 `lease journal compaction + ownership migration replay` 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个 lease journal 与 ownership migration 面板](lease-journal-ownership-migration-dashboard.md)
	先对比“owner 迁移怎样被 durable journal 正式记住”和“这些 journal 怎样被正式裁剪并 replay”到底差在哪里
2. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
	对比“通用 replay cursor”与“ownership migration replay cursor”到底差在哪里
3. 更重的下一页通常会走向 `lease control-plane orchestration`
	那时要开始把 compaction、replay、仲裁、handoff 和 publish 收成新的正式主线
