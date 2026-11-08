#include <xredis/pool_watch.h>
#include "../internal/pool_admit.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), true); }
#endif
