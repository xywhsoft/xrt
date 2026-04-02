# 把本地控制台服务升级成一个共识日志复制面板

> 这页要解决的不是“仲裁成功之后再把结果写一次文件”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `term / majority / commit index / checkpoint 装载` 之后，又开始需要把“这次 ballot 最终通过的不是一个抽象状态，而是一条正式 log entry”“leader 本地 append 完成之后，还要把同一个 `log_index` 复制给多个 follower”“什么时候只能记成 `append_opened`，什么时候才能正式推进 `commit_index`”“为什么 `majority committed` 和 `log replicated` 仍然不能和下一层 `state-machine apply` 混成一件事”放进同一条后台主线时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 串成一条正式主线，而不是继续让上层把仲裁结果直接当成最终业务提交。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 持久化协调怎样把 checkpoint 变成正式状态面
- 一致性仲裁怎样把 `term / accepted_votes / commit_index` 放进正式裁决主线

但真实系统再往前走一步，很快又会出现一个新的问题：

- 上一页里的 `commit_index` 仍然更像“这次裁决通过了”
- 它还没有稳定回答“到底是哪一条 log entry 被复制和提交了”
- 如果 follower 只返回“同意”而没有稳定绑定到具体 `log_index`
- 那么 dashboard、脚本和恢复代码都无法回答“当前已经复制到哪一条日志”

如果这时还把实现停在：

- `if ( accepted_votes >= quorum ) commit_index++;`
- `checkpoint.state = "committed";`

很快就会出现几个典型问题：

- `commit` 没有绑定到正式 `log_index`
- follower 当前匹配到哪条 entry 完全不可见
- checkpoint 里只有“仲裁成功”，没有“日志复制成功”
- 下一层 `state-machine apply` 会被迫去猜“当前到底有哪些 entry 已经真正可提交”

所以这页真正要补出的，不是“在仲裁页里再加几个字段”，而是：

> 当多数派裁决已经成立之后，怎样把正式 log entry 的 append、peer replication、majority commit 和 checkpoint 更新做成一条可恢复、可解释、可导出的共识日志复制主线。


## 2. 为什么这次不能只靠“一致性仲裁 + commit_index”

### 2.1 一致性仲裁只回答“当前 ballot 有没有通过”

上一页里的一致性仲裁已经很好地解决了：

- 当前 ballot 属于哪个 `term`
- 本地 vote 和远端 vote 如何形成 majority
- 当前 `commit_index` 什么时候可以推进

但它不回答：

- 这次通过的是哪一条正式 log entry
- 哪些 follower 已经复制到了这个 `log_index`
- 当前 checkpoint 里记录的到底是“票数通过”还是“日志已经提交”


### 2.2 `commit_index` 不是日志复制细节本身

到了这一层，系统真正要稳定回答的是：

- 这次 leader 打开的 entry `log_index` 是多少
- 当前有多少 follower 已经复制到了这个 entry
- 多数派成立之后，checkpoint 该记成 `committed`
- 多数派未成立时，checkpoint 该记成 `replication_rejected`

这说明：

- `commit_index` 仍然很重要
- 但它只是“哪些日志已经可提交”的结果
- 它不能替代 entry 本身的复制过程和 `matched_index`


### 2.3 这次真正新增的是“log replication 状态层”

更稳的分工方式是：

- `consensus arbitration`
	- 决定这次 majority 有没有形成
- `log replication`
	- 决定哪条 entry 被复制到哪些 follower，并推进 `commit_index`
- `state-machine commit`
	- 再决定哪些已提交 entry 真正应用到业务状态

一句话记住：

> 上一页补的是“多数派有没有正式裁决”，这一页补的是“正式裁决之后，哪条日志真正完成了复制并进入 commit”。 


