#include <stdio.h>
#include <xssh.h>



/* 使用操作系统安全随机 cookie 构建生产 KEXINIT。 */
int main(void)
{
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshwriter Writer;

	if ( !xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) || !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshKexInitWriteSecure(&Writer, &Config) != XSSH_OK) ) {
		return 1;
	}
	printf("secure-kexinit=%zu\n", Writer.Size);
	return 0;
}
