#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件保留命名组元数据和当前已编译密码后端。 */
int main(void)
{
	static const uint16 Groups[] = {
		XTLS_GROUP_X25519,
		XTLS_GROUP_X448,
		XTLS_GROUP_SECP256R1,
		XTLS_GROUP_SECP384R1
	};
	uint8 PrivateA[56];
	uint8 PrivateB[56];
	uint8 PublicA[97];
	uint8 PublicB[97];
	uint8 SharedA[56];
	uint8 SharedB[56];
	bool bTested = false;

	if ( xrtTlsGroupInfo(XTLS_GROUP_X25519) == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < (sizeof(Groups) / sizeof(Groups[0])); i++ ) {
		const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(Groups[i]);

		if ( !xrtTlsGroupAvailable(Groups[i]) ) {
			continue;
		}
		if ( (pInfo == NULL) || !xrtTlsKeyShareGenerate(
			Groups[i], PrivateA, sizeof(PrivateA), PublicA, sizeof(PublicA)
		) || !xrtTlsKeyShareGenerate(
			Groups[i], PrivateB, sizeof(PrivateB), PublicB, sizeof(PublicB)
		) || !xrtTlsKeyShareDerive(
			Groups[i],
			(xbytesview) { PrivateA, pInfo->PrivateSize },
			(xbytesview) { PublicB, pInfo->PublicSize },
			SharedA, sizeof(SharedA)
		) || !xrtTlsKeyShareDerive(
			Groups[i],
			(xbytesview) { PrivateB, pInfo->PrivateSize },
			(xbytesview) { PublicA, pInfo->PublicSize },
			SharedB, sizeof(SharedB)
		) || (memcmp(SharedA, SharedB, pInfo->SharedSize) != 0) ) {
			return 1;
		}
		bTested = true;
	}
	printf("[PASS] single-tls-key-exchange backends=%s\n",
		bTested ? "present" : "trimmed");
	return 0;
}
