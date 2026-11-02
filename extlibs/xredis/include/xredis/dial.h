#ifndef XREDIS_DIAL_H
#define XREDIS_DIAL_H
#include <xredis/client.h>

#if defined(XREDIS_FEATURE_XREDIS_DIAL) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "XREDIS_FEATURE_XREDIS_DIAL requires client and net_tcp_dial"
#endif

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#define XREDIS_DIAL_ADDRESSES_MAX 64u
#define XREDIS_DIAL_HOST_MAX 65535u

/* Network policy only; the client Config.Stream remains the sole TCP limits.
 * Timeout=0 disables the additional Dial timer, -1 is infinite. Managed Init's
 * original deadline still spans host copying, DNS, candidate race and HELLO.
 * Default dual stack, 30000 ms, 250 ms fallback, eight candidates, 1024 host
 * bytes. Maximum candidate/address count 64; maximum borrowed host span 65535.
 * No IDNA conversion, TLS, reconnect or application-command replay is implied. */
typedef struct xredisclientdialconfig {
	size_t Size;
	xnetfamily Family;
	int64 Timeout;
	int64 FallbackDelay;
	uint32 MaxAttempts;
	size_t MaxHostBytes;
} xredisclientdialconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config);

/* Async hostname setup on the existing Engine/Resolver. Host is a bounded,
 * nonempty readable view without embedded NUL; copied before return. Resolver
 * is application-owned and must outlive Closed (then retire/join it separately).
 * It is shared; closing a client does not close the service or stop an OS
 * lookup already running there. All client callback/data ownership retires
 * through the client Close barrier. NULL Init selects strict RESP3 defaults.
 * Calls can queue, expire/cancel through DNS and TCP setup. Result unchanged on
 * failure. Successful submission owns one Client reference: Close then Release.
 * Core XRT supplies bounded family interleaving, fallback and losing-socket
 * retirement before transfer. No business operation is retried or replayed. */
XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);

/* Already resolved immutable endpoints; no Resolver is consulted. Each port
 * is preserved. At most 64 entries; Family filters candidates and MaxAttempts
 * bounds selected attempts. Retains the list only until native Dial admission;
 * caller may release its reference immediately. Output storage must be disjoint
 * from descriptors and borrowed host/credential/name/endpoint storage. */
XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine,
	xnetaddrlist* Addresses, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif
