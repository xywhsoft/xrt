# XRT Case Study: Session Registry with MemPool, AVLTree, and List

> A practical registry model for session objects, variable-length strings, two index layers, and one defensive sweep. The point is not to prove that containers can run. The point is to make the boundaries explicit: which layer is the real live root, which layer is only a secondary index, when to use explicit `Remove + Free`, and when to add one `GC(FALSE)` sweep.

[中文](session-registry-pool-index.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a long-lived gateway or an agent scheduler and need one active-session registry.

That registry has 6 constraints:

1. sessions must be found quickly by `session_id`
2. auth, refresh, and forced logout paths must also find sessions by `token`
3. operations want an ordered export by `token`
4. session objects contain fixed fields plus variable-length `token / user_id / display_name`
5. preload, hot-restore, or rollback may leave pool objects that were allocated but never committed into the live root
6. the same runtime boundaries must stay valid on Windows and Linux

Without one explicit mainline, this usually degrades into:

- `malloc/free` scattered everywhere
- `session_id` and `token` each pretending to own the object lifetime
- unclear delete order between the two tables
- failed recovery paths that rely on manual cleanup

This case solves the exact problem of how to split one session registry into a stable indexing and reclamation model on top of the current XRT mainline.

---

## 2. Why This Mainline

### Why not just use `malloc/free`?

Because the real problem is not "allocate three objects and exit".

The real problems are:

- sessions are created and removed continuously
- string lengths vary
- one business chain keeps producing many small and medium allocations
- failed recovery paths still need one defensive cleanup route

So this page intentionally starts from:

- `xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL)`

instead of dropping back into ad hoc `malloc/free`.

### Why not use `dict` for the token side?

`dict` can absolutely do string-key lookup. That is not the point.

This case also wants:

- ordered walks by token
- stable export order
- one explicit secondary index tree

That is exactly where:

- `avltree`

fits more naturally.

### Why not use an array, or another `avltree`, for `session_id`?

Because `session_id` is naturally:

- an `int64` key
- possibly sparse
- possibly non-contiguous

That is the most direct mainline for:

- `list`

You could build another `avltree`, but then an already-fixed integer-key problem gets turned back into a general compare-function problem.

### Why is `GC(FALSE)` not a replacement for explicit `Remove + Free`?

This is one of the most important boundaries in the whole page.

For normal disconnect, forced logout, and expiry, the stable path is still:

1. remove the secondary index entry
2. remove the live-root entry
3. free the session object and the pool-owned strings

Meanwhile:

- `xrtMemPoolGC(objMP, FALSE)`

only solves a different class of problems:

- orphaned pool objects left by failed recovery
- temporary objects created during preload or import but never committed into the live root

So in this model, GC is:

- a defensive sweep

not:

- a replacement for normal explicit deletion

---

## 3. What Each Layer Does

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| `mempool` | one allocation domain for session objects and variable-length strings | fixed fields and variable fields converge into one pool lifecycle |
| `list` | the authoritative `session_id -> session*` live root | sparse integer-key mapping for active objects |
| `avltree` | ordered secondary `token -> session*` index | token lookup and ordered token export |
| `list walk + GC(FALSE)` | mark the live root, then sweep pool objects that never entered it | one cleanup path for failed recovery leftovers |

One-sentence version:

> `list` answers whether a session is still alive, `avltree` answers how to find it by token, and `mempool` keeps the object and its strings inside one unified allocation domain.

---

## 4. Ownership Model

This page intentionally keeps ownership explicit:

- `DemoSession`
	- is allocated from `mempool`
	- owns `sToken / sUserID / sDisplayName`
- `list`
	- stores only `DemoSession*`
	- acts as the live root
- `avltree`
	- stores only `DemoSession*` inside its nodes
	- does not own the session object
	- only acts as a token index

That means the real ownership exists in exactly one place:

- the `DemoSession` object itself

The two index structures do not compete for release responsibility.

---

## 5. Code Skeleton

The Chinese page contains the longer walkthrough. The code below keeps the same boundaries and is complete enough to compile as one example:

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

---

## 6. The 5 Most Important Boundaries

### 6.1 `list` is the authoritative live root

This page intentionally marks reachability only from:

- `pBySessionID`

The reason is not that `avltree` is unimportant. The reason is that the model must keep the root explicit:

- `list`
	- means "these sessions are still alive"
- `avltree`
	- is only a secondary token index for those live sessions

If both structures are treated as roots, it becomes much harder to say where sweep should start.

### 6.2 `avltree` nodes do not own session objects

This page does not bind:

- `pByToken->FreeProc`

to session release logic.

The token tree only exists to:

- organize the secondary index

It should not own:

- `DemoSession`

Otherwise, delete paths on the token side and the live-root side start fighting over release responsibility.

### 6.3 Normal disconnect still uses explicit `Remove + Free`

`procRegistryRemoveSession()` does this:

1. find the live session by `session_id`
2. remove the token-side secondary index
3. remove the live-root entry from `list`
4. explicitly release `sToken / sUserID / sDisplayName / DemoSession`

That is the normal mainline. Normal disconnect, expiry, and forced logout should still converge through explicit cleanup.

### 6.4 `GC(FALSE)` only cleans pool orphans that never entered the root

This page intentionally creates:

- `pOrphan = procAllocSession(...)`

without inserting it into either index.

That simulates:

- preload failure
- restore rollback
- an interrupted object before commit into the live root

Then:

- `procRegistrySweep()`

can use:

- `list walk -> mark live -> xrtMemPoolGC(FALSE)`

to reclaim those pool orphans.

### 6.5 `mempool` is intentionally used for both objects and variable strings

If this were only fixed-size structs, the better fit might be:

- `fsmempool`

But the real shape here is:

- fixed-size session structs
- variable-length `token / user_id / display_name`

That is exactly where `mempool` becomes the most natural option:

- one pool lifecycle for small objects and variable strings together

---

## 7. Where This Model Fits Best

This model fits well for:

- long-lived gateway session registries
- active-task registries in agent or worker systems
- online state tables with both integer IDs and auth tokens
- restore, snapshot rebuild, and batch-import flows that may generate partial objects before commit

It is not a good fit for:

- tiny scripts with only a few short-lived objects
- designs where several index tables are all treated as strong owners
- pure fixed-size workloads with no variable-length strings

---

## 8. Common Mistakes

### Mistake 1: letting `avltree` and `list` both own the session object

Then delete paths will compete over who frees the object.

### Mistake 2: treating `GC(FALSE)` as the normal delete path

Normal session removal should still go through explicit `Remove + Free`. GC here is a recovery-side defense.

### Mistake 3: marking the session object but not its pool-owned strings

`mempool` does not walk the object graph for you.

### Mistake 4: building another general compare tree for `session_id`

That makes a straightforward integer live root more complex than necessary.

### Mistake 5: duplicating the whole session object inside token-tree nodes

This page intentionally keeps token-tree nodes at:

- `DemoSession*`

because the tree is an index, not another owner of full object copies.

---

## 9. Suggested Reading

- [Guide: MemPool Intro](../guide/mempool-intro.md) (Chinese for now)
- [Guide: AVLTree Intro](../guide/avltree-intro.md) (Chinese for now)
- [Guide: List Intro](../guide/list-intro.md) (Chinese for now)
- [MemPool API](../api/api-mempool.en.md)
- [AVLTree API](../api/api-avltree.en.md)
- [List API](../api/api-list.en.md)

---

**One-sentence summary:** the key is not "use three containers". The key is to keep `list` as the explicit live root, keep `avltree` as the token-side secondary index, let `mempool` own both the object and its variable strings, use explicit `Remove + Free` for normal deletion, and reserve `GC(FALSE)` for defensive cleanup of uncommitted pool objects.
