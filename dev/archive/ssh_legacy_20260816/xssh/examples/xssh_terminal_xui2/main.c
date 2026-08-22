#define XRT_IMPLEMENTATION
#include "xui.h"
#include "xge.h"
#include "../../xssh.h"
#include "../xssh_example_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XSSH_TERM_W 820
#define XSSH_TERM_H 520
#define XSSH_TERM_OFFSET_X 10.0f
#define XSSH_TERM_OFFSET_Y 20.0f

typedef struct xssh_terminal_xui2_t {
	xui_proxy_t tProxy;
	xui_context pContext;
	xui_surface pTarget;
	xui_font pFont;
	xui_widget pRoot;
	xui_widget pTerminal;
	xui_widget pStatus;
	xsshconfig tConfig;
	xsshauth tAuth;
	xsshpty tPty;
	xssh_session_t* pSession;
	xssh_channel_t* pChannel;
	const char* sPasswordEnv;
	const char* sIdentity;
	int bMock;
	int bConnectOK;
	int bChannelOK;
	int bCreateOK;
	int bLayoutOK;
	int bExerciseDone;
	int bDynamicOK;
	int iFrame;
	int iMaxFrames;
	double fMaxSeconds;
	int iInputBytes;
	int iResizeCount;
	int iXsshEvents;
	int iDataBytes;
	int iLastCols;
	int iLastRows;
} xssh_terminal_xui2_t;

static void xsshTerminalUsage(void)
{
	printf("usage: xssh_terminal_xui2 --mock [--user USER] [--frames N] [--seconds N]\n");
	printf("       xssh_terminal_xui2 --host HOST [--port PORT] --user USER --password-env ENV\n");
	printf("\n");
	printf("options:\n");
	printf("  --mock              use built-in xssh mock runtime\n");
	printf("  --host HOST         SSH host for live password-auth shell transport\n");
	printf("  --port PORT         SSH port, default 22\n");
	printf("  --user USER         username\n");
	printf("  --identity PATH     OpenSSH v1 unencrypted Ed25519 private key path\n");
	printf("  --password-env ENV  read password from environment variable\n");
	printf("  --known-hosts PATH  known_hosts file; useful with --accept-new tests\n");
	printf("  --accept-new        allow accept-new host key policy; default is strict\n");
	printf("  --frames N          run N frames for automated smoke testing\n");
	printf("  --seconds N         run for N seconds for automated smoke testing\n");
	printf("  --help              show this help\n");
}

static const char* xsshTerminalArgValue(int* pi, int argc, char** argv)
{
	if ( *pi + 1 >= argc ) {
		return NULL;
	}
	*pi = *pi + 1;
	return argv[*pi];
}

