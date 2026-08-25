#include <xrt/tls_client.h>

#include <stdio.h>



/* 用恢复对象创建一条携带真实 PSK binder 的 TLS 1.3 ClientHello。 */
int main(void)
{
	uint8 Ticket[] = { 0x10, 0x20, 0x30, 0x40 };
	uint8 Secret[32] = { 0 };
	xtlsresumeconfig ResumeConfig;
	xtlsclientconfig ClientConfig;
	xtlsresume* pResume;
	xtlssession* pSession;

	xrtTlsResumeConfigInit(&ResumeConfig);
	ResumeConfig.Cipher = XTLS_AES_128_GCM_SHA256;
	ResumeConfig.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	ResumeConfig.Secret = (xbytesview) { Secret, sizeof(Secret) };
	ResumeConfig.ServerName = XRT_STR_LITERAL("example.com");
	ResumeConfig.Protocol = XRT_BYTES_LITERAL("h2");
	ResumeConfig.Lifetime = 3600u;
	pResume = xrtTlsResumeCreate(&ResumeConfig);
	if ( pResume == NULL ) {
		return 1;
	}

	/* 省略 SNI 与 ALPN 时，客户端精确继承恢复对象的路由绑定。 */
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Resume = pResume;
	ClientConfig.ResumeOnly = true;
	pSession = xrtTlsClientCreate(&ClientConfig, NULL);
	xrtTlsResumeRelease(pResume);
	if ( pSession == NULL ) {
		return 2;
	}

	printf(
		"client hello=%zu bytes, accepted=%s\n",
		xrtTlsSessionSendSize(pSession),
		xrtTlsClientResumed(pSession) ? "yes" : "not yet"
	);
	xrtTlsSessionDestroy(pSession);
	return 0;
}
