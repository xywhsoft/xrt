# 把本地控制台服务升级成一个日志回放面板

> 这页要解决的不是“程序启动后把状态文件重新读一下”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `commit_index / last_applied / state-machine.json / checkpoint 装载` 之后，又开始需要把“重启时发现 `commit_index > last_applied` 应该怎样正式补 replay”“多条 committed entry 必须按 `log_index` 顺序 apply，为什么不能并行乱放”“checkpoint、committed-log 文件和状态机文件三者怎样互相校准”“如果 replay 只做到一半，下次启动应该从哪里继续补”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + file + file_async + path + value + json + time + hash + xhttpd + template` 串成一条正式主线，而不是继续让进程启动后靠几段临时 `for` 循环和几条日志去猜“哪些 entry 大概已经应用过了”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 共识日志复制怎样稳定回答 `log_index / matched_votes / commit_index`
- 状态机提交怎样稳定回答 `last_applied / apply_done / state-machine.json`

但真实系统再往前走一步，很快又会出现一个新的问题：

- 上一页已经知道如何把一条 committed entry apply 到状态机
- 可一旦进程在 apply 过程中重启，系统就可能留下 `commit_index > last_applied`
- 此时 worker 需要做的，不是重新开新 ballot，也不是重新复制日志
- 而是先把已经 committed 但尚未 apply 的 entry 按顺序 replay 完

如果这时还把实现停在：

- `if ( commit_index > last_applied ) while (...) apply();`
- `state-machine.json not found -> rebuild somehow`

很快就会出现几个典型问题：

- replay 起点和终点没有正式状态
- 多条 committed entry 的 apply 顺序不稳定
- 进程中途崩掉之后，下次启动不知道 replay 到哪一步
- dashboard 只能看到“当前状态文件”，看不到 replay gap

所以这页真正要补出的，不是“多写几个恢复脚本”，而是：

> 当系统重启后发现 `commit_index` 已经领先于 `last_applied` 时，怎样把 replay gap、顺序 apply、checkpoint 更新和状态文件发布做成一条可恢复、可解释、可导出的正式日志回放主线。


## 2. 为什么这次不能只靠“状态机提交页再跑一次 apply”

### 2.1 状态机提交只回答“当前这条 entry 怎样 apply”

上一页里的状态机提交已经很好地解决了：

- 当前 committed entry 如何进入 apply phase
- `last_applied` 什么时候推进
- 业务状态文件什么时候正式写出

但它不回答：

- 如果有多条 committed entry 尚未 apply，该从哪里开始 replay
- 如果 replay 做到一半崩掉，下次启动应从哪一条继续
- replay 的完成点怎样进入正式 checkpoint


### 2.2 `commit_index > last_applied` 不是一个小提示，而是正式恢复状态

到了这一层，系统真正要稳定回答的是：

- 当前 replay gap 的起点是哪条 entry
- 当前 replay gap 的终点是哪条 entry
- 当前 replay 已经做到哪一条
- 这次启动是否已经把 gap 补齐

这说明：

- `commit_index` 和 `last_applied` 的差值不是日志旁注
- 它本身就是正式恢复状态


### 2.3 这次真正新增的是“replay 状态层”

更稳的分工方式是：

- `state-machine commit`
	- 管理单条 committed entry 如何 apply
- `log replay`
	- 管理一批 committed entry 如何按顺序补齐 apply gap
- `snapshot / compaction`
	- 再决定 replay 完之后怎样截断和压缩历史

一句话记住：

> 上一页补的是“单条 committed entry 如何 apply”，这一页补的是“进程重启后，整段 apply gap 如何被正式补齐”。 


## 3. 这条日志回放主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active replay registry | 当前有哪些 replay session 正在运行 |
| `list` | recent replay history | 页面和 JSON 展示最近 replay 的起点、终点和结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 replay |
| `list` | recent apply history | 页面和 JSON 展示 replay 过程中每条 entry 的 apply 结果 |
| `queue + thread` | 后台消费 `REPLAY / SWEEP` | 请求线程不阻塞在回放上 |
| `task group` | 管住一次 replay scope 里的 load / apply child | `Close / Join / Cancel / Destroy` |
| `future` | 表达每条 entry 的 load、apply、publish 完成态 | 统一 join 和失败边界 |
| `file + path` | 读取 committed-log 文件、装载 checkpoint、写出状态文件 | 让 replay 进入正式文件主线 |
| `file_async` | 回放完成后异步发布新的状态机快照 | 状态发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 replay checkpoint 与状态文件 | 让 `replay_from / replay_to / replay_cursor / last_applied` 有正式结构 |
| `time` | 记录 replay 启动时间、每条 apply 时间和完成时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 生成稳定指纹 | 组织 committed-log 路径和状态快照标识 |
| `xhttpd + template` | 展示当前 replay gap、cursor、last_applied 和 apply 历史 | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `state-machine commit` 管单条 apply，`log replay` 管整段 committed gap 如何被按顺序补齐。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成日志回放语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/log-replay-checkpoint.json
runtime/committed-log/<index>.json
runtime/state-machine.json
runtime/state-machine-published.json
```

