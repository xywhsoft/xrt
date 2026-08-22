#include <stdio.h>

#include <xssh.h>



/* 构建普通密码请求并读取借用凭据视图。 */
int main(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthpassword Password;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshAuthPasswordWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL("secret")
		) != XSSH_OK) || (xrtSshAuthPasswordRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Password
		) != XSSH_OK) ) {
		return 1;
	}
	printf("user=%.*s bytes=%zu\n",
		(int)Password.User.Size,
		Password.User.Data,
		Writer.Size);
	return 0;
}
