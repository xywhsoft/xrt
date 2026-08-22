#include <xssh.h>
#include <stdio.h>



/* 随机便利层不改变动态交换对象布局。 */
int main(void)
{
	printf("SSH random KEX exchange bytes: %zu\n", sizeof(xsshkexexchange));
	return 0;
}
