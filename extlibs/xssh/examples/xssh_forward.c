#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../xssh.h"
#include "xssh_example_common.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSSH_FORWARD_CLOSESOCK closesocket
#define XSSH_FORWARD_SHUT_WR SD_SEND
#else
#define XSSH_FORWARD_CLOSESOCK close
#define XSSH_FORWARD_SHUT_WR SHUT_WR
#endif

typedef struct {
	xsocket hListen;
	xsocket hLocal;
	uint8 arrUp[4096];
	size_t iUpPos;
	size_t iUpLen;
	uint8 arrDown[4096];
	size_t iDownPos;
	size_t iDownLen;
	size_t iLocalToRemote;
	size_t iRemoteToLocal;
	bool bLocalEof;
	bool bRemoteEof;
	bool bRemoteClosed;
	bool bSentChannelEof;
	bool bIgnoreNextLocalEofEvent;
} xssh_forward_pump;

typedef struct {
	const char* sHost;
	uint16 iPort;
	bool bSuccess;
	char sRecv[64];
} xssh_forward_selftest_client;

static void xssh_forward_help(void)
{
	printf("usage: xssh_forward --mock --bind-port PORT --target-host HOST --target-port PORT\n");
	printf("       xssh_forward --host HOST --user USER [--password-env ENV|--identity PATH] --bind-port PORT --target-host HOST --target-port PORT\n");
	printf("\n");
	printf("options:\n");
	printf("  --mock              use built-in xssh mock runtime\n");
	printf("  --host HOST         SSH host for live local forwarding\n");
	printf("  --port PORT         SSH port, default 22\n");
	printf("  --user USER         username\n");
	printf("  --password-env ENV  read password from environment variable\n");
	printf("  --identity PATH     OpenSSH v1 unencrypted Ed25519 private key path\n");
	printf("  --known-hosts PATH  known_hosts file; useful with --accept-new tests\n");
	printf("  --accept-new        allow accept-new host key policy\n");
	printf("  --bind-host HOST    local bind host, default 127.0.0.1\n");
	printf("  --bind-port PORT    local bind port\n");
	printf("  --target-host HOST  remote target host\n");
	printf("  --target-port PORT  remote target port\n");
	printf("  --accept-timeout-ms N  live accept timeout, default 0 means wait forever\n");
	printf("  --idle-timeout-ms N    live idle timeout after accept, default 30000\n");
	printf("  --frames N          mock frame count, default 1\n");
	printf("  --self-test         run one local socket pump check in mock mode\n");
	printf("  --help              show this help\n");
}

static const char* xssh_forward_arg_value(int* pi, int argc, char** argv)
{
	if ( *pi + 1 >= argc ) {
		return NULL;
	}
	*pi = *pi + 1;
	return argv[*pi];
}

static int xssh_forward_last_error(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return WSAGetLastError();
#else
	return errno;
#endif
}

static bool xssh_forward_would_block(int iErr)
{
#if defined(_WIN32) || defined(_WIN64)
	return (iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEALREADY) ? TRUE : FALSE;
#else
	return (iErr == EAGAIN || iErr == EWOULDBLOCK || iErr == EINPROGRESS) ? TRUE : FALSE;
#endif
}

static bool xssh_forward_set_nonblock(xsocket hSocket, bool bEnable)
{
#if defined(_WIN32) || defined(_WIN64)
	u_long iMode = bEnable ? 1u : 0u;
	return (ioctlsocket(hSocket, FIONBIO, &iMode) == 0) ? TRUE : FALSE;
#else
	int iFlags = fcntl(hSocket, F_GETFL, 0);
	if ( iFlags < 0 ) {
		return FALSE;
	}
	if ( bEnable ) {
		iFlags |= O_NONBLOCK;
	} else {
		iFlags &= ~O_NONBLOCK;
	}
	return (fcntl(hSocket, F_SETFL, iFlags) == 0) ? TRUE : FALSE;
#endif
}

static void xssh_forward_pump_init(xssh_forward_pump* pPump)
{
	memset(pPump, 0, sizeof(*pPump));
	pPump->hListen = XSOCKET_INVALID;
	pPump->hLocal = XSOCKET_INVALID;
}