static int xsshTerminalParseArgs(xssh_terminal_xui2_t* pDemo, int argc, char** argv)
{
	int i;

	xsshConfigInit(&pDemo->tConfig);
	xsshAuthInit(&pDemo->tAuth);
	xsshPtyInit(&pDemo->tPty);
	pDemo->tConfig.sUser = "user";
	pDemo->iMaxFrames = 0;
	pDemo->fMaxSeconds = 0.0;

	for ( i = 1; i < argc; ++i ) {
		if ( strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ) {
			xsshTerminalUsage();
			return 1;
		} else if ( strcmp(argv[i], "--mock") == 0 ) {
			pDemo->bMock = 1;
		} else if ( strcmp(argv[i], "--host") == 0 ) {
			pDemo->tConfig.sHost = xsshTerminalArgValue(&i, argc, argv);
		} else if ( strcmp(argv[i], "--port") == 0 ) {
			const char* sPort = xsshTerminalArgValue(&i, argc, argv);
			pDemo->tConfig.iPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--user") == 0 ) {
			pDemo->tConfig.sUser = xsshTerminalArgValue(&i, argc, argv);
		} else if ( strcmp(argv[i], "--identity") == 0 ) {
			pDemo->sIdentity = xsshTerminalArgValue(&i, argc, argv);
			pDemo->tAuth.sPrivateKeyPath = pDemo->sIdentity;
		} else if ( strcmp(argv[i], "--password-env") == 0 ) {
			pDemo->sPasswordEnv = xsshTerminalArgValue(&i, argc, argv);
		} else if ( strcmp(argv[i], "--known-hosts") == 0 ) {
			pDemo->tConfig.sKnownHostsPath = xsshTerminalArgValue(&i, argc, argv);
		} else if ( strcmp(argv[i], "--accept-new") == 0 ) {
			pDemo->tConfig.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
		} else if ( strcmp(argv[i], "--frames") == 0 ) {
			const char* sFrames = xsshTerminalArgValue(&i, argc, argv);
			pDemo->iMaxFrames = sFrames ? atoi(sFrames) : 0;
			if ( pDemo->iMaxFrames <= 0 ) return XSSH_ERR_INVALID;
		} else if ( strncmp(argv[i], "--frames=", 9) == 0 ) {
			pDemo->iMaxFrames = atoi(argv[i] + 9);
			if ( pDemo->iMaxFrames <= 0 ) return XSSH_ERR_INVALID;
		} else if ( strcmp(argv[i], "--seconds") == 0 ) {
			const char* sSeconds = xsshTerminalArgValue(&i, argc, argv);
			pDemo->fMaxSeconds = sSeconds ? atof(sSeconds) : 0.0;
			if ( pDemo->fMaxSeconds <= 0.0 ) return XSSH_ERR_INVALID;
		} else if ( strncmp(argv[i], "--seconds=", 10) == 0 ) {
			pDemo->fMaxSeconds = atof(argv[i] + 10);
			if ( pDemo->fMaxSeconds <= 0.0 ) return XSSH_ERR_INVALID;
		} else {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return XSSH_ERR_INVALID;
		}
	}
	if ( pDemo->bMock ) {
		pDemo->tConfig.sHost = "mock";
		if ( pDemo->sPasswordEnv == NULL ) {
			pDemo->tAuth.sPassword = "secret";
		}
	}
	if ( pDemo->sPasswordEnv != NULL ) {
		pDemo->tAuth.sPassword = getenv(pDemo->sPasswordEnv);
		if ( pDemo->tAuth.sPassword == NULL ) {
			fprintf(stderr, "password env not set: %s\n", pDemo->sPasswordEnv);
			return XSSH_ERR_INVALID;
		}
	}
	if ( pDemo->tConfig.sHost == NULL || pDemo->tConfig.sUser == NULL ) {
		xsshTerminalUsage();
		return XSSH_ERR_INVALID;
	}
	return XSSH_OK;
}

static const char* xsshTerminalFindTtf(void)
{
	static const char* arrPaths[] = {
		"C:\\Windows\\Fonts\\CascadiaMono.ttf",
		"C:\\Windows\\Fonts\\consola.ttf",
		"C:\\Windows\\Fonts\\SarasaMonoSC-Regular.ttf",
		"C:\\Windows\\Fonts\\NotoSansMonoCJKsc-Regular.otf",
		"C:\\Windows\\Fonts\\simsun.ttc",
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\Deng.ttf"
	};
	FILE* pFile;
	int i;

	for ( i = 0; i < (int)(sizeof(arrPaths) / sizeof(arrPaths[0])); ++i ) {
		pFile = fopen(arrPaths[i], "rb");
		if ( pFile != NULL ) {
			fclose(pFile);
			return arrPaths[i];
		}
	}
	return NULL;
}

static int xsshTerminalRootRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xssh_terminal_xui2_t* pDemo;
	xui_rect_t tRect;
	xui_rect_t tPanel;

	(void)iStateId;
	pDemo = (xssh_terminal_xui2_t*)pUser;
	if ( pWidget == NULL || pDraw == NULL || pDemo == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( pDemo->tProxy.drawRectFill != NULL ) {
		(void)pDemo->tProxy.drawRectFill(&pDemo->tProxy, pDraw, tRect, XUI_COLOR_RGBA(225, 231, 235, 255));
	}
	if ( pDemo->tProxy.drawRoundRectFill != NULL ) {
		tPanel = (xui_rect_t){28.0f, 22.0f, tRect.fW - 56.0f, tRect.fH - 44.0f};
		(void)pDemo->tProxy.drawRoundRectFill(&pDemo->tProxy, pDraw, tPanel, 6.0f, XUI_COLOR_RGBA(246, 248, 250, 255));
	}
	return XUI_OK;
}

