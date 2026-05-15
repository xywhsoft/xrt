#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "xsmtp.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int xsmtp_mime_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xsmtp_mime_expect_contains(const char* sName, const char* sText, const char* sNeedle)
{
	if ( sText == NULL || sNeedle == NULL || strstr(sText, sNeedle) == NULL ) {
		return xsmtp_mime_fail(sName, sNeedle);
	}
	printf("PASS %s\n", sName);
	return 0;
}

static int xsmtp_mime_expect_not_contains(const char* sName, const char* sText, const char* sNeedle)
{
	if ( sText != NULL && sNeedle != NULL && strstr(sText, sNeedle) != NULL ) {
		return xsmtp_mime_fail(sName, sNeedle);
	}
	printf("PASS %s\n", sName);
	return 0;
}

typedef struct {
	xsocket hListen;
	uint16 iPort;
	bool bAdvertiseAuthPlain;
	bool bAdvertiseAuthLogin;
	bool bAdvertiseAuthXoauth2;
	bool bSawAuthPlain;
	bool bSawAuthLogin;
	bool bSawAuthXoauth2;
	bool bAuthLoginUserOK;
	bool bAuthLoginPassOK;
	bool bAuthXoauth2OK;
	bool bSawEhlo;
	bool bSawQuit;
	bool bSawMailFrom;
	bool bSawRcptTo;
	bool bSawDsnMailFrom;
	bool bSawDsnRcptTo;
	bool bSawData;
	bool bDataHasSubject;
	bool bDataHasBody;
	char sDataSample[1024];
} xsmtp_mock_server;

static void xsmtp_mock_close_socket(xsocket hSock)
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

static bool xsmtp_mock_send_all(xsocket hSock, const char* sText)
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

static bool xsmtp_mock_recv_line(xsocket hSock, char* sOut, size_t iOutCap)
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

