# 把本地控制台服务升级成一个一致性仲裁面板

> 这页要解决的不是“分布式编排多做几次 quorum 统计”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `multi-peer fan-out / quorum / checkpoint / 启动装载` 之后，又开始需要把“这次恢复到底由哪个 term 发起”“本地 prepare vote 和远端 granted vote 怎样合成正式 majority”“什么时候只能记成 ballot rejected，什么时候才能真的推进 commit index”“重启之后为什么不能只记一个 latest status，而要把仲裁状态作为正式 checkpoint 装回来”放进同一条后台主线时，怎样把 `dict + list + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + value + json + time + path + hash + xhttpd + template` 串成一条正式主线，而不是继续让每轮恢复只做“谁成功了就先用谁”的临时裁决。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 跨机器恢复怎样做 peer 选择、remote fetch 和 publish 回本地
- 分布式编排怎样做 multi-peer fan-out、quorum 和统一 publish
- 持久化协调怎样把 checkpoint 变成正式状态面，并在启动时装回来

但真实系统再往前走一步，很快又会出现一个新的问题：

- 上一页的 checkpoint 仍然更像“最近一次协调状态”
- 它还没有稳定回答“这次 commit 是不是由正式 majority 仲裁出来的”
- 也没有稳定回答“当前 term 是多少，当前 commit index 是多少”
- 一旦多台 peer 都可能返回不同版本、不同延迟和不同接受态，系统就需要一层更重的裁决逻辑

如果这时还把实现停在：

- `if ( success_count >= quorum ) commit();`
- `checkpoint.state = "published";`

很快就会出现几个典型问题：

- `quorum` 只表示“够不够多”，但不表示“是不是同一个正式 ballot”
- 页面和脚本看不到 `term / majority / commit_index`
- 重启之后 worker 只知道“上次是 published”，却不知道“published 是在哪个 term 上达成的”
- 同一条恢复主线里，瞬时网络结果和正式仲裁状态继续混在一起

所以这页真正要补出的，不是“再写一个更重的 quorum 页面”，而是：

> 当恢复链路已经进入“多数派正式裁决”的阶段时，怎样把 ballot、majority、commit index 和 checkpoint 装载做成一条可恢复、可解释、可导出的正式一致性仲裁主线。


## 2. 为什么这次不能只靠“持久化协调 + quorum”

### 2.1 持久化协调只回答“上次状态是什么”

上一页里的持久化协调已经很好地解决了：

- fan-out 开始前、结束后、defer 时怎样写 checkpoint
- worker 启动时怎样先装载 checkpoint
- dashboard 怎样展示最近一次协调状态

但它不回答：

- 当前 ballot term 是多少
- 这次多数派到底有没有形成正式裁决
- commit index 是不是已经推进


### 2.2 quorum 不是一致性仲裁本身

到了这一层，系统真正要稳定回答的是：

- 本地 candidate 在当前 term 是否先完成了本地 prepare
- 远端 granted vote 是在同一个 term 上返回的吗
- 本地 vote 加远端 vote 加起来是不是形成 majority
- 没有形成 majority 时，到底该记成 `ballot_rejected`、`defer`，还是继续重试

这说明：

- `quorum` 仍然很重要
- 但它只是多数派裁决里的一个结果
- 它不能替代 term、ballot 和 commit index 这些正式状态


### 2.3 这次真正新增的是“arbitration 状态层”

更稳的分工方式是：

- `distributed orchestration`
	- 管理这次 fan-out 给谁、谁完成了、谁超时了
- `persistent coordination`
	- 管理 checkpoint 怎样写、怎样装载
- `consensus arbitration`
	- 管理当前 term、granted vote、majority 和 commit index

一句话记住：

> 上一页补的是“状态怎么持久化”，这一页补的是“这个持久化状态里，哪些字段才算正式仲裁结果”。


