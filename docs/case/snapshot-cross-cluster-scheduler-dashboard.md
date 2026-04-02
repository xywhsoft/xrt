# 把本地控制台服务升级成一个快照跨集群调度面板

> 这页要解决的不是“上一页已经把共享 quota 仲裁出来了，所以再多挂几个 cluster 名字就行了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state`，并且上一页已经能把“这一轮共享 quota 到底有没有过票”收成正式状态之后，又开始需要回答“home cluster 和 failover cluster 到底谁先排进当前窗口”“什么时候只是 `wait_cluster_quorum`，什么时候才真正进入 `queue_granted_cluster_budget` 或 `run_home_cluster_now`”“为什么 lease epoch 不能只靠 publish 成功反推”“为什么 shared cluster budget 和 failover cluster budget 不能混成一个冷却字段”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在多个 cluster 同时争抢恢复窗口时靠“谁先发请求谁先拿走资源”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local segment 落盘和 published summary 收进同一个 takeover scope
- 快照续传编排怎样把多轮 install、publish 和 checkpoint 更新收成长期主线
- 快照持久化怎样把 `manifest_cursor / archive_cursor / checkpoint_version / publish` 收成 durable baseline
- 快照调度配额怎样把 `active_slot / waiting_queue / retry_budget / next_wake_at / scheduler_state` 收成单队列 admission 主线
- 快照多队列调度怎样把 `active_home_cluster_slot / active_failover_cluster_slot / home_cluster_queue / failover_cluster_queue / defer_queue / next_shared_wake_at / next_failover_wake_at` 收成本机双域 admission 主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- key-a 需要优先留在 `home cluster` 本地恢复
- key-b、key-c 已经进入 failover 路线，不该再和 key-a 抢同一份 home cluster slot
- 当前 `local_quota_limit = 1`、`remote_quota_limit = 1`
- 但全局还要受 `shared_quota_limit = 2` 约束
- 当前 `home_cluster_queue = 1`、`failover_cluster_queue = 2`
- 当前 `shared_cluster_retry_budget = 3`、`failover_cluster_retry_budget = 2`
- 当前 `grant_cluster_vote_count = 0`、`reject_cluster_vote_count = 0`
- 当前 `granted_cluster_quota = 0`，说明这轮共享 cluster quota 还没真正放行
- `next_shared_wake_at` 和 `next_failover_wake_at` 不一样
- 当前 cluster grant 还带着 `cluster_epoch = 4`
- 如果此时再来一波 `POST /api/consensus-snapshot-cross-cluster-scheduler`，系统必须知道是 `run_cluster_vote_now`、`wait_cluster_quorum`、`queue_granted_cluster_budget`、`queue_home_cluster_domain`、`run_home_cluster_now`、`queue_failover_cluster_domain`、`run_failover_cluster_now`、`sleep_failover_cluster_wake`、`defer_shared_cluster_budget`，还是 `defer_failover_cluster_budget`

如果这时还把实现停在：

- `if ( queue > 0 ) run next;`
- `if ( active < quota ) admit;`
- `if ( fail ) put back to queue;`

很快就会出现几个典型问题：

- 本机 `home_cluster_queue` 看起来还能跑，但其实这轮根本还没拿到 `granted_cluster_quota`
- `failover_cluster_queue` 明明还在 failover cluster cooldown，却被本地 worker 当成“可以立刻 admission”
- `grant_cluster_vote_count / reject_cluster_vote_count` 和 `shared_cluster_retry_budget / failover_cluster_retry_budget` 混成一类字段，结果“投票没过”和“预算没了”被写成一种失败
- publish 已成功，但没有明确写出本次 admitted 的到底是 local 还是 remote，排障时只能猜

所以这页真正要补出的，不是“继续把 quota 再加重一点”，而是：

> 当 snapshot scheduler 已经进入跨 cluster 长期运行状态之后，怎样把 `active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state` 和真正的 manifest / archive / publish 收口统一进一条可恢复、可解释、可导出的正式跨集群调度主线。

## 2. 为什么这次不能只靠“有 persistence 就够了”

### 2.1 上一页只回答“共享 quota 有没有过票”，不回答“过票以后两个 cluster 怎样正式排班”

上一页的快照分布式调度配额已经很好地解决了：

- 当前 home cluster 和 failover cluster 怎样各自拿到本轮 slot
- shared cluster quota 怎样和 domain quota、wake 时间一起收口
- `shared_cluster_quota_in_use / home_cluster_queue / failover_cluster_queue / next_shared_wake_at / next_failover_wake_at` 怎样写回 checkpoint

但它不回答：

- 为什么这轮 quota 还要再过一次 `grant_cluster_vote_count / reject_cluster_vote_count / cluster_quorum_required`
- 为什么 `granted_cluster_quota` 不能直接等于 `shared_quota_limit`
- 为什么 `lease_epoch` 不能只靠 publish 成功反推
- 为什么 admission 结果里必须明确写出“本次是 wait quorum、home cluster 还是 failover cluster”

### 2.2 `shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch` 不是一类字段

到了这一层，系统真正要稳定回答的是：

- 当前 `shared_cluster_quota_in_use` 到底已经占了几份全局 admission
- 当前 `granted_cluster_quota` 到底有没有真的被仲裁下来
- 当前 `grant_cluster_vote_count / reject_cluster_vote_count` 到底已经看到几票
- 当前 `lease_epoch` 到底代表哪一轮有效配额租约
- 当前 `active_home_cluster_slot` 还有没有空位
- 当前 `active_failover_cluster_slot` 还有没有空位
- 当前 `home_cluster_queue` 里还有多少本机 key 没被 admission
- 当前 `failover_cluster_queue` 里还有多少跨节点任务还在等 failover cluster domain
- 当前 `defer_queue` 到底是共享预算耗尽，还是 failover cluster domain 暂时不给放行

这说明：

- `snapshot persistence checkpoint`
	- 回答 manifest / archive / checkpoint version 当前推进到了哪
- `snapshot multi-queue checkpoint`
	- 回答本机 home / failover cluster admission 当前推进到了哪
- `snapshot cross-cluster scheduler checkpoint`
	- 回答 shared quota、grant/reject 票数、lease epoch、domain quota 和双预算当前到底卡在哪

### 2.3 这次真正新增的是“quorum vote + granted quota + lease epoch”状态层

更稳的分工方式是：

- `snapshot persistence`
	- 负责 durable manifest、archive、checkpoint version 和 publish 的长期收口
- `snapshot distributed scheduler / quota`
	- 负责 shared quota、domain quota、双预算和 wake 的长期收口
- `snapshot cross-cluster scheduler`
	- 再往下负责 `grant_cluster_vote_count / reject_cluster_vote_count / cluster_quorum_required / granted_cluster_quota / lease_epoch / cluster_epoch`，以及 home / failover cluster 的正式排班收口

一句话记住：

> 上一页补的是“这轮共享 quota 到底有没有过票”，这一页补的是“过票以后 home cluster 和 failover cluster 怎样正式排班”。

## 3. 这条快照跨集群调度主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active cross-cluster scheduler registry | 当前有哪些 key 正在 home cluster、failover cluster、defer queue、granted quota 和 lease 上 |
| `list` | recent scheduler history | 页面和 JSON 展示最近 `run_cluster_vote_now / wait_cluster_quorum / queue_granted_cluster_budget / queue_home_cluster_domain / run_home_cluster_now / queue_failover_cluster_domain / run_failover_cluster_now / defer` 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有被 admission |
| `queue + thread` | 后台消费 `ENQUEUE / VOTE / HANDOFF / SWEEP` | 请求线程不阻塞在跨节点投票、租约刷新和 wake 判断上 |
| `task group` | 管住一次 admitted persistence scope 里的 inspect / remote fetch / manifest child / archive child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote vote、manifest stage、archive stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot cross-cluster scheduler summary | 让远端 `target_segment_seq / remote_archive_cursor / remote_publish_index / cluster_epoch / lease_epoch` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local stage、写 manifest stage、写 archive stage、读取 cross-cluster scheduler checkpoint | 让 scheduler 进入正式文件主线 |
| `file_async` | 把 published cross-cluster scheduler summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state` 有正式结构 |
| `time` | 记录 shared wake、failover wake、lease 过期时间、cluster epoch 更新时间、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `manifest_cursor / archive_cursor / checkpoint_version / active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照分布式配额仲裁管“这轮共享 quota 到底有没有过票”，快照跨集群调度管“过票以后哪个 cluster 先真正进入恢复窗口”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot cross-cluster scheduler 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-cross-cluster-scheduler-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-manifest/<round>-<seq>.json
runtime/consensus-snapshot-archive/<round>-<seq>.json
runtime/consensus-snapshot-cross-cluster-scheduler-published.json
runtime/remote-consensus-snapshot-cross-cluster-scheduler-summary.json
```

