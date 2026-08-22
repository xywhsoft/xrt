#include <stdio.h>

#include <xssh.h>



/* 校验 hostbased 客户端主机名。 */
int main(void)
{
	xstrview HostName = XRT_STR_LITERAL("builder.example.com.");

	if ( !xrtSshAuthHostNameValid(HostName) ) {
		return 1;
	}
	printf("host=%.*s\n", (int)HostName.Size, HostName.Data);
	return 0;
}
