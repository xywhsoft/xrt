# 把本地控制台服务升级成一个 ownership 仲裁面板

> 这页要解决的不是“多个 contender 都想接 unfinished work，那就谁先拿到锁谁上”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `owner / role / term / compaction_cursor / truncated_to / target_truncated_to`，并且上一页已经能把 unfinished compaction 的 cursor continuation 正式接管之后，又开始需要把“本地 claim 和远端 claim 到底怎么比较”“为什么 `leader_active` 和 `compaction_taken_over` 还不够回答 ownership 问题”“如果多个 contender 同时看见同一段 unfinished work，为什么 `local inspect / remote fetch / winner decide / publish` 仍然要进入同一个 arbitration scope”“为什么 published winner 文件和 arbitration checkpoint 不能混成一份状态”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在竞争接管时靠“谁先执行完谁就是 owner”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- lease 过期之后怎样正式探测 peer 并 takeover
- 新 leader takeover 之后怎样把恢复链正式接上
- unfinished compaction 怎样通过 cursor continuation 被正式接管

但真实系统再往前走一步，很快又会出现一个新的问题：

- 多个 contender 可能同时看见同一段 unfinished work
- 每个 contender 都可能持有一份“看起来合理”的本地 claim
- 远端 peer 还可能带来更高 term、更高 priority 或更完整 cursor 的 claim summary
- 如果没有正式仲裁层，系统仍然会回到“谁先执行完谁就算赢”

如果这时还把实现停在：

- `if ( local_priority >= remote_priority ) owner = local;`
- `if ( remote term is newer ) trust remote immediately;`
- `if ( publish winner succeeded ) consider arbitration done;`

很快就会出现几个典型问题：

- checkpoint 里记录自己赢了，但 published winner 文件还是上一次的 owner
- 本地 claim 和远端 claim 分别在不同线程里改状态，没人真正回答“这轮到底谁赢”
- `term` 比较、`priority` 比较、`cursor` 完整度比较被散落在不同代码路径
- 下次启动时只能看见 winner，已经看不清当时的仲裁依据

所以这页真正要补出的，不是“再加一个 `if/else`”，而是：

> 当多个 contender 同时争抢同一段 unfinished work 时，怎样把本地 claim、远端 claim、winner 决策、published winner 和 checkpoint 更新做成一条可恢复、可解释、可导出的正式 ownership 仲裁主线。


## 2. 为什么这次不能只靠“谁先接管成功谁就是 owner”

### 2.1 `compaction_taken_over` 只回答“某个 contender 已经继续跑了”

上一页的 compaction 接管已经很好地解决了：

- unfinished compaction 的 cursor continuation 应该怎么接
- 远端 compaction summary 怎样进入正式 future 主线
- `compaction_cursor / truncated_to / target_truncated_to` 怎样进入正式 checkpoint

但它不回答：

- 如果两个 contender 都做到了这一层，谁才是真正的 winner
- 本地 claim 和远端 claim 的 term / priority / cursor 应该怎样比较
- published winner 和 checkpoint 应该在什么时候一起更新


### 2.2 仲裁结果不是一个副作用，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前这轮 arbitration 的 `term / round / local_priority / remote_priority` 分别是多少
- 当前 winner 是谁，依据是什么
- 当前 winner 文件是不是和 checkpoint 描述的是同一轮仲裁
- 当前 loser 是 defer 了，还是应该继续回退重试

这说明：

- `owner` 只是结果
- `ownership_arbitration_checkpoint` 才是过程状态


### 2.3 这次真正新增的是“winner 决策状态层”

更稳的分工方式是：

- `compaction takeover`
	- 负责 unfinished work 怎样被接手
- `ownership arbitration`
	- 负责多个 contender 的 claim 怎样比较、winner 怎样发布
- `distributed takeover orchestration`
	- 再往下负责跨更多节点的多阶段接管编排

一句话记住：

