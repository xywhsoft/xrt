#ifndef XRPC_PENDING_H
#define XRPC_PENDING_H
#include <xrpc/id.h>

#if defined(XRPC_FEATURE_XRPC_PENDING) && !defined(XRPC_FEATURE_XRPC_ID)
#error "XRPC_FEATURE_XRPC_PENDING requires id"
#endif
#if defined(XRPC_FEATURE_XRPC_PENDING)
typedef struct xrpcpending xrpcpending;
typedef struct xrpcpendingconfig {
	size_t Size, MaxCalls, MaxOwnedBytes;
	xrpcidconfig Id;
} xrpcpendingconfig;
typedef struct xrpcpendingtoken { uint64 Serial; size_t Slot; } xrpcpendingtoken;
typedef struct xrpcpendingcall {
	xrpcpendingtoken Token;
	uint64 Deadline;
	ptr User;
} xrpcpendingcall;
typedef struct xrpcpendinginfo {
	size_t Size, Capacity, Count, OwnedBytes;
	uint64 NextDeadline;
} xrpcpendinginfo;
typedef enum xrpcpendingstatus {
	XRPC_PENDING_ERROR = -1, XRPC_PENDING_MISSING = 0, XRPC_PENDING_OK = 1,
	XRPC_PENDING_DUPLICATE, XRPC_PENDING_FULL
} xrpcpendingstatus;
typedef enum xrpcpendingerror {
	XRPC_PENDING_ARGUMENT = 5001, XRPC_PENDING_LIMIT, XRPC_PENDING_MEMORY
} xrpcpendingerror;
XRT_EXTERN_C_BEGIN
/* Defaults: 1024 calls, 2 MiB requested owned bytes; ID input 4096, decoded
 * string 256, key 512 bytes. All limits positive. No engine/transport dependency. */
XRPC_API bool xrpcPendingConfigInit(xrpcpendingconfig* Config);
/* One allocation for entries, fixed key slots, deadline heap and shared key
 * scratch; no per-operation ordinary allocation. MaxOwnedBytes includes these
 * requested bytes, not allocator headers/caches. Failure leaves Result unchanged.
 * Config is copied. Every operation, inspection and destruction on one table
 * MUST be serialized by its owner; no internal locking, callbacks or threads.
 * The runtime can confine this table to its connection/worker owner. */
XRPC_API bool xrpcPendingCreate(const xrpcpendingconfig* Config, xrpcpending** Result);
/* Does not destroy User pointers or complete calls. Drain with TakeFirst before
 * destroying when the owner needs to complete every outstanding call. NULL OK. */
XRPC_API void xrpcPendingDestroy(xrpcpending* Pending);
XRPC_API bool xrpcPendingInfo(const xrpcpending* Pending, xrpcpendinginfo* Info);
/* Independently validate/canonicalize Id, copy its comparison key and borrow
 * User (NULL allowed). No raw ID retained: the owner keeps original wire IDs.
 * Deadlines use one caller-chosen monotonic tick domain; UINT64_MAX means never.
 * Full/duplicate are ordinary outcomes, preserving outputs and ambient error;
 * duplicate wins over full. Invalid IDs/config/spans are local errors even full.
 * Success assigns a never-reused nonzero Serial and zero-based Slot. Tokens are
 * scoped to this table lifetime; serial exhaustion fails rather than wrapping.
 * Live IDs use exact xrpcIdKey equality, including null and signed numeric zero.
 * Input/output must not overlap each other or table-owned storage. */
XRPC_API xrpcpendingstatus xrpcPendingInsert(xrpcpending* Pending, xstrview Id,
	uint64 Deadline, ptr User, xrpcpendingtoken* Token);
/* All Take/Expire operations remove before publishing one record. Only OK
 * writes Call; missing/error leave it unchanged. They never run business code.
 * After removal, duplicate replies and stale local tokens are missing, including
 * a stale token after slot reuse. Concurrent response/cancel/expiry arbitration
 * belongs on the serialized owner. Call output cannot overlap table/input.
 * Reusing a removed wire ID CAN match an old response to a new call: tokens only
 * prevent local ABA. The runtime must use fresh wire IDs/connection epochs and
 * reject ineligible envelopes before calling TakeId. This leaf takes an ID,
 * not a response document; it cannot validate envelope role or batch atomicity. */
XRPC_API xrpcpendingstatus xrpcPendingTakeId(xrpcpending* Pending, xstrview Id,
	xrpcpendingcall* Call);
XRPC_API xrpcpendingstatus xrpcPendingTakeToken(xrpcpending* Pending,
	xrpcpendingtoken Token, xrpcpendingcall* Call);
/* Remove the earliest finite deadline <= Now; never-deadline entries stay live
 * even for Now=UINT64_MAX. Equal deadlines use admission serial order. */
XRPC_API xrpcpendingstatus xrpcPendingExpire(xrpcpending* Pending, uint64 Now,
	xrpcpendingcall* Call);
/* Remove earliest deadline, including never, for bounded shutdown/draining. */
XRPC_API xrpcpendingstatus xrpcPendingTakeFirst(xrpcpending* Pending, xrpcpendingcall* Call);
/* Lookup uses a balanced comparison tree, not an attacker-controlled hash
 * probe chain: O(MaxKeyBytes * log(MaxCalls)); deadline heap O(log(MaxCalls)),
 * token slot check O(1) before removal. Mutations may recurse on AVL tree height.
 * Valid operations have no table allocation; JSON/error reference bookkeeping
 * can synchronize and cold diagnostics can allocate. Caller views remain alive
 * and unchanged throughout calls; no view/callback is retained. */
XRT_EXTERN_C_END
#endif
#endif
