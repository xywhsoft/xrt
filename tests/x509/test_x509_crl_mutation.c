#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"



/* 检查一次变异 CRL 的终止性及全部借用输出的失败原子性。 */
static void testX509CrlMutationOne(
	const uint8* pDer,
	size_t iSize,
	size_t* pParsed,
	size_t* pEntries
)
{
	xx509crl Crl;
	xx509crl BeforeCrl;
	xx509crlcursor Cursor;
	xx509crlcursor BeforeCursor;
	xx509crlentry Entry;
	xx509crlentry BeforeEntry;
	xx509result Result;

	memset(&Crl, 0xA5, sizeof(Crl));
	BeforeCrl = Crl;
	if ( !xrtX509CrlParse(pDer, iSize, &Crl) ) {
		testRequire(memcmp(&Crl, &BeforeCrl, sizeof(Crl)) == 0,
			"mutated CRL changed failed parse output");
		return;
	}
	(*pParsed)++;
	memset(&Cursor, 0xA5, sizeof(Cursor));
	BeforeCursor = Cursor;
	if ( !xrtX509CrlEntryInit(&Crl, &Cursor) ) {
		testRequire(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0,
			"mutated CRL changed failed cursor output");
		return;
	}
	for ( size_t i = 0; i < 64u; i++ ) {
		BeforeCursor = Cursor;
		memset(&Entry, 0xA5, sizeof(Entry));
		BeforeEntry = Entry;
		Result = xrtX509CrlEntryRead(&Cursor, &Entry);
		if ( Result == X509_VALUE ) {
			(*pEntries)++;
			continue;
		}
		testRequire((memcmp(
			&Cursor, &BeforeCursor, sizeof(Cursor)
		) == 0) && (memcmp(
			&Entry, &BeforeEntry, sizeof(Entry)
		) == 0), "mutated CRL changed terminal cursor output");
		return;
	}
	testRequire(false, "mutated CRL entry cursor did not terminate");
}



/* 对完整 v2 CRL 执行单比特翻转和确定性多字节变异。 */
int main(void)
{
	uint8 Mutated[sizeof(X509_CRL_V2)];
	uint32 iState = UINT32_C(0x8A5CD789);
	size_t iParsed = 0;
	size_t iEntries = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, X509_CRL_V2, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testX509CrlMutationOne(
				Mutated, sizeof(Mutated), &iParsed, &iEntries
			);
			iCases++;
		}
	}
	for ( size_t i = 0; i < 4096u; i++ ) {
		memcpy(Mutated, X509_CRL_V2, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		testX509CrlMutationOne(
			Mutated, sizeof(Mutated), &iParsed, &iEntries
		);
		iCases++;
	}
	testRequire((iParsed != 0) && (iEntries != 0),
		"CRL mutation corpus missed successful structural paths");
	printf(
		"[PASS] x509_crl_mutation cases=%zu parsed=%zu entries=%zu\n",
		iCases, iParsed, iEntries
	);
	return 0;
}
