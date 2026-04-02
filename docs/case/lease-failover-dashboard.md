# 把本地控制台服务升级成一个租约故障切换面板

> 这页要解决的不是“某台节点看起来挂了，就换另一台节点继续跑”这么简单，而是更贴近真实服务的问题：当一个本地控制台服务已经有了 `snapshot_index / truncated_to / compaction_cursor / published snapshot` 之后，又开始需要把“当前 ownership 到底属于谁”“当前 lease 什么时候过期”“租约过期后 takeover 是不是已经得到正式多数派确认”“compaction 期间的 `snapshot_index / truncated_to` 怎样跟着 ownership 一起进入正式 checkpoint”放进同一条后台主线时，怎样把 `dict + list + list + queue + thread + task group + future + xurl + xhttp + file + file_async + path + value + json + time + hash + xhttpd + template` 串成一条正式主线，而不是继续让系统在节点异常时靠人工判断谁来接管。

[返回案例索引](README.md)

---

## 1. 场景

前面的页面已经分别讲了：

- 日志回放怎样稳定回答 `replay_from / replay_to / replay_cursor`
- 快照压缩怎样稳定回答 `snapshot_index / truncated_to / compaction_cursor`

但真实系统再往前走一步，很快又会出现一个新的问题：

- compaction 和 snapshot 已经不是一次性动作，而是长期运行主线
- 一旦当前 owner 失联，系统就必须判断 lease 是否已经正式过期
- 只有 lease 正式过期之后，另一台节点才可以接管
- takeover 发生时，当前 `snapshot_index / truncated_to` 也必须跟着一起进入正式 checkpoint

如果这时还把实现停在：

- `if ( timeout ) owner = backup;`
- `printf("take over\\n");`

很快就会出现几个典型问题：

- ownership 没有正式 lease 边界
- 节点只是短时抖动，也会被误判为 takeover
- dashboard 看不到当前 owner、lease_expires_at 和 takeover 状态
- 下次启动后根本不知道自己是不是已经正式接管

所以这页真正要补出的，不是“一个 leader 变量”，而是：

> 当 compaction 和 snapshot 已经进入长期运行主线之后，怎样把 lease、takeover、ownership 和 published failover 状态做成一条可恢复、可解释、可导出的正式租约故障切换主线。


## 2. 为什么这次不能只靠“compaction 完成后谁先继续跑”

### 2.1 快照压缩只回答“当前哪些日志已经被 snapshot 吸收”

上一页里的快照压缩已经很好地解决了：

- 当前 snapshot 覆盖到了哪里
- 当前日志已经截断到哪里
- 当前 compaction 进行到哪一步

但它不回答：

- 当前 owner 是谁
- 当前 lease 什么时候过期
- 另一台节点什么时候才有资格正式 takeover


### 2.2 ownership 不是一个内存变量，而是正式状态

到了这一层，系统真正要稳定回答的是：

- 当前 lease 的 owner 是谁
- 当前 lease 的 term 是多少
- 当前 `lease_expires_at` 是多少
- 当前 takeover 是否已经正式完成

这说明：

- ownership 不是一句日志
- 它本身就是正式恢复状态


### 2.3 这次真正新增的是“lease / failover 状态层”

更稳的分工方式是：

- `snapshot compaction`
	- 负责 snapshot 和 truncation
- `lease failover`
	- 负责 owner、lease、takeover 和 published failover 状态
- `leader handoff recovery`
	- 再决定 takeover 之后怎样继续恢复日志和 compaction 主线

一句话记住：

> 上一页补的是“snapshot 和 truncation 如何完成”，这一页补的是“这条长期运行主线在 owner 异常时怎样正式接管”。 


## 3. 这条租约故障切换主线里每层负责什么

