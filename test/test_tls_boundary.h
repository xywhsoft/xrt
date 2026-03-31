#ifndef XRT_TEST_TLS_BOUNDARY_H
#define XRT_TEST_TLS_BOUNDARY_H

#include "../xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint32 iSeed;
} __test_tls_boundary_rng;


// 内部函数：__Test_TLSBoundaryNext
static uint32 __Test_TLSBoundaryNext(__test_tls_boundary_rng* pRng)
{
	uint32 x = pRng ? pRng->iSeed : 0u;
	if ( x == 0u ) x = 0x9e3779b9u;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	if ( pRng ) pRng->iSeed = x;
	return x;
}


// 内部函数：__Test_TLSBoundaryFileExists
static bool __Test_TLSBoundaryFileExists(const char* sPath)
{
	FILE* pFile;
	if ( !sPath || sPath[0] == '\0' ) return false;
	pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_TLSBoundaryMoveCipher
static size_t __Test_TLSBoundaryMoveCipher(xtlssession* pSrc, xtlssession* pDst, __test_tls_boundary_rng* pRng, size_t iMaxChunk)
{
	char aBuf[257];
	size_t iMoved = 0u;
	size_t iChunkCap = iMaxChunk > sizeof(aBuf) ? sizeof(aBuf) : iMaxChunk;
	if ( !pSrc || !pDst || iChunkCap == 0u ) return 0u;

	while ( xrtNetTlsSessionPendingCipher(pSrc) > 0u ) {
		size_t iWant = (size_t)(__Test_TLSBoundaryNext(pRng) % (uint32)iChunkCap) + 1u;
		size_t iRead = 0u;
		if ( iWant > xrtNetTlsSessionPendingCipher(pSrc) ) iWant = xrtNetTlsSessionPendingCipher(pSrc);
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


// 内部函数：__Test_TLSBoundaryPumpHandshake
static bool __Test_TLSBoundaryPumpHandshake(xtlssession* pClient, xtlssession* pServer, __test_tls_boundary_rng* pRng)
{
	for ( uint32 iStep = 0; iStep < 4096u; ++iStep ) {
		bool bProgress = false;
		xnet_result iResClient;
		xnet_result iResServer;

		iResClient = xrtNetTlsSessionDriveHandshake(pClient);
		iResServer = xrtNetTlsSessionDriveHandshake(pServer);
		if ( iResClient != XRT_NET_OK && iResClient != XRT_NET_AGAIN ) return false;
		if ( iResServer != XRT_NET_OK && iResServer != XRT_NET_AGAIN ) return false;

		if ( __Test_TLSBoundaryMoveCipher(pClient, pServer, pRng, 37u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pServer, pClient, pRng, 53u) > 0u ) bProgress = true;

		if ( xrtNetTlsSessionIsReady(pClient) && xrtNetTlsSessionIsReady(pServer) ) {
			return true;
		}
		if ( !bProgress && xrtNetTlsSessionPendingRecv(pClient) == 0u && xrtNetTlsSessionPendingRecv(pServer) == 0u ) {
			break;
		}
	}
	return false;
}


// 内部函数：__Test_TLSBoundaryTransferPlain
static bool __Test_TLSBoundaryTransferPlain(xtlssession* pSrc, xtlssession* pDst,
	const char* pPayload, size_t iLen, __test_tls_boundary_rng* pRng)
{
	char* pRecv = NULL;
	size_t iWritten = 0u;
	size_t iRecvTotal = 0u;
	bool bOk = false;

	if ( !pSrc || !pDst || !pPayload || iLen == 0u ) return false;
	pRecv = (char*)malloc(iLen);
	if ( !pRecv ) return false;
	memset(pRecv, 0, iLen);

	if ( xrtNetTlsSessionWritePlain(pSrc, pPayload, iLen, &iWritten) != XRT_NET_OK || iWritten != iLen ) {
		goto cleanup;
	}

	for ( uint32 iStep = 0; iStep < 4096u && iRecvTotal < iLen; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( __Test_TLSBoundaryMoveCipher(pSrc, pDst, pRng, 29u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pDst, pSrc, pRng, 17u) > 0u ) bProgress = true;

		do {
			iRes = xrtNetTlsSessionReadPlain(pDst, pRecv + iRecvTotal, iLen - iRecvTotal, &iRead);
			if ( iRes == XRT_NET_OK && iRead > 0u ) {
				iRecvTotal += iRead;
				bProgress = true;
			}
		} while ( iRes == XRT_NET_OK && iRead > 0u && iRecvTotal < iLen );

		if ( iRecvTotal == iLen ) break;
		if ( !bProgress ) break;
	}

	if ( iRecvTotal == iLen && memcmp(pRecv, pPayload, iLen) == 0 ) {
		bOk = true;
	}

cleanup:
	free(pRecv);
	return bOk;
}


// 内部函数：__Test_TLSBoundaryCloseNotify
static bool __Test_TLSBoundaryCloseNotify(xtlssession* pSrc, xtlssession* pDst, __test_tls_boundary_rng* pRng)
{
	char aBuf[64];
	for ( uint32 iStep = 0; iStep < 2048u; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( iStep == 0u ) {
			if ( xrtNetTlsSessionQueueClose(pSrc) != XRT_NET_OK ) return false;
		}
		if ( __Test_TLSBoundaryMoveCipher(pSrc, pDst, pRng, 19u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pDst, pSrc, pRng, 19u) > 0u ) bProgress = true;

		iRes = xrtNetTlsSessionReadPlain(pDst, aBuf, sizeof(aBuf), &iRead);
		if ( iRes == XRT_NET_CLOSED ) return true;
		if ( iRes == XRT_NET_OK && iRead > 0u ) bProgress = true;
		if ( !bProgress ) break;
	}
	return false;
}


// TLS边界STRESS测试
static void Test_TLSBoundaryStress(void)
{
	const char* sCertPath = "dev/xnet2_tls_test_cert.pem";
	const char* sKeyPath = "dev/xnet2_tls_test_key.pem";
	xtlsconfig tServerCfg;
	xtlsconfig tClientCfg;
	bool bAllPass = true;
	uint32 iRounds = 32u;
	xrtInit();

	printf("\n\n------------------------------------\n");
	printf("\n TLS Boundary Stress Test:\n\n");

	if ( !__Test_TLSBoundaryFileExists(sCertPath) || !__Test_TLSBoundaryFileExists(sKeyPath) ) {
		printf("  TLS boundary fixtures : SKIP\n");
		return;
	}

	memset(&tServerCfg, 0, sizeof(tServerCfg));
	memset(&tClientCfg, 0, sizeof(tClientCfg));
	tServerCfg.sCertFile = sCertPath;
	tServerCfg.sKeyFile = sKeyPath;
	tServerCfg.bVerifyPeer = false;
	tClientCfg.sHostName = "localhost";
	tClientCfg.bVerifyPeer = false;

	for ( uint32 i = 0; i < iRounds; ++i ) {
		__test_tls_boundary_rng tRng;
		xtlssession* pClient = NULL;
		xtlssession* pServer = NULL;
		size_t iPayloadLen;
		char* pPayloadA = NULL;
		char* pPayloadB = NULL;

		tRng.iSeed = 0x12345678u ^ (i * 0x9e3779b9u);
		pClient = xrtNetTlsSessionCreate(&tClientCfg, false);
		pServer = xrtNetTlsSessionCreate(&tServerCfg, true);
		if ( !pClient || !pServer ) {
			bAllPass = false;
			xrtNetTlsSessionDestroy(pClient);
			xrtNetTlsSessionDestroy(pServer);
			break;
		}

		if ( !__Test_TLSBoundaryPumpHandshake(pClient, pServer, &tRng) ) {
			bAllPass = false;
			xrtNetTlsSessionDestroy(pClient);
			xrtNetTlsSessionDestroy(pServer);
			break;
		}

		iPayloadLen = (size_t)(__Test_TLSBoundaryNext(&tRng) % 2048u) + 1u;
		pPayloadA = (char*)malloc(iPayloadLen);
		pPayloadB = (char*)malloc(iPayloadLen);
		if ( !pPayloadA || !pPayloadB ) {
			bAllPass = false;
			free(pPayloadA);
			free(pPayloadB);
			xrtNetTlsSessionDestroy(pClient);
			xrtNetTlsSessionDestroy(pServer);
			break;
		}

		for ( size_t j = 0; j < iPayloadLen; ++j ) {
			pPayloadA[j] = (char)('A' + (__Test_TLSBoundaryNext(&tRng) % 26u));
			pPayloadB[j] = (char)('a' + (__Test_TLSBoundaryNext(&tRng) % 26u));
		}

		if ( !__Test_TLSBoundaryTransferPlain(pClient, pServer, pPayloadA, iPayloadLen, &tRng) ||
			!__Test_TLSBoundaryTransferPlain(pServer, pClient, pPayloadB, iPayloadLen, &tRng) ||
			!__Test_TLSBoundaryCloseNotify(pClient, pServer, &tRng) ) {
			bAllPass = false;
			free(pPayloadA);
			free(pPayloadB);
			xrtNetTlsSessionDestroy(pClient);
			xrtNetTlsSessionDestroy(pServer);
			break;
		}

		free(pPayloadA);
		free(pPayloadB);
		xrtNetTlsSessionDestroy(pClient);
		xrtNetTlsSessionDestroy(pServer);
	}

	printf("  TLS boundary handshake/data/close rounds : %s\n", bAllPass ? "PASS" : "FAIL");
}

#endif
