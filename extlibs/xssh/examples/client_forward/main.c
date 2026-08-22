#include <stdio.h>

#include <xrt/ssh_client_forward.h>



/* 展示 forwarding 发起端不引入隐藏 listener 或线程。 */
int main(void)
{
	xsshclientconfig Config;

	if ( !xrtSshClientConfigInit(&Config) ) {
		return 1;
	}
	printf("global-replies=%zu\n", Config.GlobalReplyLimit);
	return 0;
}
