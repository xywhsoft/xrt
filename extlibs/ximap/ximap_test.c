#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "ximap.h"

typedef struct {
	xsocket hListen;
	uint16 iPort;
	uint32 iMaxConnections;
} ximap_mock_server;

static int ximap_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static void ximap_test_link_helpers(void)
{
	(void)&xrtImapConfigInit;
	(void)&xrtImapResultInit;
	(void)&xrtImapAsyncOptsInit;
	(void)&xrtImapIdleLoopOptsInit;
	(void)&xrtImapResultFree;
	(void)&xrtImapFetchRawResultFree;
	(void)&xrtImapFetchMimeResultFree;
	(void)&xrtImapFlagsResultFree;
	(void)&xrtImapStatusResultFree;
	(void)&xrtImapListResultFree;
	(void)&xrtImapSearchResultFree;
	(void)&xrtImapBodyStructureResultFree;
	(void)&xrtImapOpen;
	(void)&xrtImapClose;
	(void)&xrtImapLogin;
	(void)&xrtImapAuthenticateXOAuth2;
	(void)&xrtImapCapability;
	(void)&xrtImapCommandBegin;
	(void)&xrtImapCommandFinish;
	(void)&xrtImapStatusRaw;
	(void)&xrtImapStatus;
	(void)&xrtImapSelect;
	(void)&xrtImapSelectStatus;
	(void)&xrtImapExamine;
	(void)&xrtImapExamineStatus;
	(void)&xrtImapListRaw;
	(void)&xrtImapList;
	(void)&xrtImapListFree;
	(void)&xrtImapSearchRaw;
	(void)&xrtImapUidSearchRaw;
	(void)&xrtImapSearch;
	(void)&xrtImapUidSearch;
	(void)&xrtImapSearchIdsFree;
	(void)&xrtImapFetchRaw;
	(void)&xrtImapFetchFlags;
	(void)&xrtImapFetchBodyStructureRaw;
	(void)&xrtImapFetchBodyStructure;
	(void)&xrtImapBodyStructureFree;
	(void)&xrtImapUidFetchBodyRaw;
	(void)&xrtImapUidPeekBodyRaw;
	(void)&xrtImapUidFetchBodyLiteral;
	(void)&xrtImapUidPeekBodyLiteral;
	(void)&xrtImapUidFetchBodySectionRaw;
	(void)&xrtImapUidPeekBodySectionRaw;
	(void)&xrtImapUidFetchHeaderFieldsRaw;
	(void)&xrtImapUidPeekHeaderFieldsRaw;
	(void)&xrtImapUidFetchHeaderFields;
	(void)&xrtImapUidPeekHeaderFields;
	(void)&xrtImapUidFetchMime;
	(void)&xrtImapUidPeekMime;
	(void)&xrtImapStoreFlags;
	(void)&xrtImapStoreFlagsParsed;
	(void)&xrtImapUidStoreFlags;
	(void)&xrtImapUidStoreFlagsParsed;
	(void)&xrtImapExpungeRaw;
	(void)&xrtImapExpunge;
	(void)&xrtImapUidExpungeRaw;
	(void)&xrtImapUidExpunge;
	(void)&xrtImapExpungeResultFree;
	(void)&xrtImapExpungeAsyncResultFree;
	(void)&xrtImapIdleOnce;
	(void)&xrtImapIdleProbe;
	(void)&xrtImapIdleCollect;
	(void)&xrtImapIdleWatch;
	(void)&xrtImapIdleLoop;
	(void)&xrtImapIdleEventsFree;
	(void)&xrtImapIdleResultFree;
	(void)&xrtImapIdleWatchResultFree;
	(void)&xrtImapLogout;
	(void)&xrtImapUidFetchBodyRawFuture;
	(void)&xrtImapStatusFuture;
	(void)&xrtImapListFuture;
	(void)&xrtImapSearchFuture;
	(void)&xrtImapUidSearchFuture;
	(void)&xrtImapFetchBodyStructureFuture;
	(void)&xrtImapFetchFlagsFuture;
	(void)&xrtImapStoreFlagsFuture;
	(void)&xrtImapUidStoreFlagsFuture;
	(void)&xrtImapExpungeFuture;
	(void)&xrtImapUidExpungeFuture;
	(void)&xrtImapIdleCollectFuture;
	(void)&xrtImapIdleWatchFuture;
	(void)&xrtImapUidFetchBodyLiteralFuture;
	(void)&xrtImapUidFetchMimeFuture;
	(void)&xrtImapUidPeekBodyRawFuture;
	(void)&xrtImapUidPeekBodyLiteralFuture;
	(void)&xrtImapUidPeekMimeFuture;
	(void)&xrtImapUidFetchBodySectionRawFuture;
	(void)&xrtImapUidPeekBodySectionRawFuture;
	(void)&xrtImapUidFetchHeaderFieldsRawFuture;
	(void)&xrtImapUidFetchHeaderFieldsFuture;
	(void)&xrtImapUidPeekHeaderFieldsRawFuture;
	(void)&xrtImapUidPeekHeaderFieldsFuture;
	(void)&xrtImapUidFetchBodyRawAsyncWait;
	(void)&xrtImapStatusAsyncWait;
	(void)&xrtImapListAsyncWait;
	(void)&xrtImapSearchAsyncWait;
	(void)&xrtImapUidSearchAsyncWait;
	(void)&xrtImapFetchBodyStructureAsyncWait;
	(void)&xrtImapFetchFlagsAsyncWait;
	(void)&xrtImapStoreFlagsAsyncWait;
	(void)&xrtImapUidStoreFlagsAsyncWait;
	(void)&xrtImapExpungeAsyncWait;
	(void)&xrtImapUidExpungeAsyncWait;
	(void)&xrtImapIdleCollectAsyncWait;
	(void)&xrtImapIdleWatchAsyncWait;
	(void)&xrtImapUidFetchBodyLiteralAsyncWait;
	(void)&xrtImapUidFetchMimeAsyncWait;
	(void)&xrtImapUidPeekBodyRawAsyncWait;
	(void)&xrtImapUidPeekBodyLiteralAsyncWait;
	(void)&xrtImapUidPeekMimeAsyncWait;
	(void)&xrtImapUidFetchBodySectionRawAsyncWait;
	(void)&xrtImapUidPeekBodySectionRawAsyncWait;
	(void)&xrtImapUidFetchHeaderFieldsRawAsyncWait;
	(void)&xrtImapUidFetchHeaderFieldsAsyncWait;
	(void)&xrtImapUidPeekHeaderFieldsRawAsyncWait;
	(void)&xrtImapUidPeekHeaderFieldsAsyncWait;
	(void)&ximap_buf_append;
	(void)&ximap_buf_free;
	(void)&ximap_result_errorf;
}

static bool ximap_mock_send_all(xsocket hSock, const char* sText)
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

static bool ximap_mock_recv_line(xsocket hSock, char* sOut, size_t iOutCap)
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

