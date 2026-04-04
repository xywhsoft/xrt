#ifndef XRT_EXTLIB_XSMTP_H
#define XRT_EXTLIB_XSMTP_H

#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#if defined(XSMTP_DEBUG)
#define XSMTP_TRACE(...) do { fprintf(stderr, "[xsmtp] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)
#else
#define XSMTP_TRACE(...) ((void)0)
#endif

#define XSMTP_SECURE_AUTO      0
#define XSMTP_SECURE_NONE      1
#define XSMTP_SECURE_SSL       2
#define XSMTP_SECURE_STARTTLS  3

#define XSMTP_AUTH_AUTO        0
#define XSMTP_AUTH_NONE        1
#define XSMTP_AUTH_PLAIN       2
#define XSMTP_AUTH_LOGIN       3

#define XSMTP_CAP_AUTH_PLAIN   0x0001u
#define XSMTP_CAP_AUTH_LOGIN   0x0002u
#define XSMTP_CAP_STARTTLS     0x0004u

typedef struct xsmtpsession_struct xsmtpsession;

typedef struct {
	const char* sEmail;
	const char* sName;
} xsmtpaddr;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bAuth;
	int iAuthMode;
	bool bVerifyPeer;
	const char* sUser;
	const char* sPass;
	const char* sHeloName;
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
} xsmtpconfig;

typedef struct {
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
	const xsmtpaddr* arrTo;
	size_t iToCount;
	const xsmtpaddr* arrCc;
	size_t iCcCount;
	const xsmtpaddr* arrBcc;
	size_t iBccCount;
	const char* sSubject;
	const char* sTextBody;
	const char* sHtmlBody;
	const char* const* arrHeaderNames;
	const char* const* arrHeaderValues;
	size_t iHeaderCount;
} xsmtpmessage;

typedef struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iServerCode;
	int iAuthMode;
	uint32 iCapabilities;
	char sError[256];
	char sLastReply[1024];
} xsmtpresult;

typedef struct {
	uint32 iTimeoutMs;
	xnetengine* pEngine;
	const char* sDebugName;
} xsmtpasyncopts;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} xsmtpbuf;

typedef struct {
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult* pRet;
	xsmtpaddr* arrTo;
	xsmtpaddr* arrCc;
	xsmtpaddr* arrBcc;
	char** arrHeaderNames;
	char** arrHeaderValues;
} xsmtptaskctx;

typedef enum {
	XSMTP_AST_NONE = 0,
	XSMTP_AST_WAIT_BANNER,
	XSMTP_AST_WAIT_EHLO,
	XSMTP_AST_WAIT_STARTTLS,
	XSMTP_AST_WAIT_STARTTLS_TLS,
	XSMTP_AST_WAIT_EHLO_TLS,
	XSMTP_AST_WAIT_AUTH,
	XSMTP_AST_WAIT_AUTH_LOGIN_USER,
	XSMTP_AST_WAIT_AUTH_LOGIN_PASS,
	XSMTP_AST_WAIT_MAIL_FROM,
	XSMTP_AST_WAIT_RCPT,
	XSMTP_AST_WAIT_DATA,
	XSMTP_AST_WAIT_BODY,
	XSMTP_AST_WAIT_QUIT
} xsmtpasyncstate;

typedef struct {
	xnetengine* pEngine;
	xnetstream* pStream;
	xpromise* pPromise;
	xsmtptaskctx* pCtx;
	xsmtpresult* pRet;
	xsmtpsession* pSess;
	xsmtpbuf tReply;
	xsmtpbuf tScratch;
	int iReplyCode;
	uint32 iReplyCaps;
	bool bReplyHasFirst;
	bool bDone;
	bool bUsingNative;
	uint32 iWaitToken;
	xsmtpasyncstate iState;
	uint32 iCapabilities;
	int iAuthMode;
	size_t iRcptIndex;
	int iRcptListKind;
	xtlsconfig tTlsCfg;
} xsmtpasyncop;

typedef struct xsmtpsession_struct {
	xnetengine* pEngine;
	xnetstream* pStream;
	xtlssession* pTls;
	xmutex pLock;
	xcond pCond;
	bool bOwnEngine;
	bool bOpen;
	bool bClosed;
	bool bStreamTls;
	bool bTlsReady;
	bool bRecvOverflow;
	int iCloseReason;
	int iLastSysErr;
	uint32 iTimeoutMs;
	xsmtpbuf tWireRecv;
	char aRecv[32768];
	size_t iRecvLen;
} xsmtpsession;

static bool xsmtp_buf_reserve(xsmtpbuf* pBuf, size_t iNeed);
static bool xsmtp_buf_append_n(xsmtpbuf* pBuf, const char* sText, size_t iLen);
static bool xsmtp_buf_append(xsmtpbuf* pBuf, const char* sText);
static void xsmtp_buf_unit(xsmtpbuf* pBuf);

static void xrtSmtpConfigInit(xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = 465;
	pCfg->iTimeoutMs = 15000;
	pCfg->iSecureMode = XSMTP_SECURE_AUTO;
	pCfg->bAuth = TRUE;
	pCfg->iAuthMode = XSMTP_AUTH_AUTO;
	pCfg->bVerifyPeer = TRUE;
}

static void xrtSmtpMessageInit(xsmtpmessage* pMsg)
{
	if ( pMsg == NULL ) {
		return;
	}
	memset(pMsg, 0, sizeof(*pMsg));
}

static void xrtSmtpResultInit(xsmtpresult* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
}

static void xrtSmtpAsyncOptsInit(xsmtpasyncopts* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
}

static void xrtSmtpResultFree(xsmtpresult* pRet)
{
	if ( pRet != NULL ) {
		xrtFree(pRet);
	}
}

static void xsmtp_stream_close_destroy(xnetstream** ppStream, uint32 iTimeoutMs)
{
	xnetstream* pStream;

	if ( ppStream == NULL || *ppStream == NULL ) {
		return;
	}

	pStream = *ppStream;
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	(void)xrtNetStreamWaitTimeoutEx(pStream, XNET_STREAM_WAIT_CLOSE, iTimeoutMs > 0 ? iTimeoutMs : 1000u);
	if ( xrtGetError() ) {
		xrtClearError();
	}
	xrtNetStreamDestroy(pStream);
	if ( xrtGetError() ) {
		xrtClearError();
	}
	*ppStream = NULL;
}

static const char* xsmtp_str_or_empty(const char* sText)
{
	return sText ? sText : "";
}

static char* xsmtp_dup_text(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		return NULL;
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

static bool xsmtp_copy_addr(xsmtpaddr* pDst, const xsmtpaddr* pSrc)
{
	memset(pDst, 0, sizeof(*pDst));
	if ( pSrc == NULL ) {
		return TRUE;
	}
	if ( pSrc->sEmail != NULL ) {
		pDst->sEmail = xsmtp_dup_text(pSrc->sEmail);
		if ( pDst->sEmail == NULL ) {
			return FALSE;
		}
	}
	if ( pSrc->sName != NULL ) {
		pDst->sName = xsmtp_dup_text(pSrc->sName);
		if ( pDst->sName == NULL ) {
			xrtFree((ptr)pDst->sEmail);
			pDst->sEmail = NULL;
			return FALSE;
		}
	}
	return TRUE;
}

static void xsmtp_free_addr(xsmtpaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return;
	}
	if ( pAddr->sEmail != NULL ) {
		xrtFree((ptr)pAddr->sEmail);
	}
	if ( pAddr->sName != NULL ) {
		xrtFree((ptr)pAddr->sName);
	}
	memset(pAddr, 0, sizeof(*pAddr));
}

static xsmtpaddr* xsmtp_dup_addr_list(const xsmtpaddr* arrSrc, size_t iCount)
{
	xsmtpaddr* arrDst;
	size_t i;

	if ( arrSrc == NULL || iCount == 0 ) {
		return NULL;
	}
	arrDst = (xsmtpaddr*)xrtMalloc(sizeof(xsmtpaddr) * iCount);
	if ( arrDst == NULL ) {
		return NULL;
	}
	memset(arrDst, 0, sizeof(xsmtpaddr) * iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( !xsmtp_copy_addr(&arrDst[i], &arrSrc[i]) ) {
			while ( i > 0 ) {
				--i;
				xsmtp_free_addr(&arrDst[i]);
			}
			xrtFree(arrDst);
			return NULL;
		}
	}
	return arrDst;
}

static void xsmtp_free_addr_list(xsmtpaddr* arrList, size_t iCount)
{
	size_t i;

	if ( arrList == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		xsmtp_free_addr(&arrList[i]);
	}
	xrtFree(arrList);
}

static bool xsmtp_copy_text_list(char*** ppDst, const char* const* arrSrc, size_t iCount)
{
	char** arrDst;
	size_t i;

	*ppDst = NULL;
	if ( arrSrc == NULL || iCount == 0 ) {
		return TRUE;
	}
	arrDst = (char**)xrtMalloc(sizeof(char*) * iCount);
	if ( arrDst == NULL ) {
		return FALSE;
	}
	memset(arrDst, 0, sizeof(char*) * iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( arrSrc[i] != NULL ) {
			arrDst[i] = xsmtp_dup_text(arrSrc[i]);
			if ( arrDst[i] == NULL ) {
				while ( i > 0 ) {
					--i;
					if ( arrDst[i] != NULL ) {
						xrtFree(arrDst[i]);
					}
				}
				xrtFree(arrDst);
				return FALSE;
			}
		}
	}
	*ppDst = arrDst;
	return TRUE;
}

static void xsmtp_free_text_list(char** arrText, size_t iCount)
{
	size_t i;

	if ( arrText == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( arrText[i] != NULL ) {
			xrtFree(arrText[i]);
		}
	}
	xrtFree(arrText);
}

static void xsmtp_result_error(xsmtpresult* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s", xsmtp_str_or_empty(sError));
}

static void xsmtp_task_ctx_free(xsmtptaskctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	xsmtp_free_addr(&pCtx->tCfg.tFrom);
	xsmtp_free_addr(&pCtx->tCfg.tReplyTo);
	if ( pCtx->tCfg.sHost != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sHost);
	}
	if ( pCtx->tCfg.sUser != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sUser);
	}
	if ( pCtx->tCfg.sPass != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sPass);
	}
	if ( pCtx->tCfg.sHeloName != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sHeloName);
	}
	xsmtp_free_addr(&pCtx->tMsg.tFrom);
	xsmtp_free_addr(&pCtx->tMsg.tReplyTo);
	xsmtp_free_addr_list(pCtx->arrTo, pCtx->tMsg.iToCount);
	xsmtp_free_addr_list(pCtx->arrCc, pCtx->tMsg.iCcCount);
	xsmtp_free_addr_list(pCtx->arrBcc, pCtx->tMsg.iBccCount);
	xsmtp_free_text_list(pCtx->arrHeaderNames, pCtx->tMsg.iHeaderCount);
	xsmtp_free_text_list(pCtx->arrHeaderValues, pCtx->tMsg.iHeaderCount);
	if ( pCtx->tMsg.sSubject != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sSubject);
	}
	if ( pCtx->tMsg.sTextBody != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sTextBody);
	}
	if ( pCtx->tMsg.sHtmlBody != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sHtmlBody);
	}
	xrtFree(pCtx);
}

