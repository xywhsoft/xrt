#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSSH_TEST_CLOSESOCK closesocket
#else
#define XSSH_TEST_CLOSESOCK close
#endif

enum {
	XSSH_RUNTIME_FAULT_NONE = 0,
	XSSH_RUNTIME_FAULT_BAD_GCM_TAG = 1,
	XSSH_RUNTIME_FAULT_BAD_PACKET_LENGTH = 2,
	XSSH_RUNTIME_FAULT_HALF_PACKET = 3,
	XSSH_RUNTIME_FAULT_OUT_OF_ORDER_CHANNEL = 4
};

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bSawClientBanner;
	bool bKexDone;
	bool bSawServiceRequest;
	bool bSawPasswordAuth;
	bool bSawPublicKeyAuth;
	bool bSawKeyboardInteractiveAuth;
	bool bAuthDone;
	bool bUseAES256GCM;
	bool bEnableFirstKexPacketFollows;
	bool bSentFirstKexFollower;
	bool bEnableAuthPacketFlood;
	bool bEnableDisconnectAfterAuth;
	bool bEnableServerGlobalRequest;
	bool bEnableKeepalive;
	bool bEnableExecChannel;
	bool bEnableMultiChannel;
	bool bEnableShellEcho;
	bool bEnableSubsystemEcho;
	bool bEnableEnvRequest;
	bool bRejectEnvRequest;
	bool bChannelOpen;
	bool bExecRequest;
	bool bPtyRequest;
	bool bShellRequest;
	bool bSubsystemRequest;
	bool bEnvRequest;
	bool bChannelClosed;
	bool bEnableForwarding;
	bool bEnableDirectTcpipData;
	bool bEnableDirectTcpipReject;
	bool bEnableRemoteForwardData;
	bool bSawTcpipForward;
	bool bSawDirectTcpip;
	bool bSawForwardedTcpip;
	bool bSawDirectTcpipData;
	bool bSawCancelForward;
	bool bSawIgnore;
	bool bSawDebug;
	bool bSawServerGlobalFailure;
	bool bSawKeepalive;
	bool bSawMultiChannel;
	bool bFaultInjected;
	int iFaultMode;
	char sClientBanner[256];
	char sAuthUser[64];
	char sExecCommand[128];
	char sSecondExecCommand[128];
	char sSubsystemName[64];
	char sSubsystemInput[128];
	char sEnvName[64];
	char sEnvValue[128];
	char sShellInput[128];
	char sForwardTargetHost[128];
	char sForwardData[128];
} xssh_runtime_banner_server;

typedef struct {
	uint8 arrSeed[32];
} xssh_runtime_signer;

typedef struct {
	int iCalls;
} xssh_runtime_kbdint;