其中：

- `config/console.json`
	- 保存 `home_cluster_queue_timeout_ms`、`failover_cluster_queue_timeout_ms`、`local_quota_limit`、`remote_quota_limit`、`shared_quota_limit`、`cluster_quorum_required`、`lease_ttl_ms`、`shared_cluster_retry_budget`、`failover_cluster_retry_budget`、`global_sweep_interval_ms`、`remote_sweep_interval_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state`
- `runtime/console.log`
	- 记录 `run_cluster_vote_now / wait_cluster_quorum / queue_granted_cluster_budget / queue_home_cluster_domain / run_home_cluster_now / queue_failover_cluster_domain / run_failover_cluster_now`、grant/reject vote 变化、lease 刷新、remote summary merge、manifest 推进、archive 推进和 publish
- `runtime/consensus-snapshot-cross-cluster-scheduler-checkpoint.json`
	- 保存最近一次正式 snapshot cross-cluster scheduler 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-manifest/<round>-<seq>.json`
	- 保存这轮 manifest 推进的中间文件
- `runtime/consensus-snapshot-archive/<round>-<seq>.json`
	- 保存这轮 archive 推进的中间文件
- `runtime/consensus-snapshot-cross-cluster-scheduler-published.json`
	- 保存异步发布后的 cross-cluster scheduler 输出
- `runtime/remote-consensus-snapshot-cross-cluster-scheduler-summary.json`
	- 保存这次 remote cross-cluster scheduler summary 的本地 stage 文件

## 5. 先把“启动装载 cross-cluster scheduler checkpoint -> 判断 home / failover cluster admission -> admitted 后再推进 manifest / archive / publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-cross-cluster-scheduler-checkpoint.json`
2. checkpoint 里明确记录 `manifest_cursor / archive_cursor / checkpoint_version / active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state`
3. `dict` 表示当前 active cross-cluster scheduler registry
4. `list` 表示 scheduler / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 cross-cluster scheduler admission 意图”
6. `task group` 先管住本次 admitted persistence scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 cluster control-plane summary 走多条 `xhttp future`
9. admission 逻辑先决定 `run_cluster_vote_now / wait_cluster_quorum / queue_granted_cluster_budget / queue_home_cluster_domain / run_home_cluster_now / queue_failover_cluster_domain / run_failover_cluster_now / sleep_shared_wake / sleep_failover_cluster_wake / defer_shared_cluster_budget / defer_failover_cluster_budget`
10. admitted 后再统一推进 `manifest_cursor / archive_cursor / local_segment_seq / publish_index / checkpoint_version`
11. 最后再用 `file_async future` 异步发布 committed cross-cluster scheduler 结果，并把 `granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / cluster_epoch` 一起导出

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`manifest_cursor = 4`，`archive_cursor = 3`
- 当前 `local_quota_limit = 1`、`remote_quota_limit = 1`、`shared_quota_limit = 2`
- 当前 `home_cluster_queue = 1`、`failover_cluster_queue = 2`
- 当前 `shared_cluster_retry_budget = 3`、`failover_cluster_retry_budget = 2`
- 当前 `granted_cluster_quota = 0`、`grant_cluster_vote_count = 0`、`reject_cluster_vote_count = 0`
- 当前 `lease_epoch = 12`、`cluster_epoch = 4`
- worker 会从 `cluster-a`、`cluster-b` 拉两份 cross-cluster scheduler summary
- 只有 quorum vote 过线后，才会把 local inspect、remote summary gather、manifest stage、archive stage 和 publish 收进同一个 cross-cluster scheduler scope
- 最后异步发布 `runtime/consensus-snapshot-cross-cluster-scheduler-published.json`

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
} DemoClusterNode;