static xsmtptaskctx* xsmtp_task_ctx_create(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtptaskctx* pCtx;

	if ( pCfg == NULL || pMsg == NULL ) {
		return NULL;
	}

	pCtx = (xsmtptaskctx*)xrtMalloc(sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	memset(pCtx, 0, sizeof(*pCtx));

	pCtx->tCfg = *pCfg;
	pCtx->tMsg = *pMsg;
	pCtx->tCfg.sHost = xsmtp_dup_text(pCfg->sHost);
	pCtx->tCfg.sUser = xsmtp_dup_text(pCfg->sUser);
	pCtx->tCfg.sPass = xsmtp_dup_text(pCfg->sPass);
	pCtx->tCfg.sHeloName = xsmtp_dup_text(pCfg->sHeloName);
	if ( !xsmtp_copy_addr(&pCtx->tCfg.tFrom, &pCfg->tFrom) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( !xsmtp_copy_addr(&pCtx->tCfg.tReplyTo, &pCfg->tReplyTo) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sHost != NULL && pCtx->tCfg.sHost == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sUser != NULL && pCtx->tCfg.sUser == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sPass != NULL && pCtx->tCfg.sPass == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sHeloName != NULL && pCtx->tCfg.sHeloName == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}

	if ( pOpts != NULL && pOpts->iTimeoutMs > 0 ) {
		pCtx->tCfg.iTimeoutMs = pOpts->iTimeoutMs;
	}

	if ( !xsmtp_copy_addr(&pCtx->tMsg.tFrom, &pMsg->tFrom) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( !xsmtp_copy_addr(&pCtx->tMsg.tReplyTo, &pMsg->tReplyTo) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->arrTo = xsmtp_dup_addr_list(pMsg->arrTo, pMsg->iToCount);
	pCtx->arrCc = xsmtp_dup_addr_list(pMsg->arrCc, pMsg->iCcCount);
	pCtx->arrBcc = xsmtp_dup_addr_list(pMsg->arrBcc, pMsg->iBccCount);
	if ( (pMsg->iToCount > 0 && pCtx->arrTo == NULL)
		|| (pMsg->iCcCount > 0 && pCtx->arrCc == NULL)
		|| (pMsg->iBccCount > 0 && pCtx->arrBcc == NULL) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tMsg.arrTo = pCtx->arrTo;
	pCtx->tMsg.arrCc = pCtx->arrCc;
	pCtx->tMsg.arrBcc = pCtx->arrBcc;

	pCtx->tMsg.sSubject = xsmtp_dup_text(pMsg->sSubject);
	pCtx->tMsg.sTextBody = xsmtp_dup_text(pMsg->sTextBody);
	pCtx->tMsg.sHtmlBody = xsmtp_dup_text(pMsg->sHtmlBody);
	if ( (pMsg->sSubject != NULL && pCtx->tMsg.sSubject == NULL)
		|| (pMsg->sTextBody != NULL && pCtx->tMsg.sTextBody == NULL)
		|| (pMsg->sHtmlBody != NULL && pCtx->tMsg.sHtmlBody == NULL) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}

	if ( !xsmtp_copy_text_list(&pCtx->arrHeaderNames, pMsg->arrHeaderNames, pMsg->iHeaderCount)
		|| !xsmtp_copy_text_list(&pCtx->arrHeaderValues, pMsg->arrHeaderValues, pMsg->iHeaderCount) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tMsg.arrHeaderNames = (const char* const*)pCtx->arrHeaderNames;
	pCtx->tMsg.arrHeaderValues = (const char* const*)pCtx->arrHeaderValues;

	return pCtx;
}

static bool xsmtp_try_read_line_from_buffer(xsmtpsession* pSess, char* sOut, size_t iOutCap)
{
	size_t i;

	if ( pSess == NULL || sOut == NULL || iOutCap == 0 ) {
		return FALSE;
	}
	for ( i = 0; i + 1 < pSess->iRecvLen; ++i ) {
		if ( pSess->aRecv[i] == '\r' && pSess->aRecv[i + 1] == '\n' ) {
			size_t iCopy = i;
			if ( iCopy >= iOutCap ) {
				iCopy = iOutCap - 1;
			}
			memcpy(sOut, pSess->aRecv, iCopy);
			sOut[iCopy] = '\0';
			memmove(pSess->aRecv, pSess->aRecv + i + 2, pSess->iRecvLen - (i + 2));
			pSess->iRecvLen -= (i + 2);
			pSess->aRecv[pSess->iRecvLen] = '\0';
			return TRUE;
		}
	}
	return FALSE;
}

static char* xsmtp_build_data_body_bytes(const char* sData)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_reserve(&tBuf, strlen(xsmtp_str_or_empty(sData)) + 16) ) {
		return NULL;
	}
	for ( p = xsmtp_str_or_empty(sData); *p; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0 && p[0] == '.' ) {
			if ( !xsmtp_buf_append(&tBuf, ".") ) {
				xsmtp_buf_unit(&tBuf);
				return NULL;
			}
		}
		if ( !xsmtp_buf_append_n(&tBuf, p, iLineLen) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
		if ( !sLineEnd ) {
			break;
		}
		p = sLineEnd + 2;
	}
	if ( !xsmtp_buf_append(&tBuf, ".\r\n") ) {
		xsmtp_buf_unit(&tBuf);
		return NULL;
	}
	return tBuf.sData;
}

static bool xsmtp_starts_with_i(const char* sText, const char* sPrefix)
{
	size_t i;

	if ( sText == NULL || sPrefix == NULL ) {
		return FALSE;
	}
	for ( i = 0; sPrefix[i] != '\0'; i++ ) {
		unsigned char a = (unsigned char)sText[i];
		unsigned char b = (unsigned char)sPrefix[i];
		if ( a == '\0' ) {
			return FALSE;
		}
		if ( tolower(a) != tolower(b) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_find_i(const char* sText, const char* sSub)
{
	size_t i;
	size_t j;

	if ( sText == NULL || sSub == NULL || sSub[0] == '\0' ) {
		return FALSE;
	}
	for ( i = 0; sText[i] != '\0'; i++ ) {
		for ( j = 0; sSub[j] != '\0'; j++ ) {
			if ( sText[i + j] == '\0' ) {
				return FALSE;
			}
			if ( tolower((unsigned char)sText[i + j]) != tolower((unsigned char)sSub[j]) ) {
				break;
			}
		}
		if ( sSub[j] == '\0' ) {
			return TRUE;
		}
	}
	return FALSE;
}

static bool xsmtp_buf_reserve(xsmtpbuf* pBuf, size_t iNeed)
{
	char* sNew;
	size_t iCap;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( pBuf->iLen + iNeed + 1 <= pBuf->iCap ) {
		return TRUE;
	}
	iCap = pBuf->iCap ? pBuf->iCap : 256;
	while ( iCap < pBuf->iLen + iNeed + 1 ) {
		iCap *= 2;
	}
	sNew = (char*)xrtRealloc(pBuf->sData, iCap);
	if ( sNew == NULL ) {
		return FALSE;
	}
	pBuf->sData = sNew;
	pBuf->iCap = iCap;
	return TRUE;
}

static bool xsmtp_buf_append_n(xsmtpbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL || sText == NULL ) {
		return FALSE;
	}
	if ( !xsmtp_buf_reserve(pBuf, iLen) ) {
		return FALSE;
	}
	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool xsmtp_buf_append(xsmtpbuf* pBuf, const char* sText)
{
	return xsmtp_buf_append_n(pBuf, xsmtp_str_or_empty(sText), strlen(xsmtp_str_or_empty(sText)));
}

static bool xsmtp_buf_appendf(xsmtpbuf* pBuf, const char* sFormat, ...)
{
	va_list ap;
	va_list ap2;
	int iNeed;
	char* sText;
	bool bOK;

	if ( pBuf == NULL || sFormat == NULL ) {
		return FALSE;
	}

	va_start(ap, sFormat);
	va_copy(ap2, ap);
	iNeed = vsnprintf(NULL, 0, sFormat, ap);
	va_end(ap);
	if ( iNeed < 0 ) {
		va_end(ap2);
		return FALSE;
	}
	sText = (char*)xrtMalloc((size_t)iNeed + 1);
	if ( sText == NULL ) {
		va_end(ap2);
		return FALSE;
	}
	vsnprintf(sText, (size_t)iNeed + 1, sFormat, ap2);
	va_end(ap2);
	if ( sText == NULL ) {
		return FALSE;
	}
	bOK = xsmtp_buf_append(pBuf, sText);
	xrtFree(sText);
	return bOK;
}

static void xsmtp_buf_unit(xsmtpbuf* pBuf)
{
	if ( pBuf && pBuf->sData ) {
		xrtFree(pBuf->sData);
		pBuf->sData = NULL;
		pBuf->iLen = 0;
		pBuf->iCap = 0;
	}
}

static bool xsmtp_has_non_ascii(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( sText == NULL ) {
		return FALSE;
	}
	while ( *p ) {
		if ( *p >= 0x80 ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static bool xsmtp_needs_addr_quote(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( sText == NULL || sText[0] == '\0' ) {
		return FALSE;
	}
	while ( *p ) {
		if ( *p < 0x20 || *p == '"' || *p == '\\' || *p == ',' || *p == '<' || *p == '>' || *p == '@' ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static str xsmtp_encode_header_word(const char* sText)
{
	str sBase64;
	str sOut;
	size_t iNeed;

	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	if ( !xsmtp_has_non_ascii(sText) ) {
		return xrtCopyStr((str)sText, 0);
	}
	sBase64 = xrtBase64Encode((ptr)sText, strlen(sText), NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	iNeed = strlen((const char*)sBase64) + 13;
	sOut = (str)xrtMalloc(iNeed);
	if ( sOut ) {
		snprintf((char*)sOut, iNeed, "=?UTF-8?B?%s?=", (const char*)sBase64);
	}
	xrtFree(sBase64);
	return sOut;
}

static str xsmtp_format_mailbox(const xsmtpaddr* pAddr)
{
	str sName = NULL;
	str sOut = NULL;
	const char* sEmail;

	if ( pAddr == NULL || pAddr->sEmail == NULL || pAddr->sEmail[0] == '\0' ) {
		return xrtCopyStr((str)"", 0);
	}

	sEmail = pAddr->sEmail;
	if ( pAddr->sName == NULL || pAddr->sName[0] == '\0' ) {
		return xrtCopyStr((str)sEmail, 0);
	}

	sName = xsmtp_encode_header_word(pAddr->sName);
	if ( sName == NULL ) {
		return NULL;
	}

	if ( !xsmtp_has_non_ascii(pAddr->sName) && xsmtp_needs_addr_quote(pAddr->sName) ) {
		sOut = xrtFormat("\"%s\" <%s>", pAddr->sName, sEmail);
	} else {
		sOut = xrtFormat("%s <%s>", sName, sEmail);
	}

	xrtFree(sName);
	return sOut;
}

static str xsmtp_join_mailboxes(const xsmtpaddr* arrList, size_t iCount)
{
	xsmtpbuf tBuf;
	size_t i;

	memset(&tBuf, 0, sizeof(tBuf));
	for ( i = 0; i < iCount; i++ ) {
		str sItem = xsmtp_format_mailbox(&arrList[i]);
		if ( sItem == NULL ) {
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
		if ( i > 0 ) {
			xsmtp_buf_append(&tBuf, ", ");
		}
		xsmtp_buf_append(&tBuf, (const char*)sItem);
		xrtFree(sItem);
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xsmtp_base64_wrap(const void* pData, size_t iLen)
{
	xsmtpbuf tBuf;
	str sBase64;
	size_t i;

	memset(&tBuf, 0, sizeof(tBuf));
	sBase64 = xrtBase64Encode((ptr)pData, iLen, NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	for ( i = 0; sBase64[i] != '\0'; i += 76 ) {
		size_t iChunk = strlen((const char*)sBase64 + i);
		if ( iChunk > 76 ) {
			iChunk = 76;
		}
		if ( !xsmtp_buf_append_n(&tBuf, (const char*)sBase64 + i, iChunk) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
			xrtFree(sBase64);
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
	}
	xrtFree(sBase64);
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xsmtp_normalize_crlf(const char* sText)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}

	for ( p = sText; *p; p++ ) {
		if ( *p == '\r' ) {
			if ( p[1] == '\n' ) {
				xsmtp_buf_append(&tBuf, "\r\n");
				p++;
			} else {
				xsmtp_buf_append(&tBuf, "\r\n");
			}
		} else if ( *p == '\n' ) {
			xsmtp_buf_append(&tBuf, "\r\n");
		} else {
			xsmtp_buf_append_n(&tBuf, p, 1);
		}
	}

	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static bool xsmtp_count_recipients(const xsmtpmessage* pMsg, size_t* pCount)
{
	size_t iTotal = 0;
	if ( pMsg == NULL || pCount == NULL ) {
		return FALSE;
	}
	iTotal += pMsg->iToCount;
	iTotal += pMsg->iCcCount;
	iTotal += pMsg->iBccCount;
	*pCount = iTotal;
	return iTotal > 0;
}

static void xsmtp_random_hex(char* sOut, size_t iOutLen, size_t iByteCount)
{
	uint8 aBuf[32];
	static const char sHex[] = "0123456789abcdef";
	size_t i;

	if ( sOut == NULL || iOutLen == 0 ) {
		return;
	}
	if ( iByteCount > sizeof(aBuf) ) {
		iByteCount = sizeof(aBuf);
	}
	xrtRandomBytes(aBuf, iByteCount);
	for ( i = 0; i < iByteCount && (i * 2 + 1) < iOutLen; i++ ) {
		sOut[i * 2] = sHex[(aBuf[i] >> 4) & 0x0F];
		sOut[i * 2 + 1] = sHex[aBuf[i] & 0x0F];
	}
	sOut[(iByteCount * 2 < iOutLen) ? iByteCount * 2 : iOutLen - 1] = '\0';
}

static str xsmtp_rfc2822_date(void)
{
	time_t tNow;
	struct tm tmNow;
	struct tm tmUtc;
	char sBuf[80];
	long iOffsetSec;
	char cSign = '+';
	int iOffsetHour;
	int iOffsetMin;
	static const char* const arrWeekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* const arrMonth[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	time(&tNow);
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tmNow, &tNow);
	gmtime_s(&tmUtc, &tNow);
#else
	localtime_r(&tNow, &tmNow);
	gmtime_r(&tNow, &tmUtc);
#endif
	iOffsetSec = (long)difftime(mktime(&tmNow), mktime(&tmUtc));
	if ( iOffsetSec < 0 ) {
		cSign = '-';
		iOffsetSec = -iOffsetSec;
	}
	iOffsetHour = (int)(iOffsetSec / 3600);
	iOffsetMin = (int)((iOffsetSec % 3600) / 60);
	if ( tmNow.tm_wday < 0 || tmNow.tm_wday > 6 || tmNow.tm_mon < 0 || tmNow.tm_mon > 11 ) {
		return xrtCopyStr((str)"", 0);
	}
	snprintf(sBuf, sizeof(sBuf), "%s, %02d %s %04d %02d:%02d:%02d %c%02d%02d",
		arrWeekday[tmNow.tm_wday],
		tmNow.tm_mday,
		arrMonth[tmNow.tm_mon],
		tmNow.tm_year + 1900,
		tmNow.tm_hour,
		tmNow.tm_min,
		tmNow.tm_sec,
		cSign,
		iOffsetHour,
		iOffsetMin);
	return xrtCopyStr((str)sBuf, 0);
}

static str xsmtp_build_message_id(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg)
{
	char sRand[33];
	const char* sHost = NULL;
	xsmtp_random_hex(sRand, sizeof(sRand), 16);
	if ( pMsg && pMsg->tFrom.sEmail && strchr(pMsg->tFrom.sEmail, '@') ) {
		sHost = strchr(pMsg->tFrom.sEmail, '@') + 1;
	} else if ( pCfg && pCfg->tFrom.sEmail && strchr(pCfg->tFrom.sEmail, '@') ) {
		sHost = strchr(pCfg->tFrom.sEmail, '@') + 1;
	} else if ( pCfg && pCfg->sHost ) {
		sHost = pCfg->sHost;
	} else {
		sHost = "localhost";
	}
	return xrtFormat("<%s@%s>", sRand, sHost);
}

static str xsmtp_build_body_payload(const char* sBody)
{
	str sNorm;
	str sOut;
	sNorm = xsmtp_normalize_crlf(sBody);
	if ( sNorm == NULL ) {
		return NULL;
	}
	sOut = xsmtp_base64_wrap((const void*)sNorm, strlen((const char*)sNorm));
	xrtFree(sNorm);
	return sOut;
}

static bool xsmtp_is_7bit_safe_text(const char* sText)
{
	const unsigned char* p = (const unsigned char*)xsmtp_str_or_empty(sText);
	for ( ; *p; ++p ) {
		if ( *p == '\r' || *p == '\n' || *p == '\t' ) {
			continue;
		}
		if ( *p < 32 || *p > 126 ) {
			return FALSE;
		}
	}
	return TRUE;
}

static str xsmtp_build_quoted_printable(const char* sText)
{
	static const char sHex[] = "0123456789ABCDEF";
	xsmtpbuf tBuf;
	const unsigned char* p = (const unsigned char*)xsmtp_str_or_empty(sText);
	size_t iLineLen = 0;

	memset(&tBuf, 0, sizeof(tBuf));
	while ( *p ) {
		unsigned char c = *p++;
		char aEnc[3];
		size_t iNeed = 1;
		const char* sAppend = NULL;

		if ( c == '\r' ) {
			if ( *p == '\n' ) {
				++p;
			}
			if ( !xsmtp_buf_append(&tBuf, "\r\n") ) {
				xsmtp_buf_unit(&tBuf);
				return NULL;
			}
			iLineLen = 0;
			continue;
		}
		if ( c == '\n' ) {
			if ( !xsmtp_buf_append(&tBuf, "\r\n") ) {
				xsmtp_buf_unit(&tBuf);
				return NULL;
			}
			iLineLen = 0;
			continue;
		}
		if ( (c >= 33 && c <= 60) || (c >= 62 && c <= 126) ) {
			aEnc[0] = (char)c;
			aEnc[1] = '\0';
			sAppend = aEnc;
			iNeed = 1;
		} else {
			aEnc[0] = '=';
			aEnc[1] = sHex[(c >> 4) & 0x0F];
			aEnc[2] = sHex[c & 0x0F];
			sAppend = aEnc;
			iNeed = 3;
		}
		if ( iLineLen + iNeed > 72 ) {
			if ( !xsmtp_buf_append(&tBuf, "=\r\n") ) {
				xsmtp_buf_unit(&tBuf);
				return NULL;
			}
			iLineLen = 0;
		}
		if ( !xsmtp_buf_append_n(&tBuf, sAppend, iNeed) ) {
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
		iLineLen += iNeed;
	}

	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xsmtp_build_body_payload_ex(const char* sBody, const char* sContentType, const char** psEncoding)
{
	str sNorm;
	str sOut;

	if ( psEncoding ) {
		*psEncoding = "base64";
	}
	sNorm = xsmtp_normalize_crlf(sBody);
	if ( sNorm == NULL ) {
		return NULL;
	}
	if ( xsmtp_is_7bit_safe_text((const char*)sNorm) ) {
		if ( psEncoding ) {
			*psEncoding = "7bit";
		}
		return sNorm;
	}
	if ( sContentType && strcmp(sContentType, "text/plain") == 0 ) {
		if ( psEncoding ) {
			*psEncoding = "quoted-printable";
		}
		sOut = xsmtp_build_quoted_printable((const char*)sNorm);
		xrtFree(sNorm);
		return sOut;
	}
	sOut = xsmtp_base64_wrap((const void*)sNorm, strlen((const char*)sNorm));
	xrtFree(sNorm);
	return sOut;
}

static bool xsmtp_append_body_headers(xsmtpbuf* pBuf, const char* sContentType, const char* sTransferEncoding, const char* sPayload)
{
	return xsmtp_buf_appendf(pBuf,
		"Content-Type: %s; charset=UTF-8\r\n"
		"Content-Transfer-Encoding: %s\r\n"
		"\r\n"
		"%s\r\n",
		sContentType, xsmtp_str_or_empty(sTransferEncoding), xsmtp_str_or_empty(sPayload));
}

static str xsmtp_build_message_data(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg)
{
	xsmtpbuf tBuf;
	str sDate = NULL;
	str sMessageID = NULL;
	str sSubject = NULL;
	str sFrom = NULL;
	str sTo = NULL;
	str sCc = NULL;
	str sReplyTo = NULL;
	str sTextPayload = NULL;
	str sHtmlPayload = NULL;
	bool bOK = FALSE;
	const char* sTextEncoding = "base64";
	const char* sHtmlEncoding = "base64";

	memset(&tBuf, 0, sizeof(tBuf));

	sDate = xsmtp_rfc2822_date();
	sMessageID = xsmtp_build_message_id(pCfg, pMsg);
	sSubject = xsmtp_encode_header_word(xsmtp_str_or_empty(pMsg->sSubject));
	sFrom = xsmtp_format_mailbox((pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0]) ? &pMsg->tFrom : &pCfg->tFrom);
	sTo = xsmtp_join_mailboxes(pMsg->arrTo, pMsg->iToCount);
	sCc = xsmtp_join_mailboxes(pMsg->arrCc, pMsg->iCcCount);
	if ( pMsg->tReplyTo.sEmail && pMsg->tReplyTo.sEmail[0] ) {
		sReplyTo = xsmtp_format_mailbox(&pMsg->tReplyTo);
	} else if ( pCfg->tReplyTo.sEmail && pCfg->tReplyTo.sEmail[0] ) {
		sReplyTo = xsmtp_format_mailbox(&pCfg->tReplyTo);
	}

	if ( sDate == NULL || sMessageID == NULL || sSubject == NULL || sFrom == NULL || sTo == NULL ) {
		goto cleanup;
	}

	if ( !xsmtp_buf_appendf(&tBuf, "Date: %s\r\n", sDate) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "Message-ID: %s\r\n", sMessageID) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "From: %s\r\n", sFrom) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "To: %s\r\n", sTo) ) goto cleanup;
	if ( sCc && sCc[0] && !xsmtp_buf_appendf(&tBuf, "Cc: %s\r\n", sCc) ) goto cleanup;
	if ( sReplyTo && sReplyTo[0] && !xsmtp_buf_appendf(&tBuf, "Reply-To: %s\r\n", sReplyTo) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "Subject: %s\r\n", sSubject) ) goto cleanup;
	if ( !xsmtp_buf_append(&tBuf, "MIME-Version: 1.0\r\n") ) goto cleanup;

	if ( pMsg->arrHeaderNames && pMsg->arrHeaderValues ) {
		size_t i;
		for ( i = 0; i < pMsg->iHeaderCount; i++ ) {
			const char* sName = pMsg->arrHeaderNames[i];
			const char* sValue = pMsg->arrHeaderValues[i];
			if ( sName && sName[0] && sValue ) {
				if ( !xsmtp_buf_appendf(&tBuf, "%s: %s\r\n", sName, sValue) ) goto cleanup;
			}
		}
	}

	if ( pMsg->sTextBody && pMsg->sTextBody[0] && pMsg->sHtmlBody && pMsg->sHtmlBody[0] ) {
		char sBoundary[49];
		xsmtp_random_hex(sBoundary, sizeof(sBoundary), 24);
		sTextPayload = xsmtp_build_body_payload_ex(pMsg->sTextBody, "text/plain", &sTextEncoding);
		sHtmlPayload = xsmtp_build_body_payload_ex(pMsg->sHtmlBody, "text/html", &sHtmlEncoding);
		if ( sTextPayload == NULL || sHtmlPayload == NULL ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "Content-Type: multipart/alternative; boundary=\"%s\"\r\n\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/plain", sTextEncoding, (const char*)sTextPayload) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/html", sHtmlEncoding, (const char*)sHtmlPayload) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s--\r\n", sBoundary) ) goto cleanup;
	} else if ( pMsg->sHtmlBody && pMsg->sHtmlBody[0] ) {
		sHtmlPayload = xsmtp_build_body_payload_ex(pMsg->sHtmlBody, "text/html", &sHtmlEncoding);
		if ( sHtmlPayload == NULL ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/html", sHtmlEncoding, (const char*)sHtmlPayload) ) goto cleanup;
	} else {
		sTextPayload = xsmtp_build_body_payload_ex(xsmtp_str_or_empty(pMsg->sTextBody), "text/plain", &sTextEncoding);
		if ( sTextPayload == NULL ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/plain", sTextEncoding, (const char*)sTextPayload) ) goto cleanup;
	}

	bOK = TRUE;

cleanup:
	if ( sDate ) xrtFree(sDate);
	if ( sMessageID ) xrtFree(sMessageID);
	if ( sSubject ) xrtFree(sSubject);
	if ( sFrom ) xrtFree(sFrom);
	if ( sTo ) xrtFree(sTo);
	if ( sCc ) xrtFree(sCc);
	if ( sReplyTo ) xrtFree(sReplyTo);
	if ( sTextPayload ) xrtFree(sTextPayload);
	if ( sHtmlPayload ) xrtFree(sHtmlPayload);
	if ( !bOK ) {
		xsmtp_buf_unit(&tBuf);
		return NULL;
	}
	return (str)tBuf.sData;
}

static int xsmtp_secure_mode_auto(const xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iSecureMode != XSMTP_SECURE_AUTO ) {
		return pCfg->iSecureMode;
	}
	if ( pCfg->iPort == 465 ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iPort == 587 ) {
		return XSMTP_SECURE_STARTTLS;
	}
	return XSMTP_SECURE_NONE;
}

static int xsmtp_auth_mode_auto(const xsmtpconfig* pCfg, uint32 iCaps)
{
	if ( pCfg == NULL ) {
		return XSMTP_AUTH_NONE;
	}
	if ( !pCfg->bAuth || pCfg->sUser == NULL || pCfg->sUser[0] == '\0' ) {
		return XSMTP_AUTH_NONE;
	}
	if ( pCfg->iAuthMode != XSMTP_AUTH_AUTO ) {
		return pCfg->iAuthMode;
	}
	if ( iCaps & XSMTP_CAP_AUTH_PLAIN ) {
		return XSMTP_AUTH_PLAIN;
	}
	if ( iCaps & XSMTP_CAP_AUTH_LOGIN ) {
		return XSMTP_AUTH_LOGIN;
	}
	return XSMTP_AUTH_LOGIN;
}

static uint64 xsmtp_now_ms(void)
{
	double fNow = xrtTimer();
	if ( fNow <= 0.0 ) {
		return 0;
	}
	return (uint64)(fNow * 1000.0);
}

static void xsmtp_session_reset_state(xsmtpsession* pSess)
{
	if ( pSess == NULL ) {
		return;
	}
	pSess->bOpen = FALSE;
	pSess->bClosed = FALSE;
	pSess->bStreamTls = FALSE;
	pSess->bTlsReady = FALSE;
	pSess->bRecvOverflow = FALSE;
	pSess->iCloseReason = 0;
	pSess->iLastSysErr = 0;
	pSess->iRecvLen = 0;
	if ( pSess->tWireRecv.sData ) {
		pSess->tWireRecv.iLen = 0;
		pSess->tWireRecv.sData[0] = '\0';
	}
}

static void xsmtp_stream_signal(xsmtpsession* pSess)
{
	if ( pSess == NULL || pSess->pCond == NULL ) {
		return;
	}
	xrtCondBroadcast(pSess->pCond);
}

static void xsmtp_stream_on_open(ptr pOwner, xnetstream* pStream)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->bOpen = TRUE;
	if ( pSess->bStreamTls ) {
		pSess->bTlsReady = TRUE;
	}
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	char aBuf[4096];
	size_t iLeft;
	(void)pStream;

	if ( pSess == NULL || pSess->pLock == NULL || pChain == NULL ) {
		return;
	}

	xrtMutexLock(pSess->pLock);
	iLeft = xrtNetChainBytes(pChain);
	while ( iLeft > 0 ) {
		size_t iChunk = iLeft > sizeof(aBuf) ? sizeof(aBuf) : iLeft;
		size_t iRead = xrtNetChainPeek(pChain, aBuf, iChunk);
		if ( iRead == 0 ) {
			break;
		}
		if ( !xsmtp_buf_append_n(&pSess->tWireRecv, aBuf, iRead) ) {
			pSess->bRecvOverflow = TRUE;
			xsmtp_stream_signal(pSess);
			xrtMutexUnlock(pSess->pLock);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtNetChainConsume(pChain, iRead);
		iLeft -= iRead;
	}
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_close(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->bClosed = TRUE;
	pSess->iCloseReason = (int)iReason;
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->iLastSysErr = iSysErr;
	XSMTP_TRACE("stream error sys=%d", iSysErr);
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static const xnetstreamevents* xsmtp_stream_events(void)
{
	static const xnetstreamevents tEvents = {
		xsmtp_stream_on_open,
		xsmtp_stream_on_recv,
		NULL,
		xsmtp_stream_on_close,
		xsmtp_stream_on_error,
		NULL,
		NULL
	};
	return &tEvents;
}

static void xsmtp_result_errorf(xsmtpresult* pRet, const char* sFormat, ...)
{
	va_list ap;
	char sBuf[256];

	if ( pRet == NULL || sFormat == NULL ) {
		return;
	}
	va_start(ap, sFormat);
	vsnprintf(sBuf, sizeof(sBuf), sFormat, ap);
	va_end(ap);
	xsmtp_result_error(pRet, sBuf);
}

static bool xsmtp_stream_wait_connect_state(xsmtpsession* pSess, uint64 iDeadlineMs)
{
	uint32 iRemain = 0;
	int iWaitRet;

	if ( pSess == NULL || pSess->pLock == NULL || pSess->pCond == NULL ) {
		return FALSE;
	}

	for (;;) {
		if ( pSess->bOpen || pSess->bClosed || pSess->iLastSysErr != 0 || pSess->bRecvOverflow ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0 ) {
			xrtCondWait(pSess->pCond, pSess->pLock);
			continue;
		}
		if ( xsmtp_now_ms() >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - xsmtp_now_ms());
		if ( iRemain == 0 ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSess->pCond, pSess->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
			return FALSE;
		}
		if ( iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xsmtp_stream_wait_recv_state(xsmtpsession* pSess, uint64 iDeadlineMs)
{
	uint32 iRemain = 0;
	int iWaitRet;

	if ( pSess == NULL || pSess->pLock == NULL || pSess->pCond == NULL ) {
		return FALSE;
	}

	for (;;) {
		if ( pSess->tWireRecv.iLen > 0 || pSess->bClosed || pSess->iLastSysErr != 0 || pSess->bRecvOverflow ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0 ) {
			xrtCondWait(pSess->pCond, pSess->pLock);
			continue;
		}
		if ( xsmtp_now_ms() >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - xsmtp_now_ms());
		if ( iRemain == 0 ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSess->pCond, pSess->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
			return FALSE;
		}
		if ( iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xsmtp_stream_connect_once(xsmtpsession* pSess, const xsmtpconfig* pCfg, int iSecureMode, const char* sConnectHost, xsmtpresult* pRet)
{
	xnetengineconfig tEngineCfg;
	xnetconnectconfig tConnCfg;
	xtlsconfig tTlsCfg;
	uint64 iDeadlineMs;
	str sErr;
	const char* sErrText;

	if ( pSess == NULL || pCfg == NULL || sConnectHost == NULL || sConnectHost[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP host is empty");
		return FALSE;
	}

	pSess->iTimeoutMs = pCfg->iTimeoutMs ? pCfg->iTimeoutMs : 15000;
	xsmtp_session_reset_state(pSess);
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1;
	pSess->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pSess->pEngine == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP engine create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}
	pSess->bOwnEngine = TRUE;
	if ( xrtNetEngineStart(pSess->pEngine) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP engine start failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	pSess->pStream = xrtNetStreamCreate(pSess->pEngine, xsmtp_stream_events(), pSess);
	if ( pSess->pStream == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = sConnectHost;
	tConnCfg.iPort = pCfg->iPort;
	tConnCfg.iConnectTimeoutMs = pSess->iTimeoutMs;
	tConnCfg.iRecvLimit = sizeof(pSess->aRecv) * 4u;
	if ( iSecureMode == XSMTP_SECURE_SSL ) {
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sHostName = pCfg->sHost;
		tTlsCfg.bVerifyPeer = pCfg->bVerifyPeer;
		tConnCfg.pTlsConfig = &tTlsCfg;
		pSess->bStreamTls = TRUE;
	}
	XSMTP_TRACE("connect submit host=%s port=%u secure=%d", sConnectHost, (unsigned)pCfg->iPort, iSecureMode);
	if ( xrtNetStreamConnect(pSess->pStream, &tConnCfg) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream connect submit failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	iDeadlineMs = xsmtp_now_ms() + pSess->iTimeoutMs;
	xrtMutexLock(pSess->pLock);
	if ( !xsmtp_stream_wait_connect_state(pSess, iDeadlineMs) ) {
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP connect timeout (%u ms)", pSess->iTimeoutMs);
		return FALSE;
	}
	if ( pSess->iLastSysErr != 0 ) {
		int iSysErr = pSess->iLastSysErr;
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP stream connect failed (sys=%d)", iSysErr);
		return FALSE;
	}
	if ( pSess->bClosed && !pSess->bOpen ) {
		int iCloseReason = pSess->iCloseReason;
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP stream closed during connect (reason=%d)", iCloseReason);
		return FALSE;
	}
	xrtMutexUnlock(pSess->pLock);
	XSMTP_TRACE("connect ready host=%s secure=%d streamTls=%d", sConnectHost, iSecureMode, pSess->bStreamTls ? 1 : 0);
	return TRUE;
}

static bool xsmtp_sockaddr_to_host(const struct sockaddr* pAddr, char* sHost, size_t iHostCap)
{
	if ( pAddr == NULL || sHost == NULL || iHostCap == 0 ) {
		return FALSE;
	}
	sHost[0] = '\0';
	if ( pAddr->sa_family == AF_INET ) {
		const struct sockaddr_in* pIPv4 = (const struct sockaddr_in*)pAddr;
		return inet_ntop(AF_INET, &pIPv4->sin_addr, sHost, (socklen_t)iHostCap) != NULL;
	}
	if ( pAddr->sa_family == AF_INET6 ) {
		const struct sockaddr_in6* pIPv6 = (const struct sockaddr_in6*)pAddr;
		return inet_ntop(AF_INET6, &pIPv6->sin6_addr, sHost, (socklen_t)iHostCap) != NULL;
	}
	return FALSE;
}

static bool xsmtp_stream_connect(xsmtpsession* pSess, const xsmtpconfig* pCfg, int iSecureMode, xsmtpresult* pRet)
{
	xnetaddr tAddr;
	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	int iPass;

	if ( pSess == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP host is empty");
		return FALSE;
	}
	(void)xrtInit();

	pSess->pLock = xrtMutexCreate();
	pSess->pCond = xrtCondCreate();
	if ( pSess->pLock == NULL || pSess->pCond == NULL ) {
		xsmtp_result_error(pRet, "SMTP session sync init failed");
		return FALSE;
	}

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.iPort = pCfg->iPort;
	if ( xrtNetAddrParse(&tAddr, pCfg->sHost, pCfg->iPort) == XRT_NET_OK ) {
		return xsmtp_stream_connect_once(pSess, pCfg, iSecureMode, pCfg->sHost, pRet);
	}

	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = AF_UNSPEC;
	tHints.ai_socktype = SOCK_STREAM;
	if ( getaddrinfo(pCfg->sHost, NULL, &tHints, &pRes) != 0 || pRes == NULL ) {
		xsmtp_result_error(pRet, "SMTP host resolve failed");
		return FALSE;
	}

	for ( iPass = 0; iPass < 2; ++iPass ) {
		struct addrinfo* pIt;
		int iFamilyWant = (iPass == 0) ? AF_INET : AF_INET6;

		for ( pIt = pRes; pIt != NULL; pIt = pIt->ai_next ) {
			char sHost[INET6_ADDRSTRLEN + 1];

			if ( pIt->ai_addr == NULL || pIt->ai_family != iFamilyWant ) {
				continue;
			}
			if ( !xsmtp_sockaddr_to_host(pIt->ai_addr, sHost, sizeof(sHost)) ) {
				continue;
			}
			if ( xsmtp_stream_connect_once(pSess, pCfg, iSecureMode, sHost, pRet) ) {
				freeaddrinfo(pRes);
				return TRUE;
			}
			if ( pSess->pStream ) {
				xsmtp_stream_close_destroy(&pSess->pStream, pSess->iTimeoutMs);
			}
			if ( pSess->pEngine ) {
				xrtNetEngineDestroy(pSess->pEngine);
				pSess->pEngine = NULL;
			}
			xsmtp_session_reset_state(pSess);
		}
	}

	freeaddrinfo(pRes);
	if ( pRet == NULL || pRet->sError[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP stream connect failed");
	}
	return FALSE;
}

static void xsmtp_session_close(xsmtpsession* pSess)
{
	if ( pSess == NULL ) {
		return;
	}
	if ( pSess->pTls ) {
		xrtNetTlsSessionDestroy(pSess->pTls);
		pSess->pTls = NULL;
	}
	if ( pSess->pStream ) {
		xsmtp_stream_close_destroy(&pSess->pStream, pSess->iTimeoutMs);
	}
	if ( pSess->pEngine ) {
		if ( pSess->bOwnEngine ) {
			xrtNetEngineDestroy(pSess->pEngine);
		}
		pSess->pEngine = NULL;
	}
	xsmtp_buf_unit(&pSess->tWireRecv);
	if ( pSess->pCond ) {
		xrtCondDestroy(pSess->pCond);
		pSess->pCond = NULL;
	}
	if ( pSess->pLock ) {
		xrtMutexDestroy(pSess->pLock);
		pSess->pLock = NULL;
	}
	pSess->bOpen = FALSE;
	pSess->bClosed = FALSE;
	pSess->bStreamTls = FALSE;
	pSess->bTlsReady = FALSE;
	pSess->bRecvOverflow = FALSE;
	pSess->iCloseReason = 0;
	pSess->iLastSysErr = 0;
	pSess->iRecvLen = 0;
	pSess->bOwnEngine = FALSE;
}

static bool xsmtp_stream_send_all(xsmtpsession* pSess, const void* pData, size_t iLen, xsmtpresult* pRet)
{
	xnet_result iWaitRet;
	str sErr;
	const char* sErrText;

	if ( pSess == NULL || pSess->pStream == NULL ) {
		xsmtp_result_error(pRet, "SMTP stream missing");
		return FALSE;
	}
	if ( iLen == 0 ) {
		return TRUE;
	}
	if ( xrtNetStreamSend(pSess->pStream, pData, iLen) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream send failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}
	iWaitRet = xrtNetStreamWaitTimeoutEx(pSess->pStream, XNET_STREAM_WAIT_DRAIN, pSess->iTimeoutMs);
	if ( iWaitRet != XRT_NET_OK ) {
		if ( pSess->iLastSysErr != 0 ) {
			xsmtp_result_errorf(pRet, "SMTP stream drain failed (sys=%d)", pSess->iLastSysErr);
		} else {
			sErr = xrtGetError();
			sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
			xsmtp_result_errorf(pRet, "SMTP stream drain failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		}
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_stream_recv_some(xsmtpsession* pSess, void* pBuf, size_t iCap, size_t* pRead, xsmtpresult* pRet)
{
	uint64 iDeadlineMs;
	size_t iCopy = 0;

	if ( pRead ) {
		*pRead = 0;
	}
	if ( pSess == NULL || pBuf == NULL || iCap == 0 ) {
		xsmtp_result_error(pRet, "SMTP recv buffer invalid");
		return FALSE;
	}

	iDeadlineMs = xsmtp_now_ms() + pSess->iTimeoutMs;
	xrtMutexLock(pSess->pLock);
	for (;;) {
		if ( pSess->tWireRecv.iLen > 0 ) {
			break;
		}
		if ( pSess->bRecvOverflow ) {
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_error(pRet, "SMTP recv buffer overflow");
			return FALSE;
		}
		if ( pSess->iLastSysErr != 0 ) {
			int iSysErr = pSess->iLastSysErr;
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP recv failed (sys=%d)", iSysErr);
			return FALSE;
		}
		if ( pSess->bClosed ) {
			int iCloseReason = pSess->iCloseReason;
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP stream closed (reason=%d)", iCloseReason);
			return FALSE;
		}
		if ( !xsmtp_stream_wait_recv_state(pSess, iDeadlineMs) ) {
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP recv timeout (%u ms)", pSess->iTimeoutMs);
			return FALSE;
		}
	}

	iCopy = pSess->tWireRecv.iLen > iCap ? iCap : pSess->tWireRecv.iLen;
	memcpy(pBuf, pSess->tWireRecv.sData, iCopy);
	if ( iCopy < pSess->tWireRecv.iLen ) {
		memmove(pSess->tWireRecv.sData, pSess->tWireRecv.sData + iCopy, pSess->tWireRecv.iLen - iCopy);
	}
	pSess->tWireRecv.iLen -= iCopy;
	if ( pSess->tWireRecv.sData ) {
		pSess->tWireRecv.sData[pSess->tWireRecv.iLen] = '\0';
	}
	xrtMutexUnlock(pSess->pLock);

	if ( pRead ) {
		*pRead = iCopy;
	}
	return TRUE;
}

static bool xsmtp_tls_flush(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pSess->pTls == NULL ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingCipher(pSess->pTls) > 0 ) {
		if ( xrtNetTlsSessionPeekCipher(pSess->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS cipher peek failed");
			return FALSE;
		}
		if ( !xsmtp_stream_send_all(pSess, aBuf, iRead, pRet) ) {
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pSess->pTls, iRead);
	}
	return TRUE;
}

static bool xsmtp_tls_handshake(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xtlsconfig tTLS;
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pCfg == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS config invalid");
		return FALSE;
	}

	memset(&tTLS, 0, sizeof(tTLS));
	tTLS.sHostName = pCfg->sHost;
	tTLS.bVerifyPeer = pCfg->bVerifyPeer;
	pSess->pTls = xrtNetTlsSessionCreate(&tTLS, FALSE);
	if ( pSess->pTls == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS session create failed");
		return FALSE;
	}

	while ( !xrtNetTlsSessionIsReady(pSess->pTls) ) {
		xnet_result iResult = xrtNetTlsSessionDriveHandshake(pSess->pTls);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_error(pRet, "SMTP TLS handshake failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pSess->pTls) ) {
			break;
		}
		if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead) != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS feed failed");
			return FALSE;
		}
	}

	pSess->bTlsReady = TRUE;
	if ( pRet ) {
		pRet->bUsedTLS = TRUE;
	}
	return TRUE;
}

static bool xsmtp_send_bytes(xsmtpsession* pSess, const void* pData, size_t iLen, xsmtpresult* pRet)
{
	const char* p = (const char*)pData;

	if ( pSess == NULL ) {
		xsmtp_result_error(pRet, "SMTP session missing");
		return FALSE;
	}
	if ( pSess->pTls == NULL ) {
		return xsmtp_stream_send_all(pSess, pData, iLen, pRet);
	}

	while ( iLen > 0 ) {
		size_t iWritten = 0;
		xnet_result iResult = xrtNetTlsSessionWritePlain(pSess->pTls, p, iLen, &iWritten);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_error(pRet, "SMTP TLS write failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( iWritten == 0 && iLen > 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS write stalled");
			return FALSE;
		}
		p += iWritten;
		iLen -= iWritten;
	}
	return TRUE;
}

static bool xsmtp_append_recv(xsmtpsession* pSess, const char* pData, size_t iLen, xsmtpresult* pRet)
{
	if ( pSess == NULL || pData == NULL ) {
		return FALSE;
	}
	if ( pSess->iRecvLen + iLen >= sizeof(pSess->aRecv) ) {
		xsmtp_result_error(pRet, "SMTP recv buffer overflow");
		return FALSE;
	}
	memcpy(pSess->aRecv + pSess->iRecvLen, pData, iLen);
	pSess->iRecvLen += iLen;
	pSess->aRecv[pSess->iRecvLen] = '\0';
	return TRUE;
}

static bool xsmtp_pump_recv(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL ) {
		return FALSE;
	}

	if ( pSess->pTls == NULL ) {
		if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
			return FALSE;
		}
		return xsmtp_append_recv(pSess, aBuf, iRead, pRet);
	}

	while ( xrtNetTlsSessionPendingRecv(pSess->pTls) == 0 ) {
		if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead) != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS feed failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
	}

	while ( xrtNetTlsSessionPendingRecv(pSess->pTls) > 0 ) {
		size_t iPlain = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pSess->pTls, aBuf, sizeof(aBuf), &iPlain);
		if ( iRes == XRT_NET_AGAIN ) {
			break;
		}
		if ( iRes == XRT_NET_CLOSED ) {
			xsmtp_result_error(pRet, "SMTP TLS stream closed");
			return FALSE;
		}
		if ( iRes != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS read plain failed");
			return FALSE;
		}
		if ( iPlain == 0 ) {
			break;
		}
		if ( !xsmtp_append_recv(pSess, aBuf, iPlain, pRet) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_read_line(xsmtpsession* pSess, char* sOut, size_t iOutCap, xsmtpresult* pRet)
{
	size_t i;

	if ( sOut == NULL || iOutCap == 0 ) {
		xsmtp_result_error(pRet, "SMTP output buffer invalid");
		return FALSE;
	}

	for (;;) {
		for ( i = 0; i + 1 < pSess->iRecvLen; i++ ) {
			if ( pSess->aRecv[i] == '\r' && pSess->aRecv[i + 1] == '\n' ) {
				size_t iCopy = i;
				if ( iCopy >= iOutCap ) {
					iCopy = iOutCap - 1;
				}
				memcpy(sOut, pSess->aRecv, iCopy);
				sOut[iCopy] = '\0';
				memmove(pSess->aRecv, pSess->aRecv + i + 2, pSess->iRecvLen - (i + 2));
				pSess->iRecvLen -= (i + 2);
				pSess->aRecv[pSess->iRecvLen] = '\0';
				return TRUE;
			}
		}

		if ( !xsmtp_pump_recv(pSess, pRet) ) {
			return FALSE;
		}
	}
}

static bool xsmtp_read_reply(xsmtpsession* pSess, xsmtpresult* pRet, int* pCode, uint32* pCaps)
{
	char sLine[1024];
	int iReplyCode = 0;
	bool bFirst = TRUE;
	xsmtpbuf tReply;

	memset(&tReply, 0, sizeof(tReply));
	if ( pCaps ) {
		*pCaps = 0;
	}

	for (;;) {
		if ( !xsmtp_read_line(pSess, sLine, sizeof(sLine), pRet) ) {
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( bFirst ) {
			if ( strlen(sLine) < 3 || !isdigit((unsigned char)sLine[0]) || !isdigit((unsigned char)sLine[1]) || !isdigit((unsigned char)sLine[2]) ) {
				xsmtp_result_error(pRet, "SMTP invalid reply line");
				xsmtp_buf_unit(&tReply);
				return FALSE;
			}
			iReplyCode = (sLine[0] - '0') * 100 + (sLine[1] - '0') * 10 + (sLine[2] - '0');
			if ( pRet ) {
				pRet->iServerCode = iReplyCode;
			}
			bFirst = FALSE;
		}

		if ( !xsmtp_buf_append(&tReply, sLine) || !xsmtp_buf_append(&tReply, "\n") ) {
			xsmtp_result_error(pRet, "SMTP reply buffer alloc failed");
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( pCaps && strlen(sLine) > 4 && iReplyCode == 250 ) {
			const char* sCap = sLine + 4;
			if ( xsmtp_starts_with_i(sCap, "STARTTLS") ) {
				*pCaps |= XSMTP_CAP_STARTTLS;
			}
			if ( xsmtp_starts_with_i(sCap, "AUTH ") ) {
				if ( xsmtp_find_i(sCap, "PLAIN") ) {
					*pCaps |= XSMTP_CAP_AUTH_PLAIN;
				}
				if ( xsmtp_find_i(sCap, "LOGIN") ) {
					*pCaps |= XSMTP_CAP_AUTH_LOGIN;
				}
			}
		}

		if ( strlen(sLine) < 4 || sLine[3] != '-' ) {
			break;
		}
	}

	if ( pCode ) {
		*pCode = iReplyCode;
	}
	if ( pRet && tReply.sData ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", tReply.sData);
	}
	xsmtp_buf_unit(&tReply);
	return TRUE;
}

static bool xsmtp_expect_reply(xsmtpsession* pSess, xsmtpresult* pRet, int iExpect, uint32* pCaps)
{
	int iCode = 0;
	if ( !xsmtp_read_reply(pSess, pRet, &iCode, pCaps) ) {
		return FALSE;
	}
	if ( iCode != iExpect ) {
		xsmtp_result_error(pRet, pRet ? pRet->sLastReply : "SMTP unexpected reply");
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_send_line(xsmtpsession* pSess, xsmtpresult* pRet, const char* sLine)
{
	xsmtpbuf tBuf;
	bool bOK;
	memset(&tBuf, 0, sizeof(tBuf));
	bOK = xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(sLine)) && xsmtp_buf_append(&tBuf, "\r\n");
	if ( bOK ) {
		bOK = xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet);
	}
	xsmtp_buf_unit(&tBuf);
	return bOK;
}

static bool xsmtp_send_ehlo(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet, uint32* pCaps)
{
	const char* sHelo = (pCfg && pCfg->sHeloName && pCfg->sHeloName[0]) ? pCfg->sHeloName : "localhost";
	str sLine = xrtFormat("EHLO %s", sHelo);
	bool bOK;
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP EHLO alloc failed");
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 250, pCaps);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_plain(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sUser));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sPass));
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	if ( sBase64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN base64 failed");
		goto cleanup;
	}
	sLine = xrtFormat("AUTH PLAIN %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN alloc failed");
		goto cleanup;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 235, NULL);

cleanup:
	xsmtp_buf_unit(&tBuf);
	if ( sBase64 ) xrtFree(sBase64);
	if ( sLine ) xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_login(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	str sUser64 = NULL;
	str sPass64 = NULL;
	bool bOK = FALSE;

	sUser64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sUser), strlen(xsmtp_str_or_empty(pCfg->sUser)), NULL);
	sPass64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sPass), strlen(xsmtp_str_or_empty(pCfg->sPass)), NULL);
	if ( sUser64 == NULL || sPass64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH LOGIN base64 failed");
		goto cleanup;
	}

	if ( !xsmtp_send_line(pSess, pRet, "AUTH LOGIN") || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, (const char*)sUser64) || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, (const char*)sPass64) || !xsmtp_expect_reply(pSess, pRet, 235, NULL) ) {
		goto cleanup;
	}

	bOK = TRUE;

cleanup:
	if ( sUser64 ) xrtFree(sUser64);
	if ( sPass64 ) xrtFree(sPass64);
	return bOK;
}

static bool xsmtp_send_auth(xsmtpsession* pSess, const xsmtpconfig* pCfg, uint32 iCaps, xsmtpresult* pRet)
{
	int iAuthMode;
	if ( pRet ) {
		pRet->iCapabilities = iCaps;
	}
	iAuthMode = xsmtp_auth_mode_auto(pCfg, iCaps);
	if ( pRet ) {
		pRet->iAuthMode = iAuthMode;
	}

	if ( iAuthMode == XSMTP_AUTH_NONE ) {
		return TRUE;
	}
	if ( iAuthMode == XSMTP_AUTH_PLAIN ) {
		return xsmtp_send_auth_plain(pSess, pCfg, pRet);
	}
	if ( iAuthMode == XSMTP_AUTH_LOGIN ) {
		return xsmtp_send_auth_login(pSess, pCfg, pRet);
	}

	xsmtp_result_error(pRet, "SMTP auth mode unsupported");
	return FALSE;
}

static bool xsmtp_send_mail_from(xsmtpsession* pSess, const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
	const char* sEmail = NULL;
	str sLine = NULL;
	bool bOK;

	if ( pMsg && pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0] ) {
		sEmail = pMsg->tFrom.sEmail;
	} else if ( pCfg && pCfg->tFrom.sEmail && pCfg->tFrom.sEmail[0] ) {
		sEmail = pCfg->tFrom.sEmail;
	}
	if ( sEmail == NULL || sEmail[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP from email is empty");
		return FALSE;
	}
	sLine = xrtFormat("MAIL FROM:<%s>", sEmail);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP MAIL FROM alloc failed");
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 250, NULL);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_rcpt_list(xsmtpsession* pSess, const xsmtpaddr* arrList, size_t iCount, xsmtpresult* pRet)
{
	size_t i;
	for ( i = 0; i < iCount; i++ ) {
		str sLine;
		if ( arrList[i].sEmail == NULL || arrList[i].sEmail[0] == '\0' ) {
			continue;
		}
		sLine = xrtFormat("RCPT TO:<%s>", arrList[i].sEmail);
		if ( sLine == NULL ) {
			xsmtp_result_error(pRet, "SMTP RCPT TO alloc failed");
			return FALSE;
		}
		if ( !xsmtp_send_line(pSess, pRet, (const char*)sLine) || !xsmtp_expect_reply(pSess, pRet, 250, NULL) ) {
			xrtFree(sLine);
			return FALSE;
		}
		xrtFree(sLine);
	}
	return TRUE;
}

static bool xsmtp_send_data_body(xsmtpsession* pSess, const char* sData, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_reserve(&tBuf, strlen(xsmtp_str_or_empty(sData)) + 16) ) {
		xsmtp_result_error(pRet, "SMTP DATA buffer alloc failed");
		return FALSE;
	}
	for ( p = xsmtp_str_or_empty(sData); *p; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen;
		if ( sLineEnd ) {
			iLineLen = (size_t)(sLineEnd - p);
		} else {
			iLineLen = strlen(p);
		}
		if ( iLineLen > 0 && p[0] == '.' ) {
			xsmtp_buf_append(&tBuf, ".");
		}
		xsmtp_buf_append_n(&tBuf, p, iLineLen);
		xsmtp_buf_append(&tBuf, "\r\n");
		if ( !sLineEnd ) {
			break;
		}
		p = sLineEnd + 2;
	}
	xsmtp_buf_append(&tBuf, ".\r\n");

	if ( !xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet) ) {
		xsmtp_buf_unit(&tBuf);
		return FALSE;
	}
	xsmtp_buf_unit(&tBuf);
	return xsmtp_expect_reply(pSess, pRet, 250, NULL);
}

static bool xrtSmtpSendMail(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
	xsmtpsession tSess;
	uint32 iCaps = 0;
	int iSecureMode;
	str sData = NULL;
	size_t iRecipientCount = 0;
	bool bOK = FALSE;

	if ( pRet ) {
		xrtSmtpResultInit(pRet);
	}
	memset(&tSess, 0, sizeof(tSess));

	if ( pCfg == NULL || pMsg == NULL ) {
		xsmtp_result_error(pRet, "SMTP config or message missing");
		goto cleanup;
	}
	if ( !xsmtp_count_recipients(pMsg, &iRecipientCount) ) {
		xsmtp_result_error(pRet, "SMTP recipients are empty");
		goto cleanup;
	}

	iSecureMode = xsmtp_secure_mode_auto(pCfg);
	XSMTP_TRACE("connect host=%s port=%u secure=%d", pCfg->sHost ? pCfg->sHost : "", (unsigned)pCfg->iPort, iSecureMode);
	if ( !xsmtp_stream_connect(&tSess, pCfg, iSecureMode, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("connect ok");

	if ( iSecureMode == XSMTP_SECURE_SSL ) {
		XSMTP_TRACE("implicit tls ready via xrt stream");
		if ( pRet ) {
			pRet->bUsedTLS = TRUE;
		}
	}

	XSMTP_TRACE("wait banner");
	if ( !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
		goto cleanup;
	}
	XSMTP_TRACE("banner ok");

	XSMTP_TRACE("ehlo begin");
	if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
		goto cleanup;
	}
	XSMTP_TRACE("ehlo ok caps=0x%08x", (unsigned)iCaps);

	if ( iSecureMode == XSMTP_SECURE_STARTTLS ) {
		XSMTP_TRACE("starttls begin");
		if ( !(iCaps & XSMTP_CAP_STARTTLS) ) {
			xsmtp_result_error(pRet, "SMTP server does not support STARTTLS");
			goto cleanup;
		}
		if ( !xsmtp_send_line(&tSess, pRet, "STARTTLS") || !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
			goto cleanup;
		}
		if ( !xsmtp_tls_handshake(&tSess, pCfg, pRet) ) {
			goto cleanup;
		}
		XSMTP_TRACE("starttls handshake ok");
		if ( pRet ) {
			pRet->bUsedStartTLS = TRUE;
		}
		iCaps = 0;
		if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
			goto cleanup;
		}
		XSMTP_TRACE("post-starttls ehlo ok caps=0x%08x", (unsigned)iCaps);
	}

	XSMTP_TRACE("auth begin");
	if ( !xsmtp_send_auth(&tSess, pCfg, iCaps, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("auth ok");
	if ( !xsmtp_send_mail_from(&tSess, pCfg, pMsg, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("mail from ok");
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrTo, pMsg->iToCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrCc, pMsg->iCcCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrBcc, pMsg->iBccCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(&tSess, pRet, "DATA") || !xsmtp_expect_reply(&tSess, pRet, 354, NULL) ) {
		goto cleanup;
	}
	XSMTP_TRACE("data begin");

	sData = xsmtp_build_message_data(pCfg, pMsg);
	if ( sData == NULL ) {
		xsmtp_result_error(pRet, "SMTP build message failed");
		goto cleanup;
	}
	if ( !xsmtp_send_data_body(&tSess, (const char*)sData, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("data ok");

	xsmtp_send_line(&tSess, pRet, "QUIT");
	xsmtp_expect_reply(&tSess, pRet, 221, NULL);
	XSMTP_TRACE("quit done");

	bOK = TRUE;
	if ( pRet ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sData ) {
		xrtFree(sData);
	}
	xsmtp_session_close(&tSess);
	return bOK;
}

static void xsmtp_async_fail(xsmtpasyncop* pOp, const char* sError);
static void xsmtp_async_arm_timeout(xsmtpasyncop* pOp, const char* sReason);
static void xsmtp_async_advance(xsmtpasyncop* pOp);
static bool xsmtp_async_run_post_ehlo(xsmtpasyncop* pOp, bool bAllowStartTLS);

static bool xsmtp_async_send_stream_raw(xsmtpasyncop* pOp, const void* pData, size_t iLen)
{
	if ( pOp == NULL || pOp->pStream == NULL ) {
		return FALSE;
	}
	return xrtNetStreamSend(pOp->pStream, pData, iLen) == XRT_NET_OK;
}

static bool xsmtp_async_tls_flush_manual(xsmtpasyncop* pOp)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingCipher(pOp->pSess->pTls) > 0 ) {
		if ( xrtNetTlsSessionPeekCipher(pOp->pSess->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xsmtp_async_fail(pOp, "SMTP TLS cipher peek failed");
			return FALSE;
		}
		if ( !xsmtp_async_send_stream_raw(pOp, aBuf, iRead) ) {
			xsmtp_async_fail(pOp, "SMTP TLS cipher send failed");
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pOp->pSess->pTls, iRead);
	}
	return TRUE;
}

static bool xsmtp_async_tls_drain_plain(xsmtpasyncop* pOp)
{
	char aBuf[4096];

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) > 0 ) {
		size_t iPlain = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pOp->pSess->pTls, aBuf, sizeof(aBuf), &iPlain);
		if ( iRes == XRT_NET_AGAIN ) {
			break;
		}
		if ( iRes == XRT_NET_CLOSED ) {
			xsmtp_async_fail(pOp, "SMTP TLS stream closed");
			return FALSE;
		}
		if ( iRes != XRT_NET_OK ) {
			xsmtp_async_fail(pOp, "SMTP TLS read plain failed");
			return FALSE;
		}
		if ( iPlain == 0 ) {
			break;
		}
		if ( !xsmtp_append_recv(pOp->pSess, aBuf, iPlain, pOp->pRet) ) {
			xsmtp_async_fail(pOp, pOp->pRet ? pOp->pRet->sError : "SMTP recv append failed");
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_async_tls_drive_handshake(xsmtpasyncop* pOp)
{
	xnet_result iResult;
	int iStep;

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return FALSE;
	}

	for ( iStep = 0; iStep < 32 && !xrtNetTlsSessionIsReady(pOp->pSess->pTls); ++iStep ) {
		uint32 iPendingBefore = (uint32)xrtNetTlsSessionPendingRecv(pOp->pSess->pTls);
		uint32 iCipherBefore = (uint32)xrtNetTlsSessionPendingCipher(pOp->pSess->pTls);

		iResult = xrtNetTlsSessionDriveHandshake(pOp->pSess->pTls);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_async_fail(pOp, "SMTP TLS handshake failed");
			return FALSE;
		}
		if ( !xsmtp_async_tls_flush_manual(pOp) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pOp->pSess->pTls) ) {
			break;
		}
		if ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) == 0 ) {
			return TRUE;
		}
		if ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) == iPendingBefore
			&& xrtNetTlsSessionPendingCipher(pOp->pSess->pTls) == iCipherBefore
			&& iResult == XRT_NET_AGAIN ) {
			return TRUE;
		}
	}

	if ( !xrtNetTlsSessionIsReady(pOp->pSess->pTls) ) {
		return TRUE;
	}

	pOp->pSess->bTlsReady = TRUE;
	if ( pOp->pRet != NULL ) {
		pOp->pRet->bUsedTLS = TRUE;
		pOp->pRet->bUsedStartTLS = TRUE;
	}
	return TRUE;
}

static bool xsmtp_async_starttls_begin(xsmtpasyncop* pOp)
{
	xtlsconfig tTlsCfg;

	if ( pOp == NULL || pOp->pSess == NULL ) {
		return FALSE;
	}
	if ( pOp->pSess->pTls != NULL ) {
		return TRUE;
	}

	memset(&tTlsCfg, 0, sizeof(tTlsCfg));
	tTlsCfg.sHostName = pOp->pCtx->tCfg.sHost;
	tTlsCfg.bVerifyPeer = pOp->pCtx->tCfg.bVerifyPeer;
	pOp->pSess->pTls = xrtNetTlsSessionCreate(&tTlsCfg, FALSE);
	if ( pOp->pSess->pTls == NULL ) {
		xsmtp_async_fail(pOp, "SMTP TLS session create failed");
		return FALSE;
	}
	pOp->pSess->bStreamTls = FALSE;
	pOp->pSess->bTlsReady = FALSE;
	if ( !xsmtp_async_tls_drive_handshake(pOp) ) {
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_async_send_plain(xsmtpasyncop* pOp, const void* pData, size_t iLen)
{
	const char* p = (const char*)pData;

	if ( pOp == NULL || pOp->pSess == NULL ) {
		return FALSE;
	}
	if ( iLen == 0 ) {
		return TRUE;
	}
	if ( pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return xsmtp_async_send_stream_raw(pOp, pData, iLen);
	}
	if ( !pOp->pSess->bTlsReady ) {
		xsmtp_async_fail(pOp, "SMTP TLS not ready");
		return FALSE;
	}

	while ( iLen > 0 ) {
		size_t iWritten = 0;
		xnet_result iResult = xrtNetTlsSessionWritePlain(pOp->pSess->pTls, p, iLen, &iWritten);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_async_fail(pOp, "SMTP TLS write failed");
			return FALSE;
		}
		if ( !xsmtp_async_tls_flush_manual(pOp) ) {
			return FALSE;
		}
		if ( iWritten == 0 ) {
			xsmtp_async_fail(pOp, "SMTP TLS write stalled");
			return FALSE;
		}
		p += iWritten;
		iLen -= iWritten;
	}
	return TRUE;
}

static void xsmtp_async_cleanup(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	if ( pOp->pStream != NULL && pOp->pSess == NULL ) {
		xrtNetStreamDestroy(pOp->pStream);
		pOp->pStream = NULL;
	}
	if ( pOp->pPromise != NULL ) {
		xPromiseDestroy(pOp->pPromise);
		pOp->pPromise = NULL;
	}
	if ( pOp->pSess != NULL ) {
		xsmtp_session_close(pOp->pSess);
		xrtFree(pOp->pSess);
		pOp->pSess = NULL;
		pOp->pStream = NULL;
		pOp->pEngine = NULL;
	}
	xsmtp_buf_unit(&pOp->tReply);
	xsmtp_buf_unit(&pOp->tScratch);
	if ( pOp->pCtx != NULL ) {
		xsmtp_task_ctx_free(pOp->pCtx);
		pOp->pCtx = NULL;
	}
	xrtFree(pOp);
}

static void xsmtp_async_resolve(xsmtpasyncop* pOp)
{
	xsmtpresult* pRet;
	xpromise* pPromise;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->bDone = TRUE;
	pRet = pOp->pRet;
	pPromise = pOp->pPromise;
	pOp->pRet = NULL;
	pOp->pPromise = NULL;
	if ( pPromise != NULL ) {
		(void)xPromiseResolve(pPromise, pRet);
	}
	xsmtp_async_cleanup(pOp);
}

static void xsmtp_async_fail(xsmtpasyncop* pOp, const char* sError)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	xsmtp_result_error(pOp->pRet, sError);
	xsmtp_async_resolve(pOp);
}

static bool xsmtp_async_send_line(xsmtpasyncop* pOp, const char* sLine)
{
	xsmtpbuf tBuf;
	xnet_result iRet;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(sLine)) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_async_fail(pOp, "SMTP async line alloc failed");
		return FALSE;
	}
	iRet = xsmtp_async_send_plain(pOp, tBuf.sData, tBuf.iLen) ? XRT_NET_OK : XRT_NET_ERROR;
	xsmtp_buf_unit(&tBuf);
	if ( iRet != XRT_NET_OK ) {
		xsmtp_async_fail(pOp, "SMTP async send failed");
		return FALSE;
	}
	return TRUE;
}

static void xsmtp_async_reply_reset(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	pOp->iReplyCode = 0;
	pOp->iReplyCaps = 0;
	pOp->bReplyHasFirst = FALSE;
	pOp->tReply.iLen = 0;
	if ( pOp->tReply.sData != NULL ) {
		pOp->tReply.sData[0] = '\0';
	}
}

static void xsmtp_async_set_state(xsmtpasyncop* pOp, xsmtpasyncstate iState, const char* sReason)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->iState = iState;
	xsmtp_async_reply_reset(pOp);
	xsmtp_async_arm_timeout(pOp, sReason);
}

static const xsmtpaddr* xsmtp_async_current_rcpt(xsmtpasyncop* pOp)
{
	if ( pOp == NULL || pOp->pCtx == NULL ) {
		return NULL;
	}
	if ( pOp->iRcptListKind == 0 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iToCount ) {
			return &pOp->pCtx->tMsg.arrTo[pOp->iRcptIndex];
		}
	}
	else if ( pOp->iRcptListKind == 1 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iCcCount ) {
			return &pOp->pCtx->tMsg.arrCc[pOp->iRcptIndex];
		}
	}
	else if ( pOp->iRcptListKind == 2 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iBccCount ) {
			return &pOp->pCtx->tMsg.arrBcc[pOp->iRcptIndex];
		}
	}
	return NULL;
}

static void xsmtp_async_next_rcpt_cursor(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	pOp->iRcptIndex++;
	for (;;) {
		size_t iCount = 0;
		if ( pOp->iRcptListKind == 0 ) iCount = pOp->pCtx->tMsg.iToCount;
		else if ( pOp->iRcptListKind == 1 ) iCount = pOp->pCtx->tMsg.iCcCount;
		else if ( pOp->iRcptListKind == 2 ) iCount = pOp->pCtx->tMsg.iBccCount;
		if ( pOp->iRcptIndex < iCount ) {
			return;
		}
		pOp->iRcptListKind++;
		pOp->iRcptIndex = 0;
		if ( pOp->iRcptListKind > 2 ) {
			return;
		}
	}
}

static bool xsmtp_async_send_auth_plain_line(xsmtpasyncop* pOp)
{
	xsmtpbuf tBuf;
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass));
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	if ( sBase64 == NULL ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_async_fail(pOp, "SMTP AUTH PLAIN base64 failed");
		return FALSE;
	}
	sLine = xrtFormat("AUTH PLAIN %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_buf_unit(&tBuf);
		xrtFree(sBase64);
		xsmtp_async_fail(pOp, "SMTP AUTH PLAIN alloc failed");
		return FALSE;
	}
	bOK = xsmtp_async_send_line(pOp, (const char*)sLine);
	xsmtp_buf_unit(&tBuf);
	xrtFree(sBase64);
	xrtFree(sLine);
	return bOK;
}

static void xsmtp_async_send_next_rcpt(xsmtpasyncop* pOp)
{
	const xsmtpaddr* pAddr;
	str sLine;

	pAddr = xsmtp_async_current_rcpt(pOp);
	if ( pAddr == NULL ) {
		if ( xsmtp_async_send_line(pOp, "DATA") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_DATA, "SMTP DATA timeout");
		}
		return;
	}
	if ( pAddr->sEmail == NULL || pAddr->sEmail[0] == '\0' ) {
		xsmtp_async_next_rcpt_cursor(pOp);
		xsmtp_async_send_next_rcpt(pOp);
		return;
	}
	sLine = xrtFormat("RCPT TO:<%s>", pAddr->sEmail);
	if ( sLine == NULL ) {
		xsmtp_async_fail(pOp, "SMTP RCPT TO alloc failed");
		return;
	}
	if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_RCPT, "SMTP RCPT timeout");
	}
	xrtFree(sLine);
}

static bool xsmtp_async_run_post_ehlo(xsmtpasyncop* pOp, bool bAllowStartTLS)
{
	str sLine;
	int iSecureMode;

	if ( pOp == NULL || pOp->pCtx == NULL || pOp->pRet == NULL ) {
		return FALSE;
	}

	pOp->iCapabilities = pOp->iReplyCaps;
	pOp->pRet->iCapabilities = pOp->iCapabilities;
	iSecureMode = xsmtp_secure_mode_auto(&pOp->pCtx->tCfg);
	if ( bAllowStartTLS && iSecureMode == XSMTP_SECURE_STARTTLS ) {
		if ( !(pOp->iCapabilities & XSMTP_CAP_STARTTLS) ) {
			xsmtp_async_fail(pOp, "SMTP server does not support STARTTLS");
			return FALSE;
		}
		if ( xsmtp_async_send_line(pOp, "STARTTLS") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_STARTTLS, "SMTP STARTTLS timeout");
			return TRUE;
		}
		return FALSE;
	}

	pOp->iAuthMode = xsmtp_auth_mode_auto(&pOp->pCtx->tCfg, pOp->iCapabilities);
	pOp->pRet->iAuthMode = pOp->iAuthMode;
	if ( pOp->iAuthMode == XSMTP_AUTH_NONE ) {
		sLine = xrtFormat("MAIL FROM:<%s>",
			pOp->pCtx->tMsg.tFrom.sEmail && pOp->pCtx->tMsg.tFrom.sEmail[0]
				? pOp->pCtx->tMsg.tFrom.sEmail
				: pOp->pCtx->tCfg.tFrom.sEmail);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP MAIL FROM alloc failed");
			return FALSE;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_MAIL_FROM, "SMTP MAIL FROM timeout");
		}
		xrtFree(sLine);
		return TRUE;
	}
	if ( pOp->iAuthMode == XSMTP_AUTH_PLAIN ) {
		if ( xsmtp_async_send_auth_plain_line(pOp) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH, "SMTP AUTH timeout");
		}
		return TRUE;
	}
	if ( pOp->iAuthMode == XSMTP_AUTH_LOGIN ) {
		if ( xsmtp_async_send_line(pOp, "AUTH LOGIN") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH_LOGIN_USER, "SMTP AUTH LOGIN timeout");
		}
		return TRUE;
	}

	xsmtp_async_fail(pOp, "SMTP auth mode unsupported");
	return FALSE;
}