static uint32 xsmtp_mock_server_thread(ptr pArg)
{
	xsmtp_mock_server* pServer = (xsmtp_mock_server*)pArg;
	xsocket hClient;
	char sLine[4096];
	int iAuthLoginStep = 0;

	hClient = accept(pServer->hListen, NULL, NULL);
	if ( hClient == XSOCKET_INVALID ) {
		return 1u;
	}
	if ( !xsmtp_mock_send_all(hClient, "220 xsmtp mock ready\r\n") ) {
		xsmtp_mock_close_socket(hClient);
		return 2u;
	}
	while ( xsmtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
		if ( iAuthLoginStep == 1 ) {
			pServer->bAuthLoginUserOK = (strcmp(sLine, "c2VuZGVyQGV4YW1wbGUuY29t") == 0);
			iAuthLoginStep = 2;
			xsmtp_mock_send_all(hClient, "334 UGFzc3dvcmQ6\r\n");
		} else if ( iAuthLoginStep == 2 ) {
			pServer->bAuthLoginPassOK = (strcmp(sLine, "cGFzc3dvcmQ=") == 0);
			pServer->bSawAuthLogin = TRUE;
			iAuthLoginStep = 0;
			xsmtp_mock_send_all(hClient, "235 auth ok\r\n");
		} else if ( strncmp(sLine, "EHLO ", 5) == 0 || strncmp(sLine, "HELO ", 5) == 0 ) {
			pServer->bSawEhlo = TRUE;
			if ( pServer->bAdvertiseAuthPlain && pServer->bAdvertiseAuthLogin && pServer->bAdvertiseAuthXoauth2 ) {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-AUTH PLAIN LOGIN XOAUTH2\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			} else if ( pServer->bAdvertiseAuthPlain && pServer->bAdvertiseAuthLogin ) {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-AUTH PLAIN LOGIN\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			} else if ( pServer->bAdvertiseAuthPlain ) {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-AUTH PLAIN\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			} else if ( pServer->bAdvertiseAuthLogin ) {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-AUTH LOGIN\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			} else if ( pServer->bAdvertiseAuthXoauth2 ) {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-AUTH XOAUTH2\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			} else {
				xsmtp_mock_send_all(hClient, "250-localhost\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-DSN\r\n250 SIZE 65536\r\n");
			}
		} else if ( strncmp(sLine, "AUTH PLAIN ", 11) == 0 ) {
			pServer->bSawAuthPlain = TRUE;
			xsmtp_mock_send_all(hClient, "235 auth ok\r\n");
		} else if ( strcmp(sLine, "AUTH LOGIN") == 0 ) {
			iAuthLoginStep = 1;
			xsmtp_mock_send_all(hClient, "334 VXNlcm5hbWU6\r\n");
		} else if ( strncmp(sLine, "AUTH XOAUTH2 ", 13) == 0 ) {
			pServer->bSawAuthXoauth2 = TRUE;
			pServer->bAuthXoauth2OK = (strcmp(sLine + 13, "dXNlcj1zZW5kZXJAZXhhbXBsZS5jb20BYXV0aD1CZWFyZXIgdG9rZW4tMTIzAQE=") == 0);
			xsmtp_mock_send_all(hClient, "235 auth ok\r\n");
		} else if ( strncmp(sLine, "MAIL FROM:", 10) == 0 ) {
			pServer->bSawMailFrom = TRUE;
			if ( strstr(sLine, " RET=HDRS") != NULL && strstr(sLine, " ENVID=trace-123") != NULL ) {
				pServer->bSawDsnMailFrom = TRUE;
			}
			xsmtp_mock_send_all(hClient, "250 sender ok\r\n");
		} else if ( strncmp(sLine, "RCPT TO:", 8) == 0 ) {
			pServer->bSawRcptTo = TRUE;
			if ( strstr(sLine, " NOTIFY=SUCCESS,FAILURE") != NULL && strstr(sLine, " ORCPT=rfc822;receiver@example.com") != NULL ) {
				pServer->bSawDsnRcptTo = TRUE;
			}
			xsmtp_mock_send_all(hClient, "250 recipient ok\r\n");
		} else if ( strcmp(sLine, "DATA") == 0 ) {
			pServer->bSawData = TRUE;
			xsmtp_mock_send_all(hClient, "354 end with dot\r\n");
			while ( xsmtp_mock_recv_line(hClient, sLine, sizeof(sLine)) ) {
				if ( strcmp(sLine, ".") == 0 ) {
					break;
				}
				if ( strlen(pServer->sDataSample) + strlen(sLine) + 2u < sizeof(pServer->sDataSample) ) {
					strcat(pServer->sDataSample, sLine);
					strcat(pServer->sDataSample, "\n");
				}
				if ( strstr(sLine, "Subject:") != NULL ) {
					pServer->bDataHasSubject = TRUE;
				}
				if ( strstr(sLine, "plain=20via=20mock") != NULL ) {
					pServer->bDataHasBody = TRUE;
				}
			}
			xsmtp_mock_send_all(hClient, "250 queued\r\n");
		} else if ( strcmp(sLine, "QUIT") == 0 ) {
			pServer->bSawQuit = TRUE;
			xsmtp_mock_send_all(hClient, "221 bye\r\n");
			break;
		} else {
			xsmtp_mock_send_all(hClient, "250 ok\r\n");
		}
	}
	xsmtp_mock_close_socket(hClient);
	return 0u;
}

static bool xsmtp_mock_server_start(xsmtp_mock_server* pServer)
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
		xsmtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	if ( listen(pServer->hListen, 1) != 0 ) {
		xsmtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	iAddrLen = sizeof(tAddr);
	if ( getsockname(pServer->hListen, (struct sockaddr*)&tAddr, &iAddrLen) != 0 ) {
		xsmtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
		return FALSE;
	}
	pServer->iPort = ntohs(tAddr.sin_port);
	return TRUE;
}

static void xsmtp_mock_server_stop(xsmtp_mock_server* pServer)
{
	if ( pServer != NULL && pServer->hListen != XSOCKET_INVALID ) {
		xsmtp_mock_close_socket(pServer->hListen);
		pServer->hListen = XSOCKET_INVALID;
	}
}

