#include <stdio.h>

#include <xrt.h>



/* 展示数字地址、端点、分类和拥有文本的常用路径。 */
int main(void)
{
	xnetaddr Addr;
	str sEndpoint;

	if ( !xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0) ) {
		return 1;
	}
	sEndpoint = xrtNetAddrEndpointString(&Addr);
	if ( sEndpoint == NULL ) {
		return 1;
	}
	printf("endpoint=%s\nfamily=%u\nlink-local=%s\n",
		sEndpoint, (unsigned int)Addr.Family,
		xrtNetAddrIsLinkLocal(&Addr) ? "yes" : "no");
	xrtFree(sEndpoint);
	return 0;
}
