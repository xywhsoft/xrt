#include "../test.h"



/* 验证空 blob 的标准 SHA-256 摘要与 OpenSSH 无填充格式。 */
static void testSshFingerprintVector(void)
{
	static const unsigned char arrExpected[XSSH_FINGERPRINT_SHA256_SIZE] = {
		0xe3u, 0xb0u, 0xc4u, 0x42u, 0x98u, 0xfcu, 0x1cu, 0x14u,
		0x9au, 0xfbu, 0xf4u, 0xc8u, 0x99u, 0x6fu, 0xb9u, 0x24u,
		0x27u, 0xaeu, 0x41u, 0xe4u, 0x64u, 0x9bu, 0x93u, 0x4cu,
		0xa4u, 0x95u, 0x99u, 0x1bu, 0x78u, 0x52u, 0xb8u, 0x55u
	};
	static const char sExpected[] =
		"SHA256:47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU";
	unsigned char arrDigest[XSSH_FINGERPRINT_SHA256_SIZE];
	char sOutput[64];
	size_t iOutputSize;

	testRequire((xrtSshHostKeyDigestSha256(
		(xbytesview){ NULL, 0u },
		arrDigest
	) == XSSH_OK) &&
		(memcmp(arrDigest, arrExpected, sizeof(arrExpected)) == 0) &&
		(xrtSshHostKeyFingerprintSha256(
			(xbytesview){ NULL, 0u },
			NULL,
			0u,
			&iOutputSize
		) == XSSH_OK) && (iOutputSize == (sizeof(sExpected) - 1u)) &&
		(xrtSshHostKeyFingerprintSha256(
			(xbytesview){ NULL, 0u },
			sOutput,
			sizeof(sOutput),
			&iOutputSize
		) == XSSH_OK) && (strcmp(sOutput, sExpected) == 0),
		"ssh SHA-256 fingerprint vector mismatch");
}



/* 验证容量和输出重叠失败不会发布部分结果。 */
static void testSshFingerprintBoundaries(void)
{
	unsigned char arrBlob[64] = { 1u };
	char sOutput[64];
	size_t iOutputSize = 99u;

	memset(sOutput, 'x', sizeof(sOutput));
	testRequire((xrtSshHostKeyFingerprintSha256(
		(xbytesview){ arrBlob, sizeof(arrBlob) },
		sOutput,
		8u,
		&iOutputSize
	) == XSSH_ERROR_SPACE) && (iOutputSize == 99u) &&
		(sOutput[0] == 'x'), "ssh short fingerprint changed outputs");
	testRequire(xrtSshHostKeyFingerprintSha256(
		(xbytesview){ arrBlob, sizeof(arrBlob) },
		(char*)arrBlob,
		sizeof(arrBlob),
		&iOutputSize
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping fingerprint accepted");
	testRequire(xrtSshHostKeyDigestSha256(
		(xbytesview){ arrBlob, sizeof(arrBlob) },
		arrBlob
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping digest accepted");
}



/* 运行 SSH 主机密钥指纹测试。 */
int main(void)
{
	testSshFingerprintVector();
	testSshFingerprintBoundaries();
	return 0;
}
