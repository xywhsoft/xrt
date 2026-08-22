#include <stdio.h>

#include <xssh.h>



/* 为一条认证请求预留默认资源预算。 */
int main(void)
{
	xsshauthguard Guard;
	xsshauthguarddecision Decision;

	if ( !xrtSshAuthGuardInit(&Guard, NULL, 1000u) ||
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_ATTEMPT,
			128u,
			1001u,
			&Decision
		) != XSSH_OK) ) {
		return 1;
	}
	printf("decision=%d attempts=%u\n", (int)Decision, Guard.Attempts);
	return Decision == XSSH_AUTH_GUARD_ALLOW ? 0 : 1;
}
