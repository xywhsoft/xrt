# 用 Dict + List + AVLTree 组织一个多租户索引注册表

> 这个案例聚焦“tenant root + `tenant_id` 二级索引 + tenant 内局部有序索引”这条主线，把 `dict + list + avltree + hash` 串成一个更贴近真实业务的多租户注册表模型。重点不是证明容器都能跑，而是讲清楚：哪一层是真正的 tenant authoritative root，哪一层只是二级定位入口，什么时候该把索引拆到 tenant 内部，而不是继续往一个全局字典里硬塞复合 key。

[返回案例索引](README.md)

---

## 1. 场景

假设你在做一个本地管理面板或控制服务，需要把“同一类资源”按 tenant 组织起来。

这张注册表有 6 个约束：

1. 业务入口更习惯按 `tenant_code` 找 tenant
2. 运维和后台任务又经常按 `tenant_id` 反查
3. 同一个 tenant 下面，还要按资源 key 有序导出当前项目或缓存项
4. 新租户和整租户删除都应该有明确的收口路径
5. 以后还可能继续挂 HTTP、模板、快照导出
6. 不能把“当前 root”和“二级索引”混成同一种东西

如果没有一条清晰主线，代码很容易退化成：

- 一个全局 `dict`，key 强行拼成 `tenant:key`
- tenant 删除时，只能全表扫描找前缀
- `tenant_code`、`tenant_id`、tenant 内项目 key 三种查找方式彼此打架
- 页面和导出逻辑越来越依赖临时字符串拼接

这个案例要解决的，正是：

> 当系统从“单租户集合”升级成“tenant root + tenant 内局部索引”的多租户结构后，怎样把 root、二级索引和 tenant 内排序边界正式拆开。


## 2. 为什么这次不是“一个全局字典搞定”

### 2.1 全局复合 key 只能解决查到，不解决 tenant 边界

你当然可以用：

- `tenant:key`

这样的复合字符串做一张全局 `dict`。

它能解决：

- 某个完整复合 key 对应哪个对象

但它很难自然解决：

- tenant 自己的 root 在哪里
- 按 `tenant_id` 怎么快速找到 tenant
- 删除整个 tenant 时，怎么收口一整棵局部索引
- 怎么按 tenant 内部 key 有序导出

这正是这页要拆成 3 层的原因。


### 2.2 `dict` 负责 authoritative root，不负责所有维度

这页里最自然的 root 是：

- `tenant_code -> DemoTenantBucket*`

因为对外的大多数业务入口，本质上都是“先定位 tenant”。

所以：

- `dict`
	- 做 tenant authoritative root

而不是让它既管 tenant，又硬扛 tenant 内所有资源排序。


### 2.3 `list` 负责 `tenant_id` 这种整数二级索引

一旦系统里还有：

- `tenant_id`

这种天然整数 key，最稳的 secondary index 不是继续拼字符串，而是：

- `list`

它正适合表达：

- `tenant_id -> bucket`

这和前面的 root 不竞争所有权，只补另一条查找路径。


### 2.4 tenant 内有序导出应该回到 `avltree`

到了 tenant 内部，你的问题又变成：

- 项目 key 或缓存 key 按顺序导出
- key 不只是整数
- 我想保留局部有序结构

这时最自然的就不是：

- 再拼一层 `dict`

而是：

- `avltree`

也就是说，这页真正强调的是：

> tenant 外层先解决 root 和 secondary index，tenant 内层再解决局部有序索引。


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `dict` | `tenant_code -> bucket` authoritative root | 业务入口按租户编码快速定位 |
| `list` | `tenant_id -> bucket` secondary index | 整数 ID 反查和后台任务定位 |
| `avltree` | tenant 内 `item_key -> item` 局部有序索引 | 局部有序导出和 tenant 内查找 |
| `hash` | 为 `tenant_code / item_key` 生成稳定指纹 | 导出、日志或快照里的轻量标识 |

可以先记一句话：

> `dict` 负责先找到 tenant，`list` 负责另一条整数定位入口，`avltree` 负责 tenant 内部自己的有序世界。


## 4. 所有权约定

