#include "../test.h"
#include "../fixtures/x509_profile_vectors.h"



/* 检查一次变异输入上的证书与 profile 输出原子性。 */
static void testX509ProfileMutationOne(
	const uint8* pDer,
	size_t iSize,
	size_t* pParsed,
	size_t* pProfileErrors
)
{
	xx509cert Cert;
	xx509cert BeforeCert;
	xx509gencursor Names;
	xx509gencursor BeforeNames;
	xx509genname Name;
	xx509genname BeforeName;
	xx509oidcursor Oids;
	xx509oidcursor BeforeOids;
	xbytesview Oid;
	xbytesview BeforeOid;
	xbytesview KeyId;
	xbytesview BeforeKeyId;
	xx509basicconstraints Constraints;
	xx509basicconstraints BeforeConstraints;
	xx509result Result;
	uint16 iUsage;

	memset(&Cert, 0xA5, sizeof(Cert));
	BeforeCert = Cert;
	if ( !xrtX509Parse(pDer, iSize, &Cert) ) {
		testRequire(memcmp(&Cert, &BeforeCert, sizeof(Cert)) == 0,
			"mutated X.509 parse changed failed output");
		return;
	}
	(*pParsed)++;

	memset(&Names, 0xA5, sizeof(Names));
	BeforeNames = Names;
	Result = xrtX509SubjectAltName(&Cert, &Names);
	if ( Result != X509_VALUE ) {
		testRequire(memcmp(&Names, &BeforeNames, sizeof(Names)) == 0,
			"mutated SAN changed failed or absent output");
		if ( Result == X509_ERROR ) {
			(*pProfileErrors)++;
		}
	} else {
		for ( size_t i = 0; i < 64u; i++ ) {
			BeforeNames = Names;
			memset(&Name, 0xA5, sizeof(Name));
			BeforeName = Name;
			Result = xrtX509GeneralNameRead(&Names, &Name);
			if ( Result == X509_VALUE ) {
				continue;
			}
			testRequire((memcmp(
				&Names, &BeforeNames, sizeof(Names)
			) == 0) && (memcmp(
				&Name, &BeforeName, sizeof(Name)
			) == 0), "mutated GeneralName changed terminal output");
			if ( Result == X509_ERROR ) {
				(*pProfileErrors)++;
			}
			break;
		}
		testRequire(Result != X509_VALUE,
			"mutated GeneralNames cursor did not terminate");
	}

	iUsage = UINT16_C(0xA55A);
	Result = xrtX509KeyUsage(&Cert, &iUsage);
	if ( Result != X509_VALUE ) {
		testRequire(iUsage == UINT16_C(0xA55A),
			"mutated KeyUsage changed failed or absent output");
		if ( Result == X509_ERROR ) {
			(*pProfileErrors)++;
		}
	}

	memset(&Constraints, 0xA5, sizeof(Constraints));
	BeforeConstraints = Constraints;
	Result = xrtX509BasicConstraints(&Cert, &Constraints);
	if ( Result != X509_VALUE ) {
		testRequire(memcmp(
			&Constraints, &BeforeConstraints, sizeof(Constraints)
		) == 0, "mutated BasicConstraints changed failed or absent output");
		if ( Result == X509_ERROR ) {
			(*pProfileErrors)++;
		}
	}

	memset(&Oids, 0xA5, sizeof(Oids));
	BeforeOids = Oids;
	Result = xrtX509ExtendedKeyUsage(&Cert, &Oids);
	if ( Result != X509_VALUE ) {
		testRequire(memcmp(&Oids, &BeforeOids, sizeof(Oids)) == 0,
			"mutated EKU changed failed or absent output");
		if ( Result == X509_ERROR ) {
			(*pProfileErrors)++;
		}
	} else {
		for ( size_t i = 0; i < 64u; i++ ) {
			BeforeOids = Oids;
			memset(&Oid, 0xA5, sizeof(Oid));
			BeforeOid = Oid;
			Result = xrtX509OidRead(&Oids, &Oid);
			if ( Result == X509_VALUE ) {
				continue;
			}
			testRequire((memcmp(
				&Oids, &BeforeOids, sizeof(Oids)
			) == 0) && (memcmp(
				&Oid, &BeforeOid, sizeof(Oid)
			) == 0), "mutated OID cursor changed terminal output");
			if ( Result == X509_ERROR ) {
				(*pProfileErrors)++;
			}
			break;
		}
		testRequire(Result != X509_VALUE,
			"mutated OID cursor did not terminate");
	}

	memset(&KeyId, 0xA5, sizeof(KeyId));
	BeforeKeyId = KeyId;
	Result = xrtX509SubjectKeyId(&Cert, &KeyId);
	if ( Result != X509_VALUE ) {
		testRequire(memcmp(&KeyId, &BeforeKeyId, sizeof(KeyId)) == 0,
			"mutated SKI changed failed or absent output");
		if ( Result == X509_ERROR ) {
			(*pProfileErrors)++;
		}
	}
}



/* 对全部字节执行单比特翻转，并补充确定性多字节变异。 */
int main(void)
{
	uint8 Mutated[sizeof(X509_PROFILE_VALID)];
	uint32 iState = UINT32_C(0x6D2B79F5);
	size_t iParsed = 0;
	size_t iProfileErrors = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, X509_PROFILE_VALID, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testX509ProfileMutationOne(
				Mutated, sizeof(Mutated), &iParsed, &iProfileErrors
			);
			iCases++;
		}
	}
	for ( size_t i = 0; i < 4096u; i++ ) {
		memcpy(Mutated, X509_PROFILE_VALID, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		testX509ProfileMutationOne(
			Mutated, sizeof(Mutated), &iParsed, &iProfileErrors
		);
		iCases++;
	}
	testRequire((iParsed != 0) && (iProfileErrors != 0),
		"X.509 profile mutation corpus did not reach both result classes");
	printf(
		"[PASS] x509_profile_mutation cases=%zu parsed=%zu profile-errors=%zu\n",
		iCases, iParsed, iProfileErrors
	);
	return 0;
}
