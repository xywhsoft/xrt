#include <xpop3.h>
#include "../../../xmail/examples/mail_client_setup.h"
#include <errno.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif



/* 演示在 STLS 会话中认证并逐行读取一封邮件。 */
bool fetchMessage(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	xtlscontext* pTls,
	xtlsverifier* pVerifier,
	cstr sHost,
	uint16 iPort,
	uint64 iMessage,
	xstrview User,
	xstrview Secret,
	bool bImplicitTls
)
{
	xpop3clientconfig Config;
	xpop3client* pClient;
	xstrview Line;
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	xmailnext Next;
	bool bSuccess = false;

	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = sHost;
	Config.Net.Port = iPort;
	Config.Net.Security = bImplicitTls ? XMAIL_SECURITY_TLS :
		XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = pTls;
	Config.Net.Tls.Verifier = pVerifier;
	pClient = xrtPop3ClientOpen(&Config, Deadline, NULL);
	if ( pClient == NULL ) {
		mailExampleDiagnostic("POP3 open");
		return false;
	}
	if ( !xrtPop3ClientLogin(
		pClient,
		User,
		Secret,
		false,
		Deadline,
		NULL
	) || !xrtPop3ClientRetr(
		pClient,
		iMessage,
		Deadline,
		NULL
	) ) {
		mailExampleDiagnostic("POP3 login or retrieval");
		xrtPop3ClientDestroy(pClient);
		return false;
	}
	do {
		Next = xrtPop3ClientNext(pClient, &Line, Deadline, NULL);
		if ( Next == XMAIL_NEXT_ITEM ) {
			if ( (Line.Size != 0u &&
				fwrite(Line.Data, 1u, Line.Size, stdout) != Line.Size) ||
				fwrite("\r\n", 1u, 2u, stdout) != 2u ) {
				Next = XMAIL_NEXT_ERROR;
				break;
			}
		}
	} while ( Next == XMAIL_NEXT_ITEM );
	bSuccess = Next == XMAIL_NEXT_END;
	if ( bSuccess && fflush(stdout) != 0 ) {
		fputs("POP3 output flush failed\n", stderr);
		bSuccess = false;
	}
	/* This example performs no DELE: the complete RETR is already delivered.
	 * Keep a subsequent shutdown diagnostic separate from its result. */
	if ( bSuccess && !xrtPop3ClientQuit(pClient, Deadline, NULL) )
		mailExampleDiagnostic("POP3 retrieval completed; shutdown");
	if ( !bSuccess ) mailExampleDiagnostic("POP3 retrieval or quit");
	xrtPop3ClientDestroy(pClient);
	return bSuccess;
}



/* 用法：设置 XPOP3_USER/XPOP3_PASSWORD 后传入 host port ca.pem message [tls|stls]。 */
int main(int argc, char** argv)
{
	mail_example_net Net;
	const char* user = getenv("XPOP3_USER");
	const char* secret = getenv("XPOP3_PASSWORD");
	uint16 port;
	char* end;
	unsigned long long message;
	bool ok;
	if ( argc == 1 ) {
		puts("usage: client host port ca.pem message [tls|stls] (set XPOP3_USER and XPOP3_PASSWORD)");
		return 0;
	}
	if ( (argc != 5 && argc != 6) || user == NULL || secret == NULL ||
		!mailExamplePort(argv[2], &port) || argv[4][0] == 0 ) return 2;
	errno = 0;
	message = strtoull(argv[4], &end, 10);
	if ( argv[4][0] < '0' || argv[4][0] > '9' || errno == ERANGE ||
		*end != 0 || message == 0 ||
		(argc == 6 && strcmp(argv[5], "tls") != 0 &&
		 strcmp(argv[5], "stls") != 0) ) return 2;
	#ifdef _WIN32
	/* RETR emits exact message bytes; text mode would double each CRLF. */
	if ( _setmode(_fileno(stdout), _O_BINARY) == -1 ) {
		fputs("POP3 binary output initialization failed\n", stderr);
		return 1;
	}
	#endif
	if ( !mailExampleNetInit(&Net, argv[3]) ) {
		mailExampleDiagnostic("POP3 network initialization");
		return 1;
	}
	ok = fetchMessage(Net.Engine, Net.Resolver, Net.Tls, Net.Verifier,
		argv[1], port, (uint64)message,
		xrtStrView(user), xrtStrView(secret),
		argc == 6 && strcmp(argv[5], "tls") == 0);
	if ( !mailExampleNetUnit(&Net) ) ok = false;
	return ok ? 0 : 1;
}
