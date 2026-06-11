#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_mock_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_mock_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static xsshslice xssh_mock_slice(const void* pData, size_t iLen)
{
	xsshslice s;
	s.pData = (const uint8*)pData;
	s.iLen = iLen;
	return s;
}

static int xssh_mock_padding(void* pUser, uint8* pOut, size_t iLen)
{
	uint8 iBase = pUser ? *(uint8*)pUser : 0u;
	size_t i;

	for ( i = 0u; i < iLen; ++i ) {
		pOut[i] = (uint8)(iBase + i);
	}
	return XSSH_OK;
}

static int xssh_mock_send_packet(xsshpacketcodec* pCodec, uint8* pWire, size_t iWireCap, const void* pPayload, size_t iPayloadLen, size_t* pWireLen)
{
	uint8 iPadBase = 0x30u;
	xsshwriter wr;
	int iRet;

	xsshWriterInit(&wr, pWire, iWireCap);
	iRet = xsshPacketCodecWriteEx(pCodec, &wr, pPayload, iPayloadLen, xssh_mock_padding, &iPadBase);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	*pWireLen = wr.iLen;
	return XSSH_OK;
}

static int xssh_mock_recv_packet(xsshpacketcodec* pCodec, const uint8* pWire, size_t iWireLen, xsshpacket* pPkt, uint8* pPlain, size_t iPlainCap)
{
	xsshreader rd;
	int iRet;

	xsshReaderInit(&rd, pWire, iWireLen);
	iRet = xsshPacketCodecRead(pCodec, &rd, 0u, pPkt, pPlain, iPlainCap);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return (rd.iPos == iWireLen) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xssh_mock_build_kexinit(xsshkexinitdesc* pDesc, uint8* pCookie, uint8 iCookieBase, uint8* pOut, size_t iOutCap, size_t* pOutLen, xsshkexinit* pParsed)
{
	xsshwriter wr;
	size_t i;

	for ( i = 0u; i < XSSH_KEX_COOKIE_SIZE; ++i ) {
		pCookie[i] = (uint8)(iCookieBase + i);
	}
	xsshKexInitDescInit(pDesc);
	pDesc->pCookie = pCookie;
	pDesc->sKexAlgorithms = "curve25519-sha256";
	pDesc->sServerHostKeyAlgorithms = "ssh-ed25519";
	pDesc->sEncryptionClientToServer = "aes128-gcm@openssh.com";
	pDesc->sEncryptionServerToClient = "aes128-gcm@openssh.com";
	pDesc->sMacClientToServer = "";
	pDesc->sMacServerToClient = "";
	pDesc->sCompressionClientToServer = "none";
	pDesc->sCompressionServerToClient = "none";

	xsshWriterInit(&wr, pOut, iOutCap);
	if ( xsshKexInitWrite(&wr, pDesc) != XSSH_OK ||
		xsshKexInitRead(pOut, wr.iLen, pParsed) != XSSH_OK ) {
		return XSSH_ERR_BAD_PACKET;
	}
	*pOutLen = wr.iLen;
	return XSSH_OK;
}

static int xssh_mock_send_and_recv_newkeys(xsshpacketcodec* pSender, xsshpacketcodec* pReceiver)
{
	uint8 arrPayload[8];
	uint8 arrWire[128];
	uint8 arrPlain[128];
	size_t iWireLen = 0u;
	xsshwriter wr;
	xsshpacket pkt;

	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	if ( xsshMsgWriteNewKeys(&wr) != XSSH_OK ||
		xssh_mock_send_packet(pSender, arrWire, sizeof(arrWire), arrPayload, wr.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(pReceiver, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshMsgReadNewKeys(pkt.tPayload.pData, pkt.tPayload.iLen) != XSSH_OK ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xssh_mock_test_handshake_auth_exec(void)
{
	static const char* sClientVersion = "SSH-2.0-xssh-mock-client";
	static const char* sServerVersion = "SSH-2.0-xssh-mock-server";
	uint8 arrClientCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrServerCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrClientKexInit[512];
	uint8 arrServerKexInit[512];
	size_t iClientKexInitLen = 0u;
	size_t iServerKexInitLen = 0u;
	xsshkexinitdesc clientDesc;
	xsshkexinitdesc serverDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;
	uint8 arrClientPriv[32], arrClientPub[32], arrServerPriv[32], arrServerPub[32];
	uint8 arrSecretClient[32], arrSecretServer[32];
	uint8 arrHostSeed[32], arrHostPub[32], arrHostKeyBlob[128];
	uint8 arrHash[XSSH_SHA256_SIZE], arrSig[64], arrSigBlob[128];
	uint8 arrPayload[512], arrWire[1024], arrPlain[512];
	uint8 arrC2SIV[XSSH_GCM_NONCE_SIZE], arrS2CIV[XSSH_GCM_NONCE_SIZE];
	uint8 arrC2SKey[XSSH_AES128_KEY_SIZE], arrS2CKey[XSSH_AES128_KEY_SIZE];
	size_t iWireLen = 0u;
	xsshwriter wrHostKey, wrSig, wrPayload;
	xsshkexhashdesc hashDesc;
	xsshkexecdhinit init;
	xsshkexecdhreply reply;
	xsshpublickey hostKey;
	xsshsignature hostSig;
	xsshpacketcodec clientCodec;
	xsshpacketcodec serverCodec;
	xsshpacket pkt;
	xsshservicemsg service;
	xsshauthrequestmsg authReq;
	xsshchannelopenmsg openMsg;
	xsshchannelopenconfirmmsg confirmMsg;
	xsshchannelrequestmsg chanReq;
	xsshchanneldataeventmsg dataMsg;
	uint32 iExitStatus = 0u;
	uint32 iCloseChannel = 0u;
	size_t i;

	if ( xssh_mock_build_kexinit(&clientDesc, arrClientCookie, 0x10u, arrClientKexInit, sizeof(arrClientKexInit), &iClientKexInitLen, &clientKex) != XSSH_OK ||
		xssh_mock_build_kexinit(&serverDesc, arrServerCookie, 0x80u, arrServerKexInit, sizeof(arrServerKexInit), &iServerKexInitLen, &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ||
		!xssh_mock_slice_eq(neg.tKexAlgorithm, "curve25519-sha256") ||
		!xssh_mock_slice_eq(neg.tCipherClientToServer, "aes128-gcm@openssh.com") ) {
		return xssh_mock_test_fail("mock_kex", "kexinit negotiate failed");
	}

	if ( xsshKexCurve25519Keypair(arrClientPriv, arrClientPub) != XSSH_OK ||
		xsshKexCurve25519Keypair(arrServerPriv, arrServerPub) != XSSH_OK ||
		xsshKexCurve25519SharedSecret(arrSecretClient, arrClientPriv, arrServerPub) != XSSH_OK ||
		xsshKexCurve25519SharedSecret(arrSecretServer, arrServerPriv, arrClientPub) != XSSH_OK ||
		memcmp(arrSecretClient, arrSecretServer, sizeof(arrSecretClient)) != 0 ) {
		return xssh_mock_test_fail("mock_kex", "curve25519 failed");
	}

	for ( i = 0u; i < sizeof(arrHostSeed); ++i ) arrHostSeed[i] = (uint8)(0x40u + i);
	xrtEd25519PublicKey(arrHostPub, arrHostSeed);
	xsshWriterInit(&wrHostKey, arrHostKeyBlob, sizeof(arrHostKeyBlob));
	if ( xsshPublicKeyWriteEd25519(&wrHostKey, arrHostPub) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_kex", "host key blob failed");
	}
	memset(&hashDesc, 0, sizeof(hashDesc));
	hashDesc.tClientVersion = xssh_mock_slice(sClientVersion, strlen(sClientVersion));
	hashDesc.tServerVersion = xssh_mock_slice(sServerVersion, strlen(sServerVersion));
	hashDesc.tClientKexInit = xssh_mock_slice(arrClientKexInit, iClientKexInitLen);
	hashDesc.tServerKexInit = xssh_mock_slice(arrServerKexInit, iServerKexInitLen);
	hashDesc.tServerHostKey = xssh_mock_slice(arrHostKeyBlob, wrHostKey.iLen);
	hashDesc.tClientEphemeral = xssh_mock_slice(arrClientPub, sizeof(arrClientPub));
	hashDesc.tServerEphemeral = xssh_mock_slice(arrServerPub, sizeof(arrServerPub));
	hashDesc.tSharedSecret = xssh_mock_slice(arrSecretClient, sizeof(arrSecretClient));
	if ( xsshKexHashSHA256Curve25519(&hashDesc, arrHash) != XSSH_OK ||
		!xrtEd25519Sign(arrSig, arrHash, sizeof(arrHash), arrHostSeed) ) {
		return xssh_mock_test_fail("mock_kex", "exchange hash/sign failed");
	}
	xsshWriterInit(&wrSig, arrSigBlob, sizeof(arrSigBlob));
	if ( xsshSignatureWrite(&wrSig, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_kex", "signature blob failed");
	}

	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshKexECDHInitWrite(&wrPayload, arrClientPub, sizeof(arrClientPub)) != XSSH_OK ||
		xsshKexECDHInitRead(arrPayload, wrPayload.iLen, &init) != XSSH_OK ||
		memcmp(init.tClientPublic.pData, arrClientPub, sizeof(arrClientPub)) != 0 ) {
		return xssh_mock_test_fail("mock_kex", "ecdh init failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshKexECDHReplyWrite(&wrPayload,
			xssh_mock_slice(arrHostKeyBlob, wrHostKey.iLen),
			xssh_mock_slice(arrServerPub, sizeof(arrServerPub)),
			xssh_mock_slice(arrSigBlob, wrSig.iLen)) != XSSH_OK ||
		xsshKexECDHReplyRead(arrPayload, wrPayload.iLen, &reply) != XSSH_OK ||
		xsshPublicKeyReadEd25519(reply.tServerHostKey.pData, reply.tServerHostKey.iLen, &hostKey) != XSSH_OK ||
		xsshSignatureRead(reply.tSignature.pData, reply.tSignature.iLen, &hostSig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&hostKey, &hostSig, arrHash, sizeof(arrHash)) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_kex", "ecdh reply verify failed");
	}

	if ( xsshKexDeriveKeySHA256(arrC2SIV, sizeof(arrC2SIV), hashDesc.tSharedSecret, arrHash, arrHash, 'A') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CIV, sizeof(arrS2CIV), hashDesc.tSharedSecret, arrHash, arrHash, 'B') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrC2SKey, sizeof(arrC2SKey), hashDesc.tSharedSecret, arrHash, arrHash, 'C') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CKey, sizeof(arrS2CKey), hashDesc.tSharedSecret, arrHash, arrHash, 'D') != XSSH_OK ) {
		return xssh_mock_test_fail("mock_kex", "key derivation failed");
	}

	xsshPacketCodecInit(&clientCodec);
	xsshPacketCodecInit(&serverCodec);
	if ( xssh_mock_send_and_recv_newkeys(&clientCodec, &serverCodec) != XSSH_OK ||
		xsshPacketCodecSetWriteAESGCM(&clientCodec, arrC2SKey, sizeof(arrC2SKey), arrC2SIV) != XSSH_OK ||
		xsshPacketCodecSetReadAESGCM(&serverCodec, arrC2SKey, sizeof(arrC2SKey), arrC2SIV) != XSSH_OK ||
		xssh_mock_send_and_recv_newkeys(&serverCodec, &clientCodec) != XSSH_OK ||
		xsshPacketCodecSetWriteAESGCM(&serverCodec, arrS2CKey, sizeof(arrS2CKey), arrS2CIV) != XSSH_OK ||
		xsshPacketCodecSetReadAESGCM(&clientCodec, arrS2CKey, sizeof(arrS2CKey), arrS2CIV) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_newkeys", "newkeys switch failed");
	}

	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshMsgWriteServiceRequest(&wrPayload, "ssh-userauth") != XSSH_OK ||
		xssh_mock_send_packet(&clientCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&serverCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshMsgReadServiceRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &service) != XSSH_OK ||
		!xssh_mock_slice_eq(service.tServiceName, "ssh-userauth") ) {
		return xssh_mock_test_fail("mock_service", "service request failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshMsgWriteServiceAccept(&wrPayload, "ssh-userauth") != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshMsgReadServiceAccept(pkt.tPayload.pData, pkt.tPayload.iLen, &service) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_service", "service accept failed");
	}

	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshAuthWritePasswordRequest(&wrPayload, "alice", "secret") != XSSH_OK ||
		xssh_mock_send_packet(&clientCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&serverCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshAuthReadRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &authReq) != XSSH_OK ||
		!xssh_mock_slice_eq(authReq.tUserName, "alice") ||
		!xssh_mock_slice_eq(authReq.tPassword, "secret") ) {
		return xssh_mock_test_fail("mock_auth", "password request failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshAuthWriteSuccess(&wrPayload) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshAuthReadSuccess(pkt.tPayload.pData, pkt.tPayload.iLen) != XSSH_OK ) {
		return xssh_mock_test_fail("mock_auth", "auth success failed");
	}

	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelOpenSessionWrite(&wrPayload, 0u, 65536u, 32768u) != XSSH_OK ||
		xssh_mock_send_packet(&clientCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&serverCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelOpenRead(pkt.tPayload.pData, pkt.tPayload.iLen, &openMsg) != XSSH_OK ||
		openMsg.iSenderChannel != 0u ) {
		return xssh_mock_test_fail("mock_channel", "open failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelOpenConfirmationWrite(&wrPayload, 0u, 7u, 65536u, 32768u) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelOpenConfirmationRead(pkt.tPayload.pData, pkt.tPayload.iLen, &confirmMsg) != XSSH_OK ||
		confirmMsg.iSenderChannel != 7u ) {
		return xssh_mock_test_fail("mock_channel", "open confirmation failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelRequestExecWrite(&wrPayload, 7u, TRUE, "printf ok") != XSSH_OK ||
		xssh_mock_send_packet(&clientCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&serverCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &chanReq) != XSSH_OK ||
		!xssh_mock_slice_eq(chanReq.tRequestType, "exec") ) {
		return xssh_mock_test_fail("mock_channel", "exec request failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelSuccessWrite(&wrPayload, 0u) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelSuccessRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseChannel) != XSSH_OK ||
		iCloseChannel != 0u ) {
		return xssh_mock_test_fail("mock_channel", "exec success failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelDataWrite(&wrPayload, 0u, "ok\n", 3u) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelDataRead(pkt.tPayload.pData, pkt.tPayload.iLen, &dataMsg) != XSSH_OK ||
		!xssh_mock_slice_eq(dataMsg.tData, "ok\n") ) {
		return xssh_mock_test_fail("mock_channel", "stdout data failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelRequestExitStatusWrite(&wrPayload, 0u, 0u) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelRequestRead(pkt.tPayload.pData, pkt.tPayload.iLen, &chanReq) != XSSH_OK ||
		xsshChannelRequestExitStatusParse(&chanReq, &iExitStatus) != XSSH_OK ||
		iExitStatus != 0u ) {
		return xssh_mock_test_fail("mock_channel", "exit-status failed");
	}
	xsshWriterInit(&wrPayload, arrPayload, sizeof(arrPayload));
	if ( xsshChannelCloseWrite(&wrPayload, 0u) != XSSH_OK ||
		xssh_mock_send_packet(&serverCodec, arrWire, sizeof(arrWire), arrPayload, wrPayload.iLen, &iWireLen) != XSSH_OK ||
		xssh_mock_recv_packet(&clientCodec, arrWire, iWireLen, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshChannelCloseRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iCloseChannel) != XSSH_OK ||
		iCloseChannel != 0u ) {
		return xssh_mock_test_fail("mock_channel", "close failed");
	}

	xsshPacketCodecClear(&clientCodec);
	xsshPacketCodecClear(&serverCodec);
	xsshSecureZero(arrHostSeed, sizeof(arrHostSeed));
	xsshSecureZero(arrSig, sizeof(arrSig));
	xsshSecureZero(arrSecretClient, sizeof(arrSecretClient));
	xsshSecureZero(arrSecretServer, sizeof(arrSecretServer));
	printf("xssh_mock_server_test: PASS\n");
	return 0;
}

int main(void)
{
	return xssh_mock_test_handshake_auth_exec();
}