typedef struct
{
	uint32 iHomeClusterQueueTimeoutMs;
	uint32 iFailoverClusterQueueTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
	uint32 iLocalQuotaLimit;
	uint32 iRemoteQuotaLimit;
	uint32 iSharedQuotaLimit;
	uint32 iClusterQuorumRequired;
	uint32 iLeaseTTLms;
	uint32 iSharedClusterRetryBudget;
	uint32 iFailoverClusterRetryBudget;
	uint32 iGlobalSweepIntervalMs;
	uint32 iRemoteSweepIntervalMs;
} DemoSnapshotCrossClusterSchedulerPolicy;

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
	DemoSnapshotCrossClusterSchedulerPolicy tPolicy;
	uint32 iActiveHomeClusterSlot;
	uint32 iActiveFailoverClusterSlot;
	uint32 iSharedClusterQuotaInUse;
	uint32 iGrantedClusterQuota;
	uint32 iGrantClusterVoteCount;
	uint32 iRejectClusterVoteCount;
	uint32 iLeaseEpoch;
	uint32 iHomeClusterQueue;
	uint32 iFailoverClusterQueue;
	uint32 iDeferQueue;
	uint32 iSharedClusterRetryBudget;
	uint32 iFailoverClusterRetryBudget;
	uint32 iNextSharedWakeAt;
	uint32 iNextFailoverWakeAt;
	uint32 iClusterEpoch;
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
	char sInstalledCluster[32];
	char sSchedulerState[48];
	char sSelectedCluster[16];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotCrossClusterSchedulerCenter;

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

static bool procSaveConsensusSnapshotCrossClusterSchedulerCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sCommittedOwner,
	const char* sInstalledCluster,
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
	uint32 iActiveHomeClusterSlot,
	uint32 iActiveFailoverClusterSlot,
	uint32 iSharedClusterQuotaInUse,
	uint32 iGrantedClusterQuota,
	uint32 iGrantClusterVoteCount,
	uint32 iRejectClusterVoteCount,
	uint32 iLeaseEpoch,
	uint32 iHomeClusterQueue,
	uint32 iFailoverClusterQueue,
	uint32 iDeferQueue,
	uint32 iSharedClusterRetryBudget,
	uint32 iFailoverClusterRetryBudget,
	uint32 iLocalQuotaLimit,
	uint32 iRemoteQuotaLimit,
	uint32 iSharedQuotaLimit,
	uint32 iClusterQuorumRequired,
	uint32 iNextSharedWakeAt,
	uint32 iNextFailoverWakeAt,
	uint32 iClusterEpoch,
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
	xvoTableSetText(vRoot, "installed_cluster", 0, (uint8*)((sInstalledCluster != NULL) ? sInstalledCluster : ""), 0, FALSE);
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
	xvoTableSetInt(vRoot, "active_home_cluster_slot", 0, (int32)iActiveHomeClusterSlot);
	xvoTableSetInt(vRoot, "active_failover_cluster_slot", 0, (int32)iActiveFailoverClusterSlot);
	xvoTableSetInt(vRoot, "shared_cluster_quota_in_use", 0, (int32)iSharedClusterQuotaInUse);
	xvoTableSetInt(vRoot, "granted_cluster_quota", 0, (int32)iGrantedClusterQuota);
	xvoTableSetInt(vRoot, "grant_cluster_vote_count", 0, (int32)iGrantClusterVoteCount);
	xvoTableSetInt(vRoot, "reject_cluster_vote_count", 0, (int32)iRejectClusterVoteCount);
	xvoTableSetInt(vRoot, "lease_epoch", 0, (int32)iLeaseEpoch);
	xvoTableSetInt(vRoot, "home_cluster_queue", 0, (int32)iHomeClusterQueue);
	xvoTableSetInt(vRoot, "failover_cluster_queue", 0, (int32)iFailoverClusterQueue);
	xvoTableSetInt(vRoot, "defer_queue", 0, (int32)iDeferQueue);
	xvoTableSetInt(vRoot, "shared_cluster_retry_budget", 0, (int32)iSharedClusterRetryBudget);
	xvoTableSetInt(vRoot, "failover_cluster_retry_budget", 0, (int32)iFailoverClusterRetryBudget);
	xvoTableSetInt(vRoot, "local_quota_limit", 0, (int32)iLocalQuotaLimit);
	xvoTableSetInt(vRoot, "remote_quota_limit", 0, (int32)iRemoteQuotaLimit);
	xvoTableSetInt(vRoot, "shared_quota_limit", 0, (int32)iSharedQuotaLimit);
	xvoTableSetInt(vRoot, "cluster_quorum_required", 0, (int32)iClusterQuorumRequired);
	xvoTableSetInt(vRoot, "next_shared_wake_at", 0, (int32)iNextSharedWakeAt);
	xvoTableSetInt(vRoot, "next_failover_wake_at", 0, (int32)iNextFailoverWakeAt);
	xvoTableSetInt(vRoot, "cluster_epoch", 0, (int32)iClusterEpoch);
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

static xvalue procLoadConsensusSnapshotCrossClusterSchedulerCheckpoint(const char* sCheckpointPath)
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
	const char* sInstalledCluster,
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
	xvoTableSetText(vRoot, "installed_cluster", 0, (uint8*)((sInstalledCluster != NULL) ? sInstalledCluster : ""), 0, FALSE);
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

static bool procWriteRemoteSnapshotCrossClusterSchedulerSummaryStage(
	const char* sPath,
	const char* sKey,
	uint32 iRound,
	uint32 iSnapshotIndex,
	uint32 iRemoteSegmentSeq,
	uint32 iRemoteArchiveCursor,
	uint32 iTargetSegmentSeq,
	uint32 iPublishIndex,
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_distributed_scheduler_summary_ready", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "remote_snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "remote_segment_seq", 0, (int32)iRemoteSegmentSeq);
	xvoTableSetInt(vRoot, "remote_archive_cursor", 0, (int32)iRemoteArchiveCursor);
	xvoTableSetInt(vRoot, "target_segment_seq", 0, (int32)iTargetSegmentSeq);
	xvoTableSetInt(vRoot, "remote_publish_index", 0, (int32)iPublishIndex);
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

static bool procPrepareSnapshotCrossClusterSchedulerRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoClusterNode* pPeer,
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-cross-cluster-scheduler/%s?round=%u", sKey, (unsigned)iRound);
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

static int procSelectClusters(
	const DemoClusterNode arrCluster[],
	int iClusterCount,
	uint32 iMaxLatencyMs,
	const DemoClusterNode* arrOut[],
	int iOutCap)
{
	int iSelected = 0;
	int i;

	if ( (arrCluster == NULL) || (arrOut == NULL) || (iOutCap <= 0) ) {
		return 0;
	}

	for ( i = 0; i < iClusterCount; i++ ) {
		if ( !arrCluster[i].bHealthy ) {
			continue;
		}
		if ( arrCluster[i].iLatencyMs > iMaxLatencyMs ) {
			continue;
		}
		if ( iSelected >= iOutCap ) {
			break;
		}

		arrOut[iSelected++] = &arrCluster[i];
	}

	return iSelected;
}