static void xssh_forward_pump_close(xssh_forward_pump* pPump)
{
	if ( pPump == NULL ) {
		return;
	}
	if ( pPump->hLocal != XSOCKET_INVALID ) {
		XSSH_FORWARD_CLOSESOCK(pPump->hLocal);
		pPump->hLocal = XSOCKET_INVALID;
	}
	if ( pPump->hListen != XSOCKET_INVALID ) {
		XSSH_FORWARD_CLOSESOCK(pPump->hListen);
		pPump->hListen = XSOCKET_INVALID;
	}
}

static bool xssh_forward_parse_bind_ipv4(const char* sHost, uint16 iPort, struct sockaddr_in* pAddr)
{
	unsigned long iAddr;

	if ( pAddr == NULL ) {
		return FALSE;
	}
	memset(pAddr, 0, sizeof(*pAddr));
	pAddr->sin_family = AF_INET;
	pAddr->sin_port = htons(iPort);
	if ( sHost == NULL || sHost[0] == '\0' || strcmp(sHost, "0.0.0.0") == 0 || strcmp(sHost, "*") == 0 ) {
		pAddr->sin_addr.s_addr = htonl(INADDR_ANY);
		return TRUE;
	}
	if ( strcmp(sHost, "localhost") == 0 ) {
		sHost = "127.0.0.1";
	}
	iAddr = inet_addr(sHost);
	if ( iAddr == INADDR_NONE && strcmp(sHost, "255.255.255.255") != 0 ) {
		return FALSE;
	}
	pAddr->sin_addr.s_addr = iAddr;
	return TRUE;
}

static bool xssh_forward_send_all_socket(xsocket hSocket, const void* pData, size_t iLen)
{
	const char* p = (const char*)pData;
	size_t iLeft = iLen;

	while ( iLeft > 0u ) {
		int iSent = send(hSocket, p, (int)iLeft, 0);
		if ( iSent <= 0 ) {
			return FALSE;
		}
		p += iSent;
		iLeft -= (size_t)iSent;
	}
	return TRUE;
}