## 3. 这条一致性仲裁主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active ballot registry | 当前有哪些仲裁正在运行 |
| `list` | peer catalog | 哪些节点可参与 vote，请求应该发给谁 |
| `list` | recent ballot history | 页面和 JSON 展示最近 ballot / majority / reject 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式仲裁 |
| `list` | recent commit history | 页面和 JSON 展示哪次 majority 最终推进了 commit index |
| `queue + thread` | 后台消费 `HYDRATE / SWEEP` | 请求线程不阻塞在 ballot 与 commit 上 |
| `task group` | 管住一次 ballot scope 里的 prepare 与 peer vote child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local prepare、每个 peer vote、commit publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向多个 peer 发起 vote 请求 | 让 ballot 进入正式网络主线 |
| `value + json` | 构造和解析 arbitration checkpoint | 让 `term / commit_index / accepted_votes` 有正式结构 |
| `file + path` | 保存和装载 arbitration checkpoint | 让仲裁状态能跨重启恢复 |
| `file_async` | 把最终 commit 结果异步写成 committed snapshot | commit publish 仍然走正式 future 主线 |
| `time` | 记录 ballot 打开时间、majority 时间、commit 时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 生成稳定指纹 | 组织 commit snapshot 路径和轻量标识 |
| `xhttpd + template` | 展示当前 term、majority、commit index 和 defer 原因 | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `persistent coordination` 把状态留下来，`consensus arbitration` 决定留下来的到底是不是正式多数派裁决。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成一致性仲裁语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/consensus-checkpoint.json
runtime/committed/<hash>.json
```

其中：

- `config/console.json`
	- 保存 `fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`
- `web/dashboard.html`
	- 同时展示当前 term、current ballot、recent ballot history、recent commit history
- `runtime/console.log`
	- 记录 prepare、vote、majority、reject、commit publish 和 defer
- `runtime/consensus-checkpoint.json`
	- 保存最近一次正式仲裁状态
- `runtime/committed/<hash>.json`
	- 保存 majority 达成后正式写出的 commit snapshot


## 5. 先把“启动装载 checkpoint -> propose ballot -> multi-peer vote -> majority commit -> checkpoint 更新”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先从 checkpoint 文件装载最近一次仲裁状态
2. `dict` 表示当前 active ballot registry
3. `list` 表示 peer catalog 和 recent histories
4. `queue + thread` 仍然只承接 `HYDRATE / SWEEP`
5. `task group` 只管理本地 prepare 和多个 peer vote child
6. 本地 prepare vote 用线程 child 表示
7. 多个 peer granted vote 仍然用真实 `xurl + xhttp` future 表示
8. majority 达成之后，再用 `file_async future` 正式写出 committed snapshot
9. checkpoint 用 `value + json + file` 持久化 term、accepted_votes 和 commit index
10. join 完成之后再统一决定这次是 `committed`、`ballot_rejected` 还是 `defer`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint，然后按正式方式再读回来
- `beta` 仍有 warm shadow，不进入一致性仲裁
- `theta` 在 `term = 7` 上发起 ballot，本地 vote + `peer-a` / `peer-b` 形成 `3/3` majority，推进 commit index
- `gamma` 只有一个健康 peer，可形成的票数不足 majority，这次直接 defer，并写回 checkpoint

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_HYDRATE 1
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
	uint32 iCommitIndex;
	char sKey[64];
	char sPeer[32];
	char sState[32];
	char sReason[64];
} DemoArbEvent;

typedef struct
{
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iNextTerm;
} DemoArbPolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pBallotHistory;
	xlist pDeferred;
	xlist pCommitted;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoArbPolicy tPolicy;
	uint32 iActiveBallots;
	uint32 iCommitIndex;
	char sLocalNode[32];
	char sCheckpointPath[260];
	char sCommitPath[260];
} DemoArbCenter;

typedef struct
{
	char sStage[32];
	uint32 iTerm;
	bool bGranted;
	int iDelayMs;
	int iStatus;
} DemoPrepareTask;

typedef struct
{
	char sPath[260];
	char sState[32];
	char sKey[64];
	char sLeader[32];
	uint32 iTerm;
	uint32 iCommitIndex;
	uint32 iAcceptedVotes;
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

static bool procSaveConsensusCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sLeader,
	uint32 iTerm,
	uint32 iCommitIndex,
	uint32 iAcceptedVotes,
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
	xvoTableSetInt(vRoot, "commit_index", 0, (int32)iCommitIndex);
	xvoTableSetInt(vRoot, "accepted_votes", 0, (int32)iAcceptedVotes);
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

static xvalue procLoadConsensusCheckpoint(const char* sCheckpointPath)
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

static int32 procLocalPrepareTask(ptr pArg, xfuture_result* pOut)
{
	DemoPrepareTask* pTask = (DemoPrepareTask*)pArg;

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

	pTask->iStatus = procSaveConsensusCheckpoint(
		pTask->sPath,
		pTask->sState,
		pTask->sKey,
		pTask->sLeader,
		pTask->iTerm,
		pTask->iCommitIndex,
		pTask->iAcceptedVotes,
		pTask->iMajorityDone) ? XRT_NET_OK : XRT_NET_ERROR;

	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static bool procPreparePeerVoteRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	const char* sCandidate,
	uint32 iTerm,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[192];
	char sTerm[32];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) || (sCandidate == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/ballot/%s?term=%u", sKey, (unsigned)iTerm);
	if ( !xrtUrlResolve(&tBase, sRef, sURL, iURLCap, NULL) ) {
		return false;
	}
	if ( !xrtUrlMakeHostHeader(&tBase, sHostHeader, iHostCap) ) {
		return false;
	}

	snprintf(sTerm, sizeof(sTerm), "%u", (unsigned)iTerm);
	xrtHttpRequestInit(pReq);
	if ( !xrtHttpRequestSetMethod(pReq, "POST") ) {
		return false;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return false;
	}
	(void)xrtHttpRequestSetHeader(pReq, "Host", sHostHeader);
	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	(void)xrtHttpRequestSetHeader(pReq, "X-Ballot-Key", sKey);
	(void)xrtHttpRequestSetHeader(pReq, "X-Ballot-Candidate", sCandidate);
	(void)xrtHttpRequestSetHeader(pReq, "X-Ballot-Term", sTerm);
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

static const char* procChooseArbPlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoArbCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (uint32)(iSelectedPeerCount + 1) < pCenter->tPolicy.iQuorumCount ) {
		return "defer_quorum_not_met";
	}
	return "arbitrate_now";
}

static bool procRunConsensusArbitration(DemoArbCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoPrepareTask tPrepare;
	DemoCheckpointTask tCheckpointStart;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	char arrURL[DEMO_MAX_PEER][512];
	char arrHost[DEMO_MAX_PEER][320];
	char sCommitPath[260];
	const char* sCommitJson = "{\n\t\"state\": \"committed\"\n}\n";
	uint32 iGrantedVotes = 0u;
	uint32 iTerm = 0u;
	const char* sFinalState = "ballot_rejected";
	int iChildCount = 0;
	int i;
	bool bCommitted = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tPrepare, 0, sizeof(tPrepare));
	memset(&tCheckpointStart, 0, sizeof(tCheckpointStart));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	iTerm = pCenter->tPolicy.iNextTerm;

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

	procCopyText(tPrepare.sStage, sizeof(tPrepare.sStage), "prepare_ballot");
	tPrepare.iTerm = iTerm;
	tPrepare.bGranted = true;
	tPrepare.iDelayMs = 40;
	tPrepare.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalPrepareTask, &tPrepare, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tCheckpointStart.sPath, sizeof(tCheckpointStart.sPath), pCenter->sCheckpointPath);
	procCopyText(tCheckpointStart.sState, sizeof(tCheckpointStart.sState), "ballot_opened");
	procCopyText(tCheckpointStart.sKey, sizeof(tCheckpointStart.sKey), sKey);
	procCopyText(tCheckpointStart.sLeader, sizeof(tCheckpointStart.sLeader), pCenter->sLocalNode);
	tCheckpointStart.iTerm = iTerm;
	tCheckpointStart.iCommitIndex = pCenter->iCommitIndex;
	tCheckpointStart.iAcceptedVotes = 0u;
	tCheckpointStart.iMajorityDone = 0u;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procCheckpointTask, &tCheckpointStart, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPreparePeerVoteRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHost[i],
			sizeof(arrHost[i]),
			&arrPeer[i],
			sKey,
			pCenter->sLocalNode,
			iTerm,
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
		printf("%s -> arbitration_timeout\n", sKey);
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	if ( tPrepare.bGranted && (tPrepare.iStatus == XRT_NET_OK) ) {
		iGrantedVotes++;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrChild[i + 2]);
		if ( (arrResp[i] != NULL) && (arrResp[i]->iStatusCode >= 200u) && (arrResp[i]->iStatusCode < 300u) ) {
			iGrantedVotes++;
		}
		printf("peer[%d]=%s term=%u vote_status=%u\n",
			i,
			arrPeer[i].sName,
			(unsigned)iTerm,
			(unsigned)((arrResp[i] != NULL) ? arrResp[i]->iStatusCode : 0u));
	}

	if ( iGrantedVotes >= pCenter->tPolicy.iQuorumCount ) {
		procBuildPath(sCommitPath, sizeof(sCommitPath), "committed", sKey, ".json");
		pPublish = xrtFileWriteAllAsync((str)sCommitPath, (str)sCommitJson, strlen(sCommitJson), XRT_CP_UTF8);
		if ( (pPublish != NULL) && xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
			pCenter->iCommitIndex++;
			procCopyText(pCenter->sCommitPath, sizeof(pCenter->sCommitPath), sCommitPath);
			sFinalState = "committed";
			bCommitted = true;
		} else {
			sFinalState = "commit_publish_timeout";
		}
	}

	(void)procSaveConsensusCheckpoint(
		pCenter->sCheckpointPath,
		sFinalState,
		sKey,
		pCenter->sLocalNode,
		iTerm,
		pCenter->iCommitIndex,
		iGrantedVotes,
		(iGrantedVotes >= pCenter->tPolicy.iQuorumCount) ? 1u : 0u);

	printf("%s -> checkpoint=%s term=%u votes=%u/%u commit_index=%u committed=%s\n",
		sKey,
		pCenter->sCheckpointPath,
		(unsigned)iTerm,
		(unsigned)iGrantedVotes,
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
	DemoArbCenter tCenter;
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
	tCenter.tPolicy.iNextTerm = 7u;
	tCenter.iCommitIndex = 12u;
	procCopyText(tCenter.sLocalNode, sizeof(tCenter.sLocalNode), "node-a");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/consensus-checkpoint.json");
	procBuildPath(tCenter.sCommitPath, sizeof(tCenter.sCommitPath), "committed", "consensus", ".json");

	(void)procSaveConsensusCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"none",
		tCenter.sLocalNode,
		tCenter.tPolicy.iNextTerm - 1u,
		tCenter.iCommitIndex,
		0u,
		0u);

	vBoot = procLoadConsensusCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: state=%s leader=%s term=%d commit_index=%d accepted_votes=%d\n",
			xvoTableGetText(vBoot, "state", 0),
			xvoTableGetText(vBoot, "leader", 0),
			(int)xvoTableGetInt(vBoot, "term", 0),
			(int)xvoTableGetInt(vBoot, "commit_index", 0),
			(int)xvoTableGetInt(vBoot, "accepted_votes", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = tNow - 120;
	tBetaWarm.iKeyHash = 7001u;
	tBetaWarm.iHeat = 16;
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
	arrPeer[1].iLatencyMs = 95u;
	arrPeer[1].iPriority = 2u;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "https://peer-c.example.com/");
	arrPeer[2].bHealthy = false;
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChooseArbPlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseArbPlan(NULL, iSelected, &tCenter, tNow);
	if ( strcmp(sPlan, "arbitrate_now") == 0 ) {
		tCenter.iActiveBallots = 1u;
		if ( procRunConsensusArbitration(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveBallots = 0u;
		}
	}

	arrPeer[1].bHealthy = false;
	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseArbPlan(NULL, iSelected, &tCenter, tNow);
	if ( strncmp(sPlan, "defer_", 6) == 0 ) {
		(void)procSaveConsensusCheckpoint(
			tCenter.sCheckpointPath,
			"defer_quorum_not_met",
			"gamma",
			tCenter.sLocalNode,
			tCenter.tPolicy.iNextTerm,
			tCenter.iCommitIndex,
			2u,
			0u);
		printf("gamma -> %s\n", sPlan);
	}

	printf("checkpoint=%s commit=%s\n", tCenter.sCheckpointPath, tCenter.sCommitPath);
	return 0;
}
```


