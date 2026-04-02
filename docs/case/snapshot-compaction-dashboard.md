# 把本地控制台服务升级成一个快照压缩面板

> 这页要解决的不是“日志回放做完后顺手删几个文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `commit_index / last_applied / replay_from / replay_to / replay_cursor` 之后，又开始需要把“哪些 committed-log 已经可以被快照吸收”“什么时候才能正式推进 `snapshot_index / truncated_to`”“为什么 `state-machine.json`、snapshot 文件和 replay checkpoint 不能混成同一份状态”“如果快照已经写出但日志截断只做了一半，下次启动应该从哪里继续补 compaction”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 replay 结束后靠人工清理旧日志。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 状态机提交怎样稳定回答 `commit_index / last_applied / apply_done`
- 日志回放怎样稳定回答 `replay_from / replay_to / replay_cursor`

但真实系统再往前走一步，很快又会出现一个新的问题：

- replay 完成之后，历史 committed-log 不可能无限保留
- 如果系统已经把 `last_applied` 追到 `commit_index`
- 那么旧日志就应该被正式吸收到一个 snapshot 里
- 然后再按正式策略推进日志截断

如果这时还把实现停在：

- `if ( last_applied == commit_index ) delete old logs;`
- `state-machine.json is good enough`

很快就会出现几个典型问题：

- 旧日志删掉了，但没有正式 snapshot，可恢复性直接下降
- snapshot 写出了一半，下次启动不知道该不该继续截断
- `truncated_to` 没有正式状态，dashboard 和脚本都无法确认 compaction 进度
- 系统越来越依赖人工判断“哪些日志看起来已经没用了”

所以这页真正要补出的，不是“一个清理脚本”，而是：

> 当 replay 已经把 apply gap 补齐之后，怎样把 snapshot 生成、published snapshot、日志截断和 checkpoint 更新做成一条可恢复、可解释、可导出的正式快照压缩主线。


## 2. 为什么这次不能只靠“日志回放结束后删旧文件”

### 2.1 日志回放只回答“gap 已经补齐”

上一页里的日志回放已经很好地解决了：

- `commit_index > last_applied` 时怎样发现 replay gap
- 多条 committed entry 怎样按顺序 replay
- replay 完成之后怎样正式发布状态机快照

但它不回答：

- 哪些旧日志已经被 snapshot 吸收
- 当前 `snapshot_index` 和 `truncated_to` 是多少
- 这次 compaction 进行到一半时，下一次启动应该从哪里继续


### 2.2 日志截断不是一个附带动作，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前 snapshot 覆盖到了哪条 `log_index`
- 当前旧日志已经正式截断到哪里
- 如果 snapshot 已经写出，但只删除了一部分旧日志
- 那么 checkpoint 必须明确记录这次 compaction 还没有完整收口

这说明：

- `snapshot_index` 和 `truncated_to` 不是旁注
- 它们本身就是正式恢复状态


### 2.3 这次真正新增的是“snapshot / compaction 状态层”

更稳的分工方式是：

- `log replay`
	- 负责把 committed gap 补齐到 `last_applied`
- `snapshot compaction`
	- 负责生成 snapshot 并正式推进 `truncated_to`
- `lease / failover`
	- 再决定 compaction 期间 ownership 怎样切换

一句话记住：

> 上一页补的是“如何把 gap replay 完”，这一页补的是“replay 完之后，怎样把历史日志正式吸收到 snapshot 并推进截断边界”。


## 3. 这条快照压缩主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active compaction registry | 当前有哪些 snapshot compaction 正在运行 |
| `list` | recent compaction history | 页面和 JSON 展示最近 snapshot / truncation 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 compaction |
| `queue + thread` | 后台消费 `COMPACT / SWEEP` | 请求线程不阻塞在 snapshot 和截断上 |
| `task group` | 管住一次 compaction scope 里的 load / snapshot / truncate child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 log load、snapshot write、truncate delete、publish 的完成态 | 统一 join 和失败边界 |
| `file + path` | 读取 committed-log、写 snapshot、删除旧日志、装载 checkpoint | 让 compaction 进入正式文件主线 |
| `file_async` | 把 snapshot 异步发布成对外可读的 published snapshot | 状态发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 compaction checkpoint 与 snapshot 文件 | 让 `snapshot_index / truncated_to / compaction_cursor` 有正式结构 |
| `time` | 记录 compaction 启动时间、snapshot 时间和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `snapshot_index / truncated_to / compaction_cursor / last_applied` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `log replay` 管 gap 如何补齐，`snapshot compaction` 管补齐之后哪些历史日志可以正式被吸收到 snapshot 并截断。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成快照压缩语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/snapshot-compaction-checkpoint.json
runtime/committed-log/<index>.json
runtime/state-machine.json
runtime/state-machine-snapshot.json
runtime/state-machine-snapshot-published.json
```

其中：

- `config/console.json`
	- 保存 `compaction_batch`、`publish_timeout_ms`、`state_flush_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 `snapshot_index / truncated_to / last_applied / compaction_cursor`
