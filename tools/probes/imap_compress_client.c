/* Public-API client for the independent zlib interoperability runner. */
#define XIMAP_MODULE_IMAP_CLIENT_TLS
#define XIMAP_MODULE_IMAP_AUTH
#define XIMAP_MODULE_IMAP_COMMAND
#define XIMAP_MODULE_IMAP_APPEND
#define XIMAP_MODULE_IMAP_COMPRESS
#define XRT_MODULE_PEM
#define XRT_MODULE_X509_STORE
#if defined(XRT_TEST_IMAP_SINGLE)
	#define XIMAP_IMPLEMENTATION
	#include "../../extlibs/ximap/include/ximap/features.h"
	#include "../../extlibs/xmail/include/xmail/features.h"
	#ifndef XRT_IMPLEMENTATION
	#define XRT_IMPLEMENTATION
	#endif
	#include "../../single/xrt.h"
	#define XMAIL_IMPLEMENTATION
	#include "../../single/extlibs/xmail.h"
	#include "../../single/extlibs/ximap.h"
#else
	#include <ximap.h>
#endif
#include "../../extlibs/xmail/examples/mail_client_setup.h"

static bool probeRequire(bool Result, const char* Message)
{
	if ( !Result ) fprintf(stderr, "[FAIL] %s; error kind=%d\n", Message,
		(int)xrtErrorKind(xrtGetError()));
	return Result;
}

static bool probeText(xstrview Text, const char* Expected)
{
	size_t Length = strlen(Expected);
	return Text.Size == Length && memcmp(Text.Data, Expected, Length) == 0;
}

static void probePayload(char* Data, size_t Size)
{
	uint32 Value = UINT32_C(0x7351a2b9);
	for ( size_t i = 0; i < Size; ++i ) {
		Value ^= Value << 13; Value ^= Value >> 17; Value ^= Value << 5;
		Data[i] = (char)(32u + Value % 95u);
	}
}

