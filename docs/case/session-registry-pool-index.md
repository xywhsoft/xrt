# 用 MemPool + AVLTree + List 组织一个会话索引与回收池

> 这个案例聚焦“会话对象 + 变长字符串 + 两套索引 + 一轮防御性 sweep”这条主线，把 `mempool + avltree + list` 串成一个真实可落地的注册表模型。重点不是证明容器能跑，而是讲清楚：哪一层是真正的 live root，哪一层只是二级索引，什么时候该显式 `Remove + Free`，什么时候才该补一轮 `GC(FALSE)`。

[返回案例索引](README.md)

---

## 1. 场景

假设你在写一个长连接网关或 agent 调度器，需要维护一张“活跃会话表”。

这张表有 6 个约束：

1. 会话要能按 `session_id` 快速定位
2. 认证、续期和踢人又需要按 `token` 反查
3. 运维侧希望按 `token` 有序导出当前会话快照
4. 会话对象里既有固定字段，也有长度发散的 `token / user_id / display_name`
5. 失败的预加载、热恢复或半路回滚，可能留下“已经分配、但还没进入 live root”的池内对象
6. Windows / Linux 上都要沿同一套运行时边界工作

如果没有一条清晰主线，代码很容易变成：

- `malloc/free` 到处散
- `session_id` 和 `token` 两边各管一份对象生命周期
- 清理时先删哪张表、谁该释放对象、谁只是借指针，全靠约定
- 一旦出现半路失败，就只能靠人工补 free

这个案例要解决的，正是“XRT 当前正式主线里，怎样把一张会话注册表拆成一套稳定的索引与回收模型”。


## 2. 为什么这次不用别的方案

### 2.1 为什么不是直接 `malloc/free`

因为这次的问题不是“只建 3 个对象然后马上退出”。

真正的问题是：

- 会话持续增删
- 字符串长度发散
- 同一条业务链里会不断创建和销毁很多小块、中块对象
- 还需要一条防御性 sweep 处理半路失败残留

所以这页故意从：

- `xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL)`

开始，而不是重新把生命周期拆回一堆零散 `malloc/free`。


### 2.2 为什么 `token` 这一侧不用 `dict`

`dict` 当然能做字符串 key 映射，但这次除了“查到对象”，还想顺手得到：

- 按 token 有序 walk
- 稳定导出快照
- 一棵明确的二级索引树

这正是：

- `avltree`

更顺手的地方。

也就是说，这页不是在证明 `dict` 不行，而是在强调：

- `dict` 更像“任意字节 key 的映射”
- `avltree` 更像“我想自己决定排序和遍历结构”


### 2.3 为什么 `session_id` 这一侧不用数组或再上一棵 `avltree`

因为 `session_id` 天生就是：

- `int64` key
- 可能跳号
- 可能很稀疏

这正是：

- `list`

最自然的主线。

你当然也可以再建一棵 `avltree`，但那样只是把已经固定为整数 key 的问题，又重新讲成“通用比较函数”。


### 2.4 为什么 `GC(FALSE)` 不能替代显式 `Remove + Free`

这是这页最重要的边界之一。

正常断开、踢人、会话过期时，最稳的收口仍然是：

1. 从二级索引删掉
2. 从 live root 删掉
3. 释放会话对象和它拥有的池内字符串

而：

- `xrtMemPoolGC(objMP, FALSE)`

只适合处理另一类问题：

- 半路失败留下的池内孤儿对象
- 恢复 / 导入阶段生成、但没进入 live root 的临时对象

也就是说，GC 在这里是：

- 防御性 sweep

不是：

- 替代一切显式释放


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `mempool` | 承担会话对象和变长字符串的统一分配域 | 固定字段和变长字段可以沿同一池收口 |
| `list` | 作为 `session_id -> session*` 的 authoritative live root | 用整数 ID 稀疏映射活跃对象 |
| `avltree` | 作为 `token -> session*` 的有序二级索引 | 既能查 token，又能按 token 有序导出 |
| `list walk + GC(FALSE)` | 标记 live root，再 sweep 未进入 root 的池内对象 | 恢复 / 导入阶段的半成品残留可以统一清走 |

可以先记一句话：

> `list` 负责“这批会话现在还活着吗”，`avltree` 负责“我怎样按 token 找到它们”，`mempool` 负责“对象和字符串沿同一个分配域统一收口”。


