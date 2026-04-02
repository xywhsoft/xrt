# 把本地控制台服务升级成一个 lease journal 与 ownership migration 面板

> 这页要解决的，不再是“当前 lease owner 是谁”“这一轮 lease window 有没有过 quorum”这种单轮协调问题，而是更贴近长期运行服务的问题：当一个本地控制台服务已经同时积累了 `committed_owner / current_owner / target_owner / lease_epoch / ownership_epoch / lease_journal_seq / migration_ticket / migration_ack_count / migration_reject_count / active_owner_slot / active_migration_slot / shared_lease_in_use / granted_lease_window / owner_journal_queue / migration_queue / audit_queue / journal_flush_retry_budget / migration_retry_budget / next_journal_flush_at / next_migration_wake_at / journal_state / witness_cluster`，并且上一页已经能把“当前 lease 怎样续租或接管”收成正式状态之后，又开始需要回答“owner 迁移为什么必须写成正式 journal entry”“为什么 publish 成功不等于迁移审计完成”“为什么 journal flush、migration run、audit export 不能混成同一轮重试”“为什么当前 owner 和 target owner 必须同时留在 durable checkpoint 里”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续靠日志猜测 owner 迁移到底走到了哪一步。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- lease 故障切换怎样回答“谁应该接手当前 ownership”
- ownership 仲裁怎样回答“多个 contender 里谁赢了”
- 共识式 ownership 怎样回答“当前 committed owner 到底是谁”
- 快照租约协调怎样回答“lease 怎样续租、接管和收口”

但真实系统再往前走一步，很快又会出现一个新的问题：

- 当前 `current_owner = node-b`
- 这一轮想把 owner 迁到 `target_owner = node-c`
- `lease_epoch = 14`、`ownership_epoch = 9`
- `lease_journal_seq = 27`，说明 journal 已经不是空白状态
- `migration_ticket = 41`
- `migration_ack_count = 0`、`migration_reject_count = 0`
- `active_owner_slot = 1`、`active_migration_slot = 0`
- `shared_lease_in_use = 1`、`granted_lease_window = 2`
- `owner_journal_queue = 1`、`migration_queue = 2`、`audit_queue = 1`
- `journal_flush_retry_budget = 3`、`migration_retry_budget = 2`
- `next_journal_flush_at` 和 `next_migration_wake_at` 并不相同

如果这时再来一波 `POST /api/lease-journal-ownership-migration`，系统必须知道这次应该是：

- `append_owner_journal_now`
- `wait_migration_quorum`
- `queue_granted_lease_window`
- `queue_owner_window`
- `run_owner_migration_now`
- `queue_migration_window`
- `run_target_owner_now`
- `sleep_journal_flush`
- `sleep_migration_wake`
- `defer_migration_budget`
- 还是 `rotate_journal_now`

如果这时还把实现停在：

- `if ( quorum ) switch owner;`
- `if ( publish ok ) mark migration done;`
- `if ( fail ) retry later;`

很快就会出现几个典型问题：

- owner 已经被切到 `target_owner`，但 journal 里根本没有对应 entry
- publish 已成功，但 migration audit 还没写完，排障时只能猜当前 owner 是“已迁移”还是“半迁移”
- `migration_ack_count / migration_reject_count` 和 `migration_retry_budget` 混成一种失败语义，结果“票没过”和“预算没了”被写成同一类 defer
- 当前 `lease_journal_seq` 已经超过轮转阈值，但系统还在不停往同一个 journal 里追加，后续恢复根本不知道从哪段 replay

所以这页真正要补出的，不是“再给 lease 协调加几列字段”，而是：

> 当当前 owner、target owner、lease window、journal flush、migration run 和 audit export 已经同时存在时，怎样把它们统一收成一条可恢复、可审计、可轮转、可发布的 `lease journal + ownership migration` 正式主线。

## 2. 为什么这次不能只靠“checkpoint 里记住 current owner”就够了

### 2.1 `current_owner` 不是完整迁移状态

上一页的快照租约协调已经很好地解决了：

- 当前 lease owner 到底是谁
- `granted_lease_window` 有没有真的拿到
- 续租侧和接管侧什么时候可以再次 admission

但它不回答：

- 当前 `target_owner` 到底是谁
- 当前 owner 迁移是不是已经写进正式 journal
- 这轮迁移到底停在“等 quorum”“等 journal flush”还是“等 audit export”
- 为什么 `ownership_epoch` 不能只靠 publish 文件反推

### 2.2 `lease_journal_seq / migration_ticket / ownership_epoch` 不是一类字段

到了这一层，系统真正要稳定回答的是：

- 当前 `lease_journal_seq` 已经写到了哪一条
- 当前 `migration_ticket` 到底代表哪一轮迁移请求
- 当前 `ownership_epoch` 到底已经提交到哪一轮 committed owner
- 当前 `migration_ack_count / migration_reject_count` 已经累积了几票
- 当前 `owner_journal_queue / migration_queue / audit_queue` 分别还堆了多少工作
- 当前 `journal_flush_retry_budget / migration_retry_budget` 到底还有多少次重试机会
- 当前 `next_journal_flush_at / next_migration_wake_at` 到底什么时候再合法看一眼

这说明：

- `lease coordination checkpoint`
	- 回答 lease window、quorum 和 owner 当前卡在哪
- `lease journal checkpoint`
	- 回答 journal append、journal rotate、migration ticket 和 audit export 当前卡在哪
- `ownership migration checkpoint`
	- 回答当前 committed owner、target owner、ownership epoch 和 migration quorum 当前卡在哪

### 2.3 这次真正新增的是“journal append + migration audit + owner publish”状态层

更稳的分工方式是：

- `lease coordination`
	- 负责 lease window、grant/reject、当前 lease owner 的长期收口
- `ownership arbitration`
	- 负责 contender、winner 和 committed owner 的正式收口
- `lease journal + ownership migration`
	- 再往下负责 `lease_journal_seq / migration_ticket / migration_ack_count / migration_reject_count / ownership_epoch / current_owner / target_owner / journal_flush_retry_budget / migration_retry_budget`，以及 owner 迁移和 audit 导出的正式收口

一句话记住：

> 上一页补的是“当前 lease 怎样续租或接管”，这一页补的是“owner 迁移怎样写成 durable journal，并且怎样被正式审计和发布”。

