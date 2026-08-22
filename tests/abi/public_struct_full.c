#define XRT_MODULE_TLS_CLIENT_RESUME
#define XRT_MODULE_TLS_SERVER_RESUME
#define XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_WEBSOCKET_STREAM_DEFLATE
#define XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE
#include <xrt.h>

#include "public_struct_abi.h"
#include <stddef.h>
#include <string.h>



/* 返回完整功能配置下的结构尺寸和全部可选字段偏移。 */
size_t xrtAbiFullLayout(xrtabitesttype Type, size_t iField)
{
	switch ( Type ) {
		case XRT_ABI_TLS_CLIENT_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xtlsclientconfig);
				case 1: return offsetof(xtlsclientconfig, Verifier);
				case 2: return offsetof(xtlsclientconfig, Resume);
				case 3: return offsetof(xtlsclientconfig, ResumeLimit);
			}
			break;
		case XRT_ABI_TLS_SERVER_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xtlsserverconfig);
				case 1: return offsetof(xtlsserverconfig, Resume);
				case 2: return offsetof(xtlsserverconfig, ResumeContext);
				case 3: return offsetof(xtlsserverconfig, ResumeAgeTolerance);
			}
			break;
		case XRT_ABI_TLS_STREAM_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xtlsstreamconfig);
				case 1: return offsetof(xtlsstreamconfig, AsyncBytesLimit);
				case 2: return offsetof(xtlsstreamconfig, AsyncCountLimit);
				case 3: return offsetof(xtlsstreamconfig, AsyncBatch);
			}
			break;
		case XRT_ABI_TLS_STREAM_EVENTS:
			switch ( iField ) {
				case 0: return sizeof(xtlsstreamevents);
				case 1: return offsetof(xtlsstreamevents, Ticket);
			}
			break;
		case XRT_ABI_WS_STREAM_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xwsstreamconfig);
				case 1: return offsetof(xwsstreamconfig, Deflate);
				case 2: return offsetof(xwsstreamconfig, Inflater);
				case 3: return offsetof(xwsstreamconfig, Deflater);
				case 4: return offsetof(xwsstreamconfig, DeflateEnabled);
			}
			break;
		case XRT_ABI_WS_UPGRADE_SERVER_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xwsupgradeserverconfig);
				case 1: return offsetof(xwsupgradeserverconfig, AcceptDeflate);
				case 2: return offsetof(xwsupgradeserverconfig, DeflateData);
				case 3: return offsetof(xwsupgradeserverconfig, EnableDeflate);
				case 4: return offsetof(xwsupgradeserverconfig, RequireDeflate);
			}
			break;
		case XRT_ABI_WS_UPGRADE_CLIENT_CONFIG:
			switch ( iField ) {
				case 0: return sizeof(xwsupgradeclientconfig);
				case 1: return offsetof(xwsupgradeclientconfig, Deflate);
				case 2: return offsetof(xwsupgradeclientconfig, EnableDeflate);
				case 3: return offsetof(xwsupgradeclientconfig, RequireDeflate);
			}
			break;
		case XRT_ABI_WS_UPGRADE_RESULT:
			switch ( iField ) {
				case 0: return sizeof(xwsupgrade);
				case 1: return offsetof(xwsupgrade, Deflate);
				case 2: return offsetof(xwsupgrade, Extensions);
				case 3: return offsetof(xwsupgrade, ExtensionSize);
				case 4: return offsetof(xwsupgrade, DeflateEnabled);
			}
			break;
		default:
			break;
	}
	return SIZE_MAX;
}



/* 使用完整布局写入对象，裁剪侧尾部哨兵可以直接发现尺寸缩短。 */
void xrtAbiFullWrite(xrtabitesttype Type, void* pData)
{
	size_t iSize = xrtAbiFullLayout(Type, 0);

	if ( (pData != NULL) && (iSize != SIZE_MAX) ) {
		memset(pData, 0xA5, iSize);
	}
}
