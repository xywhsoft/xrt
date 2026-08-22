#include <stdio.h>

#include <xssh.h>



/* 展示不持有 Stream 的按需 SSH packet 读取器。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessionreader Reader;
	xsshsessiontcp Session;

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) || !xrtSshSessionReaderInit(
		&Reader,
		NULL,
		&Session
	) ) {
		return 1;
	}
	printf(
		"state=%d reader_bytes=%zu host_key_bytes=%zu\n",
		(int)xrtSshSessionReaderState(&Reader),
		sizeof(Reader),
		xrtSshSessionReaderHostKey(&Reader).Size
	);
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Session);
	return 0;
}
