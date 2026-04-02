# 把本地控制台服务升级成一个状态机提交面板

> 这页要解决的不是“日志已经 committed 了，所以业务状态自然就算更新了”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `term / log_index / matched_votes / commit_index / replication checkpoint` 之后，又开始需要把“当前 committed entry 什么时候真正进入业务状态机”“`last_applied` 为什么不能和 `commit_index` 混成同一个字段”“如果 committed entry 已经写出，但 apply 还没完成，checkpoint 应该如何正式表示”“重启之后为什么要先装载 checkpoint 和状态机文件，再决定是否继续 replay”放进同一条后台主线时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 串成一条正式主线，而不是继续让上层把“日志已提交”误当成“业务状态已经应用完成”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 一致性仲裁怎样稳定回答 `term / accepted_votes / commit_index`
- 共识日志复制怎样稳定回答 `log_index / matched_votes / committed entry`

但真实系统再往前走一步，很快又会出现一个新的问题：

- 上一页里的 `commit_index` 只表示“这条 entry 已经可提交”
- 它还没有稳定回答“这条 entry 是否已经真正应用到业务状态机”
- 如果 worker、dashboard 和恢复逻辑继续把 `commit_index` 当成“业务状态已完成”
- 一旦 apply 失败、超时或重启，系统就会立刻失真

如果这时还把实现停在：

- `if ( commit_index >= log_index ) state = done;`
- `checkpoint.state = "committed";`

很快就会出现几个典型问题：

- `commit_index` 和 `last_applied` 被硬塞成一个概念
- committed entry 文件存在，但业务状态文件没有真正更新
- 重启之后 worker 无法区分“已经提交但尚未 apply”和“已经正式 apply 完成”
- 下一层的 replay / failover 会失去可靠的恢复边界

所以这页真正要补出的，不是“在日志复制页里再加一个状态字段”，而是：

> 当 log entry 已经提交之后，怎样把状态机 apply、`last_applied`、业务状态文件和 checkpoint 装载做成一条可恢复、可解释、可导出的正式状态机提交主线。


## 2. 为什么这次不能只靠“日志已 committed”

### 2.1 共识日志复制只回答“这条 entry 已经可提交”

上一页里的共识日志复制已经很好地解决了：

- leader 打开的 `log_index` 是多少
- 当前有多少 `matched_votes`
- 什么时候可以推进 `commit_index`
- committed entry 文件什么时候正式写出

但它不回答：

- 这条 committed entry 是否已经真正应用到业务状态机
- apply 失败或超时时 checkpoint 该怎么记
- `last_applied` 到底推进到了哪里


### 2.2 `commit_index` 不是 `last_applied`

到了这一层，系统真正要稳定回答的是：

- 当前有哪些 entry 已经提交
- 当前有哪些 entry 已经真正应用
- 如果 `commit_index = 41`，但 `last_applied = 40`
- 那么 dashboard 和 worker 必须能明确知道：还有一条 entry 处于“已提交未应用”状态

这说明：

- `commit_index` 仍然很重要
- 但它只是“可以应用到状态机”的上界
- 它不能替代真正的 `last_applied`


### 2.3 这次真正新增的是“state-machine apply 状态层”

更稳的分工方式是：

- `consensus log replication`
	- 决定哪条 entry 已经复制并 committed
- `state-machine apply`
	- 决定 committed entry 是否已经真正进入业务状态
- `replay / failover`
	- 再决定重启或切换之后怎样补 apply 和恢复

一句话记住：

> 上一页补的是“entry 什么时候提交”，这一页补的是“entry 提交之后，什么时候才算真正进入业务状态机”。


