#include <xmail.h>



/* 演示调用方共享 Engine、Resolver，并用一条截止时间完成 SMTP 事务。 */
bool submitMessage(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	xtlscontext* pTls,
	xtlsverifier* pVerifier,
	cstr sHost,
	uint16 iPort
)
{
	xsmtpclientconfig ClientConfig;
	xsmtpauthconfig AuthConfig;
	xsmtpclient* pClient;
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	bool bSuccess;

	xrtSmtpClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = sHost;
	ClientConfig.Net.Port = iPort;
	ClientConfig.Net.Security = XMAIL_SECURITY_STARTTLS;
	ClientConfig.Net.Tls.Context = pTls;
	ClientConfig.Net.Tls.Verifier = pVerifier;
	ClientConfig.Hello = (xstrview)XRT_STR_LITERAL("client.example");
	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	if ( pClient == NULL ) {
		return false;
	}
	xrtSmtpAuthConfigInit(&AuthConfig);
	AuthConfig.Username = (xstrview)XRT_STR_LITERAL("user@example.com");
	AuthConfig.Secret = (xstrview)XRT_STR_LITERAL("application-password");
	bSuccess = xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	) && xrtSmtpClientMail(
		pClient,
		XRT_STR_LITERAL("sender@example.com"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	) && xrtSmtpClientRcpt(
		pClient,
		XRT_STR_LITERAL("target@example.net"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	) && xrtSmtpClientData(
		pClient,
		XRT_STR_LITERAL(
			"From: sender@example.com\r\n"
			"To: target@example.net\r\n"
			"Subject: xmail\r\n"
			"\r\n"
			"message body\r\n"
		),
		Deadline,
		NULL
	);
	if ( bSuccess ) {
		bSuccess = xrtSmtpClientQuit(pClient, Deadline, NULL);
	}
	xrtSmtpClientDestroy(pClient);
	return bSuccess;
}



/* 示例由宿主提供已经启动的网络对象和真实 TLS 配置。 */
int main(void)
{
	return 0;
}
