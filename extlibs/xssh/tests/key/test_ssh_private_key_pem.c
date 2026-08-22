#include "private_key_fixture.h"



/* 验证 PEM 查询、调用方缓冲解码与二进制容器借用关系。 */
static void testSshPrivateKeyPemRead(void)
{
	testsshprivatekeyfixture Fixture;
	unsigned char arrBinary[512];
	size_t iBinarySize = 0u;
	xsshopensshprivatekey PrivateKey;

	testSshPrivateKeyFixture(&Fixture);
	testRequire((xrtSshPrivateKeyPemRead(
		(xstrview){ Fixture.Pem, Fixture.PemSize },
		NULL,
		0u,
		&iBinarySize,
		NULL
	) == XSSH_OK) && (iBinarySize == Fixture.BinarySize) &&
		(xrtSshPrivateKeyPemRead(
			(xstrview){ Fixture.Pem, Fixture.PemSize },
			arrBinary,
			sizeof(arrBinary),
			&iBinarySize,
			&PrivateKey
		) == XSSH_OK) && (iBinarySize == Fixture.BinarySize) &&
		(memcmp(arrBinary, Fixture.Binary, iBinarySize) == 0) &&
		(PrivateKey.Blob.Data == arrBinary), "ssh private PEM read mismatch");
	xrtSecureZero(arrBinary, sizeof(arrBinary));
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 验证短缓冲、错误标签和输出重叠保持结构化输出不变。 */
static void testSshPrivateKeyPemBoundaries(void)
{
	testsshprivatekeyfixture Fixture;
	unsigned char arrBinary[512];
	size_t iBinarySize = 77u;
	xsshopensshprivatekey PrivateKey;
	xsshopensshprivatekey Keep;

	testSshPrivateKeyFixture(&Fixture);
	memset(arrBinary, 0x5au, sizeof(arrBinary));
	memset(&Keep, 0x6bu, sizeof(Keep));
	PrivateKey = Keep;
	testRequire((xrtSshPrivateKeyPemRead(
		(xstrview){ Fixture.Pem, Fixture.PemSize },
		arrBinary,
		Fixture.BinarySize - 1u,
		&iBinarySize,
		&PrivateKey
	) == XSSH_ERROR_SPACE) && (iBinarySize == 77u) &&
		(memcmp(&PrivateKey, &Keep, sizeof(Keep)) == 0) &&
		(arrBinary[0] == 0x5au), "ssh short private PEM changed outputs");
	testRequire((xrtSshPrivateKeyPemRead(
		XRT_STR_LITERAL("-----BEGIN OTHER-----\nAAAA\n-----END OTHER-----\n"),
		NULL,
		0u,
		&iBinarySize,
		NULL
	) == XSSH_ERROR_PROTOCOL) && (iBinarySize == 77u) &&
		(xrtSshPrivateKeyPemRead(
			(xstrview){ Fixture.Pem, Fixture.PemSize },
			Fixture.Pem,
			Fixture.PemSize,
			&iBinarySize,
			&PrivateKey
		) == XSSH_ERROR_ARGUMENT), "ssh invalid private PEM accepted");
	xrtSecureZero(Fixture.Binary, sizeof(Fixture.Binary));
}



/* 运行 OpenSSH 私钥 PEM 适配测试。 */
int main(void)
{
	testSshPrivateKeyPemRead();
	testSshPrivateKeyPemBoundaries();
	return 0;
}