## 3. 这条共识日志复制主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active replication registry | 当前有哪些 log replication 正在运行 |
| `list` | peer catalog | 哪些 follower 会参与复制 |
| `list` | recent replication history | 页面和 JSON 展示最近 entry、matched votes 和 commit 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式复制 |
| `list` | recent commit history | 页面和 JSON 展示哪次 replication 最终推进了 `commit_index` |
| `queue + thread` | 后台消费 `APPEND / SWEEP` | 请求线程不阻塞在 append 和 replication 上 |
| `task group` | 管住一次 replication scope 里的 local append 和 peer append child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local append、每个 peer append、commit publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向 follower 发起 append 请求 | 让 replication 进入正式网络主线 |
| `value + json` | 构造和解析 replication checkpoint | 让 `term / log_index / matched_votes / commit_index` 有正式结构 |
| `file + path` | 保存和装载 replication checkpoint | 让复制状态能跨重启恢复 |
| `file_async` | 把最终 committed entry 异步写成 committed snapshot | commit publish 仍然走正式 future 主线 |
| `time` | 记录 append 打开时间、majority 时间、commit 时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 和 entry 生成稳定指纹 | 组织 committed snapshot 路径和轻量标识 |
| `xhttpd + template` | 展示当前 term、log index、matched votes、commit index 和 defer 原因 | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `consensus arbitration` 决定这次 vote 是否通过，`log replication` 决定通过之后哪条 entry 真正进入了 commit。 


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成日志复制语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/replication-checkpoint.json
runtime/committed-log/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 `term / log_index / matched_votes / commit_index`
- `runtime/console.log`
	- 记录 local append、peer append、majority、reject 和 committed snapshot
- `runtime/replication-checkpoint.json`
	- 保存最近一次正式 replication 状态
- `runtime/committed-log/<hash>.json`
	- 保存 majority 成立后正式提交的 log entry snapshot


## 5. 先把“启动装载 checkpoint -> local append -> multi-peer append -> majority commit -> checkpoint 更新”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先从 checkpoint 文件装载最近一次 replication 状态
2. `dict` 表示当前 active replication registry
3. `list` 表示 peer catalog 和 recent histories
4. `queue + thread` 仍然只承接 `APPEND / SWEEP`
5. `task group` 只管理本地 append 和多个 peer append child
6. 本地 append entry 用线程 child 表示
7. 多个 follower append 仍然用真实 `xurl + xhttp` future 表示
8. majority 成立之后，再用 `file_async future` 正式写出 committed entry snapshot
9. checkpoint 用 `value + json + file` 持久化 `term / log_index / matched_votes / commit_index`
10. join 完成之后再统一决定这次是 `committed`、`replication_rejected` 还是 `defer`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint，然后按正式方式再读回来
- `beta` 仍有 warm shadow，不进入日志复制
- `theta` 在 `term = 8` 上打开 `log_index = 41`，本地 append + `peer-a` / `peer-b` 形成 `3/3` matched votes，并推进 `commit_index`
- `gamma` 只有一个健康 peer，无法形成 majority，这次直接 defer，并写回 checkpoint

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_APPEND 1
#define DEMO_SIGNAL_SWEEP 2
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
	char sKey[64];
	char sPeer[32];
	char sState[32];
	char sReason[64];
} DemoReplicationEvent;

typedef struct
{
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iNextTerm;
	uint32 iNextLogIndex;
} DemoRepPolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pReplicationHistory;
	xlist pDeferred;
	xlist pCommitted;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoRepPolicy tPolicy;
	uint32 iActiveReplications;
	uint32 iCommitIndex;
	char sLocalNode[32];
	char sCheckpointPath[260];
	char sCommitPath[260];
} DemoRepCenter;

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
	char sPath[260];
	char sState[32];
	char sKey[64];
	char sLeader[32];
	uint32 iTerm;
	uint32 iLogIndex;
	uint32 iMatchedVotes;
	uint32 iCommitIndex;
	uint32 iMajorityDone;
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