static void xsmtp_async_advance(xsmtpasyncop* pOp)
{
	const char* sHelo;
	str sLine;
	str sData;
	char* sBody;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}

	switch ( pOp->iState ) {
	case XSMTP_AST_WAIT_BANNER:
		sHelo = (pOp->pCtx->tCfg.sHeloName && pOp->pCtx->tCfg.sHeloName[0]) ? pOp->pCtx->tCfg.sHeloName : "localhost";
		sLine = xrtFormat("EHLO %s", sHelo);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP EHLO alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_EHLO, "SMTP EHLO timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_EHLO:
		(void)xsmtp_async_run_post_ehlo(pOp, TRUE);
		return;
	case XSMTP_AST_WAIT_STARTTLS:
		if ( !xsmtp_async_starttls_begin(pOp) ) {
			return;
		}
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_STARTTLS_TLS, "SMTP TLS handshake timeout");
		if ( pOp->pSess->bTlsReady ) {
			xsmtp_async_advance(pOp);
		}
		return;
	case XSMTP_AST_WAIT_STARTTLS_TLS:
		sHelo = (pOp->pCtx->tCfg.sHeloName && pOp->pCtx->tCfg.sHeloName[0]) ? pOp->pCtx->tCfg.sHeloName : "localhost";
		sLine = xrtFormat("EHLO %s", sHelo);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP EHLO alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_EHLO_TLS, "SMTP EHLO timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_EHLO_TLS:
		(void)xsmtp_async_run_post_ehlo(pOp, FALSE);
		return;
	case XSMTP_AST_WAIT_AUTH:
		sLine = xrtFormat("MAIL FROM:<%s>",
			pOp->pCtx->tMsg.tFrom.sEmail && pOp->pCtx->tMsg.tFrom.sEmail[0]
				? pOp->pCtx->tMsg.tFrom.sEmail
				: pOp->pCtx->tCfg.tFrom.sEmail);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP MAIL FROM alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_MAIL_FROM, "SMTP MAIL FROM timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_AUTH_LOGIN_USER:
		sLine = xrtBase64Encode((ptr)xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser), strlen(xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser)), NULL);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP AUTH LOGIN base64 user failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH_LOGIN_PASS, "SMTP AUTH LOGIN user timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_AUTH_LOGIN_PASS:
		sLine = xrtBase64Encode((ptr)xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass), strlen(xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass)), NULL);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP AUTH LOGIN base64 pass failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH, "SMTP AUTH LOGIN pass timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_MAIL_FROM:
		pOp->iRcptListKind = 0;
		pOp->iRcptIndex = 0;
		xsmtp_async_send_next_rcpt(pOp);
		return;
	case XSMTP_AST_WAIT_RCPT:
		xsmtp_async_next_rcpt_cursor(pOp);
		xsmtp_async_send_next_rcpt(pOp);
		return;
	case XSMTP_AST_WAIT_DATA:
		sData = xsmtp_build_message_data(&pOp->pCtx->tCfg, &pOp->pCtx->tMsg);
		if ( sData == NULL ) {
			xsmtp_async_fail(pOp, "SMTP build message failed");
			return;
		}
		sBody = xsmtp_build_data_body_bytes((const char*)sData);
		xrtFree(sData);
		if ( sBody == NULL ) {
			xsmtp_async_fail(pOp, "SMTP DATA body alloc failed");
			return;
		}
		if ( !xsmtp_async_send_plain(pOp, sBody, strlen(sBody)) ) {
			xrtFree(sBody);
			xsmtp_async_fail(pOp, "SMTP DATA send failed");
			return;
		}
		xrtFree(sBody);
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_BODY, "SMTP DATA body timeout");
		return;
	case XSMTP_AST_WAIT_BODY:
		if ( xsmtp_async_send_line(pOp, "QUIT") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_QUIT, "SMTP QUIT timeout");
		}
		return;
	case XSMTP_AST_WAIT_QUIT:
		pOp->pRet->bSuccess = TRUE;
		xsmtp_async_resolve(pOp);
		return;
	default:
		xsmtp_async_fail(pOp, "SMTP async invalid state");
		return;
	}
}

