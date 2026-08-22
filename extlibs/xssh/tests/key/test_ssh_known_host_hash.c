#include "../test.h"



/* 固定 OpenSSH hashed-host 向量，独立验证 HMAC 与 Base64 封装。 */
static void testSshKnownHostHashVector(void)
{
	static const char sExpected[] =
		"|1|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8=";
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE];
	unsigned char arrHash[XSSH_KNOWN_HOST_HASH_SIZE];
	char sOutput[80];
	size_t iOutputSize = 0u;
	bool bMatch = false;
	size_t i;

	for ( i = 0u; i < sizeof(arrSalt); ++i ) {
		arrSalt[i] = (unsigned char)(0x20u + i);
	}
	testRequire((xrtSshKnownHostHash(
		XRT_STR_LITERAL("hashed.example"),
		22u,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		arrHash
	) == XSSH_OK) && (xrtSshKnownHostHashWrite(
		XRT_STR_LITERAL("hashed.example"),
		22u,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		NULL,
		0u,
		&iOutputSize
	) == XSSH_OK) && (iOutputSize == (sizeof(sExpected) - 1u)) &&
		(xrtSshKnownHostHashWrite(
			XRT_STR_LITERAL("hashed.example"),
			22u,
			(xbytesview){ arrSalt, sizeof(arrSalt) },
			sOutput,
			sizeof(sOutput),
			&iOutputSize
		) == XSSH_OK) &&
		(strcmp(sOutput, sExpected) == 0), "ssh known-host hash vector mismatch");
	testRequire((xrtSshKnownHostHashMatch(
		XRT_STR_LITERAL(sExpected),
		XRT_STR_LITERAL("HASHED.EXAMPLE"),
		22u,
		&bMatch
	) == XSSH_OK) && bMatch && (xrtSshKnownHostHashMatch(
		XRT_STR_LITERAL(sExpected),
		XRT_STR_LITERAL("other.example"),
		22u,
		&bMatch
	) == XSSH_OK) && !bMatch, "ssh known-host hash match mismatch");
}



/* 验证非默认端口、known_hosts 行组合和精确端口隔离。 */
static void testSshKnownHostHashPort(void)
{
	static const char sKey[] =
		"ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE] = { 1u };
	char sHash[80];
	char sLine[256];
	size_t iHashSize;
	xsshknownhostline KnownHost;
	bool bMatch;
	int iWritten;

	testRequire(xrtSshKnownHostHashWrite(
		XRT_STR_LITERAL("port.example"),
		2200u,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		sHash,
		sizeof(sHash),
		&iHashSize
	) == XSSH_OK, "ssh port hash write failed");
	iWritten = snprintf(sLine, sizeof(sLine), "%s %s", sHash, sKey);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshKnownHostLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KnownHost
		) == XSSH_OK) && KnownHost.Hashed &&
		(xrtSshKnownHostLineHashMatch(
			&KnownHost,
			XRT_STR_LITERAL("port.example"),
			2200u,
			&bMatch
		) == XSSH_OK) && bMatch &&
		(xrtSshKnownHostLineHashMatch(
			&KnownHost,
			XRT_STR_LITERAL("port.example"),
			22u,
			&bMatch
		) == XSSH_OK) && !bMatch, "ssh known-host port hash mismatch");
}



/* 验证版本、字段、salt 长度、容量与重叠失败保持输出不变。 */
static void testSshKnownHostHashBoundaries(void)
{
	static const char sExpected[] =
		"|1|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8=";
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE] = { 0u };
	char sOutput[80];
	size_t iOutputSize = 91u;
	bool bMatch = true;

	memset(sOutput, 'x', sizeof(sOutput));
	testRequire((xrtSshKnownHostHashWrite(
		XRT_STR_LITERAL("host"),
		22u,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		sOutput,
		8u,
		&iOutputSize
	) == XSSH_ERROR_SPACE) && (iOutputSize == 91u) &&
		(sOutput[0] == 'x'), "ssh short hash write changed outputs");
	testRequire((xrtSshKnownHostHashMatch(
		XRT_STR_LITERAL("|2|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8="),
		XRT_STR_LITERAL("host"),
		22u,
		&bMatch
	) == XSSH_ERROR_PROTOCOL) && bMatch &&
		(xrtSshKnownHostHashMatch(
			XRT_STR_LITERAL("|1|AAAA|BBBB|CCCC"),
			XRT_STR_LITERAL("host"),
			22u,
			&bMatch
		) == XSSH_ERROR_PROTOCOL) && bMatch &&
		(xrtSshKnownHostHashMatch(
			XRT_STR_LITERAL("|1|AAAA|BBBB"),
			XRT_STR_LITERAL("host"),
			22u,
			&bMatch
		) == XSSH_ERROR_PROTOCOL) && bMatch,
		"ssh malformed host hash accepted");
	testRequire(xrtSshKnownHostHashMatch(
		XRT_STR_LITERAL(sExpected),
		XRT_STR_LITERAL("host"),
		22u,
		(bool*)sExpected
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping hash result accepted");
}



/* 运行 OpenSSH hashed-host 兼容测试。 */
int main(void)
{
	testSshKnownHostHashVector();
	testSshKnownHostHashPort();
	testSshKnownHostHashBoundaries();
	return 0;
}
