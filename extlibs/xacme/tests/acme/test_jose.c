#include "../test.h"

#include "../../src/internal/xacme_jose.h"

#include <string.h>

/*
	固定密钥 = RFC 6979 A.2.5 P-256 测试密钥。
	全部期望值独立于实现导出：
	- 公钥 = RFC 6979 附录文档值；
	- JWK/指纹 = python hashlib 现算；
	- JWS = openssl pkeyutl 独立验签通过后固化。
*/
static const char* __testHexD =
	"C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721";
static const char* __testPubHex =
	"0460FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6"
	"7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299";
static const char* __testJwk =
	"{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":"
	"\"YP7UuiVanTHJYet0xjVtaMBJuJI7Yfps5mliLmDyn7Y\","
	"\"y\":\"eQP-EAi4vJmkGunpVii8ZPLxsgwtfp9Rd6PClNRGIpk\"}";
static const char* __testThumbprint =
	"DOvxvJiAdIqVWIkFt5hDtCunXLF0BV4-JGv4f-ALSm0";
static const char* __testJws =
	"{\"protected\":\"eyJhbGciOiJFUzI1NiIsIm5vbmNlIjoibm9uY2UtMTY1Mjc4MTgz"
	"MiIsInVybCI6Imh0dHBzOi8vZXhhbXBsZS5jb20vYWNtZS9uZXctb3JkZXIiLCJqd2siOnsi"
	"Y3J2IjoiUC0yNTYiLCJrdHkiOiJFQyIsIngiOiJZUDdVdWlWYW5USEpZZXQweGpWdGFNQkp1"
	"Skk3WWZwczVtbGlMbUR5bjdZIiwieSI6ImVRUC1FQWk0dkpta0d1bnBWaWk4WlBMeHNnd3Rm"
	"cDlSZDZQQ2xOUkdJcGsifX0\",\"payload\":\"eyJpZGVudGlmaWVycyI6W3sidHlwZSI6"
	"ImRucyIsInZhbHVlIjoidGVzdC54eHJwYS5jb20ifV19\",\"signature\":\"NVj6a91M"
	"7F11tIkAqffZvIOHZXU4-0xW9UFNwt3TaP9ro58U2bb4G0CWO2QfBydyS_vMFq2YpQSRVZpA"
	"AXY3FQ\"}";
/* EAB 内层 JWS（kid=eab-kid-0192，MAC=0011..EEFF×2）：python 固化。 */
static const char* __testEabJws =
	"{\"protected\":\"eyJhbGciOiJIUzI1NiIsImtpZCI6ImVhYi1raWQtMDE5MiIsInVyb"
	"CI6Imh0dHBzOi8vZXhhbXBsZS5jb20vYWNtZS9uZXctYWNjb3VudCJ9\",\"payload\":"
	"\"eyJjcnYiOiJQLTI1NiIsImt0eSI6IkVDIiwieCI6IllQN1V1aVZhblRISllldDB4alZ0"
	"YU1CSnVKSTdZZnBzNW1saUxtRHluN1kiLCJ5IjoiZVFQLUVBaTR2Sm1rR3VucFZpaThaUEx4"
	"c2d3dGZwOVJkNlBDbE5SR0lwayJ9\",\"signature\":"
	"\"aOaN9_LQpkpQPnef_1XdA1e2V4tshegGLN7441JqI-8\"}";

/*
	RFC 6979 A.2.5：消息 "sample" 的确定性 r||s。
	注意 xrt 的签名做了 low-S 规范化，s 是 RFC 向量 s 关于
	曲线阶 n 的补（openssl 验签独立确认等价有效）。
*/
static const char* __test6979SampleRs =
	"EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716"
	"0834E36AD29A83BF2BC9385E491D6099C8FDF9D1ED67AA7EA5F51F93782857A9";

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

