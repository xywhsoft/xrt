#include <stdio.h>
#include <xrt.h>



/*
 * 范例：http/host —— Host 字段解析：IPv6 字面量与默认端口
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpHostParse      严格解析 Host（IPv4/IPv6/[::]:port/域名）
 *   xrtHttpAuthorityPort  取端口，缺省时返回调用方给的默认值
 *   xhttpauthority        结果结构：Host 视图 + 显式 Port
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/host/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   host=2001:db8::1 port=8443
 *
 * 严格性：[2001:db8::1]:8443 的方括号必须成对、IPv6 字面量
 *   逐组校验——域名冒号端口这类歧义输入不会误判。
 * "默认端口"设计：解析结果区分"显式给了端口"与"没给"，
 *   8443 是显式的；没给时 AuthorityPort 用第二参的默认值。
 */


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
