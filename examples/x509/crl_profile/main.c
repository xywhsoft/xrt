#include <stdio.h>

#include <xrt.h>



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
