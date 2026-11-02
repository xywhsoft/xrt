#include <xredis/pool_connect.h>
#include "../internal/pool_connect_internal.h"
#include <xrt/memory.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid pool connector config output"); return false;
    }
    *Config = (xredispoolconnectconfig){sizeof(*Config), NULL, NULL, NULL,
        XREDIS_RESP3, 4, 5000, 100, 10000}; return true;
}
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{ return __xredisPoolConnect(Engine, Affinity, Count, Config, Connect, Result); }
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{ return __xredisPoolConnectInfo(Pool, Info); }
#endif