static int xssh_runtime_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xssh_runtime_ed25519_sign(ptr pUser, const uint8* pData, size_t iLen, uint8* pSig, size_t* pSigLen)
{
	xssh_runtime_signer* pSigner = (xssh_runtime_signer*)pUser;

	if ( pSigner == NULL || pData == NULL || pSig == NULL || pSigLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( *pSigLen < 64u ) {
		return XSSH_ERR_NO_SPACE;
	}
	if ( !xrtEd25519Sign(pSig, pData, iLen, pSigner->arrSeed) ) {
		return XSSH_ERR_IO;
	}
	*pSigLen = 64u;
	return XSSH_OK;
}

static int xssh_runtime_keyboard_interactive(ptr pUser, const char* sName, const char* sInstruction,
	const char* sPrompt, bool bEcho, char* sOut, size_t iOutCap)
{
	xssh_runtime_kbdint* pKbd = (xssh_runtime_kbdint*)pUser;
	const char* sResponse = NULL;

	if ( pKbd == NULL || sName == NULL || sInstruction == NULL || sPrompt == NULL || sOut == NULL || iOutCap == 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( strcmp(sName, "xssh login") != 0 || strcmp(sInstruction, "two factors") != 0 || bEcho ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( strcmp(sPrompt, "Password: ") == 0 ) {
		sResponse = "secret";
	} else if ( strcmp(sPrompt, "OTP: ") == 0 ) {
		sResponse = "123456";
	} else {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( strlen(sResponse) + 1u > iOutCap ) {
		return XSSH_ERR_NO_SPACE;
	}
	strcpy(sOut, sResponse);
	++pKbd->iCalls;
	return XSSH_OK;
}

static int xssh_runtime_make_openssh_ed25519_private_key(char* sOut, size_t iOutCap, const uint8 arrSeed[32])
{
	static const uint8 arrMagic[] = {
		'o','p','e','n','s','s','h','-','k','e','y','-','v','1','\0'
	};
	uint8 arrPub[32];
	uint8 arrKeyBlob[128];
	uint8 arrPrivateRaw[64];
	uint8 arrPrivateList[512];
	uint8 arrBin[1024];
	xsshwriter keyWr;
	xsshwriter privWr;
	xsshwriter wr;
	str sB64;
	size_t iPad;
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || arrSeed == NULL ) {
		return XSSH_ERR_INVALID;
	}
	xrtEd25519PublicKey(arrPub, arrSeed);
	xsshWriterInit(&keyWr, arrKeyBlob, sizeof(arrKeyBlob));
	if ( xsshPublicKeyWriteEd25519(&keyWr, arrPub) != XSSH_OK ) {
		return XSSH_ERR_NO_SPACE;
	}
	memcpy(arrPrivateRaw, arrSeed, 32u);
	memcpy(arrPrivateRaw + 32u, arrPub, 32u);
	xsshWriterInit(&privWr, arrPrivateList, sizeof(arrPrivateList));
	if ( xsshWireWriteU32(&privWr, 0x23456789u) != XSSH_OK ||
		xsshWireWriteU32(&privWr, 0x23456789u) != XSSH_OK ||
		xsshWireWriteString(&privWr, "ssh-ed25519", strlen("ssh-ed25519")) != XSSH_OK ||
		xsshWireWriteString(&privWr, arrPub, 32u) != XSSH_OK ||
		xsshWireWriteString(&privWr, arrPrivateRaw, sizeof(arrPrivateRaw)) != XSSH_OK ||
		xsshWireWriteString(&privWr, "xssh-runtime", strlen("xssh-runtime")) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		return XSSH_ERR_NO_SPACE;
	}
	for ( iPad = 1u; (privWr.iLen % 8u) != 0u; ++iPad ) {
		if ( xsshWireWriteU8(&privWr, (uint8)iPad) != XSSH_OK ) {
			xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
			return XSSH_ERR_NO_SPACE;
		}
	}
	xsshWriterInit(&wr, arrBin, sizeof(arrBin));
	if ( xsshWireReserve(&wr, sizeof(arrMagic)) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		return XSSH_ERR_NO_SPACE;
	}
	memcpy(wr.pData + wr.iLen, arrMagic, sizeof(arrMagic));
	wr.iLen += sizeof(arrMagic);
	if ( xsshWireWriteString(&wr, "none", strlen("none")) != XSSH_OK ||
		xsshWireWriteString(&wr, "none", strlen("none")) != XSSH_OK ||
		xsshWireWriteString(&wr, "", 0u) != XSSH_OK ||
		xsshWireWriteU32(&wr, 1u) != XSSH_OK ||
		xsshWireWriteString(&wr, arrKeyBlob, keyWr.iLen) != XSSH_OK ||
		xsshWireWriteString(&wr, arrPrivateList, privWr.iLen) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
		return XSSH_ERR_NO_SPACE;
	}
	sB64 = xrtBase64Encode(arrBin, wr.iLen, NULL);
	if ( sB64 == NULL ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
		return XSSH_ERR_NO_SPACE;
	}
	iWritten = snprintf(sOut, iOutCap,
		"-----BEGIN OPENSSH PRIVATE KEY-----\n%s\n-----END OPENSSH PRIVATE KEY-----\n",
		(const char*)sB64);
	xrtFree(sB64);
	xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
	xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
	xsshSecureZero(arrBin, sizeof(arrBin));
	return (iWritten > 0 && (size_t)iWritten < iOutCap) ? XSSH_OK : XSSH_ERR_NO_SPACE;
}

static bool xssh_runtime_server_send_all(xsocket hSock, const char* sText)
{
	const char* p = sText;
	size_t iLeft = strlen(sText);

	while ( iLeft > 0u ) {
		int iSent = send(hSock, p, (int)iLeft, 0);
		if ( iSent <= 0 ) {
			return FALSE;
		}
		p += iSent;
		iLeft -= (size_t)iSent;
	}
	return TRUE;
}

static bool xssh_runtime_server_send_bytes(xsocket hSock, const void* pData, size_t iLen)
{
	const char* p = (const char*)pData;
	size_t iLeft = iLen;

	while ( iLeft > 0u ) {
		int iSent = send(hSock, p, (int)iLeft, 0);
		if ( iSent <= 0 ) {
			return FALSE;
		}
		p += iSent;
		iLeft -= (size_t)iSent;
	}
	return TRUE;
}

static bool xssh_runtime_server_recv_line(xsocket hSock, char* sOut, size_t iOutCap)
{
	size_t iLen = 0u;

	if ( sOut == NULL || iOutCap == 0u ) {
		return FALSE;
	}
	for (;;) {
		char ch;
		int iRet = recv(hSock, &ch, 1, 0);
		if ( iRet <= 0 ) {
			return FALSE;
		}
		if ( ch == '\n' ) {
			if ( iLen > 0u && sOut[iLen - 1u] == '\r' ) {
				iLen--;
			}
			sOut[iLen] = '\0';
			return TRUE;
		}
		if ( iLen + 1u < iOutCap ) {
			sOut[iLen++] = ch;
		}
	}
}

static bool xssh_runtime_server_recv_all(xsocket hSock, void* pOut, size_t iLen)
{
	uint8* p = (uint8*)pOut;
	size_t iLeft = iLen;

	while ( iLeft > 0u ) {
		int iRead = recv(hSock, (char*)p, (int)iLeft, 0);
		if ( iRead <= 0 ) {
			return FALSE;
		}
		p += iRead;
		iLeft -= (size_t)iRead;
	}
	return TRUE;
}

static bool xssh_runtime_server_send_packet(xsocket hSock, xsshpacketcodec* pCodec, const void* pPayload, size_t iPayloadLen)
{
	uint8 arrWire[4096];
	xsshwriter wr;

	xsshWriterInit(&wr, arrWire, sizeof(arrWire));
	if ( xsshPacketCodecWrite(pCodec, &wr, pPayload, iPayloadLen) != XSSH_OK ) {
		return FALSE;
	}
	return xssh_runtime_server_send_bytes(hSock, arrWire, wr.iLen);
}

static bool xssh_runtime_server_send_fault_packet(xsocket hSock, xsshpacketcodec* pCodec, int iFaultMode)
{
	uint8 arrWire[4096];
	uint8 arrPayload[64];
	xsshwriter wrPayload;
	xsshwriter wrWire;
	size_t iPartialLen;

	if ( iFaultMode == XSSH_RUNTIME_FAULT_BAD_PACKET_LENGTH ) {
		/*
		 * AES-GCM 模式下 packet_length 是明文 AAD。只发一个非法长度字段即可让客户端
		 * 在等待密文前拒绝坏包，验证长度上限和块对齐保护不会依赖后续网络数据。
		 */
		arrWire[0] = 0xffu;
		arrWire[1] = 0xffu;
		arrWire[2] = 0xffu;
		arrWire[3] = 0xffu;
		return xssh_runtime_server_send_bytes(hSock, arrWire, 4u);
	}

	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( iFaultMode == XSSH_RUNTIME_FAULT_OUT_OF_ORDER_CHANNEL ) {
		/*
		 * 这里发送语法合法但状态非法的 CHANNEL_DATA：客户端尚未打开任何 channel，
		 * 因此 recipient 不应被找到。这个用例覆盖“解密成功但连接层状态机拒绝”的路径。
		 */
		if ( xsshChannelDataWrite(&wrPayload, 999u, "early", 5u) != XSSH_OK ) {
			return FALSE;
		}
	} else if ( xsshMsgWriteIgnore(&wrPayload, "fault", 5u) != XSSH_OK ) {
		return FALSE;
	}
	xsshWriterInit(&wrWire, arrWire, sizeof(arrWire));
	if ( xsshPacketCodecWrite(pCodec, &wrWire, arrPayload, wrPayload.iLen) != XSSH_OK || wrWire.iLen < 12u ) {
		return FALSE;
	}

	if ( iFaultMode == XSSH_RUNTIME_FAULT_BAD_GCM_TAG ) {
		/*
		 * 只翻转认证 tag 的最后一字节。packet_length、密文长度和 seqno 都保持合法，
		 * 客户端必须在 AEAD 认证失败时拒绝整个 packet，不能交给上层 channel 分发。
		 */
		arrWire[wrWire.iLen - 1u] ^= 0x5au;
		return xssh_runtime_server_send_bytes(hSock, arrWire, wrWire.iLen);
	}
	if ( iFaultMode == XSSH_RUNTIME_FAULT_HALF_PACKET ) {
		/*
		 * 半包用于验证 transport 在流关闭时能从 NEED_MORE 退出，并把错误归到 channel
		 * packet 读取阶段，而不是无限等待或错误地推进解密计数器。
		 */
		iPartialLen = wrWire.iLen / 2u;
		if ( iPartialLen < 5u ) {
			iPartialLen = 5u;
		}
		return xssh_runtime_server_send_bytes(hSock, arrWire, iPartialLen);
	}
	if ( iFaultMode == XSSH_RUNTIME_FAULT_OUT_OF_ORDER_CHANNEL ) {
		return xssh_runtime_server_send_bytes(hSock, arrWire, wrWire.iLen);
	}
	return FALSE;
}

static bool xssh_runtime_server_recv_packet(xsocket hSock, xsshpacketcodec* pCodec, xsshpacket* pPkt, uint8* pPlain, size_t iPlainCap)
{
	uint8 arrWire[4096];
	xsshreader rd;
	uint32 iPacketLen;
	size_t iTotalLen;
	size_t iTagLen;

	if ( !xssh_runtime_server_recv_all(hSock, arrWire, 4u) ) {
		return FALSE;
	}
	iPacketLen = xsshPacketPeekU32(arrWire);
	iTagLen = (pCodec != NULL && pCodec->iReadMode == XSSH_PACKET_MODE_AESGCM) ? XSSH_AEAD_TAG_SIZE : 0u;
	iTotalLen = 4u + (size_t)iPacketLen + iTagLen;
	if ( iPacketLen < 5u || iTotalLen > sizeof(arrWire) ) {
		return FALSE;
	}
	if ( !xssh_runtime_server_recv_all(hSock, arrWire + 4u, (size_t)iPacketLen + iTagLen) ) {
		return FALSE;
	}
	xsshReaderInit(&rd, arrWire, iTotalLen);
	return (xsshPacketCodecRead(pCodec, &rd, 0u, pPkt, pPlain, iPlainCap) == XSSH_OK && rd.iPos == iTotalLen) ? TRUE : FALSE;
}

static uint32 xssh_runtime_banner_server_thread(ptr pArg)
{
	xssh_runtime_banner_server* pServer = (xssh_runtime_banner_server*)pArg;
	xsocket hClient;
	xsshpacketcodec codec;
	xsshpacket pkt;
	uint8 arrPlain[4096];
	uint8 arrClientKex[2048];
	uint8 arrServerKex[512];
	uint8 arrServerCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrServerPriv[32];
	uint8 arrServerPub[32];
	uint8 arrSecret[32];
	uint8 arrHostSeed[32];
	uint8 arrHostPub[32];
	uint8 arrHostKeyBlob[128];
	uint8 arrHash[XSSH_SHA256_SIZE];
	uint8 arrSig[64];
	uint8 arrSigBlob[128];
	uint8 arrPayload[512];
	uint8 arrC2SIV[XSSH_GCM_NONCE_SIZE];
	uint8 arrS2CIV[XSSH_GCM_NONCE_SIZE];
	uint8 arrC2SKey[XSSH_AES256_KEY_SIZE];
	uint8 arrS2CKey[XSSH_AES256_KEY_SIZE];
	size_t iClientKexLen = 0u;
	size_t iServerKexLen = 0u;
	size_t iC2SKeyLen = XSSH_AES128_KEY_SIZE;
	size_t iS2CKeyLen = XSSH_AES128_KEY_SIZE;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;
	xsshkexinitdesc serverDesc;
	xsshkexecdhinit initMsg;
	xsshkexhashdesc hashDesc;
	xsshslice sharedSecret;
	xsshservicemsg svc;
	xsshauthrequestmsg authReq;
	xsshchannelopenmsg openMsg;
	xsshchanneltcpipopenmsg tcpipOpen;
	xsshchannelrequestmsg channelReq;
	xsshglobalrequestmsg globalReq;
	xsshglobaltcpipforwardmsg tcpipReq;
	xsshwriter wr;
	xsshwriter wrHostKey;
	xsshwriter wrSig;
	size_t i;
	uint32 iRetCode = 0u;

	if ( pServer == NULL ) {
		return 1u;
	}
	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 2u;
	}
#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iTimeout = 5000u;
		setsockopt(hClient, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iTimeout, sizeof(iTimeout));
		setsockopt(hClient, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iTimeout, sizeof(iTimeout));
	}
#else
	{
		struct timeval tTimeout;
		tTimeout.tv_sec = 5;
		tTimeout.tv_usec = 0;
		setsockopt(hClient, SOL_SOCKET, SO_RCVTIMEO, &tTimeout, sizeof(tTimeout));
		setsockopt(hClient, SOL_SOCKET, SO_SNDTIMEO, &tTimeout, sizeof(tTimeout));
	}
#endif
	xsshPacketCodecInit(&codec);
	if ( !xssh_runtime_server_send_all(hClient, "SSH-2.0-xssh-banner-test\r\n") ) {
		iRetCode = 3u;
		goto done;
	}
	if ( xssh_runtime_server_recv_line(hClient, pServer->sClientBanner, sizeof(pServer->sClientBanner)) ) {
		pServer->bSawClientBanner = TRUE;
	}
	if ( !pServer->bSawClientBanner ) {
		iRetCode = 4u;
		goto done;
	}
	if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
		pkt.tPayload.iLen > sizeof(arrClientKex) ||
		xsshKexInitRead(pkt.tPayload.pData, pkt.tPayload.iLen, &clientKex) != XSSH_OK ) {
		iRetCode = 5u;
		goto done;
	}
	memcpy(arrClientKex, pkt.tPayload.pData, pkt.tPayload.iLen);
	iClientKexLen = pkt.tPayload.iLen;

	for ( i = 0u; i < sizeof(arrServerCookie); ++i ) arrServerCookie[i] = (uint8)(0x80u + i);
	xsshKexInitDescInit(&serverDesc);
	serverDesc.pCookie = arrServerCookie;
	serverDesc.sKexAlgorithms = pServer->bEnableFirstKexPacketFollows ?
		"ecdh-sha2-nistp256,curve25519-sha256" : "curve25519-sha256";
	serverDesc.sServerHostKeyAlgorithms = "ssh-ed25519";
	serverDesc.sEncryptionClientToServer = pServer->bUseAES256GCM ? "aes256-gcm@openssh.com" : "aes128-gcm@openssh.com";
	serverDesc.sEncryptionServerToClient = pServer->bUseAES256GCM ? "aes256-gcm@openssh.com" : "aes128-gcm@openssh.com";
	serverDesc.sMacClientToServer = "";
	serverDesc.sMacServerToClient = "";
	serverDesc.sCompressionClientToServer = "none";
	serverDesc.sCompressionServerToClient = "none";
	serverDesc.bFirstKexPacketFollows = pServer->bEnableFirstKexPacketFollows;
	xsshWriterInit(&wr, arrServerKex, sizeof(arrServerKex));
	if ( xsshKexInitWrite(&wr, &serverDesc) != XSSH_OK ||
		xsshKexInitRead(arrServerKex, wr.iLen, &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ||
		!xssh_runtime_server_send_packet(hClient, &codec, arrServerKex, wr.iLen) ) {
		iRetCode = 6u;
		goto done;
	}
	iServerKexLen = wr.iLen;
	if ( pServer->bEnableFirstKexPacketFollows ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshWireWriteU8(&wr, XSSH_MSG_KEX_ECDH_REPLY) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 90u;
			goto done;
		}
		pServer->bSentFirstKexFollower = TRUE;
	}

	if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
		xsshKexECDHInitRead(pkt.tPayload.pData, pkt.tPayload.iLen, &initMsg) != XSSH_OK ||
		initMsg.tClientPublic.iLen != 32u ) {
		iRetCode = 7u;
		goto done;
	}
	if ( xsshKexCurve25519Keypair(arrServerPriv, arrServerPub) != XSSH_OK ||
		xsshKexCurve25519SharedSecret(arrSecret, arrServerPriv, initMsg.tClientPublic.pData) != XSSH_OK ) {
		iRetCode = 8u;
		goto done;
	}
	for ( i = 0u; i < sizeof(arrHostSeed); ++i ) arrHostSeed[i] = (uint8)(0x40u + i);
	xrtEd25519PublicKey(arrHostPub, arrHostSeed);
	xsshWriterInit(&wrHostKey, arrHostKeyBlob, sizeof(arrHostKeyBlob));
	if ( xsshPublicKeyWriteEd25519(&wrHostKey, arrHostPub) != XSSH_OK ) {
		iRetCode = 9u;
		goto done;
	}
	memset(&hashDesc, 0, sizeof(hashDesc));
	hashDesc.tClientVersion.pData = (const uint8*)pServer->sClientBanner;
	hashDesc.tClientVersion.iLen = strlen(pServer->sClientBanner);
	hashDesc.tServerVersion.pData = (const uint8*)"SSH-2.0-xssh-banner-test";
	hashDesc.tServerVersion.iLen = strlen("SSH-2.0-xssh-banner-test");
	hashDesc.tClientKexInit.pData = arrClientKex;
	hashDesc.tClientKexInit.iLen = iClientKexLen;
	hashDesc.tServerKexInit.pData = arrServerKex;
	hashDesc.tServerKexInit.iLen = iServerKexLen;
	hashDesc.tServerHostKey.pData = arrHostKeyBlob;
	hashDesc.tServerHostKey.iLen = wrHostKey.iLen;
	hashDesc.tClientEphemeral = initMsg.tClientPublic;
	hashDesc.tServerEphemeral.pData = arrServerPub;
	hashDesc.tServerEphemeral.iLen = sizeof(arrServerPub);
	hashDesc.tSharedSecret.pData = arrSecret;
	hashDesc.tSharedSecret.iLen = sizeof(arrSecret);
	if ( xsshKexHashSHA256Curve25519(&hashDesc, arrHash) != XSSH_OK ||
		!xrtEd25519Sign(arrSig, arrHash, sizeof(arrHash), arrHostSeed) ) {
		iRetCode = 10u;
		goto done;
	}
	xsshWriterInit(&wrSig, arrSigBlob, sizeof(arrSigBlob));
	if ( xsshSignatureWrite(&wrSig, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ) {
		iRetCode = 11u;
		goto done;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	{
		xsshslice tHostKey;
		xsshslice tServerPub;
		xsshslice tSig;
		tHostKey.pData = arrHostKeyBlob;
		tHostKey.iLen = wrHostKey.iLen;
		tServerPub.pData = arrServerPub;
		tServerPub.iLen = sizeof(arrServerPub);
		tSig.pData = arrSigBlob;
		tSig.iLen = wrSig.iLen;
		if ( xsshKexECDHReplyWrite(&wr, tHostKey, tServerPub, tSig) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 12u;
			goto done;
		}
	}
	if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
		xsshMsgReadNewKeys(pkt.tPayload.pData, pkt.tPayload.iLen) != XSSH_OK ) {
		iRetCode = 13u;
		goto done;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	if ( xsshMsgWriteNewKeys(&wr) != XSSH_OK ||
		!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
		iRetCode = 14u;
		goto done;
	}
	pServer->bKexDone = TRUE;

	sharedSecret.pData = arrSecret;
	sharedSecret.iLen = sizeof(arrSecret);
	iC2SKeyLen = xsshKexSliceEqText(neg.tCipherClientToServer, "aes256-gcm@openssh.com") ? XSSH_AES256_KEY_SIZE : XSSH_AES128_KEY_SIZE;
	iS2CKeyLen = xsshKexSliceEqText(neg.tCipherServerToClient, "aes256-gcm@openssh.com") ? XSSH_AES256_KEY_SIZE : XSSH_AES128_KEY_SIZE;
	if ( xsshKexDeriveKeySHA256(arrC2SIV, sizeof(arrC2SIV), sharedSecret, arrHash, arrHash, 'A') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CIV, sizeof(arrS2CIV), sharedSecret, arrHash, arrHash, 'B') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrC2SKey, iC2SKeyLen, sharedSecret, arrHash, arrHash, 'C') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CKey, iS2CKeyLen, sharedSecret, arrHash, arrHash, 'D') != XSSH_OK ||
		xsshPacketCodecSetReadAESGCM(&codec, arrC2SKey, iC2SKeyLen, arrC2SIV) != XSSH_OK ||
		xsshPacketCodecSetWriteAESGCM(&codec, arrS2CKey, iS2CKeyLen, arrS2CIV) != XSSH_OK ) {
		iRetCode = 15u;
		goto done;
	}

	if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
		xsshMsgReadServiceRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &svc) != XSSH_OK ||
		!xsshSliceEqualText(svc.tServiceName, "ssh-userauth") ) {
		iRetCode = 16u;
		goto done;
	}
	pServer->bSawServiceRequest = TRUE;
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	if ( xsshMsgWriteServiceAccept(&wr, "ssh-userauth") != XSSH_OK ||
		!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
		iRetCode = 17u;
		goto done;
	}
	if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
		xsshAuthReadRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &authReq) != XSSH_OK ) {
		iRetCode = 18u;
		goto done;
	}
	if ( authReq.tUserName.iLen < sizeof(pServer->sAuthUser) ) {
		memcpy(pServer->sAuthUser, authReq.tUserName.pData, authReq.tUserName.iLen);
		pServer->sAuthUser[authReq.tUserName.iLen] = '\0';
	}
	if ( pServer->bEnableAuthPacketFlood ) {
		uint32 i;

		for ( i = 0u; i < XSSH_AUTH_PACKET_MAX + 2u; ++i ) {
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshAuthWriteBanner(&wr, "auth flood", "en") != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 18u;
				goto done;
			}
		}
		goto done;
	}
	if ( xsshSliceEqualText(authReq.tUserName, "alice") &&
		xsshSliceEqualText(authReq.tServiceName, "ssh-connection") &&
		xsshSliceEqualText(authReq.tMethodName, "password") ) {
		pServer->bSawPasswordAuth = TRUE;
	}
	if ( pServer->bSawPasswordAuth &&
		xsshSliceEqualText(authReq.tPassword, "secret") ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWriteSuccess(&wr) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 19u;
			goto done;
		}
		pServer->bAuthDone = TRUE;
	}
	if ( !pServer->bAuthDone &&
		xsshSliceEqualText(authReq.tUserName, "alice") &&
		xsshSliceEqualText(authReq.tServiceName, "ssh-connection") &&
		xsshSliceEqualText(authReq.tMethodName, "publickey") &&
		!authReq.bPublicKeyHasSignature &&
		xsshSliceEqualText(authReq.tPublicKeyAlgorithm, "ssh-ed25519") ) {
		xsshauthrequestmsg signedReq;
		xsshpublickey pubKey;
		xsshsignature sig;
		xsshwriter signWr;
		xsshslice sessionId;
		uint8 arrSignData[1024];

		pServer->bSawPublicKeyAuth = TRUE;
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWritePublicKeyOk(&wr, "ssh-ed25519", authReq.tPublicKeyBlob) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 21u;
			goto done;
		}
		if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
			xsshAuthReadRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &signedReq) != XSSH_OK ||
			!xsshSliceEqualText(signedReq.tUserName, "alice") ||
			!xsshSliceEqualText(signedReq.tServiceName, "ssh-connection") ||
			!xsshSliceEqualText(signedReq.tMethodName, "publickey") ||
			!signedReq.bPublicKeyHasSignature ||
			!xsshSliceEqualText(signedReq.tPublicKeyAlgorithm, "ssh-ed25519") ||
			signedReq.tPublicKeyBlob.iLen != authReq.tPublicKeyBlob.iLen ||
			memcmp(signedReq.tPublicKeyBlob.pData, authReq.tPublicKeyBlob.pData, authReq.tPublicKeyBlob.iLen) != 0 ) {
			iRetCode = 22u;
			goto done;
		}
		sessionId.pData = arrHash;
		sessionId.iLen = XSSH_SHA256_SIZE;
		xsshWriterInit(&signWr, arrSignData, sizeof(arrSignData));
		if ( xsshAuthWritePublicKeySignData(&signWr, sessionId, "alice", "ssh-ed25519", signedReq.tPublicKeyBlob) != XSSH_OK ||
			xsshPublicKeyReadEd25519(signedReq.tPublicKeyBlob.pData, signedReq.tPublicKeyBlob.iLen, &pubKey) != XSSH_OK ||
			xsshSignatureRead(signedReq.tSignature.pData, signedReq.tSignature.iLen, &sig) != XSSH_OK ||
			xsshHostKeyVerifyEd25519(&pubKey, &sig, arrSignData, signWr.iLen) != XSSH_OK ) {
			iRetCode = 23u;
			goto done;
		}
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWriteSuccess(&wr) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 24u;
			goto done;
		}
		pServer->bAuthDone = TRUE;
	}
	if ( !pServer->bAuthDone &&
		xsshSliceEqualText(authReq.tUserName, "alice") &&
		xsshSliceEqualText(authReq.tServiceName, "ssh-connection") &&
		xsshSliceEqualText(authReq.tMethodName, "keyboard-interactive") ) {
		xsshauthkbdintresponsemsg kbdResp;
		const char* psPrompts[] = { "Password: ", "OTP: " };
		const bool arrEcho[] = { FALSE, FALSE };

		pServer->bSawKeyboardInteractiveAuth = TRUE;
		if ( !xsshSliceEqualText(authReq.tKbdIntLanguageTag, "") ||
			!xsshSliceEqualText(authReq.tKbdIntSubmethods, "") ) {
			iRetCode = 25u;
			goto done;
		}
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWriteKeyboardInteractiveInfoRequest(&wr, "xssh login", "two factors", psPrompts, arrEcho, 2u) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 26u;
			goto done;
		}
		if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
			xsshAuthReadKeyboardInteractiveInfoResponse(pkt.tPayload.pData, pkt.tPayload.iLen, &kbdResp) != XSSH_OK ||
			kbdResp.iResponseCount != 2u ||
			!xsshSliceEqualText(kbdResp.arrResponses[0], "secret") ||
			!xsshSliceEqualText(kbdResp.arrResponses[1], "123456") ) {
			iRetCode = 27u;
			goto done;
		}
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWriteSuccess(&wr) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 28u;
			goto done;
		}
		pServer->bAuthDone = TRUE;
	}
	if ( pServer->bAuthDone ) {
		if ( pServer->iFaultMode != XSSH_RUNTIME_FAULT_NONE ) {
			if ( !xssh_runtime_server_send_fault_packet(hClient, &codec, pServer->iFaultMode) ) {
				iRetCode = 89u;
				goto done;
			}
			pServer->bFaultInjected = TRUE;
		} else if ( pServer->bEnableDisconnectAfterAuth ) {
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshMsgWriteDisconnect(&wr, 11u, "server shutdown", "en") != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 55u;
				goto done;
			}
		} else if ( pServer->bEnableServerGlobalRequest ) {
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshMsgWriteKeepalive(&wr, TRUE) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 87u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadRequestFailure(pkt.tPayload.pData, pkt.tPayload.iLen) != XSSH_OK ) {
				iRetCode = 88u;
				goto done;
			}
			pServer->bSawServerGlobalFailure = TRUE;
		} else if ( pServer->bEnableKeepalive ) {
			xsshignoremsg ignoreMsg;
			xsshdebugmsg debugMsg;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadIgnore(pkt.tPayload.pData, pkt.tPayload.iLen, &ignoreMsg) != XSSH_OK ||
				!xsshSliceEqualText(ignoreMsg.tData, "pad") ) {
				iRetCode = 66u;
				goto done;
			}
			pServer->bSawIgnore = TRUE;
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadDebug(pkt.tPayload.pData, pkt.tPayload.iLen, &debugMsg) != XSSH_OK ||
				debugMsg.bAlwaysDisplay ||
				!xsshSliceEqualText(debugMsg.tMessage, "debug note") ||
				!xsshSliceEqualText(debugMsg.tLanguageTag, "en") ) {
				iRetCode = 67u;
				goto done;
			}
			pServer->bSawDebug = TRUE;
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadGlobalRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &globalReq) != XSSH_OK ||
				!xsshSliceEqualText(globalReq.tRequestName, "keepalive@openssh.com") ||
				!globalReq.bWantReply ||
				globalReq.tPayload.iLen != 0u ) {
				iRetCode = 68u;
				goto done;
			}
			pServer->bSawKeepalive = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshMsgWriteRequestFailure(&wr) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 69u;
				goto done;
			}
		} else if ( pServer->bEnableEnvRequest ) {
			/* env 是会话通道请求；这里同时覆盖服务端接受和拒绝两种真实回包路径。 */
			uint32 iServerChannel = 708u;
			uint32 iCloseRecipient = 0u;
			xsshreader rdReq;
			xsshslice envName;
			xsshslice envValue;
			bool bExpectRejectName;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "session") ) {
				iRetCode = 79u;
				goto done;
			}
			pServer->bChannelOpen = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 80u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
				channelReq.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(channelReq.tRequestType, "env") ||
				!channelReq.bWantReply ) {
				iRetCode = 81u;
				goto done;
			}
			xsshReaderInit(&rdReq, channelReq.tPayload.pData, channelReq.tPayload.iLen);
			if ( xsshWireReadString(&rdReq, &envName) != XSSH_OK ||
				xsshWireReadString(&rdReq, &envValue) != XSSH_OK ||
				xsshMsgReadNoTrailing(&rdReq) != XSSH_OK ||
				envName.iLen >= sizeof(pServer->sEnvName) ||
				envValue.iLen >= sizeof(pServer->sEnvValue) ) {
				iRetCode = 82u;
				goto done;
			}
			memcpy(pServer->sEnvName, envName.pData, envName.iLen);
			pServer->sEnvName[envName.iLen] = '\0';
			memcpy(pServer->sEnvValue, envValue.pData, envValue.iLen);
			pServer->sEnvValue[envValue.iLen] = '\0';
			bExpectRejectName = pServer->bRejectEnvRequest;
			if ( (!bExpectRejectName && (!xsshSliceEqualText(envName, "LANG") || !xsshSliceEqualText(envValue, "zh_CN.UTF-8"))) ||
				(bExpectRejectName && (!xsshSliceEqualText(envName, "LC_SECRET") || !xsshSliceEqualText(envValue, "nope"))) ) {
				iRetCode = 83u;
				goto done;
			}
			pServer->bEnvRequest = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( (pServer->bRejectEnvRequest ?
					xsshChannelFailureWrite(&wr, openMsg.iSenderChannel) :
					xsshChannelSuccessWrite(&wr, openMsg.iSenderChannel)) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 84u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
				iCloseRecipient != iServerChannel ) {
				iRetCode = 85u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelCloseWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 86u;
				goto done;
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableShellEcho ) {
			uint32 iServerChannel = 703u;
			uint32 iCloseRecipient = 0u;
			xsshchanneldataeventmsg dataMsg;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "session") ) {
				iRetCode = 46u;
				goto done;
			}
			pServer->bChannelOpen = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 47u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
				channelReq.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(channelReq.tRequestType, "pty-req") ||
				!channelReq.bWantReply ) {
				iRetCode = 48u;
				goto done;
			}
			pServer->bPtyRequest = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelSuccessWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 49u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
				channelReq.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(channelReq.tRequestType, "shell") ||
				!channelReq.bWantReply ) {
				iRetCode = 50u;
				goto done;
			}
			pServer->bShellRequest = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelSuccessWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 51u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelDataRead(pkt.tPayload.pData, pkt.tPayload.iLen, &dataMsg) != XSSH_OK ||
				dataMsg.iRecipientChannel != iServerChannel ||
				dataMsg.tData.iLen >= sizeof(pServer->sShellInput) ) {
				iRetCode = 52u;
				goto done;
			}
			memcpy(pServer->sShellInput, dataMsg.tData.pData, dataMsg.tData.iLen);
			pServer->sShellInput[dataMsg.tData.iLen] = '\0';
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, openMsg.iSenderChannel, "shell:hello\n", 12u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 53u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
				iCloseRecipient != iServerChannel ) {
				iRetCode = 54u;
				goto done;
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableSubsystemEcho ) {
			/* subsystem 只承载字节流；具体协议（如 SFTP）应留给上层库实现。 */
			uint32 iServerChannel = 705u;
			uint32 iCloseRecipient = 0u;
			xsshchanneldataeventmsg dataMsg;
			xsshreader rdReq;
			xsshslice subsystemName;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "session") ) {
				iRetCode = 58u;
				goto done;
			}
			pServer->bChannelOpen = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 59u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
				channelReq.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(channelReq.tRequestType, "subsystem") ||
				!channelReq.bWantReply ) {
				iRetCode = 60u;
				goto done;
			}
			xsshReaderInit(&rdReq, channelReq.tPayload.pData, channelReq.tPayload.iLen);
			if ( xsshWireReadString(&rdReq, &subsystemName) != XSSH_OK ||
				xsshMsgReadNoTrailing(&rdReq) != XSSH_OK ||
				!xsshSliceEqualText(subsystemName, "raw") ) {
				iRetCode = 61u;
				goto done;
			}
			if ( subsystemName.iLen < sizeof(pServer->sSubsystemName) ) {
				memcpy(pServer->sSubsystemName, subsystemName.pData, subsystemName.iLen);
				pServer->sSubsystemName[subsystemName.iLen] = '\0';
			}
			pServer->bSubsystemRequest = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelSuccessWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 62u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelDataRead(pkt.tPayload.pData, pkt.tPayload.iLen, &dataMsg) != XSSH_OK ||
				dataMsg.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(dataMsg.tData, "subsystem-ping") ||
				dataMsg.tData.iLen >= sizeof(pServer->sSubsystemInput) ) {
				iRetCode = 63u;
				goto done;
			}
			memcpy(pServer->sSubsystemInput, dataMsg.tData.pData, dataMsg.tData.iLen);
			pServer->sSubsystemInput[dataMsg.tData.iLen] = '\0';
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, openMsg.iSenderChannel, "subsystem:pong", 14u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 64u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
				iCloseRecipient != iServerChannel ) {
				iRetCode = 65u;
				goto done;
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableMultiChannel ) {
			uint32 arrServerChannel[2] = { 706u, 707u };
			xsshchannelopenmsg arrOpen[2];
			xsshreader rdReq;
			xsshslice cmd;
			size_t iChannel;

			memset(arrOpen, 0, sizeof(arrOpen));
			for ( iChannel = 0u; iChannel < 2u; ++iChannel ) {
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &arrOpen[iChannel]) != XSSH_OK ||
					!xsshSliceEqualText(arrOpen[iChannel].tChannelType, "session") ) {
					iRetCode = 70u;
					goto done;
				}
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelOpenConfirmationWrite(&wr, arrOpen[iChannel].iSenderChannel, arrServerChannel[iChannel],
						XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 71u;
					goto done;
				}
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
					channelReq.iRecipientChannel != arrServerChannel[iChannel] ||
					!xsshSliceEqualText(channelReq.tRequestType, "exec") ||
					!channelReq.bWantReply ) {
					iRetCode = 72u;
					goto done;
				}
				xsshReaderInit(&rdReq, channelReq.tPayload.pData, channelReq.tPayload.iLen);
				if ( xsshWireReadString(&rdReq, &cmd) != XSSH_OK ||
					xsshMsgReadNoTrailing(&rdReq) != XSSH_OK ) {
					iRetCode = 73u;
					goto done;
				}
				if ( iChannel == 0u && cmd.iLen < sizeof(pServer->sExecCommand) ) {
					memcpy(pServer->sExecCommand, cmd.pData, cmd.iLen);
					pServer->sExecCommand[cmd.iLen] = '\0';
				}
				if ( iChannel == 1u && cmd.iLen < sizeof(pServer->sSecondExecCommand) ) {
					memcpy(pServer->sSecondExecCommand, cmd.pData, cmd.iLen);
					pServer->sSecondExecCommand[cmd.iLen] = '\0';
				}
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelSuccessWrite(&wr, arrOpen[iChannel].iSenderChannel) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 74u;
					goto done;
				}
			}
			pServer->bChannelOpen = TRUE;
			pServer->bExecRequest = TRUE;
			pServer->bSawMultiChannel = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, arrOpen[1].iSenderChannel, "two\n", 4u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 75u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, arrOpen[0].iSenderChannel, "one\n", 4u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 76u;
				goto done;
			}
			for ( iChannel = 0u; iChannel < 2u; ++iChannel ) {
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelRequestExitStatusWrite(&wr, arrOpen[iChannel].iSenderChannel, 0u) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 77u;
					goto done;
				}
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelCloseWrite(&wr, arrOpen[iChannel].iSenderChannel) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 78u;
					goto done;
				}
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableExecChannel ) {
			uint32 iServerChannel = 700u;
			xsshreader rdReq;
			xsshslice cmd;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "session") ) {
				iRetCode = 22u;
				goto done;
			}
			pServer->bChannelOpen = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 23u;
				goto done;
			}

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &channelReq) != XSSH_OK ||
				channelReq.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(channelReq.tRequestType, "exec") ||
				!channelReq.bWantReply ) {
				iRetCode = 24u;
				goto done;
			}
			xsshReaderInit(&rdReq, channelReq.tPayload.pData, channelReq.tPayload.iLen);
			if ( xsshWireReadString(&rdReq, &cmd) != XSSH_OK || xsshMsgReadNoTrailing(&rdReq) != XSSH_OK ) {
				iRetCode = 25u;
				goto done;
			}
			if ( cmd.iLen < sizeof(pServer->sExecCommand) ) {
				memcpy(pServer->sExecCommand, cmd.pData, cmd.iLen);
				pServer->sExecCommand[cmd.iLen] = '\0';
			}
			pServer->bExecRequest = TRUE;

			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelSuccessWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 26u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, openMsg.iSenderChannel, "remote ok\n", 10u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 27u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelExtendedDataWrite(&wr, openMsg.iSenderChannel, 1u, "remote err\n", 11u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 28u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelRequestExitStatusWrite(&wr, openMsg.iSenderChannel, 7u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 29u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelRequestExitSignalWrite(&wr, openMsg.iSenderChannel, "TERM", FALSE, "terminated", "en") != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 46u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelEofWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 30u;
				goto done;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelCloseWrite(&wr, openMsg.iSenderChannel) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 31u;
				goto done;
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableDirectTcpipReject ) {
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "direct-tcpip") ||
				xsshChannelOpenTcpipExtraRead(&openMsg, &tcpipOpen) != XSSH_OK ||
				!xsshSliceEqualText(tcpipOpen.tConnectedHost, "blocked.example") ||
				tcpipOpen.iConnectedPort != 2222u ) {
				iRetCode = 56u;
				goto done;
			}
			if ( tcpipOpen.tConnectedHost.iLen < sizeof(pServer->sForwardTargetHost) ) {
				memcpy(pServer->sForwardTargetHost, tcpipOpen.tConnectedHost.pData, tcpipOpen.tConnectedHost.iLen);
				pServer->sForwardTargetHost[tcpipOpen.tConnectedHost.iLen] = '\0';
			}
			pServer->bSawDirectTcpip = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenFailureWrite(&wr, openMsg.iSenderChannel, 2u, "blocked by policy", "en") != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 57u;
				goto done;
			}
		} else if ( pServer->bEnableDirectTcpipData ) {
			uint32 iServerChannel = 702u;
			uint32 iCloseRecipient = 0u;
			xsshchanneldataeventmsg dataMsg;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
				!xsshSliceEqualText(openMsg.tChannelType, "direct-tcpip") ||
				xsshChannelOpenTcpipExtraRead(&openMsg, &tcpipOpen) != XSSH_OK ||
				!xsshSliceEqualText(tcpipOpen.tConnectedHost, "example.com") ||
				tcpipOpen.iConnectedPort != 22u ) {
				iRetCode = 39u;
				goto done;
			}
			if ( tcpipOpen.tConnectedHost.iLen < sizeof(pServer->sForwardTargetHost) ) {
				memcpy(pServer->sForwardTargetHost, tcpipOpen.tConnectedHost.pData, tcpipOpen.tConnectedHost.iLen);
				pServer->sForwardTargetHost[tcpipOpen.tConnectedHost.iLen] = '\0';
			}
			pServer->bSawDirectTcpip = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 40u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelDataRead(pkt.tPayload.pData, pkt.tPayload.iLen, &dataMsg) != XSSH_OK ||
				dataMsg.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(dataMsg.tData, "ping-") ) {
				iRetCode = 41u;
				goto done;
			}
			memcpy(pServer->sForwardData, dataMsg.tData.pData, dataMsg.tData.iLen);
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelWindowAdjustWrite(&wr, openMsg.iSenderChannel, 64u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 42u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelDataRead(pkt.tPayload.pData, pkt.tPayload.iLen, &dataMsg) != XSSH_OK ||
				dataMsg.iRecipientChannel != iServerChannel ||
				!xsshSliceEqualText(dataMsg.tData, "through") ) {
				iRetCode = 43u;
				goto done;
			}
			memcpy(pServer->sForwardData + 5u, dataMsg.tData.pData, dataMsg.tData.iLen);
			pServer->sForwardData[5u + dataMsg.tData.iLen] = '\0';
			pServer->bSawDirectTcpipData = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshChannelDataWrite(&wr, openMsg.iSenderChannel, "pong-through", 12u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 44u;
				goto done;
			}
			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
				iCloseRecipient != iServerChannel ) {
				iRetCode = 45u;
				goto done;
			}
			pServer->bChannelClosed = TRUE;
		} else if ( pServer->bEnableForwarding ) {
			uint32 iServerChannel = 701u;
			uint32 iCloseRecipient = 0u;

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadGlobalRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &globalReq) != XSSH_OK ||
				!xsshSliceEqualText(globalReq.tRequestName, "tcpip-forward") ||
				!globalReq.bWantReply ||
				xsshMsgReadTcpipForwardPayload(&globalReq, &tcpipReq) != XSSH_OK ||
				!xsshSliceEqualText(tcpipReq.tBindAddress, "127.0.0.1") ||
				tcpipReq.iBindPort != 0u ) {
				iRetCode = 32u;
				goto done;
			}
			pServer->bSawTcpipForward = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshMsgWriteRequestSuccess(&wr, TRUE, 12022u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 33u;
				goto done;
			}

			if ( pServer->bEnableRemoteForwardData ) {
				xsshchannelopenconfirmmsg confirmMsg;

				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelOpenForwardedTcpipWrite(&wr, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u,
						"127.0.0.1", 12022u, "10.0.0.5", 50001u) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 34u;
					goto done;
				}
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelOpenConfirmationRead(pkt.tPayload.pData, pkt.tPayload.iLen, &confirmMsg) != XSSH_OK ||
					confirmMsg.iRecipientChannel != iServerChannel ) {
					iRetCode = 35u;
					goto done;
				}
				pServer->bSawForwardedTcpip = TRUE;
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelDataWrite(&wr, confirmMsg.iSenderChannel, "remote-forward", 14u) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 36u;
					goto done;
				}
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
					iCloseRecipient != iServerChannel ) {
					iRetCode = 37u;
					goto done;
				}
				pServer->bChannelClosed = TRUE;
			} else {
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
					!xsshSliceEqualText(openMsg.tChannelType, "direct-tcpip") ||
					xsshChannelOpenTcpipExtraRead(&openMsg, &tcpipOpen) != XSSH_OK ||
					!xsshSliceEqualText(tcpipOpen.tConnectedHost, "example.com") ||
					tcpipOpen.iConnectedPort != 22u ) {
					iRetCode = 34u;
					goto done;
				}
				if ( tcpipOpen.tConnectedHost.iLen < sizeof(pServer->sForwardTargetHost) ) {
					memcpy(pServer->sForwardTargetHost, tcpipOpen.tConnectedHost.pData, tcpipOpen.tConnectedHost.iLen);
					pServer->sForwardTargetHost[tcpipOpen.tConnectedHost.iLen] = '\0';
				}
				pServer->bSawDirectTcpip = TRUE;
				xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
				if ( xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, iServerChannel, XSSH_CHANNEL_WINDOW_DEFAULT, 32768u) != XSSH_OK ||
					!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
					iRetCode = 35u;
					goto done;
				}
				if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
					xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseRecipient) != XSSH_OK ||
					iCloseRecipient != iServerChannel ) {
					iRetCode = 36u;
					goto done;
				}
				pServer->bChannelClosed = TRUE;
			}

			if ( !xssh_runtime_server_recv_packet(hClient, &codec, &pkt, arrPlain, sizeof(arrPlain)) ||
				xsshMsgReadGlobalRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &globalReq) != XSSH_OK ||
				!xsshSliceEqualText(globalReq.tRequestName, "cancel-tcpip-forward") ||
				!globalReq.bWantReply ||
				xsshMsgReadTcpipForwardPayload(&globalReq, &tcpipReq) != XSSH_OK ||
				!xsshSliceEqualText(tcpipReq.tBindAddress, "127.0.0.1") ||
				tcpipReq.iBindPort != 12022u ) {
				iRetCode = 37u;
				goto done;
			}
			pServer->bSawCancelForward = TRUE;
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			if ( xsshMsgWriteRequestSuccess(&wr, FALSE, 0u) != XSSH_OK ||
				!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
				iRetCode = 38u;
				goto done;
			}
		}
	} else {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshAuthWriteFailure(&wr, "password", FALSE) != XSSH_OK ||
			!xssh_runtime_server_send_packet(hClient, &codec, arrPayload, wr.iLen) ) {
			iRetCode = 20u;
			goto done;
		}
		iRetCode = 21u;
		goto done;
	}