| 层 | 角色 | 真正解决的问题 |
|----|------|----------------|
| `dict` | 当前 active takeover registry | 当前有哪些 lease failover 正在运行 |
| `list` | peer catalog | 哪些节点可参与 takeover 探测和确认 |
| `list` | recent failover history | 页面和 JSON 展示最近 lease 检查、peer probe 和 takeover 结果 |
| `list` | recent defer history | 页面和 JSON 展示为什么这次没有进入正式 takeover |
| `queue + thread` | 后台消费 `FAILOVER / SWEEP` | 请求线程不阻塞在 lease 检查和 takeover 上 |
| `task group` | 管住一次 takeover scope 里的 local check 和 peer probe child | `Close / Join / Cancel / Destroy` |
| `future` | 表达 local lease check、peer probe、published failover 的完成态 | 统一 join 和失败边界 |
| `xurl + xhttp` | 向其他节点发起 lease / health probe | 让 takeover 进入正式网络主线 |
| `file + path` | 保存和装载 lease checkpoint | 让 ownership 状态能跨重启恢复 |
| `file_async` | 把最终 failover 结果异步写成 published snapshot | published failover 仍然走正式 future 主线 |
| `value + json` | 构造和解析 lease checkpoint 与 published failover 文件 | 让 `owner / role / term / lease_expires_at / takeover_done` 有正式结构 |
| `time` | 记录 lease 到期时间、probe 时间和 takeover 完成时间 | 页面和策略使用正式时间边界 |
| `hash` | 给 key 生成稳定指纹 | 组织 published failover 路径和轻量标识 |
| `xhttpd + template` | 展示当前 `owner / role / term / lease_expires_at / snapshot_index / truncated_to` | 浏览器和脚本共享同一份状态面 |

一句话记住：

> `snapshot compaction` 管数据边界，`lease failover` 管 ownership 边界。


## 4. 文件和输出约定

沿用本地控制台服务的目录约定，这页把输出改成租约故障切换语义：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/lease-failover-checkpoint.json
runtime/failover-published.json
runtime/state-machine-snapshot.json
```

其中：

- `config/console.json`
	- 保存 `fanout_width`、`quorum_count`、`remote_timeout_ms`、`publish_timeout_ms`、`lease_window_ms`
- `web/dashboard.html`
	- 同时展示当前 `owner / role / term / lease_expires_at / snapshot_index / truncated_to`
- `runtime/console.log`
	- 记录 lease 检查、peer probe、takeover 和 defer
- `runtime/lease-failover-checkpoint.json`
	- 保存最近一次正式 failover 状态
- `runtime/failover-published.json`
	- 保存最终 published failover 输出
- `runtime/state-machine-snapshot.json`
	- 表示 takeover 时当前已经存在的最新 snapshot 边界


## 5. 先把“启动装载 checkpoint -> 检查 lease 是否过期 -> 探测 peer -> 正式 takeover -> publish failover 状态”这条后台主线立起来

下面这段代码故意只保留 10 件事：

1. 启动时先装载 lease checkpoint
2. checkpoint 里明确记录 `owner / role / term / lease_expires_at / snapshot_index / truncated_to`
3. `dict` 表示当前 active takeover registry
4. `list` 表示 peer catalog 和 recent histories
5. `queue + thread` 仍然只承接 `FAILOVER / SWEEP`
6. `task group` 先管住本次 takeover scope
7. local check 先判断 lease 是否已经正式过期
8. 再用真实 `xurl + xhttp` 去探测其他 peer
9. 满足 quorum 后，再用 `file_async future` 异步发布 failover 输出
10. join 完成之后再统一把 checkpoint 更新成 `takeover_committed` 或 `takeover_deferred`

这个骨架会展示这些事：

- 启动时先写一个 boot checkpoint：`owner = node-a`、`role = leader`、`lease_expires_at = now - 30`
- `snapshot_index = 44`、`truncated_to = 44` 已经来自上一页的 compaction 结果
- `beta` 仍有 warm shadow，不进入租约故障切换
- `theta` 会在 lease 已过期的前提下，向 `peer-a` 和 `peer-b` 发起 probe，随后由 `node-b` 正式 takeover
- 最终写出 `runtime/failover-published.json`

```c
#include "xrt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SIGNAL_FAILOVER 1
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
	uint32 iTerm;
	xtime tLeaseExpiresAt;
	char sKey[64];
	char sPeer[32];
	char sState[32];
	char sReason[64];
} DemoLeaseEvent;

