# 把本地控制台服务升级成一个快照持久化面板

> 这页要解决的不是“快照续传已经把 `local_segment_seq` 追到一个比较新的值，所以把当前结果写个文件就行了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / persistence_state`，并且上一页已经能把 install、published summary 和 continuation checkpoint 收成一条正式主线之后，又开始需要回答“哪些 segment 已经真正进入 durable manifest”“archive cursor 什么时候才算正式推进”“为什么 `checkpoint_version` 不能只靠 publish 文件推断”“manifest、archive、checkpoint version 和 publish 为什么仍然要进同一个 persistence scope”时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在重启后靠“读一下最新 segment 再猜当前 durable baseline 到哪了”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识快照编排怎样稳定回答 `snapshot_index / segment_seq / truncated_to / publish_index`
- 快照接管怎样把 remote install、local segment 落盘和 published summary 收进同一个 takeover scope
- 快照续传编排怎样把多轮 install、publish 和 checkpoint 更新收成长期主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- 一轮 persistence 可能只把 durable manifest 推到 `manifest_cursor = 4`
- 当前 `target_segment_seq` 可能已经是 `7`
- 本地 `local_segment_seq` 也许已经跟上了 `4`
- 但 `archive_cursor` 仍然只到 `3`
- 如果此时进程退出，下次启动必须知道是继续补 manifest、继续补 archive，还是 checkpoint version 还没真正提升

如果这时还把实现停在：

- `if ( manifest_cursor < target ) persist next segment;`
- `if ( archive_cursor >= target ) publish success;`
- `if ( restart ) just read latest segment and guess persistence state;`

很快就会出现几个典型问题：

- checkpoint 说 persistence 已开始，但看不出这一轮到底卡在 manifest、archive 还是 checkpoint version 提升
- local segment 已经推进到更深值，archive 还停在旧值，却被误判成 durable baseline 已完整收口
- 某次 publish 文件写出成功后，新的 checkpoint version 没有正式提升，下一轮 persistence 仍然读到旧状态
- 下次启动时知道 target 很深，却不知道该先继续补 manifest 还是先补 archive gap

所以这页真正要补出的，不是“接着把剩下几段装完”，而是：

> 当 snapshot continuation 已经进入长期运行状态之后，怎样把 durable manifest、archive 推进、checkpoint version 提升和 published summary 更新统一进一条可恢复、可解释、可导出的正式快照持久化主线。

## 2. 为什么这次不能只靠“把当前结果写个 manifest 就算了”

### 2.1 快照接管只回答“这一轮 install / publish 怎样正式接上”

上一页的快照接管已经很好地解决了：

- 本地 snapshot state 和 local segment 怎样 inspect
- remote summary 怎样进入正式 future 主线
- `local_segment_seq / target_segment_seq / publish_index` 怎样在同一轮里推进

但它不回答：

- manifest 一轮只推进一步时，下次要从哪继续
- archive 如果落后于 manifest，一轮结束后应不应该立即判定完成
- `checkpoint_version` 什么时候才能安全提升
- 多轮 persistence 之间的 gap 怎样正式记录

### 2.2 `manifest_cursor`、`archive_cursor` 和 `checkpoint_version` 不是同一个层级

到了这一层，系统真正要稳定回答的是：

- 当前 `manifest_cursor` 已经推进到哪
- 当前 `archive_cursor` 还落后多少
- 当前 `local_segment_seq` 是不是只是“本地有数据”，还是“已经进入 durable manifest”
- 当前 publish 文件回答的是“这轮 persistence 还在跑”，还是“checkpoint version 也已经正式提升”

这说明：

- `snapshot takeover checkpoint`
	- 回答“一轮 takeover 当前推进到了哪”
- `snapshot persistence checkpoint`
	- 回答“manifest / archive / checkpoint version 当前到底卡在什么位置”

### 2.3 这次真正新增的是“durable manifest + archive + checkpoint version”状态层

更稳的分工方式是：

- `snapshot takeover`
	- 负责把 remote install、local segment 落盘和 published summary 先接进正式主线
- `snapshot continuation orchestration`
	- 负责把 install gap、publish gap 和 continuation checkpoint 串起来
- `snapshot persistence`
	- 负责 durable manifest、archive、checkpoint version 和 publish 的长期收口
- 更重的 snapshot scheduler / quota
	- 再往下负责更长生命周期的 admission、跨轮恢复和调度策略

一句话记住：

> 上一页补的是“接住之后 install 和 publish 怎样继续推进”，这一页补的是“推进之后 durable manifest、archive 和 checkpoint version 怎样正式收口”。

## 3. 这条快照持久化主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active persistence registry | 当前有哪些 key 正在做 snapshot persistence |
| `list` | recent persistence history | 页面和 JSON 展示最近 manifest / archive / publish 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 persistence |
| `queue + thread` | 后台消费 `PERSIST / SWEEP` | 请求线程不阻塞在 manifest / archive / publish / checkpoint version 上 |
| `task group` | 管住一次 persistence scope 里的 local inspect / remote fetch / manifest child / archive child / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、manifest stage、archive stage、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 snapshot persistence summary | 让远端 `target_segment_seq / remote_archive_cursor / remote_publish_index` 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、写 local segment、写 manifest stage、写 archive stage、读取本地 snapshot state | 让 persistence 进入正式文件主线 |
| `file_async` | 把 published persistence summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 checkpoint、remote summary、publish JSON | 让 `local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / checkpoint_version / persistence_state` 有正式结构 |
| `time` | 记录 persistence 启动、join、manifest、archive、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / checkpoint_version / persistence_state` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> 快照续传编排管“数据怎样继续追上”，快照持久化管“哪些结果已经正式 durable 并且可恢复地写进 checkpoint”。

## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 snapshot persistence 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-snapshot-persistence-checkpoint.json
runtime/consensus-snapshot-state/<round>.json
runtime/consensus-snapshot-segment/<round>-<seq>.json
runtime/consensus-snapshot-manifest/<round>-<seq>.json
runtime/consensus-snapshot-archive/<round>-<seq>.json
runtime/consensus-snapshot-persistence-published.json
runtime/remote-consensus-snapshot-persistence-summary.json
```

其中：

- `config/console.json`
	- 保存 `persistence_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / persistence_state`
- `runtime/console.log`
	- 记录 persistence 启动、remote summary merge、manifest 推进、archive 推进、checkpoint version 提升和 publish
- `runtime/consensus-snapshot-persistence-checkpoint.json`
	- 保存最近一次正式 snapshot persistence 状态
- `runtime/consensus-snapshot-state/<round>.json`
	- 保存当前 round 的本地 snapshot state
- `runtime/consensus-snapshot-segment/<round>-<seq>.json`
	- 保存本地已经正式落盘的 snapshot segment
- `runtime/consensus-snapshot-manifest/<round>-<seq>.json`
	- 保存这轮 manifest 推进的中间文件
- `runtime/consensus-snapshot-archive/<round>-<seq>.json`
	- 保存这轮 archive 推进的中间文件
- `runtime/consensus-snapshot-persistence-published.json`
	- 保存异步发布后的 persistence 输出
- `runtime/remote-consensus-snapshot-persistence-summary.json`
	- 保存这次 remote persistence summary 的本地 stage 文件

## 5. 先把“启动装载 persistence checkpoint -> inspect local state -> gather remote summary -> 推进 manifest_cursor -> 推进 archive_cursor -> 提升 checkpoint_version -> publish”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先装载 `consensus-snapshot-persistence-checkpoint.json`
2. checkpoint 里明确记录 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / persistence_state`
3. `dict` 表示当前 active persistence registry
4. `list` 表示 persistence / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交 persistence 意图”
6. `task group` 先管住本次 persistence scope
7. local snapshot state inspect 和 local segment inspect 先各用一个 child 表达
8. 多个 peer persistence summary 走多条 `xhttp future`
9. 统一推进 `manifest_cursor / archive_cursor / local_segment_seq / publish_index / checkpoint_version`
10. manifest stage、archive stage 和 remote summary 先落本地文件，再进入 publish
11. 最后再用 `file_async future` 异步发布 committed persistence 结果

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`committed_owner = node-b`、`committed_round = 8`
- 本地 snapshot state 已经到 `snapshot_index = 8`
- 但 local segment 只到 `local_segment_seq = 4`，`manifest_cursor = 4`，`archive_cursor = 3`
- worker 会从 `peer-a`、`peer-b` 拉两份 persistence summary
- 再把 local inspect、remote summary gather、manifest stage、archive stage 和 publish 收进同一个 persistence scope
- 最后异步发布 `runtime/consensus-snapshot-persistence-published.json`

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
	uint32 iPersistenceTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoSnapshotPersistencePolicy;

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
	DemoSnapshotPersistencePolicy tPolicy;
	uint32 iActivePersistence;
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
	char sPersistenceState[48];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoSnapshotPersistenceCenter;

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

