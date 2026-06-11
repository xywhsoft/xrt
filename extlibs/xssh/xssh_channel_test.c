#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_channel_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_channel_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xssh_channel_test_open(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshchannelopenmsg openMsg;
	xsshchannelopenconfirmmsg confirmMsg;
	xsshchannelopenfailuremsg failMsg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenSessionWrite(&wr, 3u, 4096u, 32768u) != XSSH_OK ||
		xsshChannelOpenRead(arrBuf, wr.iLen, &openMsg) != XSSH_OK ||
		!xssh_channel_slice_eq(openMsg.tChannelType, "session") ||
		openMsg.iSenderChannel != 3u ||
		openMsg.iInitialWindow != 4096u ||
		openMsg.iMaxPacket != 32768u ||
		openMsg.tExtra.iLen != 0u ) {
		return xssh_channel_test_fail("channel_open", "session open mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenConfirmationWrite(&wr, 3u, 8u, 8192u, 16384u) != XSSH_OK ||
		xsshChannelOpenConfirmationRead(arrBuf, wr.iLen, &confirmMsg) != XSSH_OK ||
		confirmMsg.iRecipientChannel != 3u ||
		confirmMsg.iSenderChannel != 8u ||
		confirmMsg.iInitialWindow != 8192u ||
		confirmMsg.iMaxPacket != 16384u ) {
		return xssh_channel_test_fail("channel_open", "confirmation mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenFailureWrite(&wr, 3u, 2u, "denied", "en") != XSSH_OK ||
		xsshChannelOpenFailureRead(arrBuf, wr.iLen, &failMsg) != XSSH_OK ||
		failMsg.iRecipientChannel != 3u ||
		failMsg.iReason != 2u ||
		!xssh_channel_slice_eq(failMsg.tDescription, "denied") ||
		!xssh_channel_slice_eq(failMsg.tLanguageTag, "en") ) {
		return xssh_channel_test_fail("channel_open", "failure mismatch");
	}
	return 0;
}

static int xssh_channel_test_tcpip_open(void)
{
	uint8 arrBuf[256];
	xsshwriter wr;
	xsshchannelopenmsg openMsg;
	xsshchanneltcpipopenmsg tcpip;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenDirectTcpipWrite(&wr, 11u, 4096u, 32768u, "db.internal", 5432u, "127.0.0.1", 50000u) != XSSH_OK ||
		xsshChannelOpenRead(arrBuf, wr.iLen, &openMsg) != XSSH_OK ||
		!xssh_channel_slice_eq(openMsg.tChannelType, "direct-tcpip") ||
		openMsg.iSenderChannel != 11u ||
		xsshChannelOpenTcpipExtraRead(&openMsg, &tcpip) != XSSH_OK ||
		!xssh_channel_slice_eq(tcpip.tConnectedHost, "db.internal") ||
		tcpip.iConnectedPort != 5432u ||
		!xssh_channel_slice_eq(tcpip.tOriginatorHost, "127.0.0.1") ||
		tcpip.iOriginatorPort != 50000u ) {
		return xssh_channel_test_fail("channel_tcpip_open", "direct-tcpip mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenForwardedTcpipWrite(&wr, 12u, 4096u, 32768u, "0.0.0.0", 10022u, "192.0.2.8", 45000u) != XSSH_OK ||
		xsshChannelOpenRead(arrBuf, wr.iLen, &openMsg) != XSSH_OK ||
		!xssh_channel_slice_eq(openMsg.tChannelType, "forwarded-tcpip") ||
		xsshChannelOpenTcpipExtraRead(&openMsg, &tcpip) != XSSH_OK ||
		!xssh_channel_slice_eq(tcpip.tConnectedHost, "0.0.0.0") ||
		tcpip.iConnectedPort != 10022u ||
		!xssh_channel_slice_eq(tcpip.tOriginatorHost, "192.0.2.8") ||
		tcpip.iOriginatorPort != 45000u ) {
		return xssh_channel_test_fail("channel_tcpip_open", "forwarded-tcpip mismatch");
	}
	arrBuf[wr.iLen++] = 0u;
	if ( xsshChannelOpenRead(arrBuf, wr.iLen, &openMsg) != XSSH_OK ||
		xsshChannelOpenTcpipExtraRead(&openMsg, &tcpip) != XSSH_ERR_BAD_PACKET ) {
		return xssh_channel_test_fail("channel_tcpip_open", "trailing tcpip extra accepted");
	}
	return 0;
}

static int xssh_channel_test_data_and_window(void)
{
	uint8 arrData[] = { 'o', 'u', 't', 0u };
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshchanneldataeventmsg dataMsg;
	xsshchannelwindowadjustmsg winMsg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelDataWrite(&wr, 5u, arrData, sizeof(arrData)) != XSSH_OK ||
		xsshChannelDataRead(arrBuf, wr.iLen, &dataMsg) != XSSH_OK ||
		dataMsg.iRecipientChannel != 5u ||
		dataMsg.tData.iLen != sizeof(arrData) ||
		memcmp(dataMsg.tData.pData, arrData, sizeof(arrData)) != 0 ) {
		return xssh_channel_test_fail("channel_data", "stdout data mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelExtendedDataWrite(&wr, 5u, 1u, "err", 3u) != XSSH_OK ||
		xsshChannelExtendedDataRead(arrBuf, wr.iLen, &dataMsg) != XSSH_OK ||
		dataMsg.iRecipientChannel != 5u ||
		dataMsg.iDataType != 1u ||
		!xssh_channel_slice_eq(dataMsg.tData, "err") ) {
		return xssh_channel_test_fail("channel_data", "stderr data mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelWindowAdjustWrite(&wr, 5u, 1024u) != XSSH_OK ||
		xsshChannelWindowAdjustRead(arrBuf, wr.iLen, &winMsg) != XSSH_OK ||
		winMsg.iRecipientChannel != 5u ||
		winMsg.iBytes != 1024u ) {
		return xssh_channel_test_fail("channel_data", "window adjust mismatch");
	}
	return 0;
}

static int xssh_channel_test_simple_messages(void)
{
	uint8 arrBuf[32];
	xsshwriter wr;
	uint32 iChan = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelEofWrite(&wr, 9u) != XSSH_OK ||
		xsshChannelEofRead(arrBuf, wr.iLen, &iChan) != XSSH_OK ||
		iChan != 9u ) {
		return xssh_channel_test_fail("channel_simple", "eof mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelCloseWrite(&wr, 9u) != XSSH_OK ||
		xsshChannelCloseRead(arrBuf, wr.iLen, &iChan) != XSSH_OK ||
		iChan != 9u ) {
		return xssh_channel_test_fail("channel_simple", "close mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelSuccessWrite(&wr, 9u) != XSSH_OK ||
		xsshChannelSuccessRead(arrBuf, wr.iLen, &iChan) != XSSH_OK ||
		iChan != 9u ) {
		return xssh_channel_test_fail("channel_simple", "success mismatch");
	}
	return 0;
}

static int xssh_channel_test_shell_exec_requests(void)
{
	uint8 arrBuf[256];
	xsshwriter wr;
	xsshchannelrequestmsg req;
	xsshreader rd;
	xsshslice cmd;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestShellWrite(&wr, 4u, TRUE) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.iRecipientChannel != 4u ||
		!req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "shell") ||
		req.tPayload.iLen != 0u ) {
		return xssh_channel_test_fail("channel_request", "shell mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestExecWrite(&wr, 4u, TRUE, "echo ok") != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_channel_slice_eq(req.tRequestType, "exec") ) {
		return xssh_channel_test_fail("channel_request", "exec read mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &cmd) != XSSH_OK ||
		!xssh_channel_slice_eq(cmd, "echo ok") ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_request", "exec payload mismatch");
	}
	return 0;
}

static int xssh_channel_test_subsystem_env_signal_break_requests(void)
{
	uint8 arrBuf[256];
	xsshwriter wr;
	xsshchannelrequestmsg req;
	xsshreader rd;
	xsshslice a;
	xsshslice b;
	uint32 iBreakMs = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestSubsystemWrite(&wr, 4u, TRUE, "sftp") != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "subsystem") ) {
		return xssh_channel_test_fail("channel_subsystem", "request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &a) != XSSH_OK || !xssh_channel_slice_eq(a, "sftp") || xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_subsystem", "payload mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestEnvWrite(&wr, 4u, TRUE, "LANG", "zh_CN.UTF-8") != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_channel_slice_eq(req.tRequestType, "env") ) {
		return xssh_channel_test_fail("channel_env", "request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &a) != XSSH_OK ||
		xsshWireReadString(&rd, &b) != XSSH_OK ||
		!xssh_channel_slice_eq(a, "LANG") ||
		!xssh_channel_slice_eq(b, "zh_CN.UTF-8") ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_env", "payload mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestSignalWrite(&wr, 4u, FALSE, "TERM") != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "signal") ) {
		return xssh_channel_test_fail("channel_signal", "request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &a) != XSSH_OK || !xssh_channel_slice_eq(a, "TERM") || xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_signal", "payload mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestBreakWrite(&wr, 4u, FALSE, 500u) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "break") ) {
		return xssh_channel_test_fail("channel_break", "request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadU32(&rd, &iBreakMs) != XSSH_OK || iBreakMs != 500u || xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_break", "payload mismatch");
	}
	return 0;
}

static int xssh_channel_test_pty_and_resize_requests(void)
{
	uint8 arrBuf[256];
	xsshwriter wr;
	xsshchannelrequestmsg req;
	xsshreader rd;
	xsshpty pty;
	xsshslice term;
	xsshslice modes;
	uint32 iCols = 0u;
	uint32 iRows = 0u;
	uint32 iPixW = 0u;
	uint32 iPixH = 0u;

	xsshPtyInit(&pty);
	pty.iCols = 100u;
	pty.iRows = 40u;
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestPtyWrite(&wr, 4u, TRUE, &pty) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_channel_slice_eq(req.tRequestType, "pty-req") ||
		!req.bWantReply ) {
		return xssh_channel_test_fail("channel_pty", "pty request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &term) != XSSH_OK ||
		xsshWireReadU32(&rd, &iCols) != XSSH_OK ||
		xsshWireReadU32(&rd, &iRows) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixW) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixH) != XSSH_OK ||
		xsshWireReadString(&rd, &modes) != XSSH_OK ||
		!xssh_channel_slice_eq(term, "xterm-256color") ||
		iCols != 100u ||
		iRows != 40u ||
		modes.iLen != 0u ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_pty", "pty payload mismatch");
	}

	if ( xsshPtyModeSet(&pty, XSSH_TTY_OP_ECHO, 0u) != XSSH_OK ||
		xsshPtyModeSet(&pty, XSSH_TTY_OP_ISPEED, 115200u) != XSSH_OK ||
		xsshPtyModeSet(&pty, XSSH_TTY_OP_ECHO, 1u) != XSSH_OK ||
		xsshPtyModeSet(&pty, XSSH_TTY_OP_END, 0u) != XSSH_ERR_INVALID ) {
		return xssh_channel_test_fail("channel_pty", "pty mode set failed");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestPtyWrite(&wr, 4u, TRUE, &pty) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ) {
		return xssh_channel_test_fail("channel_pty", "pty modes request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &term) != XSSH_OK ||
		xsshWireReadU32(&rd, &iCols) != XSSH_OK ||
		xsshWireReadU32(&rd, &iRows) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixW) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixH) != XSSH_OK ||
		xsshWireReadString(&rd, &modes) != XSSH_OK ||
		modes.iLen != 11u ||
		modes.pData[0] != XSSH_TTY_OP_ECHO ||
		modes.pData[1] != 0u || modes.pData[2] != 0u || modes.pData[3] != 0u || modes.pData[4] != 1u ||
		modes.pData[5] != XSSH_TTY_OP_ISPEED ||
		modes.pData[6] != 0u || modes.pData[7] != 1u || modes.pData[8] != 0xc2u || modes.pData[9] != 0x00u ||
		modes.pData[10] != XSSH_TTY_OP_END ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_pty", "pty modes payload mismatch");
	}
	xsshPtyModeClear(&pty);
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestPtyWrite(&wr, 4u, TRUE, &pty) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ) {
		return xssh_channel_test_fail("channel_pty", "pty clear request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadString(&rd, &term) != XSSH_OK ||
		xsshWireReadU32(&rd, &iCols) != XSSH_OK ||
		xsshWireReadU32(&rd, &iRows) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixW) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixH) != XSSH_OK ||
		xsshWireReadString(&rd, &modes) != XSSH_OK ||
		modes.iLen != 0u ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_pty", "pty modes clear mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestWindowChangeWrite(&wr, 4u, 120u, 50u, 800u, 600u) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_channel_slice_eq(req.tRequestType, "window-change") ||
		req.bWantReply ) {
		return xssh_channel_test_fail("channel_pty", "window-change request mismatch");
	}
	xsshReaderInit(&rd, req.tPayload.pData, req.tPayload.iLen);
	if ( xsshWireReadU32(&rd, &iCols) != XSSH_OK ||
		xsshWireReadU32(&rd, &iRows) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixW) != XSSH_OK ||
		xsshWireReadU32(&rd, &iPixH) != XSSH_OK ||
		iCols != 120u ||
		iRows != 50u ||
		iPixW != 800u ||
		iPixH != 600u ||
		xsshReaderLeft(&rd) != 0u ) {
		return xssh_channel_test_fail("channel_pty", "window-change payload mismatch");
	}
	return 0;
}

