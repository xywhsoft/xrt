#ifndef XREDIS_CLIENT_CONNECT_INTERNAL_H
#define XREDIS_CLIENT_CONNECT_INTERNAL_H
#include <xredis/client.h>
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#include <xredis/transaction.h>
xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
xredisadmission __xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#endif
typedef struct xredisclienttlsconfig xredisclienttlsconfig;
typedef struct xtlsclientauthconfigv1 xtlsclientauthconfigv1;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#include <xrt/tls_stream.h>
typedef struct xredistlsplan {
	xtlssession* Session;
	xtlsstreamconfig Config;
	size_t WriteBytes;
} xredistlsplan;
bool __xredisClientDialTls(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, double Entered, xredisclient** Result);
bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan);
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
bool __xredisClientDialTlsAuthV1(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
	const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result);
#endif
/* Consumes TCP always, and Session only on true. Owning Worker only. */
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan);
const xnetstreamevents* __xredisClientTlsPendingEvents(void);
#endif

/* Private cold connection-start handoff. No per-request dispatch, public
 * transport plugin or shared runtime. The descriptor lives in Data's owner.
 * Factory failure consumes nothing; success drops it at physical Close. */
typedef struct xredisconnectplan {
	ptr Data;
	bool (*Start)(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
		const xnetstreamconfig* Config, const xnetstreamevents* Events);
	void (*Cancel)(ptr Data);
	void (*Drop)(ptr Data);
} xredisconnectplan;

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result);
/* Owning Worker only. Done consumes any winning Stream reference, then the
 * source calls Retired as its final action before returning from its callback.
 * The queued client work cannot publish Close until that callback returns. */
void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause);
void __xredisClientConnectRetired(xredisclient* Client);
#endif
#endif