static void ximap_mock_tag(const char* sLine, char* sTag, size_t iTagCap)
{
	const char* sEnd;
	size_t iLen;

	if ( sTag == NULL || iTagCap == 0u ) {
		return;
	}
	sTag[0] = '\0';
	if ( sLine == NULL ) {
		return;
	}
	sEnd = strchr(sLine, ' ');
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

static uint32 ximap_mock_server_thread(ptr pArg)
{
	ximap_mock_server* pServer = (ximap_mock_server*)pArg;
	xsocket hClient;
	char sLine[512];
	char sTag[32];
	char sReply[1024];
	uint32 iConn;
	uint32 iMaxConn;

	iMaxConn = pServer->iMaxConnections == 0u ? 1u : pServer->iMaxConnections;
	for ( iConn = 0u; iConn < iMaxConn; iConn++ ) {
		hClient = accept(pServer->hListen, NULL, NULL);
		if ( hClient == XSOCKET_INVALID ) {
			return 1u;
		}
		if ( !ximap_mock_send_all(hClient, "* OK mock ready\r\n") ) {
			XIMAP_CLOSESOCK(hClient);
			return 2u;
		}
		while ( ximap_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		ximap_mock_tag(sLine, sTag, sizeof(sTag));
		if ( strstr(sLine, " CAPABILITY") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* CAPABILITY IMAP4rev1 STARTTLS IDLE UIDPLUS AUTH=XOAUTH2\r\n%s OK CAPABILITY completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " STATUS ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* STATUS \"INBOX\" (MESSAGES 2 RECENT 1 UIDNEXT 103 UIDVALIDITY 777 UNSEEN 1)\r\n%s OK STATUS completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " AUTHENTICATE XOAUTH2") != NULL ) {
			ximap_mock_send_all(hClient, "+ \r\n");
			if ( !ximap_mock_recv_line(hClient, sLine, sizeof(sLine)) || sLine[0] == '\0' ) {
				snprintf(sReply, sizeof(sReply), "%s BAD XOAUTH2 missing payload\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "%s OK XOAUTH2 completed\r\n", sTag);
			}
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LOGIN ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "%s OK LOGIN completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " SELECT ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* 2 EXISTS\r\n* 1 RECENT\r\n* OK [UIDVALIDITY 777] valid\r\n* OK [UIDNEXT 103] next\r\n* FLAGS (\\Seen \\Answered)\r\n%s OK [READ-WRITE] SELECT completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " EXAMINE ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* 2 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 777] valid\r\n* OK [UIDNEXT 103] next\r\n%s OK [READ-ONLY] EXAMINE completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LIST ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n%s OK LIST completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID SEARCH ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* SEARCH 101 102\r\n%s OK UID SEARCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " SEARCH ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* SEARCH 1 2\r\n%s OK SEARCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " BODYSTRUCTURE") != NULL ) {
			if ( strstr(sLine, " FETCH 2 ") != NULL ) {
				snprintf(sReply, sizeof(sReply), "* 2 FETCH (BODYSTRUCTURE (\"MESSAGE\" \"RFC822\" (\"NAME\" \"forwarded.eml\") \"msg-id\" \"fwd description\" \"7BIT\" 345 (\"Mon, 1 Jan 2024 00:00:00 +0000\" \"fwd subject\" ((\"Sender\" NIL \"sender\" \"example.com\")) ((\"Sender\" NIL \"sender\" \"example.com\")) ((\"Reply\" NIL \"reply\" \"example.com\")) ((\"To\" NIL \"to\" \"example.com\")) NIL NIL NIL \"<mid@example.com>\") (\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 12 1) 20 \"msg-md5\" (\"ATTACHMENT\" (\"FILENAME\" \"forwarded.eml\")) \"en\" \"msg-loc\"))\r\n%s OK BODYSTRUCTURE completed\r\n", sTag);
			} else if ( strstr(sLine, " FETCH 3 ") != NULL ) {
				snprintf(sReply, sizeof(sReply), "* 3 FETCH (BODYSTRUCTURE (\"APPLICATION\" \"PDF\" (\"NAME\" \"report.pdf\") \"pdf-id\" \"pdf description\" \"BASE64\" 2048 \"pdf-md5\" (\"ATTACHMENT\" (\"FILENAME\" \"report.pdf\" \"SIZE\" \"2048\")) NIL \"pdf-loc\"))\r\n%s OK BODYSTRUCTURE completed\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (BODYSTRUCTURE ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") \"plain-id\" \"plain description\" \"7BIT\" 4 1 \"plain-md5\" (\"INLINE\" (\"FILENAME\" \"plain.txt\")) \"en\" \"plain-part\")(\"TEXT\" \"HTML\" (\"CHARSET\" \"UTF-8\") \"html-id\" \"html description\" \"7BIT\" 11 1 \"html-md5\" (\"INLINE\" (\"FILENAME\" \"html.html\")) \"en\" \"html-part\") \"ALTERNATIVE\" (\"BOUNDARY\" \"alt-boundary\") (\"INLINE\" (\"FILENAME\" \"root.eml\")) \"en\" \"root-part\"))\r\n%s OK BODYSTRUCTURE completed\r\n", sTag);
			}
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL
			&& (strstr(sLine, " BODY[HEADER.FIELDS (Subject From)]") != NULL || strstr(sLine, " BODY.PEEK[HEADER.FIELDS (Subject From)]") != NULL) ) {
			const char* sHeader = "Subject: hi\r\nFrom: sender@example.com\r\n\r\n";
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
				strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[HEADER.FIELDS (Subject From)]" : "BODY[HEADER.FIELDS (Subject From)]",
				(unsigned)strlen(sHeader));
			ximap_mock_send_all(hClient, sReply);
			ximap_mock_send_all(hClient, sHeader);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL
			&& (strstr(sLine, " BODY[HEADER]") != NULL || strstr(sLine, " BODY.PEEK[HEADER]") != NULL) ) {
			const char* sHeader = "Subject: hi\r\nFrom: sender@example.com\r\n\r\n";
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
				strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[HEADER]" : "BODY[HEADER]",
				(unsigned)strlen(sHeader));
			ximap_mock_send_all(hClient, sReply);
			ximap_mock_send_all(hClient, sHeader);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL
			&& (strstr(sLine, " BODY[1.MIME]") != NULL || strstr(sLine, " BODY.PEEK[1.MIME]") != NULL) ) {
			const char* sPartHeader = "Content-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: 7bit\r\n\r\n";
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
				strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[1.MIME]" : "BODY[1.MIME]",
				(unsigned)strlen(sPartHeader));
			ximap_mock_send_all(hClient, sReply);
			ximap_mock_send_all(hClient, sPartHeader);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL
			&& (strstr(sLine, " BODY[1.HEADER.FIELDS (Subject From)]") != NULL || strstr(sLine, " BODY.PEEK[1.HEADER.FIELDS (Subject From)]") != NULL) ) {
			const char* sPartHeader = "Subject: part hi\r\nFrom: part@example.com\r\n\r\n";
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
				strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[1.HEADER.FIELDS (Subject From)]" : "BODY[1.HEADER.FIELDS (Subject From)]",
				(unsigned)strlen(sPartHeader));
			ximap_mock_send_all(hClient, sReply);
			ximap_mock_send_all(hClient, sPartHeader);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID FETCH ") != NULL ) {
			const char* sMsg = "Subject: hi\r\nFrom: sender@example.com\r\n\r\nbody\r\n";
			if ( strstr(sLine, " UID FETCH 202 ") != NULL ) {
				sMsg =
					"Subject: multipart hi\r\n"
					"From: sender@example.com\r\n"
					"Content-Type: multipart/mixed; boundary=\"mix\"\r\n"
					"\r\n"
					"--mix\r\n"
					"Content-Type: multipart/alternative; boundary=\"alt\"\r\n"
					"\r\n"
					"--alt\r\n"
					"Content-Type: text/plain; charset=UTF-8\r\n"
					"\r\n"
					"plain body\r\n"
					"--alt\r\n"
					"Content-Type: text/html; charset=UTF-8\r\n"
					"\r\n"
					"<p>html body</p>\r\n"
					"--alt--\r\n"
					"--mix\r\n"
					"Content-Type: text/plain\r\n"
					"Content-Disposition: attachment; filename=\"report.txt\"\r\n"
					"\r\n"
					"file body\r\n"
					"--mix--\r\n";
			}
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 %s {%u}\r\n",
				strstr(sLine, "BODY.PEEK") ? "BODY.PEEK[]" : "BODY[]",
				(unsigned)strlen(sMsg));
			ximap_mock_send_all(hClient, sReply);
			ximap_mock_send_all(hClient, sMsg);
			snprintf(sReply, sizeof(sReply), ")\r\n%s OK UID FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " FETCH ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* 1 FETCH (FLAGS (\\Seen \\Answered))\r\n%s OK FETCH completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID STORE ") != NULL ) {
			if ( strstr(sLine, " +FLAGS ") != NULL ) {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 FLAGS (\\Seen \\Deleted))\r\n%s OK UID STORE completed\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (UID 101 FLAGS (\\Seen))\r\n%s OK UID STORE completed\r\n", sTag);
			}
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " STORE ") != NULL ) {
			if ( strstr(sLine, " -FLAGS ") != NULL ) {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (FLAGS (\\Answered))\r\n%s OK STORE completed\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "* 1 FETCH (FLAGS (\\Seen \\Answered))\r\n%s OK STORE completed\r\n", sTag);
			}
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " UID EXPUNGE ") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* 2 EXPUNGE\r\n%s OK UID EXPUNGE completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " EXPUNGE") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* 1 EXPUNGE\r\n%s OK EXPUNGE completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " IDLE") != NULL ) {
			ximap_mock_send_all(hClient, "+ idling\r\n* 3 EXISTS\r\n* 4 EXPUNGE\r\n");
			if ( !ximap_mock_recv_line(hClient, sLine, sizeof(sLine)) || strcmp(sLine, "DONE") != 0 ) {
				snprintf(sReply, sizeof(sReply), "%s BAD IDLE expected DONE\r\n", sTag);
			} else {
				snprintf(sReply, sizeof(sReply), "%s OK IDLE completed\r\n", sTag);
			}
			ximap_mock_send_all(hClient, sReply);
		} else if ( strstr(sLine, " LOGOUT") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* BYE logging out\r\n%s OK LOGOUT completed\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
			break;
		} else {
			snprintf(sReply, sizeof(sReply), "%s BAD unknown\r\n", sTag);
			ximap_mock_send_all(hClient, sReply);
		}
	}
		XIMAP_CLOSESOCK(hClient);
	}
	return 0u;
}