static int xsshTerminalAddLabel(xssh_terminal_xui2_t* pDemo, const char* sText, xui_rect_t tRect, xui_widget pParent, uint32_t iColor, uint32_t iFlags, xui_widget* ppLabel)
{
	xui_label_desc_t tDesc;
	xui_widget pLabel;
	int iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = sText;
	tDesc.pFont = pDemo->pFont;
	tDesc.iTextColor = iColor;
	tDesc.iTextFlags = iFlags | XUI_TEXT_CLIP;
	iRet = xuiLabelCreate(pDemo->pContext, &pLabel, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pLabel, tRect);
	(void)xuiWidgetSetHitTestVisible(pLabel, 0);
	iRet = xuiWidgetAddChild(pParent, pLabel);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pLabel);
		return iRet;
	}
	if ( ppLabel != NULL ) *ppLabel = pLabel;
	return XUI_OK;
}

static void xsshTerminalDrainEvents(xssh_terminal_xui2_t* pDemo)
{
	xsshevent ev;
	int iRet;

	if ( pDemo == NULL || pDemo->pSession == NULL || pDemo->pTerminal == NULL ) return;
	while ( xsshNextEvent(pDemo->pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
		pDemo->iXsshEvents++;
		if ( (ev.iType == XSSH_EVENT_CHANNEL_DATA || ev.iType == XSSH_EVENT_CHANNEL_EXTENDED_DATA) && ev.pData != NULL && ev.iLen > 0u ) {
			iRet = xuiTerminalWrite(pDemo->pTerminal, ev.pData, (int)ev.iLen);
			if ( iRet == XUI_OK ) {
				pDemo->iDataBytes += (int)ev.iLen;
			}
		}
	}
	(void)xuiTerminalFlush(pDemo->pTerminal);
}

static void xsshTerminalInput(xui_widget pWidget, const uint8_t* pData, int iSize, void* pUser)
{
	xssh_terminal_xui2_t* pDemo = (xssh_terminal_xui2_t*)pUser;
	size_t iWritten = 0u;

	(void)pWidget;
	if ( pDemo == NULL || pDemo->pChannel == NULL || pData == NULL || iSize <= 0 ) return;
	/* 输入回调运行在 UI 线程；当前 xsshChannelWrite 是同步发送，未来后台 pump 只能通过事件队列回到 UI 线程。 */
	if ( xsshChannelWrite(pDemo->pChannel, pData, (size_t)iSize, &iWritten) == XSSH_OK ) {
		pDemo->iInputBytes += (int)iWritten;
	}
}

static void xsshTerminalResize(xui_widget pWidget, int iColumns, int iRows, void* pUser)
{
	xssh_terminal_xui2_t* pDemo = (xssh_terminal_xui2_t*)pUser;

	(void)pWidget;
	if ( pDemo == NULL ) return;
	pDemo->iResizeCount++;
	pDemo->iLastCols = iColumns;
	pDemo->iLastRows = iRows;
	if ( pDemo->pChannel != NULL ) {
		/* window-change 只携带终端尺寸；像素尺寸在当前 XUI 回调中不可得，保持为 0。 */
		(void)xsshChannelResizePty(pDemo->pChannel, (uint32)iColumns, (uint32)iRows, 0u, 0u);
	}
}

static int xsshTerminalOpenShell(xssh_terminal_xui2_t* pDemo)
{
	int iRet;

	iRet = xsshConnect(&pDemo->tConfig, &pDemo->tAuth, &pDemo->pSession);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "xssh_terminal_xui2: connect failed", pDemo->pSession, iRet);
		return iRet;
	}
	pDemo->bConnectOK = 1;
	xsshTerminalDrainEvents(pDemo);
	iRet = xsshOpenSessionChannel(pDemo->pSession, &pDemo->pChannel);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "xssh_terminal_xui2: session channel failed", pDemo->pSession, iRet);
		return iRet;
	}
	iRet = xsshChannelRequestPty(pDemo->pChannel, &pDemo->tPty);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "xssh_terminal_xui2: pty request failed", pDemo->pSession, iRet);
		return iRet;
	}
	iRet = xsshChannelRequestShell(pDemo->pChannel);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "xssh_terminal_xui2: shell request failed", pDemo->pSession, iRet);
		return iRet;
	}
	pDemo->bChannelOK = 1;
	xsshTerminalDrainEvents(pDemo);
	return XSSH_OK;
}

