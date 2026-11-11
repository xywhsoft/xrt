#include <xredis/pool_shared_connect.h>
#include "../internal/pool_connect_internal.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
XREDIS_API xredisadmission xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result)
{ return __xredisSharedPoolConnect(Engine, Affinity, Count, Config, Connect, Result); }
#endif
