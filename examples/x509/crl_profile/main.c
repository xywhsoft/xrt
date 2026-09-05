#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/crl_profile —— CRL 编号与撤销原因扩展解析
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509CrlNumberParse   CRLNumber 扩展 → 任意精度字节视图
 *   xrtX509CrlReasonParse   ReasonCode → 枚举
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/crl_profile/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   CRL number bytes: 2, reason: 1
 *
 * 编号 0x0100（256）以两字节视图返回——大 CA 的 CRL 计数
 *   超过 64 位也不截断，调用方自行选择整数策略。
 *   reason=1 即 keyCompromise：高安全场景按原因分级响应
 *   （密钥泄露需立即断连并吊销会话，CA 维护性停用则不必）。
 */


/* 展示从已解析 CRL 读取任意精度编号和条目撤销原因。 */
int main(void)
{
	static const uint8 NumberDer[] = { 0x02, 0x02, 0x01, 0x00 };
	static const uint8 ReasonDer[] = { 0x0A, 0x01, 0x01 };
	xbytesview Number;
	xx509crlreason Reason;

	if ( !xrtX509CrlNumberParse(
		(xbytesview) { NumberDer, sizeof(NumberDer) }, &Number
	) || !xrtX509CrlReasonParse(
		(xbytesview) { ReasonDer, sizeof(ReasonDer) }, &Reason
	) ) {
		return 1;
	}
	printf(
		"CRL number bytes: %zu, reason: %d\n",
		Number.Size, (int)Reason
	);
	return 0;
}