这页故意把 ownership 压得非常明确：

- `DemoTenantRegistry`
	- 拥有一张 `dict`
	- 拥有一张 `list`
- `dict`
	- 作为 authoritative root
	- 保存并拥有 `DemoTenantBucket*`
- `list`
	- 只保存同一批 `DemoTenantBucket*`
	- 不拥有 bucket
- `DemoTenantBucket`
	- 拥有自己的 `xavltree`
- `xavltree`
	- 直接保存 tenant 内的 `DemoTenantItem`

也就是说，真实释放责任只有一条主线：

1. 先从二级索引摘掉 bucket
2. 再从 authoritative root 移除 bucket
3. 最后销毁 bucket 自己的局部 `avltree`


## 5. 完整骨架

下面这段代码会做 7 件事：

1. 创建一个 `dict + list` 组成的 tenant 注册表
2. 新增两个 tenant
3. 在不同 tenant 下插入局部 item
4. 同时支持按 `tenant_code` 和 `tenant_id` 两种方式查 tenant
5. 在 tenant 内按 item key 有序导出
6. 演示更新同一个 tenant 内的现有 item
7. 演示删除整个 tenant 的明确收口路径

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	char sItemKey[64];
	char sTitle[64];
	uint64 iKeyHash;
	int32 iValueCount;
} DemoTenantItem;

typedef struct
{
	int64 iTenantID;
	int32 iItemCount;
	uint64 iTenantHash;
	char sTenantCode[32];
	xavltree pByItemKey;
} DemoTenantBucket;

typedef struct
{
	xdict pByTenantCode;
	xlist pByTenantID;
} DemoTenantRegistry;


static void procCopyText(char* sDst, size_t iCap, const char* sText)
{
	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	snprintf(sDst, iCap, "%s", (sText != NULL) ? sText : "");
}

static int procCompareTenantItem(ptr pNode, ptr pKey)
{
	DemoTenantItem* pItem = (DemoTenantItem*)pNode;

	if ( (pItem == NULL) || (pKey == NULL) ) {
		return -1;
	}

	return strcmp(pItem->sItemKey, (const char*)pKey);
}

static DemoTenantBucket* procTenantBucketCreate(int64 iTenantID, const char* sTenantCode)
{
	DemoTenantBucket* pBucket = NULL;

	if ( sTenantCode == NULL ) {
		return NULL;
	}

	pBucket = (DemoTenantBucket*)xrtMalloc(sizeof(DemoTenantBucket));
	if ( pBucket == NULL ) {
		return NULL;
	}

	memset(pBucket, 0, sizeof(*pBucket));
	pBucket->iTenantID = iTenantID;
	procCopyText(pBucket->sTenantCode, sizeof(pBucket->sTenantCode), sTenantCode);
	pBucket->iTenantHash = xrtHash64((ptr)pBucket->sTenantCode, strlen(pBucket->sTenantCode));

	pBucket->pByItemKey = xrtAVLTreeCreate(sizeof(DemoTenantItem), procCompareTenantItem, XRT_OBJMODE_LOCAL);
	if ( pBucket->pByItemKey == NULL ) {
		xrtFree(pBucket);
		return NULL;
	}

	return pBucket;
}

static void procTenantBucketDestroy(DemoTenantBucket* pBucket)
{
	if ( pBucket == NULL ) {
		return;
	}

	if ( pBucket->pByItemKey != NULL ) {
		xrtAVLTreeDestroy(pBucket->pByItemKey);
		pBucket->pByItemKey = NULL;
	}

	xrtFree(pBucket);
}

static bool procFreeTenantBucket(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	DemoTenantBucket* pBucket = *(DemoTenantBucket**)pVal;
	(void)pKey;
	(void)pArg;

	if ( pBucket != NULL ) {
		procTenantBucketDestroy(pBucket);
	}

	return FALSE;
}

