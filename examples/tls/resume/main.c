#include <xrt/tls_resume.h>

#include <stdio.h>



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