> 上一页补的是“unfinished work 怎样被接手”，这一页补的是“多个 contender 同时想接手时，谁才应该成为正式 owner”。


## 3. 这条 ownership 仲裁主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active arbitration registry | 当前有哪些 key 正在做 ownership arbitration |
| `list` | recent winner history | 页面和 JSON 展示最近 arbitration 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有成为 winner |
| `queue + thread` | 后台消费 `ARBITRATE / SWEEP` | 请求线程不阻塞在 claim 比较上 |
| `task group` | 管住一次 arbitration scope 里的 local inspect / remote fetch / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote claim、publish winner 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 claim summary | 让远端 contender 信息进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取本地 claim、写 winner checkpoint | 让仲裁进入正式文件主线 |
| `file_async` | 把 published winner 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 claim、checkpoint 与 published winner JSON | 让 `term / round / priority / winner` 有正式结构 |
| `time` | 记录 arbitration 启动、join、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `owner / winner / term / round / priority` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `compaction takeover` 管 unfinished work 怎么接，`ownership arbitration` 管多个 contender 同时想接时谁才算正式 owner。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 ownership arbitration 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/ownership-arbitration-checkpoint.json
runtime/ownership-claim/<round>.json
runtime/ownership-arbitration-published.json
runtime/remote-ownership-summary.json
```

其中：

- `config/console.json`
	- 保存 `arbitration_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `owner / winner / term / round / local_priority / remote_priority`
- `runtime/console.log`
	- 记录 arbitration 启动、remote claim join、winner publish 和 defer
- `runtime/ownership-arbitration-checkpoint.json`
	- 保存最近一次正式 ownership arbitration 状态
- `runtime/ownership-claim/<round>.json`
	- 保存当前轮次的本地 claim
- `runtime/ownership-arbitration-published.json`
	- 保存异步发布后的 winner 输出
- `runtime/remote-ownership-summary.json`
	- 保存这次 remote claim 的本地 stage 文件


