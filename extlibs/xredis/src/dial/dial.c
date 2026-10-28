#include <xredis/dial.h>
#include "../internal/client_connect.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
typedef struct xredisconnectdata {
	xredisconnectplan Plan;
	xredisclient* Client; /* Borrowed through this source's retirement callback. */
	xnetresolver* Resolver;
	xnetaddrlist* Addresses;
	xnetdial* Dial;
	xnetdialconfig Config;
	uint16 Port;
	char Host[];
} xredisconnectdata;

static bool xredisDialArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisDialArgument("invalid Dial config output");
	*Config = (xredisclientdialconfig){sizeof(*Config), XNET_FAMILY_UNSPEC, 30000, 250, 8, 1024}; return true;
}

static void xredisDialDone(xnetdial* Dial, xnetresult Result, xnetstream* Stream, const xerror* Error, ptr Data)
{
	(void)Dial; xredisconnectdata* Context = (xredisconnectdata*)Data;
	__xredisClientConnectDone(Context->Client, Result, Stream, Error);
	/* Last access. This same Worker's queued client turn executes after the
	 * native callback's return; no fabricated owned/collector policy is used. */
	__xredisClientConnectRetired(Context->Client);
}

static bool xredisDialStart(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
	const xnetstreamconfig* Config, const xnetstreamevents* Events)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data; Context->Client = Client;
	Context->Config.Stream = *Config; Context->Config.Affinity = Affinity;
	if (Context->Addresses != NULL) Context->Dial = xrtNetDialAddresses(Engine, Context->Addresses, &Context->Config, Events, Client, xredisDialDone, Context);
	else Context->Dial = xrtNetDial(Engine, Context->Resolver, Context->Host, Context->Port, &Context->Config, Events, Client, xredisDialDone, Context);
	if (Context->Dial == NULL) return false;
	xrtNetAddrListDestroy(Context->Addresses); Context->Addresses = NULL; return true;
}

static void xredisDialCancel(ptr Data)
{ xredisconnectdata* Context = (xredisconnectdata*)Data; (void)xrtNetDialCancel(Context->Dial); }

static void xredisDialDrop(ptr Data)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data;
	xrtNetDialDestroy(Context->Dial); xrtNetAddrListDestroy(Context->Addresses); xrtFree(Context);
}

static bool xredisDialCreate(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	double Entered, xredisclient** Result)
{
	if (Engine == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Dial != NULL && (!xrtMemRangeValid(Dial, sizeof(*Dial)) || xrtMemRangesOverlap(Dial, sizeof(*Dial), Result, sizeof(*Result)))))
		return xredisDialArgument("invalid Dial arguments or config/output overlap");
	xredisclientdialconfig Policy;
	if (Dial == NULL) xredisClientDialConfigInit(&Policy); else Policy = *Dial;
	if (Policy.Size != sizeof(Policy) || (Policy.Family != XNET_FAMILY_UNSPEC && Policy.Family != XNET_FAMILY_IPV4 && Policy.Family != XNET_FAMILY_IPV6) ||
		Policy.Timeout < XRT_WAIT_FOREVER || Policy.FallbackDelay < 0 || Policy.MaxAttempts == 0 || Policy.MaxAttempts > XREDIS_DIAL_ADDRESSES_MAX ||
		Policy.MaxHostBytes == 0 || Policy.MaxHostBytes > XREDIS_DIAL_HOST_MAX) return xredisDialArgument("invalid Dial family, deadline or bounds");
	if (Addresses == NULL) {
		if (Resolver == NULL || Port == 0 || Host.Size == 0 || Host.Size > Policy.MaxHostBytes ||
			Host.Size > SIZE_MAX - sizeof(xredisconnectdata) - 1u || !xrtMemRangeValid(Host.Data, Host.Size) ||
			xrtMemRangesOverlap(Host.Data, Host.Size, Result, sizeof(*Result)) || memchr(Host.Data, 0, Host.Size) != NULL)
			return xredisDialArgument("invalid host, port, Resolver or input/output overlap");
	} else {
		size_t Count = xrtNetAddrListCount(Addresses), Eligible = 0;
		if (Count == 0 || Count > XREDIS_DIAL_ADDRESSES_MAX) return xredisDialArgument("resolved endpoint list exceeds bounds");
		for (size_t Index = 0; Index < Count; ++Index) {
			const xnetaddr* Address = xrtNetAddrListGet(Addresses, Index); if (Address == NULL) return false;
			if (xrtMemRangesOverlap(Address, sizeof(*Address), Result, sizeof(*Result))) return xredisDialArgument("endpoint output overlaps address storage");
			if (Policy.Family != XNET_FAMILY_UNSPEC && Address->Family != Policy.Family) continue;
			if (Address->Port == 0) return xredisDialArgument("resolved endpoint port is zero");
			++Eligible;
		}
		if (Eligible == 0) return xredisDialArgument("no endpoint matches the selected family");
	}
	xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	xredisconnectdata* Context = (xredisconnectdata*)xrtCalloc(1, sizeof(*Context) + Host.Size + 1u);
	if (Context == NULL) return false;
	Context->Plan = (xredisconnectplan){Context, xredisDialStart, xredisDialCancel, xredisDialDrop};
	xrtNetDialConfigInit(&Context->Config); Context->Config.Family = Policy.Family; Context->Config.Timeout = Policy.Timeout;
	Context->Config.FallbackDelay = Policy.FallbackDelay; Context->Config.MaxAttempts = Policy.MaxAttempts;
	Context->Resolver = Resolver; Context->Port = Port;
	if (Addresses != NULL) {
		Context->Addresses = xrtNetAddrListRef(Addresses);
		if (Context->Addresses == NULL) { xredisDialDrop(Context); return false; }
	} else memcpy(Context->Host, Host.Data, Host.Size);
	if (__xredisClientCreatePlanned(Engine, Affinity, Config, Init, Entered, &Context->Plan, Result)) return true;
	xredisDialDrop(Context); return false;
}

XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial, xredisclient** Result)
{ return xredisDialCreate(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, Dial, xrtTimer(), Result); }

XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclientdialconfig* Dial, xredisclient** Result)
{
	if (Addresses == NULL) return xredisDialArgument("resolved endpoint list is NULL");
	return xredisDialCreate(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, Dial, xrtTimer(), Result);
}
#endif
