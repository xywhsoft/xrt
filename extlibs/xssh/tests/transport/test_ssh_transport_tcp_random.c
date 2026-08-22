#include "../test.h"



/* 验证系统随机便利层只负责选择 padding 源。 */
int main(void)
{
	unsigned char arrPayload[512];
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	xsshtransporttcpconfig TransportConfig;
	xsshtransporttcp Transport;
	xsshkexinitconfig KexConfig;
	xsshwriter Writer;

	testRequire(xrtSshTransportTcpConfigInit(
		&TransportConfig,
		XSSH_ROLE_CLIENT
	) && xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&TransportConfig,
		0u
	) && (xrtSshTransportCoreIdentificationCommit(
		&Transport.Core,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		&Transport.Core,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && xrtSshKexInitConfigInit(
		&KexConfig,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&KexConfig
	) == XSSH_OK) && (xrtSshTransportTcpWritePrepare(
		&Transport,
		(xbytesview){ arrPayload, Writer.Size },
		1u
	) == XSSH_OK) &&
		(xrtSshTransportTcpWriteSize(&Transport) != 0u) &&
		(xrtSshTransportTcpWriteAbort(&Transport) == XSSH_OK),
		"ssh TCP secure-random prepare failed");
	xrtSshTransportTcpClear(&Transport);
	return 0;
}