static int xsmtp_mime_test_build_message(void)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpaddr arrCc[1];
	xsmtpaddr arrBcc[1];
	xsmtpattachment arrAttachments[2];
	const char* arrHeaderNames[1];
	const char* arrHeaderValues[1];
	static const unsigned char arrLogo[] = { 0x89u, 'P', 'N', 'G', 0x0du, 0x0au };
	static const char sAttachment[] = "hello attachment\r\n";
	str sData;
	int iFailed = 0;

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "smtp.example.com";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender Name";
	tCfg.tReplyTo.sEmail = "reply@example.com";
	tCfg.tReplyTo.sName = "Reply Name";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";
	memset(arrCc, 0, sizeof(arrCc));
	arrCc[0].sEmail = "copy@example.com";
	arrCc[0].sName = "Copy";
	memset(arrBcc, 0, sizeof(arrBcc));
	arrBcc[0].sEmail = "hidden@example.com";
	arrBcc[0].sName = "Hidden";

	memset(arrAttachments, 0, sizeof(arrAttachments));
	arrAttachments[0].sFileName = "logo.png";
	arrAttachments[0].sContentType = "image/png";
	arrAttachments[0].sContentId = "logo@cid";
	arrAttachments[0].pData = arrLogo;
	arrAttachments[0].iDataLen = sizeof(arrLogo);
	arrAttachments[0].bInline = TRUE;
	arrAttachments[1].sFileName = "report.txt";
	arrAttachments[1].sContentType = "text/plain";
	arrAttachments[1].pData = sAttachment;
	arrAttachments[1].iDataLen = strlen(sAttachment);
	arrAttachments[1].bInline = FALSE;

	arrHeaderNames[0] = "X-XSMTP-Test";
	arrHeaderValues[0] = "mime integration";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1;
	tMsg.arrCc = arrCc;
	tMsg.iCcCount = 1;
	tMsg.arrBcc = arrBcc;
	tMsg.iBccCount = 1;
	tMsg.sSubject = "xsmtp mime 测试";
	tMsg.sTextBody = "plain body";
	tMsg.sHtmlBody = "<html><body><img src=\"cid:logo@cid\"><p>html body</p></body></html>";
	tMsg.arrAttachments = arrAttachments;
	tMsg.iAttachmentCount = 2;
	tMsg.arrHeaderNames = arrHeaderNames;
	tMsg.arrHeaderValues = arrHeaderValues;
	tMsg.iHeaderCount = 1;

	sData = xsmtp_build_message_data(&tCfg, &tMsg);
	if ( sData == NULL ) {
		return xsmtp_mime_fail("build_message", "xsmtp_build_message_data returned NULL");
	}

	iFailed |= xsmtp_mime_expect_contains("message_from", (const char*)sData, "From: Sender Name <sender@example.com>\r\n");
	iFailed |= xsmtp_mime_expect_contains("message_to", (const char*)sData, "To: Receiver <receiver@example.com>\r\n");
	iFailed |= xsmtp_mime_expect_contains("message_cc", (const char*)sData, "Cc: Copy <copy@example.com>\r\n");
	iFailed |= xsmtp_mime_expect_not_contains("message_bcc_hidden", (const char*)sData, "\r\nBcc:");
	iFailed |= xsmtp_mime_expect_contains("message_reply_to", (const char*)sData, "Reply-To: Reply Name <reply@example.com>\r\n");
	iFailed |= xsmtp_mime_expect_contains("message_custom_header", (const char*)sData, "X-XSMTP-Test: mime integration\r\n");
	iFailed |= xsmtp_mime_expect_contains("message_subject_encoded", (const char*)sData, "Subject: =?UTF-8?B?");
	iFailed |= xsmtp_mime_expect_contains("message_mixed", (const char*)sData, "Content-Type: multipart/mixed;");
	iFailed |= xsmtp_mime_expect_contains("message_alternative", (const char*)sData, "Content-Type: multipart/alternative;");
	iFailed |= xsmtp_mime_expect_contains("message_related", (const char*)sData, "Content-Type: multipart/related;");
	iFailed |= xsmtp_mime_expect_contains("message_inline", (const char*)sData, "Content-Disposition: inline; filename=\"logo.png\"");
	iFailed |= xsmtp_mime_expect_contains("message_content_id", (const char*)sData, "Content-ID: <logo@cid>");
	iFailed |= xsmtp_mime_expect_contains("message_attachment", (const char*)sData, "Content-Disposition: attachment; filename=\"report.txt\"");
	iFailed |= xsmtp_mime_expect_contains("message_filename_ext", (const char*)sData, "filename*=UTF-8''report.txt");

	xrtFree(sData);
	return iFailed;
}

