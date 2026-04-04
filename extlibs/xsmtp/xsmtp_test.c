#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xsmtp.h"

/*
 * xsmtp example configuration
 *
 * Replace the placeholder values below with your real SMTP settings before
 * running this example. Do not commit real passwords or provider app tokens.
 */
#define XSMTP_EXAMPLE_HOST            "smtp.example.com"
#define XSMTP_EXAMPLE_PORT            587
#define XSMTP_EXAMPLE_SECURE_MODE     XSMTP_SECURE_STARTTLS
#define XSMTP_EXAMPLE_AUTH            1
#define XSMTP_EXAMPLE_AUTH_MODE       XSMTP_AUTH_AUTO
#define XSMTP_EXAMPLE_VERIFY_PEER     1
#define XSMTP_EXAMPLE_TIMEOUT_MS      15000
#define XSMTP_EXAMPLE_HELO_NAME       "localhost"

#define XSMTP_EXAMPLE_USERNAME        "user@example.com"
#define XSMTP_EXAMPLE_PASSWORD        "replace-with-app-password"

#define XSMTP_EXAMPLE_FROM_EMAIL      "user@example.com"
#define XSMTP_EXAMPLE_FROM_NAME       "xsmtp example"
#define XSMTP_EXAMPLE_REPLY_TO_EMAIL  "user@example.com"
#define XSMTP_EXAMPLE_REPLY_TO_NAME   "xsmtp example"

#define XSMTP_EXAMPLE_TO_EMAIL        "receiver@example.com"
#define XSMTP_EXAMPLE_TO_NAME         "Receiver"

#define XSMTP_EXAMPLE_USE_ASYNC       1
#define XSMTP_EXAMPLE_USE_COROUTINE   0
#define XSMTP_EXAMPLE_ASYNC_DEBUG     "xsmtp_example"

static const char* xsmtp_example_mode_name(void)
{
#if XSMTP_EXAMPLE_USE_COROUTINE
	return "coroutine";
#elif XSMTP_EXAMPLE_USE_ASYNC
	return "async_wait";
#else
	return "sync";
#endif
}

/* Keep all public example entry styles referenced so header-only builds stay warning-free. */
static void xsmtp_example_link_all_entrypoints(void)
{
	(void)&xrtSmtpSendMail;
	(void)&xrtSmtpSendMailAsyncWait;
	(void)&xrtSmtpSendMailCo;
	(void)&xrtSmtpSendMailFuture;
}

static void xsmtp_example_fill_config(xsmtpconfig* pCfg)
{
	xrtSmtpConfigInit(pCfg);
	pCfg->sHost = XSMTP_EXAMPLE_HOST;
	pCfg->iPort = (uint16)XSMTP_EXAMPLE_PORT;
	pCfg->iTimeoutMs = (uint32)XSMTP_EXAMPLE_TIMEOUT_MS;
	pCfg->iSecureMode = XSMTP_EXAMPLE_SECURE_MODE;
	pCfg->bAuth = XSMTP_EXAMPLE_AUTH ? TRUE : FALSE;
	pCfg->iAuthMode = XSMTP_EXAMPLE_AUTH_MODE;
	pCfg->bVerifyPeer = XSMTP_EXAMPLE_VERIFY_PEER ? TRUE : FALSE;
	pCfg->sUser = XSMTP_EXAMPLE_USERNAME;
	pCfg->sPass = XSMTP_EXAMPLE_PASSWORD;
	pCfg->sHeloName = XSMTP_EXAMPLE_HELO_NAME;
	pCfg->tFrom.sEmail = XSMTP_EXAMPLE_FROM_EMAIL;
	pCfg->tFrom.sName = XSMTP_EXAMPLE_FROM_NAME;
	pCfg->tReplyTo.sEmail = XSMTP_EXAMPLE_REPLY_TO_EMAIL;
	pCfg->tReplyTo.sName = XSMTP_EXAMPLE_REPLY_TO_NAME;
}

