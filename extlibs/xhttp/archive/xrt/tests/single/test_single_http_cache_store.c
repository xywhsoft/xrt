#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件保留不可变 Record 和线程安全内存 Cache。 */
int main(void)
{
	static const uint8 Body[] = { 'o', 'k' };
	static const uint8 UpdatedBody[] = { 'n', 'e', 'w' };
	xhttpcachepart Part = {
		0, { Body, sizeof(Body) }
	};
	xhttpcachepart UpdatedPart = {
		0, { UpdatedBody, sizeof(UpdatedBody) }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord = NULL;
	xhttpcacherecord* pUpdated = NULL;
	xhttpcacherecord* pHit = NULL;
	xhttpcache* pCache = NULL;
	const xhttpcachepart* pCurrent;
	bool bPass = false;

	if ( !xrtHttpCacheKeyInit(
		&Key,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/single")
	) || !xrtHttpCacheRecordInputInit(
		&Input, &Key, XHTTP_STATUS_OK
	) ) {
		goto done;
	}
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = sizeof(Body);
	pRecord = xrtHttpCacheRecordCreate(&Input);
	pCache = xrtHttpCacheCreate(NULL);
	if ( (pRecord == NULL) || (pCache == NULL) ||
		(xrtHttpCacheInsert(
			pCache, pRecord
		) != XHTTP_CACHE_PUT_STORED) ||
		(xrtHttpCacheGet(
			pCache, &Key, &pHit
		) != XHTTP_CACHE_LOOKUP_HIT) ) {
		goto done;
	}

	Input.Parts = &UpdatedPart;
	Input.Length = sizeof(UpdatedBody);
	pUpdated = xrtHttpCacheRecordCreate(&Input);
	if ( (pUpdated == NULL) ||
		(xrtHttpCacheReplace(
			pCache,
			pHit,
			pUpdated
		 ) != XHTTP_CACHE_PUT_REPLACED) ) {
		goto done;
	}
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;
	if ( xrtHttpCacheGet(
		pCache, &Key, &pHit
	) != XHTTP_CACHE_LOOKUP_HIT ) {
		goto done;
	}
	pCurrent = xrtHttpCacheRecordPartAt(pHit, 0);
	bPass = (pCurrent != NULL) &&
		(pCurrent->Data.Size == sizeof(UpdatedBody)) &&
		(pCurrent->Data.Data[0] == 'n') &&
		(xrtHttpCacheRemoveRecord(
			pCache,
			pHit
		 ) == XHTTP_CACHE_CHANGE_APPLIED);

done:
	xrtHttpCacheRecordRelease(pHit);
	xrtHttpCacheRecordRelease(pUpdated);
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(pCache);
	printf(
		"%s single-http-cache-store\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
