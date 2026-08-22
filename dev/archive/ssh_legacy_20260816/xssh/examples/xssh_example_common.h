#ifndef XSSH_EXAMPLE_COMMON_H
#define XSSH_EXAMPLE_COMMON_H

#include "../xssh.h"
#include <stdio.h>

static const char* xsshExampleTextOrDash(const char* sText)
{
	return (sText != NULL && sText[0] != '\0') ? sText : "-";
}

static void xsshExamplePrintResult(FILE* pFile, const char* sPrefix, const xsshresult* pResult)
{
	if ( pFile == NULL || sPrefix == NULL || pResult == NULL ) {
		return;
	}
	/* 示例和 interop 排障时必须能看到 SSH 阶段、服务端 banner、协商算法和认证上下文。 */
	fprintf(pFile,
		"%s: stage=%d error=%d disconnect=%d banner=%s kex=%s hostkey=%s c2s=%s s2c=%s mac-c2s=%s mac-s2c=%s auth=%s fingerprint=%s text=%s\n",
		sPrefix,
		pResult->iStage,
		pResult->iError,
		pResult->iDisconnectReason,
		xsshExampleTextOrDash(pResult->sServerBanner),
		xsshExampleTextOrDash(pResult->sKex),
		xsshExampleTextOrDash(pResult->sHostKey),
		xsshExampleTextOrDash(pResult->sCipherClientToServer),
		xsshExampleTextOrDash(pResult->sCipherServerToClient),
		xsshExampleTextOrDash(pResult->sMacClientToServer),
		xsshExampleTextOrDash(pResult->sMacServerToClient),
		xsshExampleTextOrDash(pResult->sAuthMethods),
		xsshExampleTextOrDash(pResult->sHostKeyFingerprint),
		xsshExampleTextOrDash(pResult->sError));
}

static void xsshExamplePrintSessionResult(FILE* pFile, const char* sPrefix, xssh_session_t* pSession, int iFallbackError)
{
	xsshresult result;

	if ( pSession != NULL && xsshGetLastResult(pSession, &result) == XSSH_OK ) {
		xsshExamplePrintResult(pFile, sPrefix, &result);
		return;
	}
	if ( pFile != NULL && sPrefix != NULL ) {
		fprintf(pFile, "%s: error=%d\n", sPrefix, iFallbackError);
	}
}

#endif