static void xsmtp_async_on_reply_complete(xsmtpasyncop* pOp)
{
	int iExpect = 0;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	if ( pOp->pRet != NULL && pOp->tReply.sData != NULL ) {
		snprintf(pOp->pRet->sLastReply, sizeof(pOp->pRet->sLastReply), "%s", pOp->tReply.sData);
	}
	switch ( pOp->iState ) {
	case XSMTP_AST_WAIT_BANNER: iExpect = 220; break;
	case XSMTP_AST_WAIT_EHLO: iExpect = 250; break;
	case XSMTP_AST_WAIT_STARTTLS: iExpect = 220; break;
	case XSMTP_AST_WAIT_EHLO_TLS: iExpect = 250; break;
	case XSMTP_AST_WAIT_AUTH: iExpect = 235; break;
	case XSMTP_AST_WAIT_AUTH_LOGIN_USER: iExpect = 334; break;
	case XSMTP_AST_WAIT_AUTH_LOGIN_PASS: iExpect = 334; break;
	case XSMTP_AST_WAIT_MAIL_FROM: iExpect = 250; break;
	case XSMTP_AST_WAIT_RCPT: iExpect = 250; break;
	case XSMTP_AST_WAIT_DATA: iExpect = 354; break;
	case XSMTP_AST_WAIT_BODY: iExpect = 250; break;
	case XSMTP_AST_WAIT_QUIT: iExpect = 221; break;
	default: iExpect = 0; break;
	}
	if ( iExpect > 0 && pOp->iReplyCode != iExpect ) {
		xsmtp_async_fail(pOp, pOp->pRet ? pOp->pRet->sLastReply : "SMTP unexpected reply");
		return;
	}
	if ( pOp->pRet != NULL ) {
		pOp->pRet->iServerCode = pOp->iReplyCode;
	}
	xsmtp_async_advance(pOp);
}

