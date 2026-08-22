#include "private_key_fixture.h"



/* 验证容器元数据、公开公钥游标和未加密状态。 */
static void testSshPrivateKeyRead(void)
{
	testsshprivatekeyfixture Fixture;
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	xbytesview Keep;
	bool bEncrypted = true;

	testSshPrivateKeyFixture(&Fixture);
	testRequire((xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&PrivateKey
	) == XSSH_OK) && testSshTextEqual(
		PrivateKey.Cipher,
		XRT_STR_LITERAL(XSSH_PRIVATE_KEY_NONE)
	) && testSshTextEqual(
		PrivateKey.Kdf,
		XRT_STR_LITERAL(XSSH_PRIVATE_KEY_NONE)
	) && (PrivateKey.KdfOptions.Size == 0u) &&
		(PrivateKey.KeyCount == 1u) && (xrtSshPrivateKeyIsEncrypted(
			&PrivateKey,
			&bEncrypted
		) == XSSH_OK) && !bEncrypted, "ssh private container fields mismatch");
	testRequire(xrtSshPrivateKeyPublicsInit(
		&PrivateKey,
		&Publics
	) == XSSH_OK, "ssh private public-key cursor init failed");
	testRequire((xrtSshPrivateKeyPublicsNext(
		&Publics,
		&PublicKeyBlob
	) == XSSH_OK) && testSshBytesEqual(
		PublicKeyBlob,
		(xbytesview){ Fixture.PublicKeyBlob, Fixture.PublicKeyBlobSize }
	), "ssh private public-key cursor mismatch");
	Keep = PublicKeyBlob;
	testRequire((xrtSshPrivateKeyPublicsNext(
		&Publics,
		&PublicKeyBlob
	) == XSSH_NEED_MORE) &&
		(memcmp(&PublicKeyBlob, &Keep, sizeof(Keep)) == 0),
		"ssh private public cursor end changed output");
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 验证未知加密 cipher/KDF 元数据保持可扩展，而算法层显式接管解密。 */
static void testSshPrivateKeyEncryptedMetadata(void)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	testsshprivatekeyfixture Fixture;
	xsshopensshprivatekey Source;
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	unsigned char arrBinary[256];
	unsigned char arrOptions[] = { 1u, 2u, 3u };
	unsigned char arrEncrypted[] = { 4u, 5u, 6u, 7u };
	xsshwriter Writer;
	bool bEncrypted = false;

	testSshPrivateKeyFixture(&Fixture);
	testRequire(xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&Source
	) == XSSH_OK, "ssh encrypted fixture read failed");
	testRequire(xrtSshPrivateKeyPublicsInit(
		&Source,
		&Publics
	) == XSSH_OK, "ssh encrypted fixture public cursor init failed");
	testRequire(xrtSshPrivateKeyPublicsNext(
		&Publics,
		&PublicKeyBlob
	) == XSSH_OK, "ssh encrypted fixture public key read failed");
	testRequire(xrtSshWriterInit(&Writer, arrBinary, sizeof(arrBinary)) &&
		(xrtSshWriteBytes(
			&Writer,
			(xbytesview){ arrMagic, sizeof(arrMagic) }
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("aes256-ctr")
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("bcrypt")
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			(xbytesview){ arrOptions, sizeof(arrOptions) }
		) == XSSH_OK) && (xrtSshWriteU32(&Writer, 1u) == XSSH_OK) &&
		(xrtSshWriteString(&Writer, PublicKeyBlob) == XSSH_OK) &&
		(xrtSshWriteString(
			&Writer,
			(xbytesview){ arrEncrypted, sizeof(arrEncrypted) }
		) == XSSH_OK) && (xrtSshPrivateKeyRead(
			(xbytesview){ arrBinary, Writer.Size },
			&PrivateKey
		) == XSSH_OK) && (xrtSshPrivateKeyIsEncrypted(
			&PrivateKey,
			&bEncrypted
		) == XSSH_OK) && bEncrypted,
		"ssh encrypted private metadata mismatch");
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 验证 magic、截断、尾随数据和输出重叠失败保持结构输出不变。 */
static void testSshPrivateKeyBoundaries(void)
{
	testsshprivatekeyfixture Fixture;
	xsshopensshprivatekey PrivateKey;
	xsshopensshprivatekey Keep;

	testSshPrivateKeyFixture(&Fixture);
	memset(&Keep, 0x5au, sizeof(Keep));
	PrivateKey = Keep;
	Fixture.Binary[0] ^= 1u;
	testRequire((xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		&PrivateKey
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&PrivateKey, &Keep, sizeof(Keep)) == 0),
		"ssh bad private magic changed output");
	Fixture.Binary[0] ^= 1u;
	Fixture.Binary[Fixture.BinarySize] = 0u;
	testRequire((xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize + 1u },
		&PrivateKey
	) == XSSH_ERROR_PROTOCOL) && (xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize - 1u },
		&PrivateKey
	) == XSSH_ERROR_PROTOCOL) && (xrtSshPrivateKeyRead(
		(xbytesview){ Fixture.Binary, Fixture.BinarySize },
		(xsshopensshprivatekey*)Fixture.Binary
	) == XSSH_ERROR_ARGUMENT), "ssh private container boundary accepted");
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 运行 openssh-key-v1 二进制容器测试。 */
int main(void)
{
	testSshPrivateKeyRead();
	testSshPrivateKeyEncryptedMetadata();
	testSshPrivateKeyBoundaries();
	return 0;
}
