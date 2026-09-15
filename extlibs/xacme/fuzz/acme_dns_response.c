#include <stdlib.h>
#include <string.h>

#include "../src/internal/xacme_dnstxt.h"



/* 契约：成功输出必须落在缓冲内且计数受容量约束。 */
static void __xacmeDnsFuzzContract(
	const char (*sRecords)[XACME_TXT_RECORD_MAX],
	size_t iCount,
	size_t iCapacity
)
{
	if ( iCount > iCapacity ) {
		abort();
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( strlen(sRecords[i]) >= XACME_TXT_RECORD_MAX ) {
			abort();
		}
	}
}



/* 期望 ID 取输入前两字节，覆盖任意结构化响应。 */
int xacmeDnsResponseFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	char arrRecords[4][XACME_TXT_RECORD_MAX];
	uint16 uExpectId = 0u;
	size_t iCount = 0u;

	if ( (pData == NULL) && (iSize != 0u) ) {
		return 0;
	}
	if ( iSize >= 2u ) {
		uExpectId = (uint16)(((uint16)pData[0] << 8u) | pData[1]);
	}
	if ( xacmeTxtParseResponse(
			pData, iSize, uExpectId,
			arrRecords, 4u, &iCount
		) ) {
		__xacmeDnsFuzzContract(arrRecords, iCount, 4u);
	}
	xrtClearError();

	/* 固定 ID 的第二视角：命中 ID 路径差异。 */
	iCount = 0u;
	if ( xacmeTxtParseResponse(
			pData, iSize, 0x1234u, arrRecords, 4u, &iCount
		) ) {
		__xacmeDnsFuzzContract(arrRecords, iCount, 4u);
	}
	xrtClearError();
	return 0;
}



#if defined(XACME_DNS_FUZZ_LIBFUZZER)

int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xacmeDnsResponseFuzzerTestOneInput(pData, iSize);
}

#endif
