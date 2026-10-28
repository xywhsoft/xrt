#ifndef XREDIS_CLIENT_CONNECT_INTERNAL_H
#define XREDIS_CLIENT_CONNECT_INTERNAL_H
#include <xredis/client.h>

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