static bool procTenantRegistryInit(DemoTenantRegistry* pRegistry)
{
	memset(pRegistry, 0, sizeof(*pRegistry));

	pRegistry->pByTenantCode = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	if ( pRegistry->pByTenantCode == NULL ) {
		return FALSE;
	}

	pRegistry->pByTenantID = xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	if ( pRegistry->pByTenantID == NULL ) {
		xrtDictDestroy(pRegistry->pByTenantCode);
		pRegistry->pByTenantCode = NULL;
		return FALSE;
	}

	return TRUE;
}

static void procTenantRegistryUnit(DemoTenantRegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}

	if ( pRegistry->pByTenantCode != NULL ) {
		xrtDictWalk(pRegistry->pByTenantCode, procFreeTenantBucket, NULL);
		xrtDictDestroy(pRegistry->pByTenantCode);
		pRegistry->pByTenantCode = NULL;
	}
	if ( pRegistry->pByTenantID != NULL ) {
		xrtListDestroy(pRegistry->pByTenantID);
		pRegistry->pByTenantID = NULL;
	}
}

static DemoTenantBucket* procTenantRegistryFindByCode(DemoTenantRegistry* pRegistry, const char* sTenantCode)
{
	if ( (pRegistry == NULL) || (sTenantCode == NULL) ) {
		return NULL;
	}

	return (DemoTenantBucket*)xrtDictGetPtr(pRegistry->pByTenantCode, (ptr)sTenantCode, (uint32)strlen(sTenantCode));
}

static DemoTenantBucket* procTenantRegistryFindByID(DemoTenantRegistry* pRegistry, int64 iTenantID)
{
	if ( pRegistry == NULL ) {
		return NULL;
	}

	return (DemoTenantBucket*)xrtListGetPtr(pRegistry->pByTenantID, iTenantID);
}

static bool procTenantRegistryAddTenant(DemoTenantRegistry* pRegistry, int64 iTenantID, const char* sTenantCode)
{
	DemoTenantBucket* pBucket = NULL;

	if ( (pRegistry == NULL) || (sTenantCode == NULL) ) {
		return FALSE;
	}

	if ( xrtDictExists(pRegistry->pByTenantCode, (ptr)sTenantCode, (uint32)strlen(sTenantCode)) ) {
		return FALSE;
	}
	if ( xrtListExists(pRegistry->pByTenantID, iTenantID) ) {
		return FALSE;
	}

	pBucket = procTenantBucketCreate(iTenantID, sTenantCode);
	if ( pBucket == NULL ) {
		return FALSE;
	}

	if ( !xrtDictSetPtr(pRegistry->pByTenantCode, (ptr)pBucket->sTenantCode, (uint32)strlen(pBucket->sTenantCode), pBucket, NULL) ) {
		procTenantBucketDestroy(pBucket);
		return FALSE;
	}

	if ( !xrtListSetPtr(pRegistry->pByTenantID, iTenantID, pBucket, NULL) ) {
		(void)xrtDictRemovePtr(pRegistry->pByTenantCode, (ptr)pBucket->sTenantCode, (uint32)strlen(pBucket->sTenantCode));
		procTenantBucketDestroy(pBucket);
		return FALSE;
	}

	return TRUE;
}

static bool procTenantRegistryAddItem(
	DemoTenantRegistry* pRegistry,
	const char* sTenantCode,
	const char* sItemKey,
	const char* sTitle,
	int32 iValueCount
)
{
	DemoTenantBucket* pBucket = NULL;
	DemoTenantItem* pItem = NULL;
	bool bNew = FALSE;

	if ( (pRegistry == NULL) || (sTenantCode == NULL) || (sItemKey == NULL) ) {
		return FALSE;
	}

	pBucket = procTenantRegistryFindByCode(pRegistry, sTenantCode);
	if ( pBucket == NULL ) {
		return FALSE;
	}

	pItem = (DemoTenantItem*)xrtAVLTreeInsert(pBucket->pByItemKey, (ptr)sItemKey, &bNew);
	if ( pItem == NULL ) {
		return FALSE;
	}

	if ( bNew ) {
		memset(pItem, 0, sizeof(*pItem));
		procCopyText(pItem->sItemKey, sizeof(pItem->sItemKey), sItemKey);
		pBucket->iItemCount++;
	}

	procCopyText(pItem->sTitle, sizeof(pItem->sTitle), sTitle);
	pItem->iKeyHash = xrtHash64((ptr)pItem->sItemKey, strlen(pItem->sItemKey));
	pItem->iValueCount = iValueCount;

	return TRUE;
}