## 3. 这条 lease journal 与 ownership migration 主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active owner registry | 当前有哪些 key 正在 current owner、target owner、journal queue 和 migration queue 上 |
| `list` | recent journal history | 页面和 JSON 展示最近 `append_owner_journal_now / wait_migration_quorum / run_owner_migration_now / run_target_owner_now / rotate_journal_now` 结果 |
| `list` | recent audit history | 页面和 JSON 展示最近一次迁移审计为什么成功、失败或 defer |
| `queue + thread` | 后台消费 `ENQUEUE / JOURNAL / MIGRATE / ROTATE / SWEEP` | 请求线程不阻塞在 quorum gather、journal flush 和 audit export 上 |
| `task group` | 管住一次 admitted migration scope 里的 local inspect / remote fetch / journal child / audit child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote migration summary、journal flush、audit export、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 ownership migration summary | 让远端 `remote_owner / remote_target_owner / remote_ownership_epoch / remote_publish_index` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 journal stage、写 audit stage、写 published stage | 让迁移状态进入正式文件主线 |
| `file_async` | 把 published owner migration summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、published JSON | 让 `lease_journal_seq / migration_ticket / migration_ack_count / migration_reject_count / ownership_epoch / current_owner / target_owner / owner_journal_queue / migration_queue / audit_queue / next_journal_flush_at / next_migration_wake_at` 有正式结构 |
| `time` | 记录 journal flush、migration wake、ownership epoch 更新时间、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 owner、target owner、journal seq、migration ticket、queue 长度、audit 状态和 published summary | 浏览器和脚本共享同一份状态面 |

一句话记住：