static uint32 ximap_mock_pipeline_server_thread(ptr pArg)
{
	ximap_mock_server* pServer = (ximap_mock_server*)pArg;
	xsocket hClient;
	char sLine1[512];
	char sLine2[512];
	char sTag1[32];
	char sTag2[32];
	char sReply[1024];

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !ximap_mock_send_all(hClient, "* OK mock ready\r\n") ) {
		XIMAP_CLOSESOCK(hClient);
		return 2u;
	}
	if ( !ximap_mock_recv_line(hClient, sLine1, sizeof(sLine1))
		|| !ximap_mock_recv_line(hClient, sLine2, sizeof(sLine2)) ) {
		XIMAP_CLOSESOCK(hClient);
		return 3u;
	}
	ximap_mock_tag(sLine1, sTag1, sizeof(sTag1));
	ximap_mock_tag(sLine2, sTag2, sizeof(sTag2));
	snprintf(sReply, sizeof(sReply),
		"* STATUS \"INBOX\" (MESSAGES 4 RECENT 0 UIDNEXT 105 UIDVALIDITY 777 UNSEEN 2)\r\n"
		"%s OK STATUS completed\r\n"
		"* CAPABILITY IMAP4rev1 IDLE UIDPLUS\r\n"
		"%s OK CAPABILITY completed\r\n",
		sTag2, sTag1);
	ximap_mock_send_all(hClient, sReply);
	while ( ximap_mock_recv_line(hClient, sLine1, sizeof(sLine1)) ) {
		ximap_mock_tag(sLine1, sTag1, sizeof(sTag1));
		if ( strstr(sLine1, " LOGOUT") != NULL ) {
			snprintf(sReply, sizeof(sReply), "* BYE logging out\r\n%s OK LOGOUT completed\r\n", sTag1);
			ximap_mock_send_all(hClient, sReply);
			break;
		}
	}
	XIMAP_CLOSESOCK(hClient);
	return 0u;
}