static int xsmtp_mime_test_reject_header_injection(void)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	const char* arrHeaderNames[1];
	const char* arrHeaderValues[1];
	str sData;

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "smtp.example.com";
	tCfg.tFrom.sEmail = "sender@example.com";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";

	arrHeaderNames[0] = "X-Bad\r\nInjected";
	arrHeaderValues[0] = "bad";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1;
	tMsg.sSubject = "bad header";
	tMsg.sTextBody = "body";
	tMsg.arrHeaderNames = arrHeaderNames;
	tMsg.arrHeaderValues = arrHeaderValues;
	tMsg.iHeaderCount = 1;

	sData = xsmtp_build_message_data(&tCfg, &tMsg);
	if ( sData != NULL ) {
		xrtFree(sData);
		return xsmtp_mime_fail("header_injection_rejected", "message was built");
	}
	printf("PASS header_injection_rejected\n");
	return 0;
}

static int xsmtp_mime_test_async_context_copies_attachments(void)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpattachment arrAttachments[1];
	xsmtptaskctx* pCtx;
	char arrData[8] = "payload";
	int iFailed = 0;

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "smtp.example.com";
	tCfg.tFrom.sEmail = "sender@example.com";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";

	memset(arrAttachments, 0, sizeof(arrAttachments));
	arrAttachments[0].sFileName = "copy.txt";
	arrAttachments[0].sContentType = "text/plain";
	arrAttachments[0].pData = arrData;
	arrAttachments[0].iDataLen = strlen(arrData);

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1;
	tMsg.sSubject = "copy";
	tMsg.sTextBody = "body";
	tMsg.arrAttachments = arrAttachments;
	tMsg.iAttachmentCount = 1;

	pCtx = xsmtp_task_ctx_create(&tCfg, &tMsg, NULL);
	if ( pCtx == NULL ) {
		return xsmtp_mime_fail("async_context_attachment_copy", "ctx create failed");
	}
	if ( pCtx->tMsg.arrAttachments == arrAttachments ) {
		iFailed = xsmtp_mime_fail("async_context_attachment_array_copy", "array pointer reused");
	} else if ( pCtx->tMsg.arrAttachments[0].pData == arrData ) {
		iFailed = xsmtp_mime_fail("async_context_attachment_data_copy", "data pointer reused");
	} else if ( memcmp(pCtx->tMsg.arrAttachments[0].pData, arrData, strlen(arrData)) != 0 ) {
		iFailed = xsmtp_mime_fail("async_context_attachment_data_value", "data mismatch");
	} else {
		printf("PASS async_context_attachment_copy\n");
	}
	xsmtp_task_ctx_free(pCtx);
	return iFailed;
}

