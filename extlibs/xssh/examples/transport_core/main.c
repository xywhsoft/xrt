#include <stdio.h>
#include <xssh.h>



/* 创建一个不持有网络和缓冲的 client transport core。 */
int main(void)
{
	xsshtransportcore Core;

	if ( !xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) ) {
		return 1;
	}
	printf("transport-core=%zu max-packet=%u\n",
		sizeof(Core),
		(unsigned int)Core.Codec.MaxPacketSize);
	xrtSshTransportCoreClear(&Core);
	return 0;
}
