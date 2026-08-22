#include <stdio.h>

#include <xssh.h>



/* 展示认证编排对象与直接写入最终 payload 的 service 起点。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshauthsession Session;
	xsshwriter Writer;

	if ( !xrtSshAuthSessionInit(&Session, XSSH_ROLE_CLIENT) ||
		!xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) != XSSH_OK) ) {
		return 1;
	}
	printf(
		"session=%zu service-request=%zu event=%d\n",
		sizeof(Session),
		Writer.Size,
		(int)xrtSshAuthSessionEvent(&Session)
	);
	return 0;
}
