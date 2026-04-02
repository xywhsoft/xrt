# 把本地控制台服务升级成一个快照分布式配额仲裁面板

> 这页要解决的不是“上一页已经有 shared quota 了，所以再多记几个计数器就行了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state`，并且上一页已经能把“跨节点 shared quota + domain quota”收成一条正式调度主线之后，又开始需要回答“谁给这轮 quota 投了赞成票”“什么时候只是 `wait_quorum_vote`，什么时候才真正拿到 `granted_quota`”“为什么 lease epoch 不能只靠 publish 成功反推”“为什么 reject vote 不能和 peer retry budget 混成一类字段”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在多个节点同时抢 slot 时靠“谁先发请求谁先拿走资源”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local segment 落盘和 published summary 收进同一个 takeover scope
- 快照续传编排怎样把多轮 install、publish 和 checkpoint 更新收成长期主线
- 快照持久化怎样把 `manifest_cursor / archive_cursor / checkpoint_version / publish` 收成 durable baseline
- 快照调度配额怎样把 `active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` 收成单队列 admission 主线
- 快照多队列调度怎样把 `active_local_slot / active_remote_slot / local_queue / remote_queue / defer_queue / next_global_wake_at / next_remote_wake_at` 收成本机双域 admission 主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- key-a 需要本机立刻接管，应该先吃掉 local domain 的 quota
- key-b、key-c 需要跨节点拉取 remote summary，不该和 key-a 抢同一份 local slot
- 当前 `local_quota_limit = 1`、`remote_quota_limit = 1`
- 但全局还要受 `shared_quota_limit = 2` 约束
- 当前 `local_queue = 1`、`remote_queue = 2`
- 当前 `shared_retry_budget = 3`、`peer_retry_budget = 2`
- 当前 `grant_vote_count = 0`、`reject_vote_count = 0`
- 当前 `granted_quota = 0`，说明这轮还没真正过仲裁
- `next_global_wake_at` 和 `next_remote_wake_at` 不一样
- 当前 peer grant 还带着 `arbitration_epoch = 4`
- 如果此时再来一波 `POST /api/consensus-snapshot-distributed-quota-arbitration`，系统必须知道是 `run_arbitration_now`、`wait_quorum_vote`、`queue_granted_quota`、`queue_local_domain`、`run_local_now`、`queue_remote_domain`、`run_remote_now`、`sleep_remote_wake`、`defer_shared_budget`，还是 `defer_peer_budget`

如果这时还把实现停在：

- `if ( queue > 0 ) run next;`
- `if ( active < quota ) admit;`
- `if ( fail ) put back to queue;`

很快就会出现几个典型问题：

- 本机 local queue 看起来还能跑，但其实这轮根本还没拿到 `granted_quota`
- remote queue 明明还在 peer cooldown，却被本地 worker 当成“可以立刻 admission”
- `grant_vote_count / reject_vote_count` 和 `shared_retry_budget / peer_retry_budget` 混成一类字段，结果“投票没过”和“预算没了”被写成一种失败
- publish 已成功，但没有明确写出本次 admitted 的到底是 local 还是 remote，排障时只能猜

所以这页真正要补出的，不是“继续把 quota 再加重一点”，而是：

> 当 snapshot scheduler 已经进入跨节点长期运行状态之后，怎样把 `active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state` 和真正的 manifest / archive / publish 收口统一进一条可恢复、可解释、可导出的正式分布式配额仲裁主线。

## 2. 为什么这次不能只靠“有 persistence 就够了”

### 2.1 上一页只回答“共享 quota 怎样被看见”，不回答“谁真正把它仲裁下来”

上一页的快照分布式调度配额已经很好地解决了：

- 当前 local domain 和 remote domain 怎样各自拿到本机 slot
- shared quota 怎样和 domain quota、wake 时间一起收口
- `shared_quota_in_use / local_queue / remote_queue / next_global_wake_at / next_remote_wake_at` 怎样写回 checkpoint

但它不回答：

- 为什么这轮 quota 还要再过一次 `grant_vote_count / reject_vote_count / quorum_required`
- 为什么 `granted_quota` 不能直接等于 `shared_quota_limit`
- 为什么 `lease_epoch` 不能只靠 publish 成功反推
- 为什么 admission 结果里必须明确写出“本次是 wait quorum、local 还是 remote”

### 2.2 `shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch` 不是一类字段

到了这一层，系统真正要稳定回答的是：

- 当前 `shared_quota_in_use` 到底已经占了几份全局 admission
- 当前 `granted_quota` 到底有没有真的被仲裁下来
- 当前 `grant_vote_count / reject_vote_count` 到底已经看到几票
- 当前 `lease_epoch` 到底代表哪一轮有效配额租约
- 当前 `active_local_slot` 还有没有空位
- 当前 `active_remote_slot` 还有没有空位
- 当前 `local_queue` 里还有多少本机 key 没被 admission
- 当前 `remote_queue` 里还有多少跨节点任务还在等 peer domain
- 当前 `defer_queue` 到底是共享预算耗尽，还是 peer domain 暂时不给放行

这说明：

- `snapshot persistence checkpoint`
	- 回答 manifest / archive / checkpoint version 当前推进到了哪
- `snapshot multi-queue checkpoint`
	- 回答本机 local / remote admission 当前推进到了哪
- `snapshot distributed quota arbitration checkpoint`
	- 回答 shared quota、grant/reject 票数、lease epoch、domain quota 和双预算当前到底卡在哪

### 2.3 这次真正新增的是“quorum vote + granted quota + lease epoch”状态层

更稳的分工方式是：

- `snapshot persistence`
	- 负责 durable manifest、archive、checkpoint version 和 publish 的长期收口
- `snapshot distributed scheduler / quota`
	- 负责 shared quota、domain quota、双预算和 wake 的长期收口
- `snapshot distributed quota arbitration`
	- 再往下负责 `grant_vote_count / reject_vote_count / quorum_required / granted_quota / lease_epoch / arbitration_epoch` 的长期收口

一句话记住：

> 上一页补的是“多节点怎样共享 quota”，这一页补的是“谁真正把这轮 quota 仲裁下来”。

## 3. 这条快照分布式配额仲裁主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active distributed arbitration registry | 当前有哪些 key 正在 local domain、remote domain、defer queue、granted quota 和 lease 上 |
| `list` | recent scheduler history | 页面和 JSON 展示最近 `run_arbitration_now / wait_quorum_vote / queue_granted_quota / queue_local_domain / run_local_now / queue_remote_domain / run_remote_now / defer` 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有被 admission |
| `queue + thread` | 后台消费 `ENQUEUE / VOTE / HANDOFF / SWEEP` | 请求线程不阻塞在跨节点投票、租约刷新和 wake 判断上 |
| `task group` | 管住一次 admitted persistence scope 里的 inspect / remote fetch / manifest child / archive child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote vote、manifest stage、archive stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot distributed quota arbitration summary | 让远端 `target_segment_seq / remote_archive_cursor / remote_publish_index / arbitration_epoch / lease_epoch` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local stage、写 manifest stage、写 archive stage、读取 distributed quota arbitration checkpoint | 让 scheduler 进入正式文件主线 |
| `file_async` | 把 published distributed quota arbitration summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state` 有正式结构 |
| `time` | 记录 global wake、remote wake、lease 过期时间、arbitration epoch 更新时间、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `manifest_cursor / archive_cursor / checkpoint_version / active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照分布式调度配额管“多节点怎样共享 quota”，快照分布式配额仲裁管“谁真正拿到这轮 granted quota”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot distributed quota arbitration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-distributed-quota-arbitration-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-manifest/<round>-<seq>.json
runtime/consensus-snapshot-archive/<round>-<seq>.json
runtime/consensus-snapshot-distributed-quota-arbitration-published.json
runtime/remote-consensus-snapshot-distributed-quota-arbitration-summary.json
```

