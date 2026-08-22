#include <xssh.h>
#include <stdio.h>



/* 展示会话对象本身不创建网络、任务或大块固定缓冲。 */
int main(void)
{
	xsshkexsession Session;

	if ( !xrtSshKexSessionInit(&Session, XSSH_ROLE_CLIENT) ) {
		return 1;
	}
	printf("SSH KEX session bytes: %zu\n", sizeof(Session));
	xrtSshKexSessionClear(&Session);
	return 0;
}
