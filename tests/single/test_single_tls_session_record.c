#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的会话记录组合层可完成受保护应用数据往返。 */
int main(void)
{
	static const uint8 Key[16] = { 0 };
	static const uint8 Iv[12] = { 0 };
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	xnetspan Span;
	char sPlain[8] = { 0 };
	size_t iWritten = 0;
	size_t iRead = 0;
	bool bResult = false;

	if ( pContext == NULL ) {
		return 1;
	}
	pSender = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	pReceiver = __xrtTlsSessionCreate(pContext, NULL, XTLS_SERVER);
	xrtTlsContextRelease(pContext);
	if ( (pSender == NULL) || (pReceiver == NULL) ) {
		goto Exit;
	}
	if ( !__xrtTlsSessionWriteKey(
		pSender, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { Key, sizeof(Key) },
		(xbytesview) { Iv, sizeof(Iv) }
	) || !__xrtTlsSessionReadKey(
		pReceiver, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { Key, sizeof(Key) },
		(xbytesview) { Iv, sizeof(Iv) }
	) || !__xrtTlsSessionSetState(
		pSender, XTLS_STATE_HANDSHAKE
	) || !__xrtTlsSessionSetState(
		pSender, XTLS_STATE_READY
	) || !__xrtTlsSessionSetState(
		pReceiver, XTLS_STATE_HANDSHAKE
	) || !__xrtTlsSessionSetState(
		pReceiver, XTLS_STATE_READY
	) || (xrtTlsSessionWrite(
		pSender, "single", 6u, &iWritten
	) != XTLS_OK) || (iWritten != 6u) ) {
		goto Exit;
	}
	while ( xrtTlsSessionSendSize(pSender) != 0 ) {
		if ( !xrtTlsSessionSendFront(pSender, &Span) ||
			(xrtTlsSessionFeed(
				pReceiver, Span.Data, Span.Size
			) != XTLS_OK) ||
			!xrtTlsSessionSendConsume(pSender, Span.Size) ) {
			goto Exit;
		}
	}
	if ( (__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) != XTLS_OK) || !Record.Protected ||
		(Record.Type != XTLS_RECORD_APPLICATION_DATA) ||
		(__xrtTlsSessionRecordFinish(pReceiver, true) != XTLS_OK) ||
		(xrtTlsSessionRead(
			pReceiver, sPlain, sizeof(sPlain), &iRead
		) != XTLS_OK) || (iRead != 6u) ||
		(memcmp(sPlain, "single", 6u) != 0) ) {
		goto Exit;
	}
	bResult = true;

Exit:
	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	return bResult ? 0 : 1;
}
