#define XRT_IMPLEMENTATION
#include "xmail.h"

#include <ctype.h>
#include <stdlib.h>
#include <time.h>

static const char* xmail_live_env(const char* sName, const char* sDefault)
{
	const char* sValue = getenv(sName);
	return (sValue != NULL && sValue[0] != '\0') ? sValue : sDefault;
}

static int xmail_live_env_int(const char* sName, int iDefault)
{
	const char* sValue = getenv(sName);
	return (sValue != NULL && sValue[0] != '\0') ? atoi(sValue) : iDefault;
}

static bool xmail_live_streq_i(const char* a, const char* b)
{
	size_t i;
	if ( a == NULL || b == NULL ) {
		return FALSE;
	}
	for ( i = 0u; a[i] != '\0' && b[i] != '\0'; i++ ) {
		if ( tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]) ) {
			return FALSE;
		}
	}
	return a[i] == '\0' && b[i] == '\0';
}

static int xmail_live_smtp_secure_mode(void)
{
	const char* sMode = xmail_live_env("XMAIL_LIVE_SMTP_SECURE", "ssl");
	if ( xmail_live_streq_i(sMode, "starttls") ) {
		return XSMTP_SECURE_STARTTLS;
	}
	if ( xmail_live_streq_i(sMode, "none") ) {
		return XSMTP_SECURE_NONE;
	}
	if ( xmail_live_streq_i(sMode, "auto") ) {
		return XSMTP_SECURE_AUTO;
	}
	return XSMTP_SECURE_SSL;
}

static int xmail_live_imap_secure_mode(void)
{
	const char* sMode = xmail_live_env("XMAIL_LIVE_IMAP_SECURE", "ssl");
	if ( xmail_live_streq_i(sMode, "starttls") ) {
		return XIMAP_SECURE_STARTTLS;
	}
	if ( xmail_live_streq_i(sMode, "none") ) {
		return XIMAP_SECURE_NONE;
	}
	if ( xmail_live_streq_i(sMode, "auto") ) {
		return XIMAP_SECURE_AUTO;
	}
	return XIMAP_SECURE_SSL;
}

static int xmail_live_pop3_secure_mode(void)
{
	const char* sMode = xmail_live_env("XMAIL_LIVE_POP3_SECURE", "ssl");
	if ( xmail_live_streq_i(sMode, "starttls") ) {
		return XPOP3_SECURE_STARTTLS;
	}
	if ( xmail_live_streq_i(sMode, "none") ) {
		return XPOP3_SECURE_NONE;
	}
	if ( xmail_live_streq_i(sMode, "auto") ) {
		return XPOP3_SECURE_AUTO;
	}
	return XPOP3_SECURE_SSL;
}

static bool xmail_live_verify_peer(void)
{
	return xmail_live_env_int("XMAIL_LIVE_VERIFY_PEER", 1) ? TRUE : FALSE;
}

static bool xmail_live_smtp_expect_tls(void)
{
	const char* sDefault = xmail_live_streq_i(xmail_live_env("XMAIL_LIVE_SMTP_SECURE", "ssl"), "none") ? "0" : "1";
	return xmail_live_env_int("XMAIL_LIVE_SMTP_EXPECT_TLS", atoi(sDefault)) ? TRUE : FALSE;
}

static uint16 xmail_live_tls_max_version(const char* sEnvName)
{
	const char* sValue = xmail_live_env(sEnvName, "");
	if ( xmail_live_streq_i(sValue, "tls1.2") || xmail_live_streq_i(sValue, "tlsv1.2") || xmail_live_streq_i(sValue, "1.2") ) {
		return 0x0303u;
	}
	if ( xmail_live_streq_i(sValue, "tls1.3") || xmail_live_streq_i(sValue, "tlsv1.3") || xmail_live_streq_i(sValue, "1.3") ) {
		return 0x0304u;
	}
	return (uint16)xmail_live_env_int(sEnvName, 0);
}

static void xmail_live_sleep_ms(uint32 iMs)
{
#if defined(_WIN32) || defined(_WIN64)
	Sleep(iMs);
#else
	usleep((useconds_t)iMs * 1000u);
#endif
}