static bool ximap_mock_server_start(ximap_mock_server* pServer)
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
		XIMAP_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	if ( listen(pServer->hListen, 1) != 0 ) {
		XIMAP_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		XIMAP_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void ximap_mock_server_stop(ximap_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		XIMAP_CLOSESOCK(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

typedef struct {
	size_t iCount;
	size_t iTraceCount;
	size_t iHeartbeatCount;
	char sFirst[128];
	char sLastTrace[64];
} ximap_idle_watch_state;

static bool ximap_test_idle_watch_callback(const char* sEvent, void* pUser)
{
	ximap_idle_watch_state* pState = (ximap_idle_watch_state*)pUser;

	if ( pState == NULL || sEvent == NULL ) {
		return FALSE;
	}
	pState->iCount++;
	if ( pState->iCount == 1u ) {
		snprintf(pState->sFirst, sizeof(pState->sFirst), "%s", sEvent);
	}
	return FALSE;
}

static bool ximap_test_idle_loop_callback(const char* sEvent, void* pUser)
{
	ximap_idle_watch_state* pState = (ximap_idle_watch_state*)pUser;

	if ( pState == NULL || sEvent == NULL ) {
		return FALSE;
	}
	pState->iCount++;
	if ( pState->iCount == 1u ) {
		snprintf(pState->sFirst, sizeof(pState->sFirst), "%s", sEvent);
	}
	return TRUE;
}

static bool ximap_test_idle_loop_should_stop(void* pUser)
{
	ximap_idle_watch_state* pState = (ximap_idle_watch_state*)pUser;

	return pState != NULL && pState->iCount >= 1u;
}

static void ximap_test_idle_loop_trace(const char* sEvent, size_t iCycle, size_t iEvents, uint32 iDelayMs, const ximapresult* pRet, void* pUser)
{
	ximap_idle_watch_state* pState = (ximap_idle_watch_state*)pUser;

	(void)iCycle;
	(void)iEvents;
	(void)iDelayMs;
	(void)pRet;
	if ( pState == NULL || sEvent == NULL ) {
		return;
	}
	pState->iTraceCount++;
	snprintf(pState->sLastTrace, sizeof(pState->sLastTrace), "%s", sEvent);
}

static bool ximap_test_idle_loop_heartbeat(size_t iCycle, size_t iEvents, const ximapresult* pRet, void* pUser)
{
	ximap_idle_watch_state* pState = (ximap_idle_watch_state*)pUser;

	(void)iCycle;
	(void)iEvents;
	(void)pRet;
	if ( pState == NULL ) {
		return FALSE;
	}
	pState->iHeartbeatCount++;
	return TRUE;
}

static int ximap_test_init_defaults(void)
{
	ximapconfig tCfg;
	ximapresult tRet;

	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	if ( tCfg.iPort != 993u || tCfg.iTimeoutMs != 15000u || tCfg.iSecureMode != XIMAP_SECURE_AUTO || !tCfg.bVerifyPeer ) {
		return ximap_test_fail("init_defaults", "unexpected config defaults");
	}
	ximap_result_stage(&tRet, XIMAP_STAGE_FETCH);
	ximap_result_error(&tRet, "sample");
	if ( tRet.iStage != XIMAP_STAGE_FETCH || tRet.bSuccess || strcmp(tRet.sError, "sample") != 0 ) {
		return ximap_test_fail("result_stage_error", "unexpected result state");
	}
	printf("PASS init_defaults\n");
	return 0;
}

static int ximap_test_tagged_command(void)
{
	uint32 iCounter = 0u;
	str sCmd;
	int iFailed = 0;

	sCmd = ximap_build_tagged_command(&iCounter, "CAPABILITY");
	if ( sCmd == NULL || strcmp((const char*)sCmd, "A0001 CAPABILITY") != 0 ) {
		iFailed = ximap_test_fail("tagged_command", sCmd ? (const char*)sCmd : "(null)");
	}
	if ( sCmd ) xrtFree(sCmd);
	if ( iCounter != 1u ) {
		return ximap_test_fail("tag_counter", "counter mismatch");
	}
	if ( !iFailed ) {
		printf("PASS tagged_command\n");
	}
	return iFailed;
}

static int ximap_test_tagged_line(void)
{
	ximaptaggedline tLine;

	if ( !ximap_parse_tagged_line("A0001 OK completed", &tLine) ) {
		return ximap_test_fail("tagged_line_parse", "parse failed");
	}
	if ( strcmp(tLine.sTag, "A0001") != 0 || tLine.iStatus != XIMAP_STATUS_OK || strcmp(tLine.sText, "completed") != 0 ) {
		return ximap_test_fail("tagged_line_values", "unexpected values");
	}
	if ( !ximap_parse_tagged_line("B002 NO nope", &tLine) || tLine.iStatus != XIMAP_STATUS_NO ) {
		return ximap_test_fail("tagged_line_no", "NO not parsed");
	}
	printf("PASS tagged_line\n");
	return 0;
}

static int ximap_test_untagged_literal_capability(void)
{
	ximapuntaggedline tUntagged;
	ximapliteralmarker tLiteral;
	ximapflags tFlags;
	uint32 iCaps = 0u;

	if ( !ximap_parse_untagged_line("* 23 FETCH (FLAGS (\\Seen) BODY[] {12}", &tUntagged) ) {
		return ximap_test_fail("untagged_parse", "parse failed");
	}
	if ( strncmp(tUntagged.sAtom, "FETCH", 5) != 0 || strstr(tUntagged.sRest, "FLAGS") == NULL ) {
		return ximap_test_fail("untagged_values", "unexpected values");
	}
	if ( !ximap_parse_literal_marker("* 23 FETCH (BODY[] {12}", &tLiteral) || !tLiteral.bHasLiteral || tLiteral.iLiteralLen != 12u ) {
		return ximap_test_fail("literal_marker", "literal parse failed");
	}
	if ( !ximap_line_is_continuation("+ go ahead") ) {
		return ximap_test_fail("continuation", "continuation not detected");
	}
	ximap_parse_capability_line("* CAPABILITY IMAP4rev1 STARTTLS IDLE UIDPLUS AUTH=XOAUTH2", &iCaps);
	if ( !(iCaps & XIMAP_CAP_IMAP4REV1) || !(iCaps & XIMAP_CAP_STARTTLS) || !(iCaps & XIMAP_CAP_IDLE)
		|| !(iCaps & XIMAP_CAP_UIDPLUS) || !(iCaps & XIMAP_CAP_XOAUTH2) ) {
		return ximap_test_fail("capability_parse", "missing capability");
	}
	if ( !ximap_parse_fetch_flags_response("* 23 FETCH (FLAGS (\\Seen \\Answered \\Draft))\r\nA0001 OK done\r\n", &tFlags)
		|| !tFlags.bSeen || !tFlags.bAnswered || !tFlags.bDraft || tFlags.bDeleted ) {
		return ximap_test_fail("flags_parse", "flags parse failed");
	}
	printf("PASS untagged_literal_capability\n");
	return 0;
}

static int ximap_test_mock_server_flow(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	xmailparsedmessage tParsed;
	ximaplist tList;
	ximapmailboxstatus tMailboxStatus;
	ximapexpungeresult tExpunge;
	ximapsearchids tSearchIds;
	ximapflags tFlags;
	ximapbodystructure tBody;
	uint32 iCaps = 0u;
	str sRaw = NULL;
	str sLiteral = NULL;
	str sIdleEvent = NULL;
	str sHeader = NULL;
	str* arrIdleEvents = NULL;
	size_t iIdleEventCount = 0u;
	ximap_idle_watch_state tIdleWatch;
	size_t iIdleWatchCount = 0u;
	const char* arrHeaderFields[2];
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("mock_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("mock_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	xrtImapResultInit(&tRet);
	memset(&tList, 0, sizeof(tList));
	memset(&tMailboxStatus, 0, sizeof(tMailboxStatus));
	memset(&tExpunge, 0, sizeof(tExpunge));
	memset(&tSearchIds, 0, sizeof(tSearchIds));

	if ( !xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		iFailed = ximap_test_fail("mock_open", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapCapability(pClient, &iCaps, &tRet) || !(iCaps & XIMAP_CAP_IMAP4REV1)
		|| !(iCaps & XIMAP_CAP_STARTTLS) || !(iCaps & XIMAP_CAP_UIDPLUS) || !(iCaps & XIMAP_CAP_XOAUTH2) ) {
		iFailed = ximap_test_fail("mock_capability", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapStatusRaw(pClient, "INBOX", &sRaw, &tRet) || strstr((const char*)sRaw, "* STATUS \"INBOX\"") == NULL ) {
		iFailed = ximap_test_fail("mock_status_raw", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapStatus(pClient, "INBOX", &tMailboxStatus, &tRet)
		|| tMailboxStatus.iExists != 2u || tMailboxStatus.iRecent != 1u || tMailboxStatus.iUnseen != 1u
		|| tMailboxStatus.iUidValidity != 777u || tMailboxStatus.iUidNext != 103u ) {
		iFailed = ximap_test_fail("mock_status", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapSelect(pClient, "INBOX", &sRaw, &tRet) || strstr((const char*)sRaw, "* 2 EXISTS") == NULL ) {
		iFailed = ximap_test_fail("mock_select", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapSelectStatus(pClient, "INBOX", &tMailboxStatus, &tRet)
		|| tMailboxStatus.iExists != 2u || tMailboxStatus.iRecent != 1u
		|| tMailboxStatus.iUidValidity != 777u || tMailboxStatus.iUidNext != 103u
		|| !tMailboxStatus.bReadWrite || tMailboxStatus.bReadOnly ) {
		iFailed = ximap_test_fail("mock_select_status", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapExamine(pClient, "INBOX", &sRaw, &tRet) || strstr((const char*)sRaw, "READ-ONLY") == NULL ) {
		iFailed = ximap_test_fail("mock_examine", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapExamineStatus(pClient, "INBOX", &tMailboxStatus, &tRet)
		|| tMailboxStatus.iExists != 2u || tMailboxStatus.iRecent != 0u
		|| tMailboxStatus.iUidValidity != 777u || tMailboxStatus.iUidNext != 103u
		|| !tMailboxStatus.bReadOnly || tMailboxStatus.bReadWrite ) {
		iFailed = ximap_test_fail("mock_examine_status", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapListRaw(pClient, "", "*", &sRaw, &tRet) || strstr((const char*)sRaw, "\"INBOX\"") == NULL ) {
		iFailed = ximap_test_fail("mock_list", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapList(pClient, "", "*", &tList, &tRet)
		|| tList.iCount != 1u || strcmp(tList.arrItems[0].sMailbox, "INBOX") != 0
		|| strcmp(tList.arrItems[0].sDelimiter, "/") != 0 || !tList.arrItems[0].bHasNoChildren ) {
		iFailed = ximap_test_fail("mock_list_parse", tRet.sError);
		goto cleanup;
	}
	xrtImapListFree(&tList);
	if ( !xrtImapSearchRaw(pClient, "ALL", &sRaw, &tRet) || strstr((const char*)sRaw, "* SEARCH 1 2") == NULL ) {
		iFailed = ximap_test_fail("mock_search", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapSearch(pClient, "ALL", &tSearchIds, &tRet)
		|| tSearchIds.iCount != 2u || tSearchIds.arrIds[0] != 1u || tSearchIds.arrIds[1] != 2u ) {
		iFailed = ximap_test_fail("mock_search_ids", tRet.sError);
		goto cleanup;
	}
	xrtImapSearchIdsFree(&tSearchIds);
	if ( !xrtImapUidSearchRaw(pClient, "ALL", &sRaw, &tRet) || strstr((const char*)sRaw, "* SEARCH 101 102") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_search", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidSearch(pClient, "ALL", &tSearchIds, &tRet)
		|| tSearchIds.iCount != 2u || tSearchIds.arrIds[0] != 101u || tSearchIds.arrIds[1] != 102u ) {
		iFailed = ximap_test_fail("mock_uid_search_ids", tRet.sError);
		goto cleanup;
	}
	xrtImapSearchIdsFree(&tSearchIds);
	if ( !xrtImapFetchRaw(pClient, "1", "FLAGS", &sRaw, &tRet) || strstr((const char*)sRaw, "FLAGS") == NULL ) {
		iFailed = ximap_test_fail("mock_fetch", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapFetchFlags(pClient, "1", &tFlags, &tRet) || !tFlags.bSeen || !tFlags.bAnswered || tFlags.bDeleted ) {
		iFailed = ximap_test_fail("mock_fetch_flags", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapFetchBodyStructureRaw(pClient, "1", &sRaw, &tRet) || strstr((const char*)sRaw, "BODYSTRUCTURE") == NULL
		|| strstr((const char*)sRaw, "ALTERNATIVE") == NULL ) {
		iFailed = ximap_test_fail("mock_bodystructure_raw", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	memset(&tBody, 0, sizeof(tBody));
	if ( !xrtImapFetchBodyStructure(pClient, "1", &tBody, &tRet) || !tBody.bMultipart
		|| tBody.iPartCount != 2u || strcmp(tBody.sType, "MULTIPART") != 0 || strcmp(tBody.sSubType, "ALTERNATIVE") != 0
		|| tBody.iParamCount != 1u || strcmp(tBody.arrParams[0].sName, "BOUNDARY") != 0
		|| strcmp(tBody.arrParams[0].sValue, "alt-boundary") != 0
		|| strcmp(tBody.sDisposition, "INLINE") != 0 || tBody.iDispositionParamCount != 1u
		|| strcmp(tBody.arrDispositionParams[0].sName, "FILENAME") != 0
		|| strcmp(tBody.arrDispositionParams[0].sValue, "root.eml") != 0
		|| strcmp(tBody.sLanguage, "en") != 0 || strcmp(tBody.sLocation, "root-part") != 0
		|| tBody.arrChildren == NULL
		|| strcmp(tBody.arrChildren[0].sType, "TEXT") != 0 || strcmp(tBody.arrChildren[0].sSubType, "PLAIN") != 0
		|| tBody.arrChildren[0].iParamCount != 1u || strcmp(tBody.arrChildren[0].arrParams[0].sName, "CHARSET") != 0
		|| strcmp(tBody.arrChildren[0].arrParams[0].sValue, "UTF-8") != 0
		|| strcmp(tBody.arrChildren[0].sId, "plain-id") != 0 || strcmp(tBody.arrChildren[0].sDescription, "plain description") != 0
		|| strcmp(tBody.arrChildren[0].sEncoding, "7BIT") != 0 || tBody.arrChildren[0].iOctets != 4u || tBody.arrChildren[0].iLines != 1u
		|| strcmp(tBody.arrChildren[0].sMd5, "plain-md5") != 0
		|| strcmp(tBody.arrChildren[0].sDisposition, "INLINE") != 0 || tBody.arrChildren[0].iDispositionParamCount != 1u
		|| strcmp(tBody.arrChildren[0].arrDispositionParams[0].sName, "FILENAME") != 0
		|| strcmp(tBody.arrChildren[0].arrDispositionParams[0].sValue, "plain.txt") != 0
		|| strcmp(tBody.arrChildren[0].sLanguage, "en") != 0 || strcmp(tBody.arrChildren[0].sLocation, "plain-part") != 0
		|| strcmp(tBody.arrChildren[1].sType, "TEXT") != 0 || strcmp(tBody.arrChildren[1].sSubType, "HTML") != 0
		|| tBody.arrChildren[1].iParamCount != 1u || strcmp(tBody.arrChildren[1].arrParams[0].sName, "CHARSET") != 0
		|| strcmp(tBody.arrChildren[1].arrParams[0].sValue, "UTF-8") != 0
		|| strcmp(tBody.arrChildren[1].sId, "html-id") != 0 || strcmp(tBody.arrChildren[1].sDescription, "html description") != 0
		|| strcmp(tBody.arrChildren[1].sEncoding, "7BIT") != 0 || tBody.arrChildren[1].iOctets != 11u || tBody.arrChildren[1].iLines != 1u
		|| strcmp(tBody.arrChildren[1].sMd5, "html-md5") != 0
		|| strcmp(tBody.arrChildren[1].sDisposition, "INLINE") != 0 || tBody.arrChildren[1].iDispositionParamCount != 1u
		|| strcmp(tBody.arrChildren[1].arrDispositionParams[0].sName, "FILENAME") != 0
		|| strcmp(tBody.arrChildren[1].arrDispositionParams[0].sValue, "html.html") != 0
		|| strcmp(tBody.arrChildren[1].sLanguage, "en") != 0 || strcmp(tBody.arrChildren[1].sLocation, "html-part") != 0 ) {
		xrtImapBodyStructureFree(&tBody);
		iFailed = ximap_test_fail("mock_bodystructure_parse", tRet.sError);
		goto cleanup;
	}
	xrtImapBodyStructureFree(&tBody);
	memset(&tBody, 0, sizeof(tBody));
	if ( !xrtImapFetchBodyStructure(pClient, "2", &tBody, &tRet)
		|| strcmp(tBody.sType, "MESSAGE") != 0 || strcmp(tBody.sSubType, "RFC822") != 0
		|| tBody.iOctets != 345u || tBody.iMessageLines != 20u
		|| tBody.sEnvelope == NULL || strstr((const char*)tBody.sEnvelope, "fwd subject") == NULL
		|| tBody.pMessageBody == NULL || strcmp(tBody.pMessageBody->sType, "TEXT") != 0
		|| strcmp(tBody.pMessageBody->sSubType, "PLAIN") != 0
		|| tBody.pMessageBody->iLines != 1u
		|| strcmp(tBody.sMd5, "msg-md5") != 0
		|| strcmp(tBody.sDisposition, "ATTACHMENT") != 0
		|| tBody.iDispositionParamCount != 1u
		|| strcmp(tBody.arrDispositionParams[0].sName, "FILENAME") != 0
		|| strcmp(tBody.arrDispositionParams[0].sValue, "forwarded.eml") != 0
		|| strcmp(tBody.sLanguage, "en") != 0 || strcmp(tBody.sLocation, "msg-loc") != 0 ) {
		xrtImapBodyStructureFree(&tBody);
		iFailed = ximap_test_fail("mock_bodystructure_message_rfc822", tRet.sError);
		goto cleanup;
	}
	xrtImapBodyStructureFree(&tBody);
	memset(&tBody, 0, sizeof(tBody));
	if ( !xrtImapFetchBodyStructure(pClient, "3", &tBody, &tRet)
		|| strcmp(tBody.sType, "APPLICATION") != 0 || strcmp(tBody.sSubType, "PDF") != 0
		|| tBody.iParamCount != 1u || strcmp(tBody.arrParams[0].sName, "NAME") != 0
		|| strcmp(tBody.arrParams[0].sValue, "report.pdf") != 0
		|| strcmp(tBody.sId, "pdf-id") != 0 || strcmp(tBody.sDescription, "pdf description") != 0
		|| strcmp(tBody.sEncoding, "BASE64") != 0 || tBody.iOctets != 2048u || tBody.iLines != 0u
		|| strcmp(tBody.sMd5, "pdf-md5") != 0
		|| strcmp(tBody.sDisposition, "ATTACHMENT") != 0 || tBody.iDispositionParamCount != 2u
		|| strcmp(tBody.arrDispositionParams[0].sName, "FILENAME") != 0
		|| strcmp(tBody.arrDispositionParams[0].sValue, "report.pdf") != 0
		|| strcmp(tBody.arrDispositionParams[1].sName, "SIZE") != 0
		|| strcmp(tBody.arrDispositionParams[1].sValue, "2048") != 0
		|| strcmp(tBody.sLocation, "pdf-loc") != 0 ) {
		xrtImapBodyStructureFree(&tBody);
		iFailed = ximap_test_fail("mock_bodystructure_application", tRet.sError);
		goto cleanup;
	}
	xrtImapBodyStructureFree(&tBody);
	if ( !xrtImapUidFetchBodyRaw(pClient, "101", &sRaw, &tRet) || strstr((const char*)sRaw, "Subject: hi") == NULL
		|| strstr((const char*)sRaw, "BODY[] {") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_fetch_body", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidFetchBodyLiteral(pClient, "101", &sLiteral, &tRet)
		|| strcmp((const char*)sLiteral, "Subject: hi\r\nFrom: sender@example.com\r\n\r\nbody\r\n") != 0 ) {
		iFailed = ximap_test_fail("mock_uid_fetch_body_literal", sLiteral ? (const char*)sLiteral : tRet.sError);
		goto cleanup;
	}
	xrtFree(sLiteral);
	sLiteral = NULL;
	if ( !xrtImapUidPeekBodyRaw(pClient, "101", &sRaw, &tRet) || strstr((const char*)sRaw, "Subject: hi") == NULL
		|| strstr((const char*)sRaw, "BODY.PEEK[] {") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_peek_body", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidPeekBodyLiteral(pClient, "101", &sLiteral, &tRet)
		|| strcmp((const char*)sLiteral, "Subject: hi\r\nFrom: sender@example.com\r\n\r\nbody\r\n") != 0 ) {
		iFailed = ximap_test_fail("mock_uid_peek_body_literal", sLiteral ? (const char*)sLiteral : tRet.sError);
		goto cleanup;
	}
	xrtFree(sLiteral);
	sLiteral = NULL;
	if ( !xrtImapUidFetchBodySectionRaw(pClient, "101", "HEADER", &sRaw, &tRet) || strstr((const char*)sRaw, "BODY[HEADER] {") == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_fetch_body_section", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidPeekBodySectionRaw(pClient, "101", "HEADER", &sRaw, &tRet) || strstr((const char*)sRaw, "BODY.PEEK[HEADER] {") == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_peek_body_section", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidFetchBodySectionRaw(pClient, "101", "1.MIME", &sRaw, &tRet) || strstr((const char*)sRaw, "BODY[1.MIME] {") == NULL
		|| strstr((const char*)sRaw, "Content-Type: text/plain") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_fetch_body_section_mime", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidPeekBodySectionRaw(pClient, "101", "1.HEADER.FIELDS (Subject From)", &sRaw, &tRet)
		|| strstr((const char*)sRaw, "BODY.PEEK[1.HEADER.FIELDS (Subject From)] {") == NULL
		|| strstr((const char*)sRaw, "Subject: part hi") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_peek_body_section_header_fields", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( xrtImapUidFetchBodySectionRaw(pClient, "101", "HEADER FIELDS", &sRaw, &tRet) ) {
		iFailed = ximap_test_fail("body_section_reject_invalid", "unsafe section accepted");
		goto cleanup;
	}
	if ( xrtImapUidFetchBodySectionRaw(pClient, "101", "1.HEADER.FIELDS (Subject\r\nBcc)", &sRaw, &tRet) ) {
		iFailed = ximap_test_fail("body_section_reject_injection", "unsafe section accepted");
		goto cleanup;
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
		sRaw = NULL;
	}
	arrHeaderFields[0] = "Subject";
	arrHeaderFields[1] = "From";
	if ( !xrtImapUidFetchHeaderFieldsRaw(pClient, "101", arrHeaderFields, 2u, &sRaw, &tRet)
		|| strstr((const char*)sRaw, "BODY[HEADER.FIELDS (Subject From)] {") == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_fetch_header_fields_raw", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidFetchHeaderFields(pClient, "101", arrHeaderFields, 2u, &sHeader, &tRet)
		|| strstr((const char*)sHeader, "Subject: hi") == NULL
		|| strstr((const char*)sHeader, "From: sender@example.com") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_fetch_header_fields", sHeader ? (const char*)sHeader : tRet.sError);
		goto cleanup;
	}
	xrtFree(sHeader);
	sHeader = NULL;
	if ( !xrtImapUidPeekHeaderFieldsRaw(pClient, "101", arrHeaderFields, 2u, &sRaw, &tRet)
		|| strstr((const char*)sRaw, "BODY.PEEK[HEADER.FIELDS (Subject From)] {") == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_peek_header_fields_raw", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidPeekHeaderFields(pClient, "101", arrHeaderFields, 2u, &sHeader, &tRet)
		|| strstr((const char*)sHeader, "Subject: hi") == NULL
		|| strstr((const char*)sHeader, "From: sender@example.com") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_peek_header_fields", sHeader ? (const char*)sHeader : tRet.sError);
		goto cleanup;
	}
	xrtFree(sHeader);
	sHeader = NULL;
	arrHeaderFields[0] = "Subject:";
	if ( xrtImapUidFetchHeaderFieldsRaw(pClient, "101", arrHeaderFields, 1u, &sRaw, &tRet) ) {
		iFailed = ximap_test_fail("header_fields_reject_invalid", "unsafe field accepted");
		goto cleanup;
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
		sRaw = NULL;
	}
	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xrtImapUidFetchMime(pClient, "101", &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| strcmp((const char*)tParsed.sBody, "body\r\n") != 0 ) {
		xmailMimeParsedMessageFree(&tParsed);
		iFailed = ximap_test_fail("mock_uid_fetch_mime", tRet.sError);
		goto cleanup;
	}
	xmailMimeParsedMessageFree(&tParsed);
	memset(&tParsed, 0, sizeof(tParsed));
	if ( !xrtImapUidPeekMime(pClient, "101", &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| strcmp((const char*)tParsed.sBody, "body\r\n") != 0 ) {
		xmailMimeParsedMessageFree(&tParsed);
		iFailed = ximap_test_fail("mock_uid_peek_mime", tRet.sError);
		goto cleanup;
	}
	xmailMimeParsedMessageFree(&tParsed);
	if ( !xrtImapStoreFlags(pClient, "1", "+FLAGS", "(\\Seen)", &sRaw, &tRet) || strstr((const char*)sRaw, "STORE completed") == NULL ) {
		iFailed = ximap_test_fail("mock_store", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapStoreFlagsParsed(pClient, "1", "-FLAGS", "(\\Seen)", &tFlags, &tRet)
		|| tFlags.bSeen || !tFlags.bAnswered ) {
		iFailed = ximap_test_fail("mock_store_flags_parsed", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapUidStoreFlags(pClient, "101", "+FLAGS", "(\\Deleted)", &sRaw, &tRet)
		|| strstr((const char*)sRaw, "UID STORE completed") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_store", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidStoreFlagsParsed(pClient, "101", "-FLAGS", "(\\Deleted)", &tFlags, &tRet)
		|| !tFlags.bSeen || tFlags.bDeleted ) {
		iFailed = ximap_test_fail("mock_uid_store_flags_parsed", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapExpungeRaw(pClient, &sRaw, &tRet) || strstr((const char*)sRaw, "* 1 EXPUNGE") == NULL
		|| strstr((const char*)sRaw, "EXPUNGE completed") == NULL ) {
		iFailed = ximap_test_fail("mock_expunge", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapExpunge(pClient, &tExpunge, &tRet)
		|| tExpunge.iCount != 1u || tExpunge.arrSequences[0] != 1u ) {
		iFailed = ximap_test_fail("mock_expunge_parse", tRet.sError);
		goto cleanup;
	}
	xrtImapExpungeResultFree(&tExpunge);
	if ( !xrtImapUidExpungeRaw(pClient, "101", &sRaw, &tRet) || strstr((const char*)sRaw, "* 2 EXPUNGE") == NULL
		|| strstr((const char*)sRaw, "UID EXPUNGE completed") == NULL ) {
		iFailed = ximap_test_fail("mock_uid_expunge", sRaw ? (const char*)sRaw : tRet.sError);
		goto cleanup;
	}
	xrtFree(sRaw);
	sRaw = NULL;
	if ( !xrtImapUidExpunge(pClient, "101:*", &tExpunge, &tRet)
		|| tExpunge.iCount != 1u || tExpunge.arrSequences[0] != 2u ) {
		iFailed = ximap_test_fail("mock_uid_expunge_parse", tRet.sError);
		goto cleanup;
	}
	xrtImapExpungeResultFree(&tExpunge);
	if ( xrtImapUidExpungeRaw(pClient, "101 BAD", &sRaw, &tRet) ) {
		iFailed = ximap_test_fail("uid_expunge_reject_invalid", "unsafe uid set accepted");
		goto cleanup;
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
		sRaw = NULL;
	}
	if ( !xrtImapIdleProbe(pClient, &tRet) ) {
		iFailed = ximap_test_fail("mock_idle_probe", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapIdleOnce(pClient, &sIdleEvent, &tRet) || strcmp((const char*)sIdleEvent, "* 3 EXISTS") != 0 ) {
		iFailed = ximap_test_fail("mock_idle_once", sIdleEvent ? (const char*)sIdleEvent : tRet.sError);
		goto cleanup;
	}
	xrtFree(sIdleEvent);
	sIdleEvent = NULL;
	if ( !xrtImapIdleCollect(pClient, 2u, &arrIdleEvents, &iIdleEventCount, &tRet)
		|| iIdleEventCount != 2u || strcmp((const char*)arrIdleEvents[0], "* 3 EXISTS") != 0
		|| strcmp((const char*)arrIdleEvents[1], "* 4 EXPUNGE") != 0 ) {
		iFailed = ximap_test_fail("mock_idle_collect", (arrIdleEvents && iIdleEventCount > 0u) ? (const char*)arrIdleEvents[0] : tRet.sError);
		goto cleanup;
	}
	xrtImapIdleEventsFree(arrIdleEvents, iIdleEventCount);
	arrIdleEvents = NULL;
	iIdleEventCount = 0u;
	memset(&tIdleWatch, 0, sizeof(tIdleWatch));
	if ( !xrtImapIdleWatch(pClient, 0u, ximap_test_idle_watch_callback, &tIdleWatch, &iIdleWatchCount, &tRet)
		|| iIdleWatchCount != 1u || tIdleWatch.iCount != 1u || strcmp(tIdleWatch.sFirst, "* 3 EXISTS") != 0 ) {
		iFailed = ximap_test_fail("mock_idle_watch_cancel", tIdleWatch.sFirst[0] ? tIdleWatch.sFirst : tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, &tRet) ) {
		pClient = NULL;
		iFailed = ximap_test_fail("mock_logout", tRet.sError);
		goto cleanup;
	}
	pClient = NULL;
	printf("PASS mock_server_flow\n");

cleanup:
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	xrtImapListFree(&tList);
	xrtImapExpungeResultFree(&tExpunge);
	xrtImapSearchIdsFree(&tSearchIds);
	if ( sLiteral != NULL ) {
		xrtFree(sLiteral);
	}
	if ( sIdleEvent != NULL ) {
		xrtFree(sIdleEvent);
	}
	if ( sHeader != NULL ) {
		xrtFree(sHeader);
	}
	xrtImapIdleEventsFree(arrIdleEvents, iIdleEventCount);
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_pipeline_out_of_order(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	str sCapability = NULL;
	str sStatus = NULL;
	char sCapTag[32];
	char sStatusTag[32];
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("pipeline_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_pipeline_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("pipeline_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	xrtImapResultInit(&tRet);

	if ( !xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		iFailed = ximap_test_fail("pipeline_open", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapCommandBegin(pClient, "CAPABILITY", XIMAP_STAGE_CAPABILITY, sCapTag, sizeof(sCapTag), &tRet)
		|| !xrtImapCommandBegin(pClient, "STATUS \"INBOX\" (MESSAGES)", XIMAP_STAGE_COMMAND, sStatusTag, sizeof(sStatusTag), &tRet) ) {
		iFailed = ximap_test_fail("pipeline_begin", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapCommandFinish(pClient, sCapTag, &sCapability, &tRet)
		|| strstr((const char*)sCapability, "* CAPABILITY IMAP4rev1 IDLE UIDPLUS") == NULL ) {
		iFailed = ximap_test_fail("pipeline_finish_first", sCapability ? (const char*)sCapability : tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapCommandFinish(pClient, sStatusTag, &sStatus, &tRet)
		|| strstr((const char*)sStatus, "* STATUS \"INBOX\"") == NULL ) {
		iFailed = ximap_test_fail("pipeline_finish_pending", sStatus ? (const char*)sStatus : tRet.sError);
		goto cleanup;
	}
	printf("PASS pipeline_out_of_order\n");

cleanup:
	if ( sCapability != NULL ) xrtFree(sCapability);
	if ( sStatus != NULL ) xrtFree(sStatus);
	if ( pClient != NULL ) xrtImapClose(pClient);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_mock_login_flow(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("mock_login_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("mock_login_server_thread", "thread create failed");
	}
	xrtImapConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	xrtImapResultInit(&tRet);
	if ( !xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		iFailed = ximap_test_fail("mock_login_open", tRet.sError);
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, &tRet) ) {
		pClient = NULL;
		iFailed = ximap_test_fail("mock_login_logout", tRet.sError);
		goto cleanup;
	}
	pClient = NULL;
	printf("PASS mock_login_flow\n");

cleanup:
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_fetch_raw(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	str sRaw = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_raw_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_raw_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-raw";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidFetchBodyRawAsyncWait(&tCfg, "101", &tOpts, &sRaw, &tRet)
		|| sRaw == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL
		|| strstr((const char*)sRaw, "UID FETCH completed") == NULL ) {
		iFailed = ximap_test_fail("async_fetch_raw", sRaw ? (const char*)sRaw : tRet.sError);
	} else {
		printf("PASS async_fetch_raw\n");
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_fetch_literal(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	str sLiteral = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_literal_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_literal_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-literal";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidFetchBodyLiteralAsyncWait(&tCfg, "101", &tOpts, &sLiteral, &tRet)
		|| sLiteral == NULL
		|| strcmp((const char*)sLiteral, "Subject: hi\r\nFrom: sender@example.com\r\n\r\nbody\r\n") != 0 ) {
		iFailed = ximap_test_fail("async_fetch_literal", sLiteral ? (const char*)sLiteral : tRet.sError);
	} else {
		printf("PASS async_fetch_literal\n");
	}
	if ( sLiteral != NULL ) {
		xrtFree(sLiteral);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_peek_raw(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	str sRaw = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_peek_raw_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_peek_raw_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-peek-raw";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidPeekBodyRawAsyncWait(&tCfg, "101", &tOpts, &sRaw, &tRet)
		|| sRaw == NULL
		|| strstr((const char*)sRaw, "BODY.PEEK[] {") == NULL
		|| strstr((const char*)sRaw, "Subject: hi") == NULL ) {
		iFailed = ximap_test_fail("async_peek_raw", sRaw ? (const char*)sRaw : tRet.sError);
	} else {
		printf("PASS async_peek_raw\n");
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_status(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapmailboxstatus tStatus;
	int iFailed = 0;

	memset(&tStatus, 0, sizeof(tStatus));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_status_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_status_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-status";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapStatusAsyncWait(&tCfg, "INBOX", &tOpts, &tStatus, &tRet)
		|| tStatus.iExists != 2u || tStatus.iRecent != 1u || tStatus.iUnseen != 1u
		|| tStatus.iUidValidity != 777u || tStatus.iUidNext != 103u ) {
		iFailed = ximap_test_fail("async_status", tRet.sError);
	} else {
		printf("PASS async_status\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_list(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximaplist tList;
	int iFailed = 0;

	memset(&tList, 0, sizeof(tList));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_list_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_list_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-list";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapListAsyncWait(&tCfg, "", "*", &tOpts, &tList, &tRet)
		|| tList.iCount != 1u
		|| strcmp(tList.arrItems[0].sMailbox, "INBOX") != 0
		|| strcmp(tList.arrItems[0].sDelimiter, "/") != 0
		|| !tList.arrItems[0].bHasNoChildren ) {
		iFailed = ximap_test_fail("async_list", tRet.sError);
	} else {
		printf("PASS async_list\n");
	}
	xrtImapListFree(&tList);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_uid_search(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapsearchids tIds;
	int iFailed = 0;

	memset(&tIds, 0, sizeof(tIds));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_uid_search_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_uid_search_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-uid-search";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidSearchAsyncWait(&tCfg, "ALL", &tOpts, &tIds, &tRet)
		|| tIds.iCount != 2u || tIds.arrIds[0] != 101u || tIds.arrIds[1] != 102u ) {
		iFailed = ximap_test_fail("async_uid_search", tRet.sError);
	} else {
		printf("PASS async_uid_search\n");
	}
	xrtImapSearchIdsFree(&tIds);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_bodystructure(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapbodystructure tBody;
	int iFailed = 0;

	memset(&tBody, 0, sizeof(tBody));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_bodystructure_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_bodystructure_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-bodystructure";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapFetchBodyStructureAsyncWait(&tCfg, "1", &tOpts, &tBody, &tRet)
		|| !tBody.bMultipart || tBody.iPartCount != 2u
		|| strcmp(tBody.sSubType, "ALTERNATIVE") != 0
		|| tBody.arrChildren == NULL
		|| strcmp(tBody.arrChildren[0].sSubType, "PLAIN") != 0
		|| strcmp(tBody.arrChildren[1].sSubType, "HTML") != 0 ) {
		iFailed = ximap_test_fail("async_bodystructure", tRet.sError);
	} else {
		printf("PASS async_bodystructure\n");
	}
	xrtImapBodyStructureFree(&tBody);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_bodystructure_message_rfc822(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapbodystructure tBody;
	int iFailed = 0;

	memset(&tBody, 0, sizeof(tBody));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_bodystructure_msg_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_bodystructure_msg_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-bodystructure-msg";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapFetchBodyStructureAsyncWait(&tCfg, "2", &tOpts, &tBody, &tRet)
		|| strcmp(tBody.sType, "MESSAGE") != 0
		|| strcmp(tBody.sSubType, "RFC822") != 0
		|| tBody.sEnvelope == NULL
		|| strstr((const char*)tBody.sEnvelope, "fwd subject") == NULL
		|| tBody.pMessageBody == NULL
		|| strcmp(tBody.pMessageBody->sSubType, "PLAIN") != 0
		|| tBody.iMessageLines != 20u ) {
		iFailed = ximap_test_fail("async_bodystructure_message_rfc822", tRet.sError);
	} else {
		printf("PASS async_bodystructure_message_rfc822\n");
	}
	xrtImapBodyStructureFree(&tBody);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_bodystructure_application(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapbodystructure tBody;
	int iFailed = 0;

	memset(&tBody, 0, sizeof(tBody));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_bodystructure_app_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_bodystructure_app_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-bodystructure-app";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapFetchBodyStructureAsyncWait(&tCfg, "3", &tOpts, &tBody, &tRet)
		|| strcmp(tBody.sType, "APPLICATION") != 0
		|| strcmp(tBody.sSubType, "PDF") != 0
		|| tBody.iOctets != 2048u
		|| strcmp(tBody.sDisposition, "ATTACHMENT") != 0
		|| tBody.iDispositionParamCount != 2u
		|| strcmp(tBody.sLocation, "pdf-loc") != 0 ) {
		iFailed = ximap_test_fail("async_bodystructure_application", tRet.sError);
	} else {
		printf("PASS async_bodystructure_application\n");
	}
	xrtImapBodyStructureFree(&tBody);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_fetch_flags(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapflags tFlags;
	int iFailed = 0;

	memset(&tFlags, 0, sizeof(tFlags));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_flags_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_flags_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-flags";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapFetchFlagsAsyncWait(&tCfg, "1", &tOpts, &tFlags, &tRet)
		|| !tFlags.bSeen || !tFlags.bAnswered || tFlags.bDeleted
		|| strstr(tFlags.sRaw, "\\Seen") == NULL ) {
		iFailed = ximap_test_fail("async_fetch_flags", tRet.sError);
	} else {
		printf("PASS async_fetch_flags\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_store_flags(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapflags tFlags;
	int iFailed = 0;

	memset(&tFlags, 0, sizeof(tFlags));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_store_flags_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_store_flags_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-store";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidStoreFlagsAsyncWait(&tCfg, "101", "+FLAGS", "(\\Deleted)", &tOpts, &tFlags, &tRet)
		|| !tFlags.bSeen || !tFlags.bDeleted ) {
		iFailed = ximap_test_fail("async_store_flags", tRet.sError);
	} else {
		printf("PASS async_store_flags\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_uid_expunge(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximapexpungeresult tExpunge;
	int iFailed = 0;

	memset(&tExpunge, 0, sizeof(tExpunge));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_uid_expunge_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_uid_expunge_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-expunge";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidExpungeAsyncWait(&tCfg, "101:*", &tOpts, &tExpunge, &tRet)
		|| tExpunge.iCount != 1u || tExpunge.arrSequences[0] != 2u ) {
		iFailed = ximap_test_fail("async_uid_expunge", tRet.sError);
	} else {
		printf("PASS async_uid_expunge\n");
	}
	xrtImapExpungeResultFree(&tExpunge);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_idle_collect(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	str* arrEvents = NULL;
	size_t iCount = 0u;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_idle_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_idle_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-idle";
	tOpts.iTimeoutMs = 5000u;
	tOpts.sMailbox = "INBOX";

	if ( !xrtImapIdleCollectAsyncWait(&tCfg, 2u, &tOpts, &arrEvents, &iCount, &tRet)
		|| iCount != 2u
		|| arrEvents == NULL
		|| strcmp((const char*)arrEvents[0], "* 3 EXISTS") != 0
		|| strcmp((const char*)arrEvents[1], "* 4 EXPUNGE") != 0 ) {
		iFailed = ximap_test_fail("async_idle_collect", tRet.sError);
	} else {
		printf("PASS async_idle_collect\n");
	}
	xrtImapIdleEventsFree(arrEvents, iCount);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_idle_watch(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	ximap_idle_watch_state tState;
	size_t iCount = 0u;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_idle_watch_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_idle_watch_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	xrtImapResultInit(&tRet);
	memset(&tState, 0, sizeof(tState));
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-idle-watch";
	tOpts.iTimeoutMs = 5000u;
	tOpts.sMailbox = "INBOX";

	if ( !xrtImapIdleWatchAsyncWait(&tCfg, 0u, ximap_test_idle_watch_callback, &tState, &tOpts, &iCount, &tRet)
		|| iCount != 1u || tState.iCount != 1u || strcmp(tState.sFirst, "* 3 EXISTS") != 0 ) {
		iFailed = ximap_test_fail("async_idle_watch", tRet.sError);
	} else {
		printf("PASS async_idle_watch\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_idle_loop(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapidleloopopts tOpts;
	ximapresult tRet;
	ximapidleloopsummary tSummary;
	ximap_idle_watch_state tState;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("idle_loop_server_start", "listen failed");
	}
	tServer.iMaxConnections = 2u;
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("idle_loop_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapIdleLoopOptsInit(&tOpts);
	xrtImapResultInit(&tRet);
	memset(&tSummary, 0, sizeof(tSummary));
	memset(&tState, 0, sizeof(tState));
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sMailbox = "INBOX";
	tOpts.iMaxCycles = 2u;
	tOpts.iMaxEventsPerCycle = 1u;
	tOpts.iReconnectDelayMs = 0u;
	tOpts.iMaxReconnectDelayMs = 0u;
	tOpts.iHeartbeatEveryCycles = 1u;
	tOpts.pTrace = ximap_test_idle_loop_trace;
	tOpts.pTraceUser = &tState;
	tOpts.pHeartbeat = ximap_test_idle_loop_heartbeat;
	tOpts.pHeartbeatUser = &tState;

	if ( !xrtImapIdleLoop(&tCfg, &tOpts, ximap_test_idle_loop_callback, &tState, &tSummary, &tRet)
		|| tSummary.iCycles != 2u || tSummary.iEvents != 2u || tSummary.bCancelled || tSummary.bLastCycleFailed
		|| tSummary.iHeartbeats != 2u || tSummary.bHeartbeatStopped
		|| tState.iCount != 2u || tState.iHeartbeatCount != 2u || tState.iTraceCount < 7u || strcmp(tState.sFirst, "* 3 EXISTS") != 0
		|| strcmp(tState.sLastTrace, XIMAP_IDLE_LOOP_EVENT_DONE) != 0 ) {
		iFailed = ximap_test_fail("idle_loop", tRet.sError);
	} else {
		printf("PASS idle_loop\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_idle_loop_stop_callback(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapidleloopopts tOpts;
	ximapresult tRet;
	ximapidleloopsummary tSummary;
	ximap_idle_watch_state tState;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("idle_loop_stop_server_start", "listen failed");
	}
	tServer.iMaxConnections = 1u;
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("idle_loop_stop_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapIdleLoopOptsInit(&tOpts);
	xrtImapResultInit(&tRet);
	memset(&tSummary, 0, sizeof(tSummary));
	memset(&tState, 0, sizeof(tState));
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sMailbox = "INBOX";
	tOpts.iMaxCycles = 5u;
	tOpts.iMaxEventsPerCycle = 1u;
	tOpts.iReconnectDelayMs = 0u;
	tOpts.iMaxReconnectDelayMs = 0u;
	tOpts.pShouldStop = ximap_test_idle_loop_should_stop;
	tOpts.pStopUser = &tState;
	tOpts.pTrace = ximap_test_idle_loop_trace;
	tOpts.pTraceUser = &tState;

	if ( !xrtImapIdleLoop(&tCfg, &tOpts, ximap_test_idle_loop_callback, &tState, &tSummary, &tRet)
		|| tSummary.iCycles != 1u || tSummary.iEvents != 1u || !tSummary.bStopped
		|| tSummary.bCancelled || tSummary.bLastCycleFailed || tState.iCount != 1u
		|| tState.iTraceCount < 4u || strcmp(tState.sLastTrace, XIMAP_IDLE_LOOP_EVENT_DONE) != 0 ) {
		iFailed = ximap_test_fail("idle_loop_stop", tRet.sError);
	} else {
		printf("PASS idle_loop_stop\n");
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_header_fields(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	const char* arrFields[2];
	str sHeader = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_headers_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_headers_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-headers";
	tOpts.iTimeoutMs = 5000u;
	arrFields[0] = "Subject";
	arrFields[1] = "From";

	if ( !xrtImapUidFetchHeaderFieldsAsyncWait(&tCfg, "101", arrFields, 2u, &tOpts, &sHeader, &tRet)
		|| sHeader == NULL
		|| strstr((const char*)sHeader, "Subject: hi") == NULL
		|| strstr((const char*)sHeader, "From: sender@example.com") == NULL ) {
		iFailed = ximap_test_fail("async_header_fields", sHeader ? (const char*)sHeader : tRet.sError);
	} else {
		printf("PASS async_header_fields\n");
	}
	if ( sHeader != NULL ) {
		xrtFree(sHeader);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_body_section(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	str sRaw = NULL;
	int iFailed = 0;

	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_section_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_section_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sOAuth2Token = "token";
	tOpts.sDebugName = "ximap-test-section";
	tOpts.iTimeoutMs = 5000u;

	if ( !xrtImapUidPeekBodySectionRawAsyncWait(&tCfg, "101", "1.HEADER.FIELDS (Subject From)", &tOpts, &sRaw, &tRet)
		|| sRaw == NULL
		|| strstr((const char*)sRaw, "BODY.PEEK[1.HEADER.FIELDS (Subject From)] {") == NULL
		|| strstr((const char*)sRaw, "Subject: part hi") == NULL ) {
		iFailed = ximap_test_fail("async_body_section", sRaw ? (const char*)sRaw : tRet.sError);
	} else {
		printf("PASS async_body_section\n");
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_fetch_mime_select(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	xmailparsedmessage tParsed;
	int iFailed = 0;

	memset(&tParsed, 0, sizeof(tParsed));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_mime_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_mime_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "ximap-test-mime";
	tOpts.iTimeoutMs = 5000u;
	tOpts.sMailbox = "INBOX";

	if ( !xrtImapUidFetchMimeAsyncWait(&tCfg, "101", &tOpts, &tParsed, &tRet)
		|| strcmp(xmailMimeParsedGetHeader(&tParsed, "Subject"), "hi") != 0
		|| strcmp((const char*)tParsed.sBody, "body\r\n") != 0 ) {
		iFailed = ximap_test_fail("async_fetch_mime", tRet.sError);
	} else {
		printf("PASS async_fetch_mime_select\n");
	}
	xmailMimeParsedMessageFree(&tParsed);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

static int ximap_test_async_fetch_multipart_mime_select(void)
{
	ximap_mock_server tServer;
	xthread pThread;
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	ximapresult tRet;
	xmailparsedmessage tParsed;
	xmailmimepart* pRoot;
	int iFailed = 0;

	memset(&tParsed, 0, sizeof(tParsed));
	if ( !ximap_mock_server_start(&tServer) ) {
		return ximap_test_fail("async_multipart_mime_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)ximap_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		ximap_mock_server_stop(&tServer);
		return ximap_test_fail("async_multipart_mime_server_thread", "thread create failed");
	}

	xrtImapConfigInit(&tCfg);
	xrtImapAsyncOptsInit(&tOpts);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XIMAP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.sUser = "user";
	tCfg.sPass = "pass";
	tOpts.sDebugName = "ximap-test-multipart";
	tOpts.iTimeoutMs = 5000u;
	tOpts.sMailbox = "INBOX";

	if ( !xrtImapUidFetchMimeAsyncWait(&tCfg, "202", &tOpts, &tParsed, &tRet) ) {
		iFailed = ximap_test_fail("async_fetch_multipart_mime", tRet.sError);
	} else {
		pRoot = tParsed.pRootPart;
		if ( pRoot == NULL
			|| strcmp(pRoot->sMediaType, "multipart") != 0
			|| strcmp(pRoot->sSubType, "mixed") != 0
			|| pRoot->iChildCount != 2u
			|| strcmp(pRoot->arrChildren[0].sSubType, "alternative") != 0
			|| pRoot->arrChildren[0].iChildCount != 2u
			|| strcmp((const char*)pRoot->arrChildren[0].arrChildren[0].sBody, "plain body") != 0
			|| strcmp((const char*)pRoot->arrChildren[0].arrChildren[1].sBody, "<p>html body</p>") != 0
			|| !pRoot->arrChildren[1].bAttachment
			|| strcmp(pRoot->arrChildren[1].sFileName, "report.txt") != 0
			|| strcmp((const char*)pRoot->arrChildren[1].sBody, "file body") != 0 ) {
			iFailed = ximap_test_fail("async_fetch_multipart_mime_tree", "unexpected MIME tree");
		} else {
			printf("PASS async_fetch_multipart_mime_select\n");
		}
	}
	xmailMimeParsedMessageFree(&tParsed);
	ximap_mock_server_stop(&tServer);
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	return iFailed;
}

int main(void)
{
	int iFailed = 0;
	setvbuf(stdout, NULL, _IONBF, 0);
	ximap_test_link_helpers();
	printf("ximap_test\n");
	iFailed |= ximap_test_init_defaults();
	iFailed |= ximap_test_tagged_command();
	iFailed |= ximap_test_tagged_line();
	iFailed |= ximap_test_untagged_literal_capability();
	iFailed |= ximap_test_mock_server_flow();
	iFailed |= ximap_test_pipeline_out_of_order();
	iFailed |= ximap_test_mock_login_flow();
	iFailed |= ximap_test_async_fetch_raw();
	iFailed |= ximap_test_async_fetch_literal();
	iFailed |= ximap_test_async_peek_raw();
	iFailed |= ximap_test_async_status();
	iFailed |= ximap_test_async_list();
	iFailed |= ximap_test_async_uid_search();
	iFailed |= ximap_test_async_bodystructure();
	iFailed |= ximap_test_async_bodystructure_message_rfc822();
	iFailed |= ximap_test_async_bodystructure_application();
	iFailed |= ximap_test_async_fetch_flags();
	iFailed |= ximap_test_async_store_flags();
	iFailed |= ximap_test_async_uid_expunge();
	iFailed |= ximap_test_async_idle_collect();
	iFailed |= ximap_test_async_idle_watch();
	iFailed |= ximap_test_idle_loop();
	iFailed |= ximap_test_idle_loop_stop_callback();
	iFailed |= ximap_test_async_header_fields();
	iFailed |= ximap_test_async_body_section();
	iFailed |= ximap_test_async_fetch_mime_select();
	iFailed |= ximap_test_async_fetch_multipart_mime_select();
	if ( iFailed ) {
		printf("ximap_test: FAIL\n");
		return 1;
	}
	printf("ximap_test: PASS\n");
	return 0;
}
