#ifndef XREDIS_POOL_WATCH_H
#define XREDIS_POOL_WATCH_H
#include <xredis/pool.h>
#include <xredis/watch.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_WATCH))
#error "XREDIS_FEATURE_XREDIS_POOL_WATCH requires pool and watch"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XRT_EXTERN_C_BEGIN
/* Nested WATCH scope on a physically idle lease. Busy returns OVERLOADED;
 * admission failure restores the unused lease. Acceptance consumes/seals this
 * lease for the scope; ordinary/transaction/second-WATCH admissions cannot
 * enter it. Result is the native xrediswatchresult on every accepted outcome,
 * including a failed start's physical Closed observer. All scope requests and
 * transaction/control metadata retain the pool's hard aggregate quota. Both
 * total scope/call deadlines are capped at the original lease hold.
 * Return/Abort/hold expiry/PoolClose cannot release a live nested owner. Return
 * requests bounded scope abandonment (UNWATCH when clean); grace expiry or
 * uncertain/busy cleanup retires the client. Explicitly Return the lease after
 * scope Closed. Retained completed scope/lease metadata pins no pool/engine.
 * Same input/output/ownership and sync-worker rules as standalone WATCH. */
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
