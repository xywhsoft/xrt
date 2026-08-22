#include <xmail.h>



/* 演示在 STLS 会话中认证并逐行读取一封邮件。 */
bool fetchMessage(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	xtlscontext* pTls,
	xtlsverifier* pVerifier,
	cstr sHost,
	uint16 iPort,
	uint64 iMessage
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
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = pTls;
	Config.Net.Tls.Verifier = pVerifier;
	pClient = xrtPop3ClientOpen(&Config, Deadline, NULL);
	if ( pClient == NULL ) {
		return false;
	}
	if ( !xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user@example.com"),
		XRT_STR_LITERAL("application-password"),
		false,
		Deadline,
		NULL
	) || !xrtPop3ClientRetr(
		pClient,
		iMessage,
		Deadline,
		NULL
	) ) {
		xrtPop3ClientDestroy(pClient);
		return false;
	}
	do {
		Next = xrtPop3ClientNext(pClient, &Line, Deadline, NULL);
		if ( Next == XMAIL_NEXT_ITEM ) {
			/* 实际程序在这里把借用行写入文件或增量 MIME 解析器。 */
			(void)Line;
		}
	} while ( Next == XMAIL_NEXT_ITEM );
	bSuccess = (Next == XMAIL_NEXT_END) &&
		xrtPop3ClientQuit(pClient, Deadline, NULL);
	xrtPop3ClientDestroy(pClient);
	return bSuccess;
}



/* 示例由宿主提供共享网络和 TLS 对象。 */
int main(void)
{
	return 0;
}
