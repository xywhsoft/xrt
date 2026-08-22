#include <xssh.h>
#include <stdio.h>



/* 随机便利层保留与确定性核心相同的会话对象布局。 */
int main(void)
{
	printf("SSH KEX random adapter bytes: %zu\n", sizeof(xsshkexsession));
	return 0;
}