static bool procSaveReplicationCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sLeader,
	uint32 iTerm,
	uint32 iLogIndex,
	uint32 iMatchedVotes,
	uint32 iCommitIndex,
	uint32 iMajorityDone)
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
	xvoTableSetInt(vRoot, "majority_done", 0, (int32)iMajorityDone);
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

static xvalue procLoadReplicationCheckpoint(const char* sCheckpointPath)
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

static int32 procCheckpointTask(ptr pArg, xfuture_result* pOut)
{
	DemoCheckpointTask* pTask = (DemoCheckpointTask*)pArg;

	if ( (pTask == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pTask->iStatus = procSaveReplicationCheckpoint(
		pTask->sPath,
		pTask->sState,
		pTask->sKey,
		pTask->sLeader,
		pTask->iTerm,
		pTask->iLogIndex,
		pTask->iMatchedVotes,
		pTask->iCommitIndex,
		pTask->iMajorityDone) ? XRT_NET_OK : XRT_NET_ERROR;

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

static const char* procChooseReplicationPlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoRepCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (uint32)(iSelectedPeerCount + 1) < pCenter->tPolicy.iQuorumCount ) {
		return "defer_quorum_not_met";
	}
	return "replicate_now";
}

static bool procRunLogReplication(DemoRepCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoAppendTask tAppend;
	DemoCheckpointTask tCheckpointStart;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
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
	bool bCommitted = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tAppend, 0, sizeof(tAppend));
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
	tCheckpointStart.iMajorityDone = 0u;

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
			sFinalState = "committed";
			bCommitted = true;
		} else {
			sFinalState = "commit_publish_timeout";
		}
	}

	(void)procSaveReplicationCheckpoint(
		pCenter->sCheckpointPath,
		sFinalState,
		sKey,
		pCenter->sLocalNode,
		iTerm,
		iLogIndex,
		iMatchedVotes,
		pCenter->iCommitIndex,
		(iMatchedVotes >= pCenter->tPolicy.iQuorumCount) ? 1u : 0u);

	printf("%s -> checkpoint=%s term=%u log_index=%u votes=%u/%u commit_index=%u committed=%s\n",
		sKey,
		pCenter->sCheckpointPath,
		(unsigned)iTerm,
		(unsigned)iLogIndex,
		(unsigned)iMatchedVotes,
		(unsigned)pCenter->tPolicy.iQuorumCount,
		(unsigned)pCenter->iCommitIndex,
		bCommitted ? pCenter->sCommitPath : "no");

cleanup:
	for ( i = 0; i < iPeerCount; i++ ) {
		if ( arrResp[i] != NULL ) {
			xrtHttpResponseDestroy(arrResp[i]);
		}
		xrtHttpRequestUnit(&arrReq[i]);
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

	return bCommitted;
}

