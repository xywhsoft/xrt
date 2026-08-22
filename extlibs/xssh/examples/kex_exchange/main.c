#include <xssh.h>
#include <stdio.h>



/* 展示动态 transcript 对象空闲时不分配任何数据块。 */
int main(void)
{
	xsshkexexchange Exchange;

	if ( !xrtSshKexExchangeInit(
		&Exchange,
		NULL,
		XSSH_ROLE_CLIENT
	) ) {
		return 1;
	}
	printf("SSH KEX exchange bytes: %zu\n", sizeof(Exchange));
	xrtSshKexExchangeClear(&Exchange);
	return 0;
}