其中：

- `config/console.json`
	- 保存 `replay_batch`、`publish_timeout_ms`、`state_flush_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 `commit_index / last_applied / replay_from / replay_to / replay_cursor`
- `runtime/console.log`
	- 记录 replay 启动、每条 entry apply、publish 和 defer
- `runtime/log-replay-checkpoint.json`
	- 保存最近一次正式 replay 状态
- `runtime/committed-log/<index>.json`
	- 保存已 committed 但尚待回放的 entry 内容
- `runtime/state-machine.json`
	- 保存当前已 apply 的业务状态
- `runtime/state-machine-published.json`
	- 保存 replay 完成后异步发布的状态机快照


## 5. 先把“启动装载 checkpoint -> 发现 apply gap -> 顺序读取 committed-log -> 顺序 apply -> publish 状态机快照”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 replay checkpoint
2. checkpoint 里明确记录 `commit_index / last_applied / replay_from / replay_to / replay_cursor`
3. `dict` 表示当前 active replay registry
4. `list` 表示 replay / defer / apply histories
5. `queue + thread` 仍然只承接 `REPLAY / SWEEP`
6. `task group` 先管住本次 replay scope
7. 每条 committed-log 文件都先被正式读取和解析，再进入 apply
8. apply 必须按 `log_index` 顺序推进，不能并行乱序
9. replay 完成后，再用 `file_async future` 正式发布 `state-machine-published.json`
10. join 完成之后再统一把 checkpoint 更新成 `replayed` 或 `replay_incomplete`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`commit_index = 44`、`last_applied = 41`
- 同时准备 42、43、44 这 3 个 committed-log 文件
- worker 启动后正式发现 replay gap：`42 -> 44`
- 然后按顺序回放 42、43、44，并把 `last_applied` 逐步推进到 44
- 最后发布 `runtime/state-machine-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEMO_SIGNAL_REPLAY 1
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
	uint64 iKeyHash;
	uint32 iLogIndex;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	char sKey[64];
	char sState[32];
	char sReason[64];
} DemoReplayEvent;

typedef struct
{
	uint32 iReplayBatch;
	uint32 iPublishTimeoutMs;
	uint32 iStateFlushTimeoutMs;
} DemoReplayPolicy;

typedef struct
{
	xdict pActive;
	xlist pReplayHistory;
	xlist pDeferred;
	xlist pApplied;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoReplayPolicy tPolicy;
	uint32 iActiveReplay;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	uint32 iReplayFrom;
	uint32 iReplayTo;
	uint32 iReplayCursor;
	char sCheckpointPath[260];
	char sStatePath[260];
	char sPublishedPath[260];
} DemoReplayCenter;

typedef struct
{
	char sPath[260];
	uint32 iLogIndex;
	int iStatus;
} DemoLoadTask;

typedef struct
{
	char sStatePath[260];
	char sKey[64];
	uint32 iLogIndex;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	int iStatus;
} DemoApplyTask;