## 4. 数据模型约定

这页故意把 ownership 约定压得很明确：

- `DemoSession`
	- 由 `mempool` 分配
	- 拥有 `sToken / sUserID / sDisplayName`
- `list`
	- 只保存 `DemoSession*`
	- 这张表是 live root
- `avltree`
	- 节点里只保存 `DemoSession*`
	- 不拥有会话对象
	- 只是按 `token` 的二级索引

也就是说，真实所有权只在一处：

- `DemoSession` 自己拥有自己的池内字段

而不是让两张索引表去竞争释放责任。


## 5. 完整骨架

下面这段代码会做 7 件事：

1. 创建一个 `mempool + list + avltree` 组成的会话注册表
2. 分配会话对象和变长字符串
3. 把同一个 `DemoSession*` 同时挂到 `session_id` live root 和 `token` 二级索引
4. 支持按 `session_id` / `token` 两种方式查找
5. 按 token 有序导出当前会话
6. 用一条“半路失败的孤儿会话”演示防御性 sweep
7. 用显式 `Remove + Free` 演示正常断开的收口路径

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct DemoSession DemoSession;
struct DemoSession {
	int64 iSessionID;
	uint32 iLastSeenMS;
	char* sToken;
	char* sUserID;
	char* sDisplayName;
};

typedef struct {
	DemoSession* pSession;
} DemoTokenSlot;

typedef struct {
	xmempool pPool;
	xlist pBySessionID;
	xavltree pByToken;
} DemoSessionRegistry;


static int procCompareTokenSlot(ptr pNode, ptr pKey)
{
	DemoTokenSlot* pSlot = (DemoTokenSlot*)pNode;

	if ( (pSlot == NULL) || (pSlot->pSession == NULL) || (pSlot->pSession->sToken == NULL) ) {
		return -1;
	}

	return strcmp(pSlot->pSession->sToken, (const char*)pKey);
}

static char* procPoolCopy(xmempool pPool, const char* sText)
{
	size_t iLen = 0;
	char* sCopy = NULL;

	if ( (pPool == NULL) || (sText == NULL) ) {
		return NULL;
	}

	iLen = strlen(sText);
	sCopy = (char*)xrtMemPoolAlloc(pPool, (uint32)(iLen + 1u));
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void procFreeSessionFields(DemoSessionRegistry* pRegistry, DemoSession* pSession)
{
	if ( (pRegistry == NULL) || (pRegistry->pPool == NULL) || (pSession == NULL) ) {
		return;
	}

	if ( pSession->sDisplayName != NULL ) {
		xrtMemPoolFree(pRegistry->pPool, pSession->sDisplayName);
		pSession->sDisplayName = NULL;
	}
	if ( pSession->sUserID != NULL ) {
		xrtMemPoolFree(pRegistry->pPool, pSession->sUserID);
		pSession->sUserID = NULL;
	}
	if ( pSession->sToken != NULL ) {
		xrtMemPoolFree(pRegistry->pPool, pSession->sToken);
		pSession->sToken = NULL;
	}
}

static DemoSession* procAllocSession(
	DemoSessionRegistry* pRegistry,
	int64 iSessionID,
	const char* sToken,
	const char* sUserID,
	const char* sDisplayName,
	uint32 iNowMS
)
{
	DemoSession* pSession = NULL;

	if ( pRegistry == NULL ) {
		return NULL;
	}

	pSession = (DemoSession*)xrtMemPoolAlloc(pRegistry->pPool, sizeof(DemoSession));
	if ( pSession == NULL ) {
		return NULL;
	}

	memset(pSession, 0, sizeof(*pSession));
	pSession->iSessionID = iSessionID;
	pSession->iLastSeenMS = iNowMS;
	pSession->sToken = procPoolCopy(pRegistry->pPool, sToken);
	pSession->sUserID = procPoolCopy(pRegistry->pPool, sUserID);
	pSession->sDisplayName = procPoolCopy(pRegistry->pPool, sDisplayName);

	if ( (pSession->sToken == NULL) || (pSession->sUserID == NULL) || (pSession->sDisplayName == NULL) ) {
		procFreeSessionFields(pRegistry, pSession);
		xrtMemPoolFree(pRegistry->pPool, pSession);
		return NULL;
	}

	return pSession;
}

static bool procRegistryInit(DemoSessionRegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return FALSE;
	}

	memset(pRegistry, 0, sizeof(*pRegistry));

	pRegistry->pPool = xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL);
	pRegistry->pBySessionID = xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	pRegistry->pByToken = xrtAVLTreeCreate(sizeof(DemoTokenSlot), procCompareTokenSlot, XRT_OBJMODE_LOCAL);

	if ( (pRegistry->pPool == NULL) || (pRegistry->pBySessionID == NULL) || (pRegistry->pByToken == NULL) ) {
		if ( pRegistry->pByToken != NULL ) {
			xrtAVLTreeDestroy(pRegistry->pByToken);
			pRegistry->pByToken = NULL;
		}
		if ( pRegistry->pBySessionID != NULL ) {
			xrtListDestroy(pRegistry->pBySessionID);
			pRegistry->pBySessionID = NULL;
		}
		if ( pRegistry->pPool != NULL ) {
			xrtMemPoolDestroy(pRegistry->pPool);
			pRegistry->pPool = NULL;
		}
		return FALSE;
	}

	return TRUE;
}

