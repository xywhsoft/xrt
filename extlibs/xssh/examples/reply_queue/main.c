#include <stdio.h>

#include <xssh.h>



/* 演示把 want-reply 请求关联到调用方任务 token。 */
int main(void)
{
	uint64 arrTokens[8];
	xsshreplyqueue Queue;
	uint64 iToken;

	if ( !xrtSshReplyQueueInit(&Queue, arrTokens, 8u) ||
		(xrtSshReplyQueuePush(&Queue, 1001u) != XSSH_OK) ||
		(xrtSshReplyQueuePop(&Queue, &iToken) != XSSH_OK) ) {
		return 1;
	}
	printf("completed-token=%llu\n", (unsigned long long)iToken);
	return 0;
}
