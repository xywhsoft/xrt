#include <stdio.h>
#include <xssh.h>



/* 在发送包之前登记本代密钥消耗。 */
int main(void)
{
	xsshrekeystate State;
	xsshrekeydecision Decision;

	if ( !xrtSshRekeyInit(&State, NULL, ((double)(0u)) / 1000.0) ||
		(xrtSshRekeyReserveSend(
			&State,
			1024u,
			64u, ((double)(1u)) / 1000.0,
			&Decision
		) != XSSH_OK) ) {
		return 1;
	}
	printf("packets=%llu decision=%d\n",
		(unsigned long long)State.Sent.Packets,
		(int)Decision);
	return Decision == XSSH_REKEY_REQUIRED ? 1 : 0;
}
