#ifndef XREDIS_POOL_TRANSACTION_H
#define XREDIS_POOL_TRANSACTION_H
#include <xredis/pool.h>
#include <xredis/transaction.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_TRANSACTION))
#error "XREDIS_FEATURE_XREDIS_POOL_TRANSACTION requires pool and transaction"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XRT_EXTERN_C_BEGIN
/* Direct staged transaction on this lease. Accepted enqueue seals the lease
 * against further calls/transactions; earlier leased calls drain before MULTI.
 * Config/commands are borrowed only during admission. Output changes only on
 * ACCEPTED. Native xredistransactionresult preserves execution uncertainty and
 * queue/runtime errors. Group request/byte quotas include wire/control/state.
 * Deadline is capped at the original lease hold. Return the lease explicitly;
 * its physical Closed barrier waits for actual DISCARD/EXEC/drain/retirement.
 * Return never authorizes replay. Sync rejects all workers of this engine. */
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