其中：

- `config/console.json`
	- 保存 `local_queue_timeout_ms`、`remote_queue_timeout_ms`、`local_quota_limit`、`remote_quota_limit`、`shared_quota_limit`、`quorum_required`、`lease_ttl_ms`、`shared_retry_budget`、`peer_retry_budget`、`global_sweep_interval_ms`、`remote_sweep_interval_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state`
- `runtime/console.log`
	- 记录 `run_arbitration_now / wait_quorum_vote / queue_granted_quota / queue_local_domain / run_local_now / queue_remote_domain / run_remote_now`、grant/reject vote 变化、lease 刷新、remote summary merge、manifest 推进、archive 推进和 publish
- `runtime/consensus-snapshot-distributed-quota-arbitration-checkpoint.json`
	- 保存最近一次正式 snapshot distributed quota arbitration 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-manifest/<round>-<seq>.json`
	- 保存这轮 manifest 推进的中间文件
- `runtime/consensus-snapshot-archive/<round>-<seq>.json`
	- 保存这轮 archive 推进的中间文件
- `runtime/consensus-snapshot-distributed-quota-arbitration-published.json`
	- 保存异步发布后的 distributed quota arbitration 输出
- `runtime/remote-consensus-snapshot-distributed-quota-arbitration-summary.json`
	- 保存这次 remote distributed quota arbitration summary 的本地 stage 文件

## 5. 先把“启动装载 distributed quota arbitration checkpoint -> 判断 local / remote + shared quota admission -> admitted 后再推进 manifest / archive / publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-distributed-quota-arbitration-checkpoint.json`
2. checkpoint 里明确记录 `manifest_cursor / archive_cursor / checkpoint_version / active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state`
3. `dict` 表示当前 active distributed quota registry
4. `list` 表示 scheduler / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 distributed quota arbitration admission 意图”
6. `task group` 先管住本次 admitted persistence scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer distributed quota arbitration summary 走多条 `xhttp future`
9. admission 逻辑先决定 `run_arbitration_now / wait_quorum_vote / queue_granted_quota / queue_local_domain / run_local_now / queue_remote_domain / run_remote_now / sleep_global_wake / sleep_remote_wake / defer_shared_budget / defer_peer_budget`
10. admitted 后再统一推进 `manifest_cursor / archive_cursor / local_segment_seq / publish_index / checkpoint_version`
11. 最后再用 `file_async future` 异步发布 committed distributed quota arbitration 结果，并把 `granted_quota / grant_vote_count / reject_vote_count / lease_epoch / arbitration_epoch` 一起导出

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`manifest_cursor = 4`，`archive_cursor = 3`
- 当前 `local_quota_limit = 1`、`remote_quota_limit = 1`、`shared_quota_limit = 2`
- 当前 `local_queue = 1`、`remote_queue = 2`
- 当前 `shared_retry_budget = 3`、`peer_retry_budget = 2`
- 当前 `granted_quota = 0`、`grant_vote_count = 0`、`reject_vote_count = 0`
- 当前 `lease_epoch = 12`、`arbitration_epoch = 4`
- worker 会从 `peer-a`、`peer-b` 拉两份 distributed quota arbitration summary
- 只有 quorum vote 过线后，才会把 local inspect、remote summary gather、manifest stage、archive stage 和 publish 收进同一个 distributed quota arbitration scope
- 最后异步发布 `runtime/consensus-snapshot-distributed-quota-arbitration-published.json`

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
	uint32 iLocalQueueTimeoutMs;
	uint32 iRemoteQueueTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iLocalQuotaLimit;
	uint32 iRemoteQuotaLimit;
	uint32 iSharedQuotaLimit;
	uint32 iQuorumRequired;
	uint32 iLeaseTTLms;
	uint32 iSharedRetryBudget;
	uint32 iPeerRetryBudget;
	uint32 iGlobalSweepIntervalMs;
	uint32 iRemoteSweepIntervalMs;
} DemoSnapshotDistributedQuotaArbitrationPolicy;

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
	DemoSnapshotDistributedQuotaArbitrationPolicy tPolicy;
	uint32 iActiveLocalSlot;
	uint32 iActiveRemoteSlot;
	uint32 iSharedQuotaInUse;
	uint32 iGrantedQuota;
	uint32 iGrantVoteCount;
	uint32 iRejectVoteCount;
	uint32 iLeaseEpoch;
	uint32 iLocalQueue;
	uint32 iRemoteQueue;
	uint32 iDeferQueue;
	uint32 iSharedRetryBudget;
	uint32 iPeerRetryBudget;
	uint32 iNextGlobalWakeAt;
	uint32 iNextRemoteWakeAt;
	uint32 iArbitrationEpoch;
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
	char sArbitrationState[48];
	char sSelectedCluster[16];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotDistributedQuotaArbitrationCenter;

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