done:
	xsshPacketCodecClear(&codec);
	xsshSecureZero(arrServerPriv, sizeof(arrServerPriv));
	xsshSecureZero(arrSecret, sizeof(arrSecret));
	xsshSecureZero(arrHostSeed, sizeof(arrHostSeed));
	xsshSecureZero(arrSig, sizeof(arrSig));
	xsshSecureZero(arrPlain, sizeof(arrPlain));
	xsshSecureZero(arrPayload, sizeof(arrPayload));
	xsshSecureZero(arrC2SIV, sizeof(arrC2SIV));
	xsshSecureZero(arrS2CIV, sizeof(arrS2CIV));
	xsshSecureZero(arrC2SKey, sizeof(arrC2SKey));
	xsshSecureZero(arrS2CKey, sizeof(arrS2CKey));
	XSSH_TEST_CLOSESOCK(hClient);
	return iRetCode;
}

static bool xssh_runtime_banner_server_start(xssh_runtime_banner_server* pServer)
{
	struct sockaddr_in tAddr;
	socklen_t iAddrLen;
	int iOpt = 1;

	(void)xrtInit();
	memset(pServer, 0, sizeof(*pServer));
	pServer->hListen = XSOCKET_INVALID;
	pServer->hListen = socket(AF_INET, SOCK_STREAM, 0);
	if ( pServer->hListen == XSOCKET_INVALID ) {
		return FALSE;
	}
	setsockopt(pServer->hListen, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt));
	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.sin_family = AF_INET;
	tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	tAddr.sin_port = 0;
	if ( bind(pServer->hListen, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 ) {
		XSSH_TEST_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	if ( listen(pServer->hListen, 1) != 0 ) {
		XSSH_TEST_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		XSSH_TEST_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xssh_runtime_banner_server_stop(xssh_runtime_banner_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		XSSH_TEST_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xssh_runtime_expect_event(xssh_session_t* pSession, int iType, xssh_channel_t* pChannel, const char* sData)
{
	xsshevent ev;
	size_t iLen = sData ? strlen(sData) : 0u;

	if ( xsshNextEvent(pSession, &ev) != XSSH_OK ) {
		return xssh_runtime_test_fail("runtime_event", "next event failed");
	}
	if ( ev.iType != iType || ev.pChannel != pChannel ) {
		return xssh_runtime_test_fail("runtime_event", "event metadata mismatch");
	}
	if ( iLen != 0u && (ev.iLen != iLen || memcmp(ev.pData, sData, iLen) != 0) ) {
		return xssh_runtime_test_fail("runtime_event", "event data mismatch");
	}
	return 0;
}

static int xssh_runtime_test_mock_exec(void)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshresult result;
	char arrBuf[32];
	size_t iRead = 0u;
	size_t iWritten = 0u;
	uint32 iExitStatus = 99u;
	xsshevent ev;

	xsshConfigInit(&cfg);
	cfg.sHost = "mock";
	cfg.sUser = "alice";
	xsshAuthInit(&auth);
	auth.sPassword = "secret";

	if ( xsshConnect(&cfg, &auth, &pSession) != XSSH_OK || pSession == NULL ) {
		return xssh_runtime_test_fail("runtime_mock_exec", "connect failed");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		!result.bSuccess ||
		result.iError != XSSH_OK ||
		strcmp(result.sServerBanner, "SSH-2.0-xssh-mock-server") != 0 ||
		strcmp(result.sKex, "curve25519-sha256") != 0 ) {
		return xssh_runtime_test_fail("runtime_mock_exec", "result mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		return 1;
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "open channel failed");
	}
	if ( xsshChannelRequestPty(pChannel, NULL) != XSSH_OK ||
		xsshChannelSetEnv(pChannel, "LANG", "zh_CN.UTF-8") != XSSH_OK ||
		xsshChannelRequestExec(pChannel, "stderr then ok") != XSSH_OK ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "channel request failed");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CHANNEL_EXTENDED_DATA, pChannel, "err\n") ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_CHANNEL_DATA, pChannel, "ok\n") ) {
		xsshFree(pSession);
		return 1;
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 3u ||
		memcmp(arrBuf, "ok\n", 3u) != 0 ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "stdout read mismatch");
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( xsshChannelReadStderr(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 4u ||
		memcmp(arrBuf, "err\n", 4u) != 0 ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "stderr read mismatch");
	}
	if ( xsshChannelGetExitStatus(pChannel, &iExitStatus) != XSSH_OK || iExitStatus != 0u ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "exit status mismatch");
	}
	if ( xsshChannelWrite(pChannel, "echo", 4u, &iWritten) != XSSH_OK || iWritten != 4u ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_CHANNEL_DATA, pChannel, "echo") ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "channel write mismatch");
	}
	if ( xsshChannelSendEof(pChannel) != XSSH_OK ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_CHANNEL_EOF, pChannel, NULL) ||
		xsshChannelClose(pChannel) != XSSH_OK ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_CHANNEL_CLOSE, pChannel, NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "close/eof mismatch");
	}
	if ( xsshPoll(pSession, 0u) != XSSH_OK ||
		xsshDisconnect(pSession) != XSSH_OK ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_DISCONNECT, NULL, NULL) ||
		xsshNextEvent(pSession, &ev) != XSSH_OK ||
		ev.iType != XSSH_EVENT_NONE ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_mock_exec", "poll/disconnect mismatch");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_forwarding_api(void)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	uint16 iBoundPort = 0u;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "mock";
	cfg.sUser = "alice";
	auth.sPassword = "secret";

	if ( xsshConnect(&cfg, &auth, &pSession) != XSSH_OK ) {
		return xssh_runtime_test_fail("runtime_forward", "connect failed");
	}
	if ( xsshOpenDirectTcpipChannel(pSession, &pChannel, "example.com", 22u, "127.0.0.1", 50000u) != XSSH_OK ||
		pChannel == NULL ||
		xsshChannelClose(pChannel) != XSSH_OK ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_forward", "direct-tcpip channel failed");
	}
	if ( xsshRequestTcpipForward(pSession, "127.0.0.1", 0u, &iBoundPort) != XSSH_OK ||
		iBoundPort != 10022u ||
		xsshCancelTcpipForward(pSession, "127.0.0.1", iBoundPort) != XSSH_OK ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_forward", "tcpip-forward failed");
	}
	if ( xsshRequestTcpipForward(pSession, "127.0.0.1", 8022u, &iBoundPort) != XSSH_OK ||
		iBoundPort != 8022u ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_forward", "fixed forward port failed");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_errors(void)
{
	xsshconfig cfg;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;

	xsshConfigInit(&cfg);
	cfg.sHost = "mock";
	if ( xsshConnect(&cfg, NULL, &pSession) != XSSH_OK || pSession == NULL ) {
		return xssh_runtime_test_fail("runtime_errors", "mock connect without auth failed");
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_ERR_AUTH_FAILED || pChannel != NULL ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_errors", "unauth channel accepted");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_config_limits(void)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "mock";
	cfg.iMaxPacketSize = 0xffffffffu;
	cfg.iChannelWindow = 0xffffffffu;
	auth.sPassword = "secret";
	if ( xsshConnect(&cfg, &auth, &pSession) != XSSH_OK || pSession == NULL ) {
		return xssh_runtime_test_fail("runtime_config_limits", "mock connect failed");
	}
	if ( pSession->tConfig.iMaxPacketSize != XSSH_PACKET_MAX_DEFAULT ||
		pSession->tConfig.iChannelWindow != XSSH_CHANNEL_WINDOW_MAX ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_config_limits", "config clamp mismatch");
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ||
		pChannel->iLocalWindow != XSSH_CHANNEL_WINDOW_MAX ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_config_limits", "channel window clamp mismatch");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_auth_packet_limit(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_auth_packet_limit", "server start failed");
	}
	tServer.bEnableAuthPacketFlood = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_auth_packet_limit", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);
	if ( iRet != XSSH_ERR_AUTH_FAILED || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_auth_packet_limit", "auth flood was not rejected");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.iError != XSSH_ERR_AUTH_FAILED ||
		result.iStage != XSSH_STAGE_AUTH ||
		result.bSuccess ||
		strstr(result.sError, "too many packets") == NULL ||
		strstr(result.sError, "secret") != NULL ||
		!tServer.bSawClientBanner ||
		!tServer.bKexDone ||
		!tServer.bSawServiceRequest ||
		strcmp(tServer.sAuthUser, "alice") != 0 ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_auth_packet_limit", "auth flood result mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_ERROR, NULL, NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_auth_packet_limit", "events missing");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_fault_injection_one(int iFaultMode, const char* sCaseName, int iExpectedError, const char* sErrorNeedle)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail(sCaseName, "server start failed");
	}
	tServer.iFaultMode = iFaultMode;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail(sCaseName, "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sCaseName, "connect/auth failed before injected fault");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sCaseName, "connect/auth events missing");
	}
	iRet = xsshPoll(pSession, 3000u);
	xrtThreadWait(pThread);
	if ( iRet != iExpectedError ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail(sCaseName, "injected fault returned unexpected error");
	}
	if ( !tServer.bFaultInjected ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail(sCaseName, "server did not inject fault packet");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.bSuccess ||
		result.iError != iExpectedError ||
		result.iStage != XSSH_STAGE_CHANNEL ||
		(sErrorNeedle != NULL && strstr(result.sError, sErrorNeedle) == NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail(sCaseName, "result did not preserve fault details");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_fault_injection_transport(void)
{
	if ( xssh_runtime_test_fault_injection_one(XSSH_RUNTIME_FAULT_BAD_GCM_TAG,
			"runtime_fault_bad_gcm_tag", XSSH_ERR_BAD_PACKET, "packet decode failed") ) return 1;
	if ( xssh_runtime_test_fault_injection_one(XSSH_RUNTIME_FAULT_BAD_PACKET_LENGTH,
			"runtime_fault_bad_packet_length", XSSH_ERR_BAD_PACKET, "packet decode failed") ) return 1;
	if ( xssh_runtime_test_fault_injection_one(XSSH_RUNTIME_FAULT_HALF_PACKET,
			"runtime_fault_half_packet", XSSH_ERR_IO, "stream closed while reading packet") ) return 1;
	if ( xssh_runtime_test_fault_injection_one(XSSH_RUNTIME_FAULT_OUT_OF_ORDER_CHANNEL,
			"runtime_fault_out_of_order_channel", XSSH_ERR_BAD_PACKET, "recipient not found") ) return 1;
	return 0;
}

static int xssh_runtime_test_remote_disconnect_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_remote_disconnect", "server start failed");
	}
	tServer.bEnableDisconnectAfterAuth = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "connect/auth failed");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "connect/auth events missing");
	}
	iRet = xsshPoll(pSession, 0u);
	if ( iRet != XSSH_ERR_CLOSED ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "disconnect poll did not close");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.iError != XSSH_ERR_CLOSED ||
		result.iStage != XSSH_STAGE_DISCONNECT ||
		result.iDisconnectReason != 11 ||
		strstr(result.sError, "server shutdown") == NULL ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "disconnect result mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_DISCONNECT, NULL, NULL) ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_disconnect", "disconnect event missing");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_server_global_request_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshevent ev;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_server_global_request", "server start failed");
	}
	tServer.bEnableServerGlobalRequest = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_server_global_request", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_server_global_request", "connect/auth failed");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_server_global_request", "connect/auth events missing");
	}
	if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_server_global_request", "poll failed");
	}
	xrtThreadWait(pThread);
	if ( !tServer.bSawServerGlobalFailure ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_server_global_request", "server did not receive REQUEST_FAILURE");
	}
	if ( xsshNextEvent(pSession, &ev) != XSSH_OK || ev.iType != XSSH_EVENT_NONE ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_server_global_request", "unexpected event");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_keepalive_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshevent ev;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_keepalive_transport", "server start failed");
	}
	tServer.bEnableKeepalive = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_keepalive_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_keepalive_transport", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshSendIgnore(pSession, "pad", 3u) != XSSH_OK ||
		xsshSendDebug(pSession, FALSE, "debug note", "en") != XSSH_OK ||
		xsshSendKeepalive(pSession) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_keepalive_transport", "send keepalive path failed");
	}
	if ( !tServer.bSawIgnore || !tServer.bSawDebug || !tServer.bSawKeepalive ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_keepalive_transport", "server transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_env_transport_case(const char* sName, bool bReject)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshresult result;
	xsshevent ev;
	const char* sEnvName = bReject ? "LC_SECRET" : "LANG";
	const char* sEnvValue = bReject ? "nope" : "zh_CN.UTF-8";
	bool bSawClose = FALSE;
	int i;
	int iRet;
	int iEnvRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail(sName, "server start failed");
	}
	tServer.bEnableEnvRequest = TRUE;
	tServer.bRejectEnvRequest = bReject;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail(sName, "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sName, "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sName, "open channel failed");
	}

	/* 失败分支必须保留 channel failure 上下文，方便上层区分 env 被服务器拒绝。 */
	iEnvRet = xsshChannelSetEnv(pChannel, sEnvName, sEnvValue);
	if ( !bReject && iEnvRet != XSSH_OK ) {
		xsshChannelClose(pChannel);
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sName, "env success path failed");
	}
	if ( bReject ) {
		if ( iEnvRet != XSSH_ERR_CHANNEL_FAILED ||
			xsshGetLastResult(pSession, &result) != XSSH_OK ||
			result.bSuccess ||
			result.iError != XSSH_ERR_CHANNEL_FAILED ||
			result.iStage != XSSH_STAGE_CHANNEL ||
			strstr(result.sError, "env request failed") == NULL ) {
			xsshChannelClose(pChannel);
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail(sName, "env failure result mismatch");
		}
	}
	if ( xsshChannelClose(pChannel) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sName, "channel close failed");
	}
	for ( i = 0; i < 50 && !bSawClose; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail(sName, "close poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE && ev.pChannel == pChannel ) {
				bSawClose = TRUE;
			}
		}
		if ( !bSawClose ) {
			xrtSleep(10u);
		}
	}
	if ( !bSawClose ||
		!pChannel->bCloseSent ||
		!pChannel->bCloseRecv ||
		!pChannel->bClosed ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail(sName, "close handshake mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	if ( !tServer.bChannelOpen ||
		!tServer.bEnvRequest ||
		!tServer.bChannelClosed ||
		strcmp(tServer.sEnvName, sEnvName) != 0 ||
		strcmp(tServer.sEnvValue, sEnvValue) != 0 ) {
		return xssh_runtime_test_fail(sName, "server transcript mismatch");
	}
	return 0;
}

