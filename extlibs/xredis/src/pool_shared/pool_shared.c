#include <xredis/pool_shared.h>
#include "../internal/pool_admit.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
XREDIS_API xredisadmission xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool)
{ return __xredisSharedPoolCreate(Engine, Affinity, Clients, Count, Config, Pool); }
XREDIS_API xredisadmission xredisSharedPoolCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolCall(Pool, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisSharedPoolCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return __xredisSharedPoolCall(Pool, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#endif
