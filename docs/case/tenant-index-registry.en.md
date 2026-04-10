# XRT Case Study: A Multi-Tenant Index Registry with Dict, List, and AVLTree

> This case focuses on one practical multi-tenant registry mainline: one tenant root, one `tenant_id` secondary index, and one tenant-local ordered index. The point is not to prove that several containers can run together. The point is to make the boundaries explicit: which layer is the real tenant authoritative root, which layer is only a secondary lookup path, and when one tenant-local ordered tree is a better structure than forcing everything into one global composite-key dictionary.

[中文](tenant-index-registry.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building one local management panel or control service that must organize one class of resources by tenant.

The registry has six constraints:

1. business entry points naturally start from `tenant_code`
2. operations and background jobs often need reverse lookup by `tenant_id`
3. inside one tenant, resource keys still need ordered export
4. adding and removing a whole tenant must have one explicit teardown path
5. later you may still attach HTTP, templates, and snapshot export
6. the current root and the secondary indexes must not collapse into one unclear structure

Without one clear mainline, the code quickly degrades into:

- one global `dict` with forced keys like `tenant:key`
- full-table prefix scans when one tenant must be removed
- `tenant_code`, `tenant_id`, and tenant-local item lookup fighting each other
- pages and exports depending more and more on temporary string composition

So the real question here is:

> once the system grows from a flat single-tenant set into a tenant root plus tenant-local indexes, how do you separate the root, the secondary indexes, and the tenant-local ordering boundary formally?


## 2. Why This Is Not "One Global Dict Solves Everything"

### 2.1 A global composite key only solves lookup, not tenant structure

You can absolutely use:

- `tenant:key`

as one global `dict` key.

That solves:

- which object belongs to one full composite key

But it does not naturally solve:

- where the tenant root itself lives
- how to find a tenant quickly by `tenant_id`
- how to tear down one whole tenant-local index
- how to export tenant-local items in order

That is exactly why this page splits the registry into three layers.


### 2.2 `dict` owns the authoritative tenant root

The most natural root here is:

- `tenant_code -> DemoTenantBucket*`

because most external business entry points first need to locate one tenant.

So in this case:

- `dict`
	- is the tenant authoritative root

instead of being forced to carry every dimension, including tenant-local ordered items.


### 2.3 `list` is the natural secondary index for `tenant_id`

Once the system already has one natural integer key:

- `tenant_id`

the stable secondary index is:

- `list`

because it directly expresses:

- `tenant_id -> bucket`

without forcing that integer back into a string key path.


### 2.4 Tenant-local ordered export should return to `avltree`

Inside one tenant, the problem changes again:

- item keys need ordered export
- the key is not just one integer
- the tenant wants one local ordered structure

That is no longer a reason to keep layering more global `dict` keys.

It is a reason to return to:

- `avltree`

So the real design rule is:

> solve the tenant root and secondary lookup at the outer layer, then solve ordered tenant-local structure inside the bucket itself.


## 3. What Each Layer Owns

| Layer | Role in this case | What it actually solves |
|------|-------------------|-------------------------|
| `dict` | `tenant_code -> bucket` authoritative root | fast tenant lookup by code |
| `list` | `tenant_id -> bucket` secondary index | integer reverse lookup and background-job entry |
| `avltree` | tenant-local `item_key -> item` ordered index | ordered export and tenant-local lookup |
| `hash` | stable fingerprints for `tenant_code / item_key` | lightweight identifiers in logs and exports |

One-sentence version:

> `dict` gets you to the tenant, `list` gives you the integer-side secondary lookup, and `avltree` owns the ordered local world inside each tenant.


## 4. Ownership Boundaries

This page keeps ownership deliberately explicit:

- `DemoTenantRegistry`
	- owns one `dict`
	- owns one `list`
- `dict`
	- acts as the authoritative root
	- stores and owns `DemoTenantBucket*`
- `list`
	- stores the same `DemoTenantBucket*`
	- does not own the bucket
- `DemoTenantBucket`
	- owns its own `xavltree`
- `xavltree`
	- directly stores tenant-local `DemoTenantItem`

So the real teardown line is:

1. detach the bucket from the secondary index
2. remove it from the authoritative root
3. destroy the bucket-local ordered tree


## 5. Compact Multi-Tenant Skeleton

The reviewed Chinese page contains the fully expanded explanation. The English page keeps the formal mainline compact and source-aligned.

This skeleton does seven things:

1. create one tenant registry built from `dict + list`
2. add two tenants
3. insert tenant-local items under different tenants
4. support lookup by both `tenant_code` and `tenant_id`
5. export one tenant in item-key order
6. update one existing tenant-local item
7. remove one whole tenant through one explicit teardown path

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


## 6. Five Things That Matter Most

### 6.1 There is only one authoritative root

In this page, the structure that really owns the bucket is:

- `pByTenantCode`

while:

- `pByTenantID`

is only the secondary lookup path.

So normal tenant teardown becomes:

1. detach from the `list`
2. remove from the `dict` and take the bucket back
3. destroy the bucket-local ordered tree


### 6.2 The outer tenant layer and the inner tenant layer are different problems

The outer tenant layer answers:

- which tenant are we talking about

The inner tenant layer answers:

- how should that tenant-local item space stay ordered

That is why this page does not force every lookup dimension into one container shape.


### 6.3 `tenant_id` should not be forced back into string keys

This is the main anti-pattern the page wants to avoid.

Once the system already has a natural integer key:

- `tenant_id`

the stable path is:

- `list`

instead of turning that integer back into another string key.


### 6.4 Tenant-local ordered export belongs to the bucket-local `avltree`

Each `DemoTenantBucket` owns:

- `pByItemKey`

That means:

- every tenant has its own local ordered world
- removing one whole tenant can also tear down that whole local ordered layer cleanly


### 6.5 `hash` is only a stable index fingerprint here

In this page:

- `iTenantHash`
- `iKeyHash`

are only:

- stable identifiers
- lightweight fingerprints for export or logging

They are not security primitives.


## 7. How It Reconnects to HTTP, Templates, and Snapshots

If you attach this registry back to the earlier service cases, the mainline becomes straightforward.

### 7.1 `GET /api/tenants/{tenant}/items`

First use:

- `dict`

to locate the tenant bucket, then use:

- `avltree`

to export the tenant-local items in order.


### 7.2 `GET /api/tenants/by-id/{id}`

If operations or background jobs start from an integer tenant ID, they should go straight through:

- `list`

instead of rebuilding that path through tenant-code lookups.


### 7.3 `DELETE /api/tenants/{tenant}`

The teardown path should stay explicit:

1. detach the `tenant_id` secondary index
2. remove the bucket from the authoritative root
3. destroy the bucket-local ordered tree

The page layer or export layer should never participate in ownership teardown.


### 7.4 Snapshot and HTML export

Snapshots or template pages naturally export two layers:

1. tenant summary fields
2. the tenant-local ordered item list

That maps directly onto:

- the tenant root
- the bucket-local `avltree`


## 8. Common Mistakes

### 8.1 Flattening every dimension into one global composite key

That looks convenient at first, but it makes:

- whole-tenant removal
- tenant-local ordered export
- secondary integer lookup

harder over time.


### 8.2 Letting the secondary index compete for ownership

`list` and `dict` point to the same buckets. That does not mean both structures should free them.


### 8.3 Forgetting that the tenant-local index belongs on the bucket

If the tenant-local `avltree` does not travel with the bucket, tenant deletion, tenant export, and tenant migration all become much harder to reason about.


## 9. What To Read Next

The next natural directions from this mainline are:

1. multi-tenant cache dashboards
2. multi-tenant batch centers
3. multi-tenant snapshot export

If the container boundaries are still not fully stable yet, read these together:

- [Dict Intro: When to Use a Keyed Dictionary Instead of Arrays, Lists, or Hand-Rolled Trees](../guide/dict-intro.md) (Chinese for now)
- [List Intro: When to Use an Integer-Keyed Sparse Mapping Instead of Arrays or Dict](../guide/list-intro.md) (Chinese for now)
- [AVLTree Intro: When to Use a General Ordered Tree Instead of Dict, List, or Arrays](../guide/avltree-intro.md) (Chinese for now)
- [Session Registry with `mempool + avltree + list`](session-registry-pool-index.en.md)