static void procRegistryUnit(DemoSessionRegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}

	if ( pRegistry->pBySessionID != NULL ) {
		xrtListDestroy(pRegistry->pBySessionID);
		pRegistry->pBySessionID = NULL;
	}
	if ( pRegistry->pByToken != NULL ) {
		xrtAVLTreeDestroy(pRegistry->pByToken);
		pRegistry->pByToken = NULL;
	}
	if ( pRegistry->pPool != NULL ) {
		xrtMemPoolDestroy(pRegistry->pPool);
		pRegistry->pPool = NULL;
	}
}

static bool procRegistryAddSession(
	DemoSessionRegistry* pRegistry,
	int64 iSessionID,
	const char* sToken,
	const char* sUserID,
	const char* sDisplayName,
	uint32 iNowMS
)
{
	DemoSession* pSession = NULL;
	DemoTokenSlot* pTokenSlot = NULL;
	bool bNew = FALSE;

	if ( (pRegistry == NULL) || (sToken == NULL) ) {
		return FALSE;
	}

	if ( xrtListExists(pRegistry->pBySessionID, iSessionID) ) {
		return FALSE;
	}
	if ( xrtAVLTreeSearch(pRegistry->pByToken, (ptr)sToken) != NULL ) {
		return FALSE;
	}

	pSession = procAllocSession(pRegistry, iSessionID, sToken, sUserID, sDisplayName, iNowMS);
	if ( pSession == NULL ) {
		return FALSE;
	}

	pTokenSlot = (DemoTokenSlot*)xrtAVLTreeInsert(pRegistry->pByToken, pSession->sToken, &bNew);
	if ( (pTokenSlot == NULL) || !bNew ) {
		goto fail;
	}

	memset(pTokenSlot, 0, sizeof(*pTokenSlot));
	pTokenSlot->pSession = pSession;

	if ( !xrtListSetPtr(pRegistry->pBySessionID, iSessionID, pSession, NULL) ) {
		xrtAVLTreeRemove(pRegistry->pByToken, pSession->sToken);
		goto fail;
	}

	return TRUE;

fail:
	procFreeSessionFields(pRegistry, pSession);
	xrtMemPoolFree(pRegistry->pPool, pSession);
	return FALSE;
}

static DemoSession* procRegistryFindBySessionID(DemoSessionRegistry* pRegistry, int64 iSessionID)
{
	if ( pRegistry == NULL ) {
		return NULL;
	}

	return (DemoSession*)xrtListGetPtr(pRegistry->pBySessionID, iSessionID);
}

static DemoSession* procRegistryFindByToken(DemoSessionRegistry* pRegistry, const char* sToken)
{
	DemoTokenSlot* pTokenSlot = NULL;

	if ( (pRegistry == NULL) || (sToken == NULL) ) {
		return NULL;
	}

	pTokenSlot = (DemoTokenSlot*)xrtAVLTreeSearch(pRegistry->pByToken, (ptr)sToken);
	return (pTokenSlot != NULL) ? pTokenSlot->pSession : NULL;
}