- `runtime/console.log`
	- 记录 compaction 启动、snapshot 写出、truncate 删除和 defer
- `runtime/snapshot-compaction-checkpoint.json`
	- 保存最近一次正式 compaction 状态
- `runtime/committed-log/<index>.json`
	- 保存尚未被 snapshot 吸收的 committed entry
- `runtime/state-machine.json`
	- 保存当前已 apply 的业务状态
- `runtime/state-machine-snapshot.json`
	- 保存本次 compaction 生成的正式 snapshot
- `runtime/state-machine-snapshot-published.json`
	- 保存异步发布后的 snapshot 输出


## 5. 先把“启动装载 checkpoint -> 检查 snapshot gap -> 读取 committed-log -> 写 snapshot -> truncate 旧日志 -> publish snapshot”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 compaction checkpoint
2. checkpoint 里明确记录 `snapshot_index / truncated_to / compaction_cursor / last_applied`
3. `dict` 表示当前 active compaction registry
4. `list` 表示 compaction / defer histories
5. `queue + thread` 仍然只承接 `COMPACT / SWEEP`
6. `task group` 先管住本次 compaction scope
7. 先正式读取待吸收的 committed-log，再写 snapshot
8. snapshot 写出成功之后，再正式删除旧日志
9. 最后再用 `file_async future` 异步发布 snapshot
10. join 完成之后再统一把 checkpoint 更新成 `compacted` 或 `compaction_incomplete`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`last_applied = 44`、`snapshot_index = 41`、`truncated_to = 41`
- 同时准备 42、43、44 这 3 个 committed-log 文件
- worker 启动后正式发现 compaction gap：`42 -> 44`
- 然后读取这 3 条日志，写出 `runtime/state-machine-snapshot.json`
- 最后删除 42、43、44 这 3 个 committed-log，并发布 `runtime/state-machine-snapshot-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_COMPACT 1
#define DEMO_SIGNAL_SWEEP 2
#define DEMO_MAX_BATCH 8

typedef struct
{
	xtime tTouchedAt;
	uint64 iKeyHash;
	int iHeat;
	char sKey[64];
	char sTitle[64];
} DemoWarmItem;

typedef struct
{
	xtime tAt;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	uint32 iLastApplied;
	char sKey[64];
	char sState[32];
	char sReason[64];
} DemoCompactEvent;

typedef struct
{
	uint32 iCompactionBatch;
	uint32 iPublishTimeoutMs;
	uint32 iStateFlushTimeoutMs;
} DemoCompactPolicy;

typedef struct
{
	xdict pActive;
	xlist pCompactionHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoCompactPolicy tPolicy;
	uint32 iActiveCompaction;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	uint32 iCompactionCursor;
	char sCheckpointPath[260];
	char sStatePath[260];
	char sSnapshotPath[260];
	char sPublishedPath[260];
} DemoCompactCenter;

typedef struct
{
	char sPath[260];
	uint32 iLogIndex;
	int iStatus;
} DemoLoadTask;

typedef struct
{
	char sPath[260];
	uint32 iLogIndex;
	int iStatus;
} DemoDeleteTask;

typedef struct
{
	char sStatePath[260];
	char sSnapshotPath[260];
	char sKey[64];
	uint32 iSnapshotIndex;
	uint32 iLastApplied;
	int iStatus;
} DemoSnapshotTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPath(char* sDst, size_t iCap, const char* sDir, uint32 iIndex, const char* sExt)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(
		sDst,
		iCap,
		"runtime/%s/%08u%s",
		(sDir != NULL) ? sDir : "state",
		(unsigned)iIndex,
		(sExt != NULL) ? sExt : ".json");
}

static bool procSaveCompactionCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	uint32 iLastApplied,
	uint32 iSnapshotIndex,
	uint32 iTruncatedTo,
	uint32 iCompactionCursor,
	uint32 iCompactionDone)
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
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)iLastApplied);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "truncated_to", 0, (int32)iTruncatedTo);
	xvoTableSetInt(vRoot, "compaction_cursor", 0, (int32)iCompactionCursor);
	xvoTableSetInt(vRoot, "compaction_done", 0, (int32)iCompactionDone);
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

static xvalue procLoadCompactionCheckpoint(const char* sCheckpointPath)
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

static bool procWriteCommittedLog(uint32 iLogIndex, const char* sKey)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildPath(sPath, sizeof(sPath), "committed-log", iLogIndex, ".json");

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"committed", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "log_index", 0, (int32)iLogIndex);
	xvoTableSetInt(vRoot, "committed_at", 0, (int32)xrtNow());

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

static bool procWriteStateMachineFile(const char* sStatePath, const char* sKey, uint32 iLastApplied)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( sStatePath == NULL ) {
		return false;
	}

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"applied", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)iLastApplied);
	xvoTableSetInt(vRoot, "updated_at", 0, (int32)xrtNow());

	sDir = xrtPathGetDir((str)sStatePath, 0u);
	if ( (sDir != NULL) && !xrtDirCreateAll(sDir) ) {
		goto cleanup;
	}

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		goto cleanup;
	}

	bOk = xrtFileWriteAll((str)sStatePath, sJson, iJsonSize, XRT_CP_UTF8) > 0;

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

static int32 procLoadCommittedLogTask(ptr pArg, xfuture_result* pOut)
{
	DemoLoadTask* pTask = (DemoLoadTask*)pArg;
	str sJson = NULL;
	size_t iJsonSize = 0;
	xvalue vRoot = NULL;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

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

static int32 procWriteSnapshotTask(ptr pArg, xfuture_result* pOut)
{
	DemoSnapshotTask* pTask = (DemoSnapshotTask*)pArg;
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"snapshotted", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)pTask->sKey, 0, FALSE);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)pTask->iSnapshotIndex);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)pTask->iLastApplied);
	xvoTableSetInt(vRoot, "updated_at", 0, (int32)xrtNow());

	sDir = xrtPathGetDir((str)pTask->sSnapshotPath, 0u);
	if ( (sDir != NULL) && !xrtDirCreateAll(sDir) ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	bOk = xrtFileWriteAll((str)pTask->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) > 0;
	pTask->iStatus = bOk ? XRT_NET_OK : XRT_NET_ERROR;

finish:
	if ( sJson != NULL ) {
		xrtFree(sJson);
	}
	if ( sDir != NULL ) {
		xrtFree(sDir);
	}
	if ( vRoot != NULL ) {
		xvoUnref(vRoot);
	}

	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static int32 procDeleteCommittedLogTask(ptr pArg, xfuture_result* pOut)
{
	DemoDeleteTask* pTask = (DemoDeleteTask*)pArg;
	bool bOk = false;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	if ( !xrtPathExists((str)pTask->sPath) ) {
		pTask->iStatus = XRT_NET_OK;
	} else {
		bOk = xrtFileDelete((str)pTask->sPath);
		pTask->iStatus = bOk ? XRT_NET_OK : XRT_NET_ERROR;
	}

	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static bool procNeedCompaction(const DemoCompactCenter* pCenter)
{
	return (pCenter != NULL) && (pCenter->iLastApplied > pCenter->iSnapshotIndex);
}

static bool procRunSnapshotCompaction(DemoCompactCenter* pCenter, const char* sKey)
{
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_BATCH];
	DemoLoadTask arrLoad[DEMO_MAX_BATCH];
	DemoDeleteTask arrDelete[DEMO_MAX_BATCH];
	DemoSnapshotTask tSnapshot;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	str sSnapshotJson = NULL;
	size_t iSnapshotJsonSize = 0;
	uint32 iIndex = 0u;
	uint32 iCount = 0u;
	bool bOk = true;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrLoad, 0, sizeof(arrLoad));
	memset(arrDelete, 0, sizeof(arrDelete));
	memset(&tSnapshot, 0, sizeof(tSnapshot));

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return false;
	}

	pCenter->iCompactionCursor = pCenter->iSnapshotIndex;
	(void)procSaveCompactionCheckpoint(
		pCenter->sCheckpointPath,
		"compaction_opened",
		sKey,
		pCenter->iLastApplied,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		pCenter->iCompactionCursor,
		0u);

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		return false;
	}

	for ( iIndex = pCenter->iSnapshotIndex + 1u; iIndex <= pCenter->iLastApplied; iIndex++ ) {
		procBuildPath(arrLoad[iCount].sPath, sizeof(arrLoad[iCount].sPath), "committed-log", iIndex, ".json");
		arrLoad[iCount].iLogIndex = iIndex;

		arrChild[iCount] = xTaskGroupRunThread(pGroup, procLoadCommittedLogTask, &arrLoad[iCount], 0u);
		if ( arrChild[iCount] == NULL ) {
			xTaskGroupCancel(pGroup);
			bOk = false;
			goto cleanup;
		}
		iCount++;
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iStateFlushTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	for ( iIndex = 0u; iIndex < iCount; iIndex++ ) {
		if ( arrLoad[iIndex].iStatus != XRT_NET_OK ) {
			bOk = false;
			goto cleanup;
		}
	}

	xFutureRelease(pJoin);
	pJoin = NULL;
	for ( iIndex = 0u; iIndex < iCount; iIndex++ ) {
		xFutureRelease(arrChild[iIndex]);
		arrChild[iIndex] = NULL;
	}
	xTaskGroupDestroy(pGroup);
	pGroup = NULL;

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		bOk = false;
		goto cleanup;
	}

	procCopyText(tSnapshot.sStatePath, sizeof(tSnapshot.sStatePath), pCenter->sStatePath);
	procCopyText(tSnapshot.sSnapshotPath, sizeof(tSnapshot.sSnapshotPath), pCenter->sSnapshotPath);
	procCopyText(tSnapshot.sKey, sizeof(tSnapshot.sKey), sKey);
	tSnapshot.iSnapshotIndex = pCenter->iLastApplied;
	tSnapshot.iLastApplied = pCenter->iLastApplied;

	arrChild[0] = xTaskGroupRunThread(pGroup, procWriteSnapshotTask, &tSnapshot, 0u);
	if ( arrChild[0] == NULL ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iStateFlushTimeoutMs) || (tSnapshot.iStatus != XRT_NET_OK) ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	xFutureRelease(pJoin);
	pJoin = NULL;
	xFutureRelease(arrChild[0]);
	arrChild[0] = NULL;
	xTaskGroupDestroy(pGroup);
	pGroup = NULL;

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		bOk = false;
		goto cleanup;
	}

	iCount = 0u;
	for ( iIndex = pCenter->iTruncatedTo + 1u; iIndex <= pCenter->iLastApplied; iIndex++ ) {
		procBuildPath(arrDelete[iCount].sPath, sizeof(arrDelete[iCount].sPath), "committed-log", iIndex, ".json");
		arrDelete[iCount].iLogIndex = iIndex;

		arrChild[iCount] = xTaskGroupRunThread(pGroup, procDeleteCommittedLogTask, &arrDelete[iCount], 0u);
		if ( arrChild[iCount] == NULL ) {
			xTaskGroupCancel(pGroup);
			bOk = false;
			goto cleanup;
		}
		iCount++;
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iStateFlushTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		bOk = false;
		goto cleanup;
	}

	for ( iIndex = 0u; iIndex < iCount; iIndex++ ) {
		if ( arrDelete[iIndex].iStatus != XRT_NET_OK ) {
			bOk = false;
			goto cleanup;
		}
	}

	sSnapshotJson = xrtFileReadAll((str)pCenter->sSnapshotPath, XRT_CP_UTF8, &iSnapshotJsonSize);
	if ( sSnapshotJson == NULL ) {
		bOk = false;
		goto cleanup;
	}

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, sSnapshotJson, iSnapshotJsonSize, XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		bOk = false;
		goto cleanup;
	}

	pCenter->iSnapshotIndex = pCenter->iLastApplied;
	pCenter->iTruncatedTo = pCenter->iLastApplied;
	pCenter->iCompactionCursor = pCenter->iLastApplied;

cleanup:
	if ( pPublish != NULL ) {
		xFutureRelease(pPublish);
	}
	if ( sSnapshotJson != NULL ) {
		xrtFree(sSnapshotJson);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( iIndex = 0u; iIndex < DEMO_MAX_BATCH; iIndex++ ) {
		if ( arrChild[iIndex] != NULL ) {
			xFutureRelease(arrChild[iIndex]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	(void)procSaveCompactionCheckpoint(
		pCenter->sCheckpointPath,
		bOk ? "compacted" : "compaction_incomplete",
		sKey,
		pCenter->iLastApplied,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		pCenter->iCompactionCursor,
		bOk ? 1u : 0u);

	return bOk;
}

int main(void)
{
	DemoCompactCenter tCenter;
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const char* sPlan = NULL;
	uint32 iIndex = 0u;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iCompactionBatch = 8u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iStateFlushTimeoutMs = 5000u;
	tCenter.iCommitIndex = 44u;
	tCenter.iLastApplied = 44u;
	tCenter.iSnapshotIndex = 41u;
	tCenter.iTruncatedTo = 41u;
	tCenter.iCompactionCursor = 41u;
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/snapshot-compaction-checkpoint.json");
	procCopyText(tCenter.sStatePath, sizeof(tCenter.sStatePath), "runtime/state-machine.json");
	procCopyText(tCenter.sSnapshotPath, sizeof(tCenter.sSnapshotPath), "runtime/state-machine-snapshot.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/state-machine-snapshot-published.json");

	(void)procSaveCompactionCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.iLastApplied,
		tCenter.iSnapshotIndex,
		tCenter.iTruncatedTo,
		tCenter.iCompactionCursor,
		0u);

	(void)procWriteStateMachineFile(tCenter.sStatePath, "theta", tCenter.iLastApplied);
	for ( iIndex = 42u; iIndex <= 44u; iIndex++ ) {
		(void)procWriteCommittedLog(iIndex, "theta");
	}

	vBoot = procLoadCompactionCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s last_applied=%d snapshot_index=%d truncated_to=%d compaction_cursor=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			(int)xvoTableGetInt(vBoot, "last_applied", 0),
			(int)xvoTableGetInt(vBoot, "snapshot_index", 0),
			(int)xvoTableGetInt(vBoot, "truncated_to", 0),
			(int)xvoTableGetInt(vBoot, "compaction_cursor", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 120;
	tBetaWarm.iKeyHash = 11001u;
	tBetaWarm.iHeat = 8;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	sPlan = procNeedCompaction(&tCenter) ? "compact_now" : "no_compaction";
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "compact_now") == 0 ) {
		tCenter.iActiveCompaction = 1u;
		if ( procRunSnapshotCompaction(&tCenter, "theta") ) {
			tCenter.iActiveCompaction = 0u;
		}
	}

	printf("checkpoint=%s snapshot=%s published=%s truncated_to=%u\n",
		tCenter.sCheckpointPath,
		tCenter.sSnapshotPath,
		tCenter.sPublishedPath,
		(unsigned)tCenter.iTruncatedTo);
	return 0;
}
```