typedef struct
{
	uint32 iFanoutWidth;
	uint32 iQuorumCount;
	uint32 iRemoteTimeoutMs;
	uint32 iPublishTimeoutMs;
	uint32 iLeaseWindowMs;
	uint32 iNextTerm;
} DemoLeasePolicy;

typedef struct
{
	xdict pActive;
	xlist pPeerCatalog;
	xlist pFailoverHistory;
	xlist pDeferred;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoLeasePolicy tPolicy;
	uint32 iActiveTakeovers;
	uint32 iTerm;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	xtime tLeaseExpiresAt;
	char sOwner[32];
	char sRole[32];
	char sLocalNode[32];
	char sCheckpointPath[260];
	char sPublishedPath[260];
} DemoLeaseCenter;

typedef struct
{
	char sStage[32];
	bool bLeaseExpired;
	int iDelayMs;
	int iStatus;
} DemoLeaseTask;

typedef struct
{
	char sPath[260];
	char sState[32];
	char sKey[64];
	char sOwner[32];
	char sRole[32];
	uint32 iTerm;
	uint32 iSnapshotIndex;
	uint32 iTruncatedTo;
	xtime tLeaseExpiresAt;
	uint32 iTakeoverDone;
	int iStatus;
} DemoCheckpointTask;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static bool procSaveLeaseCheckpoint(
	const char* sCheckpointPath,
	const char* sState,
	const char* sKey,
	const char* sOwner,
	const char* sRole,
	uint32 iTerm,
	uint32 iSnapshotIndex,
	uint32 iTruncatedTo,
	xtime tLeaseExpiresAt,
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
	xvoTableSetText(vRoot, "role", 0, (uint8*)((sRole != NULL) ? sRole : ""), 0, FALSE);
	xvoTableSetInt(vRoot, "term", 0, (int32)iTerm);
	xvoTableSetInt(vRoot, "snapshot_index", 0, (int32)iSnapshotIndex);
	xvoTableSetInt(vRoot, "truncated_to", 0, (int32)iTruncatedTo);
	xvoTableSetInt(vRoot, "lease_expires_at", 0, (int32)tLeaseExpiresAt);
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

static xvalue procLoadLeaseCheckpoint(const char* sCheckpointPath)
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

static int32 procLocalLeaseTask(ptr pArg, xfuture_result* pOut)
{
	DemoLeaseTask* pTask = (DemoLeaseTask*)pArg;

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

	pTask->iStatus = procSaveLeaseCheckpoint(
		pTask->sPath,
		pTask->sState,
		pTask->sKey,
		pTask->sOwner,
		pTask->sRole,
		pTask->iTerm,
		pTask->iSnapshotIndex,
		pTask->iTruncatedTo,
		pTask->tLeaseExpiresAt,
		pTask->iTakeoverDone) ? XRT_NET_OK : XRT_NET_ERROR;

	pOut->pValue = pTask;
	pOut->iStatus = pTask->iStatus;
	return pTask->iStatus;
}

static bool procPreparePeerProbeRequest(
	xhttprequest* pReq,
	char* sURL,
	size_t iURLCap,
	char* sHostHeader,
	size_t iHostCap,
	const DemoPeerNode* pPeer,
	const char* sKey,
	const char* sOwner,
	uint32 iTerm,
	uint32 iTimeoutMs)
{
	xrturlview tBase;
	char sRef[192];
	char sTerm[32];

	if ( (pReq == NULL) || (sURL == NULL) || (sHostHeader == NULL) || (pPeer == NULL) || (sKey == NULL) || (sOwner == NULL) ) {
		return false;
	}

	if ( !xrtUrlParseView(pPeer->sBaseURL, &tBase) ) {
		return false;
	}

	snprintf(sRef, sizeof(sRef), "/api/lease/%s?term=%u", sKey, (unsigned)iTerm);
	if ( !xrtUrlResolve(&tBase, sRef, sURL, iURLCap, NULL) ) {
		return false;
	}
	if ( !xrtUrlMakeHostHeader(&tBase, sHostHeader, iHostCap) ) {
		return false;
	}

	snprintf(sTerm, sizeof(sTerm), "%u", (unsigned)iTerm);

	xrtHttpRequestInit(pReq);
	if ( !xrtHttpRequestSetMethod(pReq, "GET") ) {
		return false;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return false;
	}
	(void)xrtHttpRequestSetHeader(pReq, "Host", sHostHeader);
	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	(void)xrtHttpRequestSetHeader(pReq, "X-Lease-Key", sKey);
	(void)xrtHttpRequestSetHeader(pReq, "X-Lease-Owner", sOwner);
	(void)xrtHttpRequestSetHeader(pReq, "X-Lease-Term", sTerm);
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

static const char* procChooseLeasePlan(const DemoWarmItem* pWarm, int iSelectedPeerCount, const DemoLeaseCenter* pCenter, xtime tNow)
{
	if ( (pWarm != NULL) && ((tNow - pWarm->tTouchedAt) <= 1200) ) {
		return "local_shadow";
	}
	if ( tNow <= pCenter->tLeaseExpiresAt ) {
		return "keep_owner";
	}
	if ( (uint32)(iSelectedPeerCount + 1) < pCenter->tPolicy.iQuorumCount ) {
		return "defer_takeover";
	}
	return "takeover_now";
}

static bool procRunLeaseFailover(DemoLeaseCenter* pCenter, DemoPeerNode* arrPeer, int iPeerCount, const char* sKey)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xtaskgroup* pGroup = NULL;
	xfuture* arrChild[DEMO_MAX_CHILD];
	xhttpresponse* arrResp[DEMO_MAX_PEER];
	xhttprequest arrReq[DEMO_MAX_PEER];
	DemoLeaseTask tLease;
	DemoCheckpointTask tCheckpointStart;
	xfuture* pJoin = NULL;
	xfuture* pPublish = NULL;
	xvalue vPublished = NULL;
	str sPublishedJson = NULL;
	size_t iPublishedSize = 0;
	char arrURL[DEMO_MAX_PEER][512];
	char arrHost[DEMO_MAX_PEER][320];
	uint32 iGrantedVotes = 0u;
	const char* sFinalState = "takeover_deferred";
	int iChildCount = 0;
	int i;
	bool bTakeoverDone = false;

	memset(arrChild, 0, sizeof(arrChild));
	memset(arrResp, 0, sizeof(arrResp));
	memset(arrReq, 0, sizeof(arrReq));
	memset(&tLease, 0, sizeof(tLease));
	memset(&tCheckpointStart, 0, sizeof(tCheckpointStart));

	if ( (pCenter == NULL) || (arrPeer == NULL) || (sKey == NULL) ) {
		return false;
	}

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

	procCopyText(tLease.sStage, sizeof(tLease.sStage), "check_local_lease");
	tLease.bLeaseExpired = true;
	tLease.iDelayMs = 30;
	tLease.iStatus = XRT_NET_OK;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procLocalLeaseTask, &tLease, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	procCopyText(tCheckpointStart.sPath, sizeof(tCheckpointStart.sPath), pCenter->sCheckpointPath);
	procCopyText(tCheckpointStart.sState, sizeof(tCheckpointStart.sState), "takeover_opened");
	procCopyText(tCheckpointStart.sKey, sizeof(tCheckpointStart.sKey), sKey);
	procCopyText(tCheckpointStart.sOwner, sizeof(tCheckpointStart.sOwner), pCenter->sOwner);
	procCopyText(tCheckpointStart.sRole, sizeof(tCheckpointStart.sRole), pCenter->sRole);
	tCheckpointStart.iTerm = pCenter->tPolicy.iNextTerm;
	tCheckpointStart.iSnapshotIndex = pCenter->iSnapshotIndex;
	tCheckpointStart.iTruncatedTo = pCenter->iTruncatedTo;
	tCheckpointStart.tLeaseExpiresAt = pCenter->tLeaseExpiresAt;
	tCheckpointStart.iTakeoverDone = 0u;

	arrChild[iChildCount] = xTaskGroupRunThread(pGroup, procCheckpointTask, &tCheckpointStart, 0u);
	if ( arrChild[iChildCount] == NULL ) {
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}
	iChildCount++;

	for ( i = 0; i < iPeerCount; i++ ) {
		if ( !procPreparePeerProbeRequest(
			&arrReq[i],
			arrURL[i],
			sizeof(arrURL[i]),
			arrHost[i],
			sizeof(arrHost[i]),
			&arrPeer[i],
			sKey,
			pCenter->sOwner,
			pCenter->tPolicy.iNextTerm,
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
		xTaskGroupCancel(pGroup);
		goto cleanup;
	}

	if ( tLease.bLeaseExpired && (tLease.iStatus == XRT_NET_OK) ) {
		iGrantedVotes++;
	}

	for ( i = 0; i < iPeerCount; i++ ) {
		arrResp[i] = (xhttpresponse*)xrtNetFutureValue((xnetfuture*)arrChild[i + 2]);
		if ( (arrResp[i] != NULL) && (arrResp[i]->iStatusCode >= 200u) && (arrResp[i]->iStatusCode < 300u) ) {
			iGrantedVotes++;
		}
		printf("peer[%d]=%s probe_status=%u\n",
			i,
			arrPeer[i].sName,
			(unsigned)((arrResp[i] != NULL) ? arrResp[i]->iStatusCode : 0u));
	}

	if ( iGrantedVotes >= pCenter->tPolicy.iQuorumCount ) {
		pCenter->iTerm = pCenter->tPolicy.iNextTerm;
		procCopyText(pCenter->sOwner, sizeof(pCenter->sOwner), pCenter->sLocalNode);
		procCopyText(pCenter->sRole, sizeof(pCenter->sRole), "leader");
		pCenter->tLeaseExpiresAt = xrtNow() + pCenter->tPolicy.iLeaseWindowMs;
		sFinalState = "takeover_committed";
		bTakeoverDone = true;

		vPublished = xvoCreateTable();
		if ( vPublished != NULL ) {
			xvoTableSetText(vPublished, "state", 0, (uint8*)"takeover_committed", 0, FALSE);
			xvoTableSetText(vPublished, "owner", 0, (uint8*)pCenter->sOwner, 0, FALSE);
			xvoTableSetText(vPublished, "role", 0, (uint8*)pCenter->sRole, 0, FALSE);
			xvoTableSetInt(vPublished, "term", 0, (int32)pCenter->iTerm);
			xvoTableSetInt(vPublished, "snapshot_index", 0, (int32)pCenter->iSnapshotIndex);
			xvoTableSetInt(vPublished, "truncated_to", 0, (int32)pCenter->iTruncatedTo);
			xvoTableSetInt(vPublished, "lease_expires_at", 0, (int32)pCenter->tLeaseExpiresAt);
			sPublishedJson = xrtStringifyJSON(vPublished, TRUE, &iPublishedSize);
		}

		if ( sPublishedJson != NULL ) {
			pPublish = xrtFileWriteAllAsync((str)pCenter->sPublishedPath, sPublishedJson, iPublishedSize, XRT_CP_UTF8);
			if ( (pPublish == NULL) || !xFutureWaitTimeout(pPublish, (int64)pCenter->tPolicy.iPublishTimeoutMs) ) {
				sFinalState = "takeover_publish_timeout";
				bTakeoverDone = false;
			}
		} else {
			sFinalState = "takeover_publish_error";
			bTakeoverDone = false;
		}
	}

	(void)procSaveLeaseCheckpoint(
		pCenter->sCheckpointPath,
		sFinalState,
		sKey,
		pCenter->sOwner,
		pCenter->sRole,
		bTakeoverDone ? pCenter->iTerm : pCenter->tPolicy.iNextTerm,
		pCenter->iSnapshotIndex,
		pCenter->iTruncatedTo,
		pCenter->tLeaseExpiresAt,
		bTakeoverDone ? 1u : 0u);

	printf("%s -> checkpoint=%s owner=%s role=%s term=%u lease_expires_at=%lld snapshot_index=%u truncated_to=%u state=%s\n",
		sKey,
		pCenter->sCheckpointPath,
		pCenter->sOwner,
		pCenter->sRole,
		(unsigned)(bTakeoverDone ? pCenter->iTerm : pCenter->tPolicy.iNextTerm),
		(long long)pCenter->tLeaseExpiresAt,
		(unsigned)pCenter->iSnapshotIndex,
		(unsigned)pCenter->iTruncatedTo,
		sFinalState);

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
	if ( sPublishedJson != NULL ) {
		xrtFree(sPublishedJson);
	}
	if ( vPublished != NULL ) {
		xvoUnref(vPublished);
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

	return bTakeoverDone;
}

int main(void)
{
	DemoLeaseCenter tCenter;
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
	tCenter.tPolicy.iLeaseWindowMs = 300u;
	tCenter.tPolicy.iNextTerm = 10u;
	tCenter.iTerm = 9u;
	tCenter.iSnapshotIndex = 44u;
	tCenter.iTruncatedTo = 44u;
	tCenter.tLeaseExpiresAt = tNow - 30;
	procCopyText(tCenter.sOwner, sizeof(tCenter.sOwner), "node-a");
	procCopyText(tCenter.sRole, sizeof(tCenter.sRole), "leader");
	procCopyText(tCenter.sLocalNode, sizeof(tCenter.sLocalNode), "node-b");
	procCopyText(tCenter.sCheckpointPath, sizeof(tCenter.sCheckpointPath), "runtime/lease-failover-checkpoint.json");
	procCopyText(tCenter.sPublishedPath, sizeof(tCenter.sPublishedPath), "runtime/failover-published.json");

	(void)procSaveLeaseCheckpoint(
		tCenter.sCheckpointPath,
		"boot",
		"theta",
		tCenter.sOwner,
		tCenter.sRole,
		tCenter.iTerm,
		tCenter.iSnapshotIndex,
		tCenter.iTruncatedTo,
		tCenter.tLeaseExpiresAt,
		0u);

	vBoot = procLoadLeaseCheckpoint(tCenter.sCheckpointPath);
	if ( vBoot != NULL ) {
		printf("boot checkpoint: owner=%s role=%s term=%d lease_expires_at=%d snapshot_index=%d truncated_to=%d\n",
			xvoTableGetText(vBoot, "owner", 0),
			xvoTableGetText(vBoot, "role", 0),
			(int)xvoTableGetInt(vBoot, "term", 0),
			(int)xvoTableGetInt(vBoot, "lease_expires_at", 0),
			(int)xvoTableGetInt(vBoot, "snapshot_index", 0),
			(int)xvoTableGetInt(vBoot, "truncated_to", 0));
		xvoUnref(vBoot);
	}

	tBetaWarm.tTouchedAt = tNow - 120;
	tBetaWarm.iKeyHash = 12001u;
	tBetaWarm.iHeat = 6;
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
	arrPeer[1].iLatencyMs = 80u;
	arrPeer[1].iPriority = 2u;

	procCopyText(arrPeer[2].sName, sizeof(arrPeer[2].sName), "peer-c");
	procCopyText(arrPeer[2].sBaseURL, sizeof(arrPeer[2].sBaseURL), "https://peer-c.example.com/");
	arrPeer[2].bHealthy = false;
	arrPeer[2].iLatencyMs = 110u;
	arrPeer[2].iPriority = 3u;

	sPlan = procChooseLeasePlan(&tBetaWarm, 0, &tCenter, tNow);
	printf("%s -> %s\n", tBetaWarm.sKey, sPlan);

	iSelected = procSelectPeers(arrPeer, 3, arrSelected, DEMO_MAX_PEER, tCenter.tPolicy.iFanoutWidth);
	sPlan = procChooseLeasePlan(NULL, iSelected, &tCenter, tNow);
	printf("theta -> %s\n", sPlan);
	if ( strcmp(sPlan, "takeover_now") == 0 ) {
		tCenter.iActiveTakeovers = 1u;
		if ( procRunLeaseFailover(&tCenter, arrSelected, iSelected, "theta") ) {
			tCenter.iActiveTakeovers = 0u;
		}
	}

	printf("checkpoint=%s published=%s owner=%s role=%s term=%u\n",
		tCenter.sCheckpointPath,
		tCenter.sPublishedPath,
		tCenter.sOwner,
		tCenter.sRole,
		(unsigned)tCenter.iTerm);
	return 0;
}
```


## 6. 租约故障切换里真正要稳定下来的边界

### 6.1 `owner / role / lease_expires_at / takeover_done` 必须进入正式 checkpoint

这页真正稳定下来的，不是“某台节点已经开始跑了”，而是：

- 当前 owner 是谁
- 当前 role 是什么
- 当前 lease 什么时候过期
- 当前 takeover 是否已经正式完成

如果这些字段不进入正式 checkpoint，下次启动就无法稳定恢复 ownership。


### 6.2 先确认 lease 过期，再允许 takeover

这页最容易写错的一点是：

- 看到当前 owner 没回响应，就立刻切换 ownership

但更稳的模型应该是：

- 先正式判断 `lease_expires_at`
- 再做 peer probe
- 最后才决定是否正式 takeover

否则系统会把短时抖动误判成 ownership 切换。


### 6.3 published failover 文件和 lease checkpoint 仍然不是同一份文件

这页里也很容易写混：

- 以为 published failover 文件已经足够回答当前 ownership 状态

但更稳的边界应该是：

- `runtime/failover-published.json`
	- 回答“这次 failover 对外发布了什么”
- `runtime/lease-failover-checkpoint.json`
	- 回答“当前 lease / takeover 状态走到了哪一步”


### 6.4 这页先停在“ownership 已接管”，不提前把它说成“新 leader 的恢复链已经完整接上”

这页故意把边界停在：

- lease 已经过期
- 新 owner 已经正式 takeover
- published failover 状态已经写出

但它还没有回答：

- 新 owner takeover 后怎样继续接日志
- compaction 半途中断时怎样接管未完成 work
- 新 owner 怎样补自己的本地状态

那是下一页应该专门解决的问题。


## 7. 什么时候不该直接照搬这套写法

下面这些情况，应该先升级模型，再决定是否继续复用这套骨架：

- 如果需要正式 lease renew / revoke，应继续补租约续期主线
- 如果需要 takeover 后继续补日志和 compaction，应继续补领导切换恢复主线
- 如果需要多 owner 竞争仲裁，应继续补更正式的 ownership arbitration 主线


## 8. 接到 `xhttpd + template` 的方式

把这条后台主线接进正式 dashboard 时，最稳的做法通常是：

1. `GET /api/dashboard`
	- 导出当前 `owner / role / term / lease_expires_at / snapshot_index / truncated_to`
2. `POST /api/failover`
	- 只提交 failover 意图，让 worker 决定是否进入 takeover 主线
3. `GET /api/lease`
	- 直接返回最近一次 lease checkpoint
4. `POST /api/sweep`
	- 手工或定时触发未完成 takeover 的重试与 checkpoint 刷新
5. `GET /dashboard`
	- 用同一份 `xvalue` 同时渲染 owner、lease 剩余时间、snapshot 边界和 recent failover history


## 9. 下一步阅读

如果你准备继续把恢复体系做得更重，最顺的下一步是：

1. [把本地控制台服务升级成一个快照压缩面板](snapshot-compaction-dashboard.md)
	先对比“snapshot / truncation 已完成”与“ownership 已正式接管”到底差在哪
2. [把本地控制台服务升级成一个日志回放面板](log-replay-dashboard.md)
	回看 replay gap 为什么仍然是新 owner takeover 之后要补的底层工作
3. [把本地控制台服务升级成一个领导切换恢复面板](leader-handoff-recovery-dashboard.md)
	把新 owner 的日志补齐、compaction 接管和状态恢复主线正式补出来
