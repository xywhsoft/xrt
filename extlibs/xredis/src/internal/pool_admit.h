#ifndef XREDIS_POOL_ADMIT_INTERNAL_H
#define XREDIS_POOL_ADMIT_INTERNAL_H
#include <xredis/pool.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#include <xredis/transaction.h>
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#include <xredis/watch.h>
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync);
#endif
#endif
