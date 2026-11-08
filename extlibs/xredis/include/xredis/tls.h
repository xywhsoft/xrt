#ifndef XREDIS_TLS_H
#define XREDIS_TLS_H
#include <xredis/dial.h>
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#include <xrt/tls_stream.h>
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XREDIS_FEATURE_XREDIS_DIAL) || !defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME))
#error "XREDIS_FEATURE_XREDIS_TLS requires dial, TLS stream and verified/resumable TLS client"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384))
#error "XREDIS_FEATURE_XREDIS_TLS requires its declared AES/ChaCha, SHA256/SHA384 and X25519/P256/P384 backends"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* All views are borrowed only during the factory. Native TLS owns a snapshot
 * of names/ALPN and shared Context/Verifier/Resume references before return.
 * Supply a Verifier (or explicit ResumeOnly with a valid Resume); no implicit
 * insecure verification mode. Verifier callback code/context follow XRT's
 * shared-verifier lifetime, including Engine join after client Closed.
 * Defaults: native TLS policy/stream limits, normal Dial defaults, 64 KiB
 * metadata, 64 KiB plaintext write work per client turn, infer DNS SNI/name.
 * Numeric hosts infer VerifyName only; prepared endpoints need an explicit
 * ServerName or VerifyName. Init's original deadline spans TLS too. */
typedef struct xredisclienttlsconfig {
	size_t Size;
	xtlsclientconfig Session;
	xtlsstreamconfig Stream;
	xredisclientdialconfig Dial;
	size_t MaxMetadataBytes;
	size_t WriteBytes;
	bool ServerNameFromHost;
} xredisclienttlsconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config);
/* TLS is required. Result unchanged on rejection; no cleartext fallback or
 * operation replay. Typed readiness is published only after TLS + Redis setup.
 * Resolver is application owned through Closed, and joined separately. */
XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result);
XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
/* Numeric endpoint convenience; TLS identity must be explicit in TLS. */
XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif
