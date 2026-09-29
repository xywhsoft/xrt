#include <xsmtp.h>
#include "../../../xmail/examples/mail_client_setup.h"



/* 使用消息描述自动建立 envelope，并直接流式提交 MIME 内容。 */
bool submitMessage(xsmtpclient* pClient, xdeadline iDeadline,
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
	return xrtSmtpSubmit(pClient, &Message, iDeadline, NULL);
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
	xdeadline deadline;
	bool ok;
	if ( argc == 1 ) {
		puts("usage: submit host port ca.pem from to subject body [tls|starttls] (set XSMTP_USER and XSMTP_PASSWORD)");
		return 0;
	}
	if ( (argc != 8 && argc != 9) || user == NULL || secret == NULL ||
		!mailExamplePort(argv[2], &port) ||
		(argc == 9 && strcmp(argv[8], "tls") != 0 &&
		 strcmp(argv[8], "starttls") != 0) ) return 2;
	if ( !mailExampleNetInit(&Net, argv[3]) ) return 1;
	deadline = xrtDeadlineAfter(UINT64_C(30000000));
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
	client = xrtSmtpClientOpen(&Config, deadline, NULL);
	if ( client == NULL ) {
		mailExampleNetUnit(&Net);
		return 1;
	}
	xrtSmtpAuthConfigInit(&Auth);
	Auth.Method = XSMTP_AUTH_PLAIN;
	Auth.Username = xrtStrView(user);
	Auth.Secret = xrtStrView(secret);
	ok = xrtSmtpClientAuth(client, &Auth, deadline, NULL) &&
		submitMessage(client, deadline, argv[4], argv[5], argv[6], argv[7]) &&
		xrtSmtpClientQuit(client, deadline, NULL);
	xrtSmtpClientDestroy(client);
	mailExampleNetUnit(&Net);
	return ok ? 0 : 1;
}
