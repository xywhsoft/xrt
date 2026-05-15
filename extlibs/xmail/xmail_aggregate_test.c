#include <stdio.h>

#define XRT_IMPLEMENTATION
#include "xmail.h"

int main(void)
{
	xmailaddr tAddr;
	xsmtpconfig tSmtp;
	xpop3config tPop3;
	ximapconfig tImap;

	memset(&tAddr, 0, sizeof(tAddr));
	xrtSmtpConfigInit(&tSmtp);
	xrtPop3ConfigInit(&tPop3);
	xrtImapConfigInit(&tImap);

	if ( XSMTP_SECURE_STARTTLS == 0 || XPOP3_SECURE_STARTTLS == 0 || XIMAP_SECURE_STARTTLS == 0 ) {
		printf("FAIL xmail_aggregate_constants\n");
		return 1;
	}
	if ( tSmtp.iTimeoutMs == 0u || tPop3.iTimeoutMs == 0u || tImap.iTimeoutMs == 0u ) {
		printf("FAIL xmail_aggregate_defaults\n");
		return 1;
	}
	printf("PASS xmail_aggregate_header\n");
	return 0;
}