static int xssh_runtime_test_env_transport(void)
{
	if ( xssh_runtime_test_env_transport_case("runtime_env_success", FALSE) ) return 1;
	if ( xssh_runtime_test_env_transport_case("runtime_env_failure", TRUE) ) return 1;
	return 0;
}

static int xssh_runtime_test_password_transport_case(const char* sName, const char* sPassword,
	bool bExpectSuccess, bool bUseAES256GCM, bool bFirstKexFollows)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	const char* sExpectedCipher = bUseAES256GCM ? "aes256-gcm@openssh.com" : "aes128-gcm@openssh.com";
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = sPassword;
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail(sName, "server start failed");
	}
	tServer.bUseAES256GCM = bUseAES256GCM;
	tServer.bEnableFirstKexPacketFollows = bFirstKexFollows;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail(sName, "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);

	if ( bExpectSuccess ) {
		if ( iRet != XSSH_OK || pSession == NULL ) {
			if ( pSession != NULL ) {
				xsshFree(pSession);
			}
			return xssh_runtime_test_fail(sName, "connect password auth failed");
		}
		if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
			result.iError != XSSH_OK ||
			result.iStage != XSSH_STAGE_AUTH ||
			!result.bSuccess ||
			strcmp(result.sServerBanner, "SSH-2.0-xssh-banner-test") != 0 ||
			strcmp(result.sKex, "curve25519-sha256") != 0 ||
			strcmp(result.sHostKey, "ssh-ed25519") != 0 ||
			result.sAuthMethods[0] != '\0' ||
			memcmp(result.sHostKeyFingerprint, "SHA256:", strlen("SHA256:")) != 0 ||
			strchr(result.sHostKeyFingerprint, '=') != NULL ||
			strcmp(result.sCipherClientToServer, sExpectedCipher) != 0 ||
			strcmp(result.sCipherServerToClient, sExpectedCipher) != 0 ||
			!tServer.bSawClientBanner ||
			!tServer.bKexDone ||
			(bFirstKexFollows && !tServer.bSentFirstKexFollower) ||
			!tServer.bSawServiceRequest ||
			!tServer.bSawPasswordAuth ||
			!tServer.bAuthDone ||
			strcmp(tServer.sAuthUser, "alice") != 0 ||
			strcmp(tServer.sClientBanner, XSSH_CLIENT_BANNER) != 0 ) {
			xsshFree(pSession);
			return xssh_runtime_test_fail(sName, "password USERAUTH exchange mismatch");
		}
		if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
			xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
			xsshFree(pSession);
			return xssh_runtime_test_fail(sName, "connect/auth events missing");
		}
		xsshFree(pSession);
		return 0;
	}

	if ( iRet != XSSH_ERR_AUTH_FAILED || pSession == NULL ) {
		if ( pSession != NULL ) {
			xsshFree(pSession);
		}
		return xssh_runtime_test_fail(sName, "bad password was not rejected");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.iError != XSSH_ERR_AUTH_FAILED ||
		result.iStage != XSSH_STAGE_AUTH ||
		result.bSuccess ||
		strcmp(result.sAuthMethods, "password") != 0 ||
		strstr(result.sError, "password authentication failed") == NULL ||
		strstr(result.sError, "wrong-secret") != NULL ||
		strcmp(result.sServerBanner, "SSH-2.0-xssh-banner-test") != 0 ||
		strcmp(result.sKex, "curve25519-sha256") != 0 ||
		strcmp(result.sHostKey, "ssh-ed25519") != 0 ||
		memcmp(result.sHostKeyFingerprint, "SHA256:", strlen("SHA256:")) != 0 ||
		strchr(result.sHostKeyFingerprint, '=') != NULL ||
		strcmp(result.sCipherClientToServer, sExpectedCipher) != 0 ||
		strcmp(result.sCipherServerToClient, sExpectedCipher) != 0 ||
		!tServer.bSawClientBanner ||
		!tServer.bKexDone ||
		(bFirstKexFollows && !tServer.bSentFirstKexFollower) ||
		!tServer.bSawServiceRequest ||
		!tServer.bSawPasswordAuth ||
		tServer.bAuthDone ||
		strcmp(tServer.sAuthUser, "alice") != 0 ||
		strcmp(tServer.sClientBanner, XSSH_CLIENT_BANNER) != 0 ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail(sName, "password failure exchange mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_ERROR, NULL, NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail(sName, "connect/auth-failure events missing");
	}
	xsshFree(pSession);
	return 0;
}

