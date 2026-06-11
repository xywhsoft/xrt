#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_transport_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_transport_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xssh_transport_test_newkeys(void)
{
	uint8 arrBuf[8];
	xsshwriter wr;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteNewKeys(&wr) != XSSH_OK || wr.iLen != 1u || arrBuf[0] != XSSH_MSG_NEWKEYS ) {
		return xssh_transport_test_fail("newkeys", "write mismatch");
	}
	if ( xsshMsgReadNewKeys(arrBuf, wr.iLen) != XSSH_OK ) {
		return xssh_transport_test_fail("newkeys", "read failed");
	}
	arrBuf[0] = XSSH_MSG_IGNORE;
	if ( xsshMsgReadNewKeys(arrBuf, wr.iLen) != XSSH_ERR_BAD_PACKET ) {
		return xssh_transport_test_fail("newkeys", "bad id accepted");
	}
	return 0;
}

static int xssh_transport_test_service(void)
{
	uint8 arrBuf[64];
	xsshwriter wr;
	xsshservicemsg msg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteServiceRequest(&wr, "ssh-userauth") != XSSH_OK ||
		xsshMsgReadServiceRequest(arrBuf, wr.iLen, &msg) != XSSH_OK ||
		!xssh_transport_slice_eq(msg.tServiceName, "ssh-userauth") ) {
		return xssh_transport_test_fail("service", "request mismatch");
	}
	if ( xsshMsgReadServiceAccept(arrBuf, wr.iLen, &msg) != XSSH_ERR_BAD_PACKET ) {
		return xssh_transport_test_fail("service", "request accepted as accept");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteServiceAccept(&wr, "ssh-connection") != XSSH_OK ||
		xsshMsgReadServiceAccept(arrBuf, wr.iLen, &msg) != XSSH_OK ||
		!xssh_transport_slice_eq(msg.tServiceName, "ssh-connection") ) {
		return xssh_transport_test_fail("service", "accept mismatch");
	}
	return 0;
}

static int xssh_transport_test_global_tcpip_forward(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshglobalrequestmsg req;
	xsshglobaltcpipforwardmsg fwd;
	xsshglobalrequestsuccessmsg ok;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteTcpipForward(&wr, "0.0.0.0", 0u, TRUE) != XSSH_OK ||
		xsshMsgReadGlobalRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!req.bWantReply ||
		!xssh_transport_slice_eq(req.tRequestName, "tcpip-forward") ||
		xsshMsgReadTcpipForwardPayload(&req, &fwd) != XSSH_OK ||
		!xssh_transport_slice_eq(fwd.tBindAddress, "0.0.0.0") ||
		fwd.iBindPort != 0u ) {
		return xssh_transport_test_fail("global_tcpip_forward", "tcpip-forward mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteCancelTcpipForward(&wr, "127.0.0.1", 8022u, TRUE) != XSSH_OK ||
		xsshMsgReadGlobalRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_transport_slice_eq(req.tRequestName, "cancel-tcpip-forward") ||
		xsshMsgReadTcpipForwardPayload(&req, &fwd) != XSSH_OK ||
		!xssh_transport_slice_eq(fwd.tBindAddress, "127.0.0.1") ||
		fwd.iBindPort != 8022u ) {
		return xssh_transport_test_fail("global_tcpip_forward", "cancel mismatch");
	}
	arrBuf[wr.iLen++] = 0u;
	if ( xsshMsgReadGlobalRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		xsshMsgReadTcpipForwardPayload(&req, &fwd) != XSSH_ERR_BAD_PACKET ) {
		return xssh_transport_test_fail("global_tcpip_forward", "trailing payload accepted");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteRequestSuccess(&wr, TRUE, 10022u) != XSSH_OK ||
		xsshMsgReadRequestSuccess(arrBuf, wr.iLen, &ok) != XSSH_OK ||
		!ok.bHasBoundPort ||
		ok.iBoundPort != 10022u ) {
		return xssh_transport_test_fail("global_tcpip_forward", "success with port mismatch");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteRequestSuccess(&wr, FALSE, 0u) != XSSH_OK ||
		xsshMsgReadRequestSuccess(arrBuf, wr.iLen, &ok) != XSSH_OK ||
		ok.bHasBoundPort ) {
		return xssh_transport_test_fail("global_tcpip_forward", "success without port mismatch");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteRequestFailure(&wr) != XSSH_OK ||
		xsshMsgReadRequestFailure(arrBuf, wr.iLen) != XSSH_OK ) {
		return xssh_transport_test_fail("global_tcpip_forward", "failure mismatch");
	}
	return 0;
}

static int xssh_transport_test_disconnect(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshdisconnectmsg msg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteDisconnect(&wr, 11u, "bye", "en") != XSSH_OK ||
		xsshMsgReadDisconnect(arrBuf, wr.iLen, &msg) != XSSH_OK ||
		msg.iReason != 11u ||
		!xssh_transport_slice_eq(msg.tDescription, "bye") ||
		!xssh_transport_slice_eq(msg.tLanguageTag, "en") ) {
		return xssh_transport_test_fail("disconnect", "roundtrip mismatch");
	}
	arrBuf[wr.iLen++] = 0u;
	if ( xsshMsgReadDisconnect(arrBuf, wr.iLen, &msg) != XSSH_ERR_BAD_PACKET ) {
		return xssh_transport_test_fail("disconnect", "trailing bytes accepted");
	}
	return 0;
}