## 5. 先把“启动装载 arbitration checkpoint -> inspect local claim -> remote fetch -> decide winner -> publish winner”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `ownership-arbitration-checkpoint.json`
2. checkpoint 里明确记录 `owner / winner / term / round / local_priority / remote_priority`
3. `dict` 表示当前 active arbitration registry
4. `list` 表示 winner / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 arbitration scope
7. local claim inspect 用一个 child 表达
8. remote claim 走 `xhttp future`
9. publish winner 仍然走 `file_async future`
10. join 完成之后再统一把 checkpoint 更新成 `ownership_arbitrated` 或 `ownership_arbitration_incomplete`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`owner = node-b`、`term = 18`、`round = 4`
- 本地 claim 会声明 `priority = 80`
- worker 会从 `peer-a` 拉一份 remote ownership summary
- 再把 local inspect、remote fetch 和 publish 收进同一个 arbitration scope
- 最后异步发布 `runtime/ownership-arbitration-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
	char sBaseURL[160];
	uint32 iLatencyMs;
	int iPriority;
	bool bHealthy;
} DemoPeerNode;

typedef struct
{
	uint32 iArbitrationTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoArbPolicy;

typedef struct
{
	xdict pActive;
	xlist pWinnerHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoArbPolicy tPolicy;
	uint32 iActiveArbitration;
	uint32 iTerm;
	uint32 iRound;
	uint32 iLocalPriority;
	uint32 iRemotePriority;
	char sOwner[32];
	char sWinner[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoArbCenter;

typedef struct
{
	char sStage[64];
	char sPath[260];
	uint32 iDelayMs;
	int iStatus;
} DemoStageTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static void procBuildClaimPath(char* sDst, size_t iCap, uint32 iRound)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/ownership-claim/%08u.json", (unsigned)iRound);
}

static bool procSaveArbCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sOwner,
	const char* sWinner,
	uint32 iTerm,
	uint32 iRound,
	uint32 iLocalPriority,
	uint32 iRemotePriority,
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
	xvoTableSetText(vRoot, "owner", 0, (uint8*)((sOwner != NULL) ? sOwner : ""), 0, FALSE);
	xvoTableSetText(vRoot, "winner", 0, (uint8*)((sWinner != NULL) ? sWinner : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "local_priority", 0, (int32)iLocalPriority);
	xvoTableSetInt(vRoot, "remote_priority", 0, (int32)iRemotePriority);
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

static xvalue procLoadArbCheckpoint(const char* sCheckpointPath)
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

static bool procWriteClaimFile(uint32 iRound, const char* sKey, const char* sNode, uint32 iPriority)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildClaimPath(sPath, sizeof(sPath), iRound);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"claim_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetText(vRoot, "node", 0, (uint8*)((sNode != NULL) ? sNode : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "round", 0, (int32)iRound);
	xvoTableSetInt(vRoot, "priority", 0, (int32)iPriority);
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

static bool procPrepareArbRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[192];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/ownership-claim/%s", sKey);
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

static const DemoPeerNode* procChoosePeer(const DemoPeerNode* arrPeer, int iCount, uint32 iMaxPeerLatencyMs)
{
	const DemoPeerNode* pBest = NULL;
	int i;

	for ( i = 0; i < iCount; i++ ) {
		if ( !arrPeer[i].bHealthy ) {
			continue;
		}
		if ( arrPeer[i].iLatencyMs > iMaxPeerLatencyMs ) {
			continue;
		}
		if ( (pBest == NULL) || (arrPeer[i].iPriority < pBest->iPriority) ) {
			pBest = &arrPeer[i];
		}
	}

	return pBest;
}

static const char* procChooseArbitrationPlan(
	const DemoWarmItem* pWarm,
	const DemoPeerNode* pPeer,
	const DemoArbCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveArbitration > 0u) ) {
		return "defer_arbitration_busy";
	}
	if ( pPeer == NULL ) {
		return "defer_no_healthy_peer";
	}
	return "arbitrate_now";
}

static bool procRunOwnershipArbitration(DemoArbCenter* pCenter, const DemoPeerNode* pPeer, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xfuture* pRemoteFetch = NULL;
	xfuture* pPublish = NULL;
	xfuture* pJoin = NULL;
	xhttpresponse* pResp = NULL;
	xhttprequest tReq;
	DemoStageTask tLocalInspect;
	xvalue vRemote = NULL;
	char sURL[512];
	char sHostHeader[320];
	char sClaimPath[260];
	const char* sPublishJson =
		"{\n"
		"\t\"state\": \"ownership-arbitrated\",\n"
		"\t\"winner\": \"pending\"\n"
		"}\n";
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(&tReq, 0, sizeof(tReq));
	memset(&tLocalInspect, 0, sizeof(tLocalInspect));

	if ( (pCenter == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveArbCheckpoint(
		pCenter->sCheckpointPath,
		"ownership_arbitration_opened",
		sKey,
		pCenter->sOwner,
		pCenter->sWinner,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iLocalPriority,
		pCenter->iRemotePriority,
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

	procBuildClaimPath(sClaimPath, sizeof(sClaimPath), pCenter->iRound);
	procCopyText(tLocalInspect.sStage, sizeof(tLocalInspect.sStage), "inspect_local_claim");
	procCopyText(tLocalInspect.sPath, sizeof(tLocalInspect.sPath), sClaimPath);
	tLocalInspect.iDelayMs = 20u;
	tLocalInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tLocalInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	if ( !procPrepareArbRequest(
		&tReq,
		sURL,
		sizeof(sURL),
		sHostHeader,
		sizeof(sHostHeader),
		pPeer,
		sKey,
		pCenter->tPolicy.iArbitrationTimeoutMs) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	pRemoteFetch = (xfuture*)xrtHttpExecuteAsync(pEngine, &tReq);
	if ( pRemoteFetch == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pGroup, pRemoteFetch) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	arrChild[iChildCount] = pRemoteFetch;
	iChildCount++;

	pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, (str)sPublishJson, strlen(sPublishJson), XRT_CP_UTF8);
	if ( pPublish == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pGroup, pPublish) ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	arrChild[iChildCount] = pPublish;
	iChildCount++;

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

	pResp = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)pRemoteFetch);
	if ( (pResp == NULL) || (pResp->iStatusCode >= 500u) ) {
		goto cleanup;
	}

	if ( (pResp->pBody != NULL) && (pResp->iBodyLen > 0u) ) {
		vRemote = xrtParseJSON((str)pResp->pBody, pResp->iBodyLen);
		if ( (vRemote != NULL) && !xvoIsNull(vRemote) && (xvoType(vRemote) == XVO_DT_TABLE) ) {
			int32 iRemotePriority = xvoTableGetInt(vRemote, "remote_priority", 0);
			str sRemoteOwner = xvoTableGetText(vRemote, "remote_owner", 0);
			if ( iRemotePriority > (int32)pCenter->iRemotePriority ) {
				pCenter->iRemotePriority = (uint32)iRemotePriority;
			}
			if ( pCenter->iRemotePriority > pCenter->iLocalPriority ) {
				procCopyText(pCenter->sWinner, sizeof(pCenter->sWinner), (const char*)sRemoteOwner);
				procCopyText(pCenter->sOwner, sizeof(pCenter->sOwner), (const char*)sRemoteOwner);
			}
		}
	}

	if ( pCenter->iLocalPriority >= pCenter->iRemotePriority ) {
		procCopyText(pCenter->sWinner, sizeof(pCenter->sWinner), "node-b");
		procCopyText(pCenter->sOwner, sizeof(pCenter->sOwner), "node-b");
	}

	bFinished = true;

cleanup:
	(void)procSaveArbCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "ownership_arbitrated" : "ownership_arbitration_incomplete",
		sKey,
		pCenter->sOwner,
		pCenter->sWinner,
		pCenter->iTerm,
		pCenter->iRound,
		pCenter->iLocalPriority,
		pCenter->iRemotePriority,
		bFinished ? 1u : 0u);

	if ( vRemote != NULL ) {
		xvoUnref(vRemote);
	}
	if ( pResp != NULL ) {
		xrtHttpResponseDestroy(pResp);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	for ( i = 0; i < iChildCount; i++ ) {
		if ( arrChild[i] != NULL ) {
			xFutureRelease(arrChild[i]);
		}
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}
	xrtHttpRequestUnit(&tReq);
	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}

	return bFinished;
}

int main(void)
{
	DemoArbCenter tCenter;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const DemoPeerNode* pPeer = NULL;
	const char* sPlan = NULL;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iArbitrationTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 18u;
	tCenter.iRound = 4u;
	tCenter.iLocalPriority = 80u;
	tCenter.iRemotePriority = 72u;
	procCopyText(tCenter.sOwner, sizeof(tCenter.sOwner), "node-b");
	procCopyText(tCenter.sWinner, sizeof(tCenter.sWinner), "pending");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/ownership-arbitration-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/ownership-arbitration-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-ownership-summary.json");

	(void)procSaveArbCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sOwner,
		tCenter.sWinner,
		tCenter.iTerm,
		tCenter.iRound,
		tCenter.iLocalPriority,
		tCenter.iRemotePriority,
		0u);
	(void)procWriteClaimFile(tCenter.iRound, "theta", "node-b", tCenter.iLocalPriority);

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 20u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 45u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadArbCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: owner=%s winner=%s term=%d round=%d local=%d remote=%d\n",
			xvoTableGetText(vBoot, "owner", 0),
			xvoTableGetText(vBoot, "winner", 0),
			(int)xvoTableGetInt(vBoot, "term", 0),
			(int)xvoTableGetInt(vBoot, "round", 0),
			(int)xvoTableGetInt(vBoot, "local_priority", 0),
			(int)xvoTableGetInt(vBoot, "remote_priority", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 80;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 5;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	pPeer = procChoosePeer(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs);
	sPlan = procChooseArbitrationPlan(NULL, pPeer, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( (strcmp(sPlan, "arbitrate_now") == 0) && (pPeer != NULL) ) {
		tCenter.iActiveArbitration = 1u;
		if ( procRunOwnershipArbitration(&tCenter, pPeer, "theta") ) {
			tCenter.iActiveArbitration = 0u;
		}
	}

	printf("checkpoint=%s published=%s owner=%s winner=%s local=%u remote=%u\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		tCenter.sOwner,
		tCenter.sWinner,
		(unsigned)tCenter.iLocalPriority,
		(unsigned)tCenter.iRemotePriority);
	return 0;
}
```