static int xssh_runtime_test_password_transport(void)
{
	if ( xssh_runtime_test_password_transport_case("runtime_password_success", "secret", TRUE, FALSE, FALSE) ) return 1;
	if ( xssh_runtime_test_password_transport_case("runtime_password_failure", "wrong-secret", FALSE, FALSE, FALSE) ) return 1;
	if ( xssh_runtime_test_password_transport_case("runtime_password_aes256_gcm", "secret", TRUE, TRUE, FALSE) ) return 1;
	if ( xssh_runtime_test_password_transport_case("runtime_password_first_kex_follows", "secret", TRUE, FALSE, TRUE) ) return 1;
	return 0;
}

static int xssh_runtime_test_publickey_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_runtime_signer signer;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	uint8 arrPub[32];
	uint8 arrKeyBlob[128];
	xsshwriter keyWr;
	size_t i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	memset(&signer, 0, sizeof(signer));
	for ( i = 0u; i < sizeof(signer.arrSeed); ++i ) {
		signer.arrSeed[i] = (uint8)(0x30u + i);
	}
	xrtEd25519PublicKey(arrPub, signer.arrSeed);
	xsshWriterInit(&keyWr, arrKeyBlob, sizeof(arrKeyBlob));
	if ( xsshPublicKeyWriteEd25519(&keyWr, arrPub) != XSSH_OK ) {
		return xssh_runtime_test_fail("runtime_publickey_transport", "key blob write failed");
	}
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPublicKeyAlgorithm = "ssh-ed25519";
	auth.pPublicKeyBlob = arrKeyBlob;
	auth.iPublicKeyBlobLen = keyWr.iLen;
	auth.pSign = xssh_runtime_ed25519_sign;
	auth.pSignerUser = &signer;
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_publickey_transport", "server start failed");
	}
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_publickey_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_publickey_transport", "connect/auth failed");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		!result.bSuccess ||
		result.iError != XSSH_OK ||
		result.iStage != XSSH_STAGE_AUTH ||
		!tServer.bSawPublicKeyAuth ||
		!tServer.bAuthDone ||
		tServer.bSawPasswordAuth ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_publickey_transport", "publickey transcript mismatch");
	}
	xsshFree(pSession);
	xsshSecureZero(&signer, sizeof(signer));
	xsshSecureZero(arrKeyBlob, sizeof(arrKeyBlob));
	return 0;
}

