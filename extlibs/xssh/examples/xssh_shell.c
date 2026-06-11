#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../xssh.h"
#include "xssh_example_common.h"

static void xssh_shell_help(void)
{
	printf("usage: xssh_shell --mock [--user USER] [--input TEXT] [--frames N]\n");
	printf("       xssh_shell --host HOST [--port PORT] --user USER [--password-env ENV|--identity PATH]\n");
	printf("\n");
	printf("options:\n");
	printf("  --mock              use built-in xssh mock runtime\n");
	printf("  --host HOST         SSH host for live shell transport\n");
	printf("  --port PORT         SSH port, default 22\n");
	printf("  --user USER         username\n");
	printf("  --password-env ENV  read password from environment variable\n");
	printf("  --identity PATH     OpenSSH v1 unencrypted Ed25519 private key path\n");
	printf("  --known-hosts PATH  known_hosts file; useful with --accept-new tests\n");
	printf("  --accept-new        allow accept-new host key policy\n");
	printf("  --input TEXT        write TEXT to shell after opening it\n");
	printf("  --frames N          poll N frames, default 3\n");
	printf("  --seconds N         accepted for CLI compatibility; frame count drives polling\n");
	printf("  --help              show this help\n");
}

static const char* xssh_shell_arg_value(int* pi, int argc, char** argv)
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
	xsshpty pty;
	xssh_session_t* pSession = NULL;
	xssh_channel_t* pChannel = NULL;
	const char* sPasswordEnv = NULL;
	const char* sInput = NULL;
	bool bMock = FALSE;
	int iFrames = 3;
	int i;
	xsshevent ev;
	size_t iWritten = 0u;
	int iRet;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	xsshPtyInit(&pty);
	cfg.sUser = "user";

	for ( i = 1; i < argc; ++i ) {
		if ( strcmp(argv[i], "--help") == 0 ) {
			xssh_shell_help();
			return 0;
		} else if ( strcmp(argv[i], "--mock") == 0 ) {
			bMock = TRUE;
		} else if ( strcmp(argv[i], "--host") == 0 ) {
			cfg.sHost = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--port") == 0 ) {
			const char* sPort = xssh_shell_arg_value(&i, argc, argv);
			cfg.iPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--user") == 0 ) {
			cfg.sUser = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--password-env") == 0 ) {
			sPasswordEnv = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--identity") == 0 ) {
			auth.sPrivateKeyPath = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--known-hosts") == 0 ) {
			cfg.sKnownHostsPath = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--accept-new") == 0 ) {
			cfg.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
		} else if ( strcmp(argv[i], "--input") == 0 ) {
			sInput = xssh_shell_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--frames") == 0 ) {
			const char* sFrames = xssh_shell_arg_value(&i, argc, argv);
			iFrames = sFrames ? atoi(sFrames) : 0;
		} else if ( strcmp(argv[i], "--seconds") == 0 ) {
			(void)xssh_shell_arg_value(&i, argc, argv);
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
	if ( cfg.sHost == NULL || cfg.sUser == NULL ) {
		xssh_shell_help();
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
	iRet = xsshChannelRequestPty(pChannel, &pty);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "pty request failed", pSession, iRet);
		xsshFree(pSession);
		return 2;
	}
	iRet = xsshChannelRequestShell(pChannel);
	if ( iRet != XSSH_OK ) {
		xsshExamplePrintSessionResult(stderr, "shell request failed", pSession, iRet);
		xsshFree(pSession);
		return 2;
	}
	if ( sInput != NULL ) {
		xsshChannelWrite(pChannel, sInput, strlen(sInput), &iWritten);
	}
	for ( i = 0; i < iFrames; ++i ) {
		if ( xsshPoll(pSession, 0u) != XSSH_OK ) {
			break;
		}
		while ( xsshNextEvent(pSession, &ev) == XSSH_OK && ev.iType != XSSH_EVENT_NONE ) {
			if ( ev.iType == XSSH_EVENT_CHANNEL_DATA && ev.pData != NULL ) {
				fwrite(ev.pData, 1u, ev.iLen, stdout);
			}
		}
	}
	xsshChannelClose(pChannel);
	xsshDisconnect(pSession);
	xsshFree(pSession);
	return 0;
}
