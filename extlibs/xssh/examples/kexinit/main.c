#include <stdio.h>
#include <xssh.h>



/* 使用显式 cookie 构建并解析 KEXINIT，适合测试和自定义随机源。 */
int main(void)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshwriter Writer;
	xsshkexinit KexInit;

	if ( !xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) || !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) != XSSH_OK) || (xrtSshKexInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&KexInit
		) != XSSH_OK) ) {
		return 1;
	}
	printf("kexinit=%zu kex=%.*s\n", Writer.Size,
		(int)KexInit.KexAlgorithms.Size, KexInit.KexAlgorithms.Data);
	return 0;
}
