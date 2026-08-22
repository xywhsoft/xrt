#include "../test.h"



/* Ed25519 provider 必须生成 OpenSSH 可验证的签名请求，并保持失败写入原子性。 */
int main(void)
{
	unsigned char arrSeed[XRT_ED25519_SEED_SIZE];
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrPublicBlob[64];
	unsigned char arrPayload[512];
	unsigned char arrSignData[256];
	unsigned char arrSmall[32];
	xsshed25519identity Identity;
	xsshclientauth Auth;
	xsshauthpublickey PublicKey;
	xsshwriter PublicWriter;
	xsshwriter Writer;
	xsshwriter SignWriter;
	xsshcode Code;
	size_t i;

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (unsigned char)(0x20u + i);
	}
	testRequire(xrtEd25519Public(arrSeed, arrPublic) &&
		xrtSshWriterInit(
			&PublicWriter,
			arrPublicBlob,
			sizeof(arrPublicBlob)
		) && (xrtSshEd25519PublicKeyWrite(
			&PublicWriter,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) == XSSH_OK), "ssh client Ed25519 identity setup failed");
	memset(&Identity, 0, sizeof(Identity));
	Identity.PublicKeyBlob = (xbytesview){
		arrPublicBlob,
		PublicWriter.Size
	};
	Identity.Seed = (xbytesview){ arrSeed, sizeof(arrSeed) };
	Identity.PublicKey = (xbytesview){ arrPublic, sizeof(arrPublic) };
	memset(&Auth, 0, sizeof(Auth));
	Auth.User = XRT_STR_LITERAL("alice");
	Auth.Methods = XRT_STR_LITERAL("publickey,password");
	Auth.SessionId = XRT_BYTES_LITERAL("session-identifier");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshClientEd25519Auth(
		NULL,
		&Writer,
		&Auth,
		&Identity
	) == XSSH_OK) && (xrtSshAuthPublicKeyRead(
		(xbytesview){ arrPayload, Writer.Size },
		&PublicKey
	) == XSSH_OK) && PublicKey.HasSignature,
		"ssh client Ed25519 signed request failed");
	testRequire(xrtSshWriterInit(
		&SignWriter,
		arrSignData,
		sizeof(arrSignData)
	) && (xrtSshAuthPublicKeySignDataWrite(
		&SignWriter,
		Auth.SessionId,
		Auth.User,
		PublicKey.Algorithm,
		PublicKey.PublicKey
	) == XSSH_OK) && (xrtSshEd25519HostKeyVerify(
		Identity.PublicKeyBlob,
		PublicKey.Signature,
		(xbytesview){ arrSignData, SignWriter.Size }
	) == XSSH_OK), "ssh client Ed25519 signature mismatch");

	Auth.Methods = XRT_STR_LITERAL("password");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	), "ssh client Ed25519 unsupported fixture failed");
	Code = xrtSshClientEd25519Auth(NULL, &Writer, &Auth, &Identity);
	testRequire((Code == XSSH_ERROR_AUTHENTICATION) && (Writer.Size == 0u),
		"ssh client Ed25519 accepted an unsupported method list");

	Auth.Methods = XRT_STR_LITERAL("publickey");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrSmall,
		sizeof(arrSmall)
	), "ssh client Ed25519 small writer fixture failed");
	Code = xrtSshClientEd25519Auth(NULL, &Writer, &Auth, &Identity);
	testRequire((Code == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh client Ed25519 small writer was not atomic");
	xrtSecureZero(arrSeed, sizeof(arrSeed));
	return 0;
}
