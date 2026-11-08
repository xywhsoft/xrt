#ifndef XREDIS_TLS_PEER_H
#define XREDIS_TLS_PEER_H
#include "dial_peer.h"
#include "../../../../tests/fixtures/tls_server.h"

/* Historical public test identity and explicit custom fixture trust. This
 * does not establish production chain/hostname validation interoperability. */
typedef struct redistlspeer {
	redisdialpeer Dial;
	redistlsoptions Options;
	xtlscontext* Context;
	xtlsidentity* Identity;
	xtlsverifier* Verifier;
	xatomic32 Verified;
	bool Reject;
} redistlspeer;

static xtlsverifydecision redisTlsVerify(const xtlspeer* Peer, ptr Data)
{
	redistlspeer* Fixture = (redistlspeer*)Data;
	redisPeerRequire(Peer != NULL && Peer->Role == XTLS_SERVER && Peer->CertificateCount != 0 &&
		Peer->Name.Size == 13 && memcmp(Peer->Name.Data, "redis.fixture", 13) == 0, "TLS copied peer identity");
	xrtAtomic32Store(&Fixture->Verified, 1, XMEMORY_RELEASE); return Fixture->Reject ? XTLS_VERIFY_REJECT : XTLS_VERIFY_ACCEPT;
}
static void redisTlsPeerCredentials(redistlspeer* Peer, redispeer* Endpoint, bool HoldHandshake, bool Reject)
{
	Peer->Reject = Reject;
	Peer->Context = testTlsServerContext(); Peer->Identity = testTlsServerIdentity();
	redisPeerRequire(Peer->Context != NULL && Peer->Identity != NULL, "TLS fixture policy/identity");
	xtlsverifierconfig Config; xrtTlsVerifierConfigInit(&Config); Config.Verify = redisTlsVerify; Config.Context = Peer;
	xtlsverifylimits Limits; xrtTlsVerifyLimitsInit(&Limits); Peer->Verifier = xrtTlsVerifierCreateLimited(&Config, &Limits);
	redisPeerRequire(Peer->Verifier != NULL, "TLS fixture explicit limited verifier");
	xrtTlsServerConfigInit(&Peer->Options.Session); Peer->Options.Session.Context = Peer->Context; Peer->Options.Session.Identity = Peer->Identity;
	xrtTlsStreamConfigInit(&Peer->Options.Stream); Peer->Options.Stream.CloseTimeout = 200; Peer->Options.HoldHandshake = HoldHandshake;
	xrtAtomicPtrStore(&Endpoint->TlsOptions, &Peer->Options, XMEMORY_RELEASE);
}
static inline void redisTlsPeerStart(redistlspeer* Peer, bool HoldHandshake, bool Reject, unsigned DnsFlags)
{
	memset(Peer, 0, sizeof(*Peer)); redisDialPeerStart(&Peer->Dial, DnsFlags, 0, 0);
	redisTlsPeerCredentials(Peer, &Peer->Dial.Init.Peer, HoldHandshake, Reject);
}
static inline void redisTlsPeerConfig(redistlspeer* Peer, xredisclienttlsconfig* Config)
{
	xredisClientTlsConfigInit(Config); Config->Session.Context = Peer->Context; Config->Session.Verifier = Peer->Verifier;
	Config->Session.VerifyName = (xstrview)XRT_STR_LITERAL("redis.fixture"); Config->Stream.CloseTimeout = 200;
}
static inline void redisTlsPeerCredentialsDrop(redistlspeer* Peer)
{ xrtTlsVerifierRelease(Peer->Verifier); xrtTlsIdentityRelease(Peer->Identity); xrtTlsContextRelease(Peer->Context); }
static inline void redisTlsPeerStop(redistlspeer* Peer)
{
	redisDialPeerStop(&Peer->Dial); redisTlsPeerCredentialsDrop(Peer);
}
#endif
