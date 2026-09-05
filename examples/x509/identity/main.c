#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/identity —— 证书 DNS 身份匹配（通配符规则）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509MatchDns   SAN DNS-ID / CN 匹配，支持通配符
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/identity/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   matched=yes
 *
 * RFC 6125 规则内建：通配符只匹配最左一段
 *   （*.example.test 命中 api.example.test、不命中 a.b.example.test）、
 *   只在左标签出现、大小写不敏感——手工 strcasecmp + strstr
 *   实现的那些越界匹配这里都过不去。TLS 客户端验证
 *   身份的最后一步就是它。
 */


/* 演示零分配 DNS-ID 通配符匹配。 */
int main(void)
{
	xx509result Result = xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("api.example.test")
	);

	printf("matched=%s\n", (Result == X509_VALUE) ? "yes" : "no");
	return (Result == X509_VALUE) ? 0 : 1;
}
