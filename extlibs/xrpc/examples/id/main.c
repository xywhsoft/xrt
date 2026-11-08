#include <xrpc.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	unsigned char A[128],B[128];size_t Left=0,Right=0;
	if(!xrpcIdKeyWrite(A,sizeof(A),XRT_STR_LITERAL("10e18446744073709551615"),NULL,&Left) ||
		!xrpcIdKeyWrite(B,sizeof(B),XRT_STR_LITERAL("1e18446744073709551616"),NULL,&Right) || Left!=Right || memcmp(A,B,Left))return 1;
	if(!xrpcIdKeyWrite(A,sizeof(A),XRT_STR_LITERAL("\"a\\u0000\\u0062\""),NULL,&Left) ||
		!xrpcIdKeyWrite(B,sizeof(B),XRT_STR_LITERAL("\"a\\u0000b\""),NULL,&Right) || Left!=Right || memcmp(A,B,Left))return 2;
	/* Store comparison keys with pending entries; retain the ORIGINAL raw ID
	 * separately when building requests/responses. A binary key is not JSON. */
	puts("[PASS] equivalent huge-exponent and NUL string IDs share comparison keys");return 0;
}