static int xssh_channel_test_exit_requests(void)
{
	uint8 arrBuf[256];
	xsshwriter wr;
	xsshchannelrequestmsg req;
	xsshchannelexitsignalmsg sig;
	uint32 iStatus = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestExitStatusWrite(&wr, 4u, 127u) != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "exit-status") ||
		xsshChannelRequestExitStatusParse(&req, &iStatus) != XSSH_OK ||
		iStatus != 127u ) {
		return xssh_channel_test_fail("channel_exit_status", "status mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelRequestExitSignalWrite(&wr, 4u, "TERM", TRUE, "terminated", "en") != XSSH_OK ||
		xsshChannelRequestRead(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bWantReply ||
		!xssh_channel_slice_eq(req.tRequestType, "exit-signal") ||
		xsshChannelRequestExitSignalParse(&req, &sig) != XSSH_OK ||
		!xssh_channel_slice_eq(sig.tSignalName, "TERM") ||
		!sig.bCoreDumped ||
		!xssh_channel_slice_eq(sig.tErrorMessage, "terminated") ||
		!xssh_channel_slice_eq(sig.tLanguageTag, "en") ) {
		return xssh_channel_test_fail("channel_exit_signal", "signal mismatch");
	}
	if ( xsshChannelRequestExitStatusParse(&req, &iStatus) != XSSH_ERR_INVALID ) {
		return xssh_channel_test_fail("channel_exit_signal", "wrong parser accepted signal request");
	}
	return 0;
}

static int xssh_channel_test_errors(void)
{
	uint8 arrBuf[8];
	xsshwriter wr;
	xsshchannelopenmsg openMsg;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshChannelOpenSessionWrite(&wr, 1u, 2u, 3u) != XSSH_ERR_NO_SPACE || wr.iLen != 0u ) {
		return xssh_channel_test_fail("channel_errors", "small open writer changed state");
	}
	arrBuf[0] = XSSH_MSG_CHANNEL_DATA;
	if ( xsshChannelOpenRead(arrBuf, 1u, &openMsg) != XSSH_ERR_BAD_PACKET ) {
		return xssh_channel_test_fail("channel_errors", "bad open id accepted");
	}
	return 0;
}