static int xsshTerminalCreateUi(xssh_terminal_xui2_t* pDemo)
{
	xui_terminal_desc_t tDesc;
	int iRet;

	iRet = xuiWidgetCreate(pDemo->pContext, &pDemo->pRoot);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pDemo->pRoot, (xui_rect_t){0.0f, 0.0f, (float)XSSH_TERM_W, (float)XSSH_TERM_H});
	(void)xuiWidgetSetLayoutType(pDemo->pRoot, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetCacheRenderCallback(pDemo->pRoot, xsshTerminalRootRender, pDemo);
	iRet = xuiSetRootWidget(pDemo->pContext, pDemo->pRoot);
	if ( iRet != XUI_OK ) return iRet;

	iRet = xsshTerminalAddLabel(pDemo, "xssh native terminal", (xui_rect_t){48.0f, 38.0f, 260.0f, 24.0f}, pDemo->pRoot, XUI_COLOR_RGBA(36, 48, 64, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE, NULL);
	if ( iRet != XUI_OK ) return iRet;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = pDemo->pFont;
	tDesc.iColumns = 92;
	tDesc.iRows = 25;
	tDesc.iScrollbackLimit = 512;
	tDesc.fCellWidth = 0.0f;
	tDesc.fCellHeight = 16.0f;
	tDesc.fPadding = 8.0f;
	tDesc.iBackgroundColor = XUI_COLOR_RGBA(9, 14, 22, 255);
	tDesc.iForegroundColor = XUI_COLOR_RGBA(222, 232, 242, 255);
	tDesc.iCursorColor = XUI_COLOR_RGBA(250, 250, 250, 255);
	iRet = xuiTerminalCreate(pDemo->pContext, &pDemo->pTerminal, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pDemo->pTerminal, (xui_rect_t){48.0f, 70.0f, 724.0f, 390.0f});
	(void)xuiTerminalSetInputCallback(pDemo->pTerminal, xsshTerminalInput, pDemo);
	(void)xuiTerminalSetResizeCallback(pDemo->pTerminal, xsshTerminalResize, pDemo);
	iRet = xuiWidgetAddChild(pDemo->pRoot, pDemo->pTerminal);
	if ( iRet != XUI_OK ) return iRet;

	(void)xuiTerminalWriteText(pDemo->pTerminal, "\x1b[1;36mxssh\x1b[0m native channel attached to XUI2 Terminal\r\n");
	if ( pDemo->bMock ) {
		(void)xuiTerminalWriteText(pDemo->pTerminal, "strict host key policy is the default; this smoke uses --mock.\r\n\r\n");
	} else {
		(void)xuiTerminalWriteText(pDemo->pTerminal, "live SSH shell: strict host key policy is the default.\r\n\r\n");
	}
	(void)xuiTerminalFlush(pDemo->pTerminal);

	iRet = xsshTerminalOpenShell(pDemo);
	if ( iRet != XSSH_OK ) return iRet;

	return xsshTerminalAddLabel(pDemo, "frames=0 input=0 resize=0 events=0 bytes=0", (xui_rect_t){48.0f, 472.0f, 724.0f, 22.0f}, pDemo->pRoot, XUI_COLOR_RGBA(68, 82, 102, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE, &pDemo->pStatus);
}

static int xsshTerminalCreateAssets(xssh_terminal_xui2_t* pDemo)
{
	xui_surface_desc_t tSurfaceDesc;
	const char* sFontPath;
	int iRet;

	pDemo->tProxy = xuiProxyXge();
	iRet = xuiCreate(&pDemo->pContext);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiSetProxy(pDemo->pContext, &pDemo->tProxy);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInputViewport(pDemo->pContext, (float)XSSH_TERM_W, (float)XSSH_TERM_H);
	if ( iRet != XUI_OK ) return iRet;
	memset(&tSurfaceDesc, 0, sizeof(tSurfaceDesc));
	tSurfaceDesc.iKind = XUI_SURFACE_KIND_TEXTURE;
	tSurfaceDesc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
	tSurfaceDesc.iWidth = XSSH_TERM_W;
	tSurfaceDesc.iHeight = XSSH_TERM_H;
	tSurfaceDesc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
	iRet = pDemo->tProxy.surfaceCreate(&pDemo->tProxy, &pDemo->pTarget, &tSurfaceDesc);
	if ( iRet != XUI_OK ) return iRet;
	sFontPath = xsshTerminalFindTtf();
	if ( sFontPath == NULL ) return XUI_ERROR_FILE_NOT_FOUND;
	iRet = pDemo->tProxy.fontLoadFile(&pDemo->tProxy, &pDemo->pFont, sFontPath, 14.0f, XUI_FONT_FORMAT_TTF);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiSetDefaultFont(pDemo->pContext, pDemo->pFont);
	return xsshTerminalCreateUi(pDemo);
}

static void xsshTerminalDestroyAssets(xssh_terminal_xui2_t* pDemo)
{
	if ( pDemo->pChannel != NULL ) {
		(void)xsshChannelClose(pDemo->pChannel);
		pDemo->pChannel = NULL;
	}
	if ( pDemo->pSession != NULL ) {
		(void)xsshDisconnect(pDemo->pSession);
		xsshFree(pDemo->pSession);
		pDemo->pSession = NULL;
	}
	if ( pDemo->pContext != NULL ) {
		xuiDestroy(pDemo->pContext);
		pDemo->pContext = NULL;
	}
	if ( pDemo->pFont != NULL ) {
		pDemo->tProxy.fontDestroy(&pDemo->tProxy, pDemo->pFont);
		pDemo->pFont = NULL;
	}
	if ( pDemo->pTarget != NULL ) {
		pDemo->tProxy.surfaceDestroy(&pDemo->tProxy, pDemo->pTarget);
		pDemo->pTarget = NULL;
	}
}

static void xsshTerminalRunChecks(xssh_terminal_xui2_t* pDemo, int bAutoRun)
{
	if ( pDemo == NULL || !bAutoRun || pDemo->bExerciseDone ) return;
	pDemo->bDynamicOK = (xuiTerminalInputText(pDemo->pTerminal, "echo from xui2\r") == XUI_OK);
	pDemo->bDynamicOK = pDemo->bDynamicOK && (xuiTerminalResize(pDemo->pTerminal, 96, 26) == XUI_OK);
	pDemo->bExerciseDone = 1;
}

static void xsshTerminalUpdateStatus(xssh_terminal_xui2_t* pDemo)
{
	char sText[192];

	if ( pDemo == NULL || pDemo->pStatus == NULL ) return;
	snprintf(sText, sizeof(sText), "frames=%d input=%d resize=%d events=%d bytes=%d cols=%d rows=%d changes=%d",
		pDemo->iFrame,
		pDemo->iInputBytes,
		pDemo->iResizeCount,
		pDemo->iXsshEvents,
		pDemo->iDataBytes,
		xuiTerminalGetColumns(pDemo->pTerminal),
		xuiTerminalGetRows(pDemo->pTerminal),
		xuiTerminalGetChangeCount(pDemo->pTerminal));
	(void)xuiLabelSetText(pDemo->pStatus, sText);
}

static int xsshTerminalFrame(void* pUser)
{
	xssh_terminal_xui2_t* pDemo;
	xui_rect_i_t tFullRect;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	xui_render_stats_t tStats;
	xui_cache_stats_t tCacheStats;
	int iRet;
	int bAutoRun;

	pDemo = (xssh_terminal_xui2_t*)pUser;
	if ( pDemo == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	bAutoRun = (pDemo->iMaxFrames > 0) || (pDemo->fMaxSeconds > 0.0);
	iRet = xgeBegin();
	if ( iRet != XGE_OK ) return iRet;
	if ( xgeKeyPressed(XGE_KEY_ESCAPE) ) xgeQuit();
	iRet = xuiDispatchPendingEvents(pDemo->pContext);
	if ( iRet != XUI_OK ) return iRet;
	xsshTerminalRunChecks(pDemo, bAutoRun);
	if ( pDemo->pSession != NULL && xsshPoll(pDemo->pSession, 0u) == XSSH_OK ) {
		xsshTerminalDrainEvents(pDemo);
	}
	iRet = xuiUpdate(pDemo->pContext, xgeGetDelta());
	if ( iRet != XUI_OK ) return iRet;
	xsshTerminalUpdateStatus(pDemo);
	(void)xuiWidgetSetRect(pDemo->pRoot, (xui_rect_t){0.0f, 0.0f, (float)XSSH_TERM_W, (float)XSSH_TERM_H});
	iRet = xuiLayout(pDemo->pContext);
	if ( iRet != XUI_OK ) return iRet;
	pDemo->bLayoutOK = (xuiTerminalGetColumns(pDemo->pTerminal) > 0 && xuiTerminalGetRows(pDemo->pTerminal) > 0);
	iRet = pDemo->tProxy.surfaceClear(&pDemo->tProxy, pDemo->pTarget, XUI_COLOR_RGBA(220, 226, 232, 255));
	if ( iRet != XUI_OK ) return iRet;
	tFullRect = (xui_rect_i_t){0, 0, XSSH_TERM_W, XSSH_TERM_H};
	iRet = xuiRender(pDemo->pContext, pDemo->pTarget, &tFullRect, 1);
	if ( iRet != XUI_OK ) return iRet;
	xgeClear(XUI_COLOR_RGBA(18, 22, 28, 255));
	tSrc = (xui_rect_t){0.0f, 0.0f, (float)XSSH_TERM_W, (float)XSSH_TERM_H};
	tDst = (xui_rect_t){XSSH_TERM_OFFSET_X, XSSH_TERM_OFFSET_Y, (float)XSSH_TERM_W, (float)XSSH_TERM_H};
	iRet = pDemo->tProxy.surfaceDraw(&pDemo->tProxy, pDemo->pTarget, tSrc, tDst, XUI_COLOR_WHITE, XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xgeEnd();
	if ( iRet != XGE_OK ) return iRet;
	pDemo->iFrame++;
	if ( (pDemo->iMaxFrames > 0 && pDemo->iFrame >= pDemo->iMaxFrames) ||
	     (pDemo->fMaxSeconds > 0.0 && xgeTimer() >= pDemo->fMaxSeconds) ) {
		memset(&tStats, 0, sizeof(tStats));
		memset(&tCacheStats, 0, sizeof(tCacheStats));
		tStats.iSize = sizeof(tStats);
		tCacheStats.iSize = sizeof(tCacheStats);
		(void)xuiGetRenderStats(pDemo->pContext, &tStats);
		(void)xuiGetCacheStats(pDemo->pContext, &tCacheStats);
		printf("xssh_terminal_xui2 final-summary frames=%d connect=%d channel=%d layout=%d dynamic=%d input=%d resize=%d events=%d bytes=%d cols=%d rows=%d changes=%d updatedCaches=%d drawnCaches=%d cacheSurfaces=%d\n",
			pDemo->iFrame,
			pDemo->bConnectOK,
			pDemo->bChannelOK,
			pDemo->bLayoutOK,
			pDemo->bDynamicOK,
			pDemo->iInputBytes,
			pDemo->iResizeCount,
			pDemo->iXsshEvents,
			pDemo->iDataBytes,
			xuiTerminalGetColumns(pDemo->pTerminal),
			xuiTerminalGetRows(pDemo->pTerminal),
			xuiTerminalGetChangeCount(pDemo->pTerminal),
			tStats.iUpdatedCaches,
			tStats.iDrawnCaches,
			tCacheStats.iSurfaceCount);
		xgeQuit();
	}
	return XGE_OK;
}

int main(int argc, char** argv)
{
	xssh_terminal_xui2_t tDemo;
	xge_desc_t tDesc;
	int iRet;

	memset(&tDemo, 0, sizeof(tDemo));
	iRet = xsshTerminalParseArgs(&tDemo, argc, argv);
	if ( iRet == 1 ) return 0;
	if ( iRet != XSSH_OK ) {
		return 2;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iWidth = XSSH_TERM_W + 20;
	tDesc.iHeight = XSSH_TERM_H + 50;
	tDesc.sTitle = "xssh XUI2 Terminal";
	tDesc.iFlags = XGE_INIT_WINDOW | XGE_INIT_VSYNC;
	tDesc.iRunMode = XGE_RUN_GAME_LOOP;
	tDesc.iTargetFPS = 60;
	iRet = xgeInit(&tDesc);
	if ( iRet != XGE_OK ) {
		printf("xssh_terminal_xui2: xgeInit failed: %d\n", iRet);
		return 1;
	}
	iRet = xsshTerminalCreateAssets(&tDemo);
	if ( iRet != XUI_OK && iRet != XSSH_OK ) {
		printf("xssh_terminal_xui2: create assets failed: %d\n", iRet);
		xsshTerminalDestroyAssets(&tDemo);
		xgeUnit();
		return 1;
	}
	tDemo.bCreateOK = 1;
	iRet = xgeRun(xsshTerminalFrame, &tDemo);
	xsshTerminalDestroyAssets(&tDemo);
	xgeUnit();
	return (iRet == XGE_OK && tDemo.bCreateOK && tDemo.bConnectOK && tDemo.bChannelOK &&
	        tDemo.bLayoutOK && ((tDemo.iMaxFrames <= 0 && tDemo.fMaxSeconds <= 0.0) || tDemo.bDynamicOK)) ? 0 : 1;
}