## 6. ownership 仲裁里真正要稳定下来的边界

### 6.1 `term / round / local_priority / remote_priority` 必须进入正式 arbitration checkpoint

这页真正稳定下来的，不是“最后 winner 看起来像是 node-b”，而是：

- 当前这轮 arbitration 的 term 和 round 是多少
- 本地 claim 和远端 claim 的 priority 分别是多少
- 当前 winner 是谁、为什么是它

如果这些字段不进入正式 checkpoint，下次启动就无法解释上一轮 winner 是怎么得出的。


### 6.2 local inspect、remote fetch、winner publish 必须进入同一个 arbitration scope

这页最容易写错的一点是：

- 先随手读本地 claim
- 再在别的线程里拉远端 summary
- publish winner 时再顺手决定 owner

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 local inspect、remote fetch、publish 都挂进同一个 scope
- 最后只让 join 结果和统一比较逻辑决定这次 arbitration 是否正式完成

否则系统会出现“本地说自己赢、远端也说自己更新、published winner 又是旧值”的拆裂状态。


### 6.3 published winner 文件和 arbitration checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published winner 文件已经足够回答当前 ownership 状态

但更稳的边界应该是：

- `runtime/ownership-arbitration-published.json`
	- 回答“这次仲裁对外发布了什么”