static bool procSaveConsensusSnapshotDistributedQuotaArbitrationCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledPeer,
	const char* sSelectedCluster,
	uint32 iTerm,
	uint32 iCommittedRound,
	uint32 iSnapshotIndex,
	uint32 iLocalSegmentSeq,
	uint32 iManifestCursor,
	uint32 iArchiveCursor,
	uint32 iTargetSegmentSeq,
	uint32 iPublishIndex,
	uint32 iCheckpointVersion,
	uint32 iActiveLocalSlot,
	uint32 iActiveRemoteSlot,
	uint32 iSharedQuotaInUse,
	uint32 iGrantedQuota,
	uint32 iGrantVoteCount,
	uint32 iRejectVoteCount,
	uint32 iLeaseEpoch,
	uint32 iLocalQueue,
	uint32 iRemoteQueue,
	uint32 iDeferQueue,
	uint32 iSharedRetryBudget,
	uint32 iPeerRetryBudget,
	uint32 iLocalQuotaLimit,
	uint32 iRemoteQuotaLimit,
	uint32 iSharedQuotaLimit,
	uint32 iQuorumRequired,
	uint32 iNextGlobalWakeAt,
	uint32 iNextRemoteWakeAt,
	uint32 iArbitrationEpoch,
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
	xvoTableSetText(vRoot, "selected_cluster", 0, (uint8*)((sSelectedCluster != NULL) ? sSelectedCluster : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "committed_round", 0, (int32)iCommittedRound);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "local_segment_seq", 0, (int32)iLocalSegmentSeq);
	xvoTableSetInt(vRoot, "manifest_cursor", 0, (int32)iManifestCursor);
	xvoTableSetInt(vRoot, "archive_cursor", 0, (int32)iArchiveCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
	xvoTableSetInt(vRoot, "publish_index", 0, (int32)iPublishIndex);
	xvoTableSetInt(vRoot, "checkpoint_version", 0, (int32)iCheckpointVersion);
	xvoTableSetInt(vRoot, "active_local_slot", 0, (int32)iActiveLocalSlot);
	xvoTableSetInt(vRoot, "active_remote_slot", 0, (int32)iActiveRemoteSlot);
	xvoTableSetInt(vRoot, "shared_quota_in_use", 0, (int32)iSharedQuotaInUse);
	xvoTableSetInt(vRoot, "granted_quota", 0, (int32)iGrantedQuota);
	xvoTableSetInt(vRoot, "grant_vote_count", 0, (int32)iGrantVoteCount);
	xvoTableSetInt(vRoot, "reject_vote_count", 0, (int32)iRejectVoteCount);
	xvoTableSetInt(vRoot, "lease_epoch", 0, (int32)iLeaseEpoch);
	xvoTableSetInt(vRoot, "local_queue", 0, (int32)iLocalQueue);
	xvoTableSetInt(vRoot, "remote_queue", 0, (int32)iRemoteQueue);
	xvoTableSetInt(vRoot, "defer_queue", 0, (int32)iDeferQueue);
	xvoTableSetInt(vRoot, "shared_retry_budget", 0, (int32)iSharedRetryBudget);
	xvoTableSetInt(vRoot, "peer_retry_budget", 0, (int32)iPeerRetryBudget);
	xvoTableSetInt(vRoot, "local_quota_limit", 0, (int32)iLocalQuotaLimit);
	xvoTableSetInt(vRoot, "remote_quota_limit", 0, (int32)iRemoteQuotaLimit);
	xvoTableSetInt(vRoot, "shared_quota_limit", 0, (int32)iSharedQuotaLimit);
	xvoTableSetInt(vRoot, "quorum_required", 0, (int32)iQuorumRequired);
	xvoTableSetInt(vRoot, "next_global_wake_at", 0, (int32)iNextGlobalWakeAt);
	xvoTableSetInt(vRoot, "next_remote_wake_at", 0, (int32)iNextRemoteWakeAt);
	xvoTableSetInt(vRoot, "arbitration_epoch", 0, (int32)iArbitrationEpoch);
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

static xvalue procLoadConsensusSnapshotDistributedQuotaArbitrationCheckpoint(const char* sCheckpointPath)
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

static bool procWriteRemoteSnapshotDistributedQuotaArbitrationSummaryStage(
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_distributed_scheduler_summary_ready", 0, FALSE);
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

static bool procPrepareSnapshotDistributedQuotaArbitrationRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-distributed-quota-arbitration/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotDistributedQuotaArbitrationPlan(const DemoSnapshotDistributedQuotaArbitrationCenter* pCenter, int iPeerCount)
{
	uint32 iNow = (uint32)xrtNow();

	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iGrantedQuota == 0u ) {
		if ( pCenter->iSharedRetryBudget == 0u ) {
			return "defer_shared_budget";
		}

		if ( pCenter->iNextGlobalWakeAt > iNow ) {
			return "sleep_global_wake";
		}

		if ( pCenter->iGrantVoteCount < pCenter->tPolicy.iQuorumRequired ) {
			return "run_arbitration_now";
		}

		return "wait_quorum_vote";
	}

	if ( pCenter->iLocalQueue > 0u ) {
		if ( pCenter->iSharedQuotaInUse >= pCenter->iGrantedQuota ) {
			return "queue_granted_quota";
		}

		if ( pCenter->iActiveLocalSlot >= pCenter->tPolicy.iLocalQuotaLimit ) {
			return "queue_local_domain";
		}

		if ( (pCenter->iSharedRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_shared_budget";
		}

		if ( pCenter->iNextGlobalWakeAt > iNow ) {
			return "sleep_global_wake";
		}

		return "run_local_now";
	}

	if ( pCenter->iRemoteQueue > 0u ) {
		if ( pCenter->iSharedQuotaInUse >= pCenter->iGrantedQuota ) {
			return "queue_granted_quota";
		}

		if ( pCenter->iActiveRemoteSlot >= pCenter->tPolicy.iRemoteQuotaLimit ) {
			return "queue_remote_domain";
		}

		if ( (pCenter->iPeerRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_peer_budget";
		}

		if ( pCenter->iNextRemoteWakeAt > iNow ) {
			return "sleep_remote_wake";
		}

		return "run_remote_now";
	}

	if ( (pCenter->iManifestCursor < pCenter->iTargetSegmentSeq) || (pCenter->iArchiveCursor < pCenter->iManifestCursor) ) {
		if ( pCenter->iSharedQuotaInUse >= pCenter->iGrantedQuota ) {
			return "queue_granted_quota";
		}

		return "run_remote_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotDistributedQuotaArbitration(
	DemoSnapshotDistributedQuotaArbitrationCenter* pCenter,
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
	bool bLocalDomain = false;
	bool bFinished = false;
	bool bArbitrated = false;

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

	bLocalDomain = (pCenter->iGrantedQuota > 0u) ? (pCenter->iLocalQueue > 0u) : true;
	procCopyText(pCenter->sSelectedCluster, sizeof(pCenter->sSelectedCluster), bLocalDomain ? "local" : "remote");
	pCenter->iSharedQuotaInUse++;

	if ( bLocalDomain ) {
		pCenter->iActiveLocalSlot++;
		if ( pCenter->iLocalQueue > 0u ) {
			pCenter->iLocalQueue--;
		}
	} else {
		pCenter->iActiveRemoteSlot++;
		if ( pCenter->iRemoteQueue > 0u ) {
			pCenter->iRemoteQueue--;
		}
	}

	(void)procSaveConsensusSnapshotDistributedQuotaArbitrationCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_distributed_quota_arbitration_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->sSelectedCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveLocalSlot,
		pCenter->iActiveRemoteSlot,
		pCenter->iSharedQuotaInUse,
		pCenter->iGrantedQuota,
		pCenter->iGrantVoteCount,
		pCenter->iRejectVoteCount,
		pCenter->iLeaseEpoch,
		pCenter->iLocalQueue,
		pCenter->iRemoteQueue,
		pCenter->iDeferQueue,
		pCenter->iSharedRetryBudget,
		pCenter->iPeerRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedQuotaLimit,
		pCenter->tPolicy.iQuorumRequired,
		pCenter->iNextGlobalWakeAt,
		pCenter->iNextRemoteWakeAt,
		pCenter->iArbitrationEpoch,
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
		if ( !procPrepareSnapshotDistributedQuotaArbitrationRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			bLocalDomain ? pCenter->tPolicy.iLocalQueueTimeoutMs : pCenter->tPolicy.iRemoteQueueTimeoutMs) ) {
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

				if ( (iRemoteTarget >= (int32)pCenter->iTargetSegmentSeq) && (iRemotePublish >= (int32)(pCenter->iCommittedRound - 1u)) ) {
					pCenter->iGrantVoteCount++;
				} else {
					pCenter->iRejectVoteCount++;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iGrantVoteCount >= pCenter->tPolicy.iQuorumRequired ) {
		pCenter->iGrantedQuota = pCenter->tPolicy.iSharedQuotaLimit;
		pCenter->iLeaseEpoch++;
		pCenter->iArbitrationEpoch++;
		bArbitrated = true;
	} else {
		pCenter->iGrantedQuota = 0u;
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
	(void)procWriteRemoteSnapshotDistributedQuotaArbitrationSummaryStage(
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

	if ( !bArbitrated && (pCenter->iGrantedQuota == 0u) ) {
		pCenter->iDeferQueue++;
		pCenter->iSharedQuotaInUse = 0u;
		pCenter->iNextGlobalWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iGlobalSweepIntervalMs;
		procCopyText(pCenter->sArbitrationState, sizeof(pCenter->sArbitrationState), "snapshot_distributed_quota_arbitration_wait_quorum");
	} else if ( pCenter->iArchiveCursor >= pCenter->iTargetSegmentSeq ) {
		pCenter->iSharedRetryBudget = pCenter->tPolicy.iSharedRetryBudget;
		pCenter->iPeerRetryBudget = pCenter->tPolicy.iPeerRetryBudget;
		pCenter->iNextGlobalWakeAt = 0u;
		pCenter->iNextRemoteWakeAt = 0u;
		procCopyText(pCenter->sArbitrationState, sizeof(pCenter->sArbitrationState), "snapshot_distributed_quota_arbitration_finished");
	} else {
		if ( bLocalDomain ) {
			if ( pCenter->iSharedRetryBudget > 0u ) {
				pCenter->iSharedRetryBudget--;
			}
			pCenter->iRemoteQueue++;
			pCenter->iNextGlobalWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iGlobalSweepIntervalMs;
			pCenter->iNextRemoteWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteSweepIntervalMs;
		} else {
			if ( pCenter->iPeerRetryBudget > 0u ) {
				pCenter->iPeerRetryBudget--;
			}
			pCenter->iRemoteQueue++;
			pCenter->iNextRemoteWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteSweepIntervalMs;
		}
		pCenter->iDeferQueue++;
		procCopyText(pCenter->sArbitrationState, sizeof(pCenter->sArbitrationState), "snapshot_distributed_quota_arbitration_running");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"selected_cluster\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"manifest_cursor\": %u,\n\t\"archive_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u,\n\t\"active_local_slot\": %u,\n\t\"active_remote_slot\": %u,\n\t\"shared_quota_in_use\": %u,\n\t\"granted_quota\": %u,\n\t\"grant_vote_count\": %u,\n\t\"reject_vote_count\": %u,\n\t\"lease_epoch\": %u,\n\t\"local_queue\": %u,\n\t\"remote_queue\": %u,\n\t\"defer_queue\": %u,\n\t\"shared_retry_budget\": %u,\n\t\"peer_retry_budget\": %u,\n\t\"local_quota_limit\": %u,\n\t\"remote_quota_limit\": %u,\n\t\"shared_quota_limit\": %u,\n\t\"quorum_required\": %u,\n\t\"next_global_wake_at\": %u,\n\t\"next_remote_wake_at\": %u,\n\t\"arbitration_epoch\": %u,\n\t\"arbitration_state\": \"%s\"\n}\n",
		pCenter->sArbitrationState,
		pCenter->sSelectedCluster,
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
		(unsigned)pCenter->iActiveLocalSlot,
		(unsigned)pCenter->iActiveRemoteSlot,
		(unsigned)pCenter->iSharedQuotaInUse,
		(unsigned)pCenter->iGrantedQuota,
		(unsigned)pCenter->iGrantVoteCount,
		(unsigned)pCenter->iRejectVoteCount,
		(unsigned)pCenter->iLeaseEpoch,
		(unsigned)pCenter->iLocalQueue,
		(unsigned)pCenter->iRemoteQueue,
		(unsigned)pCenter->iDeferQueue,
		(unsigned)pCenter->iSharedRetryBudget,
		(unsigned)pCenter->iPeerRetryBudget,
		(unsigned)pCenter->tPolicy.iLocalQuotaLimit,
		(unsigned)pCenter->tPolicy.iRemoteQuotaLimit,
		(unsigned)pCenter->tPolicy.iSharedQuotaLimit,
		(unsigned)pCenter->tPolicy.iQuorumRequired,
		(unsigned)pCenter->iNextGlobalWakeAt,
		(unsigned)pCenter->iNextRemoteWakeAt,
		(unsigned)pCenter->iArbitrationEpoch,
		pCenter->sArbitrationState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotDistributedQuotaArbitrationCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sArbitrationState : "consensus_snapshot_distributed_quota_arbitration_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledPeer,
		pCenter->sSelectedCluster,
		pCenter->iTerm,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iManifestCursor,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		pCenter->iCheckpointVersion,
		pCenter->iActiveLocalSlot,
		pCenter->iActiveRemoteSlot,
		pCenter->iSharedQuotaInUse,
		pCenter->iGrantedQuota,
		pCenter->iGrantVoteCount,
		pCenter->iRejectVoteCount,
		pCenter->iLeaseEpoch,
		pCenter->iLocalQueue,
		pCenter->iRemoteQueue,
		pCenter->iDeferQueue,
		pCenter->iSharedRetryBudget,
		pCenter->iPeerRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedQuotaLimit,
		pCenter->tPolicy.iQuorumRequired,
		pCenter->iNextGlobalWakeAt,
		pCenter->iNextRemoteWakeAt,
		pCenter->iArbitrationEpoch,
		bFinished ? 1u : 0u);

	if ( pCenter->iSharedQuotaInUse > 0u ) {
		pCenter->iSharedQuotaInUse--;
	}

	if ( bLocalDomain ) {
		if ( pCenter->iActiveLocalSlot > 0u ) {
			pCenter->iActiveLocalSlot--;
		}
	} else {
		if ( pCenter->iActiveRemoteSlot > 0u ) {
			pCenter->iActiveRemoteSlot--;
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
	DemoSnapshotDistributedQuotaArbitrationCenter tCenter;
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

	tCenter.tPolicy.iLocalQueueTimeoutMs = 3000u;
	tCenter.tPolicy.iRemoteQueueTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iLocalQuotaLimit = 1u;
	tCenter.tPolicy.iRemoteQuotaLimit = 1u;
	tCenter.tPolicy.iSharedQuotaLimit = 2u;
	tCenter.tPolicy.iQuorumRequired = 2u;
	tCenter.tPolicy.iLeaseTTLms = 60u;
	tCenter.tPolicy.iSharedRetryBudget = 3u;
	tCenter.tPolicy.iPeerRetryBudget = 2u;
	tCenter.tPolicy.iGlobalSweepIntervalMs = 10u;
	tCenter.tPolicy.iRemoteSweepIntervalMs = 20u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 4u;
	tCenter.iManifestCursor = 4u;
	tCenter.iArchiveCursor = 3u;
	tCenter.iTargetSegmentSeq = 7u;
	tCenter.iPublishIndex = 8u;
	tCenter.iCheckpointVersion = 11u;
	tCenter.iArbitrationEpoch = 4u;
	tCenter.iLeaseEpoch = 12u;
	tCenter.iLocalQueue = 1u;
	tCenter.iRemoteQueue = 2u;
	tCenter.iGrantedQuota = 0u;
	tCenter.iGrantVoteCount = 0u;
	tCenter.iRejectVoteCount = 0u;
	tCenter.iSharedRetryBudget = tCenter.tPolicy.iSharedRetryBudget;
	tCenter.iPeerRetryBudget = tCenter.tPolicy.iPeerRetryBudget;
	tCenter.iNextGlobalWakeAt = (uint32)xrtNow() - 1u;
	tCenter.iNextRemoteWakeAt = (uint32)xrtNow() + 30u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sArbitrationState, sizeof(tCenter.sArbitrationState), "boot");
	procCopyText(tCenter.sSelectedCluster, sizeof(tCenter.sSelectedCluster), "pending");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-distributed-quota-arbitration-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-distributed-quota-arbitration-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-distributed-quota-arbitration-summary.json");

	(void)procSaveConsensusSnapshotDistributedQuotaArbitrationCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledPeer,
		tCenter.sSelectedCluster,
		tCenter.iTerm,
		tCenter.iCommittedRound,
		tCenter.iSnapshotIndex,
		tCenter.iLocalSegmentSeq,
		tCenter.iManifestCursor,
		tCenter.iArchiveCursor,
		tCenter.iTargetSegmentSeq,
		tCenter.iPublishIndex,
		tCenter.iCheckpointVersion,
		tCenter.iActiveLocalSlot,
		tCenter.iActiveRemoteSlot,
		tCenter.iSharedQuotaInUse,
		tCenter.iGrantedQuota,
		tCenter.iGrantVoteCount,
		tCenter.iRejectVoteCount,
		tCenter.iLeaseEpoch,
		tCenter.iLocalQueue,
		tCenter.iRemoteQueue,
		tCenter.iDeferQueue,
		tCenter.iSharedRetryBudget,
		tCenter.iPeerRetryBudget,
		tCenter.tPolicy.iLocalQuotaLimit,
		tCenter.tPolicy.iRemoteQuotaLimit,
		tCenter.tPolicy.iSharedQuotaLimit,
		tCenter.tPolicy.iQuorumRequired,
		tCenter.iNextGlobalWakeAt,
		tCenter.iNextRemoteWakeAt,
		tCenter.iArbitrationEpoch,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotDistributedQuotaArbitrationCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledPeer = xvoTableGetText(vLoaded, "installed_peer", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	procCopyText(tCenter.sSelectedCluster, sizeof(tCenter.sSelectedCluster), (const char*)xvoTableGetText(vLoaded, "selected_cluster", 0));
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iManifestCursor = (uint32)xvoTableGetInt(vLoaded, "manifest_cursor", 0);
	tCenter.iArchiveCursor = (uint32)xvoTableGetInt(vLoaded, "archive_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iActiveLocalSlot = (uint32)xvoTableGetInt(vLoaded, "active_local_slot", 0);
	tCenter.iActiveRemoteSlot = (uint32)xvoTableGetInt(vLoaded, "active_remote_slot", 0);
	tCenter.iSharedQuotaInUse = (uint32)xvoTableGetInt(vLoaded, "shared_quota_in_use", 0);
	tCenter.iGrantedQuota = (uint32)xvoTableGetInt(vLoaded, "granted_quota", 0);
	tCenter.iGrantVoteCount = (uint32)xvoTableGetInt(vLoaded, "grant_vote_count", 0);
	tCenter.iRejectVoteCount = (uint32)xvoTableGetInt(vLoaded, "reject_vote_count", 0);
	tCenter.iLeaseEpoch = (uint32)xvoTableGetInt(vLoaded, "lease_epoch", 0);
	tCenter.iLocalQueue = (uint32)xvoTableGetInt(vLoaded, "local_queue", 0);
	tCenter.iRemoteQueue = (uint32)xvoTableGetInt(vLoaded, "remote_queue", 0);
	tCenter.iDeferQueue = (uint32)xvoTableGetInt(vLoaded, "defer_queue", 0);
	tCenter.iSharedRetryBudget = (uint32)xvoTableGetInt(vLoaded, "shared_retry_budget", 0);
	tCenter.iPeerRetryBudget = (uint32)xvoTableGetInt(vLoaded, "peer_retry_budget", 0);
	tCenter.tPolicy.iLocalQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "local_quota_limit", (int32)tCenter.tPolicy.iLocalQuotaLimit);
	tCenter.tPolicy.iRemoteQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "remote_quota_limit", (int32)tCenter.tPolicy.iRemoteQuotaLimit);
	tCenter.tPolicy.iSharedQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "shared_quota_limit", (int32)tCenter.tPolicy.iSharedQuotaLimit);
	tCenter.tPolicy.iQuorumRequired = (uint32)xvoTableGetInt(vLoaded, "quorum_required", (int32)tCenter.tPolicy.iQuorumRequired);
	tCenter.iNextGlobalWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_global_wake_at", 0);
	tCenter.iNextRemoteWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_remote_wake_at", 0);
	tCenter.iArbitrationEpoch = (uint32)xvoTableGetInt(vLoaded, "arbitration_epoch", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sArbitrationState, sizeof(tCenter.sArbitrationState), (const char*)sState);
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

	sPlan = procChooseSnapshotDistributedQuotaArbitrationPlan(&tCenter, iPeerCount);

	printf("snapshot distributed quota arbitration plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_cluster=%s active_local=%u active_remote=%u shared_in_use=%u granted_quota=%u grant_votes=%u reject_votes=%u lease_epoch=%u local_queue=%u remote_queue=%u defer_queue=%u shared_retry=%u peer_retry=%u next_global=%u next_remote=%u arbitration_epoch=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sSelectedCluster,
		(unsigned)tCenter.iActiveLocalSlot,
		(unsigned)tCenter.iActiveRemoteSlot,
		(unsigned)tCenter.iSharedQuotaInUse,
		(unsigned)tCenter.iGrantedQuota,
		(unsigned)tCenter.iGrantVoteCount,
		(unsigned)tCenter.iRejectVoteCount,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iLocalQueue,
		(unsigned)tCenter.iRemoteQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iSharedRetryBudget,
		(unsigned)tCenter.iPeerRetryBudget,
		(unsigned)tCenter.iNextGlobalWakeAt,
		(unsigned)tCenter.iNextRemoteWakeAt,
		(unsigned)tCenter.iArbitrationEpoch,
		tCenter.sInstalledPeer);

	if ( (strcmp(sPlan, "run_arbitration_now") == 0) || (strcmp(sPlan, "run_local_now") == 0) || (strcmp(sPlan, "run_remote_now") == 0) ) {
		if ( !procRunConsensusSnapshotDistributedQuotaArbitration(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot distributed quota arbitration failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot distributed quota arbitration: snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_cluster=%s active_local=%u active_remote=%u shared_in_use=%u granted_quota=%u grant_votes=%u reject_votes=%u lease_epoch=%u local_queue=%u remote_queue=%u defer_queue=%u shared_retry=%u peer_retry=%u next_global=%u next_remote=%u arbitration_epoch=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sSelectedCluster,
		(unsigned)tCenter.iActiveLocalSlot,
		(unsigned)tCenter.iActiveRemoteSlot,
		(unsigned)tCenter.iSharedQuotaInUse,
		(unsigned)tCenter.iGrantedQuota,
		(unsigned)tCenter.iGrantVoteCount,
		(unsigned)tCenter.iRejectVoteCount,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iLocalQueue,
		(unsigned)tCenter.iRemoteQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iSharedRetryBudget,
		(unsigned)tCenter.iPeerRetryBudget,
		(unsigned)tCenter.iNextGlobalWakeAt,
		(unsigned)tCenter.iNextRemoteWakeAt,
		(unsigned)tCenter.iArbitrationEpoch,
		tCenter.sArbitrationState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot scheduler finished` 不等于 `snapshot distributed quota arbitration finished`

这页最容易偷懒的写法是：

- 看到 `archive_cursor >= target_segment_seq` 就认为 distributed quota arbitration 已完成
- 看到 publish 文件已经写出，就认为 local / remote slot 和 shared quota 一定都已经释放
- 看到某一轮刚失败过，就直接把整个 key 标成永久 defer

但更稳的模型应该是：

- `snapshot persistence finished`
	- 回答 manifest、archive、checkpoint version 和 publish 是否都已经正式 durable
- `snapshot multi-queue finished`
	- 回答本机 local / remote admission、双 wake 和本机 queue 长度是不是已经正式收口
- `snapshot distributed quota arbitration finished`
	- 回答当前 `granted_quota / grant_vote_count / reject_vote_count / lease_epoch / arbitration_epoch`、shared quota、domain quota 和双预算是不是也已经正式收口

### 6.2 `active_local_slot / active_remote_slot / shared_quota_in_use / local_queue / remote_queue` 不是一个字段

这页也很容易写混：

- 把 `active_local_slot = 0` 直接等同于“现在完全没有本机接管压力了”
- 或者把 `shared_quota_in_use = 1` 直接当成“remote domain 一定还能再拿 slot”
- 或者把 `remote_queue > 0` 直接当成“远端 domain 一定马上能跑”
- 或者把 `shared_retry_budget > 0` 直接当成“peer budget 也一定还够”
- 或者把 `next_global_wake_at` 和 `next_remote_wake_at` 直接当成同一个冷却时间

但更稳的边界应该是：

- `active_local_slot`
	- 回答当前 local domain 还有几个 admitted scope 正在真正占用资源
- `active_remote_slot`
	- 回答当前 remote domain 还有几个 admitted scope 正在真正占用资源
- `shared_quota_in_use`
	- 回答整个 distributed admission domain 当前已经正式占了多少份共享 quota
- `local_queue / remote_queue`
	- 回答不同 domain 里到底还有多少 key 等待 admission
- `shared_retry_budget / peer_retry_budget`
	- 回答 shared domain 和 remote peer 当前还允许失败后再被拉起几次
- `next_global_wake_at / next_remote_wake_at`
	- 回答 global domain 和 remote domain 最早什么时候可以合法再看一眼，而不是被彼此的冷却时间拖住
- `granted_quota / grant_vote_count / reject_vote_count`
	- 回答这轮 shared quota 到底有没有真的过仲裁、当前已经累计几票赞成和反对
- `lease_epoch / arbitration_epoch`
	- 回答当前 checkpoint 看到的是哪一轮有效租约、哪一轮 peer grant 视图，而不是单纯的计数器

### 6.3 remote summary 可以提高优先级，但不能直接绕过 admission

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地就直接把 plan 改成 `run_local_now`
- 即使 shared quota 已满、remote domain 还排着，也照样强行抢 slot

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`、peer 选择、`grant_vote_count / reject_vote_count`、`arbitration_epoch` 和当前 domain 判断
- 真正的 `run_local_now / run_remote_now` 还得等 quorum vote、`granted_quota`、domain quota、queue 长度和 wake 时间一起通过
- 真正的 slot 释放还得等 admitted scope 正式 close / join / publish 结束
- 真正的下一轮重试还得等 shared budget 或 peer budget 和 wake 时间一起满足

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输、校验和断点续拉协议，应继续补更重的 snapshot transport 主线
- 如果需要把 scheduler 扩展到多类 key、多级 quota domain 和更细的 peer 授权，应继续补更重的 snapshot distributed quota arbitration 主线
- 如果需要把 scheduler 主线扩展到跨集群 quota 汇总、全局 lease 协调和统一预算仲裁，应继续补更重的 cross-cluster scheduler / lease coordination 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_local_slot / active_remote_slot / shared_quota_in_use / granted_quota / grant_vote_count / reject_vote_count / lease_epoch / local_queue / remote_queue / defer_queue / shared_retry_budget / peer_retry_budget / next_global_wake_at / next_remote_wake_at / arbitration_epoch / arbitration_state`
2. `POST /api/consensus-snapshot-distributed-quota-arbitration`
	- 只提交 distributed quota arbitration admission 意图，让 worker 决定是否进入 `run_arbitration_now / wait_quorum_vote / queue_granted_quota / queue_local_domain / run_local_now / queue_remote_domain / run_remote_now / sleep_global_wake / sleep_remote_wake / defer_shared_budget / defer_peer_budget`
3. `GET /api/consensus-snapshot-distributed-quota-arbitration`
	- 直接返回最近一次 snapshot distributed quota arbitration checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 shared quota 复核、quorum vote 刷新、lease 续期、remote wake 判断、peer budget 刷新和 defer 迁移
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 manifest 进度、archive 进度、checkpoint version、local / remote quota、shared quota in use、`granted_quota / grant_vote_count / reject_vote_count / lease_epoch / arbitration_epoch`、双队列长度、双 wake 和 recent scheduler history

## 9. 下一步阅读

如果你准备继续把 snapshot distributed quota arbitration 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个快照分布式调度配额面板](snapshot-distributed-scheduler-quota-dashboard.md)
	先对比“多节点怎样共享 quota”和“谁真正把这轮 quota 仲裁下来”到底差在哪里
2. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
	对比“peer fan-out / summary gather / publish orchestration”和“quorum vote / lease epoch / granted quota”到底差在哪里
3. [把本地控制台服务升级成一个快照跨集群调度面板](snapshot-cross-cluster-scheduler-dashboard.md)
	继续看为什么“共享 quota 已经过票”还不够，系统还必须把 home cluster / failover cluster 的正式排班、cluster wake 和 shared cluster budget 收成新的调度状态






