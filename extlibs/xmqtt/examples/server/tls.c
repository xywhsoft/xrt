#include "app_listener.h"
#include <xrt/tls_identity.h>
#include <stdio.h>

static size_t readDer(const char* Path, unsigned char* Data, size_t Capacity)
{
	FILE* File = fopen(Path, "rb");
	if (!File) return 0;
	size_t Bytes = fread(Data, 1, Capacity, File);
	bool Complete = feof(File) && !ferror(File);
	fclose(File);
	return Complete ? Bytes : 0;
}

int main(int Argc, char** Argv)
{
	if (Argc != 3) {
		puts("usage: examples_server_tls certificate.der rsa-private-key.der");
		return Argc == 1 ? 0 : 1;
	}
	unsigned char Cert[8192], Key[8192];
	size_t CertBytes = readDer(Argv[1], Cert, sizeof(Cert));
	size_t KeyBytes = readDer(Argv[2], Key, sizeof(Key));
	xbytesview Leaf = {Cert, CertBytes};
	xtlsidentity* Identity = CertBytes && KeyBytes ?
		xrtTlsIdentityRsa(&Leaf, 1, (xbytesview){Key, KeyBytes}) : NULL;
	for (volatile unsigned char* Cursor = Key; Cursor != Key + sizeof(Key); ++Cursor) *Cursor = 0;
	if (!Identity) return 1;

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
	xtlsserverconfig Tls;
	xrtTlsServerConfigInit(&Tls);
	Tls.Context = Context;
	Tls.Identity = Identity;

	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {.Tls = &Tls};
	char Endpoint[80];
	bool Ready = Context && exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	xrtTlsContextRelease(Context);
	xrtTlsIdentityRelease(Identity);
	return Ready && Closed ? 0 : 1;
}
