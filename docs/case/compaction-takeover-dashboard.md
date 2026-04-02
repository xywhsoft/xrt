# 把本地控制台服务升级成一个 compaction 接管面板

> 这页要解决的不是“新 leader 已经恢复完成，所以剩下的 compaction 让它继续跑完”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `owner / role / replay_cursor / snapshot_index / truncated_to / compaction_cursor`，并且上一页已经能把 `remote handoff / local replay / compaction resume / publish` 收成一条正式恢复链之后，又开始需要把“未完成 compaction 的 cursor continuation 到底推进到哪了”“如果 takeover 时远端 peer 还持有更完整的 compaction summary，本地应该怎样决定接手边界”“为什么 `leader_recovered` 不能直接等同于 `compaction takeover 已收口`”“多个 contender 都看见同一段 unfinished compaction 时，为什么仍然要先把 `cursor inspect / remote fetch / segment truncate / publish checkpoint` 收进同一个 takeover scope”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + xhttpd + template` 串成一条正式主线，而不是继续让系统在 handoff 之后靠“谁先拿到 CPU 谁把 compaction 补完”。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- lease 过期之后怎样正式探测 peer 并 takeover
- 新 leader takeover 之后怎样把 remote handoff、local replay、compaction resume 收成一条恢复链
- `owner / role`、`replay_cursor`、`snapshot_index / truncated_to` 为什么都必须进入正式 checkpoint

但真实系统再往前走一步，很快又会出现一个新的问题：

- 新 leader 可能只把 replay gap 补齐了，unfinished compaction 还停在某个 `compaction_cursor`
- 远端 peer 可能还持有更完整的 compaction summary
- 本地 `snapshot_index` 和 `truncated_to` 之间仍然留着一段正式未收口的 gap
- takeover 成功之后，如果没有专门的 compaction takeover 主线，系统仍然会回到“看谁顺手把尾巴补完”

如果这时还把实现停在：

- `if ( role == leader_active ) continue compaction;`
- `if ( remote says newer ) overwrite local cursor;`
- `if ( delete old segments succeeds once ) consider takeover finished;`

很快就会出现几个典型问题：

- dashboard 上看到 leader 已恢复，但 `compaction_cursor` 仍然卡在旧值
- 远端 peer 的较新摘要被本地旧 checkpoint 覆盖，导致 cursor 回退
- 一部分 segment 已删，一部分还留着，`truncated_to` 却已经被提前推进
- publish 文件和 checkpoint 各自描述不同的 takeover 状态

所以这页真正要补出的，不是“compaction 继续跑一下”，而是：

> 当新 leader 已经正式接管恢复链之后，怎样把 unfinished compaction 的 cursor inspect、remote summary merge、segment truncate、published summary 和 checkpoint 更新做成一条可恢复、可解释、可导出的正式 compaction takeover 主线。


## 2. 为什么这次不能只靠“leader 恢复后继续跑 compaction”

### 2.1 `leader_recovered` 只回答“恢复链已经接上”

上一页的领导切换恢复已经很好地解决了：

- remote handoff 怎样进入正式 future 主线
- replay gap 怎样补到 `replay_to`
- `snapshot_index / truncated_to` 怎样重新进入恢复 checkpoint

但它不回答：

- unfinished compaction 当前真正推进到了哪一个 `compaction_cursor`
- 远端 peer 的 compaction summary 到底应不应该覆盖本地 cursor
- 哪些 segment 已经可以正式 truncate，哪些还不能动


### 2.2 `compaction_cursor` 不是一个旁注，而是正式 takeover 状态

到了这一层，系统真正要稳定回答的是：

- 当前 unfinished compaction 的 cursor 在哪里
- 当前 `snapshot_index / truncated_to` 之间到底还差多少
- 当前 remote summary 有没有被正式 merge 进本次 takeover scope
- 当前 publish 文件回答的是“已经 takeover 完成”还是“还在继续 truncate”

这说明：

- `leader_recovered` 只是恢复链状态
- `compaction_takeover_checkpoint` 才是 takeover 状态


### 2.3 这次真正新增的是“compaction continuation 状态层”

更稳的分工方式是：

- `leader handoff recovery`
	- 负责把新 leader 的恢复链正式接起来
- `compaction takeover`
	- 负责 unfinished compaction 的 cursor continuation、segment truncate 和 publish
- `ownership arbitration`
	- 再往下负责多 contender 竞争接管 unfinished work 的仲裁

一句话记住：

> 上一页补的是“新 leader 怎样恢复起来”，这一页补的是“恢复起来之后，unfinished compaction 怎样被正式接管并收口”。


## 3. 这条 compaction 接管主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active takeover registry | 当前有哪些 key 正在做 compaction takeover |
| `list` | recent takeover history | 页面和 JSON 展示最近 cursor continuation 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 takeover |
| `queue + thread` | 后台消费 `TAKEOVER / SWEEP` | 请求线程不阻塞在 cursor inspect 和 segment truncate 上 |
| `task group` | 管住一次 takeover scope 里的 inspect / remote fetch / truncate / publish child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 remote summary、segment truncate、publish 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 拉取远端 compaction summary | 让 remote inspect 进入正式 future 主线 |
| `file + path` | 装载 checkpoint、读取/删除 segment、写本地 cursor 文件 | 让 takeover 进入正式文件主线 |
| `file_async` | 把 published takeover summary 异步发布成对外可读状态文件 | 发布仍然走正式 future 主线 |
| `value + json` | 构造和解析 takeover checkpoint、remote summary、publish JSON | 让 `compaction_cursor / truncated_to / target_truncated_to` 有正式结构 |
| `time` | 记录 takeover 启动、join、truncate、publish 和完成时间 | 页面和策略使用正式时间边界 |
| `xhttpd + template` | 展示当前 `compaction_cursor / truncated_to / target_truncated_to / owner` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `leader handoff recovery` 管“leader 自己恢复起来”，`compaction takeover` 管“unfinished compaction 怎样被正式接手并收口”。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成 compaction takeover 语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/compaction-takeover-checkpoint.json
runtime/compaction-segment/<index>.json
runtime/compaction-takeover-published.json
runtime/remote-compaction-summary.json
```

