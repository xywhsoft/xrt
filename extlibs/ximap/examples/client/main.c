#include <ximap.h>
#include "../../../xmail/examples/mail_client_setup.h"

#include <math.h>
static inline double exampleTimerLimit(int64 Timeout)
{
    return Timeout == XRT_WAIT_FOREVER ? INFINITY : xrtTimer() + (double)Timeout / 1000.0;
}
static inline bool exampleTimerExpired(double Limit)
{
    return xrtTimer() >= Limit;
}
static inline int64 exampleTimerRemaining(double Limit)
{
    double Ms;
    if (Limit == INFINITY) return XRT_WAIT_FOREVER;
    Ms = ceil((Limit - xrtTimer()) * 1000.0);
    return Ms <= 0 ? 0 : Ms >= 0x1p63 ? INT64_MAX : (int64)Ms;
}



/* 设置 XIMAP_USER/XIMAP_PASSWORD，连接后认证、可选压缩并只读打开邮箱。 */
int main(int argc, char** argv)
{
	mail_example_net Net;
	ximapclientconfig Config;
	ximapauthconfig Auth;
	ximapcompressconfig Compress;
	ximapmailboxinfo Mailbox;
	ximapclient* client;
	const char* user = getenv("XIMAP_USER");
	const char* secret = getenv("XIMAP_PASSWORD");
	uint16 port;
	double deadline;
	bool ok;
	if ( argc == 1 ) {
		puts("usage: client host port ca.pem mailbox [tls|starttls] (set XIMAP_USER and XIMAP_PASSWORD)");
		return 0;
	}
	if ( (argc != 5 && argc != 6) || user == NULL || secret == NULL ||
		!mailExamplePort(argv[2], &port) ||
		(argc == 6 && strcmp(argv[5], "tls") != 0 &&
		 strcmp(argv[5], "starttls") != 0) ) return 2;
	if ( !mailExampleNetInit(&Net, argv[3]) ) {
		mailExampleDiagnostic("IMAP network initialization");
		return 1;
	}
	deadline = exampleTimerLimit(INT64_C(30000));
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = Net.Engine;
	Config.Net.Resolver = Net.Resolver;
	Config.Net.Host = argv[1];
	Config.Net.Port = port;
	Config.Net.Security = argc == 6 && strcmp(argv[5], "tls") == 0 ?
		XMAIL_SECURITY_TLS : XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = Net.Tls;
	Config.Net.Tls.Verifier = Net.Verifier;
	client = xrtImapClientOpen(&Config,exampleTimerRemaining(deadline), NULL);
	if ( client == NULL ) {
		mailExampleDiagnostic("IMAP open");
		(void)mailExampleNetUnit(&Net);
		return 1;
	}
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_PLAIN;
	Auth.Username = xrtStrView(user);
	Auth.Secret = xrtStrView(secret);
	ok = xrtImapClientAuth(client, &Auth,exampleTimerRemaining(deadline), NULL);

	xrtImapCompressConfigInit(&Compress);
	if ( ok &&
		(xrtImapClientCapabilities(client) & XIMAP_CAP_COMPRESS_DEFLATE) != 0u ) {
		ok = xrtImapClientCompress(client, &Compress,exampleTimerRemaining(deadline), NULL);
	}
	if ( ok ) {
		xrtImapMailboxInfoInit(&Mailbox);
		ok = xrtImapClientExamine(client, xrtStrView(argv[4]),
			&Mailbox,exampleTimerRemaining(deadline), NULL);
		if ( ok ) printf("%s: %llu messages\n", argv[4],
			(unsigned long long)Mailbox.Exists);
	}
	/* EXAMINE is read-only and its tagged completion already proved the query.
	 * LOGOUT/transport errors remain visible without discarding that result. */
	if ( ok && !xrtImapClientLogout(client,exampleTimerRemaining(deadline), NULL) )
		mailExampleDiagnostic("IMAP query completed; shutdown");
	if ( !ok ) mailExampleDiagnostic("IMAP operation");
	xrtImapClientDestroy(client);
	if ( !mailExampleNetUnit(&Net) ) ok = false;
	return ok ? 0 : 1;
}