static const char* procChooseSnapshotCrossClusterSchedulerPlan(const DemoSnapshotCrossClusterSchedulerCenter* pCenter, int iClusterCount)
{
	uint32 iNow = (uint32)xrtNow();

	if ( (pCenter == NULL) || (iClusterCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iGrantedClusterQuota == 0u ) {
		if ( pCenter->iSharedClusterRetryBudget == 0u ) {
			return "defer_shared_cluster_budget";
		}

		if ( pCenter->iNextSharedWakeAt > iNow ) {
			return "sleep_shared_wake";
		}

		if ( pCenter->iGrantClusterVoteCount < pCenter->tPolicy.iClusterQuorumRequired ) {
			return "run_cluster_vote_now";
		}

		return "wait_cluster_quorum";
	}

	if ( pCenter->iHomeClusterQueue > 0u ) {
		if ( pCenter->iSharedClusterQuotaInUse >= pCenter->iGrantedClusterQuota ) {
			return "queue_granted_cluster_budget";
		}

		if ( pCenter->iActiveHomeClusterSlot >= pCenter->tPolicy.iLocalQuotaLimit ) {
			return "queue_home_cluster_domain";
		}

		if ( (pCenter->iSharedClusterRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_shared_cluster_budget";
		}

		if ( pCenter->iNextSharedWakeAt > iNow ) {
			return "sleep_shared_wake";
		}

		return "run_home_cluster_now";
	}

	if ( pCenter->iFailoverClusterQueue > 0u ) {
		if ( pCenter->iSharedClusterQuotaInUse >= pCenter->iGrantedClusterQuota ) {
			return "queue_granted_cluster_budget";
		}

		if ( pCenter->iActiveFailoverClusterSlot >= pCenter->tPolicy.iRemoteQuotaLimit ) {
			return "queue_failover_cluster_domain";
		}

		if ( (pCenter->iFailoverClusterRetryBudget == 0u) && (pCenter->iArchiveCursor < pCenter->iTargetSegmentSeq) ) {
			return "defer_failover_cluster_budget";
		}

		if ( pCenter->iNextFailoverWakeAt > iNow ) {
			return "sleep_failover_cluster_wake";
		}

		return "run_failover_cluster_now";
	}

	if ( (pCenter->iManifestCursor < pCenter->iTargetSegmentSeq) || (pCenter->iArchiveCursor < pCenter->iManifestCursor) ) {
		if ( pCenter->iSharedClusterQuotaInUse >= pCenter->iGrantedClusterQuota ) {
			return "queue_granted_cluster_budget";
		}

		return "run_failover_cluster_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotCrossClusterScheduler(
	DemoSnapshotCrossClusterSchedulerCenter* pCenter,
	const DemoClusterNode* arrCluster[],
	int iClusterCount,
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

	if ( (pCenter == NULL) || (arrCluster == NULL) || (iClusterCount <= 0) || (sKey == NULL) ) {
		return false;
	}

	bLocalDomain = (pCenter->iGrantedClusterQuota > 0u) ? (pCenter->iHomeClusterQueue > 0u) : true;
	procCopyText(pCenter->sSelectedCluster, sizeof(pCenter->sSelectedCluster), bLocalDomain ? "local" : "remote");
	pCenter->iSharedClusterQuotaInUse++;

	if ( bLocalDomain ) {
		pCenter->iActiveHomeClusterSlot++;
		if ( pCenter->iHomeClusterQueue > 0u ) {
			pCenter->iHomeClusterQueue--;
		}
	} else {
		pCenter->iActiveFailoverClusterSlot++;
		if ( pCenter->iFailoverClusterQueue > 0u ) {
			pCenter->iFailoverClusterQueue--;
		}
	}

	(void)procSaveConsensusSnapshotCrossClusterSchedulerCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_cross_cluster_scheduler_opened",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledCluster,
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
		pCenter->iActiveHomeClusterSlot,
		pCenter->iActiveFailoverClusterSlot,
		pCenter->iSharedClusterQuotaInUse,
		pCenter->iGrantedClusterQuota,
		pCenter->iGrantClusterVoteCount,
		pCenter->iRejectClusterVoteCount,
		pCenter->iLeaseEpoch,
		pCenter->iHomeClusterQueue,
		pCenter->iFailoverClusterQueue,
		pCenter->iDeferQueue,
		pCenter->iSharedClusterRetryBudget,
		pCenter->iFailoverClusterRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedQuotaLimit,
		pCenter->tPolicy.iClusterQuorumRequired,
		pCenter->iNextSharedWakeAt,
		pCenter->iNextFailoverWakeAt,
		pCenter->iClusterEpoch,
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

	for ( i = 0; i < iClusterCount; i++ ) {
		if ( !procPrepareSnapshotCrossClusterSchedulerRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrCluster[i],
			sKey,
			pCenter->iCommittedRound,
			bLocalDomain ? pCenter->tPolicy.iHomeClusterQueueTimeoutMs : pCenter->tPolicy.iFailoverClusterQueueTimeoutMs) ) {
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

	for ( i = 0; i < iClusterCount; i++ ) {
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
					procCopyText(pCenter->sInstalledCluster, sizeof(pCenter->sInstalledCluster), arrCluster[i]->sName);
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
					pCenter->iGrantClusterVoteCount++;
				} else {
					pCenter->iRejectClusterVoteCount++;
				}
			}
			xvoUnref(vRemote);
			vRemote = NULL;
		}
	}

	if ( pCenter->iGrantClusterVoteCount >= pCenter->tPolicy.iClusterQuorumRequired ) {
		pCenter->iGrantedClusterQuota = pCenter->tPolicy.iSharedQuotaLimit;
		pCenter->iLeaseEpoch++;
		pCenter->iClusterEpoch++;
		bArbitrated = true;
	} else {
		pCenter->iGrantedClusterQuota = 0u;
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
		pCenter->sInstalledCluster,
		pCenter->iSnapshotIndex);
	(void)procWriteSnapshotArchiveStage(
		pCenter->iCommittedRound,
		pCenter->iArchiveCursor,
		sKey,
		pCenter->iSnapshotIndex);
	(void)procWriteRemoteSnapshotCrossClusterSchedulerSummaryStage(
		pCenter->sRemoteSummaryPath,
		sKey,
		pCenter->iCommittedRound,
		pCenter->iSnapshotIndex,
		pCenter->iLocalSegmentSeq,
		pCenter->iArchiveCursor,
		pCenter->iTargetSegmentSeq,
		pCenter->iPublishIndex,
		(uint32)iClusterCount);

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

	if ( !bArbitrated && (pCenter->iGrantedClusterQuota == 0u) ) {
		pCenter->iDeferQueue++;
		pCenter->iSharedClusterQuotaInUse = 0u;
		pCenter->iNextSharedWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iGlobalSweepIntervalMs;
		procCopyText(pCenter->sSchedulerState, sizeof(pCenter->sSchedulerState), "snapshot_cross_cluster_scheduler_wait_cluster_quorum");
	} else if ( pCenter->iArchiveCursor >= pCenter->iTargetSegmentSeq ) {
		pCenter->iSharedClusterRetryBudget = pCenter->tPolicy.iSharedClusterRetryBudget;
		pCenter->iFailoverClusterRetryBudget = pCenter->tPolicy.iFailoverClusterRetryBudget;
		pCenter->iNextSharedWakeAt = 0u;
		pCenter->iNextFailoverWakeAt = 0u;
		procCopyText(pCenter->sSchedulerState, sizeof(pCenter->sSchedulerState), "snapshot_cross_cluster_scheduler_finished");
	} else {
		if ( bLocalDomain ) {
			if ( pCenter->iSharedClusterRetryBudget > 0u ) {
				pCenter->iSharedClusterRetryBudget--;
			}
			pCenter->iFailoverClusterQueue++;
			pCenter->iNextSharedWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iGlobalSweepIntervalMs;
			pCenter->iNextFailoverWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteSweepIntervalMs;
		} else {
			if ( pCenter->iFailoverClusterRetryBudget > 0u ) {
				pCenter->iFailoverClusterRetryBudget--;
			}
			pCenter->iFailoverClusterQueue++;
			pCenter->iNextFailoverWakeAt = (uint32)xrtNow() + pCenter->tPolicy.iRemoteSweepIntervalMs;
		}
		pCenter->iDeferQueue++;
		procCopyText(pCenter->sSchedulerState, sizeof(pCenter->sSchedulerState), "snapshot_cross_cluster_scheduler_running");
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"selected_cluster\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_cluster\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"manifest_cursor\": %u,\n\t\"archive_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u,\n\t\"active_home_cluster_slot\": %u,\n\t\"active_failover_cluster_slot\": %u,\n\t\"shared_cluster_quota_in_use\": %u,\n\t\"granted_cluster_quota\": %u,\n\t\"grant_cluster_vote_count\": %u,\n\t\"reject_cluster_vote_count\": %u,\n\t\"lease_epoch\": %u,\n\t\"home_cluster_queue\": %u,\n\t\"failover_cluster_queue\": %u,\n\t\"defer_queue\": %u,\n\t\"shared_cluster_retry_budget\": %u,\n\t\"failover_cluster_retry_budget\": %u,\n\t\"local_quota_limit\": %u,\n\t\"remote_quota_limit\": %u,\n\t\"shared_quota_limit\": %u,\n\t\"cluster_quorum_required\": %u,\n\t\"next_shared_wake_at\": %u,\n\t\"next_failover_wake_at\": %u,\n\t\"cluster_epoch\": %u,\n\t\"scheduler_state\": \"%s\"\n}\n",
		pCenter->sSchedulerState,
		pCenter->sSelectedCluster,
		pCenter->sCommittedOwner,
		pCenter->sInstalledCluster,
		(unsigned)pCenter->iCommittedRound,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iLocalSegmentSeq,
		(unsigned)pCenter->iManifestCursor,
		(unsigned)pCenter->iArchiveCursor,
		(unsigned)pCenter->iTargetSegmentSeq,
		(unsigned)pCenter->iPublishIndex,
		(unsigned)pCenter->iCheckpointVersion,
		(unsigned)pCenter->iActiveHomeClusterSlot,
		(unsigned)pCenter->iActiveFailoverClusterSlot,
		(unsigned)pCenter->iSharedClusterQuotaInUse,
		(unsigned)pCenter->iGrantedClusterQuota,
		(unsigned)pCenter->iGrantClusterVoteCount,
		(unsigned)pCenter->iRejectClusterVoteCount,
		(unsigned)pCenter->iLeaseEpoch,
		(unsigned)pCenter->iHomeClusterQueue,
		(unsigned)pCenter->iFailoverClusterQueue,
		(unsigned)pCenter->iDeferQueue,
		(unsigned)pCenter->iSharedClusterRetryBudget,
		(unsigned)pCenter->iFailoverClusterRetryBudget,
		(unsigned)pCenter->tPolicy.iLocalQuotaLimit,
		(unsigned)pCenter->tPolicy.iRemoteQuotaLimit,
		(unsigned)pCenter->tPolicy.iSharedQuotaLimit,
		(unsigned)pCenter->tPolicy.iClusterQuorumRequired,
		(unsigned)pCenter->iNextSharedWakeAt,
		(unsigned)pCenter->iNextFailoverWakeAt,
		(unsigned)pCenter->iClusterEpoch,
		pCenter->sSchedulerState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotCrossClusterSchedulerCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sSchedulerState : "consensus_snapshot_cross_cluster_scheduler_incomplete",
		sKey,
		pCenter->sCommittedOwner,
		pCenter->sInstalledCluster,
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
		pCenter->iActiveHomeClusterSlot,
		pCenter->iActiveFailoverClusterSlot,
		pCenter->iSharedClusterQuotaInUse,
		pCenter->iGrantedClusterQuota,
		pCenter->iGrantClusterVoteCount,
		pCenter->iRejectClusterVoteCount,
		pCenter->iLeaseEpoch,
		pCenter->iHomeClusterQueue,
		pCenter->iFailoverClusterQueue,
		pCenter->iDeferQueue,
		pCenter->iSharedClusterRetryBudget,
		pCenter->iFailoverClusterRetryBudget,
		pCenter->tPolicy.iLocalQuotaLimit,
		pCenter->tPolicy.iRemoteQuotaLimit,
		pCenter->tPolicy.iSharedQuotaLimit,
		pCenter->tPolicy.iClusterQuorumRequired,
		pCenter->iNextSharedWakeAt,
		pCenter->iNextFailoverWakeAt,
		pCenter->iClusterEpoch,
		bFinished ? 1u : 0u);

	if ( pCenter->iSharedClusterQuotaInUse > 0u ) {
		pCenter->iSharedClusterQuotaInUse--;
	}

	if ( bLocalDomain ) {
		if ( pCenter->iActiveHomeClusterSlot > 0u ) {
			pCenter->iActiveHomeClusterSlot--;
		}
	} else {
		if ( pCenter->iActiveFailoverClusterSlot > 0u ) {
			pCenter->iActiveFailoverClusterSlot--;
		}
	}
	if ( pPublish != NULL ) {
		xFutureRelease(pPublish);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iClusterCount; i++ ) {
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
	DemoSnapshotCrossClusterSchedulerCenter tCenter;
	DemoClusterNode arrCluster[DEMO_MAX_PEER];
	const DemoClusterNode* arrSelected[DEMO_MAX_PEER];
	xvalue vLoaded = NULL;
	str sOwner = NULL;
	str sInstalledCluster = NULL;
	str sState = NULL;
	const char* sPlan = NULL;
	int iClusterCount = 0;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrCluster, 0, sizeof(arrCluster));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iHomeClusterQueueTimeoutMs = 3000u;
	tCenter.tPolicy.iFailoverClusterQueueTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.tPolicy.iLocalQuotaLimit = 1u;
	tCenter.tPolicy.iRemoteQuotaLimit = 1u;
	tCenter.tPolicy.iSharedQuotaLimit = 2u;
	tCenter.tPolicy.iClusterQuorumRequired = 2u;
	tCenter.tPolicy.iLeaseTTLms = 60u;
	tCenter.tPolicy.iSharedClusterRetryBudget = 3u;
	tCenter.tPolicy.iFailoverClusterRetryBudget = 2u;
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
	tCenter.iClusterEpoch = 4u;
	tCenter.iLeaseEpoch = 12u;
	tCenter.iHomeClusterQueue = 1u;
	tCenter.iFailoverClusterQueue = 2u;
	tCenter.iGrantedClusterQuota = 0u;
	tCenter.iGrantClusterVoteCount = 0u;
	tCenter.iRejectClusterVoteCount = 0u;
	tCenter.iSharedClusterRetryBudget = tCenter.tPolicy.iSharedClusterRetryBudget;
	tCenter.iFailoverClusterRetryBudget = tCenter.tPolicy.iFailoverClusterRetryBudget;
	tCenter.iNextSharedWakeAt = (uint32)xrtNow() - 1u;
	tCenter.iNextFailoverWakeAt = (uint32)xrtNow() + 30u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledCluster, sizeof(tCenter.sInstalledCluster), "cluster-b");
	procCopyText(tCenter.sSchedulerState, sizeof(tCenter.sSchedulerState), "boot");
	procCopyText(tCenter.sSelectedCluster, sizeof(tCenter.sSelectedCluster), "pending");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-cross-cluster-scheduler-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-cross-cluster-scheduler-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-cross-cluster-scheduler-summary.json");

	(void)procSaveConsensusSnapshotCrossClusterSchedulerCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"console-service",
		tCenter.sCommittedOwner,
		tCenter.sInstalledCluster,
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
		tCenter.iActiveHomeClusterSlot,
		tCenter.iActiveFailoverClusterSlot,
		tCenter.iSharedClusterQuotaInUse,
		tCenter.iGrantedClusterQuota,
		tCenter.iGrantClusterVoteCount,
		tCenter.iRejectClusterVoteCount,
		tCenter.iLeaseEpoch,
		tCenter.iHomeClusterQueue,
		tCenter.iFailoverClusterQueue,
		tCenter.iDeferQueue,
		tCenter.iSharedClusterRetryBudget,
		tCenter.iFailoverClusterRetryBudget,
		tCenter.tPolicy.iLocalQuotaLimit,
		tCenter.tPolicy.iRemoteQuotaLimit,
		tCenter.tPolicy.iSharedQuotaLimit,
		tCenter.tPolicy.iClusterQuorumRequired,
		tCenter.iNextSharedWakeAt,
		tCenter.iNextFailoverWakeAt,
		tCenter.iClusterEpoch,
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotCrossClusterSchedulerCheckpoint(tCenter.sCheckpointPath);
	if ( vLoaded == NULL ) {
		fprintf(stderr, "load checkpoint failed\n");
		return 1;
	}

	sOwner = xvoTableGetText(vLoaded, "committed_owner", 0);
	sInstalledCluster = xvoTableGetText(vLoaded, "installed_cluster", 0);
	sState = xvoTableGetText(vLoaded, "state", 0);
	procCopyText(tCenter.sSelectedCluster, sizeof(tCenter.sSelectedCluster), (const char*)xvoTableGetText(vLoaded, "selected_cluster", 0));
	tCenter.iSnapshotIndex = (uint32)xvoTableGetInt(vLoaded, "snapshot_index", 0);
	tCenter.iLocalSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "local_segment_seq", 0);
	tCenter.iManifestCursor = (uint32)xvoTableGetInt(vLoaded, "manifest_cursor", 0);
	tCenter.iArchiveCursor = (uint32)xvoTableGetInt(vLoaded, "archive_cursor", 0);
	tCenter.iTargetSegmentSeq = (uint32)xvoTableGetInt(vLoaded, "target_segment_seq", 0);
	tCenter.iPublishIndex = (uint32)xvoTableGetInt(vLoaded, "publish_index", 0);
	tCenter.iCheckpointVersion = (uint32)xvoTableGetInt(vLoaded, "checkpoint_version", 0);
	tCenter.iActiveHomeClusterSlot = (uint32)xvoTableGetInt(vLoaded, "active_home_cluster_slot", 0);
	tCenter.iActiveFailoverClusterSlot = (uint32)xvoTableGetInt(vLoaded, "active_failover_cluster_slot", 0);
	tCenter.iSharedClusterQuotaInUse = (uint32)xvoTableGetInt(vLoaded, "shared_cluster_quota_in_use", 0);
	tCenter.iGrantedClusterQuota = (uint32)xvoTableGetInt(vLoaded, "granted_cluster_quota", 0);
	tCenter.iGrantClusterVoteCount = (uint32)xvoTableGetInt(vLoaded, "grant_cluster_vote_count", 0);
	tCenter.iRejectClusterVoteCount = (uint32)xvoTableGetInt(vLoaded, "reject_cluster_vote_count", 0);
	tCenter.iLeaseEpoch = (uint32)xvoTableGetInt(vLoaded, "lease_epoch", 0);
	tCenter.iHomeClusterQueue = (uint32)xvoTableGetInt(vLoaded, "home_cluster_queue", 0);
	tCenter.iFailoverClusterQueue = (uint32)xvoTableGetInt(vLoaded, "failover_cluster_queue", 0);
	tCenter.iDeferQueue = (uint32)xvoTableGetInt(vLoaded, "defer_queue", 0);
	tCenter.iSharedClusterRetryBudget = (uint32)xvoTableGetInt(vLoaded, "shared_cluster_retry_budget", 0);
	tCenter.iFailoverClusterRetryBudget = (uint32)xvoTableGetInt(vLoaded, "failover_cluster_retry_budget", 0);
	tCenter.tPolicy.iLocalQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "local_quota_limit", (int32)tCenter.tPolicy.iLocalQuotaLimit);
	tCenter.tPolicy.iRemoteQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "remote_quota_limit", (int32)tCenter.tPolicy.iRemoteQuotaLimit);
	tCenter.tPolicy.iSharedQuotaLimit = (uint32)xvoTableGetInt(vLoaded, "shared_quota_limit", (int32)tCenter.tPolicy.iSharedQuotaLimit);
	tCenter.tPolicy.iClusterQuorumRequired = (uint32)xvoTableGetInt(vLoaded, "cluster_quorum_required", (int32)tCenter.tPolicy.iClusterQuorumRequired);
	tCenter.iNextSharedWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_shared_wake_at", 0);
	tCenter.iNextFailoverWakeAt = (uint32)xvoTableGetInt(vLoaded, "next_failover_wake_at", 0);
	tCenter.iClusterEpoch = (uint32)xvoTableGetInt(vLoaded, "cluster_epoch", 0);
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledCluster, sizeof(tCenter.sInstalledCluster), (const char*)sInstalledCluster);
	procCopyText(tCenter.sSchedulerState, sizeof(tCenter.sSchedulerState), (const char*)sState);
	xvoUnref(vLoaded);

	procCopyText(arrCluster[0].sName, sizeof(arrCluster[0].sName), "cluster-a");
	procCopyText(arrCluster[0].sBaseURL, sizeof(arrCluster[0].sBaseURL), "http://127.0.0.1:9101");
	arrCluster[0].iLatencyMs = 25u;
	arrCluster[0].iPriority = 10;
	arrCluster[0].bHealthy = true;

	procCopyText(arrCluster[1].sName, sizeof(arrCluster[1].sName), "cluster-b");
	procCopyText(arrCluster[1].sBaseURL, sizeof(arrCluster[1].sBaseURL), "http://127.0.0.1:9102");
	arrCluster[1].iLatencyMs = 32u;
	arrCluster[1].iPriority = 9;
	arrCluster[1].bHealthy = true;

	procCopyText(arrCluster[2].sName, sizeof(arrCluster[2].sName), "cluster-c");
	procCopyText(arrCluster[2].sBaseURL, sizeof(arrCluster[2].sBaseURL), "http://127.0.0.1:9103");
	arrCluster[2].iLatencyMs = 120u;
	arrCluster[2].iPriority = 5;
	arrCluster[2].bHealthy = false;

	iClusterCount = procSelectClusters(
		arrCluster,
		DEMO_MAX_PEER,
		tCenter.tPolicy.iMaxPeerLatencyMs,
		arrSelected,
		DEMO_MAX_PEER);

	sPlan = procChooseSnapshotCrossClusterSchedulerPlan(&tCenter, iClusterCount);

	printf("snapshot cross-cluster scheduler plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_cluster=%s active_home_cluster=%u active_failover_cluster=%u shared_cluster_in_use=%u granted_cluster_quota=%u grant_votes=%u reject_votes=%u lease_epoch=%u home_cluster_queue=%u failover_cluster_queue=%u defer_queue=%u shared_cluster_retry=%u failover_cluster_retry=%u next_shared_wake=%u next_failover_wake=%u cluster_epoch=%u installed_cluster=%s\n",
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
		(unsigned)tCenter.iActiveHomeClusterSlot,
		(unsigned)tCenter.iActiveFailoverClusterSlot,
		(unsigned)tCenter.iSharedClusterQuotaInUse,
		(unsigned)tCenter.iGrantedClusterQuota,
		(unsigned)tCenter.iGrantClusterVoteCount,
		(unsigned)tCenter.iRejectClusterVoteCount,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iHomeClusterQueue,
		(unsigned)tCenter.iFailoverClusterQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iSharedClusterRetryBudget,
		(unsigned)tCenter.iFailoverClusterRetryBudget,
		(unsigned)tCenter.iNextSharedWakeAt,
		(unsigned)tCenter.iNextFailoverWakeAt,
		(unsigned)tCenter.iClusterEpoch,
		tCenter.sInstalledCluster);

	if ( (strcmp(sPlan, "run_cluster_vote_now") == 0) || (strcmp(sPlan, "run_home_cluster_now") == 0) || (strcmp(sPlan, "run_failover_cluster_now") == 0) ) {
		if ( !procRunConsensusSnapshotCrossClusterScheduler(&tCenter, arrSelected, iClusterCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot cross-cluster scheduler failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot cross-cluster scheduler: snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u selected_cluster=%s active_home_cluster=%u active_failover_cluster=%u shared_cluster_in_use=%u granted_cluster_quota=%u grant_votes=%u reject_votes=%u lease_epoch=%u home_cluster_queue=%u failover_cluster_queue=%u defer_queue=%u shared_cluster_retry=%u failover_cluster_retry=%u next_shared_wake=%u next_failover_wake=%u cluster_epoch=%u state=%s installed_cluster=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sSelectedCluster,
		(unsigned)tCenter.iActiveHomeClusterSlot,
		(unsigned)tCenter.iActiveFailoverClusterSlot,
		(unsigned)tCenter.iSharedClusterQuotaInUse,
		(unsigned)tCenter.iGrantedClusterQuota,
		(unsigned)tCenter.iGrantClusterVoteCount,
		(unsigned)tCenter.iRejectClusterVoteCount,
		(unsigned)tCenter.iLeaseEpoch,
		(unsigned)tCenter.iHomeClusterQueue,
		(unsigned)tCenter.iFailoverClusterQueue,
		(unsigned)tCenter.iDeferQueue,
		(unsigned)tCenter.iSharedClusterRetryBudget,
		(unsigned)tCenter.iFailoverClusterRetryBudget,
		(unsigned)tCenter.iNextSharedWakeAt,
		(unsigned)tCenter.iNextFailoverWakeAt,
		(unsigned)tCenter.iClusterEpoch,
		tCenter.sSchedulerState,
		tCenter.sInstalledCluster);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot scheduler finished` 不等于 `snapshot cross-cluster scheduler finished`

这页最容易偷懒的写法是：

- 看到 `archive_cursor >= target_segment_seq` 就认为 cross-cluster scheduler 已完成
- 看到 publish 文件已经写出，就认为 home / failover cluster slot 和 shared quota 一定都已经释放
- 看到某一轮刚失败过，就直接把整个 key 标成永久 defer

但更稳的模型应该是：

- `snapshot persistence finished`
	- 回答 manifest、archive、checkpoint version 和 publish 是否都已经正式 durable
- `snapshot multi-queue finished`
	- 回答本机 home / failover cluster admission、双 wake 和本机 queue 长度是不是已经正式收口
- `snapshot cross-cluster scheduler finished`
	- 回答当前 `granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / cluster_epoch`、shared quota、domain quota 和双预算是不是也已经正式收口

### 6.2 `active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / home_cluster_queue / failover_cluster_queue` 不是一个字段

这页也很容易写混：

- 把 `active_home_cluster_slot = 0` 直接等同于“现在完全没有本机接管压力了”
- 或者把 `shared_cluster_quota_in_use = 1` 直接当成“failover cluster 一定还能再拿 slot”
- 或者把 `failover_cluster_queue > 0` 直接当成“failover cluster 一定马上能跑”
- 或者把 `shared_cluster_retry_budget > 0` 直接当成“failover cluster budget 也一定还够”
- 或者把 `next_shared_wake_at` 和 `next_failover_wake_at` 直接当成同一个冷却时间

但更稳的边界应该是：

- `active_home_cluster_slot`
	- 回答当前 home cluster 还有几个 admitted scope 正在真正占用资源
- `active_failover_cluster_slot`
	- 回答当前 failover cluster 还有几个 admitted scope 正在真正占用资源
- `shared_cluster_quota_in_use`
	- 回答整个 cross-cluster admission domain 当前已经正式占了多少份共享 quota
- `home_cluster_queue / failover_cluster_queue`
	- 回答不同 domain 里到底还有多少 key 等待 admission
- `shared_cluster_retry_budget / failover_cluster_retry_budget`
	- 回答 shared cluster domain 和 failover cluster 当前还允许失败后再被拉起几次
- `next_shared_wake_at / next_failover_wake_at`
	- 回答 shared cluster domain 和 failover cluster 最早什么时候可以合法再看一眼，而不是被彼此的冷却时间拖住
- `granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count`
	- 回答这轮 shared quota 到底有没有真的过仲裁、当前已经累计几票赞成和反对
- `lease_epoch / cluster_epoch`
	- 回答当前 checkpoint 看到的是哪一轮有效租约、哪一轮 cluster grant 视图，而不是单纯的计数器

### 6.3 remote summary 可以提高优先级，但不能直接绕过 admission

这页还很容易偷懒成：

- 某个 failover cluster 返回 `target_segment_seq = 6`
- 本地就直接把 plan 改成 `run_home_cluster_now`
- 即使 shared quota 已满、failover cluster 还排着，也照样强行抢 slot

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`、cluster 选择、`grant_cluster_vote_count / reject_cluster_vote_count`、`cluster_epoch` 和当前 cluster domain 判断
- 真正的 `run_home_cluster_now / run_failover_cluster_now` 还得等 quorum vote、`granted_cluster_quota`、domain quota、queue 长度和 wake 时间一起通过
- 真正的 slot 释放还得等 admitted scope 正式 close / join / publish 结束
- 真正的下一轮重试还得等 shared budget 或 failover cluster budget 和 wake 时间一起满足

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输、校验和断点续拉协议，应继续补更重的 snapshot transport 主线
- 如果需要把 scheduler 扩展到多类 key、多级 quota domain 和更细的 cluster 授权，应继续补更重的 snapshot cross-cluster scheduler 主线
- 如果需要把 scheduler 主线扩展到跨集群 quota 汇总、全局 lease 协调和统一预算协调，应继续补更重的 lease coordination 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / active_home_cluster_slot / active_failover_cluster_slot / shared_cluster_quota_in_use / granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / home_cluster_queue / failover_cluster_queue / defer_queue / shared_cluster_retry_budget / failover_cluster_retry_budget / next_shared_wake_at / next_failover_wake_at / cluster_epoch / scheduler_state`
2. `POST /api/consensus-snapshot-cross-cluster-scheduler`
	- 只提交 cross-cluster scheduler admission 意图，让 worker 决定是否进入 `run_cluster_vote_now / wait_cluster_quorum / queue_granted_cluster_budget / queue_home_cluster_domain / run_home_cluster_now / queue_failover_cluster_domain / run_failover_cluster_now / sleep_shared_wake / sleep_failover_cluster_wake / defer_shared_cluster_budget / defer_failover_cluster_budget`
3. `GET /api/consensus-snapshot-cross-cluster-scheduler`
	- 直接返回最近一次 snapshot cross-cluster scheduler checkpoint
4. `POST /api/sweep`
	- 手工或定时触发 shared quota 复核、quorum vote 刷新、lease 续期、remote wake 判断、failover cluster budget 刷新和 defer 迁移
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 manifest 进度、archive 进度、checkpoint version、home / failover cluster quota、shared quota in use、`granted_cluster_quota / grant_cluster_vote_count / reject_cluster_vote_count / lease_epoch / cluster_epoch`、双队列长度、双 wake 和 recent scheduler history

## 9. 下一步阅读

如果你准备继续把 snapshot cross-cluster scheduler 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个快照分布式配额仲裁面板](snapshot-distributed-quota-arbitration-dashboard.md)
	先对比“这轮共享 quota 到底有没有过票”和“过票以后 home cluster / failover cluster 怎样正式排班”到底差在哪里
2. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
	对比“cluster fan-out / summary gather / publish orchestration”和“cluster vote / lease epoch / granted cluster quota”到底差在哪里
3. [把本地控制台服务升级成一个快照租约协调面板](snapshot-lease-coordination-dashboard.md)
	继续看为什么“哪个 cluster 先排进恢复窗口”还不够，系统还必须把 `lease_owner_cluster / witness_cluster / renew_vote_count / granted_lease_window / lease_epoch` 收成新的协调状态








