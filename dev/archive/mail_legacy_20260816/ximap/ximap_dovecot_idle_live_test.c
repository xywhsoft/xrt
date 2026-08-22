#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "ximap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ximap_dovecot_idle_writer {
	const char* sMaildir;
	const char* sSubject;
	int iOk;
	char sError[256];
} ximap_dovecot_idle_writer;

static const char* ximap_live_env(const char* sName, const char* sDefault)
{
	const char* sValue = getenv(sName);
	return (sValue != NULL && sValue[0] != '\0') ? sValue : sDefault;
}

static int ximap_live_env_int(const char* sName, int iDefault)
{
	const char* sValue = getenv(sName);
	return (sValue != NULL && sValue[0] != '\0') ? atoi(sValue) : iDefault;
}

static void ximap_dovecot_idle_writer_thread(void* pArg)
{
	ximap_dovecot_idle_writer* pWriter = (ximap_dovecot_idle_writer*)pArg;
	char sTmp[512];
	char sNew[512];
	FILE* fp;
	unsigned long iNow = (unsigned long)time(NULL);

	xrtSleep(1200u);
	snprintf(sTmp, sizeof(sTmp), "%s/tmp/%lu.%lu.ximap-idle-live", pWriter->sMaildir, iNow, (unsigned long)xrtThreadGetCurrentId());
	snprintf(sNew, sizeof(sNew), "%s/new/%lu.%lu.ximap-idle-live", pWriter->sMaildir, iNow, (unsigned long)xrtThreadGetCurrentId());
	fp = fopen(sTmp, "wb");
	if ( fp == NULL ) {
		snprintf(pWriter->sError, sizeof(pWriter->sError), "open tmp failed");
		return;
	}
	fprintf(fp, "From: xs@localhost\r\n");
	fprintf(fp, "To: xs@localhost\r\n");
	fprintf(fp, "Subject: %s\r\n", pWriter->sSubject);
	fprintf(fp, "Date: Fri, 15 May 2026 00:00:00 +0800\r\n");
	fprintf(fp, "MIME-Version: 1.0\r\n");
	fprintf(fp, "Content-Type: text/plain; charset=utf-8\r\n");
	fprintf(fp, "\r\n");
	fprintf(fp, "ximap dovecot idle live event\r\n");
	if ( fclose(fp) != 0 ) {
		snprintf(pWriter->sError, sizeof(pWriter->sError), "close tmp failed");
		return;
	}
	if ( rename(sTmp, sNew) != 0 ) {
		snprintf(pWriter->sError, sizeof(pWriter->sError), "rename to new failed");
		return;
	}
	pWriter->iOk = 1;
}

int main(void)
{
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximap_dovecot_idle_writer tWriter;
	xthread pThread = NULL;
	str* arrEvents = NULL;
	size_t iEventCount = 0u;
	char sSubject[160];
	int iFailed = 0;

	const char* sUser = ximap_live_env("XIMAP_LIVE_USER", "xs");
	const char* sPass = ximap_live_env("XIMAP_LIVE_PASSWORD", NULL);
	const char* sMaildir = ximap_live_env("XIMAP_LIVE_MAILDIR", "/home/xs/Maildir");

	setvbuf(stdout, NULL, _IONBF, 0);
	if ( sPass == NULL ) {
		printf("SKIP ximap_dovecot_idle_live_test: set XIMAP_LIVE_PASSWORD\n");
		return 77;
	}
	snprintf(sSubject, sizeof(sSubject), "ximap-dovecot-idle-live-%lu", (unsigned long)time(NULL));

	xrtImapConfigInit(&tCfg);
	tCfg.sHost = ximap_live_env("XIMAP_LIVE_HOST", "127.0.0.1");
	tCfg.iPort = (uint16)ximap_live_env_int("XIMAP_LIVE_PORT", 143);
	tCfg.iSecureMode = XIMAP_SECURE_STARTTLS;
	tCfg.iTimeoutMs = (uint32)ximap_live_env_int("XIMAP_LIVE_TIMEOUT_MS", 20000);
	tCfg.bVerifyPeer = FALSE;
	tCfg.iTlsMaxVersion = 0x0303u;
	tCfg.sUser = sUser;
	tCfg.sPass = sPass;

	xrtImapResultInit(&tRet);
	printf("ximap_dovecot_idle_live_test subject=%s\n", sSubject);
	if ( !xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		printf("FAIL imap_open: %s\n", tRet.sError);
		return 1;
	}
	if ( !tRet.bUsedTLS ) {
		printf("FAIL imap_tls: TLS not used\n");
		iFailed = 1;
		goto cleanup;
	}
	if ( !xrtImapSelect(pClient, "INBOX", NULL, &tRet) ) {
		printf("FAIL imap_select: %s\n", tRet.sError);
		iFailed = 1;
		goto cleanup;
	}

	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.sMaildir = sMaildir;
	tWriter.sSubject = sSubject;
	pThread = xrtThreadCreate((ptr)ximap_dovecot_idle_writer_thread, &tWriter, 0);
	if ( pThread == NULL ) {
		printf("FAIL writer_thread: create failed\n");
		iFailed = 1;
		goto cleanup;
	}

	printf("INFO imap_idle_collect\n");
	if ( !xrtImapIdleCollect(pClient, 1u, &arrEvents, &iEventCount, &tRet) ) {
		printf("FAIL imap_idle_collect: %s\n", tRet.sError);
		iFailed = 1;
		goto cleanup;
	}
	if ( iEventCount == 0u || arrEvents == NULL || strstr((const char*)arrEvents[0], "EXISTS") == NULL ) {
		printf("FAIL imap_idle_event: first event missing EXISTS\n");
		iFailed = 1;
		goto cleanup;
	}
	printf("PASS imap_idle_event: %s\n", (const char*)arrEvents[0]);

cleanup:
	if ( pThread != NULL ) {
		xrtThreadWait(pThread);
		if ( !tWriter.iOk ) {
			printf("FAIL writer: %s\n", tWriter.sError[0] ? tWriter.sError : "message not written");
			iFailed = 1;
		}
		xrtThreadDestroy(pThread);
	}
	if ( arrEvents != NULL ) {
		xrtImapIdleEventsFree(arrEvents, iEventCount);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	printf("ximap_dovecot_idle_live_test: %s\n", iFailed ? "FAIL" : "PASS");
	return iFailed ? 1 : 0;
}
