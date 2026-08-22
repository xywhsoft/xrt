#include <stdio.h>

#include <xssh.h>



/* 构建一个保留原始扩展字段的全局请求。 */
int main(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshglobalrequest Request;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshGlobalRequestWrite(
			&Writer,
			XRT_STR_LITERAL("keepalive@example.com"),
			true,
			(xbytesview){ NULL, 0u }
		) != XSSH_OK) || (xrtSshGlobalRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) != XSSH_OK) ) {
		return 1;
	}
	printf("request=%.*s\n", (int)Request.Name.Size, Request.Name.Data);
	return 0;
}