static size_t xmail_live_collect_search_numbers(const char* sRaw, char arrUid[][64], size_t iMaxUid)
{
	const char* p;
	size_t iCount = 0u;

	if ( sRaw == NULL || arrUid == NULL || iMaxUid == 0u ) {
		return 0u;
	}
	p = strstr(sRaw, "* SEARCH");
	if ( p == NULL ) {
		return 0u;
	}
	p += strlen("* SEARCH");
	while ( *p != '\0' && *p != '\r' && *p != '\n' ) {
		while ( *p == ' ' || *p == '\t' ) {
			p++;
		}
		if ( *p >= '0' && *p <= '9' ) {
			const char* sStart = p;
			size_t iLen;
			while ( *p >= '0' && *p <= '9' ) {
				p++;
			}
			iLen = (size_t)(p - sStart);
			if ( iLen >= sizeof(arrUid[0]) ) {
				iLen = sizeof(arrUid[0]) - 1u;
			}
			memcpy(arrUid[iCount], sStart, iLen);
			arrUid[iCount][iLen] = '\0';
			iCount++;
			if ( iCount >= iMaxUid ) {
				return iCount;
			}
		} else if ( *p != '\0' && *p != '\r' && *p != '\n' ) {
			p++;
		}
	}
	return iCount;
}

static void xmail_live_quote_imap_text(const char* sText, char* sOut, size_t iCap)
{
	size_t i = 0u;
	size_t j = 0u;
	if ( sOut == NULL || iCap == 0u ) {
		return;
	}
	for ( ; sText != NULL && sText[i] != '\0' && j + 2u < iCap; i++ ) {
		if ( sText[i] == '\\' || sText[i] == '"' ) {
			sOut[j++] = '\\';
		}
		sOut[j++] = sText[i];
	}
	sOut[j] = '\0';
}

static int xmail_live_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xmail_live_send_smtp(const char* sEmail, const char* sPass, const char* sSubject)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpaddr tTo;
	xsmtpresult tRet;
	char sText[512];

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = xmail_live_env("XMAIL_LIVE_SMTP_HOST", "smtp.qq.com");
	tCfg.iPort = (uint16)xmail_live_env_int("XMAIL_LIVE_SMTP_PORT", 465);
	tCfg.iSecureMode = xmail_live_smtp_secure_mode();
	tCfg.iTimeoutMs = (uint32)xmail_live_env_int("XMAIL_LIVE_TIMEOUT_MS", 30000);
	tCfg.bAuth = xmail_live_env_int("XMAIL_LIVE_SMTP_AUTH", 1) ? TRUE : FALSE;
	tCfg.iAuthMode = XSMTP_AUTH_AUTO;
	tCfg.bVerifyPeer = xmail_live_verify_peer();
	tCfg.iTlsMaxVersion = xmail_live_tls_max_version("XMAIL_LIVE_SMTP_TLS_MAX_VERSION");
	tCfg.sCaFile = xmail_live_env("XMAIL_LIVE_SMTP_CA_FILE", NULL);
	tCfg.sUser = sEmail;
	tCfg.sPass = sPass;
	tCfg.sHeloName = "localhost";
	tCfg.tFrom.sEmail = sEmail;
	tCfg.tFrom.sName = "xmail live test";

	memset(&tTo, 0, sizeof(tTo));
	tTo.sEmail = sEmail;
	tTo.sName = "xmail live test";

	snprintf(sText, sizeof(sText), "xmail live test loop message\r\nSubject: %s\r\n", sSubject);
	xrtSmtpMessageInit(&tMsg);
	tMsg.arrTo = &tTo;
	tMsg.iToCount = 1u;
	tMsg.sSubject = sSubject;
	tMsg.sTextBody = sText;

	xrtSmtpResultInit(&tRet);
	if ( !xrtSmtpSendMail(&tCfg, &tMsg, &tRet) ) {
		return xmail_live_fail("smtp_send", tRet.sError);
	}
	if ( xmail_live_smtp_expect_tls() && !tRet.bUsedTLS ) {
		return xmail_live_fail("smtp_tls", "TLS not used");
	}
	printf("PASS smtp_send\n");
	return 0;
}