static bool procRegistryRemoveSession(DemoSessionRegistry* pRegistry, int64 iSessionID)
{
	DemoSession* pSession = NULL;

	if ( pRegistry == NULL ) {
		return FALSE;
	}

	pSession = (DemoSession*)xrtListGetPtr(pRegistry->pBySessionID, iSessionID);
	if ( pSession == NULL ) {
		return FALSE;
	}
	if ( !xrtAVLTreeRemove(pRegistry->pByToken, pSession->sToken) ) {
		return FALSE;
	}

	pSession = (DemoSession*)xrtListRemovePtr(pRegistry->pBySessionID, iSessionID);
	if ( pSession == NULL ) {
		return FALSE;
	}

	procFreeSessionFields(pRegistry, pSession);
	xrtMemPoolFree(pRegistry->pPool, pSession);
	return TRUE;
}

static bool procMarkLiveSession(int64 iKey, ptr pVal, ptr pArg)
{
	DemoSession* pSession = *(DemoSession**)pVal;
	(void)iKey;
	(void)pArg;

	if ( pSession != NULL ) {
		xrtMemPoolGC_Mark(pSession);
		if ( pSession->sToken != NULL ) {
			xrtMemPoolGC_Mark(pSession->sToken);
		}
		if ( pSession->sUserID != NULL ) {
			xrtMemPoolGC_Mark(pSession->sUserID);
		}
		if ( pSession->sDisplayName != NULL ) {
			xrtMemPoolGC_Mark(pSession->sDisplayName);
		}
	}

	return FALSE;
}

static void procRegistrySweep(DemoSessionRegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}

	xrtListWalk(pRegistry->pBySessionID, procMarkLiveSession, NULL);
	xrtMemPoolGC(pRegistry->pPool, FALSE);
}

static void procDumpByToken(DemoSessionRegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}

	AVLTREE_FOREACH_TYPE(pRegistry->pByToken, pSlot, DemoTokenSlot*)
	{
		DemoSession* pSession = pSlot->pSession;

		printf(
			"token=%s sid=%lld user=%s name=%s last_seen=%u\n",
			pSession->sToken,
			(long long)pSession->iSessionID,
			pSession->sUserID,
			pSession->sDisplayName,
			(unsigned)pSession->iLastSeenMS
		);
	}
}

int main(void)
{
	DemoSessionRegistry tRegistry;
	DemoSession* pSession = NULL;
	DemoSession* pOrphan = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procRegistryInit(&tRegistry) ) {
		xrtUnit();
		return 1;
	}

	if ( !procRegistryAddSession(&tRegistry, 1001, "tok-alice-001", "u-alice", "Alice", 10) ) {
		goto cleanup;
	}
	if ( !procRegistryAddSession(&tRegistry, 1002, "tok-bob-002", "u-bob", "Bob", 12) ) {
		goto cleanup;
	}
	if ( !procRegistryAddSession(&tRegistry, 2048, "tok-ops-2048", "u-ops", "OpsMonitor", 13) ) {
		goto cleanup;
	}

	pSession = procRegistryFindByToken(&tRegistry, "tok-bob-002");
	if ( pSession != NULL ) {
		printf("find by token => sid=%lld name=%s\n",
			(long long)pSession->iSessionID,
			pSession->sDisplayName);
	}

	pSession = procRegistryFindBySessionID(&tRegistry, 2048);
	if ( pSession != NULL ) {
		printf("find by session id => token=%s user=%s\n",
			pSession->sToken,
			pSession->sUserID);
	}

	printf("ordered dump:\n");
	procDumpByToken(&tRegistry);

	pOrphan = procAllocSession(&tRegistry, 9003, "tok-orphan", "u-ghost", "Ghost", 99);
	if ( pOrphan != NULL ) {
		printf("orphan session allocated but not indexed yet\n");
	}

	procRegistrySweep(&tRegistry);
	printf("live root count => %u\n", xrtListCount(tRegistry.pBySessionID));

	if ( !procRegistryRemoveSession(&tRegistry, 1002) ) {
		goto cleanup;
	}

	printf("after remove sid=1002:\n");
	procDumpByToken(&tRegistry);
	iRet = 0;

cleanup:
	procRegistryUnit(&tRegistry);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码最重要的 5 个边界

### 6.1 `list` 才是 authoritative live root

这页故意只从：

- `pBySessionID`

去做 `GC_Mark()`。

原因不是 `avltree` 不重要，而是这条模型要刻意把根说清楚：

- `list`
	- 表示“这批会话当前还活着”
