#define XHTTP_MODULE_HTTP_CLIENT
#include <xhttp.h>

#include "public_struct_abi.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>



/* 返回最小 HTTP Client 配置下的结构尺寸和关键字段偏移。 */
static size_t __xhttpAbiTrimLayout(xhttpabitesttype Type, size_t iField)
{
	switch ( Type ) {
		case XHTTP_ABI_CALL_INFO:
			switch ( iField ) {
				case 0: return sizeof(xhttpcallinfo);
				case 1: return offsetof(xhttpcallinfo, Redirects);
				case 2: return offsetof(xhttpcallinfo, Retries);
				case 3: return offsetof(xhttpcallinfo, Cache);
			}
			break;
		case XHTTP_ABI_CLIENT_STATS:
			switch ( iField ) {
				case 0: return sizeof(xhttpclientstats);
				case 1: return offsetof(xhttpclientstats, PoolRejected);
				case 2: return offsetof(xhttpclientstats, RedirectsFollowed);
			}
			break;
		case XHTTP_ABI_CLIENT_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xhttpclientconfig);
				case 1: return offsetof(xhttpclientconfig, Proxy);
				case 2: return offsetof(xhttpclientconfig, Redirect);
				case 3: return offsetof(xhttpclientconfig, Retry);
				case 4: return offsetof(xhttpclientconfig, Cookies);
				case 5: return offsetof(xhttpclientconfig, Decompress);
				case 6: return offsetof(xhttpclientconfig, Cache);
				case 7: return offsetof(xhttpclientconfig, Pool);
				case 8: return offsetof(xhttpclientconfig, TlsStream);
				case 9: return offsetof(xhttpclientconfig, TlsContext);
				case 10: return offsetof(xhttpclientconfig, TlsVerifier);
				case 11: return offsetof(xhttpclientconfig, SystemTrust);
				case 12: return offsetof(xhttpclientconfig, Resume);
			}
			break;
		case XHTTP_ABI_CALL_OPTIONS:
			switch ( iField ) {
				case 0: return sizeof(xhttpcalloptions);
				case 1: return offsetof(xhttpcalloptions, Proxy);
				case 2: return offsetof(xhttpcalloptions, Redirect);
				case 3: return offsetof(xhttpcalloptions, Retry);
				case 4: return offsetof(xhttpcalloptions, Cookies);
				case 5: return offsetof(xhttpcalloptions, Decompress);
				case 6: return offsetof(xhttpcalloptions, Cache);
			}
			break;
		case XHTTP_ABI_CALL_RESULT:
			switch ( iField ) {
				case 0: return sizeof(xhttpcallresult);
				case 1: return offsetof(xhttpcallresult, Tls);
				case 2: return offsetof(xhttpcallresult, Info);
				case 3: return offsetof(xhttpcallresult, Buffered);
				case 4: return offsetof(xhttpcallresult, Upgraded);
			}
			break;
		case XHTTP_ABI_HTTP1_CALL_RESULT:
			switch ( iField ) {
				case 0: return sizeof(xhttp1callresult);
				case 1: return offsetof(xhttp1callresult, Tls);
				case 2: return offsetof(xhttp1callresult, Error);
				case 3: return offsetof(xhttp1callresult, Buffered);
				case 4: return offsetof(xhttp1callresult, Upgraded);
			}
			break;
		default:
			break;
	}
	return SIZE_MAX;
}



/* 比较尺寸、字段偏移，并验证完整侧写入不会越过裁剪侧对象。 */
#define XHTTP_ABI_CHECK(Type, Id, Fields) \
	do { \
		struct { Type Value; unsigned char Guard[32]; } Probe; \
		size_t iField; \
		memset(&Probe, 0x5A, sizeof(Probe)); \
		for ( iField = 0; iField < (Fields); iField++ ) { \
			if ( xhttpAbiFullLayout((Id), iField) != \
				__xhttpAbiTrimLayout((Id), iField) ) { \
				return (int)((Id) + 1); \
			} \
		} \
		xhttpAbiFullWrite((Id), &Probe.Value); \
		for ( iField = 0; iField < sizeof(Probe.Guard); iField++ ) { \
			if ( Probe.Guard[iField] != 0x5A ) { \
				return (int)((Id) + 20); \
			} \
		} \
	} while ( 0 )



/* 完整 xhttp 与最小客户端必须共享完全一致的公开数据 ABI。 */
int main(void)
{
	XHTTP_ABI_CHECK(xhttpcallinfo, XHTTP_ABI_CALL_INFO, 4);
	XHTTP_ABI_CHECK(xhttpclientstats, XHTTP_ABI_CLIENT_STATS, 3);
	XHTTP_ABI_CHECK(xhttpclientconfig, XHTTP_ABI_CLIENT_CONFIG, 13);
	XHTTP_ABI_CHECK(xhttpcalloptions, XHTTP_ABI_CALL_OPTIONS, 7);
	XHTTP_ABI_CHECK(xhttpcallresult, XHTTP_ABI_CALL_RESULT, 5);
	XHTTP_ABI_CHECK(xhttp1callresult, XHTTP_ABI_HTTP1_CALL_RESULT, 5);
	puts("[PASS] xhttp public struct ABI across trimmed feature sets");
	return 0;
}