## 3. 这条状态机提交主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active apply registry | 当前有哪些 state-machine apply 正在运行 |
| `list` | peer catalog | 哪些 follower 仍参与复制前段主线 |
| `list` | recent replication history | 页面和 JSON 展示最近 entry、matched votes 和 commit 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 apply |
| `list` | recent apply history | 页面和 JSON 展示哪次 committed entry 最终推进了 `last_applied` |
| `queue + thread` | 后台消费 `APPEND / APPLY / SWEEP` | 请求线程不阻塞在复制和状态机提交上 |
| `task group` | 管住一次 append/apply scope 里的 local append、peer append 和 apply child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local append、每个 peer append、commit publish、apply 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向 follower 发起 append 请求 | 让复制仍然进入正式网络主线 |
| `value + json` | 构造和解析 state-machine checkpoint、状态文件 | 让 `commit_index / last_applied / apply_done` 有正式结构 |
| `file + path` | 保存和装载 checkpoint 与状态文件 | 让 apply 状态能跨重启恢复 |
| `file_async` | 把 committed entry 异步写成 committed snapshot | commit publish 仍然走正式 future 主线 |
| `time` | 记录 append、commit、apply 时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 生成稳定指纹 | 组织 committed snapshot 路径和轻量标识 |
| `xhttpd + template` | 展示当前 `term / log_index / commit_index / last_applied / apply_done` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `log replication` 决定 entry 什么时候可以提交，`state-machine apply` 决定业务状态什么时候真正完成。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成状态机提交语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/state-machine-checkpoint.json
runtime/committed-log/<hash>.json
runtime/state-machine.json
```

其中：

- `config/console.json`
	- 保存 `fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 `term / log_index / commit_index / last_applied / apply_done`
- `runtime/console.log`
	- 记录 local append、peer append、majority、commit、apply 和 defer
- `runtime/state-machine-checkpoint.json`
	- 保存最近一次正式状态机提交状态
- `runtime/committed-log/<hash>.json`
	- 保存 majority 成立后的 committed entry snapshot
- `runtime/state-machine.json`
	- 保存已经真正应用完成的业务状态快照


## 5. 先把“启动装载 checkpoint -> local append -> multi-peer append -> majority commit -> local apply -> checkpoint 更新”这条后台主线立起来

下面这段代码故意只保留 11 件事：

1. 启动时先从 checkpoint 文件装载最近一次状态机提交状态
2. `dict` 表示当前 active apply registry
3. `list` 表示 peer catalog 和 recent histories
4. `queue + thread` 仍然只承接 `APPEND / APPLY / SWEEP`
5. `task group` 先管理 local append 和多个 peer append child
6. 本地 append entry 用线程 child 表示
7. 多个 follower append 仍然用真实 `xurl + xhttp` future 表示
8. majority 成立之后，再用 `file_async future` 正式写出 committed entry snapshot
9. committed 之后再开一段独立的 apply phase，把 entry 写进 `runtime/state-machine.json`
10. checkpoint 用 `value + json + file` 持久化 `commit_index / last_applied / apply_done`
11. join 完成之后再统一决定这次是 `applied`、`apply_timeout` 还是 `defer`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint，然后按正式方式再读回来
- `beta` 仍有 warm shadow，不进入状态机提交
- `theta` 在 `term = 9` 上打开 `log_index = 42`，本地 append + `peer-a` / `peer-b` 形成 `3/3` matched votes，先推进 `commit_index`，再推进 `last_applied`
- `gamma` 只有一个健康 peer，无法形成 majority，这次直接 defer，并写回 checkpoint

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_APPEND 1
#define DEMO_SIGNAL_APPLY 2
#define DEMO_SIGNAL_SWEEP 3
#define DEMO_MAX_CHILD 8
#define DEMO_MAX_PEER 3

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
	char sName[32];
	char sBaseURL[256];
	bool bHealthy;
	uint32 iLatencyMs;
	uint32 iPriority;
} DemoPeerNode;

typedef struct
{
	xtime tAt;
	uint64 iKeyHash;
	uint32 iTerm;
	uint32 iLogIndex;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	char sKey[64];
	char sPeer[32];
	char sState[32];
	char sReason[64];
} DemoApplyEvent;

