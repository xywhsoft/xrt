#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件客户端会排队一条可解析的 TLS 1.3 ClientHello。 */
int main(void)
{
	xtlsclientconfig Config;
	xtlssession* pSession;
	xnetspan Span;
	xtlsrecord Record;
	xtlshandshake Handshake;
	xtlsclienthello Hello;
	bool bResult;

	xrtTlsClientConfigInit(&Config);
	Config.ServerName = XRT_STR_LITERAL("example.com");
	pSession = xrtTlsClientCreate(&Config, NULL);
	if ( pSession == NULL ) {
		return 1;
	}
	bResult = xrtTlsSessionSendFront(pSession, &Span) &&
		(xrtTlsRecordParse(
			(xbytesview) { Span.Data, Span.Size }, &Record, NULL
		) == XTLS_OK) &&
		(xrtTlsHandshakeParse(
			Record.Payload, &Handshake, NULL
		) == XTLS_OK) &&
		(Handshake.Type == XTLS_HANDSHAKE_CLIENT_HELLO) &&
		xrtTlsClientHelloParse(Handshake.Body, &Hello) &&
		(xrtTlsSessionState(pSession) == XTLS_STATE_HANDSHAKE);
	xrtTlsSessionDestroy(pSession);
	return bResult ? 0 : 1;
}