- `runtime/ownership-arbitration-checkpoint.json`
	- 回答“这轮 claim 比较和 winner 决策当前走到了哪一步”


### 6.4 这页先停在“winner 已被正式仲裁出来”，不提前把它说成“分布式接管编排已经讲完”

这页故意把边界停在：

- local claim 已 inspect
- remote claim 已 join
- winner 已经统一决定
- published winner 已经异步写出

但它还没有回答：

- 多节点多阶段接管时怎样做更重的编排
- loser 回退之后怎样进入下一轮 arbitration
- distributed takeover orchestration 怎样避免跨节点 winner 抖动

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要多节点多阶段接管，应继续补 distributed takeover orchestration 主线
- 如果需要更正式的仲裁日志和回退链，应继续补 consensus-style ownership 主线
- 如果需要跨轮 winner 稳定性保证，应继续补更重的 distributed arbitration 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `owner / winner / term / round / local_priority / remote_priority`
2. `POST /api/arbitrate`
	- 只提交 arbitration 意图，让 worker 决定是否进入 claim 比较主线
3. `GET /api/ownership`
	- 直接返回最近一次 ownership arbitration checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 arbitration 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 owner、winner、priority 对比、published winner 和 recent arbitration history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个 compaction 接管面板](compaction-takeover-dashboard.md)
	先对比“unfinished work 已被接手”与“多个 contender 已经正式决出 winner”到底差在哪
2. [把本地控制台服务升级成一个领导切换恢复面板](leader-handoff-recovery-dashboard.md)
	回看 `term / owner / publish` 为什么在 claim 比较之后仍然是恢复链的底层边界
3. [把本地控制台服务升级成一个分布式接管编排面板](distributed-takeover-orchestration-dashboard.md)
	把多节点、多轮次、多阶段 takeover 的协调、回退和统一收口主线正式补出来
