#include <xredis/pool_transaction.h>
#include "../internal/pool_admit.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), true); }
#endif
