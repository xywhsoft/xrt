#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/crl —— CRL 解析与撤销条目遍历
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509CrlParse          DER → xx509crl 借用视图
 *   xrtX509CrlEntryInit/Read 撤销序列号游标（三态）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/crl/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   version=1 entries=complete
 *
 * 本例 CRL 的撤销列表为空：Read 立即返回 DONE——
 *   "complete" 说明结构完整走到尾（区别于 error）。
 *   条目非空时循环会逐条给出 Serial 视图与撤销时间，
 *   序列号保持任意精度字节视图（不做截断式整数转换）。
 */


/* 演示零分配解析 CRL，并遍历其中的撤销序列号。 */
int main(void)
{
	static const uint8 CrlDer[] = {
		0x30, 0x3A, 0x30, 0x2D, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x30,
		0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0A,
		0x58, 0x52, 0x54, 0x20, 0x43, 0x52, 0x4C, 0x20, 0x43, 0x41, 0x17, 0x0D,
		0x32, 0x36, 0x30, 0x34, 0x30, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
		0x5A, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x03, 0x02, 0x00, 0x01,
	};
	xx509crl Crl;
	xx509crlcursor Cursor;
	xx509crlentry Entry;
	xx509result Result;

	if ( !xrtX509CrlParse(CrlDer, sizeof(CrlDer), &Crl) ||
		!xrtX509CrlEntryInit(&Crl, &Cursor) ) {
		return 1;
	}
	while ( (Result = xrtX509CrlEntryRead(
		&Cursor, &Entry
	)) == X509_VALUE ) {
		printf("serial-bytes=%zu\n", Entry.Serial.Size);
	}
	printf("version=%d entries=%s\n",
		(int)Crl.Version, Result == X509_DONE ? "complete" : "error");
	return Result == X509_DONE ? 0 : 1;
}
