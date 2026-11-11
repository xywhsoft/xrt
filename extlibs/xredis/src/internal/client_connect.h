#ifndef XREDIS_CLIENT_CONNECT_INTERNAL_H
#define XREDIS_CLIENT_CONNECT_INTERNAL_H
#include <xredis/client.h>
typedef struct xredisexclusiveowner xredisexclusiveowner;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
#include <xrt/atomic.h>
enum { XREDIS_OWNER_STARTING, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED,
    XREDIS_OWNER_CLOSING, XREDIS_OWNER_CLOSED };
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredisownerquota {
    xatomic64 Requests, Bytes;
    xatomic32 Closing;
    uint64 MaxRequests, MaxBytes;
} xredisownerquota;
#endif
/* Embedded in a session or pool slot. Retain/Release are atomic-only; Wake is called
 * outside the client Gate. The owner holds a client ref and an engine Pin
 * until Unclaim, including the physical retirement of request constructors.
 * Deadline is published before ACTIVE, and changed only while sealed, idle
 * and without API constructors from the previous generation. */
struct xredisexclusiveowner {
    ptr Data;
    xatomic32 State;
    double Deadline;
    void (*Retain)(ptr Data);
    void (*Wake)(ptr Data);
    void (*Release)(ptr Data);
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisownerquota* Quota;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent;
    void (*Abandon)(ptr Data);
    xatomic32 Nested;
#endif
};
xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine);
#endif
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientIsWorker(xredisclient* Client);
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered,
    bool Control, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
xredisadmission __xredisClientOwnedAsking(xredisclient* Client,xredisexclusiveowner* Owner,
    const xbytesview* Arguments,size_t Count,int64 Timeout,double Entered,xfuture** Future);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#include <xredis/watch.h>
xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output);
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon);
void __xredisClientChildDone(xredisexclusiveowner* Owner);
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered);
#endif
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#include <xredis/transaction.h>
xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
xredisadmission __xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#endif
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
