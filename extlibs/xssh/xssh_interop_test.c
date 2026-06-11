#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_interop_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static const char* xssh_interop_env(const char* sName)
{
	const char* sValue = getenv(sName);
	return (sValue != NULL && sValue[0] != '\0') ? sValue : NULL;
}

static const char* xssh_interop_text_or_dash(const char* sText)
{
	return (sText != NULL && sText[0] != '\0') ? sText : "-";
}

static void xssh_interop_print_result(const char* sPrefix, xssh_session_t* pSession)
{
	xsshresult result;

	if ( sPrefix == NULL || pSession == NULL || xsshGetLastResult(pSession, &result) != XSSH_OK ) {
		return;
	}
	printf("%s stage=%d error=%d disconnect=%d banner=%s kex=%s hostkey=%s c2s=%s s2c=%s mac-c2s=%s mac-s2c=%s auth=%s fingerprint=%s text=%s\n",
		sPrefix,
		result.iStage,
		result.iError,
		result.iDisconnectReason,
		xssh_interop_text_or_dash(result.sServerBanner),
		xssh_interop_text_or_dash(result.sKex),
		xssh_interop_text_or_dash(result.sHostKey),
		xssh_interop_text_or_dash(result.sCipherClientToServer),
		xssh_interop_text_or_dash(result.sCipherServerToClient),
		xssh_interop_text_or_dash(result.sMacClientToServer),
		xssh_interop_text_or_dash(result.sMacServerToClient),
		xssh_interop_text_or_dash(result.sAuthMethods),
		xssh_interop_text_or_dash(result.sHostKeyFingerprint),
		xssh_interop_text_or_dash(result.sError));
}

int main(void)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	const char* sHost = xssh_interop_env("XSSH_LIVE_HOST");
	const char* sPort = xssh_interop_env("XSSH_LIVE_PORT");
	const char* sUser = xssh_interop_env("XSSH_LIVE_USER");
	const char* sPassword = xssh_interop_env("XSSH_LIVE_PASSWORD");
	const char* sIdentity = xssh_interop_env("XSSH_LIVE_IDENTITY");
	const char* sKnownHosts = xssh_interop_env("XSSH_LIVE_KNOWN_HOSTS");
	const char* sAcceptNew = xssh_interop_env("XSSH_LIVE_ACCEPT_NEW");
	const char* sCommand = xssh_interop_env("XSSH_LIVE_COMMAND");
	int iRet;

	if ( sHost == NULL ) {
		printf("xssh_interop_test: SKIP (set XSSH_LIVE_HOST to enable live SSH interop)\n");
		return 0;
	}
	if ( sUser == NULL ) {
		return xssh_interop_fail("interop_env", "XSSH_LIVE_USER is required when XSSH_LIVE_HOST is set");
	}
	if ( sPassword == NULL && sIdentity == NULL ) {
		printf("xssh_interop_test: SKIP (set XSSH_LIVE_PASSWORD or XSSH_LIVE_IDENTITY for live auth interop)\n");
		return 0;
	}

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sHost = sHost;
	cfg.sUser = sUser;
	cfg.sKnownHostsPath = sKnownHosts;
	if ( sAcceptNew != NULL ) {
		cfg.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
	}
	if ( sPort != NULL ) {
		cfg.iPort = (uint16)atoi(sPort);
	}
	if ( sPassword != NULL ) {
		auth.sPassword = sPassword;
	} else {
		auth.sPrivateKeyPath = sIdentity;
	}
	if ( sCommand == NULL ) {
		sCommand = "printf xssh-live";
	}

	iRet = xsshConnect(&cfg, &auth, &pSession);
	if ( iRet == XSSH_ERR_UNSUPPORTED ) {
		if ( pSession != NULL ) {
			xssh_interop_print_result("xssh_interop_test: SKIP unsupported result", pSession);
			xsshFree(pSession);
			return 0;
		}
		return xssh_interop_fail("interop_connect", "unsupported without result");
	}
	if ( iRet != XSSH_OK ) {
		if ( pSession != NULL ) {
			xssh_interop_print_result("xssh_interop_test: connect failed", pSession);
			xsshFree(pSession);
		}
		return xssh_interop_fail("interop_connect", "live connect failed");
	}
	xssh_interop_print_result("xssh_interop_test: connected", pSession);
	{
		xssh_channel_t* pChannel = NULL;
		xsshevent ev;
		uint32 iExitStatus = 999u;
		char sStdout[512];
		size_t iStdoutLen = 0u;
		bool bClosed = FALSE;
		int i;

		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
		}
		if ( xsshOpenSessionChannel(pSession, &pChannel) != XSSH_OK || pChannel == NULL ) {
			xssh_interop_print_result("xssh_interop_test: channel open failed", pSession);
			xsshDisconnect(pSession);
			xsshFree(pSession);
			return xssh_interop_fail("interop_channel", "session channel open failed");
		}
		if ( xsshChannelRequestExec(pChannel, sCommand) != XSSH_OK ) {
			xssh_interop_print_result("xssh_interop_test: exec request failed", pSession);
			xsshDisconnect(pSession);
			xsshFree(pSession);
			return xssh_interop_fail("interop_channel", "exec request failed");
		}
		memset(sStdout, 0, sizeof(sStdout));
		for ( i = 0; i < 500 && !bClosed; ++i ) {
			bool bHadEvent = FALSE;
			iRet = xsshPoll(pSession, 0u);
			if ( iRet != XSSH_OK ) {
				xssh_interop_print_result("xssh_interop_test: poll failed", pSession);
				xsshDisconnect(pSession);
				xsshFree(pSession);
				return xssh_interop_fail("interop_channel", "poll failed");
			}
			while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
				bHadEvent = TRUE;
				if ( ev.iType == XSSH_EVENT_CHANNEL_DATA && ev.pData != NULL && ev.iLen != 0u ) {
					size_t iCopy = ev.iLen;
					if ( iCopy > sizeof(sStdout) - 1u - iStdoutLen ) {
						iCopy = sizeof(sStdout) - 1u - iStdoutLen;
					}
					if ( iCopy != 0u ) {
						memcpy(sStdout + iStdoutLen, ev.pData, iCopy);
						iStdoutLen += iCopy;
					}
				} else if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE ) {
					bClosed = TRUE;
				}
			}
			if ( !bHadEvent ) {
				xrtSleep(10u);
			}
		}
		if ( xsshChannelGetExitStatus(pChannel, &iExitStatus) != XSSH_OK || iExitStatus != 0u || iStdoutLen == 0u ) {
			xssh_interop_print_result("xssh_interop_test: exec result mismatch", pSession);
			xsshDisconnect(pSession);
			xsshFree(pSession);
			return xssh_interop_fail("interop_channel", "exec result mismatch");
		}
		printf("xssh_interop_test: live exec stdout=%s exit=%u\n", sStdout, (unsigned)iExitStatus);
	}
	xsshDisconnect(pSession);
	xsshFree(pSession);
	printf("xssh_interop_test: PASS\n");
	return 0;
}