static void xsmtp_async_arm_timeout(xsmtpasyncop* pOp, const char* sReason)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->iWaitToken++;
	pOp->tScratch.iLen = 0;
	if ( pOp->tScratch.sData != NULL ) {
		pOp->tScratch.sData[0] = '\0';
	}
	(void)xsmtp_buf_append(&pOp->tScratch, sReason ? sReason : "SMTP async timeout");
}

static void xsmtp_async_process_recv(xsmtpasyncop* pOp)
{
	char sLine[1024];
	xsmtpsession* pSess;

	if ( pOp == NULL || pOp->bDone || pOp->pCtx == NULL ) {
		return;
	}
	pSess = pOp->pSess;
	while ( xsmtp_try_read_line_from_buffer(pSess, sLine, sizeof(sLine)) ) {
		if ( !pOp->bReplyHasFirst ) {
			if ( strlen(sLine) < 3 || !isdigit((unsigned char)sLine[0]) || !isdigit((unsigned char)sLine[1]) || !isdigit((unsigned char)sLine[2]) ) {
				xsmtp_async_fail(pOp, "SMTP invalid reply line");
				return;
			}
			pOp->iReplyCode = (sLine[0] - '0') * 100 + (sLine[1] - '0') * 10 + (sLine[2] - '0');
			pOp->bReplyHasFirst = TRUE;
		}
		if ( !xsmtp_buf_append(&pOp->tReply, sLine) || !xsmtp_buf_append(&pOp->tReply, "\n") ) {
			xsmtp_async_fail(pOp, "SMTP reply buffer alloc failed");
			return;
		}
		if ( strlen(sLine) > 4 && pOp->iReplyCode == 250 ) {
			const char* sCap = sLine + 4;
			if ( xsmtp_starts_with_i(sCap, "STARTTLS") ) {
				pOp->iReplyCaps |= XSMTP_CAP_STARTTLS;
			}
			if ( xsmtp_starts_with_i(sCap, "AUTH ") ) {
				if ( xsmtp_find_i(sCap, "PLAIN") ) pOp->iReplyCaps |= XSMTP_CAP_AUTH_PLAIN;
				if ( xsmtp_find_i(sCap, "LOGIN") ) pOp->iReplyCaps |= XSMTP_CAP_AUTH_LOGIN;
			}
		}
		if ( strlen(sLine) < 4 || sLine[3] != '-' ) {
			xsmtp_async_on_reply_complete(pOp);
			if ( pOp->bDone ) {
				return;
			}
			xsmtp_async_reply_reset(pOp);
		}
	}
}

