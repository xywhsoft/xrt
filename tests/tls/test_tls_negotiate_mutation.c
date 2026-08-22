#include "../test.h"



/* 变异 key_share 只能产生有效选择或失败原子的错误。 */
static xtlsitemresult testTlsKeyShareMutationOne(
	const uint8* pData,
	size_t iSize
)
{
	static const uint8 GroupData[] = {
		0x00, 0x1D, 0x00, 0x17, 0x00, 0x18
	};
	static const uint16 Preferred[] = {
		XTLS_GROUP_X25519, XTLS_GROUP_SECP256R1,
		XTLS_GROUP_SECP384R1
	};
	xtlsids Groups = { { GroupData, sizeof(GroupData) } };
	xtlskeyshareselection Selection;
	xtlskeyshareselection Before;
	xtlsitemresult Result;

	memset(&Selection, 0xA5, sizeof(Selection));
	Before = Selection;
	Result = xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { pData, iSize },
		Preferred, 3, XTLS_KEY_SHARE_PREFER_READY, &Selection
	);
	if ( Result != XTLS_ITEM_VALUE ) {
		testRequire(memcmp(&Selection, &Before, sizeof(Selection)) == 0,
			"mutated TLS key-share changed failed output");
	} else {
		testRequire(
			(Selection.Share.Group == XTLS_GROUP_X25519) ||
			(Selection.Share.Group == XTLS_GROUP_SECP256R1) ||
			(Selection.Share.Group == XTLS_GROUP_SECP384R1),
			"mutated TLS key-share selected an unconfigured group"
		);
		if ( Selection.Retry ) {
			testRequire(Selection.Share.Key.Size == 0u,
				"retry selection published key material");
		} else {
			testRequire((Selection.Share.Key.Data >= pData) &&
				(Selection.Share.Key.Data < pData + iSize),
				"selected key-share view escaped mutated input");
		}
	}
	return Result;
}



/* 对 key_share 执行单比特和确定性多字节变异。 */
int main(void)
{
	static const uint8 Seed[] = {
		0x00, 0x0E,
		0x00, 0x1D, 0x00, 0x03, 0x11, 0x22, 0x33,
		0x00, 0x17, 0x00, 0x03, 0x04, 0x44, 0x55
	};
	uint8 Mutated[sizeof(Seed)];
	uint32 iState = UINT32_C(0xA341316C);
	size_t iValue = 0;
	size_t iDone = 0;
	size_t iError = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			xtlsitemresult Result;

			memcpy(Mutated, Seed, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			Result = testTlsKeyShareMutationOne(
				Mutated, sizeof(Mutated)
			);
			iValue += Result == XTLS_ITEM_VALUE;
			iDone += Result == XTLS_ITEM_DONE;
			iError += Result == XTLS_ITEM_ERROR;
			iCases++;
		}
	}
	for ( size_t i = 0; i < 2048u; i++ ) {
		xtlsitemresult Result;

		memcpy(Mutated, Seed, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) +
				UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		Result = testTlsKeyShareMutationOne(Mutated, sizeof(Mutated));
		iValue += Result == XTLS_ITEM_VALUE;
		iDone += Result == XTLS_ITEM_DONE;
		iError += Result == XTLS_ITEM_ERROR;
		iCases++;
	}
	testRequire((iValue != 0) && (iError != 0),
		"TLS negotiation mutation corpus missed a result class");
	printf(
		"[PASS] tls_negotiate_mutation cases=%zu value=%zu done=%zu errors=%zu\n",
		iCases, iValue, iDone, iError
	);
	return 0;
}