static uint32 xssh_forward_selftest_thread(ptr pArg)
{
	xssh_forward_selftest_client* pClient = (xssh_forward_selftest_client*)pArg;
	struct sockaddr_in addr;
	xsocket hSocket = XSOCKET_INVALID;
	size_t iLen = 0u;

	if ( pClient == NULL ) {
		return 1u;
	}
	xrtSleep(100u);
	if ( !xssh_forward_parse_bind_ipv4(pClient->sHost, pClient->iPort, &addr) ) {
		return 2u;
	}
	hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hSocket == XSOCKET_INVALID ) {
		return 3u;
	}
	if ( connect(hSocket, (const struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
		XSSH_FORWARD_CLOSESOCK(hSocket);
		return 4u;
	}
	if ( !xssh_forward_send_all_socket(hSocket, "forward-self-test", 17u) ) {
		XSSH_FORWARD_CLOSESOCK(hSocket);
		return 5u;
	}
	(void)shutdown(hSocket, XSSH_FORWARD_SHUT_WR);
	while ( iLen < sizeof(pClient->sRecv) - 1u ) {
		int iGot = recv(hSocket, pClient->sRecv + iLen, (int)(sizeof(pClient->sRecv) - 1u - iLen), 0);
		if ( iGot > 0 ) {
			iLen += (size_t)iGot;
			pClient->sRecv[iLen] = '\0';
			if ( strstr(pClient->sRecv, "forward-self-test") != NULL ) {
				pClient->bSuccess = TRUE;
				break;
			}
			continue;
		}
		break;
	}
	XSSH_FORWARD_CLOSESOCK(hSocket);
	return pClient->bSuccess ? 0u : 6u;
}

static int xssh_forward_listen(xssh_forward_pump* pPump, const char* sBindHost, uint16 iBindPort)
{
	struct sockaddr_in addr;
	int iReuse = 1;

	(void)xrtInit();
	if ( pPump == NULL || iBindPort == 0u || !xssh_forward_parse_bind_ipv4(sBindHost, iBindPort, &addr) ) {
		return -1;
	}
	pPump->hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( pPump->hListen == XSOCKET_INVALID ) {
		return -1;
	}
	(void)setsockopt(pPump->hListen, SOL_SOCKET, SO_REUSEADDR, (const char*)&iReuse, sizeof(iReuse));
	if ( bind(pPump->hListen, (const struct sockaddr*)&addr, sizeof(addr)) != 0 ||
		listen(pPump->hListen, 1) != 0 ) {
		return -1;
	}
	return 0;
}

static xsocket xssh_forward_accept_timeout(xsocket hListen, uint32 iTimeoutMs)
{
	fd_set rfds;
	struct timeval tv;
	struct timeval* pTv = NULL;
	int iReady;

	FD_ZERO(&rfds);
	FD_SET(hListen, &rfds);
	if ( iTimeoutMs != 0u ) {
		tv.tv_sec = (long)(iTimeoutMs / 1000u);
		tv.tv_usec = (long)((iTimeoutMs % 1000u) * 1000u);
		pTv = &tv;
	}
	iReady = select((int)(hListen + 1), &rfds, NULL, NULL, pTv);
	if ( iReady <= 0 || !FD_ISSET(hListen, &rfds) ) {
		return XSOCKET_INVALID;
	}
	return accept(hListen, NULL, NULL);
}

static void xssh_forward_peer_text(xsocket hLocal, char* sHost, size_t iHostCap, uint16* pPort)
{
	struct sockaddr_in addr;
	socklen_t iLen = (socklen_t)sizeof(addr);

	if ( sHost != NULL && iHostCap != 0u ) {
		snprintf(sHost, iHostCap, "%s", "127.0.0.1");
	}
	if ( pPort != NULL ) {
		*pPort = 0u;
	}
	if ( getpeername(hLocal, (struct sockaddr*)&addr, &iLen) == 0 && addr.sin_family == AF_INET ) {
		const char* sAddr = inet_ntoa(addr.sin_addr);
		if ( sHost != NULL && iHostCap != 0u && sAddr != NULL ) {
			snprintf(sHost, iHostCap, "%s", sAddr);
		}
		if ( pPort != NULL ) {
			*pPort = ntohs(addr.sin_port);
		}
	}
}

static bool xssh_forward_socket_readable(xsocket hSocket)
{
	fd_set rfds;
	struct timeval tv;
	int iReady;

	FD_ZERO(&rfds);
	FD_SET(hSocket, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	iReady = select((int)(hSocket + 1), &rfds, NULL, NULL, &tv);
	return (iReady > 0 && FD_ISSET(hSocket, &rfds)) ? TRUE : FALSE;
}

static bool xssh_forward_socket_writable(xsocket hSocket)
{
	fd_set wfds;
	struct timeval tv;
	int iReady;

	FD_ZERO(&wfds);
	FD_SET(hSocket, &wfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	iReady = select((int)(hSocket + 1), NULL, &wfds, NULL, &tv);
	return (iReady > 0 && FD_ISSET(hSocket, &wfds)) ? TRUE : FALSE;
}

static int xssh_forward_send_local(xssh_forward_pump* pPump, bool* pProgress)
{
	while ( pPump->iDownPos < pPump->iDownLen ) {
		int iSent;

		if ( !xssh_forward_socket_writable(pPump->hLocal) ) {
			return 0;
		}
		iSent = send(pPump->hLocal, (const char*)pPump->arrDown + pPump->iDownPos,
			(int)(pPump->iDownLen - pPump->iDownPos), 0);
		if ( iSent > 0 ) {
			pPump->iDownPos += (size_t)iSent;
			pPump->iRemoteToLocal += (size_t)iSent;
			*pProgress = TRUE;
			continue;
		}
		if ( iSent < 0 && xssh_forward_would_block(xssh_forward_last_error()) ) {
			return 0;
		}
		return -1;
	}
	pPump->iDownPos = 0u;
	pPump->iDownLen = 0u;
	return 0;
}

static int xssh_forward_send_channel(xssh_channel_t* pChannel, xssh_forward_pump* pPump, bool* pProgress)
{
	while ( pPump->iUpPos < pPump->iUpLen ) {
		size_t iLeft = pPump->iUpLen - pPump->iUpPos;
		size_t iChunk = (iLeft > 1024u) ? 1024u : iLeft;
		size_t iWritten = 0u;
		int iRet = xsshChannelWrite(pChannel, pPump->arrUp + pPump->iUpPos, iChunk, &iWritten);

		if ( iRet == XSSH_ERR_NO_SPACE ) {
			return 0;
		}
		if ( iRet != XSSH_OK || iWritten == 0u ) {
			return -1;
		}
		pPump->iUpPos += iWritten;
		pPump->iLocalToRemote += iWritten;
		*pProgress = TRUE;
	}
	pPump->iUpPos = 0u;
	pPump->iUpLen = 0u;
	return 0;
}

static int xssh_forward_pump_once(xssh_session_t* pSession, xssh_channel_t* pChannel, xssh_forward_pump* pPump, uint32 iIdleTimeoutMs)
{
	double fLastProgress = xrtTimer();

	for (;;) {
		bool bProgress = FALSE;
		xsshevent ev;
		size_t iRead = 0u;
		int iRet;

		iRet = xsshPoll(pSession, 0u);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		for (;;) {
			iRet = xsshNextEvent(pSession, &ev);
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			if ( ev.iType == XSSH_EVENT_NONE ) {
				break;
			}
			if ( ev.pChannel == pChannel && ev.iType == XSSH_EVENT_CHANNEL_EOF ) {
				if ( pPump->bIgnoreNextLocalEofEvent ) {
					pPump->bIgnoreNextLocalEofEvent = FALSE;
				} else {
					pPump->bRemoteEof = TRUE;
				}
				bProgress = TRUE;
			}
			if ( ev.pChannel == pChannel && ev.iType == XSSH_EVENT_CHANNEL_CLOSE ) {
				pPump->bRemoteClosed = TRUE;
				bProgress = TRUE;
			}
		}

		if ( pPump->iUpLen != 0u && xssh_forward_send_channel(pChannel, pPump, &bProgress) != 0 ) {
			return XSSH_ERR_IO;
		}
		if ( pPump->iDownLen != 0u && xssh_forward_send_local(pPump, &bProgress) != 0 ) {
			return XSSH_ERR_IO;
		}
		if ( pPump->iDownLen == 0u ) {
			iRet = xsshChannelRead(pChannel, pPump->arrDown, sizeof(pPump->arrDown), &iRead);
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			if ( iRead != 0u ) {
				pPump->iDownLen = iRead;
				pPump->iDownPos = 0u;
				if ( xssh_forward_send_local(pPump, &bProgress) != 0 ) {
					return XSSH_ERR_IO;
				}
			}
		}
		if ( !pPump->bLocalEof && pPump->iUpLen == 0u && xssh_forward_socket_readable(pPump->hLocal) ) {
			int iGot = recv(pPump->hLocal, (char*)pPump->arrUp, (int)sizeof(pPump->arrUp), 0);

			if ( iGot > 0 ) {
				pPump->iUpLen = (size_t)iGot;
				pPump->iUpPos = 0u;
				bProgress = TRUE;
				if ( xssh_forward_send_channel(pChannel, pPump, &bProgress) != 0 ) {
					return XSSH_ERR_IO;
				}
			} else if ( iGot == 0 ) {
				pPump->bLocalEof = TRUE;
				bProgress = TRUE;
			} else if ( !xssh_forward_would_block(xssh_forward_last_error()) ) {
				return XSSH_ERR_IO;
			}
		}
		if ( pPump->bLocalEof && !pPump->bSentChannelEof && pPump->iUpLen == 0u ) {
			iRet = xsshChannelSendEof(pChannel);
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			pPump->bSentChannelEof = TRUE;
			pPump->bIgnoreNextLocalEofEvent = TRUE;
			bProgress = TRUE;
		}
		if ( (pPump->bRemoteClosed || pPump->bRemoteEof) && pPump->iDownLen == 0u ) {
			return XSSH_OK;
		}
		if ( bProgress ) {
			fLastProgress = xrtTimer();
		} else {
			if ( iIdleTimeoutMs != 0u && (xrtTimer() - fLastProgress) * 1000.0 >= (double)iIdleTimeoutMs ) {
				return XSSH_OK;
			}
			xrtSleep(5u);
		}
	}
}

static int xssh_forward_live_once(xssh_session_t* pSession, const xsshforward* pFwd, uint32 iAcceptTimeoutMs, uint32 iIdleTimeoutMs)
{
	xssh_forward_pump pump;
	xssh_channel_t* pChannel = NULL;
	char sOriginHost[64];
	uint16 iOriginPort = 0u;
	int iRet = XSSH_ERR_IO;

	xssh_forward_pump_init(&pump);
	if ( xssh_forward_listen(&pump, pFwd->sBindHost, pFwd->iBindPort) != 0 ) {
		fprintf(stderr, "listen failed: %s:%u\n", pFwd->sBindHost, (unsigned)pFwd->iBindPort);
		goto cleanup;
	}
	printf("local forward listening on %s:%u -> %s:%u\n",
		pFwd->sBindHost, (unsigned)pFwd->iBindPort, pFwd->sTargetHost, (unsigned)pFwd->iTargetPort);
	pump.hLocal = xssh_forward_accept_timeout(pump.hListen, iAcceptTimeoutMs);
	if ( pump.hLocal == XSOCKET_INVALID ) {
		fprintf(stderr, "accept timeout or failed\n");
		goto cleanup;
	}
	(void)xssh_forward_set_nonblock(pump.hLocal, TRUE);
	xssh_forward_peer_text(pump.hLocal, sOriginHost, sizeof(sOriginHost), &iOriginPort);
	iRet = xsshOpenDirectTcpipChannel(pSession, &pChannel, pFwd->sTargetHost, pFwd->iTargetPort, sOriginHost, iOriginPort);
	if ( iRet != XSSH_OK || pChannel == NULL ) {
		fprintf(stderr, "direct-tcpip open failed: %d\n", iRet);
		goto cleanup;
	}
	iRet = xssh_forward_pump_once(pSession, pChannel, &pump, iIdleTimeoutMs);
	if ( pChannel != NULL ) {
		(void)xsshChannelClose(pChannel);
	}
	if ( iRet == XSSH_OK ) {
		printf("local forward closed local_to_remote=%zu remote_to_local=%zu\n", pump.iLocalToRemote, pump.iRemoteToLocal);
	}

cleanup:
	if ( pChannel != NULL && !pChannel->bClosed ) {
		(void)xsshChannelClose(pChannel);
	}
	xssh_forward_pump_close(&pump);
	return iRet;
}

int main(int argc, char** argv)
{
	xsshconfig cfg;
	xsshauth auth;
	xssh_session_t* pSession = NULL;
	xsshforward fwd;
	const char* sPasswordEnv = NULL;
	bool bMock = FALSE;
	bool bSelfTest = FALSE;
	int iFrames = 1;
	int i;
	uint32 iAcceptTimeoutMs = 0u;
	uint32 iIdleTimeoutMs = 30000u;

	xsshConfigInit(&cfg);
	xsshAuthInit(&auth);
	memset(&fwd, 0, sizeof(fwd));
	cfg.sUser = "user";
	fwd.sBindHost = "127.0.0.1";

	for ( i = 1; i < argc; ++i ) {
		if ( strcmp(argv[i], "--help") == 0 ) {
			xssh_forward_help();
			return 0;
		} else if ( strcmp(argv[i], "--mock") == 0 ) {
			bMock = TRUE;
		} else if ( strcmp(argv[i], "--self-test") == 0 ) {
			bSelfTest = TRUE;
		} else if ( strcmp(argv[i], "--host") == 0 ) {
			cfg.sHost = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--port") == 0 ) {
			const char* sPort = xssh_forward_arg_value(&i, argc, argv);
			cfg.iPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--user") == 0 ) {
			cfg.sUser = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--password-env") == 0 ) {
			sPasswordEnv = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--identity") == 0 ) {
			auth.sPrivateKeyPath = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--known-hosts") == 0 ) {
			cfg.sKnownHostsPath = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--accept-new") == 0 ) {
			cfg.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
		} else if ( strcmp(argv[i], "--bind-host") == 0 ) {
			fwd.sBindHost = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--bind-port") == 0 ) {
			const char* sPort = xssh_forward_arg_value(&i, argc, argv);
			fwd.iBindPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--target-host") == 0 ) {
			fwd.sTargetHost = xssh_forward_arg_value(&i, argc, argv);
		} else if ( strcmp(argv[i], "--target-port") == 0 ) {
			const char* sPort = xssh_forward_arg_value(&i, argc, argv);
			fwd.iTargetPort = sPort ? (uint16)atoi(sPort) : 0u;
		} else if ( strcmp(argv[i], "--frames") == 0 ) {
			const char* sFrames = xssh_forward_arg_value(&i, argc, argv);
			iFrames = sFrames ? atoi(sFrames) : 0;
		} else if ( strcmp(argv[i], "--accept-timeout-ms") == 0 ) {
			const char* sTimeout = xssh_forward_arg_value(&i, argc, argv);
			iAcceptTimeoutMs = sTimeout ? (uint32)strtoul(sTimeout, NULL, 10) : 0u;
		} else if ( strcmp(argv[i], "--idle-timeout-ms") == 0 ) {
			const char* sTimeout = xssh_forward_arg_value(&i, argc, argv);
			iIdleTimeoutMs = sTimeout ? (uint32)strtoul(sTimeout, NULL, 10) : 0u;
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
	if ( cfg.sHost == NULL || fwd.iBindPort == 0u || fwd.sTargetHost == NULL || fwd.iTargetPort == 0u ) {
		xssh_forward_help();
		return 2;
	}
	{
		int iRet = xsshConnect(&cfg, &auth, &pSession);
		if ( iRet != XSSH_OK ) {
			xsshExamplePrintSessionResult(stderr, "connect failed", pSession, iRet);
			if ( pSession != NULL ) {
				xsshFree(pSession);
			}
			return 2;
		}
	}
	if ( bMock ) {
		xssh_channel_t* pChannel = NULL;

		if ( bSelfTest ) {
			xssh_forward_selftest_client client;
			xthread pThread;
			int iRet;

			memset(&client, 0, sizeof(client));
			client.sHost = fwd.sBindHost;
			client.iPort = fwd.iBindPort;
			pThread = xrtThreadCreate((ptr)xssh_forward_selftest_thread, &client, 0u);
			if ( pThread == NULL ) {
				fprintf(stderr, "self-test client thread failed\n");
				xsshFree(pSession);
				return 2;
			}
			iRet = xssh_forward_live_once(pSession, &fwd, 3000u, 3000u);
			xrtThreadWait(pThread);
			if ( iRet != XSSH_OK ) {
				xsshExamplePrintSessionResult(stderr, "self-test forward failed", pSession, iRet);
				xsshFree(pSession);
				return 2;
			}
			if ( !client.bSuccess ) {
				fprintf(stderr, "self-test failed recv=%s\n", client.sRecv);
				xsshFree(pSession);
				return 2;
			}
			printf("mock forward self-test: PASS recv=%s\n", client.sRecv);
			xsshDisconnect(pSession);
			xsshFree(pSession);
			return 0;
		}
		{
			int iRet = xsshOpenDirectTcpipChannel(pSession, &pChannel, fwd.sTargetHost, fwd.iTargetPort, fwd.sBindHost, fwd.iBindPort);
			if ( iRet != XSSH_OK ) {
				xsshExamplePrintSessionResult(stderr, "mock direct-tcpip setup failed", pSession, iRet);
				xsshFree(pSession);
				return 2;
			}
		}
		printf("mock forward %s:%u -> %s:%u bound=%u frames=%d\n",
			fwd.sBindHost, (unsigned)fwd.iBindPort, fwd.sTargetHost, (unsigned)fwd.iTargetPort, (unsigned)fwd.iBindPort, iFrames);
		if ( pChannel != NULL ) {
			xsshChannelClose(pChannel);
		}
	} else {
		int iRet = xssh_forward_live_once(pSession, &fwd, iAcceptTimeoutMs, iIdleTimeoutMs);

		if ( iRet != XSSH_OK ) {
			xsshExamplePrintSessionResult(stderr, "forward failed", pSession, iRet);
			xsshDisconnect(pSession);
			xsshFree(pSession);
			return 2;
		}
	}
	xsshDisconnect(pSession);
	xsshFree(pSession);
	return 0;
}
