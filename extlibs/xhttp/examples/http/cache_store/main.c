#include <stdio.h>
#include <string.h>

#include <xrt/http_cache_store.h>



/* 创建、条件更新并删除一个按 Accept-Encoding 区分的缓存响应。 */
int main(void)
{
	static const xhttpfield RequestFields[] = {
		{
			XRT_STR_INIT("Accept-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	static const xhttpfield ResponseFields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60")
		}
	};
	static const uint8 Body[] = "{\"cached\":true}";
	static const uint8 UpdatedBody[] = "{\"cached\":\"updated\"}";
	xhttpcachepart Part = {
		0, { Body, sizeof(Body) - 1u }
	};
	xhttpcachepart UpdatedPart = {
		0, { UpdatedBody, sizeof(UpdatedBody) - 1u }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord = NULL;
	xhttpcacherecord* pUpdated = NULL;
	xhttpcacherecord* pHit = NULL;
	xhttpcache* pCache = NULL;
	const xhttpcachepart* pBody;
	int iResult = 1;

	if ( !xrtHttpCacheKeyInit(
		&Key,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://api.example.test/status")
	) ) {
		goto done;
	}
	Key.Fields = RequestFields;
	Key.FieldCount = sizeof(RequestFields) /
		sizeof(RequestFields[0]);
	if ( !xrtHttpCacheRecordInputInit(
		&Input, &Key, XHTTP_STATUS_OK
	) ) {
		goto done;
	}
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = ResponseFields;
	Input.FieldCount = sizeof(ResponseFields) /
		sizeof(ResponseFields[0]);
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = Part.Data.Size;
	Input.ResponseTime = xrtNow();
	Input.RequestClock = xrtClock();
	Input.ResponseClock = Input.RequestClock;
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

	/* 根据命中的不可变快照构造新版本并执行条件替换。 */
	Input.Parts = &UpdatedPart;
	Input.Length = UpdatedPart.Data.Size;
	Input.ResponseTime = xrtNow();
	Input.RequestClock = xrtClock();
	Input.ResponseClock = Input.RequestClock;
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

	/* 重新读取当前版本；条件删除不会误删并发替换的新版本。 */
	if ( xrtHttpCacheGet(
		pCache, &Key, &pHit
	) != XHTTP_CACHE_LOOKUP_HIT ) {
		goto done;
	}
	pBody = xrtHttpCacheRecordPartAt(pHit, 0);
	if ( pBody == NULL ) {
		goto done;
	}
	printf(
		"status=%u body=%.*s\n",
		(unsigned)xrtHttpCacheRecordStatus(pHit),
		(int)pBody->Data.Size,
		(const char*)pBody->Data.Data
	);
	if ( xrtHttpCacheRemoveRecord(
		pCache,
		pHit
	) != XHTTP_CACHE_CHANGE_APPLIED ) {
		goto done;
	}
	iResult = 0;

done:
	xrtHttpCacheRecordRelease(pHit);
	xrtHttpCacheRecordRelease(pUpdated);
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(pCache);
	return iResult;
}
