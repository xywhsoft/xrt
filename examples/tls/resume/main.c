#include <xrt/tls_resume.h>

#include <stdio.h>



/*
 * 范例：tls/resume —— 会话恢复对象：可交给自定义缓存的快照
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsResumeConfigInit / Create / Release / Info
 *   Config：套件 / 票据 / 密钥 / SNI / ALPN / 生命周期
 * 模块宏：XRT_MODULE_TLS_RESUME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/resume/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   ticket=4 bytes, secret=32 bytes, lifetime=3600 seconds
 *
 * 恢复对象 = "再来一次握手"所需的全部状态（票据 + PSK 秘密
 *   + 路由绑定 SNI/ALPN）。它是值对象：应用自己决定缓存
 *   （内存 / 磁盘 / Redis），XRT 不替你存。
 *   Lifetime 供缓存层做过期淘汰。
 */


/* 创建可交给自定义缓存保存的 TLS 1.3 恢复票据快照。 */
int main(void)
{
	uint8 Ticket[] = { 0x10, 0x20, 0x30, 0x40 };
	uint8 Secret[32] = { 0 };
	xtlsresumeconfig Config;
	xtlsresumeinfo Info;
	xtlsresume* pResume;

	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	Config.Secret = (xbytesview) { Secret, sizeof(Secret) };
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocol = XRT_BYTES_LITERAL("h2");
	Config.Lifetime = 3600u;
	pResume = xrtTlsResumeCreate(&Config);
	if ( (pResume == NULL) || !xrtTlsResumeInfo(pResume, &Info) ) {
		xrtTlsResumeRelease(pResume);
		return 1;
	}
	printf(
		"ticket=%zu bytes, secret=%zu bytes, lifetime=%u seconds\n",
		Info.Ticket.Size, Info.Secret.Size, Info.Lifetime
	);
	xrtTlsResumeRelease(pResume);
	return 0;
}