static int xssh_transport_test_ignore(void)
{
	uint8 arrPayload[] = { 1u, 2u, 3u, 0u, 4u };
	uint8 arrBuf[64];
	xsshwriter wr;
	xsshignoremsg msg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteIgnore(&wr, arrPayload, sizeof(arrPayload)) != XSSH_OK ||
		xsshMsgReadIgnore(arrBuf, wr.iLen, &msg) != XSSH_OK ||
		msg.tData.iLen != sizeof(arrPayload) ||
		memcmp(msg.tData.pData, arrPayload, sizeof(arrPayload)) != 0 ) {
		return xssh_transport_test_fail("ignore", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_transport_test_debug(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshdebugmsg msg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteDebug(&wr, TRUE, "trace", "zh-CN") != XSSH_OK ||
		xsshMsgReadDebug(arrBuf, wr.iLen, &msg) != XSSH_OK ||
		!msg.bAlwaysDisplay ||
		!xssh_transport_slice_eq(msg.tMessage, "trace") ||
		!xssh_transport_slice_eq(msg.tLanguageTag, "zh-CN") ) {
		return xssh_transport_test_fail("debug", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_transport_test_padding(void* pUser, uint8* pOut, size_t iLen)
{
	uint8 iBase = pUser ? *(uint8*)pUser : 0u;
	size_t i;

	for ( i = 0u; i < iLen; ++i ) {
		pOut[i] = (uint8)(iBase + i);
	}
	return XSSH_OK;
}

static int xssh_transport_test_packet_codec_newkeys_switch(void)
{
	const uint8 arrPlainPayload[] = { XSSH_MSG_IGNORE, 'p', 'l', 'a', 'i', 'n' };
	const uint8 arrEncryptedPayload[] = { XSSH_MSG_IGNORE, 'a', 'e', 'a', 'd' };
	uint8 arrKey[XSSH_AES128_KEY_SIZE];
	uint8 arrIV[XSSH_GCM_NONCE_SIZE] = { 0x51u, 0x52u, 0x53u, 0x54u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u };
	uint8 arrBuf[256];
	uint8 arrPlain[128];
	uint8 iPadBase = 0x90u;
	xsshpacketcodec clientCodec;
	xsshpacketcodec serverCodec;
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) arrKey[i] = (uint8)(0x60u + i);
	xsshPacketCodecInit(&clientCodec);
	xsshPacketCodecInit(&serverCodec);

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketCodecWrite(&clientCodec, &wr, arrPlainPayload, sizeof(arrPlainPayload)) != XSSH_OK ) {
		return xssh_transport_test_fail("packet_codec_newkeys", "plain write failed");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketCodecRead(&serverCodec, &rd, 0u, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		pkt.iSeqNo != 0u ||
		pkt.tPayload.iLen != sizeof(arrPlainPayload) ||
		memcmp(pkt.tPayload.pData, arrPlainPayload, sizeof(arrPlainPayload)) != 0 ||
		clientCodec.iWriteSeqNo != 1u ||
		serverCodec.iReadSeqNo != 1u ) {
		return xssh_transport_test_fail("packet_codec_newkeys", "plain read mismatch");
	}

	if ( xsshPacketCodecSetWriteAESGCM(&clientCodec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ||
		xsshPacketCodecSetReadAESGCM(&serverCodec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ) {
		return xssh_transport_test_fail("packet_codec_newkeys", "set aesgcm failed");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketCodecWriteEx(&clientCodec, &wr, arrEncryptedPayload, sizeof(arrEncryptedPayload), xssh_transport_test_padding, &iPadBase) != XSSH_OK ) {
		return xssh_transport_test_fail("packet_codec_newkeys", "encrypted write failed");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	memset(arrPlain, 0, sizeof(arrPlain));
	if ( xsshPacketCodecRead(&serverCodec, &rd, 0u, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		pkt.iSeqNo != 1u ||
		pkt.tPayload.iLen != sizeof(arrEncryptedPayload) ||
		memcmp(pkt.tPayload.pData, arrEncryptedPayload, sizeof(arrEncryptedPayload)) != 0 ||
		clientCodec.iWriteSeqNo != 2u ||
		serverCodec.iReadSeqNo != 2u ||
		clientCodec.tWriteAESGCM.iInvocationCounter != 2u ||
		serverCodec.tReadAESGCM.iInvocationCounter != 2u ) {
		return xssh_transport_test_fail("packet_codec_newkeys", "encrypted read mismatch");
	}

	xsshPacketCodecClear(&clientCodec);
	xsshPacketCodecClear(&serverCodec);
	return 0;
}

static int xssh_transport_test_no_space_is_atomic(void)
{
	uint8 arrBuf[4];
	xsshwriter wr;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshMsgWriteDisconnect(&wr, 1u, "too long", "en") != XSSH_ERR_NO_SPACE || wr.iLen != 0u ) {
		return xssh_transport_test_fail("no_space", "disconnect changed writer on failure");
	}
	xsshWriterInit(&wr, arrBuf, 0u);
	if ( xsshMsgWriteNewKeys(&wr) != XSSH_ERR_NO_SPACE ) {
		return xssh_transport_test_fail("no_space", "zero-cap writer accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_transport_test_newkeys() ) return 1;
	if ( xssh_transport_test_service() ) return 1;
	if ( xssh_transport_test_global_tcpip_forward() ) return 1;
	if ( xssh_transport_test_disconnect() ) return 1;
	if ( xssh_transport_test_ignore() ) return 1;
	if ( xssh_transport_test_debug() ) return 1;
	if ( xssh_transport_test_packet_codec_newkeys_switch() ) return 1;
	if ( xssh_transport_test_no_space_is_atomic() ) return 1;

	printf("xssh_transport_test: PASS\n");
	return 0;
}