## 6. 快照压缩里真正要稳定下来的边界

### 6.1 `snapshot_index / truncated_to / compaction_cursor` 必须进入正式 checkpoint

这页真正稳定下来的，不是“旧日志又少了几个”，而是：

- 当前 snapshot 覆盖到了哪里
- 当前日志已经正式截断到哪里
- 当前 compaction 已经推进到哪里

如果这些字段不进入正式 checkpoint，下次启动就无法稳定继续收尾。


### 6.2 先写 snapshot，再截断旧日志

这页最容易写错的一点是：

- 先删 committed-log，再去写 snapshot

但更稳的模型应该是：

- 先正式读取 committed-log
- 再写 snapshot
- snapshot 成功后，才允许推进截断

否则系统会先失去恢复输入，再尝试生成恢复输出。


### 6.3 `state-machine.json`、snapshot 文件和 compaction checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为状态机文件已经足够回答 compaction 状态

但更稳的边界应该是：

- `runtime/state-machine.json`
	- 回答“当前业务状态是什么”
- `runtime/state-machine-snapshot.json`
	- 回答“哪一份正式 snapshot 已经生成”
- `runtime/snapshot-compaction-checkpoint.json`
	- 回答“compaction 当前走到了哪一步”


### 6.4 这页先停在“snapshot / truncation 完成”，不提前把它说成“租约和 ownership 已经安全切换”