static int xsmtp_mime_test_capability_parse_and_size_check(void)
{
	xsmtpconfig tCfg;
	xsmtpresult tRet;
	uint32 iCaps = 0;
	uint64 iSizeLimit = 0;
	str sXoauth2;

	xrtSmtpResultInit(&tRet);
	xsmtp_parse_capability_line("STARTTLS", &iCaps, &iSizeLimit);
	xsmtp_parse_capability_line("8BITMIME", &iCaps, &iSizeLimit);
	xsmtp_parse_capability_line("SMTPUTF8", &iCaps, &iSizeLimit);
	xsmtp_parse_capability_line("SIZE 12", &iCaps, &iSizeLimit);
	xsmtp_parse_capability_line("DSN", &iCaps, &iSizeLimit);
	xsmtp_parse_capability_line("AUTH PLAIN LOGIN XOAUTH2", &iCaps, &iSizeLimit);

	if ( !(iCaps & XSMTP_CAP_STARTTLS) || !(iCaps & XSMTP_CAP_8BITMIME) || !(iCaps & XSMTP_CAP_SMTPUTF8)
		|| !(iCaps & XSMTP_CAP_SIZE) || !(iCaps & XSMTP_CAP_AUTH_PLAIN) || !(iCaps & XSMTP_CAP_AUTH_LOGIN)
		|| !(iCaps & XSMTP_CAP_AUTH_XOAUTH2) || !(iCaps & XSMTP_CAP_DSN) ) {
		return xsmtp_mime_fail("capability_parse_flags", "missing capability flag");
	}
	if ( iSizeLimit != 12 ) {
		return xsmtp_mime_fail("capability_parse_size", "SIZE limit mismatch");
	}
	xrtSmtpConfigInit(&tCfg);
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_AUTO;
	tCfg.sUser = "user@example.com";
	tCfg.sOAuth2Token = "token";
	if ( xsmtp_auth_mode_auto(&tCfg, iCaps) != XSMTP_AUTH_XOAUTH2 ) {
		return xsmtp_mime_fail("xoauth2_auth_auto", "XOAUTH2 was not selected");
	}
	sXoauth2 = xsmtp_build_xoauth2_response(tCfg.sUser, tCfg.sOAuth2Token);
	if ( sXoauth2 == NULL || sXoauth2[0] == '\0' ) {
		if ( sXoauth2 ) xrtFree(sXoauth2);
		return xsmtp_mime_fail("xoauth2_response", "empty response");
	}
	xrtFree(sXoauth2);
	if ( !xsmtp_check_message_size("short", iCaps, iSizeLimit, &tRet) ) {
		return xsmtp_mime_fail("size_check_accepts_small", tRet.sError);
	}
	if ( xsmtp_check_message_size("this message is too long", iCaps, iSizeLimit, &tRet) ) {
		return xsmtp_mime_fail("size_check_rejects_large", "large message accepted");
	}
	printf("PASS capability_parse_and_size_check\n");
	return 0;
}

static int xsmtp_mime_test_stage_mapping(void)
{
	xsmtpresult tRet;

	xrtSmtpResultInit(&tRet);
	xsmtp_result_stage(&tRet, XSMTP_STAGE_EHLO);
	if ( tRet.iStage != XSMTP_STAGE_EHLO ) {
		return xsmtp_mime_fail("stage_setter", "stage not stored");
	}
	if ( xsmtp_stage_from_async_state(XSMTP_AST_WAIT_STARTTLS_TLS) != XSMTP_STAGE_TLS
		|| xsmtp_stage_from_async_state(XSMTP_AST_WAIT_AUTH_LOGIN_PASS) != XSMTP_STAGE_AUTH
		|| xsmtp_stage_from_async_state(XSMTP_AST_WAIT_BODY) != XSMTP_STAGE_DATA
		|| xsmtp_stage_from_async_state(XSMTP_AST_WAIT_QUIT) != XSMTP_STAGE_QUIT ) {
		return xsmtp_mime_fail("stage_async_mapping", "unexpected mapping");
	}
	printf("PASS stage_mapping\n");
	return 0;
}