static int xssh_runtime_test_publickey_failure_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_runtime_signer signer;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	uint8 arrKeyBlob[] = { 0u, 0u, 0u, 7u, 's', 's', 'h', '-', 'r', 's', 'a' };
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	memset(&signer, 0, sizeof(signer));
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPublicKeyAlgorithm = "ssh-rsa";
	auth.pPublicKeyBlob = arrKeyBlob;
	auth.iPublicKeyBlobLen = sizeof(arrKeyBlob);
	auth.pSign = xssh_runtime_ed25519_sign;
	auth.pSignerUser = &signer;
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_publickey_failure_transport", "server start failed");
	}
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_publickey_failure_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);
	if ( iRet != XSSH_ERR_AUTH_FAILED || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_publickey_failure_transport", "bad publickey was not rejected");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.iError != XSSH_ERR_AUTH_FAILED ||
		result.iStage != XSSH_STAGE_AUTH ||
		result.bSuccess ||
		strcmp(result.sAuthMethods, "password") != 0 ||
		strstr(result.sError, "publickey authentication failed") == NULL ||
		!tServer.bSawClientBanner ||
		!tServer.bKexDone ||
		!tServer.bSawServiceRequest ||
		tServer.bSawPasswordAuth ||
		tServer.bSawPublicKeyAuth ||
		tServer.bAuthDone ||
		strcmp(tServer.sAuthUser, "alice") != 0 ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_publickey_failure_transport", "publickey failure transcript mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_ERROR, NULL, NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_publickey_failure_transport", "connect/auth-failure events missing");
	}
	xsshFree(pSession);
	xsshSecureZero(&signer, sizeof(signer));
	return 0;
}