int main(int argc, char** argv)
{
	mail_example_net Net;
	ximapclientconfig Config;
	ximapauthconfig Auth;
	ximapcompressconfig Compress;
	ximapresponseview Last;
	ximapclient* Client = NULL;
	uint16 Port;
	bool Ok = false;
	if ( argc != 6 || !mailExamplePort(argv[2], &Port) ) return 2;
	const char* Mode = argv[4];
	const char* Scenario = argv[5];
	if ( strcmp(Mode, "plain") && strcmp(Mode, "tls") && strcmp(Mode, "starttls") ) return 2;
	if ( !mailExampleNetInit(&Net, argv[3]) ) return 1;
	int64 Timeout = INT64_C(20000);
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = Net.Engine; Config.Net.Resolver = Net.Resolver;
	Config.Net.Host = argv[1]; Config.Net.Port = Port; Config.Net.LineLimit = 1024u;
	Config.Net.Security = strcmp(Mode, "plain") == 0 ? XMAIL_SECURITY_PLAIN :
		strcmp(Mode, "tls") == 0 ? XMAIL_SECURITY_TLS : XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = Net.Tls; Config.Net.Tls.Verifier = Net.Verifier;
	if ( strcmp(Scenario, "close-stall") == 0 )
		Config.Net.TlsStream.CloseTimeout = INT64_C(200);
	Client = xrtImapClientOpen(&Config, Timeout, NULL);
	if ( !probeRequire(Client != NULL, "open") ) goto done;
	if ( !probeRequire(xrtImapClientSecurity(Client) ==
		(strcmp(Mode, "plain") == 0 ? XMAIL_SECURITY_PLAIN : XMAIL_SECURITY_TLS),
		"transport security") ) goto done;
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_PLAIN;
	/* Only the local plaintext fixture opts in to unencrypted test credentials. */
	Auth.AllowPlaintext = strcmp(Mode, "plain") == 0;
	Auth.Username = XRT_STR_LITERAL("demo"); Auth.Secret = XRT_STR_LITERAL("secret");
	if ( !probeRequire(xrtImapClientAuth(Client, &Auth, Timeout, NULL) &&
		(xrtImapClientCapabilities(Client) & XIMAP_CAP_COMPRESS_DEFLATE) != 0u,
		"authentication and advertised compression") ) goto done;
	xrtImapCompressConfigInit(&Compress);
	bool Negotiated = xrtImapClientCompress(Client, &Compress, Timeout, NULL);
	bool Rejected = strncmp(Scenario, "reject-", 7u) == 0;
	if ( Rejected ) {
		if ( !probeRequire(!Negotiated && !xrtImapClientCompressed(Client) &&
			xrtImapClientState(Client) == XIMAP_CLIENT_AUTHENTICATED &&
			xrtImapClientLastResponse(Client, &Last) &&
			Last.Status == (strcmp(Scenario, "reject-no") == 0 ? XIMAP_STATUS_NO : XIMAP_STATUS_BAD) &&
			probeText(Last.Text, strcmp(Scenario, "reject-no") == 0 ?
				"[COMPRESSIONACTIVE] compression refused" : "compression refused"),
			"rejection must preserve plain session") ) goto done;
		xrtClearError();
	} else if ( !probeRequire(Negotiated && xrtImapClientCompressed(Client), "COMPRESS negotiation") ) goto done;
	bool Fault = strcmp(Scenario, "invalid") == 0 || strcmp(Scenario, "wrapped") == 0 ||
		strcmp(Scenario, "truncated") == 0 || strcmp(Scenario, "line-limit") == 0;
	bool Noop = xrtImapClientNoop(Client, Timeout, NULL);
	if ( Fault ) {
		xerrkind Kind = xrtErrorKind(xrtGetError());
		bool ErrorOk = strcmp(Scenario, "line-limit") == 0 ? Kind == XERR_RANGE :
			strcmp(Scenario, "truncated") == 0 ?
				(Kind == XERR_CLOSED || Kind == XERR_IO || Kind == XERR_PROTOCOL) : Kind == XERR_PROTOCOL;
		if ( !probeRequire(!Noop && ErrorOk && xrtImapClientState(Client) == XIMAP_CLIENT_FAILED &&
			xrtImapClientLastResponse(Client, &Last) && Last.Status == XIMAP_STATUS_OK &&
			probeText(Last.Text, "compression active"), "compressed read failure must preserve original diagnostics") ) goto done;
		xrtClearError();
		Ok = probeRequire(!xrtImapClientNoop(Client, Timeout, NULL) &&
			xrtErrorKind(xrtGetError()) == XERR_STATE, "failed session must reject another command");
		xrtClearError();
		goto done;
	}
	if ( !probeRequire(Noop, "first command after COMPRESS") ) goto done;
	ximapmailboxinfo Mailbox;
	xrtImapMailboxInfoInit(&Mailbox);
	if ( !probeRequire(xrtImapClientExamine(Client, XRT_STR_LITERAL("INBOX"), &Mailbox, Timeout, NULL) &&
		Mailbox.Exists == 2u && Mailbox.ReadOnly, "compressed mailbox summary") ) goto done;
	char Body[40000];
	probePayload(Body, sizeof(Body));
	ximapappendconfig Append;
	xrtImapAppendConfigInit(&Append);
	Append.Mailbox = XRT_STR_LITERAL("INBOX"); Append.Size = sizeof(Body); Append.Literal = XIMAP_LITERAL_SYNC;
	if ( !probeRequire(xrtImapClientAppendBegin(Client, &Append, Timeout, NULL) &&
		xrtImapClientAppendWrite(Client, Body, 7u, Timeout, NULL), "start streamed literal") ) goto done;
	xrtClearError();
	if ( !probeRequire(!xrtImapClientAppendWrite(Client, NULL, 0u, Timeout, NULL) &&
		xrtErrorKind(xrtGetError()) == XERR_STATE &&
		xrtImapClientAppendRemaining(Client) == sizeof(Body) - 7u &&
		xrtImapClientState(Client) == XIMAP_CLIENT_SELECTED, "empty chunk rejection must preserve literal state") ) goto done;
	xrtClearError();
	if ( !probeRequire(
		xrtImapClientAppendWrite(Client, Body + 7u, 32770u, Timeout, NULL) &&
		xrtImapClientAppendWrite(Client, Body + 32777u, 7223u, Timeout, NULL) &&
		xrtImapClientAppendRemaining(Client) == 0u &&
		xrtImapClientAppendEnd(Client, NULL, Timeout, NULL), "streamed literal after rejected empty chunk") ) goto done;
	for ( unsigned i = 0; i < 3u; ++i )
		if ( !probeRequire(xrtImapClientNoop(Client, Timeout, NULL), "continued DEFLATE stream") ) goto done;
	bool Logout = xrtImapClientLogout(Client, Timeout, NULL);
	if ( strcmp(Scenario, "close-truncated") == 0 || strcmp(Scenario, "close-stall") == 0 ) {
		xerrkind Kind = xrtErrorKind(xrtGetError());
		bool ErrorOk = strcmp(Scenario, "close-stall") == 0 ? Kind == XERR_TIMEOUT :
			(Kind == XERR_PROTOCOL || Kind == XERR_IO);
		if ( !probeRequire(!Logout && ErrorOk && xrtImapClientState(Client) == XIMAP_CLIENT_FAILED &&
			xrtImapClientLastResponse(Client, &Last) && Last.Status == XIMAP_STATUS_OK &&
			probeText(Last.Text, "logout complete"), "close failure must preserve final tagged OK") ) goto done;
		xrtClearError();
		Ok = probeRequire(!xrtImapClientNoop(Client, Timeout, NULL) &&
			xrtErrorKind(xrtGetError()) == XERR_STATE, "close failure must reject another command");
		xrtClearError();
		goto done;
	}
	Ok = probeRequire(Logout &&
		xrtImapClientState(Client) == XIMAP_CLIENT_CLOSED &&
		xrtImapClientLastResponse(Client, &Last) && Last.Status == XIMAP_STATUS_OK &&
		probeText(Last.Text, "logout complete"), "logout completion");
done:
	xrtImapClientDestroy(Client);
	mailExampleNetUnit(&Net);
	if ( Ok ) printf("[PASS] independent IMAP %s/%s\n", Mode, Scenario);
	return Ok ? 0 : 1;
}