static void xsmtp_async_stream_on_open(ptr pOwner, xnetstream* pStream)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	if ( pOp->pRet != NULL && xsmtp_secure_mode_auto(&pOp->pCtx->tCfg) == XSMTP_SECURE_SSL ) {
		pOp->pRet->bUsedTLS = TRUE;
	}
	xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_BANNER, "SMTP banner timeout");
}

static void xsmtp_async_stream_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	char aBuf[4096];
	size_t iLeft;
	(void)pStream;

	if ( pOp == NULL || pOp->bDone || pChain == NULL ) {
		return;
	}
	iLeft = xrtNetChainBytes(pChain);
	while ( iLeft > 0 ) {
		size_t iChunk = iLeft > sizeof(aBuf) ? sizeof(aBuf) : iLeft;
		size_t iRead = xrtNetChainPeek(pChain, aBuf, iChunk);
		if ( iRead == 0 ) {
			break;
		}
		if ( pOp->pSess != NULL && pOp->pSess->pTls != NULL && !pOp->pSess->bStreamTls ) {
			if ( xrtNetTlsSessionFeedCipher(pOp->pSess->pTls, aBuf, iRead) != XRT_NET_OK ) {
				xsmtp_async_fail(pOp, "SMTP TLS feed failed");
				return;
			}
			if ( !pOp->pSess->bTlsReady ) {
				if ( !xsmtp_async_tls_drive_handshake(pOp) ) {
					return;
				}
				if ( pOp->bDone ) {
					return;
				}
				if ( pOp->pSess->bTlsReady && pOp->iState == XSMTP_AST_WAIT_STARTTLS_TLS ) {
					xsmtp_async_advance(pOp);
				}
			}
			if ( pOp->pSess->bTlsReady && !xsmtp_async_tls_drain_plain(pOp) ) {
				return;
			}
		} else if ( !xsmtp_append_recv(pOp->pSess, aBuf, iRead, pOp->pRet) ) {
			xsmtp_async_fail(pOp, pOp->pRet->sError);
			return;
		}
		xrtNetChainConsume(pChain, iRead);
		iLeft -= iRead;
	}
	xsmtp_async_process_recv(pOp);
}

