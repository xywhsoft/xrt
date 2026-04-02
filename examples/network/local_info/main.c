/*
 * XRT Example - Local Info
 * XRT 范例 - 本机网络信息
 *
 * Description / 说明:
 *   EN: Demonstrates local IP, MAC and host name queries.
 *   CN: 演示本机 IP、MAC 和主机名获取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_local_info.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_local_info -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	str sIP;
	str sMAC;
	str sName;

	xrtInit();

	sIP = xrtGetLocalIP();
	sMAC = xrtGetLocalMAC();
	sName = xrtGetLocalName();

	printf("local_name = %s\n", sName);
	printf("local_ip = %s\n", sIP);
	printf("local_mac = %s\n", sMAC);

	xrtFree(sIP);
	xrtFree(sMAC);
	xrtFree(sName);

	xrtUnit();
	return 0;
}
