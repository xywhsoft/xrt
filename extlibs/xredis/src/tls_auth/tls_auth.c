#include <xredis/tls_auth.h>
#include "../internal/client_connect.h"
#include <xrt/memory.h>

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
static bool xredisTlsAuthArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    return __xredisClientDialTlsAuthV1(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    if (Addresses == NULL) return xredisTlsAuthArgument("TLS authentication endpoint list is NULL");
    return __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    double Entered = xrtTimer();
    if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
        xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))
        return xredisTlsAuthArgument("invalid TLS authentication numeric endpoint/output");
    xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
    bool Success = __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, Entered, Result);
    xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
