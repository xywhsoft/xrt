#include "../test.h"
#include "../fixtures/x509_profile_vectors.h"



#define TEST_X509_VIEW(sText) { (sText), sizeof(sText) - 1u }



/* 验证一次变异证书上的身份结果和输出原子性。 */
static void testX509IdentityMutationCert(
	const uint8* pDer,
	size_t iSize,
	size_t* pParsed,
	size_t* pIdentityErrors
)
{
	static const xstrview Hosts[] = {
		TEST_X509_VIEW("api.example.test"),
		TEST_X509_VIEW("other.example.test"),
		TEST_X509_VIEW("127.0.0.1"),
		TEST_X509_VIEW("2001:db8::1")
	};
	xx509cert Cert;
	xx509cert BeforeCert;
	xx509genname Name;
	xx509genname BeforeName;
	xx509result Result;

	memset(&Cert, 0xA5, sizeof(Cert));
	BeforeCert = Cert;
	if ( !xrtX509Parse(pDer, iSize, &Cert) ) {
		testRequire(memcmp(&Cert, &BeforeCert, sizeof(Cert)) == 0,
			"mutated identity certificate changed failed parse output");
		return;
	}
	(*pParsed)++;
	for ( size_t i = 0; i < (sizeof(Hosts) / sizeof(Hosts[0])); i++ ) {
		memset(&Name, 0xA5, sizeof(Name));
		BeforeName = Name;
		Result = xrtX509MatchHost(&Cert, Hosts[i], &Name);
		if ( Result != X509_VALUE ) {
			testRequire(memcmp(&Name, &BeforeName, sizeof(Name)) == 0,
				"mutated identity changed terminal output");
		}
		if ( Result == X509_ERROR ) {
			testRequire(xrtErrorCode(xrtGetError()) == X509_ERROR_IDENTITY,
				"mutated identity returned an unstable error code");
			(*pIdentityErrors)++;
		}
	}
}



#undef TEST_X509_VIEW



/* 变异完整证书并让 identity 层处理仍可解析的输入。 */
static void testX509IdentityMutationCertificates(void)
{
	uint8 Mutated[sizeof(X509_PROFILE_VALID)];
	uint32 iState = UINT32_C(0xA341316C);
	size_t iParsed = 0;
	size_t iIdentityErrors = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, X509_PROFILE_VALID, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testX509IdentityMutationCert(
				Mutated, sizeof(Mutated), &iParsed, &iIdentityErrors
			);
		}
	}
	for ( size_t i = 0; i < 2048u; i++ ) {
		memcpy(Mutated, X509_PROFILE_VALID, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		testX509IdentityMutationCert(
			Mutated, sizeof(Mutated), &iParsed, &iIdentityErrors
		);
	}
	testRequire((iParsed != 0) && (iIdentityErrors != 0),
		"identity certificate mutations did not reach both result classes");
}



/* 以任意字节和边界长度验证低层 DNS matcher 的内存安全。 */
static void testX509IdentityMutationDns(void)
{
	char Pattern[320];
	char Host[320];
	uint32 iState = UINT32_C(0xC8013EA4);
	size_t iValues = 0;
	size_t iDone = 0;
	size_t iErrors = 0;

	for ( size_t i = 0; i < 8192u; i++ ) {
		size_t iPatternSize;
		size_t iHostSize;
		xx509result Result;

		iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
		iPatternSize = iState % sizeof(Pattern);
		iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
		iHostSize = iState % sizeof(Host);
		for ( size_t j = 0; j < iPatternSize; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Pattern[j] = (char)(iState >> 24u);
		}
		for ( size_t j = 0; j < iHostSize; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Host[j] = (char)(iState >> 24u);
		}
		Result = xrtX509MatchDns(
			(xstrview) { Pattern, iPatternSize },
			(xstrview) { Host, iHostSize }
		);
		if ( Result == X509_VALUE ) {
			iValues++;
		} else if ( Result == X509_DONE ) {
			iDone++;
		} else {
			testRequire(xrtErrorCode(xrtGetError()) == X509_ERROR_DNS_NAME,
				"mutated DNS identity returned an unstable error code");
			iErrors++;
		}
	}
	testRequire((iDone + iErrors) != 0,
		"DNS identity mutations did not produce terminal results");
	(void)iValues;
}



/* 执行 X.509 identity 证书和独立 DNS 输入变异测试。 */
int main(void)
{
	testX509IdentityMutationCertificates();
	testX509IdentityMutationDns();
	printf("[PASS] x509_identity_mutation\n");
	return 0;
}