static bool procTenantRegistryRemoveTenant(DemoTenantRegistry* pRegistry, const char* sTenantCode)
{
	DemoTenantBucket* pBucket = NULL;

	if ( (pRegistry == NULL) || (sTenantCode == NULL) ) {
		return FALSE;
	}

	pBucket = procTenantRegistryFindByCode(pRegistry, sTenantCode);
	if ( pBucket == NULL ) {
		return FALSE;
	}

	(void)xrtListRemove(pRegistry->pByTenantID, pBucket->iTenantID);
	pBucket = (DemoTenantBucket*)xrtDictRemovePtr(pRegistry->pByTenantCode, (ptr)sTenantCode, (uint32)strlen(sTenantCode));
	if ( pBucket == NULL ) {
		return FALSE;
	}

	procTenantBucketDestroy(pBucket);
	return TRUE;
}

static void procDumpTenantItems(DemoTenantBucket* pBucket)
{
	if ( pBucket == NULL ) {
		return;
	}

	printf(
		"tenant=%s tenant_id=%lld tenant_hash=%llu item_count=%d\n",
		pBucket->sTenantCode,
		(long long)pBucket->iTenantID,
		(unsigned long long)pBucket->iTenantHash,
		(int)pBucket->iItemCount
	);

	AVLTREE_FOREACH_TYPE(pBucket->pByItemKey, pItem, DemoTenantItem*)
	{
		printf(
			"  item=%s title=%s count=%d hash=%llu\n",
			pItem->sItemKey,
			pItem->sTitle,
			(int)pItem->iValueCount,
			(unsigned long long)pItem->iKeyHash
		);
	}
}

int main(void)
{
	DemoTenantRegistry tRegistry;
	DemoTenantBucket* pBucket = NULL;
	int iRet = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}
	if ( !procTenantRegistryInit(&tRegistry) ) {
		xrtUnit();
		return 1;
	}

	if ( !procTenantRegistryAddTenant(&tRegistry, 1001, "alpha") ) {
		goto cleanup;
	}
	if ( !procTenantRegistryAddTenant(&tRegistry, 2002, "beta") ) {
		goto cleanup;
	}

	if ( !procTenantRegistryAddItem(&tRegistry, "alpha", "cache:user:1001", "Alpha User Cache", 3) ) {
		goto cleanup;
	}
	if ( !procTenantRegistryAddItem(&tRegistry, "alpha", "cache:user:1002", "Alpha Admin Cache", 1) ) {
		goto cleanup;
	}
	if ( !procTenantRegistryAddItem(&tRegistry, "beta", "queue:sync", "Beta Sync Queue", 5) ) {
		goto cleanup;
	}
	if ( !procTenantRegistryAddItem(&tRegistry, "alpha", "cache:user:1001", "Alpha User Cache v2", 4) ) {
		goto cleanup;
	}

	pBucket = procTenantRegistryFindByCode(&tRegistry, "alpha");
	if ( pBucket != NULL ) {
		printf("find by tenant code => tenant_id=%lld\n", (long long)pBucket->iTenantID);
	}

	pBucket = procTenantRegistryFindByID(&tRegistry, 2002);
	if ( pBucket != NULL ) {
		printf("find by tenant id => tenant=%s\n", pBucket->sTenantCode);
	}

	printf("ordered dump of tenant alpha:\n");
	procDumpTenantItems(procTenantRegistryFindByCode(&tRegistry, "alpha"));

	if ( !procTenantRegistryRemoveTenant(&tRegistry, "beta") ) {
		goto cleanup;
	}

	printf(
		"after remove beta => tenant_code_count=%u tenant_id_count=%u\n",
		(unsigned)xrtDictCount(tRegistry.pByTenantCode),
		(unsigned)xrtListCount(tRegistry.pByTenantID)
	);

	iRet = 0;

