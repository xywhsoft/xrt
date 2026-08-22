/*
 * XRT Example - TLS Session Resume
 * XRT 范例 - TLS Session 恢复
 *
 * Description / 说明:
 *   EN: Demonstrates exporting TLS resume state from a first in-memory handshake and reusing it for a resumed TLS 1.2 session.
 *   CN: 演示从首次内存握手中导出 TLS resume 状态，并在后续 TLS 1.2 会话中复用完成恢复握手。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_tls_tls_session_resume.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_tls_tls_session_resume -lm -lpthread
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

		if ( procMoveCipher(pClient, pServer, 31u) > 0u ) bProgress = true;
		if ( procMoveCipher(pServer, pClient, 47u) > 0u ) bProgress = true;

		if ( xrtNetTlsSessionIsReady(pClient) && xrtNetTlsSessionIsReady(pServer) ) {
			return true;
		}
		if ( !bProgress && xrtNetTlsSessionPendingRecv(pClient) == 0u && xrtNetTlsSessionPendingRecv(pServer) == 0u ) {
			break;
		}
	}

	return false;
}


static bool procTransferPlain(xtlssession* pSrc, xtlssession* pDst, const char* sPayload, char* sRecv, size_t iRecvCap)
{
	size_t iPayloadLen;
	size_t iWritten = 0u;
	size_t iTotal = 0u;
	uint32 iStep;

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
		if ( procMoveCipher(pDst, pSrc, 19u) > 0u ) bProgress = true;

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

	return iTotal == iPayloadLen && memcmp(sRecv, sPayload, iPayloadLen) == 0;
}


int main(void)
{
	#define EX_TLS_VERSION_12 0x0303u
	extlsfixture tFixture;
	xtlsconfig tClientCfg;
	xtlsconfig tServerCfg;
	xtlsconfig tClientResumeCfg;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	xtlssession* pClientResumed = NULL;
	xtlssession* pServerResumed = NULL;
	xtlsresume* pResume = NULL;
	char sWarmRecv[64];
	char sResumeRecv[64];
	int iRet = 0;

	xrtInit();
	memset(&tFixture, 0, sizeof(tFixture));
	memset(&tClientCfg, 0, sizeof(tClientCfg));
	memset(&tServerCfg, 0, sizeof(tServerCfg));
	memset(&tClientResumeCfg, 0, sizeof(tClientResumeCfg));
	memset(sWarmRecv, 0, sizeof(sWarmRecv));
	memset(sResumeRecv, 0, sizeof(sResumeRecv));

	if ( !exTlsFixtureInit(&tFixture, "network_tls_resume") ) {
		printf("fixture = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tServerCfg.sCertFile = tFixture.sCertPath;
	tServerCfg.sKeyFile = tFixture.sKeyPath;
	tServerCfg.bVerifyPeer = false;
	tServerCfg.iMaxVersion = EX_TLS_VERSION_12;
	tClientCfg.sHostName = "localhost";
	tClientCfg.bVerifyPeer = false;
	tClientCfg.iMaxVersion = EX_TLS_VERSION_12;

	pClient = xrtNetTlsSessionCreate(&tClientCfg, false);
	pServer = xrtNetTlsSessionCreate(&tServerCfg, true);
	if ( !pClient || !pServer ) {
		printf("create_first_session = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procDriveHandshake(pClient, pServer) ) {
		printf("first_handshake = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procTransferPlain(pClient, pServer, "warm tls", sWarmRecv, sizeof(sWarmRecv)) ) {
		printf("warm_transfer = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pResume = xrtNetTlsSessionExportResume(pClient);
	if ( !pResume ) {
		printf("export_resume = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tClientResumeCfg = tClientCfg;
	tClientResumeCfg.pResume = pResume;

	pClientResumed = xrtNetTlsSessionCreate(&tClientResumeCfg, false);
	pServerResumed = xrtNetTlsSessionCreate(&tServerCfg, true);
	if ( !pClientResumed || !pServerResumed ) {
		printf("create_resumed_session = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procDriveHandshake(pClientResumed, pServerResumed) ) {
		printf("resume_handshake = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !procTransferPlain(pClientResumed, pServerResumed, "resumed tls", sResumeRecv, sizeof(sResumeRecv)) ) {
		printf("resume_transfer = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("resume_export = %s\n", exBoolText(pResume != NULL));
	printf("first_recv = %s\n", sWarmRecv);
	printf("client_resumed = %s\n", exBoolText(xrtNetTlsSessionWasResumed(pClientResumed)));
	printf("server_resumed = %s\n", exBoolText(xrtNetTlsSessionWasResumed(pServerResumed)));
	printf("resumed_recv = %s\n", sResumeRecv);

cleanup:
	xrtNetTlsResumeDestroy(pResume);
	xrtNetTlsSessionDestroy(pClient);
	xrtNetTlsSessionDestroy(pServer);
	xrtNetTlsSessionDestroy(pClientResumed);
	xrtNetTlsSessionDestroy(pServerResumed);
	exTlsFixtureUnit(&tFixture);
	xrtUnit();
	return iRet;
}
