#include <xredis/tls.h>
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#include <xredis/tls_auth.h>
#endif
#include "../internal/client_connect.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static bool xredisTlsArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisTlsArgument("invalid TLS config output");
	memset(Config, 0, sizeof(*Config)); Config->Size = sizeof(*Config);
	xrtTlsClientConfigInit(&Config->Session); xrtTlsStreamConfigInit(&Config->Stream); xredisClientDialConfigInit(&Config->Dial);
	Config->MaxMetadataBytes = 64u * 1024u; Config->WriteBytes = 64u * 1024u; Config->ServerNameFromHost = true; return true;
}

bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan)
{
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || TLS->Size != sizeof(*TLS) ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Output, sizeof(*Output)) || TLS->MaxMetadataBytes == 0 || TLS->MaxMetadataBytes > 1024u * 1024u ||
		TLS->WriteBytes == 0 || TLS->WriteBytes > 1024u * 1024u || TLS->Stream.HandshakeTimeout < 0 || TLS->Stream.CloseTimeout < 0 ||
		TLS->Stream.AsyncBytesLimit == 0 || TLS->Stream.AsyncCountLimit == 0 || TLS->Stream.AsyncBatch == 0 || TLS->Session.ProtocolCount > 256u)
		return xredisTlsArgument("invalid TLS descriptor, bounds or output overlap");
	xtlsclientconfig Session = TLS->Session;
	if (TLS->ServerNameFromHost && Session.ServerName.Size == 0 && Host.Size != 0) {
		xerror* Saved = xrtTakeError(); xnetaddr Numeric;
		bool Literal = xrtNetAddrParse(&Numeric, Host.Data, 1); xrtSetErrorTake(Saved);
		if (!Literal) Session.ServerName = Host;
		else if (Session.VerifyName.Size == 0) Session.VerifyName = Host;
	}
	if (Session.ServerName.Size == 0 && Session.VerifyName.Size == 0) return xredisTlsArgument("TLS peer identity is required");
	size_t Bytes = Session.ProtocolCount * sizeof(xstrview);
	if (!xrtMemRangeValid(Session.Protocols, Bytes) || xrtMemRangesOverlap(Session.Protocols, Bytes, Output, sizeof(*Output)) || Bytes > TLS->MaxMetadataBytes)
		return xredisTlsArgument("invalid TLS ALPN descriptors");
	for (size_t Index = 0; Index < Session.ProtocolCount + 2u; ++Index) {
		xstrview View = Index == 0 ? Session.ServerName : Index == 1 ? Session.VerifyName : Session.Protocols[Index - 2u];
		if (View.Size > TLS->MaxMetadataBytes - Bytes || !xrtMemRangeValid(View.Data, View.Size) ||
			xrtMemRangesOverlap(View.Data, View.Size, Output, sizeof(*Output)) || (Index < 2u && View.Size != 0 && memchr(View.Data, 0, View.Size) != NULL))
			return xredisTlsArgument("invalid or excessive TLS name/ALPN bytes");
		Bytes += View.Size;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
	Plan->Session = Auth != NULL ? xrtTlsClientCreateAuthV1(&Session, Auth, NULL) : xrtTlsClientCreate(&Session, NULL);
#else
	(void)Auth; Plan->Session = xrtTlsClientCreate(&Session, NULL);
#endif
	if (Plan->Session == NULL) return false;
	Plan->Config = TLS->Stream; Plan->WriteBytes = TLS->WriteBytes; return true;
}

XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver, xstrview Host, uint16 Port, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{ return __xredisClientDialTls(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, xrtTimer(), Result); }

XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	if (Addresses == NULL) return xredisTlsArgument("TLS endpoint list is NULL");
	return __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	double Entered = xrtTimer();
	if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
		xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result))) return xredisTlsArgument("invalid TLS numeric endpoint/output");
	xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
	bool Success = __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Entered, Result);
	xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
