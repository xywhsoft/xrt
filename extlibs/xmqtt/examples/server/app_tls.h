#ifndef XMQTT_EXAMPLE_SERVER_TLS_SETUP_H
#define XMQTT_EXAMPLE_SERVER_TLS_SETUP_H
#include <xrt/tls_server.h>
#include <xrt/tls_identity.h>
#include <stdio.h>

static size_t exampleServerReadDer(const char* Path, unsigned char* Data, size_t Capacity)
{
	FILE* File = fopen(Path, "rb");
	if (!File) return 0;
	size_t Bytes = fread(Data, 1, Capacity, File);
	bool Complete = feof(File) && !ferror(File);
	fclose(File);
	return Complete ? Bytes : 0;
}

static bool exampleServerLoadTls(xtlsserverconfig* Tls, const char* Certificate, const char* PrivateKey)
{
	unsigned char Cert[8192], Key[8192];
	size_t CertBytes = exampleServerReadDer(Certificate, Cert, sizeof(Cert));
	size_t KeyBytes = exampleServerReadDer(PrivateKey, Key, sizeof(Key));
	xbytesview Leaf = {Cert, CertBytes};
	xtlsidentity* Identity = CertBytes && KeyBytes ?
		xrtTlsIdentityRsa(&Leaf, 1, (xbytesview){Key, KeyBytes}) : NULL;
	for (volatile unsigned char* Cursor = Key; Cursor != Key + sizeof(Key); ++Cursor) *Cursor = 0;
	if (!Identity) return false;

	static const xtlsversion Versions[] = {XTLS_VERSION_13};
	static const xtlscipher Ciphers[] = {XTLS_AES_128_GCM_SHA256};
	static const uint16 Groups[] = {XTLS_GROUP_X25519};
	static const xtlssignature Signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
	xtlspolicy Policy;
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = 1;
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = 1;
	Policy.Groups = Groups;
	Policy.GroupCount = 1;
	Policy.Signatures = Signatures;
	Policy.SignatureCount = 1;
	xtlscontextconfig ContextConfig;
	xrtTlsContextConfigInit(&ContextConfig);
	ContextConfig.Policy = &Policy;
	xtlscontext* Context = xrtTlsContextCreate(&ContextConfig);
	if (!Context) {
		xrtTlsIdentityRelease(Identity);
		return false;
	}
	xrtTlsServerConfigInit(Tls);
	Tls->Context = Context;
	Tls->Identity = Identity;
	return true;
}

static void exampleServerReleaseTls(const xtlsserverconfig* Tls)
{
	/* LoadTls owns these objects even though the TLS config borrows them. */
	xrtTlsContextRelease((xtlscontext*)Tls->Context);
	xrtTlsIdentityRelease((xtlsidentity*)Tls->Identity);
}
#endif