typedef struct
{
	char sPath[260];
	char sState[32];
	char sKey[64];
	uint32 iCommitIndex;
	uint32 iLastApplied;
	uint32 iReplayFrom;
	uint32 iReplayTo;
	uint32 iReplayCursor;
	uint32 iReplayDone;
	int iStatus;
} DemoCheckpointTask;


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

static bool procSaveReplayCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	uint32 iCommitIndex,
	uint32 iLastApplied,
	uint32 iReplayFrom,
	uint32 iReplayTo,
	uint32 iReplayCursor,
	uint32 iReplayDone)
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
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)iCommitIndex);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)iLastApplied);
	xvoTableSetInt(vRoot, "replay_from", 0, (int32)iReplayFrom);
	xvoTableSetInt(vRoot, "replay_to", 0, (int32)iReplayTo);
	xvoTableSetInt(vRoot, "replay_cursor", 0, (int32)iReplayCursor);
	xvoTableSetInt(vRoot, "replay_done", 0, (int32)iReplayDone);
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

static xvalue procLoadReplayCheckpoint(const char* sCheckpointPath)
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
	char sPath[260];
	str sDir = NULL;
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

static int32 procApplyReplayTask(ptr pArg, xfuture_result* pOut)
{
	DemoApplyTask* pTask = (DemoApplyTask*)pArg;
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

	xvoTableSetText(vRoot, "state", 0, (uint8*)"applied", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)pTask->sKey, 0, FALSE);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)pTask->iLastApplied);
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)pTask->iCommitIndex);
	xvoTableSetInt(vRoot, "updated_at", 0, (int32)xrtNow());

	sDir = xrtPathGetDir((str)pTask->sStatePath, 0u);
	if ( (sDir != NULL) && !xrtDirCreateAll(sDir) ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	sJson = xrtStringifyJSON(vRoot, TRUE, &iJsonSize);
	if ( sJson == NULL ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	bOk = xrtFileWriteAll((str)pTask->sStatePath, sJson, iJsonSize, XRT_CP_UTF8) > 0;
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

static bool procNeedReplay(const DemoReplayCenter* pCenter)
{
	return (pCenter != NULL) && (pCenter->iCommitIndex > pCenter->iLastApplied);
}

static bool procRunReplay(DemoReplayCenter* pCenter, const char* sKey)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pLoad = NULL;
	xfuture* pApply = NULL;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	DemoLoadTask tLoad;
	DemoApplyTask tApply;
	char sPublishJson[512];
	int iCount = 0;
	uint32 iIndex = 0u;
	bool bReplayOk = true;

	if ( (pCenter == NULL) || (sKey == NULL) ) {
		return false;
	}

	for ( iIndex = pCenter->iLastApplied + 1u; iIndex <= pCenter->iCommitIndex; iIndex++ ) {
		memset(&tLoad, 0, sizeof(tLoad));
		memset(&tApply, 0, sizeof(tApply));

		procBuildPath(tLoad.sPath, sizeof(tLoad.sPath), "committed-log", iIndex, ".json");
		tLoad.iLogIndex = iIndex;

		pGroup = xTaskGroupCreate();
		if ( pGroup == NULL ) {
			bReplayOk = false;
			break;
		}

		pLoad = xTaskGroupRunThread(pGroup, procLoadCommittedLogTask, &tLoad, 0u);
		if ( pLoad == NULL ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		pJoin = xTaskGroupJoinFuture(pGroup);
		if ( pJoin == NULL ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		xTaskGroupClose(pGroup);
		if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iStateFlushTimeoutMs) || (tLoad.iStatus != XRT_NET_OK) ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		xFutureRelease(pJoin);
		pJoin = NULL;
		xFutureRelease(pLoad);
		pLoad = NULL;
		xTaskGroupDestroy(pGroup);
		pGroup = NULL;

		pGroup = xTaskGroupCreate();
		if ( pGroup == NULL ) {
			bReplayOk = false;
			break;
		}

		procCopyText(tApply.sStatePath, sizeof(tApply.sStatePath), pCenter->sStatePath);
		procCopyText(tApply.sKey, sizeof(tApply.sKey), sKey);
		tApply.iLogIndex = iIndex;
		tApply.iCommitIndex = pCenter->iCommitIndex;
		tApply.iLastApplied = iIndex;

		pApply = xTaskGroupRunThread(pGroup, procApplyReplayTask, &tApply, 0u);
		if ( pApply == NULL ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		pJoin = xTaskGroupJoinFuture(pGroup);
		if ( pJoin == NULL ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		xTaskGroupClose(pGroup);
		if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iStateFlushTimeoutMs) || (tApply.iStatus != XRT_NET_OK) ) {
			xTaskGroupCancel(pGroup);
			bReplayOk = false;
			goto per_item_cleanup;
		}

		pCenter->iLastApplied = iIndex;
		pCenter->iReplayCursor = iIndex;
		iCount++;

		(void)procSaveReplayCheckpoint(
			pCenter->sCheckpointPath,
			"replaying",
			sKey,
			pCenter->iCommitIndex,
			pCenter->iLastApplied,
			pCenter->iReplayFrom,
			pCenter->iReplayTo,
			pCenter->iReplayCursor,
			0u);

per_item_cleanup:
		if ( pJoin != NULL ) {
			xFutureRelease(pJoin);
			pJoin = NULL;
		}
		if ( pApply != NULL ) {
			xFutureRelease(pApply);
			pApply = NULL;
		}
		if ( pLoad != NULL ) {
			xFutureRelease(pLoad);
			pLoad = NULL;
		}
		if ( pGroup != NULL ) {
			xTaskGroupDestroy(pGroup);
			pGroup = NULL;
		}

		if ( !bReplayOk ) {
			break;
		}
	}

	if ( !bReplayOk ) {
		(void)procSaveReplayCheckpoint(
			pCenter->sCheckpointPath,
			"replay_incomplete",
			sKey,
			pCenter->iCommitIndex,
			pCenter->iLastApplied,
			pCenter->iReplayFrom,
			pCenter->iReplayTo,
			pCenter->iReplayCursor,
			0u);
		return false;
	}

	snprintf(
		sPublishJson,
		sizeof(sPublishJson),
		"{\n\t\"state\": \"replayed\",\n\t\"last_applied\": %u,\n\t\"replayed_count\": %d\n}\n",
		(unsigned)pCenter->iLastApplied,
		iCount);

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		if ( pPublish != NULL ) {
			xFutureRelease(pPublish);
		}
		(void)procSaveReplayCheckpoint(
			pCenter->sCheckpointPath,
			"replay_publish_timeout",
			sKey,
			pCenter->iCommitIndex,
			pCenter->iLastApplied,
			pCenter->iReplayFrom,
			pCenter->iReplayTo,
			pCenter->iReplayCursor,
			0u);
		return false;
	}

	xFutureRelease(pPublish);
	(void)procSaveReplayCheckpoint(
		pCenter->sCheckpointPath,
		"replayed",
		sKey,
		pCenter->iCommitIndex,
		pCenter->iLastApplied,
		pCenter->iReplayFrom,
		pCenter->iReplayTo,
		pCenter->iReplayCursor,
		1u);

	return true;
}

int main(void)
{
	DemoReplayCenter tCenter;
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const char* sPlan = NULL;
	uint32 iIndex = 0u;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iReplayBatch = 8u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iStateFlushTimeoutMs = 5000u;
	tCenter.iCommitIndex = 44u;
	tCenter.iLastApplied = 41u;
	tCenter.iReplayFrom = 42u;
	tCenter.iReplayTo = 44u;
	tCenter.iReplayCursor = 41u;
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/log-replay-checkpoint.json");
	procCopyText(tCenter.sStatePath, sizeof(tCenter.sStatePath), "runtime/state-machine.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/state-machine-published.json");

	(void)procSaveReplayCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.iCommitIndex,
		tCenter.iLastApplied,
		tCenter.iReplayFrom,
		tCenter.iReplayTo,
		tCenter.iReplayCursor,
		0u);

	for ( iIndex = 42u; iIndex <= 44u; iIndex++ ) {
		(void)procWriteCommittedLog(iIndex, "theta");
	}

	vBoot = procLoadReplayCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s commit_index=%d last_applied=%d replay_from=%d replay_to=%d replay_cursor=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			(int)xvoTableGetInt(vBoot, "commit_index", 0),
			(int)xvoTableGetInt(vBoot, "last_applied", 0),
			(int)xvoTableGetInt(vBoot, "replay_from", 0),
			(int)xvoTableGetInt(vBoot, "replay_to", 0),
			(int)xvoTableGetInt(vBoot, "replay_cursor", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 120;
	tBetaWarm.iKeyHash = 10001u;
	tBetaWarm.iHeat = 10;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	sPlan = procNeedReplay(&tCenter) ? "replay_gap" : "no_gap";
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "replay_gap") == 0 ) {
		tCenter.iActiveReplay = 1u;
		if ( procRunReplay(&tCenter, "theta") ) {
			tCenter.iActiveReplay = 0u;
		}
	}

	printf("checkpoint=%s state=%s published=%s last_applied=%u\n",
		tCenter.sCheckpointPath,
		tCenter.sStatePath,
		tCenter.sPublishedPath,
		(unsigned)tCenter.iLastApplied);
	return 0;
}
```


## 6. 日志回放里真正要稳定下来的边界

### 6.1 `replay_from / replay_to / replay_cursor` 必须进入正式 checkpoint

这页真正稳定下来的，不是“启动时又跑了一段 apply”，而是：

- 当前 replay gap 从哪里开始
- 当前 replay 目标到哪里结束
- 当前 replay 已经做到哪里

如果这些字段不进入正式 checkpoint，进程中途崩掉之后就无法稳定继续。


### 6.2 replay 必须按 `log_index` 顺序推进

这页最容易写错的一点是：

- 看到多个 committed-log 文件后，直接并行 apply

但更稳的模型应该是：

- 先读取 42，再 apply 42
- 再读取 43，再 apply 43
- 再读取 44，再 apply 44

因为 `last_applied` 本身就是一个严格单调推进的顺序边界。


### 6.3 `state-machine.json`、published snapshot 和 replay checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为只要状态机文件存在，就不需要 replay checkpoint

但更稳的边界应该是：

- `runtime/state-machine.json`
	- 回答“当前业务状态是什么”
- `runtime/state-machine-published.json`
	- 回答“这次 replay 完成后对外发布了什么”
- `runtime/log-replay-checkpoint.json`
	- 回答“当前 replay 进度走到了哪一步”


### 6.4 这页先停在“gap replay 完成”，不提前把它说成“快照压缩已经完成”

这页故意把边界停在：

- committed gap 已经补齐
- `last_applied` 已经追到 `commit_index`
- 状态机快照已经正式发布

但它还没有回答：

- 历史 committed-log 什么时候可以截断
- 什么时候该生成压缩快照
- replay 完成后怎样进入 compaction

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要大量历史日志重放，应继续补 log segment / snapshot 边界
- 如果需要重放后立即压缩，应继续补 compaction 主线
- 如果需要 leader 切换后恢复 replay ownership，应继续补租约故障切换主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `commit_index / last_applied / replay_from / replay_to / replay_cursor`
2. `POST /api/replay`
	- 只提交 replay 意图，让 worker 决定是否补齐 apply gap
3. `GET /api/replay`
	- 直接返回最近一次 replay checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 replay 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 replay gap、recent replay history、recent apply history 和当前状态机摘要


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个状态机提交面板](state-machine-commit-dashboard.md)
	先对比“单条 committed entry apply”与“整段 apply gap replay”到底差在哪
2. [把本地控制台服务升级成一个共识日志复制面板](consensus-log-replication-dashboard.md)
	回看 committed-log 文件为什么仍然是 replay 的正式输入
3. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	把 replay 主线继续升级成 snapshot / compaction / 日志截断主线