int main(void)
{
	DemoRepCenter tCenter;
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
	tCenter.tPolicy.iNextTerm = 8u;
	tCenter.tPolicy.iNextLogIndex = 41u;
	tCenter.iCommitIndex = 40u;
	procCopyText(tCenter.sLocalNode, sizeof(tCenter.sLocalNode), "node-a");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/replication-checkpoint.json");
	procBuildPath(tCenter.sCommitPath, sizeof(tCenter.sCommitPath), "committed-log", "replication", ".json");

	(void)procSaveReplicationCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"none",
		tCenter.sLocalNode,
		tCenter.tPolicy.iNextTerm - 1u,
		tCenter.iCommitIndex,
		0u,
		tCenter.iCommitIndex,
		0u);

	vBoot = procLoadReplicationCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s leader=%s term=%d log_index=%d commit_index=%d matched_votes=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			xvoTableGetText(vBoot, "leader", 0),
			(int)xvoTableGetInt(vBoot, "term", 0),
			(int)xvoTableGetInt(vBoot, "log_index", 0),
			(int)xvoTableGetInt(vBoot, "commit_index", 0),
			(int)xvoTableGetInt(vBoot, "matched_votes", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = tNow - 120;
	tBetaWarm.iKeyHash = 8001u;
	tBetaWarm.iHeat = 14;
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
	arrPeer[2].iLatencyMs = 110u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChooseReplicationPlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseReplicationPlan(NULL, iSelected, &tCenter, tNow);
	if ( strcmp(sPlan, "replicate_now") == 0 ) {
		tCenter.iActiveReplications = 1u;
		if ( procRunLogReplication(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveReplications = 0u;
		}
	}

	arrPeer[1].bHealthy = false;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseReplicationPlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		(void)procSaveReplicationCheckpoint(
			tCenter.sCheckpointPath,
			"defer_quorum_not_met",
			"gamma",
			tCenter.sLocalNode,
			tCenter.tPolicy.iNextTerm,
			tCenter.tPolicy.iNextLogIndex,
			2u,
			tCenter.iCommitIndex,
			0u);
		printf("gamma -> %s\n", sPlan);
	}

	printf("checkpoint=%s commit=%s\n", tCenter.sCheckpointPath, tCenter.sCommitPath);
	return 0;
}
```


## 6. 共识日志复制里真正要稳定下来的边界

### 6.1 `log_index / matched_votes / commit_index` 必须一起进入 checkpoint

这页真正稳定下来的，不是“又一次 majority 成立了”，而是：

- 当前 checkpoint 属于哪个 `term`
- 当前 entry 的 `log_index` 是多少
- 当前到底有多少 `matched_votes`
- 当前 `commit_index` 已经推进到哪里

如果这些字段不进入正式 checkpoint，重启之后系统就无法回答：

- 上次只是 ballot 通过
- 还是 entry 已经复制并提交


### 6.2 local append 也是 replication 的正式一环

这页最容易写错的一点是：

- 只统计 follower append，不把本地 append 计入 matched votes

但更稳的模型应该是：

- leader 先本地 append entry
- 本地 append 成功后，这一票也进入 `matched_votes`
- follower append 再和它一起决定是否可以推进 `commit_index`


### 6.3 `committed-log` 文件和 `replication checkpoint` 仍然不是同一份文件

这页里也很容易写混：

- 以为 committed entry 文件已经足够回答全部复制状态

但更稳的边界应该是：

- committed entry 文件
	- 回答“哪条 entry 已经正式提交”
- replication checkpoint
	- 回答“当前复制状态走到了哪一步”


### 6.4 这页先停在“日志提交”，不提前把它说成“状态机已经应用”

这页故意把边界停在：

- entry 已经复制
- majority 已经成立
- `commit_index` 已经推进

但它还没有回答：

- 这条 entry 是否已经真正应用到业务状态机
- apply 失败时怎样回报
- apply 顺序和 `last_applied` 怎样恢复

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要真正的日志回放，应继续补 WAL 或 log segment 主线
- 如果需要 follower catch-up 和 gap repair，应继续补 `next_index / match_index` 的正式恢复逻辑
- 如果需要日志应用到正式状态机，应继续补 `state-machine apply / last_applied` 这条主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `term / log_index / matched_votes / commit_index`
2. `POST /api/append`
	- 只提交新 entry 意图，让 worker 决定是否打开 replication
3. `GET /api/replication`
	- 直接返回最近一次 replication checkpoint
4. `POST /api/sweep`
	- 手工或定时触发超时 replication 清理与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 current term、recent replication history、recent defer history 和 committed entry 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个一致性仲裁面板](consensus-arbitration-dashboard.md)
	先对比“票数通过”与“entry 真正复制并提交”到底差在哪
2. [把本地控制台服务升级成一个持久化协调面板](persistent-coordination-dashboard.md)
	回看 checkpoint 装载为什么仍然是这条复制主线的底层支撑
3. [把本地控制台服务升级成一个状态机提交面板](state-machine-commit-dashboard.md)
	把 `commit_index` 继续升级成真正可恢复的 `last_applied` 与业务状态提交主线