其中：

- `config/console.json`
	- 保存 `takeover_timeout_ms`、`publish_timeout_ms`、`max_peer_latency_ms`
- `web/dashboard.html`
	- 同时展示当前 `owner / compaction_cursor / truncated_to / target_truncated_to`
- `runtime/console.log`
	- 记录 takeover 启动、remote summary merge、segment truncate 和 publish
- `runtime/compaction-takeover-checkpoint.json`
	- 保存最近一次正式 compaction takeover 状态
- `runtime/compaction-segment/<index>.json`
	- 保存当前仍待处理的 compaction segment
- `runtime/compaction-takeover-published.json`
	- 保存异步发布后的 takeover 输出
- `runtime/remote-compaction-summary.json`
	- 保存这次 remote summary 的本地 stage 文件


## 5. 先把“启动装载 takeover checkpoint -> inspect cursor -> remote fetch -> truncate segments -> publish”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 `compaction-takeover-checkpoint.json`
2. checkpoint 里明确记录 `owner / snapshot_index / truncated_to / compaction_cursor / target_truncated_to`
3. `dict` 表示当前 active takeover registry
4. `list` 表示 takeover / defer histories
5. `queue + thread` 的边界先体现在“请求线程只提交意图”
6. `task group` 先管住本次 takeover scope
7. remote compaction summary 走 `xhttp future`
8. local cursor inspect 和 segment truncate 先各用 child 表达
9. publish 仍然走 `file_async future`
10. join 完成之后再统一把 checkpoint 更新成 `compaction_taken_over` 或 `compaction_takeover_incomplete`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`snapshot_index = 46`、`truncated_to = 44`、`compaction_cursor = 48`
- 本地还留着 `45 -> 48` 这 4 个待处理 segment
- worker 会先从 `peer-a` 拉一份 remote compaction summary
- 再把 cursor inspect、segment truncate 和 publish 收进同一个 takeover scope
- 最后异步发布 `runtime/compaction-takeover-published.json`

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
	uint32 iTakeoverTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iMaxPeerLatencyMs;
} DemoTakeoverPolicy;

typedef struct
{
	xdict pActive;
	xlist pTakeoverHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoTakeoverPolicy tPolicy;
	uint32 iActiveTakeover;
	uint32 iTerm;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	uint32 iCompactionCursor;
	uint32 iTargetTruncatedTo;
	char sOwner[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
	char sRemoteSummaryPath[260];
} DemoTakeoverCenter;

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

static void procBuildSegmentPath(char* sDst, size_t iCap, uint32 iIndex)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "runtime/compaction-segment/%08u.json", (unsigned)iIndex);
}