static int xssh_runtime_test_private_key_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	uint8 arrSeed[32];
	char sPem[2048];
	size_t i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)(0x40u + i);
	}
	if ( xssh_runtime_make_openssh_ed25519_private_key(sPem, sizeof(sPem), arrSeed) != XSSH_OK ) {
		return xssh_runtime_test_fail("runtime_private_key_transport", "pem build failed");
	}
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.pPrivateKeyData = sPem;
	auth.iPrivateKeySize = strlen(sPem);
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_private_key_transport", "server start failed");
	}
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_private_key_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_private_key_transport", "connect/auth failed");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		!result.bSuccess ||
		result.iError != XSSH_OK ||
		result.iStage != XSSH_STAGE_AUTH ||
		!tServer.bSawPublicKeyAuth ||
		!tServer.bAuthDone ||
		tServer.bSawPasswordAuth ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_private_key_transport", "private key transcript mismatch");
	}
	xsshFree(pSession);
	xsshSecureZero(sPem, sizeof(sPem));
	xsshSecureZero(arrSeed, sizeof(arrSeed));
	return 0;
}

static int xssh_runtime_test_keyboard_interactive_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_runtime_kbdint kbd;
	xssh_session_t* pSession = NULL;
	xsshresult result;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	memset(&kbd, 0, sizeof(kbd));
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.pKeyboardInteractive = xssh_runtime_keyboard_interactive;
	auth.pKeyboardUser = &kbd;
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_keyboard_interactive_transport", "server start failed");
	}
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_keyboard_interactive_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	xrtThreadWait(pThread);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_keyboard_interactive_transport", "connect/auth failed");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		!result.bSuccess ||
		result.iError != XSSH_OK ||
		result.iStage != XSSH_STAGE_AUTH ||
		kbd.iCalls != 2 ||
		!tServer.bSawKeyboardInteractiveAuth ||
		!tServer.bAuthDone ||
		tServer.bSawPasswordAuth ||
		tServer.bSawPublicKeyAuth ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_keyboard_interactive_transport", "keyboard-interactive transcript mismatch");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		return xssh_runtime_test_fail("runtime_keyboard_interactive_transport", "connect/auth events missing");
	}
	xsshFree(pSession);
	xsshSecureZero(&kbd, sizeof(kbd));
	return 0;
}

static int xssh_runtime_test_shell_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xsshpty pty;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	char arrBuf[64];
	size_t iWritten = 0u;
	size_t iRead = 0u;
	bool bSawEcho = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	xsshPtyInit(&pty);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	pty.iCols = 100u;
	pty.iRows = 30u;
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_shell_transport", "server start failed");
	}
	tServer.bEnableShellEcho = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_shell_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_shell_transport", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ||
		xsshChannelRequestPty(pChannel, &pty) != XSSH_OK ||
		xsshChannelRequestShell(pChannel) != XSSH_OK ||
		xsshChannelWrite(pChannel, "hello\n", 6u, &iWritten) != XSSH_OK ||
		iWritten != 6u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_shell_transport", "shell open/write failed");
	}
	for ( i = 0; i < 50 && !bSawEcho; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_shell_transport", "poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.pChannel == pChannel &&
				ev.iType == XSSH_EVENT_CHANNEL_DATA &&
				ev.iLen == 12u &&
				memcmp(ev.pData, "shell:hello\n", 12u) == 0 ) {
				bSawEcho = TRUE;
			}
		}
		if ( !bSawEcho ) {
			xrtSleep(10u);
		}
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( !bSawEcho ||
		xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 12u ||
		memcmp(arrBuf, "shell:hello\n", 12u) != 0 ||
		xsshChannelClose(pChannel) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_shell_transport", "shell echo mismatch");
	}
	if ( !tServer.bChannelOpen ||
		!tServer.bPtyRequest ||
		!tServer.bShellRequest ||
		!tServer.bChannelClosed ||
		strcmp(tServer.sShellInput, "hello\n") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_shell_transport", "server transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_subsystem_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	char arrBuf[64];
	size_t iWritten = 0u;
	size_t iRead = 0u;
	bool bSawData = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_subsystem_transport", "server start failed");
	}
	tServer.bEnableSubsystemEcho = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_subsystem_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_subsystem_transport", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ||
		xsshChannelRequestSubsystem(pChannel, "raw") != XSSH_OK ||
		xsshChannelWrite(pChannel, "subsystem-ping", 14u, &iWritten) != XSSH_OK ||
		iWritten != 14u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_subsystem_transport", "subsystem open/write failed");
	}
	for ( i = 0; i < 50 && !bSawData; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_subsystem_transport", "poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.pChannel == pChannel &&
				ev.iType == XSSH_EVENT_CHANNEL_DATA &&
				ev.iLen == 14u &&
				memcmp(ev.pData, "subsystem:pong", 14u) == 0 ) {
				bSawData = TRUE;
			}
		}
		if ( !bSawData ) {
			xrtSleep(10u);
		}
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( !bSawData ||
		xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 14u ||
		memcmp(arrBuf, "subsystem:pong", 14u) != 0 ||
		xsshChannelClose(pChannel) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_subsystem_transport", "subsystem data mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	if ( !tServer.bChannelOpen ||
		!tServer.bSubsystemRequest ||
		!tServer.bChannelClosed ||
		strcmp(tServer.sSubsystemName, "raw") != 0 ||
		strcmp(tServer.sSubsystemInput, "subsystem-ping") != 0 ) {
		return xssh_runtime_test_fail("runtime_subsystem_transport", "server transcript mismatch");
	}
	return 0;
}

static int xssh_runtime_test_exec_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	char arrBuf[64];
	char sExitSignal[64];
	size_t iRead = 0u;
	uint32 iExitStatus = 0u;
	bool bSawStdout = FALSE;
	bool bSawStderr = FALSE;
	bool bSawEof = FALSE;
	bool bSawClose = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_exec_transport", "server start failed");
	}
	tServer.bEnableExecChannel = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_exec_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "connect/auth failed");
	}
	if ( xssh_runtime_expect_event(pSession, XSSH_EVENT_CONNECTED, NULL, NULL) ||
		xssh_runtime_expect_event(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL) ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return 1;
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ||
		xsshChannelRequestExec(pChannel, "printf xssh") != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "open/exec failed");
	}

	for ( i = 0; i < 50 && !(bSawStdout && bSawStderr && bSawEof && bSawClose); ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_exec_transport", "poll failed");
		}
		for (;;) {
			if ( xsshNextEvent(pSession, &ev) != XSSH_OK ) {
				xsshFree(pSession);
				xrtThreadWait(pThread);
				return xssh_runtime_test_fail("runtime_exec_transport", "next event failed");
			}
			if ( ev.iType == XSSH_EVENT_NONE ) {
				break;
			}
			if ( ev.pChannel != pChannel ) {
				xsshFree(pSession);
				xrtThreadWait(pThread);
				return xssh_runtime_test_fail("runtime_exec_transport", "event channel mismatch");
			}
			if ( ev.iType == XSSH_EVENT_CHANNEL_DATA &&
				ev.iLen == 10u && memcmp(ev.pData, "remote ok\n", 10u) == 0 ) {
				bSawStdout = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_EXTENDED_DATA &&
				ev.iLen == 11u && memcmp(ev.pData, "remote err\n", 11u) == 0 ) {
				bSawStderr = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_EOF ) {
				bSawEof = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE ) {
				bSawClose = TRUE;
			} else {
				xsshFree(pSession);
				xrtThreadWait(pThread);
				return xssh_runtime_test_fail("runtime_exec_transport", "unexpected channel event");
			}
		}
		if ( !(bSawStdout && bSawStderr && bSawEof && bSawClose) ) {
			xrtSleep(10u);
		}
	}
	if ( !bSawStdout || !bSawStderr || !bSawEof || !bSawClose ||
		!tServer.bChannelOpen || !tServer.bExecRequest || !tServer.bChannelClosed ||
		strcmp(tServer.sExecCommand, "printf xssh") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "exec transcript mismatch");
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 10u || memcmp(arrBuf, "remote ok\n", 10u) != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "stdout buffer mismatch");
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( xsshChannelReadStderr(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 11u || memcmp(arrBuf, "remote err\n", 11u) != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "stderr buffer mismatch");
	}
	if ( xsshChannelGetExitStatus(pChannel, &iExitStatus) != XSSH_OK || iExitStatus != 7u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "exit status mismatch");
	}
	if ( xsshChannelGetExitSignal(pChannel, sExitSignal, sizeof(sExitSignal)) != XSSH_OK ||
		strcmp(sExitSignal, "TERM") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_exec_transport", "exit signal mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_multi_channel_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pFirst = NULL;
	xssh_channel_t* pSecond = NULL;
	xsshevent ev;
	char arrFirst[16];
	char arrSecond[16];
	size_t iRead = 0u;
	uint32 iExitStatus = 999u;
	bool bSawFirstData = FALSE;
	bool bSawSecondData = FALSE;
	bool bSawFirstClose = FALSE;
	bool bSawSecondClose = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_multi_channel", "server start failed");
	}
	tServer.bEnableMultiChannel = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_multi_channel", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_multi_channel", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshOpenSessionChannel(pSession, &pFirst) != XSSH_OK || pFirst == NULL ||
		xsshChannelRequestExec(pFirst, "printf one") != XSSH_OK ||
		xsshOpenSessionChannel(pSession, &pSecond) != XSSH_OK || pSecond == NULL ||
		xsshChannelRequestExec(pSecond, "printf two") != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_multi_channel", "open/exec failed");
	}
	for ( i = 0; i < 50 && !(bSawFirstData && bSawSecondData && bSawFirstClose && bSawSecondClose); ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_multi_channel", "poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.iType == XSSH_EVENT_CHANNEL_DATA && ev.pChannel == pFirst &&
				ev.iLen == 4u && memcmp(ev.pData, "one\n", 4u) == 0 ) {
				bSawFirstData = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_DATA && ev.pChannel == pSecond &&
				ev.iLen == 4u && memcmp(ev.pData, "two\n", 4u) == 0 ) {
				bSawSecondData = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE && ev.pChannel == pFirst ) {
				bSawFirstClose = TRUE;
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE && ev.pChannel == pSecond ) {
				bSawSecondClose = TRUE;
			}
		}
		if ( !(bSawFirstData && bSawSecondData && bSawFirstClose && bSawSecondClose) ) {
			xrtSleep(10u);
		}
	}
	memset(arrFirst, 0, sizeof(arrFirst));
	memset(arrSecond, 0, sizeof(arrSecond));
	if ( !bSawFirstData || !bSawSecondData || !bSawFirstClose || !bSawSecondClose ||
		xsshChannelRead(pFirst, arrFirst, sizeof(arrFirst), &iRead) != XSSH_OK ||
		iRead != 4u || memcmp(arrFirst, "one\n", 4u) != 0 ||
		xsshChannelRead(pSecond, arrSecond, sizeof(arrSecond), &iRead) != XSSH_OK ||
		iRead != 4u || memcmp(arrSecond, "two\n", 4u) != 0 ||
		xsshChannelGetExitStatus(pFirst, &iExitStatus) != XSSH_OK || iExitStatus != 0u ||
		xsshChannelGetExitStatus(pSecond, &iExitStatus) != XSSH_OK || iExitStatus != 0u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_multi_channel", "multi-channel transcript failed");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	if ( !tServer.bSawMultiChannel ||
		!tServer.bChannelOpen ||
		!tServer.bExecRequest ||
		!tServer.bChannelClosed ||
		strcmp(tServer.sExecCommand, "printf one") != 0 ||
		strcmp(tServer.sSecondExecCommand, "printf two") != 0 ) {
		return xssh_runtime_test_fail("runtime_multi_channel", "server transcript mismatch");
	}
	return 0;
}

