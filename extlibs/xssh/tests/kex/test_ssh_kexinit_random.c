#include "../test.h"



/* 验证系统安全随机 cookie 的生产便利路径。 */
int main(void)
{
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshwriter Writer;
	xsshkexinit KexInit;

	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWriteSecure(&Writer, &Config) == XSSH_OK) &&
		(xrtSshKexInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&KexInit
		) == XSSH_OK) && (KexInit.Cookie.Size == XSSH_KEX_COOKIE_SIZE),
		"ssh secure kexinit write failed");
	testRequire(xrtSshKexInitWriteSecure(
		&Writer,
		NULL
	) == XSSH_ERROR_ARGUMENT, "ssh secure kexinit accepted missing role");
	return 0;
}