- `avltree`
	- 只是按 token 找活跃会话的二级索引

一旦你把两张索引表都写成“谁都像 root”，后面就很难说清 sweep 该沿谁标记。


### 6.2 `avltree` 节点不拥有会话对象

这页没有给：

- `pByToken->FreeProc`

绑定会话释放逻辑。

因为 token 树的职责只是：

- 组织二级索引

它不应该拥有：

- `DemoSession`

否则你一边删 token 树，一边删 `list`，很容易把释放责任打成两份。


### 6.3 正常断开靠显式 `Remove + Free`

`procRegistryRemoveSession()` 走的是：

1. 先用 `session_id` 找到当前 live 会话
2. 从 token 树里删掉二级索引
3. 从 `list` 里删掉 live root
4. 显式释放 `sToken / sUserID / sDisplayName / DemoSession`

这条路径才是日常主线。

也就是说，平时断开连接、过期淘汰、管理员踢人时，最稳的仍然是显式收口。


### 6.4 `GC(FALSE)` 只清“没进入 root 的池内孤儿对象”

这页特意构造了：

- `pOrphan = procAllocSession(...)`

但没有把它插进任一索引。

这就模拟了：

- 预加载做到一半失败
- 热恢复构造到一半回滚
- 还没 commit 到 live root 就被中断

这时：

- `procRegistrySweep()`

就能用：

- `list walk -> mark live -> xrtMemPoolGC(FALSE)`

把这类池内孤儿对象扫掉。


### 6.5 这条模型故意让 `mempool` 管“对象 + 变长字符串”

如果这里只有固定大小结构体，优先可能反而是：

- `fsmempool`

但这页的问题是：

- 会话结构体本体大小固定
- `token / user_id / display_name` 长度发散

这正是 `mempool` 最顺手的地方：

- 小对象和变长字符串沿同一个分配域统一收口


## 7. 为什么这条模型适合做会话注册表

因为它把 3 件最容易混在一起的事拆开了：

1. 谁是真正的 live root
2. 谁只是辅助索引
3. 对象内存沿哪一个生命周期域统一分配和统一收口

你最终得到的是：

- `session_id` 查找路径清楚
- `token` 查找与有序导出路径清楚
- 失败恢复期的残留 sweep 路径也清楚

这比“几张表都随手存指针，最后靠人工找 free”稳定得多。


## 8. 这条模型最适合什么项目

它最适合：

- 长连接网关的会话注册表
- agent / worker 的活跃任务索引
- 认证 token 与整数 ID 同时存在的在线状态表
- 热恢复、快照重建、批量导入这类会产生半成品对象的服务

它不适合：

- 只有几条对象、生命周期极短的脚本
- 想把多张索引表都写成强拥有者的模型
- 根本没有变长字符串或混合尺寸对象的纯固定块场景


## 9. 常见错误

### 9.1 错误一：让 `avltree` 和 `list` 同时拥有会话对象

这样一到删除路径就会开始争抢释放责任。


### 9.2 错误二：把 `GC(FALSE)` 当成日常删除主线

正常断开仍然应该显式 `Remove + Free`，GC 在这里是恢复期防御手段。


### 9.3 错误三：只标记会话本体，不标记池内字符串

`mempool` 不会替你自动追对象图。


### 9.4 错误四：`session_id` 明明是整数 key，却还再手写一棵通用比较树

这会把本来很直接的整数 live root，重新讲复杂。


### 9.5 错误五：把 token 树节点写成“再复制一份大对象”

这页故意只让 token 树节点保存：

- `DemoSession*`

因为它的职责只是索引，不是再养一份对象副本。


## 10. 建议继续阅读

- [MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`](../guide/mempool-intro.md)
- [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](../guide/avltree-intro.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](../guide/list-intro.md)
- [MemPool API](../api/api-mempool.md)
- [AVLTree API](../api/api-avltree.md)
- [List API](../api/api-list.md)

---

**一句话总结：** 这条案例主线的关键不在“我用了 3 个容器”，而在“`list` 明确作为 live root，`avltree` 只承担按 token 的二级索引，`mempool` 统一承担对象和变长字符串的生命周期；正常删除走显式 `Remove + Free`，恢复期残留再补一轮 `GC(FALSE)`”；这才是 `mempool + avltree + list` 在真实服务里最稳的一种组合写法。