static int xssh_runtime_test_forward_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	uint16 iBoundPort = 0u;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_forward_transport", "server start failed");
	}
	tServer.bEnableForwarding = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_forward_transport", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_forward_transport", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshRequestTcpipForward(pSession, "127.0.0.1", 0u, &iBoundPort) != XSSH_OK ||
		iBoundPort != 12022u ||
		xsshOpenDirectTcpipChannel(pSession, &pChannel, "example.com", 22u, "127.0.0.1", 50000u) != XSSH_OK ||
		pChannel == NULL ||
		xsshChannelClose(pChannel) != XSSH_OK ||
		xsshCancelTcpipForward(pSession, "127.0.0.1", iBoundPort) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_forward_transport", "forward transport control path failed");
	}
	if ( !tServer.bSawTcpipForward || !tServer.bSawDirectTcpip || !tServer.bSawCancelForward ||
		!tServer.bChannelClosed || strcmp(tServer.sForwardTargetHost, "example.com") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_forward_transport", "forward transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_direct_tcpip_reject_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	xsshresult result;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_direct_tcpip_reject", "server start failed");
	}
	tServer.bEnableDirectTcpipReject = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_direct_tcpip_reject", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_reject", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	iRet = xsshOpenDirectTcpipChannel(pSession, &pChannel, "blocked.example", 2222u, "127.0.0.1", 50000u);
	if ( iRet != XSSH_ERR_CHANNEL_FAILED || pChannel != NULL ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_reject", "direct-tcpip reject result mismatch");
	}
	if ( xsshGetLastResult(pSession, &result) != XSSH_OK ||
		result.iError != XSSH_ERR_CHANNEL_FAILED ||
		result.iStage != XSSH_STAGE_CHANNEL ||
		strstr(result.sError, "direct-tcpip rejected") == NULL ||
		!tServer.bSawDirectTcpip ||
		strcmp(tServer.sForwardTargetHost, "blocked.example") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_reject", "reject transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_remote_forward_data_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	uint8 arrBuf[64];
	uint16 iBoundPort = 0u;
	size_t iRead = 0u;
	bool bSawOpen = FALSE;
	bool bSawData = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_remote_forward_data", "server start failed");
	}
	tServer.bEnableForwarding = TRUE;
	tServer.bEnableRemoteForwardData = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_remote_forward_data", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_forward_data", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshRequestTcpipForward(pSession, "127.0.0.1", 0u, &iBoundPort) != XSSH_OK ||
		iBoundPort != 12022u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_forward_data", "tcpip-forward failed");
	}
	for ( i = 0; i < 50 && !bSawData; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_remote_forward_data", "poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.iType == XSSH_EVENT_CHANNEL_OPEN ) {
				pChannel = ev.pChannel;
				bSawOpen = TRUE;
				if ( pChannel == NULL ||
					strcmp(pChannel->sChannelType, "forwarded-tcpip") != 0 ||
					strcmp(pChannel->sConnectedHost, "127.0.0.1") != 0 ||
					pChannel->iConnectedPort != 12022u ||
					strcmp(pChannel->sOriginatorHost, "10.0.0.5") != 0 ||
					pChannel->iOriginatorPort != 50001u ) {
					xsshFree(pSession);
					xrtThreadWait(pThread);
					return xssh_runtime_test_fail("runtime_remote_forward_data", "forwarded-tcpip metadata mismatch");
				}
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_DATA ) {
				if ( ev.pChannel == NULL || ev.pChannel != pChannel ||
					ev.iLen != 14u || memcmp(ev.pData, "remote-forward", 14u) != 0 ) {
					xsshFree(pSession);
					xrtThreadWait(pThread);
					return xssh_runtime_test_fail("runtime_remote_forward_data", "forwarded data event mismatch");
				}
				bSawData = TRUE;
			}
		}
		if ( !bSawData ) {
			xrtSleep(10u);
		}
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( !bSawOpen || !bSawData || pChannel == NULL ||
		xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 14u ||
		memcmp(arrBuf, "remote-forward", 14u) != 0 ||
		xsshChannelClose(pChannel) != XSSH_OK ||
		xsshCancelTcpipForward(pSession, "127.0.0.1", iBoundPort) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_forward_data", "remote forward data transcript failed");
	}
	if ( !tServer.bSawTcpipForward || !tServer.bSawForwardedTcpip ||
		!tServer.bSawCancelForward || !tServer.bChannelClosed ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_remote_forward_data", "server transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

static int xssh_runtime_test_direct_tcpip_data_transport(void)
{
	xssh_runtime_banner_server tServer;
	xthread pThread;
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	xsshevent ev;
	uint8 arrBuf[64];
	size_t iWritten = 0u;
	size_t iRead = 0u;
	bool bSawData = FALSE;
	int i;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "127.0.0.1";
	cfg.sUser = "alice";
	cfg.iConnectTimeoutMs = 3000u;
	cfg.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	auth.sPassword = "secret";
	if ( !xssh_runtime_banner_server_start(&tServer) ) {
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "server start failed");
	}
	tServer.bEnableDirectTcpipData = TRUE;
	cfg.iPort = tServer.iPort;
	pThread = xrtThreadCreate((ptr)xssh_runtime_banner_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xssh_runtime_banner_server_stop(&tServer);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "server thread failed");
	}
	iRet = xsshConnect(&cfg, &auth, &pSession);
	xssh_runtime_banner_server_stop(&tServer);
	if ( iRet != XSSH_OK || pSession == NULL ) {
		if ( pSession != NULL ) xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "connect/auth failed");
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}
	if ( xsshOpenDirectTcpipChannel(pSession, &pChannel, "example.com", 22u, "127.0.0.1", 50000u) != XSSH_OK ||
		pChannel == NULL ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "direct channel open failed");
	}
	pChannel->iRemoteWindow = 5u;
	if ( xsshChannelWrite(pChannel, "ping-through", 12u, &iWritten) != XSSH_OK ||
		iWritten != 12u ||
		pChannel->iRemoteWindow != 0u ||
		xsshChannelPendingLen(pChannel) != 7u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "queued channel write failed");
	}
	for ( i = 0; i < 50 && xsshChannelPendingLen(pChannel) != 0u; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_direct_tcpip_data", "window adjust poll failed");
		}
		if ( xsshChannelPendingLen(pChannel) != 0u ) {
			xrtSleep(10u);
		}
	}
	if ( xsshChannelPendingLen(pChannel) != 0u ||
		pChannel->iRemoteWindow == 0u ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "queued write flush failed");
	}
	for ( i = 0; i < 50 && !bSawData; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			xsshFree(pSession);
			xrtThreadWait(pThread);
			return xssh_runtime_test_fail("runtime_direct_tcpip_data", "poll failed");
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.pChannel == pChannel && ev.iType == XSSH_EVENT_CHANNEL_DATA &&
				ev.iLen == 12u && memcmp(ev.pData, "pong-through", 12u) == 0 ) {
				bSawData = TRUE;
			}
		}
		if ( !bSawData ) {
			xrtSleep(10u);
		}
	}
	memset(arrBuf, 0, sizeof(arrBuf));
	if ( !bSawData ||
		xsshChannelRead(pChannel, arrBuf, sizeof(arrBuf), &iRead) != XSSH_OK ||
		iRead != 12u ||
		memcmp(arrBuf, "pong-through", 12u) != 0 ||
		xsshChannelClose(pChannel) != XSSH_OK ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "direct data transcript failed");
	}
	if ( !tServer.bSawDirectTcpip || !tServer.bSawDirectTcpipData ||
		!tServer.bChannelClosed ||
		strcmp(tServer.sForwardTargetHost, "example.com") != 0 ||
		strcmp(tServer.sForwardData, "ping-through") != 0 ) {
		xsshFree(pSession);
		xrtThreadWait(pThread);
		return xssh_runtime_test_fail("runtime_direct_tcpip_data", "server transcript mismatch");
	}
	xsshFree(pSession);
	xrtThreadWait(pThread);
	return 0;
}

int main(void)
{
	if ( xssh_runtime_test_mock_exec() ) return 1;
	if ( xssh_runtime_test_forwarding_api() ) return 1;
	if ( xssh_runtime_test_errors() ) return 1;
	if ( xssh_runtime_test_config_limits() ) return 1;
	if ( xssh_runtime_test_password_transport() ) return 1;
	if ( xssh_runtime_test_auth_packet_limit() ) return 1;
	if ( xssh_runtime_test_fault_injection_transport() ) return 1;
	if ( xssh_runtime_test_remote_disconnect_transport() ) return 1;
	if ( xssh_runtime_test_server_global_request_transport() ) return 1;
	if ( xssh_runtime_test_keepalive_transport() ) return 1;
	if ( xssh_runtime_test_env_transport() ) return 1;
	if ( xssh_runtime_test_publickey_transport() ) return 1;
	if ( xssh_runtime_test_publickey_failure_transport() ) return 1;
	if ( xssh_runtime_test_private_key_transport() ) return 1;
	if ( xssh_runtime_test_keyboard_interactive_transport() ) return 1;
	if ( xssh_runtime_test_shell_transport() ) return 1;
	if ( xssh_runtime_test_subsystem_transport() ) return 1;
	if ( xssh_runtime_test_exec_transport() ) return 1;
	if ( xssh_runtime_test_multi_channel_transport() ) return 1;
	if ( xssh_runtime_test_forward_transport() ) return 1;
	if ( xssh_runtime_test_direct_tcpip_reject_transport() ) return 1;
	if ( xssh_runtime_test_remote_forward_data_transport() ) return 1;
	if ( xssh_runtime_test_direct_tcpip_data_transport() ) return 1;

	printf("xssh_runtime_test: PASS\n");
	return 0;
}
