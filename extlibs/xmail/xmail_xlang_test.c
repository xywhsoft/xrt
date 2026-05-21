#include <stdio.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "xmail.h"

static int xmail_xlang_expect_contains(const char* sName, const char* sJson, const char* sNeedle);

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bSawEhlo;
	bool bSawQuit;
	bool bSawMailFrom;
	bool bSawRcptTo;
	bool bSawData;
	bool bDataHasSubject;
	bool bDataHasBody;
} xmail_xlang_smtp_mock_server;

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bSawCapa;
	bool bSawStat;
	bool bSawList;
	bool bSawUidl;
	bool bSawRetr;
	bool bSawDelete;
	bool bSawQuit;
} xmail_xlang_pop3_mock_server;

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bSawCapability;
	bool bSawStatus;
	bool bSawList;
	bool bSawSelect;
	bool bSawUidSearch;
	bool bSawUidFetch;
	bool bSawHeaderFetch;
	bool bSawBodyStructure;
	bool bSawFetchFlags;
	bool bSawStoreFlags;
	bool bSawExpunge;
	bool bSawUidExpunge;
	bool bSawIdle;
	bool bSawLogout;
} xmail_xlang_imap_mock_server;

static void xmail_xlang_smtp_mock_close_socket(xsocket hSock)
{
	if ( hSock == XSOCKET_INVALID ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	closesocket(hSock);
#else
	close(hSock);
#endif
}

static bool xmail_xlang_smtp_mock_send_all(xsocket hSock, const char* sText)
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

static bool xmail_xlang_smtp_mock_recv_line(xsocket hSock, char* sOut, size_t iOutCap)
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

static uint32 xmail_xlang_smtp_mock_thread(ptr pArg)
{
	xmail_xlang_smtp_mock_server* pServer = (xmail_xlang_smtp_mock_server*)pArg;
	xsocket hClient;
	char sLine[4096];

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !xmail_xlang_smtp_mock_send_all(hClient, "220 xmail xlang smtp mock ready\r\n") ) {
		xmail_xlang_smtp_mock_close_socket(hClient);
		return 2u;
	}
	while ( xmail_xlang_smtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		if ( strncmp(sLine, "EHLO ", 5) == 0 || strncmp(sLine, "HELO ", 5) == 0 ) {
			pServer->bSawEhlo = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "250-localhost\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
		} else if ( strncmp(sLine, "MAIL FROM:", 10) == 0 ) {
			pServer->bSawMailFrom = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "250 sender ok\r\n");
		} else if ( strncmp(sLine, "RCPT TO:", 8) == 0 ) {
			pServer->bSawRcptTo = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "250 recipient ok\r\n");
		} else if ( strcmp(sLine, "DATA") == 0 ) {
			pServer->bSawData = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "354 end with dot\r\n");
			while ( xmail_xlang_smtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
				if ( strcmp(sLine, ".") == 0 ) {
					break;
				}
				if ( strstr(sLine, "Subject: xlang smtp mock") != NULL ) {
					pServer->bDataHasSubject = TRUE;
				}
				if ( strstr(sLine, "plain=20via=20xlang=20mock") != NULL ) {
					pServer->bDataHasBody = TRUE;
				}
			}
			xmail_xlang_smtp_mock_send_all(hClient, "250 queued\r\n");
		} else if ( strcmp(sLine, "QUIT") == 0 ) {
			pServer->bSawQuit = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "221 bye\r\n");
			break;
		} else {
			xmail_xlang_smtp_mock_send_all(hClient, "250 ok\r\n");
		}
	}
	xmail_xlang_smtp_mock_close_socket(hClient);
	return 0u;
}