static void xsmtp_async_stream_on_close(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	XSMTP_TRACE("async stream: close reason=%d", (int)iReason);
	if ( pOp->pRet != NULL && !pOp->pRet->bSuccess ) {
		xsmtp_result_errorf(pOp->pRet, "SMTP stream closed (%d)", (int)iReason);
	}
	xsmtp_async_resolve(pOp);
}

static void xsmtp_async_stream_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	XSMTP_TRACE("async stream: error sys=%d", iSysErr);
	xsmtp_result_errorf(pOp->pRet, "SMTP stream error (%d)", iSysErr);
	xsmtp_async_resolve(pOp);
}

static const xnetstreamevents* xsmtp_async_stream_events(void)
{
	static const xnetstreamevents tEvents = {
		xsmtp_async_stream_on_open,
		xsmtp_async_stream_on_recv,
		NULL,
		xsmtp_async_stream_on_close,
		xsmtp_async_stream_on_error,
		NULL,
		NULL
	};
	return &tEvents;
}

static xfuture* xsmtp_send_mail_future_thread(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts);

static xfuture* xsmtp_send_mail_future_native(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtpasyncop* pOp;
	xsmtptaskctx* pCtx;
	xfuture* pFuture;
	xpromise* pPromise;
	xnetengine* pEngine;
	bool bOwnEngine = FALSE;
	xnetconnectconfig tConnCfg;
	xnetengineconfig tEngineCfg;

	(void)xrtInit();

	pEngine = (pOpts != NULL) ? pOpts->pEngine : NULL;
	if ( pEngine == NULL ) {
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( pEngine == NULL ) {
			XSMTP_TRACE("async native: engine create failed, fallback thread");
			return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
		}
		if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
			XSMTP_TRACE("async native: engine start failed, fallback thread");
			xrtNetEngineDestroy(pEngine);
			return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
		}
		bOwnEngine = TRUE;
	}

	pCtx = xsmtp_task_ctx_create(pCfg, pMsg, pOpts);
	if ( pCtx == NULL ) {
		XSMTP_TRACE("async native: task ctx create failed");
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		return NULL;
	}

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async native: future create failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		return NULL;
	}
	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		XSMTP_TRACE("async native: promise create failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		xFutureRelease(pFuture);
		return NULL;
	}

	pOp = (xsmtpasyncop*)xrtMalloc(sizeof(*pOp));
	if ( pOp == NULL ) {
		XSMTP_TRACE("async native: op alloc failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}
	memset(pOp, 0, sizeof(*pOp));
	pOp->pEngine = pEngine;
	pOp->pPromise = pPromise;
	pOp->pCtx = pCtx;
	pOp->pSess = (xsmtpsession*)xrtCalloc(1, sizeof(xsmtpsession));
	if ( pOp->pSess == NULL ) {
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return NULL;
	}
	pOp->pSess->pEngine = pEngine;
	pOp->pSess->bOwnEngine = bOwnEngine;
	pOp->pSess->iTimeoutMs = pCtx->tCfg.iTimeoutMs;
	pOp->pRet = (xsmtpresult*)xrtMalloc(sizeof(xsmtpresult));
	if ( pOp->pRet == NULL ) {
		XSMTP_TRACE("async native: result alloc failed");
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return NULL;
	}
	xrtSmtpResultInit(pOp->pRet);
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pCtx->tCfg.sHost;
	tConnCfg.iPort = pCtx->tCfg.iPort;
	tConnCfg.iConnectTimeoutMs = pCtx->tCfg.iTimeoutMs;
	if ( xsmtp_secure_mode_auto(&pCtx->tCfg) == XSMTP_SECURE_SSL ) {
		memset(&pOp->tTlsCfg, 0, sizeof(pOp->tTlsCfg));
		pOp->tTlsCfg.sHostName = pCtx->tCfg.sHost;
		pOp->tTlsCfg.bVerifyPeer = pCtx->tCfg.bVerifyPeer;
		tConnCfg.pTlsConfig = &pOp->tTlsCfg;
	}
	pOp->pStream = xrtNetStreamCreate(pEngine, xsmtp_async_stream_events(), pOp);
	if ( pOp->pStream == NULL ) {
		XSMTP_TRACE("async native: stream create failed, fallback thread");
		pOp->bDone = TRUE;
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
	}
	pOp->pSess->pStream = pOp->pStream;
	if ( xrtNetStreamConnect(pOp->pStream, &tConnCfg) != XRT_NET_OK ) {
		str sErr = xrtGetError();
		XSMTP_TRACE("async native: connect submit failed: %s, fallback thread", sErr ? (const char*)sErr : "");
		pOp->bDone = TRUE;
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	XSMTP_TRACE("async native: connect submitted");
	return pFuture;
}

static int32 xsmtp_send_mail_task(ptr pArg, xfuture_result* pOut)
{
	xsmtptaskctx* pCtx = (xsmtptaskctx*)pArg;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( pCtx == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"SMTP async task context missing";
		return pOut->iStatus;
	}

	pRet = (xsmtpresult*)xrtMalloc(sizeof(*pRet));
	if ( pRet == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"SMTP async result alloc failed";
		xsmtp_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	xrtSmtpResultInit(pRet);
	(void)xrtSmtpSendMail(&pCtx->tCfg, &pCtx->tMsg, pRet);

	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pRet;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	xsmtp_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static xfuture* xsmtp_send_mail_future_thread(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtptaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xsmtp_task_ctx_create(pCfg, pMsg, pOpts);
	if ( pCtx == NULL ) {
		XSMTP_TRACE("async thread fallback: task ctx create failed");
		return NULL;
	}

	pFuture = xTaskRunThread(xsmtp_send_mail_task, pCtx, 0);
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async thread fallback: xTaskRunThread failed");
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtSmtpSendMailFuture(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	if ( pCfg == NULL || pMsg == NULL ) {
		return NULL;
	}
	return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
}

static bool xrtSmtpSendMailCo(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pOut)
{
	xfuture* pFuture;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		return FALSE;
	}
	xrtSmtpResultInit(pOut);

	pFuture = xrtSmtpSendMailFuture(pCfg, pMsg, NULL);
	if ( pFuture == NULL ) {
		xsmtp_result_error(pOut, "SMTP async future create failed");
		return FALSE;
	}

	pRet = (xsmtpresult*)xFutureWaitCoValue(pFuture);
	if ( pRet == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pRet = (xsmtpresult*)tResult.pValue;
		}
	}
	if ( pRet == NULL ) {
		str sError = xFutureError(pFuture);
		xsmtp_result_error(pOut, sError ? (const char*)sError : "SMTP async wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}

	*pOut = *pRet;
	xrtSmtpResultFree(pRet);
	xFutureRelease(pFuture);
	return pOut->bSuccess;
}

static bool xrtSmtpSendMailAsyncWait(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts, xsmtpresult* pOut)
{
	xfuture* pFuture;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		return FALSE;
	}
	xrtSmtpResultInit(pOut);

	pFuture = xrtSmtpSendMailFuture(pCfg, pMsg, pOpts);
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async wait: future create returned null");
		xsmtp_result_error(pOut, "SMTP async future create failed");
		return FALSE;
	}
	pRet = (xsmtpresult*)xFutureWaitValue(pFuture);
	if ( pRet == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pRet = (xsmtpresult*)tResult.pValue;
		}
	}
	if ( pRet == NULL ) {
		str sError = xFutureError(pFuture);
		xsmtp_result_error(pOut, sError ? (const char*)sError : "SMTP async wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}

	*pOut = *pRet;
	xrtSmtpResultFree(pRet);
	xFutureRelease(pFuture);
	return pOut->bSuccess;
}

#endif
