/*
 * XRT Example - TLS Session Handshake
 * XRT 范例 - TLS Session 握手
 *
 * Description / 说明:
 *   EN: Demonstrates driving a client/server TLS session handshake in memory, exchanging plaintext over the encrypted channel, and handling close-notify.
 *   CN: 演示在内存中驱动 client/server TLS session 握手、通过加密通道交换明文，以及处理 close-notify。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_tls_tls_session_handshake.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_tls_tls_session_handshake -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_common.h"
#include "../../example_tls_fixture.h"
#include <stdio.h>


static size_t procMoveCipher(xtlssession* pSrc, xtlssession* pDst, size_t iChunkCap)
{
	char aBuf[256];
	size_t iMoved = 0u;

	if ( !pSrc || !pDst || iChunkCap == 0u ) return 0u;

	while ( xrtNetTlsSessionPendingCipher(pSrc) > 0u ) {
		size_t iWant = xrtNetTlsSessionPendingCipher(pSrc);
		size_t iRead = 0u;

		if ( iWant > sizeof(aBuf) ) iWant = sizeof(aBuf);
		if ( iWant > iChunkCap ) iWant = iChunkCap;
		if ( xrtNetTlsSessionPeekCipher(pSrc, aBuf, iWant, &iRead) != XRT_NET_OK || iRead == 0u ) {
			break;
		}
		if ( xrtNetTlsSessionFeedCipher(pDst, aBuf, iRead) != XRT_NET_OK ) {
			return 0u;
		}
		xrtNetTlsSessionConsumeCipher(pSrc, iRead);
		iMoved += iRead;
	}

	return iMoved;
}


static bool procDriveHandshake(xtlssession* pClient, xtlssession* pServer)
{
	uint32 iStep;

	for ( iStep = 0u; iStep < 4096u; ++iStep ) {
		bool bProgress = false;
		xnet_result iClientRes = xrtNetTlsSessionDriveHandshake(pClient);
		xnet_result iServerRes = xrtNetTlsSessionDriveHandshake(pServer);

		if ( iClientRes != XRT_NET_OK && iClientRes != XRT_NET_AGAIN ) return false;
		if ( iServerRes != XRT_NET_OK && iServerRes != XRT_NET_AGAIN ) return false;

		if ( procMoveCipher(pClient, pServer, 37u) > 0u ) bProgress = true;
		if ( procMoveCipher(pServer, pClient, 53u) > 0u ) bProgress = true;

		if ( xrtNetTlsSessionIsReady(pClient) && xrtNetTlsSessionIsReady(pServer) ) {
			return true;
		}
		if ( !bProgress && xrtNetTlsSessionPendingRecv(pClient) == 0u && xrtNetTlsSessionPendingRecv(pServer) == 0u ) {
			break;
		}
	}

	return false;
}


static bool procTransferPlain(xtlssession* pSrc, xtlssession* pDst, const char* sPayload, char* sRecv, size_t iRecvCap, size_t* pRecvLen)
{
	size_t iPayloadLen;
	size_t iWritten = 0u;
	size_t iTotal = 0u;
	uint32 iStep;

	if ( pRecvLen ) *pRecvLen = 0u;
	if ( !pSrc || !pDst || !sPayload || !sRecv || iRecvCap == 0u ) return false;

	iPayloadLen = strlen(sPayload);
	if ( iPayloadLen + 1u > iRecvCap ) return false;
	memset(sRecv, 0, iRecvCap);

	if ( xrtNetTlsSessionWritePlain(pSrc, sPayload, iPayloadLen, &iWritten) != XRT_NET_OK || iWritten != iPayloadLen ) {
		return false;
	}

	for ( iStep = 0u; iStep < 4096u && iTotal < iPayloadLen; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( procMoveCipher(pSrc, pDst, 29u) > 0u ) bProgress = true;
		if ( procMoveCipher(pDst, pSrc, 17u) > 0u ) bProgress = true;

		do {
			iRes = xrtNetTlsSessionReadPlain(pDst, sRecv + iTotal, iPayloadLen - iTotal, &iRead);
			if ( iRes == XRT_NET_OK && iRead > 0u ) {
				iTotal += iRead;
				sRecv[iTotal] = '\0';
				bProgress = true;
			}
		} while ( iRes == XRT_NET_OK && iRead > 0u && iTotal < iPayloadLen );

		if ( !bProgress ) break;
	}

	if ( pRecvLen ) *pRecvLen = iTotal;
	return iTotal == iPayloadLen && memcmp(sRecv, sPayload, iPayloadLen) == 0;
}


static bool procCloseNotify(xtlssession* pSrc, xtlssession* pDst)
{
	char aBuf[64];
	uint32 iStep;

	for ( iStep = 0u; iStep < 2048u; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( iStep == 0u && xrtNetTlsSessionQueueClose(pSrc) != XRT_NET_OK ) return false;
		if ( procMoveCipher(pSrc, pDst, 19u) > 0u ) bProgress = true;
		if ( procMoveCipher(pDst, pSrc, 19u) > 0u ) bProgress = true;

		iRes = xrtNetTlsSessionReadPlain(pDst, aBuf, sizeof(aBuf), &iRead);
		if ( iRes == XRT_NET_CLOSED ) return true;
		if ( iRes == XRT_NET_OK && iRead > 0u ) bProgress = true;
		if ( !bProgress ) break;
	}

	return false;
}


int main(void)
{
	extlsfixture tFixture;
	xtlsconfig tClientCfg;
	xtlsconfig tServerCfg;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	char sServerRecv[64];
	char sClientRecv[64];
	size_t iServerRecvLen = 0u;
	size_t iClientRecvLen = 0u;
	bool bClosed = false;
	int iRet = 0;

	xrtInit();
	memset(&tFixture, 0, sizeof(tFixture));
	memset(&tClientCfg, 0, sizeof(tClientCfg));
	memset(&tServerCfg, 0, sizeof(tServerCfg));
	memset(sServerRecv, 0, sizeof(sServerRecv));
	memset(sClientRecv, 0, sizeof(sClientRecv));

	if ( !exTlsFixtureInit(&tFixture, "network_tls_session") ) {
		printf("fixture = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tServerCfg.sCertFile = tFixture.sCertPath;
	tServerCfg.sKeyFile = tFixture.sKeyPath;
	tServerCfg.bVerifyPeer = false;
	tClientCfg.sHostName = "localhost";
	tClientCfg.bVerifyPeer = false;

	pClient = xrtNetTlsSessionCreate(&tClientCfg, false);
	pServer = xrtNetTlsSessionCreate(&tServerCfg, true);
	if ( !pClient || !pServer ) {
		printf("create_session = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procDriveHandshake(pClient, pServer) ) {
		printf("handshake = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procTransferPlain(pClient, pServer, "hello tls", sServerRecv, sizeof(sServerRecv), &iServerRecvLen) ) {
		printf("client_to_server = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procTransferPlain(pServer, pClient, "pong tls", sClientRecv, sizeof(sClientRecv), &iClientRecvLen) ) {
		printf("server_to_client = failed\n");
		iRet = 1;
		goto cleanup;
	}

	bClosed = procCloseNotify(pClient, pServer);

	printf("client_ready = %s\n", exBoolText(xrtNetTlsSessionIsReady(pClient)));
	printf("server_ready = %s\n", exBoolText(xrtNetTlsSessionIsReady(pServer)));
	printf("server_sni = %s\n", xrtNetTlsSessionGetSNI(pServer));
	printf("server_recv = %s\n", sServerRecv);
	printf("client_recv = %s\n", sClientRecv);
	printf("server_recv_len = %u\n", (unsigned)iServerRecvLen);
	printf("client_recv_len = %u\n", (unsigned)iClientRecvLen);
	printf("close_notify = %s\n", exBoolText(bClosed));

cleanup:
	xrtNetTlsSessionDestroy(pClient);
	xrtNetTlsSessionDestroy(pServer);
	exTlsFixtureUnit(&tFixture);
	xrtUnit();
	return iRet;
}