static int xsmtp_mime_test_mock_server_send(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = FALSE;
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp";
	tMsg.sTextBody = "plain via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawMailFrom ) {
		iFailed = xsmtp_mime_fail("mock_server_mail_from", "MAIL FROM missing");
		goto cleanup;
	}
	if ( !tServer.bSawRcptTo ) {
		iFailed = xsmtp_mime_fail("mock_server_rcpt_to", "RCPT TO missing");
		goto cleanup;
	}
	if ( !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_data_cmd", "DATA missing");
		goto cleanup;
	}
	if ( !tServer.bDataHasSubject ) {
		iFailed = xsmtp_mime_fail("mock_server_data_subject", "Subject header missing");
		goto cleanup;
	}
	if ( !tServer.bDataHasBody ) {
		iFailed = xsmtp_mime_fail("mock_server_data_body", tServer.sDataSample);
		goto cleanup;
	}
	printf("PASS mock_server_send\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_capability(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_capability_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_capability_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = FALSE;
	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpCapability(&tCfg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_capability", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawEhlo || !tServer.bSawQuit || tServer.bSawMailFrom || tServer.bSawRcptTo || tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_capability_flow", "unexpected command flow");
		goto cleanup;
	}
	if ( !(tRet.iCapabilities & XSMTP_CAP_8BITMIME) || !(tRet.iCapabilities & XSMTP_CAP_SMTPUTF8)
		|| !(tRet.iCapabilities & XSMTP_CAP_DSN) || !(tRet.iCapabilities & XSMTP_CAP_SIZE)
		|| tRet.iSizeLimit != 65536u ) {
		iFailed = xsmtp_mime_fail("mock_server_capability_caps", "capability parse mismatch");
		goto cleanup;
	}
	printf("PASS mock_server_capability\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_dsn(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_dsn_start", "listen failed");
	}
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_dsn_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = FALSE;
	tCfg.tFrom.sEmail = "sender@example.com";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sDsnNotify = "SUCCESS,FAILURE";
	arrTo[0].sDsnOrcpt = "rfc822;receiver@example.com";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp dsn";
	tMsg.sTextBody = "plain via mock";
	tMsg.sDsnReturn = "HDRS";
	tMsg.sDsnEnvid = "trace-123";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_dsn_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawDsnMailFrom ) {
		iFailed = xsmtp_mime_fail("mock_server_dsn_mail_from", "DSN MAIL FROM params missing");
		goto cleanup;
	}
	if ( !tServer.bSawDsnRcptTo ) {
		iFailed = xsmtp_mime_fail("mock_server_dsn_rcpt_to", "DSN RCPT TO params missing");
		goto cleanup;
	}
	printf("PASS mock_server_dsn\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_plain(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_auth_start", "listen failed");
	}
	tServer.bAdvertiseAuthPlain = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_auth_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_PLAIN;
	tCfg.sUser = "sender@example.com";
	tCfg.sPass = "password";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp auth";
	tMsg.sTextBody = "auth plain via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_plain_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawAuthPlain ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_plain_cmd", "AUTH PLAIN missing");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_plain_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_plain\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_login(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_login_start", "listen failed");
	}
	tServer.bAdvertiseAuthLogin = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_login_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_LOGIN;
	tCfg.sUser = "sender@example.com";
	tCfg.sPass = "password";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp login";
	tMsg.sTextBody = "auth login via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_login_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawAuthLogin || !tServer.bAuthLoginUserOK || !tServer.bAuthLoginPassOK ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_login_cmd", "AUTH LOGIN exchange missing");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_login_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_login\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_xoauth2(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_xoauth2_start", "listen failed");
	}
	tServer.bAdvertiseAuthXoauth2 = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_xoauth2_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_XOAUTH2;
	tCfg.sUser = "sender@example.com";
	tCfg.sOAuth2Token = "token-123";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp xoauth2";
	tMsg.sTextBody = "auth xoauth2 via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_xoauth2_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( !tServer.bSawAuthXoauth2 || !tServer.bAuthXoauth2OK ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_xoauth2_cmd", "AUTH XOAUTH2 exchange missing");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_xoauth2_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_xoauth2\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_auto_prefers_xoauth2(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_auto_start", "listen failed");
	}
	tServer.bAdvertiseAuthPlain = TRUE;
	tServer.bAdvertiseAuthLogin = TRUE;
	tServer.bAdvertiseAuthXoauth2 = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_auto_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_AUTO;
	tCfg.sUser = "sender@example.com";
	tCfg.sPass = "password";
	tCfg.sOAuth2Token = "token-123";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp auto";
	tMsg.sTextBody = "auth auto via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( tRet.iAuthMode != XSMTP_AUTH_XOAUTH2 ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_mode", "AUTO did not select XOAUTH2");
		goto cleanup;
	}
	if ( !tServer.bSawAuthXoauth2 || !tServer.bAuthXoauth2OK || tServer.bSawAuthPlain || tServer.bSawAuthLogin ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_cmd", "AUTO auth exchange unexpected");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_auto_prefers_xoauth2\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_auto_falls_back_plain(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_auto_plain_start", "listen failed");
	}
	tServer.bAdvertiseAuthPlain = TRUE;
	tServer.bAdvertiseAuthLogin = TRUE;
	tServer.bAdvertiseAuthXoauth2 = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_auto_plain_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_AUTO;
	tCfg.sUser = "sender@example.com";
	tCfg.sPass = "password";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp auto plain";
	tMsg.sTextBody = "auth auto plain via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_plain_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( tRet.iAuthMode != XSMTP_AUTH_PLAIN ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_plain_mode", "AUTO did not select PLAIN");
		goto cleanup;
	}
	if ( !tServer.bSawAuthPlain || tServer.bSawAuthLogin || tServer.bSawAuthXoauth2 ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_plain_cmd", "AUTO auth exchange unexpected");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_plain_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_auto_falls_back_plain\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

static int xsmtp_mime_test_mock_server_auth_auto_falls_back_login(void)
{
	xsmtp_mock_server tServer;
	xthread pThread;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr arrTo[1];
	xsmtpresult tRet;
	int iFailed = 0;

	if ( !xsmtp_mock_server_start(&tServer) ) {
		return xsmtp_mime_fail("mock_server_auto_login_start", "listen failed");
	}
	tServer.bAdvertiseAuthLogin = TRUE;
	pThread = xrtThreadCreate((ptr)xsmtp_mock_server_thread, &tServer, 0);
	if ( pThread == NULL ) {
		xsmtp_mock_server_stop(&tServer);
		return xsmtp_mime_fail("mock_server_auto_login_thread", "thread create failed");
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = "127.0.0.1";
	tCfg.iPort = tServer.iPort;
	tCfg.iSecureMode = XSMTP_SECURE_NONE;
	tCfg.iTimeoutMs = 5000u;
	tCfg.bAuth = TRUE;
	tCfg.iAuthMode = XSMTP_AUTH_AUTO;
	tCfg.sUser = "sender@example.com";
	tCfg.sPass = "password";
	tCfg.tFrom.sEmail = "sender@example.com";
	tCfg.tFrom.sName = "Sender";

	memset(arrTo, 0, sizeof(arrTo));
	arrTo[0].sEmail = "receiver@example.com";
	arrTo[0].sName = "Receiver";

	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = arrTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "mock smtp auto login";
	tMsg.sTextBody = "auth auto login via mock";

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_login_send", tRet.sError);
		goto cleanup;
	}
	xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	if ( tRet.iAuthMode != XSMTP_AUTH_LOGIN ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_login_mode", "AUTO did not select LOGIN");
		goto cleanup;
	}
	if ( !tServer.bSawAuthLogin || !tServer.bAuthLoginUserOK || !tServer.bAuthLoginPassOK || tServer.bSawAuthPlain || tServer.bSawAuthXoauth2 ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_login_cmd", "AUTO auth exchange unexpected");
		goto cleanup;
	}
	if ( !tServer.bSawMailFrom || !tServer.bSawRcptTo || !tServer.bSawData ) {
		iFailed = xsmtp_mime_fail("mock_server_auth_auto_login_flow", "send flow incomplete");
		goto cleanup;
	}
	printf("PASS mock_server_auth_auto_falls_back_login\n");

cleanup:
	xsmtp_mock_server_stop(&tServer);
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	return iFailed;
}

int main(void)
{
	int iFailed = 0;
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("xsmtp_mime_test\n");
	iFailed |= xsmtp_mime_test_build_message();
	iFailed |= xsmtp_mime_test_reject_header_injection();
	iFailed |= xsmtp_mime_test_async_context_copies_attachments();
	iFailed |= xsmtp_mime_test_capability_parse_and_size_check();
	iFailed |= xsmtp_mime_test_stage_mapping();
	iFailed |= xsmtp_mime_test_mock_server_capability();
	iFailed |= xsmtp_mime_test_mock_server_send();
	iFailed |= xsmtp_mime_test_mock_server_dsn();
	iFailed |= xsmtp_mime_test_mock_server_auth_plain();
	iFailed |= xsmtp_mime_test_mock_server_auth_login();
	iFailed |= xsmtp_mime_test_mock_server_auth_xoauth2();
	iFailed |= xsmtp_mime_test_mock_server_auth_auto_prefers_xoauth2();
	iFailed |= xsmtp_mime_test_mock_server_auth_auto_falls_back_plain();
	iFailed |= xsmtp_mime_test_mock_server_auth_auto_falls_back_login();
	if ( iFailed ) {
		printf("xsmtp_mime_test: FAIL\n");
		return 1;
	}
	printf("xsmtp_mime_test: PASS\n");
	return 0;
}