typedef struct
{
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iNextTerm;
	uint32 iNextLogIndex;
} DemoApplyPolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pReplicationHistory;
	xlist pDeferred;
	xlist pApplied;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoApplyPolicy tPolicy;
	uint32 iActiveApplies;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	char sLocalNode[32];
	char sCheckpointPath[260];
	char sCommitPath[260];
	char sStatePath[260];
} DemoApplyCenter;

typedef struct
{
	char sStage[32];
	uint32 iTerm;
	uint32 iLogIndex;
	int iDelayMs;
	int iStatus;
} DemoAppendTask;

typedef struct
{
	char sStatePath[260];
	char sKey[64];
	uint32 iLogIndex;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	int iDelayMs;
	int iStatus;
} DemoStateTask;

typedef struct
{
	char sPath[260];
	char sState[32];
	char sKey[64];
	char sLeader[32];
	uint32 iTerm;
	uint32 iLogIndex;
	uint32 iMatchedVotes;
	uint32 iCommitIndex;
	uint32 iLastApplied;
	uint32 iApplyDone;
	int iStatus;
} DemoCheckpointTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildPath(char* sDst, size_t iCap, const char* sDir, const char* sKey, const char* sExt)
{
	const char* sSafeKey = (sKey != NULL) ? sKey : "";

	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(
		sDst,
		iCap,
		"runtime/%s/%016llx%s",
		(sDir != NULL) ? sDir : "state",
		(unsigned long long)xrtHash64((ptr)sSafeKey, strlen(sSafeKey)),
		(sExt != NULL) ? sExt : ".json");
}

static bool procSaveStateMachineCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sLeader,
	uint32 iTerm,
	uint32 iLogIndex,
	uint32 iMatchedVotes,
	uint32 iCommitIndex,
	uint32 iLastApplied,
	uint32 iApplyDone)
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
	xvoTableSetText(vRoot, "leader", 0, (uint8*)((sLeader != NULL) ? sLeader : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "log_index", 0, (int32)iLogIndex);
	xvoTableSetInt(vRoot, "matched_votes", 0, (int32)iMatchedVotes);
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)iCommitIndex);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)iLastApplied);
	xvoTableSetInt(vRoot, "apply_done", 0, (int32)iApplyDone);
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

static xvalue procLoadStateMachineCheckpoint(const char* sCheckpointPath)
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

static int32 procLocalAppendTask(ptr pArg, xfuture_result* pOut)
{
	DemoAppendTask* pTask = (DemoAppendTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pTask->iDelayMs);
	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static int32 procApplyStateTask(ptr pArg, xfuture_result* pOut)
{
	DemoStateTask* pTask = (DemoStateTask*)pArg;
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	size_t iJsonSize = 0;
	bool bOk = false;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pTask->iDelayMs);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		pTask->iStatus = XRT_NET_ERROR;
		goto finish;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"applied", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)pTask->sKey, 0, FALSE);
	xvoTableSetInt(vRoot, "applied_log_index", 0, (int32)pTask->iLogIndex);
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)pTask->iCommitIndex);
	xvoTableSetInt(vRoot, "last_applied", 0, (int32)pTask->iLastApplied);
	xvoTableSetInt(vRoot, "applied_at", 0, (int32)xrtNow());

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

