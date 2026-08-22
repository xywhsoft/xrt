#include "private_key_fixture.h"



/* 验证身份字段、原始签名和最终 SSH signature blob。 */
static void testSshPrivateKeyEd25519Sign(void)
{
	static const unsigned char arrMessage[] = "xssh private identity";
	testsshprivatekeyfixture Fixture;
	xsshopensshprivatekey PrivateKey;
	xsshed25519identity Identity;
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	unsigned char arrExpected[XSSH_ED25519_SIGNATURE_SIZE];
	unsigned char arrBlob[96];
	xsshwriter Writer;
	xsshsignature Signature;

	testSshPrivateKeyFixture(&Fixture);
	testRequire(xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&PrivateKey
	) == XSSH_OK, "ssh Ed25519 private container read failed");
	testRequire((xrtSshPrivateKeyEd25519Read(
		&PrivateKey,
		&Identity
	) == XSSH_OK) && testSshBytesEqual(
		Identity.Seed,
		(xbytesview){ Fixture.Seed, sizeof(Fixture.Seed) }
	) && testSshBytesEqual(
		Identity.PublicKey,
		(xbytesview){ Fixture.Public, sizeof(Fixture.Public) }
	) && testSshBytesEqual(
		Identity.Comment,
		XRT_BYTES_LITERAL("fixture@example")
	), "ssh Ed25519 private identity mismatch");
	testRequire((xrtSshPrivateKeyEd25519Sign(
		&Identity,
		(xbytesview){ arrMessage, sizeof(arrMessage) - 1u },
		arrSignature
	) == XSSH_OK) && xrtEd25519Sign(
		Fixture.Seed,
		arrMessage,
		sizeof(arrMessage) - 1u,
		arrExpected
	) && (memcmp(
		arrSignature,
		arrExpected,
		sizeof(arrExpected)
	) == 0), "ssh Ed25519 private signature mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshPrivateKeyEd25519SignatureWrite(
			&Writer,
			&Identity,
			(xbytesview){ arrMessage, sizeof(arrMessage) - 1u }
		) == XSSH_OK) && (xrtSshSignatureRead(
			(xbytesview){ arrBlob, Writer.Size },
			&Signature
		) == XSSH_OK) && testSshTextEqual(
		Signature.Algorithm,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
	) && testSshBytesEqual(
		Signature.Signature,
		(xbytesview){ arrExpected, sizeof(arrExpected) }
	), "ssh Ed25519 signature blob mismatch");
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	xrtSecureZero(arrExpected, sizeof(arrExpected));
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 验证 checkint、公钥、padding 和签名输出重叠边界。 */
static void testSshPrivateKeyEd25519Boundaries(void)
{
	testsshprivatekeyfixture Fixture;
	xsshopensshprivatekey PrivateKey;
	xsshopensshprivatekey EncryptedKey;
	xsshed25519identity Identity;
	xsshed25519identity Keep;
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	unsigned char* pMutable;

	testSshPrivateKeyFixture(&Fixture);
	testRequire(xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&PrivateKey
	) == XSSH_OK, "ssh Ed25519 boundary container read failed");
	testRequire(xrtSshPrivateKeyEd25519Read(
		&PrivateKey,
		&Identity
	) == XSSH_OK, "ssh Ed25519 boundary identity read failed");
	Keep = Identity;
	pMutable = (unsigned char*)PrivateKey.PrivateList.Data;
	pMutable[0] ^= 1u;
	testRequire((xrtSshPrivateKeyEd25519Read(
		&PrivateKey,
		&Identity
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&Identity, &Keep, sizeof(Keep)) == 0),
		"ssh mismatched private checkint accepted");
	pMutable[0] ^= 1u;
	pMutable = (unsigned char*)Keep.PublicKey.Data;
	pMutable[0] ^= 1u;
	testRequire(xrtSshPrivateKeyEd25519Read(
		&PrivateKey,
		&Identity
	) == XSSH_ERROR_PROTOCOL, "ssh mismatched private public key accepted");
	pMutable[0] ^= 1u;
	pMutable = Fixture.Binary + Fixture.BinarySize - 1u;
	pMutable[0] ^= 1u;
	testRequire(xrtSshPrivateKeyEd25519Read(
		&PrivateKey,
		&Identity
	) == XSSH_ERROR_PROTOCOL, "ssh bad private padding accepted");
	pMutable[0] ^= 1u;
	Identity = Keep;
	testRequire(xrtSshPrivateKeyEd25519Sign(
		&Identity,
		XRT_BYTES_LITERAL("message"),
		(void*)Identity.Seed.Data
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping private signature accepted");

	/* 加密容器由独立 cipher/KDF 层处理，不能误读为明文 Ed25519 身份。 */
	memcpy((void*)PrivateKey.Cipher.Data, "aesx", 4u);
	memcpy((void*)PrivateKey.Kdf.Data, "kdfx", 4u);
	testRequire((xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&EncryptedKey
	) == XSSH_OK) && (xrtSshPrivateKeyEd25519Read(
		&EncryptedKey,
		&Identity
	) == XSSH_ERROR_UNSUPPORTED) &&
		(memcmp(&Identity, &Keep, sizeof(Keep)) == 0),
		"ssh encrypted Ed25519 identity accepted");
	memset(arrSignature, 0, sizeof(arrSignature));
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 运行未加密 Ed25519 私钥与签名测试。 */
int main(void)
{
	testSshPrivateKeyEd25519Sign();
	testSshPrivateKeyEd25519Boundaries();
	return 0;
}