static void xsmtp_example_fill_message(const xsmtpconfig* pCfg, xsmtpmessage* pMsg)
{
	static xsmtpaddr tTo;
	static char sSubject[256];
	static char sTextBody[512];
	static char sHtmlBody[1024];
	time_t tNow;
	struct tm tLocal;

	tNow = time(NULL);
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tLocal, &tNow);
#else
	localtime_r(&tNow, &tLocal);
#endif

	snprintf(sSubject, sizeof(sSubject),
		"[xsmtp example] %04d-%02d-%02d %02d:%02d:%02d",
		tLocal.tm_year + 1900,
		tLocal.tm_mon + 1,
		tLocal.tm_mday,
		tLocal.tm_hour,
		tLocal.tm_min,
		tLocal.tm_sec);
	snprintf(sTextBody, sizeof(sTextBody),
		"This mail was sent by the xsmtp example.\r\n\r\n"
		"Host: %s\r\n"
		"Port: %u\r\n"
		"Mode: %s\r\n",
		pCfg->sHost ? pCfg->sHost : "",
		(unsigned)pCfg->iPort,
		xsmtp_example_mode_name());
	snprintf(sHtmlBody, sizeof(sHtmlBody),
		"<html><body>"
		"<h2>xsmtp example</h2>"
		"<p>Host: %s</p>"
		"<p>Port: %u</p>"
		"<p>Mode: %s</p>"
		"</body></html>",
		pCfg->sHost ? pCfg->sHost : "",
		(unsigned)pCfg->iPort,
		xsmtp_example_mode_name());

	memset(&tTo, 0, sizeof(tTo));
	tTo.sEmail = XSMTP_EXAMPLE_TO_EMAIL;
	tTo.sName = XSMTP_EXAMPLE_TO_NAME;

	xrtSmtpMessageInit(pMsg);
	pMsg->arrTo = &tTo;
	pMsg->iToCount = 1;
	pMsg->sSubject = sSubject;
	pMsg->sTextBody = sTextBody;
	pMsg->sHtmlBody = sHtmlBody;
}

static bool xsmtp_example_send(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
#if XSMTP_EXAMPLE_USE_COROUTINE
	return xrtSmtpSendMailCo(pCfg, pMsg, pRet);
#elif XSMTP_EXAMPLE_USE_ASYNC
	xsmtpasyncopts tOpts;
	xrtSmtpAsyncOptsInit(&tOpts);
	tOpts.sDebugName = XSMTP_EXAMPLE_ASYNC_DEBUG;
	return xrtSmtpSendMailAsyncWait(pCfg, pMsg, &tOpts, pRet);
#else
	return xrtSmtpSendMail(pCfg, pMsg, pRet);
#endif
}

int main(void)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult tRet;

	setvbuf(stdout, NULL, _IONBF, 0);
	xsmtp_example_link_all_entrypoints();

	xsmtp_example_fill_config(&tCfg);
	xsmtp_example_fill_message(&tCfg, &tMsg);
	xrtSmtpResultInit(&tRet);

	printf("xsmtp example\n");
	printf("host=%s port=%u secure=%d auth=%d verify_peer=%d mode=%s\n",
		tCfg.sHost ? tCfg.sHost : "(null)",
		(unsigned)tCfg.iPort,
		tCfg.iSecureMode,
		tCfg.bAuth ? 1 : 0,
		tCfg.bVerifyPeer ? 1 : 0,
		xsmtp_example_mode_name());
	printf("from=%s to=%s\n",
		tCfg.tFrom.sEmail ? tCfg.tFrom.sEmail : "(null)",
		XSMTP_EXAMPLE_TO_EMAIL);
	printf("sending...\n");

	if ( !xsmtp_example_send(&tCfg, &tMsg, &tRet) ) {
		printf("send failed\n");
		printf("error: %s\n", tRet.sError);
		printf("reply: %s\n", tRet.sLastReply);
		return 2;
	}

	printf("send ok\n");
	printf("server_code=%d auth_mode=%d caps=0x%08x tls=%d starttls=%d\n",
		tRet.iServerCode,
		tRet.iAuthMode,
		(unsigned)tRet.iCapabilities,
		tRet.bUsedTLS ? 1 : 0,
		tRet.bUsedStartTLS ? 1 : 0);
	printf("reply: %s\n", tRet.sLastReply);
	return 0;
}
