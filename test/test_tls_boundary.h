#ifndef XRT_TEST_TLS_BOUNDARY_H
#define XRT_TEST_TLS_BOUNDARY_H

#include "../xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint32 iSeed;
} __test_tls_boundary_rng;

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

static bool __Test_TLSBoundaryFileExists(const char* sPath)
{
	FILE* pFile;
	if ( !sPath || sPath[0] == '\0' ) return false;
	pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static size_t __Test_TLSBoundaryMoveCipher(xtlsctx* pSrc, xtlsctx* pDst, __test_tls_boundary_rng* pRng, size_t iMaxChunk)
{
	char aBuf[257];
	size_t iMoved = 0u;
	size_t iChunkCap = iMaxChunk > sizeof(aBuf) ? sizeof(aBuf) : iMaxChunk;
	if ( !pSrc || !pDst || iChunkCap == 0u ) return 0u;

	while ( xrtTlsPendingSend(pSrc) > 0u ) {
		size_t iWant = (size_t)(__Test_TLSBoundaryNext(pRng) % (uint32)iChunkCap) + 1u;
		size_t iRead = 0u;
		if ( iWant > xrtTlsPendingSend(pSrc) ) iWant = xrtTlsPendingSend(pSrc);
		if ( xrtTlsPeekSend(pSrc, aBuf, iWant, &iRead) != XRT_NET_OK || iRead == 0u ) {
			break;
		}
		if ( xrtTlsFeed(pDst, aBuf, iRead) != XRT_NET_OK ) {
			return 0u;
		}
		xrtTlsConsumeSend(pSrc, iRead);
		iMoved += iRead;
	}
	return iMoved;
}

static bool __Test_TLSBoundaryPumpHandshake(xtlsctx* pClient, xtlsctx* pServer, __test_tls_boundary_rng* pRng)
{
	for ( uint32 iStep = 0; iStep < 4096u; ++iStep ) {
		bool bProgress = false;
		xnet_result iResClient;
		xnet_result iResServer;

		iResClient = xrtTlsDrive(pClient);
		iResServer = xrtTlsDrive(pServer);
		if ( iResClient != XRT_NET_OK && iResClient != XRT_NET_AGAIN ) return false;
		if ( iResServer != XRT_NET_OK && iResServer != XRT_NET_AGAIN ) return false;

		if ( __Test_TLSBoundaryMoveCipher(pClient, pServer, pRng, 37u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pServer, pClient, pRng, 53u) > 0u ) bProgress = true;

		if ( xrtTlsIsReady(pClient) && xrtTlsIsReady(pServer) ) {
			return true;
		}
		if ( !bProgress && xrtTlsPendingRecv(pClient) == 0u && xrtTlsPendingRecv(pServer) == 0u ) {
			break;
		}
	}
	return false;
}

static bool __Test_TLSBoundaryTransferPlain(xtlsctx* pSrc, xtlsctx* pDst,
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

	if ( xrtTlsWrite(pSrc, pPayload, iLen, &iWritten) != XRT_NET_OK || iWritten != iLen ) {
		goto cleanup;
	}

	for ( uint32 iStep = 0; iStep < 4096u && iRecvTotal < iLen; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( __Test_TLSBoundaryMoveCipher(pSrc, pDst, pRng, 29u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pDst, pSrc, pRng, 17u) > 0u ) bProgress = true;

		do {
			iRes = xrtTlsRead(pDst, pRecv + iRecvTotal, iLen - iRecvTotal, &iRead);
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

static bool __Test_TLSBoundaryCloseNotify(xtlsctx* pSrc, xtlsctx* pDst, __test_tls_boundary_rng* pRng)
{
	char aBuf[64];
	for ( uint32 iStep = 0; iStep < 2048u; ++iStep ) {
		bool bProgress = false;
		size_t iRead = 0u;
		xnet_result iRes;

		if ( iStep == 0u ) {
			if ( xrtTlsClose(pSrc) != XRT_NET_OK ) return false;
		}
		if ( __Test_TLSBoundaryMoveCipher(pSrc, pDst, pRng, 19u) > 0u ) bProgress = true;
		if ( __Test_TLSBoundaryMoveCipher(pDst, pSrc, pRng, 19u) > 0u ) bProgress = true;

		iRes = xrtTlsRead(pDst, aBuf, sizeof(aBuf), &iRead);
		if ( iRes == XRT_NET_CLOSED ) return true;
		if ( iRes == XRT_NET_OK && iRead > 0u ) bProgress = true;
		if ( !bProgress ) break;
	}
	return false;
}

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
		xtlsctx* pClient = NULL;
		xtlsctx* pServer = NULL;
		size_t iPayloadLen;
		char* pPayloadA = NULL;
		char* pPayloadB = NULL;

		tRng.iSeed = 0x12345678u ^ (i * 0x9e3779b9u);
		pClient = xrtTlsCreate(&tClientCfg, false);
		pServer = xrtTlsCreate(&tServerCfg, true);
		if ( !pClient || !pServer ) {
			bAllPass = false;
			xrtTlsDestroy(pClient);
			xrtTlsDestroy(pServer);
			break;
		}

		if ( !__Test_TLSBoundaryPumpHandshake(pClient, pServer, &tRng) ) {
			bAllPass = false;
			xrtTlsDestroy(pClient);
			xrtTlsDestroy(pServer);
			break;
		}

		iPayloadLen = (size_t)(__Test_TLSBoundaryNext(&tRng) % 2048u) + 1u;
		pPayloadA = (char*)malloc(iPayloadLen);
		pPayloadB = (char*)malloc(iPayloadLen);
		if ( !pPayloadA || !pPayloadB ) {
			bAllPass = false;
			free(pPayloadA);
			free(pPayloadB);
			xrtTlsDestroy(pClient);
			xrtTlsDestroy(pServer);
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
			xrtTlsDestroy(pClient);
			xrtTlsDestroy(pServer);
			break;
		}

		free(pPayloadA);
		free(pPayloadB);
		xrtTlsDestroy(pClient);
		xrtTlsDestroy(pServer);
	}

	printf("  TLS boundary handshake/data/close rounds : %s\n", bAllPass ? "PASS" : "FAIL");
}

#endif
