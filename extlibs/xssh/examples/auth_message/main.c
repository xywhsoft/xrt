#include <stdio.h>

#include <xssh.h>



/* 构建 none 探测并读取用户名。 */
int main(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xstrview User;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshAuthNoneWrite(
			&Writer,
			XRT_STR_LITERAL("alice")
		) != XSSH_OK) || (xrtSshAuthNoneRead(
			(xbytesview){ arrPayload, Writer.Size },
			&User
		) != XSSH_OK) ) {
		return 1;
	}
	printf("user=%.*s bytes=%zu\n", (int)User.Size, User.Data, Writer.Size);
	return 0;
}