cleanup:
	procTenantRegistryUnit(&tRegistry);
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码里最关键的 5 个点

### 6.1 authoritative root 只有一处

这页里真正拥有 bucket 的是：

- `pByTenantCode`

而：

- `pByTenantID`

只是另一条 secondary index。

所以正常删除 tenant 时，收口顺序是：

1. 从 `list` 摘掉
2. 从 `dict` 移除并拿回 bucket
3. 销毁 bucket 自己的局部 `avltree`


### 6.2 tenant 外层和 tenant 内层是两种不同问题

tenant 外层要解决的是：

- 先找到哪个 tenant

tenant 内层要解决的是：

- 这个 tenant 下面的 key 怎样有序组织

所以这页故意不把所有查找问题都塞进一种容器。


### 6.3 `tenant_id` 不应该再回头拼成字符串 key

这是这页最想避免的一类退化写法。

如果系统里天然已经有：

- `tenant_id`

那就让它沿：

- `list`

这条整数索引主线工作，而不是继续把它硬拼成字符串再塞进字典。


### 6.4 tenant 内局部有序导出回到 `avltree`

`DemoTenantBucket` 自己拥有：

- `pByItemKey`

这意味着：

- 每个 tenant 都有自己的局部有序世界
- 删除整个 tenant 时，也能整体销毁这一层局部索引

这比继续维护一张巨大、全局、复合 key 的平面表更容易收口。


### 6.5 `hash` 在这里是稳定索引指纹

这页里：

- `iTenantHash`
- `iKeyHash`

承担的都是：

- 稳定标识
- 日志或导出里的轻量指纹

它们不是安全口令，也不是权限判定。


## 7. 接回 HTTP、页面和快照时怎么落

如果你把这套注册表接回前面的服务案例，主线会很自然：

### 7.1 `GET /api/tenants/{tenant}/items`

先通过：

- `dict`

找到 tenant bucket，再在 tenant 内用：

- `avltree`

导出有序资源列表。


### 7.2 `GET /api/tenants/by-id/{id}`

后台任务或运维入口如果拿到的是整数 ID，就直接走：

- `list`

而不是临时再查一遍 tenant code。


### 7.3 `DELETE /api/tenants/{tenant}`

真正的删除逻辑应该沿这条线：

1. 摘掉 `tenant_id` 二级索引
2. 从 authoritative root 移除 bucket
3. 销毁 bucket 的局部 `avltree`

不要让页面层或导出层自己参与释放。


### 7.4 快照和模板导出

快照或 HTML 面板最适合导出两层：

1. tenant 基本信息
2. tenant 内局部有序资源列表

这正好和：

- tenant root
- tenant 内部 `avltree`

两层结构对齐。


## 8. 这页最容易写错的地方

### 8.1 把所有维度都压成一个全局复合 key

这样短期看起来省事，长期会让：

- 整 tenant 删除
- tenant 内有序导出
- 二级索引反查

都越来越别扭。


### 8.2 让二级索引和 root 竞争所有权

`list` 和 `dict` 指向的是同一批 bucket，不代表它们都该负责释放。

这一点一旦不清楚，删除路径很容易重复 free 或漏 free。


### 8.3 忘记把 tenant 内索引挂到 bucket 自己身上

如果 tenant 内 `avltree` 没有跟着 bucket 走，后面系统一旦要：

- 删 tenant
- 导 tenant 快照
- 迁 tenant

代码就会开始到处找“这个 tenant 的局部节点到底散在哪”。


## 9. 这页之后最自然的下一步

这条主线往前走，最自然的下一步有 3 个：

1. 多租户缓存面板
	把 tenant bucket 继续接到缓存刷新页
2. 多租户批次任务中心
	把 tenant root 接到批处理页
3. 多租户快照导出
	把 tenant root + local ordered items 统一导成 JSON / HTML

如果你还没完全建立这里的容器心智，建议一起复读：

- [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](../guide/dict-intro.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](../guide/list-intro.md)
- [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](../guide/avltree-intro.md)
- [用 MemPool + AVLTree + List 组织一个会话索引与回收池](session-registry-pool-index.md)
