#include <stdio.h>

#include <xssh.h>



/* 为一条认证请求预留默认资源预算。 */
int main(void)
{
	xsshauthguard Guard;
	xsshauthguarddecision Decision;

	if ( !xrtSshAuthGuardInit(&Guard, NULL, ((double)(1000u)) / 1000.0) ||
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_ATTEMPT,
			128u, ((double)(1001u)) / 1000.0,
			&Decision
		) != XSSH_OK) ) {
		return 1;
	}
	printf("decision=%d attempts=%u\n", (int)Decision, Guard.Attempts);
	return Decision == XSSH_AUTH_GUARD_ALLOW ? 0 : 1;
}
