#define XRT_MODULE_TLS_STREAM
#define XRT_MODULE_WEBSOCKET_STREAM
#define XRT_MODULE_WEBSOCKET_UPGRADE
#include <xrt.h>

#include "public_struct_abi.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>



/* 返回裁剪功能配置下的结构尺寸和关键字段偏移。 */
static size_t __xrtAbiTrimLayout(xrtabitesttype Type, size_t iField)
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



/* 比较结构尺寸、全部可选字段偏移，并验证完整侧写入不会越过裁剪对象。 */
#define XRT_ABI_CHECK(Type, Id, Fields) \
	do { \
		struct { Type Value; unsigned char Guard[32]; } Probe; \
		size_t iField; \
		memset(&Probe, 0x5A, sizeof(Probe)); \
		for ( iField = 0; iField < (Fields); iField++ ) { \
			if ( xrtAbiFullLayout((Id), iField) != \
				__xrtAbiTrimLayout((Id), iField) ) { \
				return (int)((Id) + 1); \
			} \
		} \
		xrtAbiFullWrite((Id), &Probe.Value); \
		for ( iField = 0; iField < sizeof(Probe.Guard); iField++ ) { \
			if ( Probe.Guard[iField] != 0x5A ) { \
				return (int)((Id) + 20); \
			} \
		} \
	} while ( 0 )



/* 完整运行库与裁剪扩展库必须共享完全一致的公开数据 ABI。 */
int main(void)
{
	XRT_ABI_CHECK(xtlsclientconfig, XRT_ABI_TLS_CLIENT_CONFIG, 4);
	XRT_ABI_CHECK(xtlsserverconfig, XRT_ABI_TLS_SERVER_CONFIG, 4);
	XRT_ABI_CHECK(xtlsstreamconfig, XRT_ABI_TLS_STREAM_CONFIG, 4);
	XRT_ABI_CHECK(xtlsstreamevents, XRT_ABI_TLS_STREAM_EVENTS, 2);
	XRT_ABI_CHECK(xwsstreamconfig, XRT_ABI_WS_STREAM_CONFIG, 5);
	XRT_ABI_CHECK(
		xwsupgradeserverconfig,
		XRT_ABI_WS_UPGRADE_SERVER_CONFIG,
		5
	);
	XRT_ABI_CHECK(
		xwsupgradeclientconfig,
		XRT_ABI_WS_UPGRADE_CLIENT_CONFIG,
		4
	);
	XRT_ABI_CHECK(xwsupgrade, XRT_ABI_WS_UPGRADE_RESULT, 5);
	puts("[PASS] public struct ABI across trimmed feature sets");
	return 0;
}
