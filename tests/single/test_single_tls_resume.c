#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 TLS 恢复对象、有效期和票据年龄路径。 */
int main(void)
{
	uint8 Ticket[3] = { 1, 2, 3 };
	uint8 Secret[32] = { 0 };
	xtlsresumeconfig Config;
	xtlsresumeinfo Info;
	xtlsresume* pResume;
	uint32 iAge;

	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	Config.Secret = (xbytesview) { Secret, sizeof(Secret) };
	Config.Lifetime = 60u;
	Config.AgeAdd = 7u;
	Config.IssuedAt = 1000000;
	pResume = xrtTlsResumeCreate(&Config);
	if ( (pResume == NULL) || !xrtTlsResumeInfo(pResume, &Info) ||
		(Info.ExpiresAt != 61000000) || !xrtTlsResumeTicketAge(
			pResume, 1001000, &iAge
		) || (iAge != 8u) ) {
		xrtTlsResumeRelease(pResume);
		return 1;
	}
	xrtTlsResumeRelease(pResume);
	printf("[PASS] single-tls-resume\n");
	return 0;
}