int main(void)
{
	xacmees256key Key;
	str sJwk;
	char sThumb[44];

	testHexToBytes(__testHexD, Key.Private, 32u);

	/* 私钥派生公钥 = RFC 6979 文档公钥。 */
	testRequire(xacmeEs256FromPrivate(&Key), "acme jose from-private failed");
	{
		char sPubHex[131];
		size_t i;
		for(i = 0; i < 65u; i++)
		{
			sprintf(sPubHex + i * 2u, "%02X", Key.Public[i]);
		}
		testRequire(
			strcmp(sPubHex, __testPubHex) == 0,
			"acme jose from-private public mismatch"
		);
	}

	/* 生成路径冒烟：新密钥对可签名。 */
	{
		xacmees256key Fresh;
		xacmejwsheader H;
		str s;
		testRequire(xacmeEs256Generate(&Fresh), "acme jose generate failed");
		H.Nonce = XRT_STR_LITERAL("n");
		H.Url = XRT_STR_LITERAL("https://example.com/");
		H.Kid.Data = NULL;
		H.Kid.Size = 0;
		s = xacmeJwsEs256(
			&Fresh, &H, XRT_STR_LITERAL("{}"));
		testRequire(s != NULL, "acme jose generate jws failed");
		xrtFree(s);
	}

	/* RFC 6979 A.2.5：确定性签名 raw r||s 布局。 */
	{
		uint8 Digest[XRT_SHA256_SIZE];
		uint8 Signature[64];
		char sHex[129];
		size_t i;
		testRequire(
			xrtSha256("sample", 6u, Digest),
			"acme jose rfc6979 digest failed"
		);
		testRequire(
			xrtEcdsaP256Sign(
				XCRYPTO_HASH_SHA256, Digest, Key.Private, Signature),
			"acme jose rfc6979 sign failed"
		);
		for(i = 0; i < 64u; i++)
		{
			sprintf(sHex + i * 2u, "%02X", Signature[i]);
		}
		testRequire(
			strcmp(sHex, __test6979SampleRs) == 0,
			"acme jose rfc6979 sample signature mismatch"
		);
	}

	/* RFC 7638：RSA 规范化 JWK 指纹（thumbprint 函数对任意规范 JSON）。 */
	{
		static const char sRsaCanonical[] =
			"{\"e\":\"AQAB\",\"kty\":\"RSA\",\"n\":"
			"\"0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbf"
			"AAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMs"
			"tn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_"
			"FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n"
			"91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINH"
			"aQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw\"}";
		testRequire(
			xacmeJwkThumbprint(XRT_STR_LITERAL(sRsaCanonical), sThumb),
			"acme jose rfc7638 thumbprint failed"
		);
		testRequire(
			strcmp(
				sThumb,
				"NzbLsXh8uDCcd-6MNwXF4W_7noWXFZAfHkxZsRGC9Xs") == 0,
			"acme jose rfc7638 thumbprint mismatch"
		);
	}

	/* EC JWK 与指纹。 */
	sJwk = xacmeJwkEcJson(&Key);
	testRequire(sJwk != NULL, "acme jose jwk json failed");
	testRequire(
		strcmp(sJwk, __testJwk) == 0,
		"acme jose jwk json mismatch"
	);
	xrtFree(sJwk);
	testRequire(
		xacmeJwkEcThumbprint(&Key, sThumb),
		"acme jose ec thumbprint failed"
	);
	testRequire(
		strcmp(sThumb, __testThumbprint) == 0,
		"acme jose ec thumbprint mismatch"
	);

	/* JWS 紧凑序列化全量对照（openssl 独立验签过的值）。 */
	{
		xacmejwsheader H;
		str s = NULL;
		H.Nonce = XRT_STR_LITERAL("nonce-1652781832");
		H.Url = XRT_STR_LITERAL("https://example.com/acme/new-order");
		H.Kid.Data = NULL;
		H.Kid.Size = 0;
		s = xacmeJwsEs256(
			&Key, &H,
			XRT_STR_LITERAL(
				"{\"identifiers\":[{\"type\":\"dns\","
				"\"value\":\"test.xxrpa.com\"}]}"));
		testRequire(s != NULL, "acme jose jws assembly failed");
		testRequire(strcmp(s, __testJws) == 0, "acme jose jws mismatch");
		xrtFree(s);
	}

	/* EAB 内层 JWS：HS256 + RFC 6979 测试 JWK，期望值由 python
	   hmac/hashlib/base64 独立计算后固化。 */
	{
		uint8 Mac[32];
		str s;
		testHexToBytes(
			"00112233445566778899AABBCCDDEEFF"
			"00112233445566778899AABBCCDDEEFF",
			Mac, sizeof(Mac));
		s = xacmeJwsEabHs256(
			"eab-kid-0192", "https://example.com/acme/new-account",
			(xstrview){ __testJwk, strlen(__testJwk) }, Mac, sizeof(Mac));
		testRequire(s != NULL, "acme jose eab assembly failed");
		testRequire(strcmp(s, __testEabJws) == 0, "acme jose eab mismatch");
		xrtFree(s);
		/* 参数错误语义。 */
		xrtClearError();
		testRequire(
			(xacmeJwsEabHs256(
				NULL, "https://example.com/", XRT_STR_LITERAL("{}"),
				Mac, sizeof(Mac)) == NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme jose eab null argument mismatch"
		);
	}

	/* 参数错误语义。 */
	xrtClearError();
	testRequire(
		(xacmeJwsEs256(NULL, NULL, XRT_STR_LITERAL("{}")) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme jose jws null argument mismatch"
	);

	printf("[PASS] acme jose es256 jwk thumbprint jws\n");
	return 0;
}