## 6. 一致性仲裁里真正要稳定下来的边界

### 6.1 `term / accepted_votes / commit_index` 必须是正式 checkpoint 字段

这页真正稳定下来的，不是“某次请求里成功了几个 peer”，而是：

- 当前 checkpoint 属于哪个 `term`
- 当前 ballot 一共拿到了多少 `accepted_votes`
- 当前 `commit_index` 是多少

如果这些字段不进入正式 checkpoint，重启之后系统就无法区分：

- 上次只是短暂 quorum
- 还是已经完成了正式 commit


### 6.2 本地 prepare vote 不是装饰，它是多数派的一部分

这页最容易写错的一点是：

- 只数远端 granted vote，不数本地 prepare vote

但更稳的模型应该是：

- 本地 prepare 先进入正式 child
- 它成功之后，本地 vote 也进入 `accepted_votes`
- 远端 vote 再和它一起决定 majority


### 6.3 `committed snapshot` 和 `consensus checkpoint` 仍然不是同一份文件

这页里也很容易写混：

- 以为 `runtime/committed/<hash>.json` 就能替代 checkpoint

但更稳的边界应该是：

- committed snapshot
	- 回答“这次正式 commit 产出了什么”
- consensus checkpoint
	- 回答“当前仲裁状态走到了哪一步”