static bool procSaveConsensusSnapshotPersistenceCheckpoint(
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

static xvalue procLoadConsensusSnapshotPersistenceCheckpoint(const char* sCheckpointPath)
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

static bool procWriteRemoteSnapshotPersistenceSummaryStage(
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"remote_snapshot_persistence_summary_ready", 0, FALSE);
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

static bool procPrepareSnapshotPersistenceRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/consensus-snapshot-persistence/%s?round=%u", sKey, (unsigned)iRound);
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

static const char* procChooseSnapshotPersistencePlan(const DemoSnapshotPersistenceCenter* pCenter, int iPeerCount)
{
	if ( (pCenter == NULL) || (iPeerCount <= 0) ) {
		return "defer";
	}

	if ( pCenter->iActivePersistence > 0u ) {
		return "queue_wait";
	}

	if ( (pCenter->iManifestCursor < pCenter->iTargetSegmentSeq) || (pCenter->iArchiveCursor < pCenter->iManifestCursor) ) {
		return "persistence_now";
	}

	return "observe";
}

static bool procRunConsensusSnapshotPersistence(
	DemoSnapshotPersistenceCenter* pCenter,
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

	pCenter->iActivePersistence++;
	(void)procSaveConsensusSnapshotPersistenceCheckpoint(
		pCenter->sCheckpointPath,
		"consensus_snapshot_persistence_opened",
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
		if ( !procPrepareSnapshotPersistenceRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHostHeader[i],
			sizeof(arrHostHeader[i]),
			arrPeer[i],
			sKey,
			pCenter->iCommittedRound,
			pCenter->tPolicy.iPersistenceTimeoutMs) ) {
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
	(void)procWriteRemoteSnapshotPersistenceSummaryStage(
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

	procCopyText(
		pCenter->sPersistenceState,
		sizeof(pCenter->sPersistenceState),
		(pCenter->iArchiveCursor >= pCenter->iTargetSegmentSeq) ? "snapshot_persistence_finished" : "snapshot_persistence_running");

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"%s\",\n\t\"committed_owner\": \"%s\",\n\t\"installed_peer\": \"%s\",\n\t\"committed_round\": %u,\n\t\"snapshot_index\": %u,\n\t\"local_segment_seq\": %u,\n\t\"manifest_cursor\": %u,\n\t\"archive_cursor\": %u,\n\t\"target_segment_seq\": %u,\n\t\"publish_index\": %u,\n\t\"checkpoint_version\": %u,\n\t\"persistence_state\": \"%s\"\n}\n",
		pCenter->sPersistenceState,
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
		pCenter->sPersistenceState);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		goto cleanup;
	}

	bFinished = true;

cleanup:
	(void)procSaveConsensusSnapshotPersistenceCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? pCenter->sPersistenceState : "consensus_snapshot_persistence_incomplete",
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
		bFinished ? 1u : 0u);

	if ( pCenter->iActivePersistence > 0u ) {
		pCenter->iActivePersistence--;
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
	DemoSnapshotPersistenceCenter tCenter;
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

	tCenter.tPolicy.iPersistenceTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 22u;
	tCenter.iCommittedRound = 8u;
	tCenter.iSnapshotIndex = 8u;
	tCenter.iLocalSegmentSeq = 4u;
	tCenter.iManifestCursor = 4u;
	tCenter.iArchiveCursor = 3u;
	tCenter.iTargetSegmentSeq = 7u;
	tCenter.iPublishIndex = 8u;
	tCenter.iCheckpointVersion = 11u;
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), "node-b");
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), "peer-b");
	procCopyText(tCenter.sPersistenceState, sizeof(tCenter.sPersistenceState), "boot");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-snapshot-persistence-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/consensus-snapshot-persistence-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-consensus-snapshot-persistence-summary.json");

	(void)procSaveConsensusSnapshotPersistenceCheckpoint(
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
		0u);

	(void)procWriteSnapshotState(tCenter.iCommittedRound, "console-service", tCenter.iSnapshotIndex, tCenter.iLocalSegmentSeq);
	(void)procWriteSnapshotSegment(tCenter.iCommittedRound, tCenter.iLocalSegmentSeq, "console-service", "snapshot_segment_local_ready");

	vLoaded = procLoadConsensusSnapshotPersistenceCheckpoint(tCenter.sCheckpointPath);
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
	procCopyText(tCenter.sCommittedOwner, sizeof(tCenter.sCommittedOwner), (const char*)sOwner);
	procCopyText(tCenter.sInstalledPeer, sizeof(tCenter.sInstalledPeer), (const char*)sInstalledPeer);
	procCopyText(tCenter.sPersistenceState, sizeof(tCenter.sPersistenceState), (const char*)sState);
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

	sPlan = procChooseSnapshotPersistencePlan(&tCenter, iPeerCount);

	printf("snapshot persistence plan = %s\n", sPlan);
	printf(
		"owner=%s round=%u snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u installed_peer=%s\n",
		tCenter.sCommittedOwner,
		(unsigned)tCenter.iCommittedRound,
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sInstalledPeer);

	if ( strcmp(sPlan, "persistence_now") == 0 ) {
		if ( !procRunConsensusSnapshotPersistence(&tCenter, arrSelected, iPeerCount, "console-service") ) {
			fprintf(stderr, "consensus snapshot persistence failed\n");
			return 1;
		}
	}

	printf(
		"after snapshot persistence: snapshot=%u local_segment=%u manifest_cursor=%u archive_cursor=%u target_segment=%u publish=%u version=%u state=%s installed_peer=%s\n",
		(unsigned)tCenter.iSnapshotIndex,
		(unsigned)tCenter.iLocalSegmentSeq,
		(unsigned)tCenter.iManifestCursor,
		(unsigned)tCenter.iArchiveCursor,
		(unsigned)tCenter.iTargetSegmentSeq,
		(unsigned)tCenter.iPublishIndex,
		(unsigned)tCenter.iCheckpointVersion,
		tCenter.sPersistenceState,
		tCenter.sInstalledPeer);
	return 0;
}
```

## 6. 这一页最容易写错的边界

### 6.1 `snapshot continuation finished` 不等于 `snapshot persistence finished`

这页最容易偷懒的写法是：

- 看到 `local_segment_seq >= target_segment_seq` 就认为 persistence 已完成
- 看到 published snapshot 文件已经写出，就认为 manifest、archive 和 checkpoint version 都已经追平
- 看到某个 peer 给出更深 segment，就直接把它当成本地 durable manifest 已完成

但更稳的模型应该是：

- `snapshot continuation finished`
	- 回答 install gap、publish gap 和 continuation checkpoint 是否已经追上
- `snapshot persistence finished`
	- 回答 manifest、archive、checkpoint version 和 publish 是否都已经正式 durable

### 6.2 `manifest_cursor`、`archive_cursor` 和 `checkpoint_version` 不是一个字段

这页也很容易写混：

- 把 `local_segment_seq` 直接等同于“当前 durable baseline”
- 或者把 `manifest_cursor` 直接当成“archive 也已经一起完成了”
- 或者看到 publish 文件写出成功，就顺手把 `checkpoint_version` 当成已经正式提升

但更稳的边界应该是：

- `local_segment_seq`
	- 回答当前本地已经正式落盘到哪一段
- `manifest_cursor`
	- 回答当前 durable manifest 已经推进到了哪一段
- `archive_cursor`
	- 回答当前 archive retention 真正推进到了哪一段
- `checkpoint_version`
	- 回答这轮 durable 结果是否已经正式写进可恢复的版本边界

### 6.3 remote summary 可以提高目标，但不能直接替代本地持久化

这页还很容易偷懒成：

- 远端返回 `target_segment_seq = 6`
- 本地马上把 checkpoint 也写成 `manifest_cursor = 6`
- 再顺手把 `checkpoint_version` 也一起提升

但更稳的写法应该是：

- remote summary 只能先影响 `target_segment_seq`、peer 选择和 archive 参考值
- 真正的 `manifest_cursor` 还得等 manifest stage 正式写完
- 真正的 `archive_cursor` 还得等 archive stage 正式写完
- 真正的 `checkpoint_version` 还得等 publish 和 checkpoint 更新都正式完成

## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正讲 remote snapshot segment 的传输、校验和断点续拉协议，应继续补更重的 snapshot transport 主线
- 如果需要把 manifest retention、archive retention、publish 和更长生命周期的 `checkpoint_version` 合并进同一条长期主线，应继续补更重的 snapshot persistence scheduler 主线
- 如果需要把 persistence 主线扩展到更完整的多轮 retry、quota 和 admission 控制，应继续补更重的 snapshot scheduler / quota 主线

## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `committed_owner / committed_round / snapshot_index / local_segment_seq / manifest_cursor / archive_cursor / target_segment_seq / publish_index / checkpoint_version / persistence_state`
2. `POST /api/consensus-snapshot-persistence`
	- 只提交 persistence 意图，让 worker 决定是否进入 inspect / gather / manifest / archive / publish 主线
3. `GET /api/consensus-snapshot-persistence`
	- 直接返回最近一次 snapshot persistence checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 persistence 的重试和 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 manifest 进度、archive 进度、checkpoint version、publish 和 recent persistence history

## 9. 下一步阅读

如果你准备继续把 snapshot persistence 主线做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识快照编排面板](consensus-snapshot-orchestration-dashboard.md)
	先回看为什么更早的页面只回答 snapshot 边界，而这一页开始回答 durable manifest、archive 和 publish 的长期收口
2. [把本地控制台服务升级成一个快照续传编排面板](snapshot-continuation-dashboard.md)
	对比“install / publish 继续推进”和“manifest / archive / checkpoint version 正式 durable”到底差在哪里
3. [把本地控制台服务升级成一个快照调度配额面板](snapshot-scheduler-quota-dashboard.md)
	继续把 active slot、waiting queue、retry budget、next wake 和 admitted persistence scope 收成同一个正式 scheduler 主线
