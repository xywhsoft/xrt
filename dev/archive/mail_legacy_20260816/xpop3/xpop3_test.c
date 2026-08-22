#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xpop3.h"

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bSawAuthPlain;
} xpop3_mock_server;

static int xpop3_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static void xpop3_test_link_api(void)
{
	(void)&xrtPop3ConfigInit;
	(void)&xrtPop3ResultInit;
	(void)&xrtPop3AsyncOptsInit;
	(void)&xrtPop3ResultFree;
	(void)&xrtPop3FetchRawResultFree;
	(void)&xrtPop3FetchMimeResultFree;
	(void)&xrtPop3SummaryResultFree;
	(void)&xrtPop3Open;
	(void)&xrtPop3Close;
	(void)&xrtPop3Stat;
	(void)&xrtPop3CapaRaw;
	(void)&xrtPop3Capa;
	(void)&xrtPop3ListRaw;
	(void)&xrtPop3List;
	(void)&xrtPop3ListFree;
	(void)&xrtPop3UidlRaw;
	(void)&xrtPop3Uidl;
	(void)&xrtPop3UidlFree;
	(void)&xrtPop3ListSummaries;
	(void)&xrtPop3SummariesFree;
	(void)&xrtPop3RetrRaw;
	(void)&xrtPop3RetrMime;
	(void)&xrtPop3TopRaw;
	(void)&xrtPop3TopMime;
	(void)&xrtPop3Delete;
	(void)&xrtPop3Noop;
	(void)&xrtPop3Rset;
	(void)&xrtPop3Quit;
	(void)&xrtPop3FetchRawFuture;
	(void)&xrtPop3FetchMimeFuture;
	(void)&xrtPop3TopRawFuture;
	(void)&xrtPop3TopMimeFuture;
	(void)&xrtPop3ListSummariesFuture;
	(void)&xrtPop3FetchRawAsyncWait;
	(void)&xrtPop3FetchMimeAsyncWait;
	(void)&xrtPop3TopRawAsyncWait;
	(void)&xrtPop3TopMimeAsyncWait;
	(void)&xrtPop3ListSummariesAsyncWait;
}

static bool xpop3_mock_send_all(xsocket hSock, const char* sText)
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

static bool xpop3_mock_recv_line(xsocket hSock, char* sOut, size_t iOutCap)
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

static uint32 xpop3_mock_server_thread(ptr pArg)
{
	xpop3_mock_server* pServer = (xpop3_mock_server*)pArg;
	xsocket hClient;
	char sLine[512];

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !xpop3_mock_send_all(hClient, "+OK mock ready\r\n") ) {
		XPOP3_CLOSESOCK(hClient);
		return 2u;
	}
	while ( xpop3_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		if ( strcmp(sLine, "AUTH PLAIN AHVzZXIAcGFzcw==") == 0 ) {
			pServer->bSawAuthPlain = TRUE;
			xpop3_mock_send_all(hClient, "+OK auth plain\r\n");
		} else if ( strncmp(sLine, "USER ", 5) == 0 ) {
			xpop3_mock_send_all(hClient, "+OK user\r\n");
		} else if ( strncmp(sLine, "PASS ", 5) == 0 ) {
			xpop3_mock_send_all(hClient, "+OK pass\r\n");
		} else if ( strcmp(sLine, "CAPA") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK capabilities\r\nUSER\r\nUIDL\r\nTOP\r\nSTLS\r\nSASL PLAIN\r\n.\r\n");
		} else if ( strcmp(sLine, "STAT") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK 2 360\r\n");
		} else if ( strcmp(sLine, "LIST") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK list\r\n1 120\r\n2 240\r\n.\r\n");
		} else if ( strcmp(sLine, "UIDL") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK uidl\r\n1 abc\r\n2 def-123\r\n.\r\n");
		} else if ( strcmp(sLine, "RETR 1") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK message\r\nSubject: hi\r\nFrom: sender@example.com\r\n\r\n..dot\r\nbody\r\n.\r\n");
		} else if ( strcmp(sLine, "TOP 1 0") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK top\r\nSubject: hi\r\nFrom: sender@example.com\r\nDate: Tue, 12 May 2026 10:00:00 +0800\r\n\r\n.\r\n");
		} else if ( strcmp(sLine, "TOP 1 1") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK top\r\nSubject: hi\r\nFrom: sender@example.com\r\n\r\n..dot\r\n.\r\n");
		} else if ( strcmp(sLine, "TOP 2 0") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK top\r\nSubject: second\r\nFrom: two@example.com\r\nDate: Tue, 12 May 2026 10:01:00 +0800\r\n\r\n.\r\n");
		} else if ( strcmp(sLine, "DELE 1") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK deleted\r\n");
		} else if ( strcmp(sLine, "NOOP") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK noop\r\n");
		} else if ( strcmp(sLine, "RSET") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK reset\r\n");
		} else if ( strcmp(sLine, "QUIT") == 0 ) {
			xpop3_mock_send_all(hClient, "+OK bye\r\n");
			break;
		} else {
			xpop3_mock_send_all(hClient, "-ERR unknown\r\n");
		}
	}
	XPOP3_CLOSESOCK(hClient);
	return 0u;
}