static bool xmail_xlang_smtp_mock_start(xmail_xlang_smtp_mock_server* pServer)
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
	if ( bind(pServer->hListen, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 || listen(pServer->hListen, 1) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xmail_xlang_smtp_mock_stop(xmail_xlang_smtp_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xmail_xlang_test_smtp_mock_capability(void)
{
	xmail_xlang_smtp_mock_server tServer;
	xthread pThread;
	char sRequest[1024];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_smtp_mock_start(&tServer) ) {
		printf("FAIL smtp_mock_capability_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_smtp_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_smtp_mock_stop(&tServer);
		printf("FAIL smtp_mock_capability_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest), "{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"none\",\"timeout_ms\":5000}", (unsigned)tServer.iPort);
	sJson = xmail_smtp_capability_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("smtp_mock_capability_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("smtp_mock_capability_caps", sJson, "\"capabilities\":[\"8BITMIME\",\"SMTPUTF8\",\"SIZE\",\"DSN\"]");
	iFailed |= xmail_xlang_expect_contains("smtp_mock_capability_size", sJson, "\"size_limit\":65536");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawEhlo || !tServer.bSawQuit || tServer.bSawMailFrom || tServer.bSawRcptTo || tServer.bSawData ) {
		printf("FAIL smtp_mock_capability_flow\n");
		iFailed = 1;
	} else {
		printf("PASS smtp_mock_capability_flow\n");
	}
	xmail_xlang_smtp_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_smtp_mock_send(void)
{
	xmail_xlang_smtp_mock_server tServer;
	xthread pThread;
	char sRequest[2048];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_smtp_mock_start(&tServer) ) {
		printf("FAIL smtp_mock_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_smtp_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_smtp_mock_stop(&tServer);
		printf("FAIL smtp_mock_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"none\",\"timeout_ms\":5000},"
		"\"message\":{\"from\":{\"email\":\"sender@example.com\",\"name\":\"Sender\"},"
		"\"to\":[{\"email\":\"receiver@example.com\",\"name\":\"Receiver\"}],"
		"\"subject\":\"xlang smtp mock\",\"text\":\"plain via xlang mock\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_smtp_send_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("smtp_mock_send_ok", sJson, "\"ok\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData || !tServer.bDataHasSubject || !tServer.bDataHasBody ) {
		printf("FAIL smtp_mock_flow\n");
		iFailed = 1;
	} else {
		printf("PASS smtp_mock_flow\n");
	}
	xmail_xlang_smtp_mock_stop(&tServer);
	return iFailed;
}

static uint32 xmail_xlang_pop3_mock_thread(ptr pArg)
{
	xmail_xlang_pop3_mock_server* pServer = (xmail_xlang_pop3_mock_server*)pArg;
	xsocket hClient;
	char sLine[4096];

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !xmail_xlang_smtp_mock_send_all(hClient, "+OK xmail xlang pop3 mock ready\r\n") ) {
		xmail_xlang_smtp_mock_close_socket(hClient);
		return 2u;
	}
	while ( xmail_xlang_smtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		if ( strcmp(sLine, "USER user") == 0 ) {
			xmail_xlang_smtp_mock_send_all(hClient, "+OK user\r\n");
		} else if ( strcmp(sLine, "PASS pass") == 0 ) {
			xmail_xlang_smtp_mock_send_all(hClient, "+OK pass\r\n");
		} else if ( strcmp(sLine, "CAPA") == 0 ) {
			pServer->bSawCapa = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK capabilities\r\nUSER\r\nUIDL\r\nTOP\r\n.\r\n");
		} else if ( strcmp(sLine, "STAT") == 0 ) {
			pServer->bSawStat = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK 2 360\r\n");
		} else if ( strcmp(sLine, "LIST") == 0 ) {
			pServer->bSawList = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK list\r\n1 120\r\n2 240\r\n.\r\n");
		} else if ( strcmp(sLine, "UIDL") == 0 ) {
			pServer->bSawUidl = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK uidl\r\n1 abc\r\n2 def-123\r\n.\r\n");
		} else if ( strcmp(sLine, "TOP 1 0") == 0 ) {
			xmail_xlang_smtp_mock_send_all(hClient, "+OK top\r\nSubject: hi\r\nFrom: sender@example.com\r\nDate: Tue, 12 May 2026 10:00:00 +0800\r\n\r\n.\r\n");
		} else if ( strcmp(sLine, "TOP 2 0") == 0 ) {
			xmail_xlang_smtp_mock_send_all(hClient, "+OK top\r\nSubject: second\r\nFrom: two@example.com\r\nDate: Tue, 12 May 2026 10:01:00 +0800\r\n\r\n.\r\n");
		} else if ( strcmp(sLine, "RETR 1") == 0 ) {
			pServer->bSawRetr = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK message\r\nSubject: hi\r\nFrom: sender@example.com\r\n\r\n..dot\r\nbody\r\n.\r\n");
		} else if ( strcmp(sLine, "DELE 1") == 0 ) {
			pServer->bSawDelete = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK deleted\r\n");
		} else if ( strcmp(sLine, "QUIT") == 0 ) {
			pServer->bSawQuit = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+OK bye\r\n");
			break;
		} else {
			xmail_xlang_smtp_mock_send_all(hClient, "-ERR unknown\r\n");
		}
	}
	xmail_xlang_smtp_mock_close_socket(hClient);
	return 0u;
}

static bool xmail_xlang_pop3_mock_start(xmail_xlang_pop3_mock_server* pServer)
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
	if ( bind(pServer->hListen, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 || listen(pServer->hListen, 1) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xmail_xlang_pop3_mock_stop(xmail_xlang_pop3_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xmail_xlang_test_pop3_mock_capability(void)
{
	xmail_xlang_pop3_mock_server tServer;
	xthread pThread;
	char sRequest[1024];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_pop3_mock_start(&tServer) ) {
		printf("FAIL pop3_mock_capability_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_pop3_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_pop3_mock_stop(&tServer);
		printf("FAIL pop3_mock_capability_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest), "{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"userpass\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000}", (unsigned)tServer.iPort);
	sJson = xmail_pop3_capability_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("pop3_mock_capability_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_capability_caps", sJson, "\"capabilities\":[\"UIDL\",\"TOP\",\"USER\"]");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawCapa || !tServer.bSawQuit ) {
		printf("FAIL pop3_mock_capability_flow\n");
		iFailed = 1;
	} else {
		printf("PASS pop3_mock_capability_flow\n");
	}
	xmail_xlang_pop3_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_pop3_mock_list(void)
{
	xmail_xlang_pop3_mock_server tServer;
	xthread pThread;
	char sRequest[1024];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_pop3_mock_start(&tServer) ) {
		printf("FAIL pop3_mock_list_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_pop3_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_pop3_mock_stop(&tServer);
		printf("FAIL pop3_mock_list_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest), "{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"userpass\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000}", (unsigned)tServer.iPort);
	sJson = xmail_pop3_list_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("pop3_mock_list_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_list_subject", sJson, "\"subject\":\"second\"");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawList || !tServer.bSawUidl || !tServer.bSawQuit ) {
		printf("FAIL pop3_mock_list_flow\n");
		iFailed = 1;
	} else {
		printf("PASS pop3_mock_list_flow\n");
	}
	xmail_xlang_pop3_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_pop3_mock_status(void)
{
	xmail_xlang_pop3_mock_server tServer;
	xthread pThread;
	char sRequest[1024];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_pop3_mock_start(&tServer) ) {
		printf("FAIL pop3_mock_status_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_pop3_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_pop3_mock_stop(&tServer);
		printf("FAIL pop3_mock_status_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest), "{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"userpass\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000}", (unsigned)tServer.iPort);
	sJson = xmail_pop3_status_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("pop3_mock_status_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_status_count", sJson, "\"count\":2");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_status_size", sJson, "\"size\":360");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawStat || !tServer.bSawQuit ) {
		printf("FAIL pop3_mock_status_flow\n");
		iFailed = 1;
	} else {
		printf("PASS pop3_mock_status_flow\n");
	}
	xmail_xlang_pop3_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_pop3_mock_fetch(void)
{
	xmail_xlang_pop3_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_pop3_mock_start(&tServer) ) {
		printf("FAIL pop3_mock_fetch_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_pop3_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_pop3_mock_stop(&tServer);
		printf("FAIL pop3_mock_fetch_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"userpass\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"number\":1,\"parse_mime\":true}}",
		(unsigned)tServer.iPort);
	sJson = xmail_pop3_fetch_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("pop3_mock_fetch_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_fetch_body", sJson, ".dot\\r\\nbody");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawRetr || !tServer.bSawQuit ) {
		printf("FAIL pop3_mock_fetch_flow\n");
		iFailed = 1;
	} else {
		printf("PASS pop3_mock_fetch_flow\n");
	}
	xmail_xlang_pop3_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_pop3_mock_delete(void)
{
	xmail_xlang_pop3_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_pop3_mock_start(&tServer) ) {
		printf("FAIL pop3_mock_delete_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_pop3_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_pop3_mock_stop(&tServer);
		printf("FAIL pop3_mock_delete_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"auth\":\"userpass\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"number\":1}}",
		(unsigned)tServer.iPort);
	sJson = xmail_pop3_delete_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("pop3_mock_delete_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_delete_number", sJson, "\"number\":1");
	iFailed |= xmail_xlang_expect_contains("pop3_mock_delete_deleted", sJson, "\"deleted\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawDelete || !tServer.bSawQuit ) {
		printf("FAIL pop3_mock_delete_flow\n");
		iFailed = 1;
	} else {
		printf("PASS pop3_mock_delete_flow\n");
	}
	xmail_xlang_pop3_mock_stop(&tServer);
	return iFailed;
}

static void xmail_xlang_imap_mock_tag(const char* sLine, char* sTag, size_t iTagCap)
{
	const char* sEnd;
	size_t iLen;

	if ( sTag == NULL || iTagCap == 0u ) {
		return;
	}
	sTag[0] = '\0';
	sEnd = sLine ? strchr(sLine, ' ') : NULL;
	if ( sEnd == NULL ) {
		return;
	}
	iLen = (size_t)(sEnd - sLine);
	if ( iLen >= iTagCap ) {
		iLen = iTagCap - 1u;
	}
	memcpy(sTag, sLine, iLen);
	sTag[iLen] = '\0';
}

static uint32 xmail_xlang_imap_mock_thread(ptr pArg)
{
	xmail_xlang_imap_mock_server* pServer = (xmail_xlang_imap_mock_server*)pArg;
	xsocket hClient;
	char sLine[1024];
	char sTag[32];
	char sReply[1024];

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !xmail_xlang_smtp_mock_send_all(hClient, "* OK xmail xlang imap mock ready\r\n") ) {
		xmail_xlang_smtp_mock_close_socket(hClient);
		return 2u;
	}
	while ( xmail_xlang_smtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		xmail_xlang_imap_mock_tag(sLine, sTag, sizeof(sTag));
		if ( strstr(sLine, " CAPABILITY") != NULL ) {
			pServer->bSawCapability = TRUE;
			snprintf(sReply, sizeof(sReply), "* CAPABILITY IMAP4rev1 IDLE UIDPLUS\r\n%s OK CAPABILITY completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LOGIN ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "%s OK LOGIN completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " STATUS ") != NULL ) {
			pServer->bSawStatus = TRUE;
			snprintf(sReply, sizeof(sReply), "* STATUS \"INBOX\" (MESSAGES 2 RECENT 1 UIDNEXT 103 UIDVALIDITY 777 UNSEEN 1)\r\n%s OK STATUS completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LIST ") != NULL ) {
			pServer->bSawList = TRUE;
			snprintf(sReply, sizeof(sReply), "* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n* LIST (\\HasChildren \\Marked) \"/\" \"Archive\"\r\n%s OK LIST completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " SELECT ") != NULL ) {
			pServer->bSawSelect = TRUE;
			snprintf(sReply, sizeof(sReply), "* 2 EXISTS\r\n* OK [UIDVALIDITY 777] valid\r\n* OK [UIDNEXT 103] next\r\n%s OK [READ-WRITE] SELECT completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID SEARCH ") != NULL ) {
			pServer->bSawUidSearch = TRUE;
			snprintf(sReply, sizeof(sReply), "* SEARCH 101 102\r\n%s OK UID SEARCH completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID EXPUNGE ") != NULL ) {
			pServer->bSawUidExpunge = TRUE;
			snprintf(sReply, sizeof(sReply), "* 2 EXPUNGE\r\n%s OK UID EXPUNGE completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID STORE ") != NULL ) {
			pServer->bSawStoreFlags = TRUE;
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 FLAGS (\\Seen \\Deleted))\r\n%s OK UID STORE completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL ) {
			const char* sMsg = "Subject: hi\r\nFrom: sender@example.com\r\n\r\nbody\r\n";
			pServer->bSawUidFetch = TRUE;
			if ( strstr(sLine, "HEADER.FIELDS") != NULL ) {
				sMsg = "Subject: hi\r\nFrom: sender@example.com\r\n\r\n";
				pServer->bSawHeaderFetch = TRUE;
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
					strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)]" : "BODY[HEADER.FIELDS (SUBJECT FROM)]",
					(unsigned)strlen(sMsg));
			} else {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
					strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[]" : "BODY[]",
					(unsigned)strlen(sMsg));
			}
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
			xmail_xlang_smtp_mock_send_all(hClient, sMsg);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " FETCH ") != NULL && strstr(sLine, "BODYSTRUCTURE") != NULL ) {
			pServer->bSawBodyStructure = TRUE;
			snprintf(sReply, sizeof(sReply),
				"* 1 FETCH (BODYSTRUCTURE ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") \"plain-id\" \"plain description\" \"7BIT\" 4 1 \"plain-md5\" (\"INLINE\" (\"FILENAME\" \"plain.txt\")) \"en\" \"plain-part\") "
				"(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") \"html-id\" \"html description\" \"7BIT\" 11 1 \"html-md5\" (\"INLINE\" (\"FILENAME\" \"html.html\")) \"en\" \"html-part\") "
				"\"ALTERNATIVE\" (\"BOUNDARY\" \"alt-boundary\") (\"INLINE\" (\"FILENAME\" \"root.eml\")) \"en\" \"root-part\"))\r\n%s OK FETCH completed\r\n",
				sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " STORE ") != NULL ) {
			pServer->bSawStoreFlags = TRUE;
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (FLAGS (\\Seen \\Deleted))\r\n%s OK STORE completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " FETCH ") != NULL && strstr(sLine, "FLAGS") != NULL ) {
			pServer->bSawFetchFlags = TRUE;
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (FLAGS (\\Seen \\Answered))\r\n%s OK FETCH completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " EXPUNGE") != NULL ) {
			pServer->bSawExpunge = TRUE;
			snprintf(sReply, sizeof(sReply), "* 1 EXPUNGE\r\n%s OK EXPUNGE completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " IDLE") != NULL ) {
			pServer->bSawIdle = TRUE;
			xmail_xlang_smtp_mock_send_all(hClient, "+ idling\r\n* 3 EXISTS\r\n* 4 EXPUNGE\r\n");
			if ( !xmail_xlang_smtp_mock_recv_line(hClient, sLine, sizeof(sLine)) || strcmp(sLine, "DONE") != 0 ) {
				snprintf(sReply, sizeof(sReply), "%s BAD IDLE expected DONE\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "%s OK IDLE completed\r\n", sTag);
			}
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LOGOUT") != NULL ) {
			pServer->bSawLogout = TRUE;
			snprintf(sReply, sizeof(sReply), "* BYE logging out\r\n%s OK LOGOUT completed\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
			break;
		} else {
			snprintf(sReply, sizeof(sReply), "%s BAD unknown\r\n", sTag);
			xmail_xlang_smtp_mock_send_all(hClient, sReply);
		}
	}
	xmail_xlang_smtp_mock_close_socket(hClient);
	return 0u;
}

static bool xmail_xlang_imap_mock_start(xmail_xlang_imap_mock_server* pServer)
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
	if ( bind(pServer->hListen, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 || listen(pServer->hListen, 1) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xmail_xlang_imap_mock_stop(xmail_xlang_imap_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		xmail_xlang_smtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xmail_xlang_test_imap_mock_search(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_search_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_search_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"uid\":true,\"criteria\":\"ALL\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_search_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_search_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_search_ids", sJson, "\"ids\":[101,102]");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawUidSearch || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_search_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_search_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_fetch(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_fetch_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_fetch_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"uid\":101,\"peek\":true,\"parse_mime\":true}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_fetch_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_body", sJson, "\"text\":\"body");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawUidFetch || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_fetch_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_fetch_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_fetch_headers(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_fetch_headers_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_fetch_headers_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"uid\":101,\"peek\":true,\"headers\":[\"Subject\",\"From\"]}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_fetch_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_headers_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_headers_only", sJson, "\"headers_only\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_headers_subject_name", sJson, "\"name\":\"Subject\"");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_headers_subject_value", sJson, "\"value\":\"hi\"");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_headers_requested", sJson, "\"requested_headers\":[\"Subject\",\"From\"]");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawUidFetch || !tServer.bSawHeaderFetch || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_fetch_headers_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_fetch_headers_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_bodystructure(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_bodystructure_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_bodystructure_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"sequence\":1}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_bodystructure_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_bodystructure_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_bodystructure_multipart", sJson, "\"multipart\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_bodystructure_subtype", sJson, "\"subtype\":\"ALTERNATIVE\"");
	iFailed |= xmail_xlang_expect_contains("imap_mock_bodystructure_child_plain", sJson, "\"subtype\":\"PLAIN\"");
	iFailed |= xmail_xlang_expect_contains("imap_mock_bodystructure_param", sJson, "\"value\":\"alt-boundary\"");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawBodyStructure || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_bodystructure_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_bodystructure_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_fetch_flags(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_fetch_flags_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_fetch_flags_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"sequence\":1}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_fetch_flags_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_flags_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_flags_seen", sJson, "\"seen\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_fetch_flags_answered", sJson, "\"answered\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawFetchFlags || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_fetch_flags_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_fetch_flags_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_store_flags(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_store_flags_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_store_flags_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"uid\":101,\"operation\":\"+FLAGS\",\"flags\":\"(\\\\Deleted)\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_store_flags_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_store_flags_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_store_flags_deleted", sJson, "\"deleted\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_store_flags_uid_mode", sJson, "\"uid_mode\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawStoreFlags || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_store_flags_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_store_flags_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_expunge(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_expunge_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_expunge_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_expunge_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_expunge_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_expunge_sequences", sJson, "\"sequences\":[1]");
	iFailed |= xmail_xlang_expect_contains("imap_mock_expunge_uid_mode", sJson, "\"uid_mode\":false");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawExpunge || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_expunge_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_expunge_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_uid_expunge(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_uid_expunge_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_uid_expunge_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"uid_set\":\"101:*\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_expunge_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_uid_expunge_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_uid_expunge_sequences", sJson, "\"sequences\":[2]");
	iFailed |= xmail_xlang_expect_contains("imap_mock_uid_expunge_uid_mode", sJson, "\"uid_mode\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawUidExpunge || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_uid_expunge_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_uid_expunge_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_capability(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1024];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_capability_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_capability_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_capability_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_capability_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_capability_caps", sJson, "\"capabilities\":[\"IMAP4rev1\",\"IDLE\",\"UIDPLUS\"]");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawCapability || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_capability_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_capability_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_status(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_status_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_status_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_status_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_status_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_status_messages", sJson, "\"messages\":2");
	iFailed |= xmail_xlang_expect_contains("imap_mock_status_unseen", sJson, "\"unseen\":1");
	iFailed |= xmail_xlang_expect_contains("imap_mock_status_uidnext", sJson, "\"uid_next\":103");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawStatus || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_status_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_status_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_list(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_list_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_list_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"reference\":\"\",\"pattern\":\"*\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_list_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_list_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_list_count", sJson, "\"count\":2");
	iFailed |= xmail_xlang_expect_contains("imap_mock_list_inbox", sJson, "\"mailbox\":\"INBOX\"");
	iFailed |= xmail_xlang_expect_contains("imap_mock_list_archive", sJson, "\"mailbox\":\"Archive\"");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawList || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_list_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_list_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_idle_probe(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_idle_probe_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_idle_probe_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\"}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_idle_probe_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_idle_probe_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_idle_probe_supported", sJson, "\"idle_supported\":true");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawIdle || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_idle_probe_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_idle_probe_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_idle(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_idle_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_idle_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"max_events\":2}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_idle_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_idle_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_idle_events", sJson, "\"events\":[\"* 3 EXISTS\",\"* 4 EXPUNGE\"]");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawIdle || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_idle_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_idle_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}

static int xmail_xlang_test_imap_mock_watch(void)
{
	xmail_xlang_imap_mock_server tServer;
	xthread pThread;
	char sRequest[1200];
	const char* sJson;
	int iFailed = 0;

	if ( !xmail_xlang_imap_mock_start(&tServer) ) {
		printf("FAIL imap_mock_watch_start\n");
		return 1;
	}
	pThread = xrtThreadCreate((ptr)xmail_xlang_imap_mock_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xmail_xlang_imap_mock_stop(&tServer);
		printf("FAIL imap_mock_watch_thread\n");
		return 1;
	}
	snprintf(sRequest, sizeof(sRequest),
		"{\"config\":{\"host\":\"127.0.0.1\",\"port\":%u,\"secure\":\"none\",\"user\":\"user\",\"password\":\"pass\",\"timeout_ms\":5000},"
		"\"request\":{\"mailbox\":\"INBOX\",\"max_cycles\":1,\"max_events_per_cycle\":2}}",
		(unsigned)tServer.iPort);
	sJson = xmail_imap_watch_json_native(sRequest);
	iFailed |= xmail_xlang_expect_contains("imap_mock_watch_ok", sJson, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("imap_mock_watch_events", sJson, "\"events\":[\"* 3 EXISTS\",\"* 4 EXPUNGE\"]");
	iFailed |= xmail_xlang_expect_contains("imap_mock_watch_summary", sJson, "\"cycles\":1");
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	if ( !tServer.bSawSelect || !tServer.bSawIdle || !tServer.bSawLogout ) {
		printf("FAIL imap_mock_watch_flow\n");
		iFailed = 1;
	} else {
		printf("PASS imap_mock_watch_flow\n");
	}
	xmail_xlang_imap_mock_stop(&tServer);
	return iFailed;
}



static int xmail_xlang_expect_contains(const char* sName, const char* sJson, const char* sNeedle)
{
	if ( sJson == NULL || strstr(sJson, sNeedle) == NULL ) {
		printf("FAIL %s\n", sName);
		printf("json  : %s\n", sJson ? sJson : "(null)");
		printf("needle: %s\n", sNeedle);
		return 1;
	}
	printf("PASS %s\n", sName);
	return 0;
}

int main(void)
{
	int iFailed = 0;
	const char* sBuilt;
	const char* sRaw;
	sBuilt = xmail_mime_build_json_native("{\"from\":{\"email\":\"sender@example.com\",\"name\":\"Sender\"},\"to\":[{\"email\":\"user@example.com\",\"name\":\"User\"}],\"subject\":\"hello\",\"text\":\"plain body\",\"html\":\"<b>html body</b>\",\"headers\":[{\"name\":\"X-Test\",\"value\":\"ok\"}],\"attachments\":[{\"filename\":\"note.txt\",\"content_type\":\"text/plain\",\"content\":\"attachment body\",\"inline\":false}]}");
	iFailed |= xmail_xlang_expect_contains("mime_build_ok", sBuilt, "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("mime_build_subject", sBuilt, "Subject: hello");
	sRaw = "Subject: hi\r\nFrom: Sender <sender@example.com>\r\nTo: User <user@example.com>\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\nhello=20world\r\n";
	iFailed |= xmail_xlang_expect_contains("mime_parse_ok", xmail_mime_parse_json_native(sRaw), "\"ok\":true");
	iFailed |= xmail_xlang_expect_contains("mime_parse_text", xmail_mime_parse_json_native(sRaw), "hello world");
	iFailed |= xmail_xlang_expect_contains("smtp_send_native_error", xmail_smtp_send_json_native("{\"config\":{\"host\":\"\",\"secure\":\"none\",\"auth\":\"none\"},\"message\":{\"from\":{\"email\":\"sender@example.com\",\"name\":\"Sender\"},\"to\":[{\"email\":\"user@example.com\",\"name\":\"User\"}],\"subject\":\"smtp native\",\"text\":\"body\"}}"), "SMTP host is empty");
	iFailed |= xmail_xlang_test_smtp_mock_capability();
	iFailed |= xmail_xlang_test_smtp_mock_send();
	iFailed |= xmail_xlang_test_pop3_mock_capability();
	iFailed |= xmail_xlang_test_pop3_mock_status();
	iFailed |= xmail_xlang_test_pop3_mock_list();
	iFailed |= xmail_xlang_test_pop3_mock_fetch();
	iFailed |= xmail_xlang_test_pop3_mock_delete();
	iFailed |= xmail_xlang_test_imap_mock_capability();
	iFailed |= xmail_xlang_test_imap_mock_status();
	iFailed |= xmail_xlang_test_imap_mock_list();
	iFailed |= xmail_xlang_test_imap_mock_search();
	iFailed |= xmail_xlang_test_imap_mock_fetch();
	iFailed |= xmail_xlang_test_imap_mock_fetch_headers();
	iFailed |= xmail_xlang_test_imap_mock_bodystructure();
	iFailed |= xmail_xlang_test_imap_mock_fetch_flags();
	iFailed |= xmail_xlang_test_imap_mock_store_flags();
	iFailed |= xmail_xlang_test_imap_mock_expunge();
	iFailed |= xmail_xlang_test_imap_mock_uid_expunge();
	iFailed |= xmail_xlang_test_imap_mock_idle_probe();
	iFailed |= xmail_xlang_test_imap_mock_idle();
	iFailed |= xmail_xlang_test_imap_mock_watch();
	if ( iFailed ) {
		printf("xmail_xlang_test: FAIL\n");
		return 1;
	}
	printf("xmail_xlang_test: PASS\n");
	return 0;
}