static int32 procCheckpointTask(ptr pArg, xfuture_result* pOut)
{
	DemoCheckpointTask* pTask = (DemoCheckpointTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pTask->iStatus = procSaveStateMachineCheckpoint(
		pTask->sPath,
		pTask->sState,
		pTask->sKey,
		pTask->sLeader,
		pTask->iTerm,
		pTask->iLogIndex,
		pTask->iMatchedVotes,
		pTask->iCommitIndex,
		pTask->iLastApplied,
		pTask->iApplyDone) ? XRT_NET_OK : XRT_NET_ERROR;

	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static bool procPreparePeerAppendRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	const char* sLeader,
	uint32 iTerm,
	uint32 iLogIndex,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[192];
	char sTerm[32];
	char sLogIndex[32];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) || (sLeader == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/log/%s?term=%u&index=%u", sKey, (unsigned)iTerm, (unsigned)iLogIndex);
	if ( !xrtUrlResolve(&tBase, sRef, sURL, iURLCap, NULL) ) {
		return false;
	}
	if ( !xrtUrlMakeHostHeader(&tBase, sHostHeader, iHostCap) ) {
		return false;
	}

	snprintf(sTerm, sizeof(sTerm), "%u", (unsigned)iTerm);
	snprintf(sLogIndex, sizeof(sLogIndex), "%u", (unsigned)iLogIndex);

	xrtHttpRequestInit(pReq);
	if ( !xrtHttpRequestSetMethod(pReq, "POST") ) {
		return false;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return false;
	}
	(void)xrtHttpRequestSetHeader(pReq, "Host", sHostHeader);
	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	(void)xrtHttpRequestSetHeader(pReq, "X-Log-Key", sKey);
	(void)xrtHttpRequestSetHeader(pReq, "X-Log-Leader", sLeader);
	(void)xrtHttpRequestSetHeader(pReq, "X-Log-Term", sTerm);
	(void)xrtHttpRequestSetHeader(pReq, "X-Log-Index", sLogIndex);
	xrtHttpRequestSetTimeout(pReq, iTimeoutMs);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, false);
	return true;
}

static int procSelectPeers(const DemoPeerNode* arrPeer, int iPeerCount, DemoPeerNode* arrSelected, int iCap, uint32 iFanoutWidth)
{
	int i;
	int iCount = 0;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( iCount >= iCap ) {
			break;
		}
		arrSelected[iCount++] = arrPeer[i];
		if ( (uint32)iCount >= iFanoutWidth ) {
			break;
		}
	}

	return iCount;
}

static const char* procChooseStateMachinePlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoApplyCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (uint32)(iSelectedPeerCount + 1) < pCenter->tPolicy.iQuorumCount ) {
		return "defer_quorum_not_met";
	}
	return "replicate_and_apply";
}

