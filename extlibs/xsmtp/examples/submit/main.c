#include <xsmtp.h>
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



/* 使用消息描述自动建立 envelope，并直接流式提交 MIME 内容。 */
bool submitMessage(xsmtpclient* pClient, double iDeadline,
	cstr from, cstr to, cstr subject, cstr body)
{
	xmailmessage Message;
	xmailaddress To;

	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL(""),
		xrtStrView(from)
	};
	To = (xmailaddress){
		XRT_STR_LITERAL(""),
		xrtStrView(to)
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = xrtStrView(subject);
	Message.Text = xrtStrView(body);
	return xrtSmtpSubmit(pClient, &Message,exampleTimerRemaining(iDeadline), NULL);
}



/* 设置 XSMTP_USER/XSMTP_PASSWORD 后传入真实服务参数；不在命令行放密码。 */
int main(int argc, char** argv)
{
	mail_example_net Net;
	xsmtpclientconfig Config;
	xsmtpauthconfig Auth;
	xsmtpclient* client;
	const char* user = getenv("XSMTP_USER");
	const char* secret = getenv("XSMTP_PASSWORD");
	uint16 port;
	double deadline;
	bool ok;
	if ( argc == 1 ) {
		puts("usage: submit host port ca.pem from to subject body [tls|starttls] (set XSMTP_USER and XSMTP_PASSWORD)");
		return 0;
	}
	if ( (argc != 8 && argc != 9) || user == NULL || secret == NULL ||
		!mailExamplePort(argv[2], &port) ||
		(argc == 9 && strcmp(argv[8], "tls") != 0 &&
		 strcmp(argv[8], "starttls") != 0) ) return 2;
	if ( !mailExampleNetInit(&Net, argv[3]) ) {
		mailExampleDiagnostic("SMTP network initialization");
		return 1;
	}
	deadline = exampleTimerLimit(INT64_C(30000));
	xrtSmtpClientConfigInit(&Config);
	Config.Net.Engine = Net.Engine;
	Config.Net.Resolver = Net.Resolver;
	Config.Net.Host = argv[1];
	Config.Net.Port = port;
	Config.Net.Security = argc == 9 && strcmp(argv[8], "tls") == 0 ?
		XMAIL_SECURITY_TLS : XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = Net.Tls;
	Config.Net.Tls.Verifier = Net.Verifier;
	Config.Hello = (xstrview)XRT_STR_LITERAL("localhost");
	client = xrtSmtpClientOpen(&Config,exampleTimerRemaining(deadline), NULL);
	if ( client == NULL ) {
		mailExampleDiagnostic("SMTP open");
		(void)mailExampleNetUnit(&Net);
		return 1;
	}
	xrtSmtpAuthConfigInit(&Auth);
	Auth.Method = XSMTP_AUTH_PLAIN;
	Auth.Username = xrtStrView(user);
	Auth.Secret = xrtStrView(secret);
	ok = xrtSmtpClientAuth(client, &Auth,exampleTimerRemaining(deadline), NULL);
	if ( !ok ) mailExampleDiagnostic("SMTP authentication");
	if ( ok ) {
		ok = submitMessage(client, deadline, argv[4], argv[5], argv[6], argv[7]);
		if ( !ok ) mailExampleDiagnostic("SMTP submission");
	}
	if ( ok ) {
		/* DATA's positive completion commits the submission. A later QUIT or
		 * TLS shutdown failure must not report it as an unsent message. */
		if ( !xrtSmtpClientQuit(client,exampleTimerRemaining(deadline), NULL) )
			mailExampleDiagnostic("SMTP submission completed; shutdown");
	}
	xrtSmtpClientDestroy(client);
	if ( !mailExampleNetUnit(&Net) ) ok = false;
	return ok ? 0 : 1;
}