static bool procSaveTakeoverCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sOwner,
	uint32 iTerm,
	uint32 iSnapshotIndex,
	uint32 iTruncatedTo,
	uint32 iCompactionCursor,
	uint32 iTargetTruncatedTo,
	uint32 iTakeoverDone)
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
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "truncated_to", 0, (int32)iTruncatedTo);
	xvoTableSetInt(vRoot, "compaction_cursor", 0, (int32)iCompactionCursor);
	xvoTableSetInt(vRoot, "target_truncated_to", 0, (int32)iTargetTruncatedTo);
	xvoTableSetInt(vRoot, "takeover_done", 0, (int32)iTakeoverDone);
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

static xvalue procLoadTakeoverCheckpoint(const char* sCheckpointPath)
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

static bool procWriteSegmentFile(uint32 iIndex, const char* sKey)
{
	xvalue vRoot = NULL;
	str sJson = NULL;
	str sDir = NULL;
	char sPath[260];
	size_t iJsonSize = 0;
	bool bOk = false;

	procBuildSegmentPath(sPath, sizeof(sPath), iIndex);

	vRoot = xvoCreateTable();
	if ( vRoot == NULL ) {
		return false;
	}

	xvoTableSetText(vRoot, "state", 0, (uint8*)"segment_open", 0, FALSE);
	xvoTableSetText(vRoot, "key", 0, (uint8*)((sKey != NULL) ? sKey : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "segment_index", 0, (int32)iIndex);
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

static bool procPrepareTakeoverRequest(
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

	snprintf(sRef, sizeof(sRef), "/api/compaction-summary/%s", sKey);
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
	(void)xrtHttpRequestSetHeader(pReq, "X-Compaction-Key", sKey);
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

static const char* procChooseTakeoverPlan(
	const DemoWarmItem* pWarm,
	const DemoPeerNode* pPeer,
	const DemoTakeoverCenter* pCenter,
	xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( (pCenter == NULL) || (pCenter->iActiveTakeover > 0u) ) {
		return "defer_takeover_busy";
	}
	if ( pPeer == NULL ) {
		return "defer_no_healthy_peer";
	}
	if ( pCenter->iCompactionCursor >= pCenter->iTargetTruncatedTo ) {
		return "already_taken_over";
	}
	return "takeover_now";
}

static bool procRunCompactionTakeover(DemoTakeoverCenter* pCenter, const DemoPeerNode* pPeer, const char* sKey)
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
	DemoStageTask tInspect;
	DemoStageTask tTruncate;
	xvalue vRemote = NULL;
	char sURL[512];
	char sHostHeader[320];
	char sSegmentPath[260];
	const char* sPublishJson =
		"{\n"
		"\t\"state\": \"compaction-taken-over\",\n"
		"\t\"publish\": \"cursor\"\n"
		"}\n";
	int iChildCount = 0;
	int i;
	bool bFinished = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(&tReq, 0, sizeof(tReq));
	memset(&tInspect, 0, sizeof(tInspect));
	memset(&tTruncate, 0, sizeof(tTruncate));

	if ( (pCenter == NULL) || (pPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

	(void)procSaveTakeoverCheckpoint(
		pCenter->sCheckpointPath,
		"compaction_takeover_opened",
		sKey,
		pCenter->sOwner,
		pCenter->iTerm,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		pCenter->iCompactionCursor,
		pCenter->iTargetTruncatedTo,
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

	procCopyText(tInspect.sStage, sizeof(tInspect.sStage), "inspect_cursor");
	procBuildSegmentPath(tInspect.sPath, sizeof(tInspect.sPath), pCenter->iCompactionCursor);
	tInspect.iDelayMs = 30u;
	tInspect.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tInspect, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procBuildSegmentPath(sSegmentPath, sizeof(sSegmentPath), pCenter->iTruncatedTo + 1u);
	procCopyText(tTruncate.sStage, sizeof(tTruncate.sStage), "truncate_segment");
	procCopyText(tTruncate.sPath, sizeof(tTruncate.sPath), sSegmentPath);
	tTruncate.iDelayMs = 50u;
	tTruncate.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procStageTask, &tTruncate, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	if ( !procPrepareTakeoverRequest(
		&tReq,
		sURL,
		sizeof(sURL),
		sHostHeader,
		sizeof(sHostHeader),
		pPeer,
		sKey,
		pCenter->tPolicy.iTakeoverTimeoutMs) ) {
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
			int32 iRemoteCursor = xvoTableGetInt(vRemote, "remote_compaction_cursor", 0);
			if ( iRemoteCursor > (int32)pCenter->iCompactionCursor ) {
				pCenter->iCompactionCursor = (uint32)iRemoteCursor;
			}
		}
	}

	if ( xrtPathExists((str)sSegmentPath) ) {
		(void)xrtFileDelete((str)sSegmentPath);
	}

	pCenter->iCompactionCursor = pCenter->iTargetTruncatedTo;
	pCenter->iTruncatedTo = pCenter->iTargetTruncatedTo;
	bFinished = true;

cleanup:
	(void)procSaveTakeoverCheckpoint(
		pCenter->sCheckpointPath,
		bFinished ? "compaction_taken_over" : "compaction_takeover_incomplete",
		sKey,
		pCenter->sOwner,
		pCenter->iTerm,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		pCenter->iCompactionCursor,
		pCenter->iTargetTruncatedTo,
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
	DemoTakeoverCenter tCenter;
	DemoPeerNode arrPeer[DEMO_MAX_PEER];
	DemoWarmItem tBetaWarm;
	xvalue vBoot = NULL;
	const DemoPeerNode* pPeer = NULL;
	const char* sPlan = NULL;
	uint32 iIndex = 0u;

	memset(&tCenter, 0, sizeof(tCenter));
	memset(arrPeer, 0, sizeof(arrPeer));
	memset(&tBetaWarm, 0, sizeof(tBetaWarm));

	tCenter.tPolicy.iTakeoverTimeoutMs = 5000u;
	tCenter.tPolicy.iPublishTimeoutMs = 5000u;
	tCenter.tPolicy.iMaxPeerLatencyMs = 80u;
	tCenter.iTerm = 18u;
	tCenter.iSnapshotIndex = 46u;
	tCenter.iTruncatedTo = 44u;
	tCenter.iCompactionCursor = 48u;
	tCenter.iTargetTruncatedTo = 52u;
	procCopyText(tCenter.sOwner, sizeof(tCenter.sOwner), "node-b");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/compaction-takeover-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/compaction-takeover-published.json");
	procCopyText(tCenter.sRemoteSummaryPath, sizeof(tCenter.sRemoteSummaryPath), "runtime/remote-compaction-summary.json");

	(void)procSaveTakeoverCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sOwner,
		tCenter.iTerm,
		tCenter.iSnapshotIndex,
		tCenter.iTruncatedTo,
		tCenter.iCompactionCursor,
		tCenter.iTargetTruncatedTo,
		0u);

	for ( iIndex = 45u; iIndex <= 48u; iIndex++ ) {
		(void)procWriteSegmentFile(iIndex, "theta");
	}

	procCopyText(arrPeer[0].sName, sizeof(arrPeer[0].sName), "peer-a");
	procCopyText(arrPeer[0].sBaseURL, sizeof(arrPeer[0].sBaseURL), "http://127.0.0.1:19081/");
	arrPeer[0].iLatencyMs = 24u;
	arrPeer[0].iPriority = 1;
	arrPeer[0].bHealthy = true;

	procCopyText(arrPeer[1].sName, sizeof(arrPeer[1].sName), "peer-b");
	procCopyText(arrPeer[1].sBaseURL, sizeof(arrPeer[1].sBaseURL), "http://127.0.0.1:19082/");
	arrPeer[1].iLatencyMs = 58u;
	arrPeer[1].iPriority = 2;
	arrPeer[1].bHealthy = true;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "http://127.0.0.1:19083/");
	arrPeer[2].iLatencyMs = 120u;
	arrPeer[2].iPriority = 3;
	arrPeer[2].bHealthy = false;

	vBoot = procLoadTakeoverCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: owner=%s snapshot=%d truncated=%d cursor=%d target=%d\n",
			xvoTableGetText(vBoot, "owner", 0),
			(int)xvoTableGetInt(vBoot, "snapshot_index", 0),
			(int)xvoTableGetInt(vBoot, "truncated_to", 0),
			(int)xvoTableGetInt(vBoot, "compaction_cursor", 0),
			(int)xvoTableGetInt(vBoot, "target_truncated_to", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = xrtNow() - 60;
	tBetaWarm.iKeyHash = xrtHash64((ptr)"beta", 4u);
	tBetaWarm.iHeat = 6;
	procCopyText(tBetaWarm.sKey, sizeof(tBetaWarm.sKey), "beta");
	procCopyText(tBetaWarm.sTitle, sizeof(tBetaWarm.sTitle), "beta warm shadow");

	if ( (tBetaWarm.iHeat > 0) && ((xrtNow() - tBetaWarm.tTouchedAt) <= 1200) ) {
		printf("%s -> local_shadow\n", tBetaWarm.sKey);
	}

	pPeer = procChoosePeer(arrPeer, DEMO_MAX_PEER, tCenter.tPolicy.iMaxPeerLatencyMs);
	sPlan = procChooseTakeoverPlan(NULL, pPeer, &tCenter, xrtNow());
	printf("theta -> %s\n", sPlan);
	if ( (strcmp(sPlan, "takeover_now") == 0) && (pPeer != NULL) ) {
		tCenter.iActiveTakeover = 1u;
		if ( procRunCompactionTakeover(&tCenter, pPeer, "theta") ) {
			tCenter.iActiveTakeover = 0u;
		}
	}

	printf("checkpoint=%s published=%s cursor=%u truncated_to=%u target=%u owner=%s\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		(unsigned)tCenter.iCompactionCursor,
		(unsigned)tCenter.iTruncatedTo,
		(unsigned)tCenter.iTargetTruncatedTo,
		tCenter.sOwner);
	return 0;
}
```


## 6. compaction 接管里真正要稳定下来的边界

### 6.1 `compaction_cursor / truncated_to / target_truncated_to` 必须进入正式 checkpoint

这页真正稳定下来的，不是“旧 segment 看起来少了一些”，而是：

- 当前 compaction continuation 到底推进到了哪里
- 当前 `truncated_to` 到底有没有正式追上目标
- 当前 published takeover summary 回答的是“已完成”还是“仍未收口”

如果这些字段不进入正式 checkpoint，下次启动就无法稳定继续接手 unfinished compaction。


### 6.2 remote summary、cursor inspect、segment truncate 必须进入同一个 takeover scope

这页最容易写错的一点是：

- 先在一个线程里 inspect cursor
- 再在另一个线程里删 segment
- 最后另一个模块顺手发布 published summary

但更稳的模型应该是：

- 先建立同一个 `task group`
- 再把 remote summary、cursor inspect、segment truncate、publish 都挂进同一个 scope
- 最后只让 join 结果决定这次 takeover 是否正式完成

否则系统会出现“本地删了一部分、远端也说自己更完整、published 文件先发了”的拆裂状态，却没人真正回答“接管是否已经收口”。


### 6.3 published takeover 文件和 compaction checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published takeover 文件已经足够回答当前 compaction 接管状态

但更稳的边界应该是：

- `runtime/compaction-takeover-published.json`
	- 回答“这次接管对外发布了什么”
- `runtime/compaction-takeover-checkpoint.json`
	- 回答“cursor continuation 和 truncate 当前走到了哪一步”


### 6.4 这页先停在“unfinished compaction 已被正式接管”，不提前把它说成“ownership arbitration 已经讲完”

这页故意把边界停在：

- remote compaction summary 已经 join
- local cursor inspect 已经完成
- `truncated_to` 已经推进到目标
- published summary 已经异步写出

但它还没有回答：

- 多个 contender 同时争抢 unfinished compaction 时怎样裁决
- ownership arbitration 和 compaction takeover 怎样统一成更重的一条主线
- 更长链路的 distributed takeover 怎样防止 cursor 回退

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要多个 contender 正式竞争 unfinished compaction，应继续补 ownership arbitration 主线
- 如果需要跨节点多阶段接管与回退，应继续补 distributed takeover orchestration 主线
- 如果需要把 compaction continuation 和 leader arbitration 统一成一条仲裁链，应继续补 consensus-style ownership 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `owner / compaction_cursor / truncated_to / target_truncated_to`
2. `POST /api/compaction-takeover`
	- 只提交 takeover 意图，让 worker 决定是否进入 cursor inspect / remote summary / truncate 主线
3. `GET /api/compaction-checkpoint`
	- 直接返回最近一次 compaction takeover checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 takeover 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 cursor、truncate 边界、published summary 和 recent takeover history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个领导切换恢复面板](leader-handoff-recovery-dashboard.md)
	先对比“leader 恢复链已接上”与“unfinished compaction 已被正式接手”到底差在哪
2. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	回看 `snapshot_index / truncated_to / compaction_cursor` 为什么仍然是 takeover 的底层边界
3. [把本地控制台服务升级成一个 ownership 仲裁面板](ownership-arbitration-dashboard.md)
	把多 contender 竞争 unfinished work 的仲裁、回退和统一收口主线正式补出来
