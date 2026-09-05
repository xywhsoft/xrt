#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/profile —— GeneralNames 遍历：SAN 的多形态身份
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509GeneralNameInit/Read   通用名序列游标
 *   xx509genname                  {Type, Value}：DNS/IP/URI/...
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/profile/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   type=2 size=16
 *   type=7 size=4
 *
 * 一个 SAN 扩展可容纳多种身份：type=2 dNSName
 *   （api.example.test，16 字节）+ type=7 iPAddress
 *   （127.0.0.1，4 字节）。证书匹配按序尝试全部身份——
 *   IP 直连（无 DNS）场景靠 iPAddress 项通过验证。
 */


/* 演示零分配遍历独立 GeneralNames DER。 */
int main(void)
{
	static const uint8 GeneralNames[] = {
		0x30, 0x18,
		0x82, 0x10, 'a', 'p', 'i', '.', 'e', 'x', 'a', 'm',
		'p', 'l', 'e', '.', 't', 'e', 's', 't',
		0x87, 0x04, 127, 0, 0, 1
	};
	xx509gencursor Cursor;
	xx509genname Name;
	xx509result Result;

	if ( !xrtX509GeneralNameInit(
		(xbytesview) { GeneralNames, sizeof(GeneralNames) }, &Cursor
	) ) {
		return 1;
	}
	while ( (Result = xrtX509GeneralNameRead(
		&Cursor, &Name
	)) == X509_VALUE ) {
		printf("type=%d size=%zu\n", (int)Name.Type, Name.Value.Size);
	}
	return (Result == X509_DONE) ? 0 : 1;
}
