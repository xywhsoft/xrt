#include <stdio.h>
#include <xssh.h>



/* 展示默认生产构建显式选择系统安全随机便利层。 */
int main(void)
{
	#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)
		printf("ssh TCP secure padding enabled\n");
		return 0;
	#else
		return 1;
	#endif
}