static int xmail_live_check_imap(const char* sEmail, const char* sPass, const char* sSubject)
{
	ximapconfig tCfg;
	ximapclient* pClient = NULL;
	ximapresult tRet;
	xmailparsedmessage tMsg;
	str sRaw = NULL;
	char arrUid[256][64];
	char sSubjectEsc[220];
	char sSearchCriteria[260];
	size_t iUidCount = 0u;
	int i;
	int iFailed = 0;

	xrtImapConfigInit(&tCfg);
	tCfg.sHost = xmail_live_env("XMAIL_LIVE_IMAP_HOST", "imap.qq.com");
	tCfg.iPort = (uint16)xmail_live_env_int("XMAIL_LIVE_IMAP_PORT", 993);
	tCfg.iSecureMode = xmail_live_imap_secure_mode();
	tCfg.iTimeoutMs = (uint32)xmail_live_env_int("XMAIL_LIVE_TIMEOUT_MS", 30000);
	tCfg.bVerifyPeer = xmail_live_verify_peer();
	tCfg.iTlsMaxVersion = xmail_live_tls_max_version("XMAIL_LIVE_IMAP_TLS_MAX_VERSION");
	tCfg.sCaFile = xmail_live_env("XMAIL_LIVE_IMAP_CA_FILE", NULL);
	tCfg.sUser = sEmail;
	tCfg.sPass = sPass;
	xrtImapResultInit(&tRet);
	printf("INFO imap_open\n");
	if ( !xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		return xmail_live_fail("imap_open", tRet.sError);
	}
	if ( !tRet.bUsedTLS ) {
		iFailed = xmail_live_fail("imap_tls", "TLS not used");
		goto cleanup;
	}
	printf("INFO imap_select\n");
	if ( !xrtImapSelect(pClient, "INBOX", NULL, &tRet) ) {
		iFailed = xmail_live_fail("imap_select", tRet.sError);
		goto cleanup;
	}
	if ( xmail_live_env_int("XMAIL_LIVE_CHECK_IDLE", 0) != 0 ) {
		printf("INFO imap_idle_probe\n");
		if ( !xrtImapIdleProbe(pClient, &tRet) ) {
			iFailed = xmail_live_fail("imap_idle_probe", tRet.sError);
			goto cleanup;
		}
		printf("PASS imap_idle_probe\n");
	}
	xmail_live_quote_imap_text(sSubject, sSubjectEsc, sizeof(sSubjectEsc));
	snprintf(sSearchCriteria, sizeof(sSearchCriteria), "SUBJECT \"%s\"", sSubjectEsc);
	for ( i = 0; i < 12; i++ ) {
		if ( sRaw != NULL ) {
			xrtFree(sRaw);
			sRaw = NULL;
		}
		printf("INFO imap_uid_search_subject try=%d\n", i + 1);
		if ( !xrtImapUidSearchRaw(pClient, sSearchCriteria, &sRaw, &tRet) ) {
			iFailed = xmail_live_fail("imap_uid_search", tRet.sError);
			goto cleanup;
		}
		iUidCount = xmail_live_collect_search_numbers((const char*)sRaw, arrUid, sizeof(arrUid) / sizeof(arrUid[0]));
		if ( iUidCount > 0u ) {
			break;
		}
		xmail_live_sleep_ms(5000u);
	}
	if ( iUidCount == 0u ) {
		iFailed = xmail_live_fail("imap_find_sent_message", sRaw ? (const char*)sRaw : "not found");
		goto cleanup;
	}
	for ( i = 0; (i < 12) && ((size_t)i < iUidCount); i++ ) {
		size_t iIndex = iUidCount - 1u - (size_t)i;
		memset(&tMsg, 0, sizeof(tMsg));
		printf("INFO imap_uid_fetch uid=%s\n", arrUid[iIndex]);
		if ( !xrtImapUidFetchMime(pClient, arrUid[iIndex], &tMsg, &tRet) ) {
			iFailed = xmail_live_fail("imap_uid_fetch_mime", tRet.sError);
			goto cleanup;
		}
		if ( strcmp(xmailMimeParsedGetHeader(&tMsg, "Subject"), sSubject) == 0 ) {
			xmailMimeParsedMessageFree(&tMsg);
			printf("PASS imap_receive\n");
			goto cleanup;
		}
		xmailMimeParsedMessageFree(&tMsg);
	}
	iFailed = xmail_live_fail("imap_subject", "not found in newest messages");

cleanup:
	if ( sRaw != NULL ) xrtFree(sRaw);
	if ( pClient != NULL ) xrtImapClose(pClient);
	return iFailed;
}

