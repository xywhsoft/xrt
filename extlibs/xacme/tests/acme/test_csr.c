#include "../test.h"

#include "../../src/internal/xacme_csr.h"

#include <string.h>

/*
	固定密钥 = RFC 6979 A.2.5（同 test_jose）。
	期望值独立于实现导出：CSR 经 openssl req -verify 验签通过并
	含 SAN（test.xxrpa.com 与通配符）后固化摘要；PKCS#8 PEM 由
	openssl pkey 加载成功后固化全文；SEC1 夹具是 openssl ec 导出
	的外部格式样本。
*/
static const char* __testHexD =
	"C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721";
static const char* __testCsrSha256 =
	"838DDB60D984B2259F414FDFCAE28BE98B84DD6E1D7EB504710F4A3CFCCBB5B9";
static const char* __testPem =
"-----BEGIN PRIVATE KEY-----\n"
"MIGTAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBHkwdwIBAQQgya+p2EW6dRZrXCFX\n"
"Z7HWk05Qw9s26JsSe4piKxIPZyGgCgYIKoZIzj0DAQehRANCAARg/tS6JVqdMclh\n"
"63TGNW1owEm4kjth+mzmaWIuYPKftnkD/hAIuLyZpBrp6VYovGTy8bIMLX6fUXej\n"
"wpTURiKZ\n"
"-----END PRIVATE KEY-----\n";
static const char* __testSec1 =
	"-----BEGIN EC PRIVATE KEY-----\n"
	"MHcCAQEEIMmvqdhFunUWa1whV2ex1pNOUMPbNuibEnuKYisSD2choAoGCCqGSM49\n"
	"AwEHoUQDQgAEYP7UuiVanTHJYet0xjVtaMBJuJI7Yfps5mliLmDyn7Z5A/4QCLi8\n"
	"maQa6elWKLxk8vGyDC1+n1F3o8KU1EYimQ==\n"
	"-----END EC PRIVATE KEY-----\n";

static void testHexToBytes(const char* sHex, uint8* pOut, size_t iCount)
{
	size_t i;
	for(i = 0; i < iCount; i++)
	{
		unsigned v = 0;
		sscanf(sHex + i * 2u, "%2x", &v);
		pOut[i] = (uint8)v;
	}
}

static void testSha256Hex(const uint8* pData, size_t iSize, char* sOut)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t i;
	testRequire(xrtSha256(pData, iSize, Digest), "acme csr digest failed");
	for(i = 0; i < XRT_SHA256_SIZE; i++)
	{
		sprintf(sOut + i * 2u, "%02X", Digest[i]);
	}
}

int main(void)
{
	xacmees256key Key;
	xacmees256key Back;
	xacmecsrconfig Csr;
	xstrview Domains[2];
	xbuffer Der;
	str sPem;
	char sHex[65];

	testHexToBytes(__testHexD, Key.Private, 32u);
	testRequire(
		xacmeEs256FromPrivate(&Key),
		"acme csr from-private failed"
	);

	/* CSR 全量 KAT：确定性输出摘要对照（openssl 验签背书）。 */
	Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
	Domains[1] = XRT_STR_LITERAL("*.test.xxrpa.com");
	Csr.CommonName = XRT_STR_LITERAL("test.xxrpa.com");
	Csr.Domains = Domains;
	Csr.DomainCount = 2u;
	xrtBufferInit(&Der);
	testRequire(
		xacmeCsrEc(&Key, &Csr, &Der),
		"acme csr assembly failed"
	);
	testRequire(Der.Size == 277u, "acme csr size mismatch");
	testSha256Hex(Der.Data, Der.Size, sHex);
	testRequire(
		strcmp(sHex, __testCsrSha256) == 0,
		"acme csr digest mismatch"
	);
	xrtBufferUnit(&Der);

	/* PKCS#8 PEM 写侧 KAT（openssl pkey 加载背书）。 */
	sPem = xacmeKeyPemWrite(&Key);
	testRequire(sPem != NULL, "acme key pem write failed");
	testRequire(
		strcmp(sPem, __testPem) == 0,
		"acme key pem write mismatch"
	);

	/* 自产 PEM 读回。 */
	memset(&Back, 0, sizeof(Back));
	testRequire(
		xacmeKeyPemRead(sPem, strlen(sPem), &Back),
		"acme key pem read own failed"
	);
	testRequire(
		(memcmp(Back.Private, Key.Private, 32u) == 0) &&
			(memcmp(Back.Public, Key.Public, 65u) == 0),
		"acme key pem roundtrip mismatch"
	);
	xrtFree(sPem);

	/* openssl 导出的外部 SEC1 格式。 */
	memset(&Back, 0, sizeof(Back));
	testRequire(
		xacmeKeyPemRead(__testSec1, strlen(__testSec1), &Back),
		"acme key pem read sec1 fixture failed"
	);
	testRequire(
		(memcmp(Back.Private, Key.Private, 32u) == 0) &&
			(memcmp(Back.Public, Key.Public, 65u) == 0),
		"acme key pem sec1 fixture key mismatch"
	);

	/* 错误语义。 */
	xrtClearError();
	testRequire(
		!xacmeKeyPemRead("no block", 9u, &Back) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"acme key pem read missing block mismatch"
	);
	xrtClearError();
	testRequire(
		!xacmeCsrEc(NULL, &Csr, &Der) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme csr null argument mismatch"
	);

	printf("[PASS] acme csr pkcs10 pkcs8 pem\n");
	return 0;
}