static int xssh_channel_test_read_buffer_reuse(void)
{
	uint8 arrFirst[12000];
	uint8 arrSecond[10000];
	uint8 arrOut[12000];
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	size_t iWritten = 0u;
	size_t iRead = 0u;
	size_t i;

	memset(arrFirst, 'a', sizeof(arrFirst));
	memset(arrSecond, 'b', sizeof(arrSecond));
	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = "mock";
	cfg.sUser = "tester";
	auth.sPassword = "secret";
	if ( xsshConnect(&cfg, &auth, &pSession) != XSSH_OK || pSession == NULL ) {
		return xssh_channel_test_fail("channel_buffer_reuse", "mock connect failed");
	}
	if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ) {
		xsshFree(pSession);
		return xssh_channel_test_fail("channel_buffer_reuse", "open channel failed");
	}
	for ( i = 0u; i < sizeof(arrFirst); i += 1000u ) {
		if ( xsshChannelWrite(pChannel, arrFirst + i, 1000u, &iWritten) != XSSH_OK || iWritten != 1000u ) {
			xsshFree(pSession);
			return xssh_channel_test_fail("channel_buffer_reuse", "first chunk write failed");
		}
	}
	if ( xsshChannelRead(pChannel, arrOut, sizeof(arrOut), &iRead) != XSSH_OK || iRead != sizeof(arrFirst) ) {
		xsshFree(pSession);
		return xssh_channel_test_fail("channel_buffer_reuse", "first write/read failed");
	}
	for ( i = 0u; i < iRead; ++i ) {
		if ( arrOut[i] != 'a' ) {
			xsshFree(pSession);
			return xssh_channel_test_fail("channel_buffer_reuse", "first payload mismatch");
		}
	}
	for ( i = 0u; i < sizeof(arrSecond); i += 1000u ) {
		if ( xsshChannelWrite(pChannel, arrSecond + i, 1000u, &iWritten) != XSSH_OK || iWritten != 1000u ) {
			xsshFree(pSession);
			return xssh_channel_test_fail("channel_buffer_reuse", "second chunk write failed");
		}
	}
	if ( xsshChannelRead(pChannel, arrOut, sizeof(arrOut), &iRead) != XSSH_OK || iRead != sizeof(arrSecond) ) {
		xsshFree(pSession);
		return xssh_channel_test_fail("channel_buffer_reuse", "second write/read failed");
	}
	for ( i = 0u; i < iRead; ++i ) {
		if ( arrOut[i] != 'b' ) {
			xsshFree(pSession);
			return xssh_channel_test_fail("channel_buffer_reuse", "second payload mismatch");
		}
	}
	xsshFree(pSession);
	return 0;
}

int main(void)
{
	if ( xssh_channel_test_open() ) return 1;
	if ( xssh_channel_test_tcpip_open() ) return 1;
	if ( xssh_channel_test_data_and_window() ) return 1;
	if ( xssh_channel_test_simple_messages() ) return 1;
	if ( xssh_channel_test_shell_exec_requests() ) return 1;
	if ( xssh_channel_test_subsystem_env_signal_break_requests() ) return 1;
	if ( xssh_channel_test_pty_and_resize_requests() ) return 1;
	if ( xssh_channel_test_exit_requests() ) return 1;
	if ( xssh_channel_test_errors() ) return 1;
	if ( xssh_channel_test_read_buffer_reuse() ) return 1;

	printf("xssh_channel_test: PASS\n");
	return 0;
}