static int xmail_live_check_pop3(const char* sEmail, const char* sPass, const char* sSubject)
{
	xpop3config tCfg;
	xpop3client* pClient = NULL;
	xpop3result tRet;
	xpop3listitem* arrItems = NULL;
	size_t iCount = 0u;
	size_t iTry;
	int iFailed = 0;

	xrtPop3ConfigInit(&tCfg);
	tCfg.sHost = xmail_live_env("XMAIL_LIVE_POP3_HOST", "pop.qq.com");
	tCfg.iPort = (uint16)xmail_live_env_int("XMAIL_LIVE_POP3_PORT", 995);
	tCfg.iSecureMode = xmail_live_pop3_secure_mode();
	tCfg.iTimeoutMs = (uint32)xmail_live_env_int("XMAIL_LIVE_TIMEOUT_MS", 30000);
	tCfg.bVerifyPeer = xmail_live_verify_peer();
	tCfg.iTlsMaxVersion = xmail_live_tls_max_version("XMAIL_LIVE_POP3_TLS_MAX_VERSION");
	tCfg.sCaFile = xmail_live_env("XMAIL_LIVE_POP3_CA_FILE", NULL);
	tCfg.sUser = sEmail;
	tCfg.sPass = sPass;
	xrtPop3ResultInit(&tRet);
	printf("INFO pop3_open\n");
	if ( !xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		return xmail_live_fail("pop3_open", tRet.sError);
	}
	if ( !tRet.bUsedTLS ) {
		iFailed = xmail_live_fail("pop3_tls", "TLS not used");
		goto cleanup;
	}
	printf("INFO pop3_list\n");
	if ( !xrtPop3List(pClient, &arrItems, &iCount, &tRet) ) {
		iFailed = xmail_live_fail("pop3_list", tRet.sError);
		goto cleanup;
	}
	for ( iTry = 0u; iTry < iCount && iTry < 10u; iTry++ ) {
		size_t iIndex = iCount - 1u - iTry;
		xmailparsedmessage tMsg;
		memset(&tMsg, 0, sizeof(tMsg));
		printf("INFO pop3_retr number=%u\n", (unsigned)arrItems[iIndex].iNumber);
		if ( xrtPop3RetrMime(pClient, arrItems[iIndex].iNumber, &tMsg, &tRet) ) {
			const char* sGot = xmailMimeParsedGetHeader(&tMsg, "Subject");
			bool bMatch = (sGot != NULL && strcmp(sGot, sSubject) == 0);
			xmailMimeParsedMessageFree(&tMsg);
			if ( bMatch ) {
				printf("PASS pop3_receive\n");
				goto cleanup;
			}
		}
	}
	iFailed = xmail_live_fail("pop3_find_sent_message", "not found in newest messages");

cleanup:
	if ( arrItems != NULL ) xrtPop3ListFree(arrItems);
	if ( pClient != NULL ) xrtPop3Close(pClient);
	return iFailed;
}

int main(void)
{
	const char* sEmail = xmail_live_env("XMAIL_LIVE_EMAIL", NULL);
	const char* sPass = xmail_live_env("XMAIL_LIVE_PASSWORD", NULL);
	const char* sSubjectEnv = xmail_live_env("XMAIL_LIVE_SUBJECT", NULL);
	char sSubject[160];
	int iFailed = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	if ( sEmail == NULL || sPass == NULL ) {
		printf("SKIP xmail_live_test: set XMAIL_LIVE_EMAIL and XMAIL_LIVE_PASSWORD\n");
		return 77;
	}
	if ( sSubjectEnv != NULL ) {
		snprintf(sSubject, sizeof(sSubject), "%s", sSubjectEnv);
	} else {
		snprintf(sSubject, sizeof(sSubject), "xmail-live-%lu", (unsigned long)time(NULL));
	}
	printf("xmail_live_test subject=%s\n", sSubject);
	if ( xmail_live_env_int("XMAIL_LIVE_SKIP_SMTP", 0) == 0 ) {
		iFailed |= xmail_live_send_smtp(sEmail, sPass, sSubject);
	}
	if ( iFailed == 0 && xmail_live_env_int("XMAIL_LIVE_SKIP_IMAP", 0) == 0 ) {
		iFailed |= xmail_live_check_imap(sEmail, sPass, sSubject);
	}
	if ( iFailed == 0 && xmail_live_env_int("XMAIL_LIVE_SKIP_POP3", 0) == 0 ) {
		iFailed |= xmail_live_check_pop3(sEmail, sPass, sSubject);
	}
	printf("xmail_live_test: %s\n", iFailed ? "FAIL" : "PASS");
	return iFailed ? 1 : 0;
}
