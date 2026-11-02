#ifndef XREDIS_TLS_AUTH_H
#define XREDIS_TLS_AUTH_H
#include <xredis/tls.h>
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#include <xrt/tls_client_auth.h>
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH) && (!defined(XREDIS_FEATURE_XREDIS_TLS) || !defined(XRT_FEATURE_TLS_CLIENT_AUTH))
#error "XREDIS_FEATURE_XREDIS_TLS_AUTH requires Redis TLS and native TLS client authentication"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
XRT_EXTERN_C_BEGIN
/* Configs/views are borrowed only during admission. The native session retains
 * the immutable identity before DNS or any worker callback; no caller identity
 * or auth-config borrow escapes. Signers run on the connection worker and must
 * not block it or reenter/destroy the native session. The identity's final
 * Release follows its existing last-reference thread contract. Resolver/code/
 * verifier-context lifetime and total deadlines follow the TLS Dial contract.
 * Auth is mandatory; defaults come from xrtTlsClientAuthConfigInitV1. With
 * RequireCertificate=true, no compatible server request fails readiness.
 * With false the caller explicitly permits a connection without local proof.
 * Full certificate handshakes only: explicit resumes are rejected and tickets
 * are not retained until identity-bound resumption is implemented. A signing
 * backend is selected separately; this module does not add private-key code.
 * Readiness includes the configured Redis initialization, so TLS 1.3 local
 * proof queueing alone cannot publish successful managed readiness. */
XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif
