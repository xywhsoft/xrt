#include <stdio.h>
#include <xrt.h>



/* 解析 Host 字段并直接读取借用的主机与端口视图。 */
int main(void)
{
	xhttpauthority Host;
	uint16 iPort;

	if ( !xrtHttpHostParse(
		XRT_STR_LITERAL("[2001:db8::1]:8443"), &Host
	) || !xrtHttpAuthorityPort(&Host, 80u, &iPort) ) {
		return 1;
	}
	printf(
		"host=%.*s port=%u\n",
		(int)Host.Host.Size,
		Host.Host.Data,
		(unsigned)iPort
	);
	return 0;
}
