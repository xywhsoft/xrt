#include <xredis/pool_shared_asking.h>
#include "../internal/pool_admit.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
XREDIS_API xredisadmission xredisSharedPoolAskingCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolAsking(Pool, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisSharedPoolAskingCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolAsking(Pool, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#endif
