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

	/* RSA 证书密钥：PKCS#8 读入 → CSR（sha256WithRSA）→ PKCS#8 写出
	   → 读回重建一致 → Unit 清零。CSR 哈希经 cryptography 独立验签
	   固化（签名/SAN/SPKI 三重校验通过后的值）。 */
	{
		static const char* sRsaPem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDhfQJQhol1x3wi\n"
		"wLKZZz56Q8be7hRYkjjHid8mD3YHFPVv9J+dDX5uV85XxHi0PMLlAzE4Gu6q4FVk\n"
		"ES2REcAknG/hL44A84MOoSVE7TcDOS9ydAlFi3olu4E+lYcudZft89uDcJjJ6NnD\n"
		"xmUhIb9e7sOSSRh94fO03Q0JQIDguVbto9GLa1K5sllnrmNybjiy4kI2+NpZOGmH\n"
		"PhX+H/ES3vpPqA8gh5W+IEKzfYeO6FqS0macT4jmztJPkNxsyUxitJUyVAQU5fLe\n"
		"yjZrZAzVZMotXfp26cdDw1AGRcgpziHS76DUPcnLn2ncekyITTNr/d4M2iMkpJrL\n"
		"Z9KDoMQXAgMBAAECggEACsHSdvH8I8zI/MVxn8Tfo6iRF7iG6vTK1aYbqa6qJEtZ\n"
		"LCag6Laole4Fb4V1dq/BYniFBBaw1SEPoc75/AY7QuPdqIICxcPYOD3yz7d7XOGB\n"
		"BHZJrPIq+RrZXxl2Ef7VKSptifc4qsU7gk34LBz2irpcRSHMk9DQNgwnn43euBm4\n"
		"gRgjE2uk8bdoG4TQaUsXYzzBgwGcSXMHzSJW08oOuGClVvWu33ZJv3/pX/z7ZTHg\n"
		"9VoKgwwhWPg+Yn/SZg3vf5mCtkiT8z8UPGChf3zByDPaVEglFId3Le2WWN694uoz\n"
		"IC7gQM+c4KFOfgIQxubukG9FdnZPl1bOXl+2cN9IrQKBgQD0OQob7dZzUGvrVSBM\n"
		"D1tSNqc+BKkokvbUpWuK6jBD0ZOFD9mnlM0D+ncT0g8a06cUhAfbOo7CetgsaHCh\n"
		"H2zD3PmGq67Ui1GRiJYI0wpbLlQ5QHKTUk8cIX6P2AUvtnCXRjrBob0YC/+NV92R\n"
		"pr/QkN8azlSpOk0KjGXzleoy3QKBgQDsXLCfQeXnnFcGdWQ3EOgFXkqYIiu5jFct\n"
		"dAzDe7BCZ/QS0eumS6TaktjOmNsXB2GvlUfAT3tjycqRNUOcypIUNT3A9fqw4eKz\n"
		"WAQf1FkZRqOfCfLIkRekbaip3yX2mcKpGclt2+zC7HBmyetk3jgRuRy5W8V2LXOh\n"
		"rAZZNd9hgwKBgGB4p6WgrbWfbwHm/nsNFeXD8QxuiuOcKiSVs4WMPMSNZNiLCk9I\n"
		"WDPaHG+X6p+OO9G+1dujpgDsxbfFCbib0TsNbwPjjYwn/HCgo6OYud6KznpPGvNW\n"
		"8CkMkhIAIwxV5OPcuhkC6s807h3HN57xX5Pjpj3Qg2DVxtkD3MH71ieBAoGBALY5\n"
		"Z7oAdh0wPS+vhYmmsRqibWQIxCkS94sFc2mqjGNF/bgcu07D9t7EY/4zfoWsnTVf\n"
		"I6gyHvD5/AAjTnMtAZ4uxeNkQNfp7ntSGivn7KE+Apt0cgcLRWzxVh1Q+tW6CYeR\n"
		"Z+gYq9pDqwy0E5T6dcPEMU7+X6gStpkoJOTWUde9AoGBANphL9EwLUthDngddXDa\n"
		"P9cmnA39Nz39EBbR/06Po8s4hId4eq5NVXYGTys5NR6+XcnhLKQPW1U1VKsMoDI5\n"
		"dLXI39ldba82xefXKdEA+BSkTgGtXKXPNQwHlQIistXszAeuOND78MUq3u5uzy5g\n"
		"MlMSMneEUcUd3VBrrRZNp3hO\n"
		"-----END PRIVATE KEY-----\n";
		static const char* sRsaCsrSha256 =
			"C406AE6D3CA8FBF872103B7BD3FEC2C20DABCD434D4ECAE170AED5ECC345A4A3";
		xacmecertkey CertKey;
		xacmecsrconfig Csr;
		xstrview Domains[2];
		xbuffer Der;
		uint8 Digest[XRT_SHA256_SIZE];
		char sHex[XRT_SHA256_SIZE * 2u + 1u];
		str sRewritten;
		size_t i;

		testRequire(
			xacmeCertKeyReadPem(sRsaPem, strlen(sRsaPem), &CertKey) &&
				(CertKey.Kind == XACME_CERT_KEY_RSA) &&
				(CertKey.Rsa.ModulusSize == 256u),
			"acme cert key rsa read failed");
		Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
		Domains[1] = XRT_STR_LITERAL("*.test.xxrpa.com");
		Csr.CommonName = Domains[0];
		Csr.Domains = Domains;
		Csr.DomainCount = 2u;
		xrtBufferInit(&Der);
		testRequire(
			xacmeCsrBuild(&CertKey, &Csr, &Der),
			"acme csr rsa build failed");
		testRequire(
			xrtSha256(Der.Data, Der.Size, Digest),
			"acme csr rsa digest failed");
		for(i = 0; i < sizeof(Digest); i++)
		{
			sprintf(sHex + i * 2u, "%02X", Digest[i]);
		}
		sHex[sizeof(Digest) * 2u] = '\0';
		testRequire(
			strcmp(sHex, sRsaCsrSha256) == 0,
			"acme csr rsa sha256 mismatch");
		xrtBufferUnit(&Der);

		/* PEM 重写 → 读回 → CSR 一致。 */
		sRewritten = xacmeCertKeyPemWrite(&CertKey);
		testRequire(
			sRewritten != NULL, "acme cert key rsa write failed");
		xacmeCertKeyUnit(&CertKey);
		testRequire(
			xacmeCertKeyReadPem(sRewritten, strlen(sRewritten), &CertKey) &&
				(CertKey.Kind == XACME_CERT_KEY_RSA),
			"acme cert key rsa reread failed");
		xrtFree(sRewritten);
		xrtBufferInit(&Der);
		testRequire(
			xacmeCsrBuild(&CertKey, &Csr, &Der) &&
				xrtSha256(Der.Data, Der.Size, Digest),
			"acme csr rsa rebuild failed");
		for(i = 0; i < sizeof(Digest); i++)
		{
			sprintf(sHex + i * 2u, "%02X", Digest[i]);
		}
		testRequire(
			strcmp(sHex, sRsaCsrSha256) == 0,
			"acme csr rsa roundtrip mismatch");
		xrtBufferUnit(&Der);
		xacmeCertKeyUnit(&CertKey);
		testRequire(
			(CertKey.Kind == XACME_CERT_KEY_ES256) &&
				(CertKey.Rsa.ModulusSize == 0u),
			"acme cert key unit clear mismatch");
	}

	/* EC 密钥经统一入口读入。 */
	{
		xacmecertkey CertKey;
		testRequire(
			xacmeCertKeyReadPem(__testPem, strlen(__testPem), &CertKey) &&
				(CertKey.Kind == XACME_CERT_KEY_ES256),
			"acme cert key ec dispatch failed");
		xacmeCertKeyUnit(&CertKey);
	}

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