### 6.4 majority 达成之前，不应提前推进 commit index

更稳的做法应该是：

- ballot 打开时只写 `ballot_opened`
- 只有本地 vote + 远端 vote 达到 majority 后，才允许推进 `commit_index`
- publish 超时或失败时，最多记成 `commit_publish_timeout`，不能假装已经正式 commit


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要正式 leader 选举和租约续期，应继续补租约与故障切换层
- 如果需要复制的是日志条目而不是单个状态快照，应继续补共识日志复制主线
- 如果需要真正的状态机提交与回滚，应继续补 WAL、日志索引和应用层 state machine


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `term / commit_index / majority_done / accepted_votes`
2. `POST /api/hydrate`
	- 只提交恢复或裁决意图，让 worker 决定是不是开新 ballot
3. `GET /api/consensus`
	- 直接返回最近一次 arbitration checkpoint
4. `POST /api/sweep`
	- 手工或定时触发超时 ballot 清理与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 current term、recent ballot history、recent defer history 和 commit 时间线


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个持久化协调面板](persistent-coordination-dashboard.md)
	先对比“checkpoint 持久化”与“正式多数派裁决”到底差在哪
2. [把本地控制台服务升级成一个分布式编排面板](distributed-orchestration-dashboard.md)
	回看 multi-peer fan-out / quorum 为什么只是仲裁的下层基础块
3. [把本地控制台服务升级成一个共识日志复制面板](consensus-log-replication-dashboard.md)
	把单次状态裁决继续升级成“entry 复制、majority commit、checkpoint 更新”的正式日志主线
