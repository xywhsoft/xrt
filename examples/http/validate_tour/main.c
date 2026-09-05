/*
 * 范例：http/validate_tour —— 地址与主机验证族
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpIpv4Valid / Ipv6Valid       RFC 3986 严格 IPv4/IPv6 谓词
 *   xrtHttpHostValid / HostEqual       主机合法性 / 规范比较
 *   xrtHttpAuthorityValid              authority 结构合法性（配合 HostParse）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/validate_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   ipv4: 192.168.1.1=1 1.2.3=0 01.2.3.4=0
 *   ipv6: ::1=1 [::1]=0 fe80::1=1
 *   host: example.com=1 EXAMPLE.com eq example.com=1
 *   authority: example.com:8443=1 [::1]:80=1
 *
 * Ipv4Valid 的"严格"含义：拒绝多段/越界/前导零
 *   （01.2.3.4 不合法——RFC 3986 禁止前导零）。
 *   Ipv6Valid 支持压缩与嵌入式 IPv4，但不接受 ZoneID。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xhttpauthority Auth;

	printf("ipv4: 192.168.1.1=%d 1.2.3=%d 01.2.3.4=%d\n",
		xrtHttpIpv4Valid(SV("192.168.1.1")) ? 1 : 0,
		xrtHttpIpv4Valid(SV("1.2.3")) ? 1 : 0,
		xrtHttpIpv4Valid(SV("01.2.3.4")) ? 1 : 0);
	printf("ipv6: ::1=%d [::1]=%d fe80::1=%d\n",
		xrtHttpIpv6Valid(SV("::1")) ? 1 : 0,
		xrtHttpIpv6Valid(SV("[::1]")) ? 1 : 0,
		xrtHttpIpv6Valid(SV("fe80::1")) ? 1 : 0);
	printf("host: example.com=%d EXAMPLE.com eq example.com=%d\n",
		xrtHttpHostValid(SV("example.com")) ? 1 : 0,
		xrtHttpHostEqual(SV("EXAMPLE.com"), SV("example.com")) ? 1 : 0);

	/* AuthorityValid 收结构：先 HostParse 再整体校验。 */
	(void)xrtHttpHostParse(SV("example.com:8443"), &Auth);
	printf("authority: example.com:8443=%d",
		xrtHttpAuthorityValid(&Auth) ? 1 : 0);
	(void)xrtHttpHostParse(SV("[::1]:80"), &Auth);
	printf(" [::1]:80=%d\n", xrtHttpAuthorityValid(&Auth) ? 1 : 0);
	return 0;
}