static bool procRunStateMachineCommit(DemoApplyCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xtaskgroup* pApplyGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoAppendTask tAppend;
	DemoStateTask tApply;
	DemoCheckpointTask tCheckpointStart;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xfuture* pApply = NULL;
	xfuture* pApplyJoin = NULL;
	char arrURL[DEMO_MAX_PEER][512];
	char arrHost[DEMO_MAX_PEER][320];
	char sCommitPath[260];
	const char* sCommitJson = "{\n\t\"state\": \"committed-log-entry\"\n}\n";
	uint32 iMatchedVotes = 0u;
	uint32 iTerm = 0u;
	uint32 iLogIndex = 0u;
	const char* sFinalState = "replication_rejected";
	int iChildCount = 0;
	int i;
	bool bApplied = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tAppend, 0, sizeof(tAppend));
	memset(&tApply, 0, sizeof(tApply));
	memset(&tCheckpointStart, 0, sizeof(tCheckpointStart));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	iTerm = pCenter->tPolicy.iNextTerm;
	iLogIndex = pCenter->tPolicy.iNextLogIndex;

	xrtNetEngineConfigInit(&tEngineCfg);
	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		return false;
	}
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		goto cleanup;
	}

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		goto cleanup;
	}

	procCopyText(tAppend.sStage, sizeof(tAppend.sStage), "append_local_entry");
	tAppend.iTerm = iTerm;
	tAppend.iLogIndex = iLogIndex;
	tAppend.iDelayMs = 40;
	tAppend.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalAppendTask, &tAppend, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tCheckpointStart.sPath, sizeof(tCheckpointStart.sPath), pCenter->sCheckpointPath);
	procCopyText(tCheckpointStart.sState, sizeof(tCheckpointStart.sState), "append_opened");
	procCopyText(tCheckpointStart.sKey, sizeof(tCheckpointStart.sKey), sKey);
	procCopyText(tCheckpointStart.sLeader, sizeof(tCheckpointStart.sLeader), pCenter->sLocalNode);
	tCheckpointStart.iTerm = iTerm;
	tCheckpointStart.iLogIndex = iLogIndex;
	tCheckpointStart.iMatchedVotes = 0u;
	tCheckpointStart.iCommitIndex = pCenter->iCommitIndex;
	tCheckpointStart.iLastApplied = pCenter->iLastApplied;
	tCheckpointStart.iApplyDone = 0u;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procCheckpointTask, &tCheckpointStart, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPreparePeerAppendRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHost[i],
			sizeof(arrHost[i]),
			&arrPeer[i],
			sKey,
			pCenter->sLocalNode,
			iTerm,
			iLogIndex,
			pCenter->tPolicy.iRemoteTimeoutMs) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}

		arrChild[iChildCount] = xrtHttpExecuteAsync(pEngine, &arrReq[i]);
		if ( arrChild[iChildCount] == NULL ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		if ( !xTaskGroupAddFuture(pGroup, arrChild[iChildCount]) ) {
			xTaskGroupCancel(pGroup);
			goto cleanup;
		}
		iChildCount++;
	}

	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	xTaskGroupClose(pGroup);

	if ( !xFutureWaitTimeout(pJoin, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
		printf("%s -> replication_timeout\n", sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	if ( tAppend.iStatus == XRT_NET_OK ) {
		iMatchedVotes++;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrChild[i + 2]);
		if ( (arrResp[i] != NULL) && (arrResp[i]->iStatusCode >= 200u) && (arrResp[i]->iStatusCode < 300u) ) {
			iMatchedVotes++;
		}
		printf("peer[%d]=%s term=%u log_index=%u append_status=%u\n",
			i,
			arrPeer[i].sName,
			(unsigned)iTerm,
			(unsigned)iLogIndex,
			(unsigned)((arrResp[i] != NULL) ? arrResp[i]->iStatusCode : 0u));
	}

	if ( iMatchedVotes >= pCenter->tPolicy.iQuorumCount ) {
		procBuildPath(sCommitPath, sizeof(sCommitPath), "committed-log", sKey, ".json");
		pPublish = xrtFileWriteAllAsync((str)sCommitPath, (str)sCommitJson, strlen(sCommitJson), XRT_CP_UTF8);
		if ( (pPublish != NULL) && xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
			pCenter->iCommitIndex = iLogIndex;
			pCenter->tPolicy.iNextLogIndex = iLogIndex + 1u;
			procCopyText(pCenter->sCommitPath, sizeof(pCenter->sCommitPath), sCommitPath);

			pApplyGroup = xTaskGroupCreate();
			if ( pApplyGroup == NULL ) {
				sFinalState = "apply_group_error";
				goto after_apply;
			}

			procCopyText(tApply.sStatePath, sizeof(tApply.sStatePath), pCenter->sStatePath);
			procCopyText(tApply.sKey, sizeof(tApply.sKey), sKey);
			tApply.iLogIndex = iLogIndex;
			tApply.iCommitIndex = pCenter->iCommitIndex;
			tApply.iLastApplied = iLogIndex;
			tApply.iDelayMs = 30;
			tApply.iStatus = XRT_NET_OK;

			pApply = xTaskGroupRunThread(pApplyGroup, procApplyStateTask, &tApply, 0u);
			if ( pApply == NULL ) {
				xTaskGroupCancel(pApplyGroup);
				sFinalState = "apply_schedule_error";
				goto after_apply;
			}

			pApplyJoin = xTaskGroupJoinFuture(pApplyGroup);
			if ( pApplyJoin == NULL ) {
				xTaskGroupCancel(pApplyGroup);
				sFinalState = "apply_join_error";
				goto after_apply;
			}

			xTaskGroupClose(pApplyGroup);
			if ( xFutureWaitTimeout(pApplyJoin, (int64)pCenter->tPolicy.iPublishTimeoutMs) && (tApply.iStatus == XRT_NET_OK) ) {
				pCenter->iLastApplied = iLogIndex;
				sFinalState = "applied";
				bApplied = true;
			} else {
				xTaskGroupCancel(pApplyGroup);
				sFinalState = "apply_timeout";
			}
		} else {
			sFinalState = "commit_publish_timeout";
		}
	}

after_apply:
	(void)procSaveStateMachineCheckpoint(
		pCenter->sCheckpointPath,
		sFinalState,
		sKey,
		pCenter->sLocalNode,
		iTerm,
		iLogIndex,
		iMatchedVotes,
		pCenter->iCommitIndex,
		pCenter->iLastApplied,
		bApplied ? 1u : 0u);

	printf("%s -> checkpoint=%s term=%u log_index=%u votes=%u/%u commit_index=%u last_applied=%u state=%s\n",
		sKey,
		pCenter->sCheckpointPath,
		(unsigned)iTerm,
		(unsigned)iLogIndex,
		(unsigned)iMatchedVotes,
		(unsigned)pCenter->tPolicy.iQuorumCount,
		(unsigned)pCenter->iCommitIndex,
		(unsigned)pCenter->iLastApplied,
		sFinalState);

cleanup:
	for ( i = 0; i < iPeerCount; i++ ) {
		if ( arrResp[i] != NULL ) {
			xrtHttpResponseDestroy(arrResp[i]);
		}
		xrtHttpRequestUnit(&arrReq[i]);
	}
	if ( pApplyJoin != NULL ) {
		xFutureRelease(pApplyJoin);
	}
	if ( pApply != NULL ) {
		xFutureRelease(pApply);
	}
	if ( pApplyGroup != NULL ) {
		xTaskGroupDestroy(pApplyGroup);
	}
	if ( pPublish != NULL ) {
		xFutureRelease(pPublish);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}

	return bApplied;
}

int main(void)
{
	DemoApplyCenter tCenter;
	DemoWarmItem tBetaWarm;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoPeerNode arrSelected[DEMO_MAX_PEER];
	xvalue vBoot = NULL;
	const char* sPlan = NULL;
	int iSelected = 0;
	xtime tNow = xrtNow();

	memset(&tCenter, 0, sizeof(tCenter));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(arrSelected, 0, sizeof(arrSelected));

	tCenter.tPolicy.iFanoutWidth = 2u;
	tCenter.tPolicy.iQuorumCount = 3u;
	tCenter.tPolicy.iRemoteTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iNextTerm = 9u;
	tCenter.tPolicy.iNextLogIndex = 42u;
	tCenter.iCommitIndex = 41u;
	tCenter.iLastApplied = 40u;
	procCopyText(tCenter.sLocalNode, sizeof(tCenter.sLocalNode), "node-a");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/state-machine-checkpoint.json");
	procBuildPath(tCenter.sCommitPath, sizeof(tCenter.sCommitPath), "committed-log", "state-machine", ".json");
	procCopyText(tCenter.sStatePath, sizeof(tCenter.sStatePath), "runtime/state-machine.json");

	(void)procSaveStateMachineCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"none",
		tCenter.sLocalNode,
		tCenter.tPolicy.iNextTerm - 1u,
		tCenter.iCommitIndex,
		0u,
		tCenter.iCommitIndex,
		tCenter.iLastApplied,
		0u);

	vBoot = procLoadStateMachineCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s leader=%s term=%d log_index=%d commit_index=%d last_applied=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			xvoTableGetText(vBoot, "leader", 0),
			(int)xvoTableGetInt(vBoot, "term", 0),
			(int)xvoTableGetInt(vBoot, "log_index", 0),
			(int)xvoTableGetInt(vBoot, "commit_index", 0),
			(int)xvoTableGetInt(vBoot, "last_applied", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = tNow - 120;
	tBetaWarm.iKeyHash = 9001u;
	tBetaWarm.iHeat = 12;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "https://peer-a.example.com/");
	arrPeer[0].bHealthy = true;
	arrPeer[0].iLatencyMs = 70u;
	arrPeer[0].iPriority = 1u;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "https://peer-b.example.com/");
	arrPeer[1].bHealthy = true;
	arrPeer[1].iLatencyMs = 90u;
	arrPeer[1].iPriority = 2u;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "https://peer-c.example.com/");
	arrPeer[2].bHealthy = false;
	arrPeer[2].iLatencyMs = 115u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChooseStateMachinePlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseStateMachinePlan(NULL, iSelected, &tCenter, tNow);
	if ( strcmp(sPlan, "replicate_and_apply") == 0 ) {
		tCenter.iActiveApplies = 1u;
		if ( procRunStateMachineCommit(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveApplies = 0u;
		}
	}

	arrPeer[1].bHealthy = false;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseStateMachinePlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		(void)procSaveStateMachineCheckpoint(
			tCenter.sCheckpointPath,
			"defer_quorum_not_met",
			"gamma",
			tCenter.sLocalNode,
			tCenter.tPolicy.iNextTerm,
			tCenter.tPolicy.iNextLogIndex,
			2u,
			tCenter.iCommitIndex,
			tCenter.iLastApplied,
			0u);
		printf("gamma -> %s\n", sPlan);
	}

	printf("checkpoint=%s commit=%s state=%s\n",
		tCenter.sCheckpointPath,
		tCenter.sCommitPath,
		tCenter.sStatePath);
	return 0;
}
```


## 6. 状态机提交里真正要稳定下来的边界

### 6.1 `commit_index` 和 `last_applied` 必须分开保留

这页真正稳定下来的，不是“又多写了一份状态文件”，而是：

- 当前 `commit_index` 是多少
- 当前 `last_applied` 是多少
- 当前 checkpoint 是 `committed`、`applied` 还是 `apply_timeout`

如果这些字段不分开保留，重启之后系统就无法回答：

- 上次只是日志已提交
- 还是已经真正应用到业务状态


### 6.2 apply phase 应该有自己的完成点

这页最容易写错的一点是：

- committed 之后直接把状态标成 done

但更稳的模型应该是：

- committed snapshot 写出之后，再显式跑一次 apply phase
- apply 也必须有自己的 future 和等待边界
- 只有 apply 成功后，才能推进 `last_applied`


### 6.3 `state-machine.json` 和 checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为业务状态文件已经足够回答当前恢复状态

但更稳的边界应该是：

- `runtime/state-machine.json`
	- 回答“当前业务状态已经应用到哪里”
- `runtime/state-machine-checkpoint.json`
	- 回答“当前复制、提交和 apply 状态走到了哪一步”


### 6.4 这页先停在“apply 完成”，不提前把它说成“日志回放已经完整”

这页故意把边界停在：

- 当前 committed entry 已经 apply
- `last_applied` 已经推进
- 业务状态文件已经正式写出

但它还没有回答：

- 多条历史 entry 怎样按顺序 replay
- 重启时怎样补 apply gap
- follower catch-up 之后怎样批量回放

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要多条 entry 顺序回放，应继续补日志回放主线
- 如果需要真正的快照恢复和截断，应继续补 snapshot / compaction 逻辑
- 如果需要 leader 失效后的租约和接管，应继续补租约故障切换主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `term / log_index / commit_index / last_applied / apply_done`
2. `POST /api/append`
	- 只提交新 entry 意图，让 worker 决定是否打开 replication 和 apply
3. `GET /api/state-machine`
	- 直接返回最近一次 state-machine checkpoint 与业务状态文件摘要
4. `POST /api/sweep`
	- 手工或定时触发超时 apply 清理与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 current term、recent apply history、recent defer history 和当前业务状态


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个共识日志复制面板](consensus-log-replication-dashboard.md)
	先对比“日志已 committed”与“状态机已经真正 apply”到底差在哪
2. [把本地控制台服务升级成一个一致性仲裁面板](consensus-arbitration-dashboard.md)
	回看多数派裁决为什么仍然是这条 apply 主线的底层前提
3. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
	把单次 apply 继续升级成真正可恢复的 replay gap 补齐主线