这页故意把边界停在：

- snapshot 已经正式写出
- 旧日志已经正式截断
- published snapshot 已经异步发布

但它还没有回答：

- compaction 期间 leader 失效怎么办
- ownership 怎样切换
- 另一台节点怎样接手这次未完成 compaction

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要分段日志和大批量历史数据，应继续补 log segment / manifest 主线
- 如果需要 snapshot 版本协调，应继续补租约或 ownership 主线
- 如果需要 compaction 期间的接管和恢复，应继续补故障切换主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `snapshot_index / truncated_to / compaction_cursor / last_applied`
2. `POST /api/compact`
	- 只提交 compaction 意图，让 worker 决定是否进入 snapshot / truncation 主线
3. `GET /api/compaction`
	- 直接返回最近一次 compaction checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 compaction 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 snapshot 状态、truncate 进度、recent compaction history 和当前状态机摘要


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
	先对比“gap replay 完成”与“历史日志已经正式被 snapshot 吸收”到底差在哪
2. [把本地控制台服务升级成一个状态机提交面板](state-machine-commit-dashboard.md)
	回看 `last_applied` 为什么仍然是 compaction 的底层前提
3. [把本地控制台服务升级成一个租约故障切换面板](lease-failover-dashboard.md)
	把 compaction 期间的 ownership、接管和恢复主线正式补出来
