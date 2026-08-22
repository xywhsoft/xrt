#define XHTTP_MODULE_ALL
#include <xhttp.h>

#include "public_struct_abi.h"
#include <stddef.h>
#include <string.h>



/* 返回完整功能配置下的结构尺寸和全部可选字段偏移。 */
size_t xhttpAbiFullLayout(xhttpabitesttype Type, size_t iField)
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



/* 使用完整布局写入对象，裁剪侧尾部哨兵可以直接发现尺寸缩短。 */
void xhttpAbiFullWrite(xhttpabitesttype Type, void* pData)
{
	size_t iSize = xhttpAbiFullLayout(Type, 0);

	if ( (pData != NULL) && (iSize != SIZE_MAX) ) {
		memset(pData, 0xA5, iSize);
	}
}
