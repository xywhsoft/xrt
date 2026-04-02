/*
 * XRT Example - DNS Resolve
 * XRT 范例 - DNS 解析
 *
 * Description / 说明:
 *   EN: Demonstrates resolving a host name into a network address.
 *   CN: 演示将主机名解析为网络地址。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_dns_resolve.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_dns_resolve -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	xnetaddr tAddr;
	xnet_result iRet;

	xrtInit();

	iRet = xrtNetResolve("example.com", &tAddr);
	printf("resolve_status = %d\n", iRet);
	if ( iRet == XRT_NET_OK ) {
		printf("resolved_addr = %s\n", xrtNetAddrToStr(&tAddr));
	}

	xrtUnit();
	return 0;
}