static bool xpop3_mock_server_start(xpop3_mock_server* pServer)
{
	struct sockaddr_in tAddr;
	socklen_t iAddrLen;
	int iOpt = 1;

	(void)xrtInit();
	memset(pServer, 0, sizeof(*pServer));
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
		XPOP3_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	if ( listen(pServer->hListen, 1) != 0 ) {
		XPOP3_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		XPOP3_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xpop3_mock_server_stop(xpop3_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		XPOP3_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xpop3_test_response_parse(void)
{
	xpop3stat tStat;

	if ( !xpop3_response_is_ok("+OK ready") ) {
		return xpop3_test_fail("response_ok", "+OK not detected");
	}
	if ( !xpop3_response_is_err("-ERR bad command") ) {
		return xpop3_test_fail("response_err", "-ERR not detected");
	}
	if ( strcmp(xpop3_response_text("+OK mailbox locked"), "mailbox locked") != 0 ) {
		return xpop3_test_fail("response_text", "unexpected text");
	}
	if ( !xpop3_parse_stat_line("+OK 2 320", &tStat) ) {
		return xpop3_test_fail("stat_parse", "parse failed");
	}
	if ( tStat.iCount != 2u || tStat.iSize != 320u ) {
		return xpop3_test_fail("stat_values", "unexpected values");
	}
	printf("PASS response_parse\n");
	return 0;
}

static int xpop3_test_dot_unstuff(void)
{
	str sOut;
	int iFailed = 0;

	sOut = xpop3_unstuff_multiline("Subject: hi\r\n..dot\r\nbody\r\n.\r\nignored\r\n");
	if ( sOut == NULL ) {
		return xpop3_test_fail("dot_unstuff_alloc", "NULL output");
	}
	if ( strcmp((const char*)sOut, "Subject: hi\r\n.dot\r\nbody\r\n") != 0 ) {
		iFailed = xpop3_test_fail("dot_unstuff_value", (const char*)sOut);
	} else {
		printf("PASS dot_unstuff\n");
	}
	xrtFree(sOut);
	return iFailed;
}

static int xpop3_test_list_uidl_parse(void)
{
	xpop3listitem* arrList = NULL;
	xpop3uidlitem* arrUidl = NULL;
	size_t iListCount = 0;
	size_t iUidlCount = 0;
	int iFailed = 0;

	if ( !xpop3_parse_list_block("1 120\r\n2 240\r\n", &arrList, &iListCount) ) {
		return xpop3_test_fail("list_block_parse", "parse failed");
	}
	if ( iListCount != 2u || arrList[0].iNumber != 1u || arrList[0].iSize != 120u || arrList[1].iNumber != 2u || arrList[1].iSize != 240u ) {
		iFailed = xpop3_test_fail("list_block_values", "unexpected values");
	}
	xrtPop3ListFree(arrList);
	if ( iFailed ) {
		return iFailed;
	}

	if ( !xpop3_parse_uidl_block("1 abc\r\n2 def-123\r\n", &arrUidl, &iUidlCount) ) {
		return xpop3_test_fail("uidl_block_parse", "parse failed");
	}
	if ( iUidlCount != 2u || arrUidl[0].iNumber != 1u || strcmp(arrUidl[0].sUid, "abc") != 0 || arrUidl[1].iNumber != 2u || strcmp(arrUidl[1].sUid, "def-123") != 0 ) {
		iFailed = xpop3_test_fail("uidl_block_values", "unexpected values");
	}
	xrtPop3UidlFree(arrUidl);
	if ( iFailed ) {
		return iFailed;
	}
	printf("PASS list_uidl_parse\n");
	return 0;
}

static int xpop3_test_capa_parse(void)
{
	uint32 iCaps = xpop3_parse_capa_block("USER\r\nUIDL\r\nTOP\r\nSTLS\r\nSASL PLAIN\r\n");

	if ( !(iCaps & XPOP3_CAP_USER) || !(iCaps & XPOP3_CAP_UIDL) || !(iCaps & XPOP3_CAP_TOP)
		|| !(iCaps & XPOP3_CAP_STLS) || !(iCaps & XPOP3_CAP_SASL) ) {
		return xpop3_test_fail("capa_parse", "missing capability flag");
	}
	printf("PASS capa_parse\n");
	return 0;
}

static int xpop3_test_init_defaults(void)
{
	xpop3config tCfg;
	xpop3result tRet;

	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	if ( tCfg.iPort != 995 || tCfg.iTimeoutMs != 15000u || tCfg.iSecureMode != XPOP3_SECURE_AUTO || !tCfg.bVerifyPeer ) {
		return xpop3_test_fail("config_defaults", "unexpected defaults");
	}
	xpop3_result_stage(&tRet, XPOP3_STAGE_RETR);
	xpop3_result_error(&tRet, "sample");
	if ( tRet.iStage != XPOP3_STAGE_RETR || tRet.bSuccess || strcmp(tRet.sError, "sample") != 0 ) {
		return xpop3_test_fail("result_stage_error", "unexpected result state");
	}
	printf("PASS init_defaults\n");
	return 0;
}

static int xpop3_test_mock_server_flow(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	xpop3stat tStat;
	uint32 iCaps = 0u;
	xpop3listitem* arrList = NULL;
	xpop3uidlitem* arrUidl = NULL;
	xpop3summary* arrSummary = NULL;
	size_t iListCount = 0u;
	size_t iUidlCount = 0u;
	size_t iSummaryCount = 0u;
	str sMsg = NULL;
	xmailparsedmessage tParsed;
	int iFailed = 0;

	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("mock_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("mock_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	xrtPop3ResultInit(&tRet);

	if ( !xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		iFailed = xpop3_test_fail("mock_open", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Capa(pClient, &iCaps, &tRet) || !(iCaps & XPOP3_CAP_USER) || !(iCaps & XPOP3_CAP_UIDL) || !(iCaps & XPOP3_CAP_STLS) ) {
		iFailed = xpop3_test_fail("mock_capa", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Stat(pClient, &tStat, &tRet) || tStat.iCount != 2u || tStat.iSize != 360u ) {
		iFailed = xpop3_test_fail("mock_stat", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3List(pClient, &arrList, &iListCount, &tRet) || iListCount != 2u || arrList[1].iNumber != 2u || arrList[1].iSize != 240u ) {
		iFailed = xpop3_test_fail("mock_list", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Uidl(pClient, &arrUidl, &iUidlCount, &tRet) || iUidlCount != 2u || strcmp(arrUidl[1].sUid, "def-123") != 0 ) {
		iFailed = xpop3_test_fail("mock_uidl", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3ListSummaries(pClient, &arrSummary, &iSummaryCount, &tRet)
		|| iSummaryCount != 2u
		|| arrSummary[0].iNumber != 1u || arrSummary[0].iSize != 120u || strcmp(arrSummary[0].sUid, "abc") != 0
		|| strcmp(arrSummary[0].sSubject, "hi") != 0 || strcmp(arrSummary[0].sFrom, "sender@example.com") != 0
		|| arrSummary[1].iNumber != 2u || strcmp(arrSummary[1].sUid, "def-123") != 0
		|| strcmp(arrSummary[1].sSubject, "second") != 0 || strcmp(arrSummary[1].sFrom, "two@example.com") != 0 ) {
		iFailed = xpop3_test_fail("mock_summaries", tRet.sError);
		goto cleanup;
	}
	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xrtPop3RetrRaw(pClient, 1u, &sMsg, &tRet) || strcmp((const char*)sMsg, "Subject: hi\r\nFrom: sender@example.com\r\n\r\n.dot\r\nbody\r\n") != 0 ) {
		iFailed = xpop3_test_fail("mock_retr", sMsg ? (const char*)sMsg : tRet.sError);
		goto cleanup;
	}
	xrtFree(sMsg);
	sMsg = NULL;
	if ( !xrtPop3RetrMime(pClient, 1u, &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| strcmp((const char*)tParsed.sBody, ".dot\r\nbody\r\n") != 0 ) {
		iFailed = xpop3_test_fail("mock_retr_mime", tRet.sError);
		goto cleanup;
	}
	xmailMimeParsedMessageFree(&tParsed);
	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xrtPop3TopRaw(pClient, 1u, 1u, &sMsg, &tRet) || strcmp((const char*)sMsg, "Subject: hi\r\nFrom: sender@example.com\r\n\r\n.dot\r\n") != 0 ) {
		iFailed = xpop3_test_fail("mock_top", sMsg ? (const char*)sMsg : tRet.sError);
		goto cleanup;
	}
	xrtFree(sMsg);
	sMsg = NULL;
	if ( !xrtPop3TopMime(pClient, 1u, 0u, &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| tParsed.sBody == NULL || strcmp((const char*)tParsed.sBody, "") != 0 ) {
		iFailed = xpop3_test_fail("mock_top_mime", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Delete(pClient, 1u, &tRet) || !xrtPop3Noop(pClient, &tRet) || !xrtPop3Rset(pClient, &tRet) ) {
		iFailed = xpop3_test_fail("mock_simple_cmd", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Quit(pClient, &tRet) ) {
		pClient = NULL;
		iFailed = xpop3_test_fail("mock_quit", tRet.sError);
		goto cleanup;
	}
	pClient = NULL;
	printf("PASS mock_server_flow\n");

cleanup:
	if ( sMsg != NULL ) {
		xrtFree(sMsg);
	}
	xrtPop3ListFree(arrList);
	xrtPop3UidlFree(arrUidl);
	xrtPop3SummariesFree(arrSummary);
	xmailMimeParsedMessageFree(&tParsed);
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	xpop3_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int xpop3_test_auth_plain_flow(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	int iFailed = 0;

	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("auth_plain_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("auth_plain_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tCfg.iAuthMode = XPOP3_AUTH_PLAIN;
	if ( !xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		iFailed = xpop3_test_fail("auth_plain_open", tRet.sError);
		goto cleanup;
	}
	if ( !xrtPop3Quit(pClient, &tRet) ) {
		iFailed = xpop3_test_fail("auth_plain_quit", tRet.sError);
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawAuthPlain ) {
		iFailed = xpop3_test_fail("auth_plain_command", "AUTH PLAIN missing");
		goto cleanup;
	}
	printf("PASS auth_plain_flow\n");

cleanup:
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	xpop3_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xpop3_test_async_fetch_raw(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3asyncopts tOpts;
	xpop3result tRet;
	str sRaw = NULL;
	int iFailed = 0;

	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("async_raw_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("async_raw_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	xrtPop3AsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "xpop3-test-raw";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtPop3FetchRawAsyncWait(&tCfg, 1u, &tOpts, &sRaw, &tRet)
		|| sRaw == NULL
		|| strcmp((const char*)sRaw, "Subject: hi\r\nFrom: sender@example.com\r\n\r\n.dot\r\nbody\r\n") != 0 ) {
		iFailed = xpop3_test_fail("async_fetch_raw", sRaw ? (const char*)sRaw : tRet.sError);
	} else {
		printf("PASS async_fetch_raw\n");
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	xpop3_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int xpop3_test_async_fetch_mime_delete(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3asyncopts tOpts;
	xpop3result tRet;
	xmailparsedmessage tParsed;
	int iFailed = 0;

	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("async_mime_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("async_mime_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	xrtPop3AsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "xpop3-test-mime";
	tOpts.iTimeoutMs = 5000u;
	tOpts.bDeleteAfterRetr = TRUE;

	if ( !xrtPop3FetchMimeAsyncWait(&tCfg, 1u, &tOpts, &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| strcmp((const char*)tParsed.sBody, ".dot\r\nbody\r\n") != 0 ) {
		iFailed = xpop3_test_fail("async_fetch_mime", tRet.sError);
	} else {
		printf("PASS async_fetch_mime_delete\n");
	}
	xmailMimeParsedMessageFree(&tParsed);
	xpop3_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int xpop3_test_async_top_preview(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3asyncopts tOpts;
	xpop3result tRet;
	xmailparsedmessage tParsed;
	str sRaw = NULL;
	int iFailed = 0;

	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("async_top_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("async_top_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	xrtPop3AsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "xpop3-test-top";
	tOpts.iTimeoutMs = 5000u;
	tOpts.bDeleteAfterRetr = TRUE;

	if ( !xrtPop3TopRawAsyncWait(&tCfg, 1u, 1u, &tOpts, &sRaw, &tRet)
		|| sRaw == NULL
		|| strcmp((const char*)sRaw, "Subject: hi\r\nFrom: sender@example.com\r\n\r\n.dot\r\n") != 0 ) {
		iFailed = xpop3_test_fail("async_top_raw", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	xpop3_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;

	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("async_top_mime_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("async_top_mime_server_thread", "thread create failed");
	}
	tCfg.iPort = tServer.iPort;
	if ( !xrtPop3TopMimeAsyncWait(&tCfg, 1u, 0u, &tOpts, &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| tParsed.sBody == NULL || strcmp((const char*)tParsed.sBody, "") != 0 ) {
		iFailed = xpop3_test_fail("async_top_mime", tRet.sError);
		goto cleanup;
	}
	printf("PASS async_top_preview\n");

cleanup:
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	xmailMimeParsedMessageFree(&tParsed);
	xpop3_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xpop3_test_async_summaries(void)
{
	xpop3_mock_server tServer;
	xthread pThread;
	xpop3config tCfg;
	xpop3asyncopts tOpts;
	xpop3result tRet;
	xpop3summary* arrSummary = NULL;
	size_t iSummaryCount = 0u;
	int iFailed = 0;

	if ( !xpop3_mock_server_start(&tServer) ) {
		return xpop3_test_fail("async_summaries_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xpop3_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xpop3_mock_server_stop(&tServer);
		return xpop3_test_fail("async_summaries_server_thread", "thread create failed");
	}

	xrtPop3ConfigInit(&tCfg);
	xrtPop3AsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XPOP3_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "xpop3-test-summaries";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtPop3ListSummariesAsyncWait(&tCfg, &tOpts, &arrSummary, &iSummaryCount, &tRet)
		|| iSummaryCount != 2u
		|| arrSummary[0].iNumber != 1u || arrSummary[0].iSize != 120u || strcmp(arrSummary[0].sUid, "abc") != 0
		|| strcmp(arrSummary[0].sSubject, "hi") != 0 || strcmp(arrSummary[0].sFrom, "sender@example.com") != 0
		|| arrSummary[1].iNumber != 2u || arrSummary[1].iSize != 240u || strcmp(arrSummary[1].sUid, "def-123") != 0
		|| strcmp(arrSummary[1].sSubject, "second") != 0 || strcmp(arrSummary[1].sFrom, "two@example.com") != 0 ) {
		iFailed = xpop3_test_fail("async_summaries", tRet.sError);
	} else {
		printf("PASS async_summaries\n");
	}
	xrtPop3SummariesFree(arrSummary);
	xpop3_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

int main(void)
{
	int iFailed = 0;
	setvbuf(stdout, NULL, _IONBF, 0);
	xpop3_test_link_api();
	printf("xpop3_test\n");
	iFailed |= xpop3_test_response_parse();
	iFailed |= xpop3_test_dot_unstuff();
	iFailed |= xpop3_test_list_uidl_parse();
	iFailed |= xpop3_test_capa_parse();
	iFailed |= xpop3_test_init_defaults();
	iFailed |= xpop3_test_mock_server_flow();
	iFailed |= xpop3_test_auth_plain_flow();
	iFailed |= xpop3_test_async_fetch_raw();
	iFailed |= xpop3_test_async_fetch_mime_delete();
	iFailed |= xpop3_test_async_top_preview();
	iFailed |= xpop3_test_async_summaries();
	if ( iFailed ) {
		printf("xpop3_test: FAIL\n");
		return 1;
	}
	printf("xpop3_test: PASS\n");
	return 0;
}
