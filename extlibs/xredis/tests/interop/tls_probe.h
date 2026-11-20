/* Shared TLS setup for the native Redis interop probes. Include after require(). */
#ifndef XREDIS_INTEROP_TLS_PROBE_H
#define XREDIS_INTEROP_TLS_PROBE_H

typedef struct redisprobetls {
	bool Enabled;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xredisclienttlsconfig Config;
	xtlscontext* Context;
	xtlsverifier* Verifier;
#endif
} redisprobetls;

static void redisProbeTlsInit(redisprobetls* Tls, cstr CaFile)
{
	if (!CaFile) return;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	unsigned char Pem[65536];
	FILE* File = fopen(CaFile, "rb");
	require(File != NULL, "open TLS trust file");
	size_t Size = fread(Pem, 1, sizeof(Pem), File);
	bool Complete = Size && Size < sizeof(Pem) && !ferror(File);
	require(fclose(File) == 0 && Complete, "read TLS trust file");
	xx509store* Store = xrtX509StoreCreate();
	require(Store && xrtX509StoreAddData(Store, (xbytesview){Pem, Size}, NULL), "load TLS trust anchor");
	xtlsverifierconfig Verify;
	xrtTlsVerifierConfigInit(&Verify);
	Verify.Store = Store;
	xtlsverifylimits Limits;
	xrtTlsVerifyLimitsInit(&Limits);
	Tls->Verifier = xrtTlsVerifierCreateLimited(&Verify, &Limits);
	xrtX509StoreFree(Store);
	const xtlsversion Versions[] = {XTLS_VERSION_13};
	const xtlscipher Ciphers[] = {XTLS_AES_128_GCM_SHA256};
	const uint16 Groups[] = {XTLS_GROUP_X25519};
	const xtlssignature Signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
	xtlspolicy Policy;
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions; Policy.VersionCount = 1;
	Policy.Ciphers = Ciphers; Policy.CipherCount = 1;
	Policy.Groups = Groups; Policy.GroupCount = 1;
	Policy.Signatures = Signatures; Policy.SignatureCount = 1;
	xtlscontextconfig Settings;
	xrtTlsContextConfigInit(&Settings);
	Settings.Policy = &Policy;
	Tls->Context = xrtTlsContextCreate(&Settings);
	require(Tls->Verifier && Tls->Context && xredisClientTlsConfigInit(&Tls->Config), "fixture TLS configuration");
	Tls->Config.Session.Context = Tls->Context;
	Tls->Config.Session.Verifier = Tls->Verifier;
	Tls->Config.Session.ServerName = Tls->Config.Session.VerifyName = XRT_STR_LITERAL("localhost");
	Tls->Config.Session.ResumeLimit = 0;
	Tls->Enabled = true;
#else
	(void)Tls;
	require(false, "fixture built without Redis TLS");
#endif
}

static void redisProbeTlsClose(redisprobetls* Tls)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xrtTlsContextRelease(Tls->Context);
	xrtTlsVerifierRelease(Tls->Verifier);
#else
	(void)Tls;
#endif
}

static bool redisProbeConnect(const redisprobetls* Tls, xnetengine* Engine, const xnetaddr* Address,
	size_t Slot, const xredisclientinitconfig* Init, xredisclient** Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Tls->Enabled) return xredisClientTlsConnect(Engine, Address, Slot, NULL, Init, &Tls->Config, Client);
#else
	(void)Tls;
#endif
	return xredisClientConnectInit(Engine, Address, Slot, NULL, Init, Client);
}

#endif
