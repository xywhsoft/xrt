#include <xrt/tls_client.h>

#include <stdio.h>



/*
 * 范例：tls/client_resume —— 用恢复对象构造 0-RTT ClientHello
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsClientConfigInit + Config.Resume / ResumeOnly
 *   xrtTlsClientCreate       由配置创建客户端会话
 *   xrtTlsSessionSendSize    待发握手字节数（驱动传输层）
 *   xrtTlsClientResumed      恢复是否已被服务端接受
 * 模块宏：XRT_MODULE_TLS_CLIENT
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/client_resume/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   client hello=289 bytes, accepted=not yet
 *
 * accepted=not yet 是关键语义：PSK binder 已写入
 *   ClientHello（证明持有秘密），但"服务端是否接受恢复"
 *   要等 ServerHello 才知道——Resumed 在握手完成前返回 false。
 *   省略 SNI/ALPN 时客户端精确继承恢复对象的路由绑定
 *   （绑定错位 = 恢复失败甚至安全风险，API 层防住）。
 */


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
