#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../xssh.h"
#include "xssh_example_common.h"

static void xssh_exec_help(void)
{
	printf("usage: xssh_exec --mock [--user USER] [--command CMD]\n");
	printf("       xssh_exec --host HOST [--port PORT] --user USER [--password-env ENV|--identity PATH] --command CMD\n");
	printf("\n");
	printf("options:\n");
	printf("  --mock              use built-in xssh mock runtime\n");
	printf("  --host HOST         SSH host for live session/exec transport\n");
	printf("  --port PORT         SSH port, default 22\n");
	printf("  --user USER         username\n");
	printf("  --password-env ENV  read password from environment variable\n");
	printf("  --identity PATH     OpenSSH v1 unencrypted Ed25519 private key path\n");
	printf("  --known-hosts PATH  known_hosts file; useful with --accept-new tests\n");
	printf("  --accept-new        allow accept-new host key policy\n");
	printf("  --command CMD       command to execute, default: printf ok\n");
	printf("  --help              show this help\n");
}

static const char* xssh_exec_arg_value(int* pi, int argc, char** argv)
{
	if ( *pi + 1 >= argc ) {
		return NULL;
	}
	*pi = *pi + 1;
	return argv[*pi];
}

int main(int argc, char** argv)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	const char* sPasswordEnv = NULL;
	const char* sCommand = "printf ok";
	bool bMock = FALSE;
	int i;
	int iRet;
	uint32 iExitStatus = 1u;
	xsshevent ev;
	bool bClosed = FALSE;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	cfg.sUser = "user";

	for ( i = 1; i < argc; ++i ) {
		if ( strcmp(argv[i], "--help") == 0 ) {
			xssh_exec_help();
			return 0;
		} else if ( strcmp(argv[i], "--mock") == 0 ) {
			bMock = TRUE;
		} else if ( strcmp(argv[i], "--host") == 0 ) {
			cfg.sHost = xssh_exec_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--port") == 0 ) {
			const char* sPort = xssh_exec_arg_value(&i, argc, argv);
			cfg.iPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--user") == 0 ) {
			cfg.sUser = xssh_exec_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--password-env") == 0 ) {
			sPasswordEnv = xssh_exec_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--identity") == 0 ) {
			auth.sPrivateKeyPath = xssh_exec_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--known-hosts") == 0 ) {
			cfg.sKnownHostsPath = xssh_exec_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--accept-new") == 0 ) {
			cfg.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
		} else if ( strcmp(argv[i], "--command") == 0 ) {
			sCommand = xssh_exec_arg_value(&i, argc, argv);
		} else {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return 2;
		}
	}
	if ( bMock ) {
		cfg.sHost = "mock";
		if ( sPasswordEnv == NULL ) {
			auth.sPassword = "secret";
		}
	}
	if ( sPasswordEnv != NULL ) {
		auth.sPassword = getenv(sPasswordEnv);
		if ( auth.sPassword == NULL ) {
			fprintf(stderr, "password env not set: %s\n", sPasswordEnv);
			return 2;
		}
	}
	if ( cfg.sHost == NULL || cfg.sUser == NULL || sCommand == NULL ) {
		xssh_exec_help();
		return 2;
	}

	iRet = xsshConnect(&cfg, &auth, &pSession);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "connect failed", pSession, iRet);
		if ( pSession != NULL ) {
			xsshFree(pSession);
		}
		return 2;
	}
	while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
	}

	iRet = xsshOpenSessionChannel(pSession, &pChannel);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "session channel failed", pSession, iRet);
		xsshFree(pSession);
		return 2;
	}
	iRet = xsshChannelRequestExec(pChannel, sCommand);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "exec request failed", pSession, iRet);
		xsshFree(pSession);
		return 2;
	}

	for ( i = 0; i < 500 && !bClosed; ++i ) {
		bool bHadEvent = FALSE;
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			break;
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			bHadEvent = TRUE;
			if ( ev.iType == XSSH_EVENT_CHANNEL_DATA && ev.pData != NULL ) {
				fwrite(ev.pData, 1u, ev.iLen, stdout);
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_EXTENDED_DATA && ev.pData != NULL ) {
				fwrite(ev.pData, 1u, ev.iLen, stderr);
			} else if ( ev.iType == XSSH_EVENT_CHANNEL_CLOSE ) {
				bClosed = TRUE;
			}
		}
		if ( !bHadEvent ) {
			xrtSleep(10u);
		}
	}
	if ( xsshChannelGetExitStatus(pChannel, &iExitStatus) == XSSH_OK ) {
		printf("exit-status=%u\n", (unsigned)iExitStatus);
	}
	xsshChannelClose(pChannel);
	xsshDisconnect(pSession);
	xsshFree(pSession);
	return (iExitStatus == 0u) ? 0 : 1;
}