> lease 协调管“当前 lease 属于谁”，lease journal 与 ownership migration 管“ownership 怎样被正式写进 durable log，并且怎样被对外证明已经迁移完成”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 lease journal / ownership migration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/lease-journal-ownership-migration-checkpoint.json
runtime/lease-journal-current.json
runtime/lease-journal-entry-<seq>.json
runtime/ownership-migration-audit-<ticket>.json
runtime/remote-ownership-migration-summary.json
runtime/lease-journal-ownership-migration-published.json
```

其中：

- `config/console.json`
	- 保存 `journal_timeout_ms`、`migration_timeout_ms`、`publish_timeout_ms`、`shared_lease_limit`、`migration_quorum_required`、`journal_rotate_threshold`、`journal_flush_retry_budget`、`migration_retry_budget`、`journal_flush_interval_ms`、`remote_merge_interval_ms`
- `runtime/lease-journal-ownership-migration-checkpoint.json`
	- 保存最近一次正式 journal / migration 协调状态
- `runtime/lease-journal-current.json`
	- 保存当前 lease journal 的最新摘要
- `runtime/lease-journal-entry-<seq>.json`
	- 保存某一条正式 journal entry
- `runtime/ownership-migration-audit-<ticket>.json`
	- 保存某一轮 owner migration 的 audit 结果
- `runtime/remote-ownership-migration-summary.json`
	- 保存这轮远端 ownership migration summary 的本地 stage 文件
- `runtime/lease-journal-ownership-migration-published.json`
	- 保存异步发布后的正式 owner migration 输出

## 5. 先把“启动装载 checkpoint -> append journal -> gather migration quorum -> publish owner migration”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `lease-journal-ownership-migration-checkpoint.json`
2. checkpoint 里明确记录 `lease_journal_seq / migration_ticket / migration_ack_count / migration_reject_count / ownership_epoch / current_owner / target_owner / owner_journal_queue / migration_queue / audit_queue / journal_flush_retry_budget / migration_retry_budget / next_journal_flush_at / next_migration_wake_at`
3. `dict` 表示当前 active owner registry
4. `list` 表示 recent journal history 和 recent audit history
5. `queue + thread` 的边界先体现在“请求线程只提交 owner migration 意图”
6. `task group` 先管住本次 admitted migration scope
7. local journal inspect 和 local owner inspect 先各用一个 child 表达
8. 多个 peer 的 ownership migration summary 走多条 `xhttp future`
9. admission 逻辑先决定 `append_owner_journal_now / wait_migration_quorum / queue_granted_lease_window / queue_owner_window / run_owner_migration_now / queue_migration_window / run_target_owner_now / sleep_journal_flush / sleep_migration_wake / defer_migration_budget / rotate_journal_now`
10. admitted 后再统一推进 `lease_journal_seq / migration_ticket / ownership_epoch / publish_index / checkpoint_version`
11. 最后再用 `file_async future` 异步发布 committed owner migration 结果，并把 `current_owner / target_owner / ownership_epoch / migration_ack_count / migration_reject_count / witness_cluster` 一起导出

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
} DemoLeaseJournalPeer;

typedef struct
{
	uint32 iJournalTimeoutMs;
	uint32 iMigrationTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iLocalQuotaLimit;
	uint32 iRemoteQuotaLimit;
	uint32 iSharedLeaseLimit;
	uint32 iMigrationQuorumRequired;
	uint32 iJournalRotateThreshold;
	uint32 iMigrationRetryBudget;
	uint32 iJournalFlushRetryBudget;
	uint32 iJournalFlushIntervalMs;
	uint32 iRemoteMergeIntervalMs;
} DemoLeaseJournalPolicy;

typedef struct
{
	char sStage[64];
	char sPath[260];
	uint32 iDelayMs;
	int32 iStatus;
} DemoLeaseJournalTask;

typedef struct
{
	DemoLeaseJournalPolicy tPolicy;
	uint32 iTerm;
	uint32 iCommittedRound;
	uint32 iSnapshotIndex;
	uint32 iManifestCursor;
	uint32 iArchiveCursor;
	uint32 iTargetSegmentSeq;
	uint32 iPublishIndex;
	uint32 iCheckpointVersion;
	uint32 iLeaseJournalSeq;
	uint32 iMigrationTicket;
	uint32 iMigrationAckCount;
	uint32 iMigrationRejectCount;
	uint32 iLeaseEpoch;
	uint32 iOwnershipEpoch;
	uint32 iActiveOwnerSlot;
	uint32 iActiveMigrationSlot;
	uint32 iSharedLeaseInUse;
	uint32 iGrantedLeaseWindow;
	uint32 iOwnerJournalQueue;
	uint32 iMigrationQueue;
	uint32 iAuditQueue;
	uint32 iMigrationRetryBudget;
	uint32 iJournalFlushRetryBudget;
	uint32 iNextJournalFlushAt;
	uint32 iNextMigrationWakeAt;
	char sCommittedOwner[48];
	char sCurrentOwner[48];
	char sTargetOwner[48];
	char sWitnessCluster[48];
	char sJournalState[64];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
	char sJournalPath[260];
} DemoLeaseJournalCenter;

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

static void procBuildLeaseJournalEntryPath(char* sBuf, size_t iCap, uint32 iSeq)
{
	snprintf(sBuf, iCap, "runtime/lease-journal-entry-%u.json", (unsigned)iSeq);
}

static void procBuildOwnershipAuditPath(char* sBuf, size_t iCap, uint32 iTicket)
{
	snprintf(sBuf, iCap, "runtime/ownership-migration-audit-%u.json", (unsigned)iTicket);
}

static bool procWriteLeaseJournalEntry(
	const char* sPath,
	const char* sKey,
	const char* sState,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	uint32 iLeaseJournalSeq,
	uint32 iMigrationTicket,
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
	xvoTableSetInt(vRoot, "lease_journal_seq", 0, (int32)iLeaseJournalSeq);
	xvoTableSetInt(vRoot, "migration_ticket", 0, (int32)iMigrationTicket);
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

static bool procWriteOwnershipMigrationAudit(
	const char* sPath,
	const char* sKey,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	const char* sWitnessCluster,
	uint32 iMigrationTicket,
	uint32 iAckCount,
	uint32 iRejectCount,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"ownership_migration_audit_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "current_owner", 0, (uint8*)((sCurrentOwner != NULL) ? sCurrentOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "target_owner", 0, (uint8*)((sTargetOwner != NULL) ? sTargetOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "witness_cluster", 0, (uint8*)((sWitnessCluster != NULL) ? sWitnessCluster : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "migration_ticket", 0, (int32)iMigrationTicket);
	xvoTableSetInt(vRoot, "migration_ack_count", 0, (int32)iAckCount);
	xvoTableSetInt(vRoot, "migration_reject_count", 0, (int32)iRejectCount);
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

static bool procSaveLeaseJournalCheckpoint(
	const char* sPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sCurrentOwner,
	const char* sTargetOwner,
	const char* sWitnessCluster,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iManifestCursor,
	uint32 iArchiveCursor,
	uint32 iTargetSegmentSeq,
	uint32 iPublishIndex,
	uint32 iCheckpointVersion,
	uint32 iLeaseJournalSeq,
	uint32 iMigrationTicket,
	uint32 iMigrationAckCount,
	uint32 iMigrationRejectCount,
	uint32 iLeaseEpoch,
	uint32 iOwnershipEpoch,
	uint32 iActiveOwnerSlot,
	uint32 iActiveMigrationSlot,
	uint32 iSharedLeaseInUse,
	uint32 iGrantedLeaseWindow,
	uint32 iOwnerJournalQueue,
	uint32 iMigrationQueue,
	uint32 iAuditQueue,
	uint32 iMigrationRetryBudget,
	uint32 iJournalFlushRetryBudget,
	uint32 iLocalQuotaLimit,
	uint32 iRemoteQuotaLimit,
	uint32 iSharedLeaseLimit,
	uint32 iMigrationQuorumRequired,
	uint32 iNextJournalFlushAt,
	uint32 iNextMigrationWakeAt,
	uint32 iJournalRotateThreshold,
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
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "manifest_cursor", 0, (int32)iManifestCursor);
	xvoTableSetInt(vRoot, "archive_cursor", 0, (int32)iArchiveCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "checkpoint_version", 0, (int32)iCheckpointVersion);
	xvoTableSetInt(vRoot, "lease_journal_seq", 0, (int32)iLeaseJournalSeq);
	xvoTableSetInt(vRoot, "migration_ticket", 0, (int32)iMigrationTicket);
	xvoTableSetInt(vRoot, "migration_ack_count", 0, (int32)iMigrationAckCount);
	xvoTableSetInt(vRoot, "migration_reject_count", 0, (int32)iMigrationRejectCount);
	xvoTableSetInt(vRoot, "lease_epoch", 0, (int32)iLeaseEpoch);
	xvoTableSetInt(vRoot, "ownership_epoch", 0, (int32)iOwnershipEpoch);
	xvoTableSetInt(vRoot, "active_owner_slot", 0, (int32)iActiveOwnerSlot);
	xvoTableSetInt(vRoot, "active_migration_slot", 0, (int32)iActiveMigrationSlot);
	xvoTableSetInt(vRoot, "shared_lease_in_use", 0, (int32)iSharedLeaseInUse);
	xvoTableSetInt(vRoot, "granted_lease_window", 0, (int32)iGrantedLeaseWindow);
	xvoTableSetInt(vRoot, "owner_journal_queue", 0, (int32)iOwnerJournalQueue);
	xvoTableSetInt(vRoot, "migration_queue", 0, (int32)iMigrationQueue);
	xvoTableSetInt(vRoot, "audit_queue", 0, (int32)iAuditQueue);
	xvoTableSetInt(vRoot, "migration_retry_budget", 0, (int32)iMigrationRetryBudget);
	xvoTableSetInt(vRoot, "journal_flush_retry_budget", 0, (int32)iJournalFlushRetryBudget);
	xvoTableSetInt(vRoot, "local_quota_limit", 0, (int32)iLocalQuotaLimit);
	xvoTableSetInt(vRoot, "remote_quota_limit", 0, (int32)iRemoteQuotaLimit);
	xvoTableSetInt(vRoot, "shared_lease_limit", 0, (int32)iSharedLeaseLimit);
	xvoTableSetInt(vRoot, "migration_quorum_required", 0, (int32)iMigrationQuorumRequired);
	xvoTableSetInt(vRoot, "next_journal_flush_at", 0, (int32)iNextJournalFlushAt);
	xvoTableSetInt(vRoot, "next_migration_wake_at", 0, (int32)iNextMigrationWakeAt);
	xvoTableSetInt(vRoot, "journal_rotate_threshold", 0, (int32)iJournalRotateThreshold);
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

static xvalue procLoadLeaseJournalCheckpoint(const char* sPath)
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

static bool procWriteRemoteOwnershipMigrationSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	const char* sRemoteOwner,
	const char* sRemoteTargetOwner,
	uint32 iRemoteOwnershipEpoch,
	uint32 iRemotePublishIndex,
	uint32 iClusterCount)
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_ownership_migration_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "remote_owner", 0, (uint8*)((sRemoteOwner != NULL) ? sRemoteOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "remote_target_owner", 0, (uint8*)((sRemoteTargetOwner != NULL) ? sRemoteTargetOwner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_ownership_epoch", 0, (int32)iRemoteOwnershipEpoch);
	xvoTableSetInt(vRoot, "remote_publish_index", 0, (int32)iRemotePublishIndex);
	xvoTableSetInt(vRoot, "peer_count", 0, (int32)iClusterCount);
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
	DemoLeaseJournalTask* pTask = (DemoLeaseJournalTask*)pArg;
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

static bool procPrepareLeaseJournalRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoLeaseJournalPeer* pPeer,
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

	snprintf(sRef, sizeof(sRef), "/api/lease-journal/%s?round=%u", sKey, (unsigned)iRound);
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
	const DemoLeaseJournalPeer arrPeer[],
	int iPeerCount,
	uint32 iMaxLatencyMs,
	const DemoLeaseJournalPeer* arrOut[],
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

static const char* procChooseLeaseJournalPlan(const DemoLeaseJournalCenter* pCenter, int iPeerCount)
{
	uint32 iNow = (uint32)xrtNow();

	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iLeaseJournalSeq >= pCenter->tPolicy.iJournalRotateThreshold ) {
		if ( (pCenter->iActiveOwnerSlot == 0u) && (pCenter->iActiveMigrationSlot == 0u) ) {
			return "rotate_journal_now";
		}
	}

	if ( pCenter->iGrantedLeaseWindow == 0u ) {
		if ( pCenter->iMigrationRetryBudget == 0u ) {
			return "defer_migration_budget";
		}

		if ( pCenter->iNextJournalFlushAt > iNow ) {
			return "sleep_journal_flush";
		}

		if ( pCenter->iMigrationAckCount < pCenter->tPolicy.iMigrationQuorumRequired ) {
			return "append_owner_journal_now";
		}

		return "wait_migration_quorum";
	}

	if ( pCenter->iOwnerJournalQueue > 0u ) {
		if ( pCenter->iSharedLeaseInUse >= pCenter->iGrantedLeaseWindow ) {
			return "queue_granted_lease_window";
		}

		if ( pCenter->iActiveOwnerSlot >= pCenter->tPolicy.iLocalQuotaLimit ) {
			return "queue_owner_window";
		}

		if ( pCenter->iNextJournalFlushAt > iNow ) {
			return "sleep_journal_flush";
		}

		return "run_owner_migration_now";
	}

	if ( pCenter->iMigrationQueue > 0u ) {
		if ( pCenter->iSharedLeaseInUse >= pCenter->iGrantedLeaseWindow ) {
			return "queue_granted_lease_window";
		}

		if ( pCenter->iActiveMigrationSlot >= pCenter->tPolicy.iRemoteQuotaLimit ) {
			return "queue_migration_window";
		}

		if ( pCenter->iNextMigrationWakeAt > iNow ) {
			return "sleep_migration_wake";
		}

		return "run_target_owner_now";
	}

	return "observe";
}

static bool procRunLeaseJournalOwnershipMigration(
	DemoLeaseJournalCenter* pCenter,
	const DemoLeaseJournalPeer* arrPeer[],
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
	DemoLeaseJournalTask tJournalInspect;
	DemoLeaseJournalTask tOwnerInspect;
	DemoLeaseJournalTask tAuditInspect;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vRemote = NULL;
	char sJournalEntryPath[260];
	char sAuditPath[260];
	char sPublishJson[1400];
	int iChildCount = 0;
	int i;
	bool bOwnerDomain = false;
	bool bFinished = false;
	bool bMigrated = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrRemoteFetch, 0, sizeof(arrRemoteFetch));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tJournalInspect, 0, sizeof(tJournalInspect));
	memset(&tOwnerInspect, 0, sizeof(tOwnerInspect));
	memset(&tAuditInspect, 0, sizeof(tAuditInspect));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (iPeerCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	bOwnerDomain = (pCenter->iGrantedLeaseWindow > 0u) ? (pCenter->iOwnerJournalQueue > 0u) : true;
	pCenter->iSharedLeaseInUse++;

	if ( bOwnerDomain ) {
		pCenter->iActiveOwnerSlot++;
		if ( pCenter->iOwnerJournalQueue > 0u ) {
			pCenter->iOwnerJournalQueue--;
		}
	} else {
		pCenter->iActiveMigrationSlot++;
		if ( pCenter->iMigrationQueue > 0u ) {
			pCenter->iMigrationQueue--;
		}
	}

	(void)procSaveLeaseJournalCheckpoint(
		pCenter->sCheckpointPath,
		"lease_journal_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iLeaseJournalSeq,
		pCenter->iMigrationTicket,
		pCenter->iMigrationAckCount,
		pCenter->iMigrationRejectCount,
		pCenter->iLeaseEpoch,
		pCenter->iOwnershipEpoch,
		pCenter->iActiveOwnerSlot,
		pCenter->iActiveMigrationSlot,
		pCenter->iSharedLeaseInUse,
		pCenter->iGrantedLeaseWindow,
		pCenter->iOwnerJournalQueue,
		pCenter->iMigrationQueue,
		pCenter->iAuditQueue,
		pCenter->iMigrationRetryBudget,
		pCenter->iJournalFlushRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedLeaseLimit,
		pCenter->tPolicy.iMigrationQuorumRequired,
		pCenter->iNextJournalFlushAt,
		pCenter->iNextMigrationWakeAt,
		pCenter->tPolicy.iJournalRotateThreshold,
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

	procCopyText(tJournalInspect.sStage, sizeof(tJournalInspect.sStage), "inspect_current_journal");
	procCopyText(tJournalInspect.sPath, sizeof(tJournalInspect.sPath), pCenter->sJournalPath);
	tJournalInspect.iDelayMs = 20u;
	tJournalInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tJournalInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tOwnerInspect.sStage, sizeof(tOwnerInspect.sStage), "inspect_owner_checkpoint");
	procCopyText(tOwnerInspect.sPath, sizeof(tOwnerInspect.sPath), pCenter->sCheckpointPath);
	tOwnerInspect.iDelayMs = 25u;
	tOwnerInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tOwnerInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPrepareLeaseJournalRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			bOwnerDomain ? pCenter->tPolicy.iJournalTimeoutMs : pCenter->tPolicy.iMigrationTimeoutMs) ) {
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
			int32 iRemoteOwnershipEpoch = 0;
			int32 iRemotePublishIndex = 0;
			const char* sRemoteOwner = NULL;
			const char* sRemoteTargetOwner = NULL;

			vRemote = xrtParseJSON((str)arrResp[i]->pBody, arrResp[i]->iBodyLen);
			if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
				iRemoteOwnershipEpoch = xvoTableGetInt(vRemote, "remote_ownership_epoch", 0);
				iRemotePublishIndex = xvoTableGetInt(vRemote, "remote_publish_index", 0);
				sRemoteOwner = (const char*)xvoTableGetText(vRemote, "remote_owner", 0);
				sRemoteTargetOwner = (const char*)xvoTableGetText(vRemote, "remote_target_owner", 0);

				if ( (iRemoteOwnershipEpoch >= (int32)pCenter->iOwnershipEpoch) && (iRemotePublishIndex >= (int32)(pCenter->iCommittedRound - 1u)) ) {
					pCenter->iMigrationAckCount++;
					if ( sRemoteTargetOwner != NULL ) {
						procCopyText(pCenter->sWitnessCluster, sizeof(pCenter->sWitnessCluster), arrPeer[i]->sName);
					}
				} else {
					pCenter->iMigrationRejectCount++;
				}

				if ( sRemoteOwner != NULL ) {
					(void)sRemoteOwner;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	pCenter->iLeaseJournalSeq++;
	pCenter->iMigrationTicket++;

	if ( pCenter->iMigrationAckCount >= pCenter->tPolicy.iMigrationQuorumRequired ) {
		pCenter->iGrantedLeaseWindow = pCenter->tPolicy.iSharedLeaseLimit;
		pCenter->iLeaseEpoch++;
		pCenter->iOwnershipEpoch++;
		procCopyText(pCenter->sCurrentOwner, sizeof(pCenter->sCurrentOwner), pCenter->sTargetOwner);
		bMigrated = true;
	}

	if ( pCenter->iManifestCursor < pCenter->iTargetSegmentSeq ) {
		pCenter->iManifestCursor++;
	}
	if ( pCenter->iArchiveCursor < pCenter->iManifestCursor ) {
		pCenter->iArchiveCursor++;
	}
	if ( pCenter->iPublishIndex < pCenter->iCommittedRound ) {
		pCenter->iPublishIndex = pCenter->iCommittedRound;
	}
	pCenter->iCheckpointVersion++;

	(void)procWriteLeaseJournalEntry(
		pCenter->sJournalPath,
		sKey,
		"lease_journal_current_ready",
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iLeaseJournalSeq,
		pCenter->iMigrationTicket,
		pCenter->iOwnershipEpoch);

	procBuildLeaseJournalEntryPath(sJournalEntryPath, sizeof(sJournalEntryPath), pCenter->iLeaseJournalSeq);
	(void)procWriteLeaseJournalEntry(
		sJournalEntryPath,
		sKey,
		"lease_journal_entry_ready",
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iLeaseJournalSeq,
		pCenter->iMigrationTicket,
		pCenter->iOwnershipEpoch);

	procBuildOwnershipAuditPath(sAuditPath, sizeof(sAuditPath), pCenter->iMigrationTicket);
	(void)procWriteOwnershipMigrationAudit(
		sAuditPath,
		sKey,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iMigrationTicket,
		pCenter->iMigrationAckCount,
		pCenter->iMigrationRejectCount,
		pCenter->iOwnershipEpoch);

	(void)procWriteRemoteOwnershipMigrationSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->iOwnershipEpoch,
		pCenter->iPublishIndex,
		(uint32)iPeerCount);

	procCopyText(tAuditInspect.sStage, sizeof(tAuditInspect.sStage), "inspect_migration_audit");
	procCopyText(tAuditInspect.sPath, sizeof(tAuditInspect.sPath), sAuditPath);
	tAuditInspect.iDelayMs = 10u;
	tAuditInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tAuditInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		goto cleanup;
	}
	iChildCount++;

	if ( !bMigrated && (pCenter->iGrantedLeaseWindow == 0u) ) {
		pCenter->iAuditQueue++;
		pCenter->iSharedLeaseInUse = 0u;
		pCenter->iNextJournalFlushAt = (uint32)xrtNow() + pCenter->tPolicy.iJournalFlushIntervalMs;
		procCopyText(pCenter->sJournalState, sizeof(pCenter->sJournalState), "lease_journal_wait_migration_quorum");
	} else if ( pCenter->iLeaseJournalSeq >= pCenter->tPolicy.iJournalRotateThreshold ) {
		pCenter->iNextJournalFlushAt = (uint32)xrtNow() + pCenter->tPolicy.iJournalFlushIntervalMs;
		procCopyText(pCenter->sJournalState, sizeof(pCenter->sJournalState), "lease_journal_rotate_ready");
	} else if ( pCenter->iArchiveCursor >= pCenter->iTargetSegmentSeq ) {
		pCenter->iMigrationRetryBudget = pCenter->tPolicy.iMigrationRetryBudget;
		pCenter->iJournalFlushRetryBudget = pCenter->tPolicy.iJournalFlushRetryBudget;
		pCenter->iNextJournalFlushAt = 0u;
		pCenter->iNextMigrationWakeAt = 0u;
		procCopyText(pCenter->sJournalState, sizeof(pCenter->sJournalState), "lease_journal_migration_finished");
	} else {
		if ( bOwnerDomain ) {
			if ( pCenter->iJournalFlushRetryBudget > 0u ) {
				pCenter->iJournalFlushRetryBudget--;
			}
			pCenter->iMigrationQueue++;
			pCenter->iNextJournalFlushAt = (uint32)xrtNow() + pCenter->tPolicy.iJournalFlushIntervalMs;
			pCenter->iNextMigrationWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteMergeIntervalMs;
		} else {
			if ( pCenter->iMigrationRetryBudget > 0u ) {
				pCenter->iMigrationRetryBudget--;
			}
			pCenter->iAuditQueue++;
			pCenter->iNextMigrationWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteMergeIntervalMs;
		}
		procCopyText(pCenter->sJournalState, sizeof(pCenter->sJournalState), "lease_journal_migration_running");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"current_owner\": \"%s\",\n\t\"target_owner\": \"%s\",\n\t\"witness_cluster\": \"%s\",\n\t\"lease_epoch\": %u,\n\t\"ownership_epoch\": %u,\n\t\"lease_journal_seq\": %u,\n\t\"migration_ticket\": %u,\n\t\"migration_ack_count\": %u,\n\t\"migration_reject_count\": %u,\n\t\"active_owner_slot\": %u,\n\t\"active_migration_slot\": %u,\n\t\"shared_lease_in_use\": %u,\n\t\"granted_lease_window\": %u,\n\t\"owner_journal_queue\": %u,\n\t\"migration_queue\": %u,\n\t\"audit_queue\": %u,\n\t\"journal_flush_retry_budget\": %u,\n\t\"migration_retry_budget\": %u,\n\t\"next_journal_flush_at\": %u,\n\t\"next_migration_wake_at\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u\n}\n",
		pCenter->sJournalState,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		(unsigned)pCenter->iLeaseEpoch,
		(unsigned)pCenter->iOwnershipEpoch,
		(unsigned)pCenter->iLeaseJournalSeq,
		(unsigned)pCenter->iMigrationTicket,
		(unsigned)pCenter->iMigrationAckCount,
		(unsigned)pCenter->iMigrationRejectCount,
		(unsigned)pCenter->iActiveOwnerSlot,
		(unsigned)pCenter->iActiveMigrationSlot,
		(unsigned)pCenter->iSharedLeaseInUse,
		(unsigned)pCenter->iGrantedLeaseWindow,
		(unsigned)pCenter->iOwnerJournalQueue,
		(unsigned)pCenter->iMigrationQueue,
		(unsigned)pCenter->iAuditQueue,
		(unsigned)pCenter->iJournalFlushRetryBudget,
		(unsigned)pCenter->iMigrationRetryBudget,
		(unsigned)pCenter->iNextJournalFlushAt,
		(unsigned)pCenter->iNextMigrationWakeAt,
		(unsigned)pCenter->iPublishIndex,
		(unsigned)pCenter->iCheckpointVersion);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveLeaseJournalCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sJournalState : "lease_journal_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sCurrentOwner,
		pCenter->sTargetOwner,
		pCenter->sWitnessCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iLeaseJournalSeq,
		pCenter->iMigrationTicket,
		pCenter->iMigrationAckCount,
		pCenter->iMigrationRejectCount,
		pCenter->iLeaseEpoch,
		pCenter->iOwnershipEpoch,
		pCenter->iActiveOwnerSlot,
		pCenter->iActiveMigrationSlot,
		pCenter->iSharedLeaseInUse,
		pCenter->iGrantedLeaseWindow,
		pCenter->iOwnerJournalQueue,
		pCenter->iMigrationQueue,
		pCenter->iAuditQueue,
		pCenter->iMigrationRetryBudget,
		pCenter->iJournalFlushRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedLeaseLimit,
		pCenter->tPolicy.iMigrationQuorumRequired,
		pCenter->iNextJournalFlushAt,
		pCenter->iNextMigrationWakeAt,
		pCenter->tPolicy.iJournalRotateThreshold,
		bFinished ? 1u : 0u);

	if ( pCenter->iSharedLeaseInUse > 0u ) {
		pCenter->iSharedLeaseInUse--;
	}
	if ( bOwnerDomain ) {
		if ( pCenter->iActiveOwnerSlot > 0u ) {
			pCenter->iActiveOwnerSlot--;
		}
	} else {
		if ( pCenter->iActiveMigrationSlot > 0u ) {
			pCenter->iActiveMigrationSlot--;
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
	DemoLeaseJournalCenter tCenter;
	DemoLeaseJournalPeer arrPeer[DEMO_MAX_PEER];
	const DemoLeaseJournalPeer* arrSelected[DEMO_MAX_PEER];
	xvalue vLoaded = NULL;
	str sCommittedOwner = NULL;
	str sCurrentOwner = NULL;
	str sTargetOwner = NULL;
	str sJournalState = NULL;
	const char* sPlan = NULL;
	int iPeerCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iJournalTimeoutMs = 3000u;
	tCenter.tPolicy.iMigrationTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iLocalQuotaLimit = 1u;
	tCenter.tPolicy.iRemoteQuotaLimit = 1u;
	tCenter.tPolicy.iSharedLeaseLimit = 2u;
	tCenter.tPolicy.iMigrationQuorumRequired = 2u;
	tCenter.tPolicy.iJournalRotateThreshold = 32u;
	tCenter.tPolicy.iMigrationRetryBudget = 2u;
	tCenter.tPolicy.iJournalFlushRetryBudget = 3u;
	tCenter.tPolicy.iJournalFlushIntervalMs = 10u;
	tCenter.tPolicy.iRemoteMergeIntervalMs = 20u;
	tCenter.iTerm = 24u;
	tCenter.iCommittedRound = 9u;
	tCenter.iSnapshotIndex = 9u;
	tCenter.iManifestCursor = 6u;
	tCenter.iArchiveCursor = 5u;
	tCenter.iTargetSegmentSeq = 8u;
	tCenter.iPublishIndex = 9u;
	tCenter.iCheckpointVersion = 13u;
	tCenter.iLeaseJournalSeq = 27u;
	tCenter.iMigrationTicket = 41u;
	tCenter.iLeaseEpoch = 14u;
	tCenter.iOwnershipEpoch = 9u;
	tCenter.iGrantedLeaseWindow = 0u;
	tCenter.iOwnerJournalQueue = 1u;
	tCenter.iMigrationQueue = 2u;
	tCenter.iAuditQueue = 1u;
	tCenter.iMigrationRetryBudget = tCenter.tPolicy.iMigrationRetryBudget;
	tCenter.iJournalFlushRetryBudget = tCenter.tPolicy.iJournalFlushRetryBudget;
	tCenter.iNextJournalFlushAt = (uint32)xrtNow() - 1u;
	tCenter.iNextMigrationWakeAt = (uint32)xrtNow() + 15u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sCurrentOwner, sizeof(tCenter.sCurrentOwner), "node-b");
	procCopyText(tCenter.sTargetOwner, sizeof(tCenter.sTargetOwner), "node-c");
	procCopyText(tCenter.sWitnessCluster, sizeof(tCenter.sWitnessCluster), "cluster-b");
	procCopyText(tCenter.sJournalState, sizeof(tCenter.sJournalState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/lease-journal-ownership-migration-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/lease-journal-ownership-migration-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-ownership-migration-summary.json");
	procCopyText(tCenter.sJournalPath, sizeof(tCenter.sJournalPath), "runtime/lease-journal-current.json");

	(void)procSaveLeaseJournalCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		tCenter.sWitnessCluster,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iManifestCursor,
		tCenter.iArchiveCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		tCenter.iCheckpointVersion,
		tCenter.iLeaseJournalSeq,
		tCenter.iMigrationTicket,
		tCenter.iMigrationAckCount,
		tCenter.iMigrationRejectCount,
		tCenter.iLeaseEpoch,
		tCenter.iOwnershipEpoch,
		tCenter.iActiveOwnerSlot,
		tCenter.iActiveMigrationSlot,
		tCenter.iSharedLeaseInUse,
		tCenter.iGrantedLeaseWindow,
		tCenter.iOwnerJournalQueue,
		tCenter.iMigrationQueue,
		tCenter.iAuditQueue,
		tCenter.iMigrationRetryBudget,
		tCenter.iJournalFlushRetryBudget,
		tCenter.tPolicy.iLocalQuotaLimit,
		tCenter.tPolicy.iRemoteQuotaLimit,
		tCenter.tPolicy.iSharedLeaseLimit,
		tCenter.tPolicy.iMigrationQuorumRequired,
		tCenter.iNextJournalFlushAt,
		tCenter.iNextMigrationWakeAt,
		tCenter.tPolicy.iJournalRotateThreshold,
		0u);

	(void)procWriteLeaseJournalEntry(
		tCenter.sJournalPath,
		"console-service",
		"lease_journal_current_boot",
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		tCenter.iLeaseJournalSeq,
		tCenter.iMigrationTicket,
		tCenter.iOwnershipEpoch);

	vLoaded = procLoadLeaseJournalCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sCommittedOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sCurrentOwner = xvoTableGetText(vLoaded, "current_owner", 0);
	sTargetOwner = xvoTableGetText(vLoaded, "target_owner", 0);
	sJournalState = xvoTableGetText(vLoaded, "state", 0);
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iManifestCursor = (uint32)xvoTableGetInt(vLoaded, "manifest_cursor", 0);
	tCenter.iArchiveCursor = (uint32)xvoTableGetInt(vLoaded, "archive_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iLeaseJournalSeq = (uint32)xvoTableGetInt(vLoaded, "lease_journal_seq", 0);
	tCenter.iMigrationTicket = (uint32)xvoTableGetInt(vLoaded, "migration_ticket", 0);
	tCenter.iMigrationAckCount = (uint32)xvoTableGetInt(vLoaded, "migration_ack_count", 0);
	tCenter.iMigrationRejectCount = (uint32)xvoTableGetInt(vLoaded, "migration_reject_count", 0);
	tCenter.iLeaseEpoch = (uint32)xvoTableGetInt(vLoaded, "lease_epoch", 0);
	tCenter.iOwnershipEpoch = (uint32)xvoTableGetInt(vLoaded, "ownership_epoch", 0);
	tCenter.iActiveOwnerSlot = (uint32)xvoTableGetInt(vLoaded, "active_owner_slot", 0);
	tCenter.iActiveMigrationSlot = (uint32)xvoTableGetInt(vLoaded, "active_migration_slot", 0);
	tCenter.iSharedLeaseInUse = (uint32)xvoTableGetInt(vLoaded, "shared_lease_in_use", 0);
	tCenter.iGrantedLeaseWindow = (uint32)xvoTableGetInt(vLoaded, "granted_lease_window", 0);
	tCenter.iOwnerJournalQueue = (uint32)xvoTableGetInt(vLoaded, "owner_journal_queue", 0);
	tCenter.iMigrationQueue = (uint32)xvoTableGetInt(vLoaded, "migration_queue", 0);
	tCenter.iAuditQueue = (uint32)xvoTableGetInt(vLoaded, "audit_queue", 0);
	tCenter.iMigrationRetryBudget = (uint32)xvoTableGetInt(vLoaded, "migration_retry_budget", 0);
	tCenter.iJournalFlushRetryBudget = (uint32)xvoTableGetInt(vLoaded, "journal_flush_retry_budget", 0);
	tCenter.tPolicy.iLocalQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "local_quota_limit", (int32)tCenter.tPolicy.iLocalQuotaLimit);
	tCenter.tPolicy.iRemoteQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "remote_quota_limit", (int32)tCenter.tPolicy.iRemoteQuotaLimit);
	tCenter.tPolicy.iSharedLeaseLimit = (uint32)xvoTableGetInt(vLoaded, "shared_lease_limit", (int32)tCenter.tPolicy.iSharedLeaseLimit);
	tCenter.tPolicy.iMigrationQuorumRequired = (uint32)xvoTableGetInt(vLoaded, "migration_quorum_required", (int32)tCenter.tPolicy.iMigrationQuorumRequired);
	tCenter.iNextJournalFlushAt = (uint32)xvoTableGetInt(vLoaded, "next_journal_flush_at", 0);
	tCenter.iNextMigrationWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_migration_wake_at", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sCommittedOwner);
	procCopyText(tCenter.sCurrentOwner, sizeof(tCenter.sCurrentOwner), (const char*)sCurrentOwner);
	procCopyText(tCenter.sTargetOwner, sizeof(tCenter.sTargetOwner), (const char*)sTargetOwner);
	procCopyText(tCenter.sJournalState, sizeof(tCenter.sJournalState), (const char*)sJournalState);
	xvoUnref(vLoaded);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "cluster-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:9201");
	arrPeer[0].iLatencyMs = 25u;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "cluster-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:9202");
	arrPeer[1].iLatencyMs = 31u;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "cluster-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:9203");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].bHealthy = false;

	iPeerCount = procSelectPeers(
		arrPeer,
		DEMO_MAX_PEER,
		tCenter.tPolicy.iMaxPeerLatencyMs,
		arrSelected,
		DEMO_MAX_PEER);

	sPlan = procChooseLeaseJournalPlan(&tCenter, iPeerCount);

	printf("lease journal ownership migration plan = %s\n", sPlan);
	printf(
		"committed_owner=%s current_owner=%s target_owner=%s lease_epoch=%u ownership_epoch=%u lease_journal_seq=%u migration_ticket=%u ack=%u reject=%u active_owner_slot=%u active_migration_slot=%u shared_lease_in_use=%u granted_lease_window=%u owner_journal_queue=%u migration_queue=%u audit_queue=%u journal_flush_retry_budget=%u migration_retry_budget=%u next_journal_flush=%u next_migration_wake=%u journal_state=%s witness_cluster=%s\n",
		tCenter.sCommittedOwner,
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iOwnershipEpoch,
		(unsigned)tCenter.iLeaseJournalSeq,
		(unsigned)tCenter.iMigrationTicket,
		(unsigned)tCenter.iMigrationAckCount,
		(unsigned)tCenter.iMigrationRejectCount,
		(unsigned)tCenter.iActiveOwnerSlot,
		(unsigned)tCenter.iActiveMigrationSlot,
		(unsigned)tCenter.iSharedLeaseInUse,
		(unsigned)tCenter.iGrantedLeaseWindow,
		(unsigned)tCenter.iOwnerJournalQueue,
		(unsigned)tCenter.iMigrationQueue,
		(unsigned)tCenter.iAuditQueue,
		(unsigned)tCenter.iJournalFlushRetryBudget,
		(unsigned)tCenter.iMigrationRetryBudget,
		(unsigned)tCenter.iNextJournalFlushAt,
		(unsigned)tCenter.iNextMigrationWakeAt,
		tCenter.sJournalState,
		tCenter.sWitnessCluster);

	if ( (strcmp(sPlan, "append_owner_journal_now") == 0) || (strcmp(sPlan, "run_owner_migration_now") == 0) || (strcmp(sPlan, "run_target_owner_now") == 0) ) {
		if ( !procRunLeaseJournalOwnershipMigration(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "lease journal ownership migration failed\n");
			return 1;
		}
	}

	printf(
		"after lease journal ownership migration: current_owner=%s target_owner=%s lease_epoch=%u ownership_epoch=%u lease_journal_seq=%u migration_ticket=%u ack=%u reject=%u owner_journal_queue=%u migration_queue=%u audit_queue=%u publish=%u version=%u journal_state=%s witness_cluster=%s\n",
		tCenter.sCurrentOwner,
		tCenter.sTargetOwner,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iOwnershipEpoch,
		(unsigned)tCenter.iLeaseJournalSeq,
		(unsigned)tCenter.iMigrationTicket,
		(unsigned)tCenter.iMigrationAckCount,
		(unsigned)tCenter.iMigrationRejectCount,
		(unsigned)tCenter.iOwnerJournalQueue,
		(unsigned)tCenter.iMigrationQueue,
		(unsigned)tCenter.iAuditQueue,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sJournalState,
		tCenter.sWitnessCluster);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `current_owner` 改了，不等于 owner migration 已完成

这页最容易偷懒的写法是：

- 看到 `current_owner == target_owner` 就认为迁移已完成
- 看到 publish 文件已经写出，就认为 journal append、audit export 和 checkpoint 更新一定都已经完成
- 看到 quorum 已过，就直接把上一轮 journal 覆盖掉

但更稳的模型应该是：

- `lease coordination finished`
	- 回答 lease window、当前 lease owner 和 quorum 是否已经正式收口
- `ownership migration finished`
	- 回答 committed owner、target owner、ownership epoch 和 migration ticket 是否已经正式收口
- `lease journal finished`
	- 回答 journal entry、journal rotate、audit export 和 published summary 是否也已经正式收口

### 6.2 `lease_journal_seq / migration_ticket / ownership_epoch` 不是一个字段

这页也很容易写混：

- 把 `lease_journal_seq` 直接当成“owner 已经迁了多少轮”
- 把 `migration_ticket` 直接当成 journal seq
- 把 `ownership_epoch` 直接当成 publish 次数

但更稳的边界应该是：

- `lease_journal_seq`
	- 回答当前 durable journal 已经真正写到了哪一条
- `migration_ticket`
	- 回答当前 owner migration 请求属于哪一轮协调
- `ownership_epoch`
	- 回答当前 committed owner 语义已经正式推进到了哪一轮
- `migration_ack_count / migration_reject_count`
	- 回答这轮 owner migration 的 quorum 到底已经收到了多少票
- `journal_flush_retry_budget / migration_retry_budget`
	- 回答 journal flush 和 remote migration 各自还允许失败后再拉起几次

### 6.3 audit export 可以提高可观测性，但不能代替 journal

这页还很容易偷懒成：

- publish 里已经有 `current_owner / target_owner`
- 所以就不再单独写 journal entry
- 或者只保留 audit，不保留 durable lease journal

但更稳的写法应该是：

- journal entry 负责 durable owner 迁移语义
- audit export 负责对外解释这一轮迁移到底怎样完成
- published summary 负责对外读取，不负责替代 journal replay
- journal rotate 必须等老 journal 已经被正式吸收，而不是只看文件大小

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 lease journal 的压缩、分段回放和历史裁剪，应继续补更重的 `lease journal compaction` 主线
- 如果需要把 owner 迁移扩展到多租户、多资源类型和跨 cluster replay，应继续补更重的 `ownership migration replay` 主线
- 如果需要把 journal、迁移、回放和仲裁统一进一个更长生命周期的控制面，应继续补更重的 `lease control-plane orchestration` 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / current_owner / target_owner / lease_epoch / ownership_epoch / lease_journal_seq / migration_ticket / migration_ack_count / migration_reject_count / owner_journal_queue / migration_queue / audit_queue / next_journal_flush_at / next_migration_wake_at / journal_state`
2. `POST /api/lease-journal-ownership-migration`
	- 只提交 owner migration 意图，让 worker 决定是否进入 `append_owner_journal_now / wait_migration_quorum / queue_granted_lease_window / queue_owner_window / run_owner_migration_now / queue_migration_window / run_target_owner_now / sleep_journal_flush / sleep_migration_wake / defer_migration_budget / rotate_journal_now`
3. `GET /api/lease-journal-ownership-migration`
	- 直接返回最近一次 lease journal / ownership migration checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 journal flush、migration quorum 复核、audit export、journal rotate 和 deferred migration 迁移
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 current owner、target owner、journal seq、migration ticket、双预算、双 wake、recent journal history 和 recent audit history

## 9. 下一步阅读

如果你准备继续把 `lease journal + ownership migration` 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个快照租约协调面板](snapshot-lease-coordination-dashboard.md)
	先对比“lease 怎样续租或接管”和“ownership 怎样被 durable journal 正式接住”到底差在哪里
2. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
	对比“谁赢得 ownership”与“赢了之后怎样写成正式迁移 journal”到底差在哪里
3. [把本地控制台服务升级成一个 lease journal compaction 与 ownership migration replay 面板](lease-journal-compaction-migration-replay-dashboard.md)
	下一页开始把 journal 裁剪、journal replay 和跨 cluster 迁移回放收成新的正式主线
