#include <stdio.h>
#include <xmail.h>



/* 创建 Message-ID 和 MIME boundary。 */
int main(void)
{
	str sMessageId = xrtMailMessageId(XRT_STR_LITERAL("example.com"), NULL);
	str sBoundary = xrtMailBoundary(NULL);

	if ( (sMessageId == NULL) || (sBoundary == NULL) ) {
		xrtFree(sMessageId);
		xrtFree(sBoundary);
		return 1;
	}
	printf("Message-ID: %s\nboundary=%s\n", sMessageId, sBoundary);
	xrtFree(sMessageId);
	xrtFree(sBoundary);
	return 0;
}
