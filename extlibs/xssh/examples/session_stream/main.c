#include <stdio.h>

#include <xssh.h>



/* 展示 callback Stream 驱动在连接前不占用 Engine、Worker 或动态缓冲。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessionstream Session;

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionStreamInit(
		&Session,
		&Config,
		NULL,
		NULL
	) ) {
		return 1;
	}
	printf(
		"state=%d stream=%p session=%p adapter_bytes=%zu\n",
		(int)xrtSshSessionStreamState(&Session),
		(void*)xrtSshSessionStreamTcp(&Session),
		(void*)xrtSshSessionStreamSession(&Session),
		sizeof(Session)
	);
	return xrtSshSessionStreamClear(&Session) ? 0 : 1;
}
