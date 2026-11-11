#ifndef XREDIS_POOL_CONNECT_INTERNAL_H
#define XREDIS_POOL_CONNECT_INTERNAL_H
#include <xredis/pool_connect.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result);
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
xredisadmission __xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result);
#endif
#endif
#endif
