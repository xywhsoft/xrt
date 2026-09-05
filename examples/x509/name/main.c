#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/name —— X.509 Name 跨编码比较（PrintableString vs UTF8）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509NameEqual   完整 DN 语义比较（非字节比较）
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/name/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   equal=true
 *
 * 为什么不相等却判相等：两个 CN 一个是 PrintableString
 *   "ALICE"、一个是 UTF8String "alice"——RFC 5280 规定
 *   DirectoryString 族做大小写不敏感的规范化比较。
 *   字节级 memcmp 会得到 false（这正是 CA 迁移编码时
 *   链校验失败的经典坑）；NameEqual 内置规范化。
 */


/* 演示跨 DirectoryString 编码比较两个完整 X.509 Name。 */
int main(void)
{
	static const uint8 PrintableName[] = {
		0x30, 0x10, 0x31, 0x0E, 0x30, 0x0C,
		0x06, 0x03, 0x55, 0x04, 0x03,
		0x13, 0x05, 'A', 'L', 'I', 'C', 'E'
	};
	static const uint8 Utf8Name[] = {
		0x30, 0x10, 0x31, 0x0E, 0x30, 0x0C,
		0x06, 0x03, 0x55, 0x04, 0x03,
		0x0C, 0x05, 'a', 'l', 'i', 'c', 'e'
	};
	xx509result Result = xrtX509NameEqual(
		(xbytesview) { PrintableName, sizeof(PrintableName) },
		(xbytesview) { Utf8Name, sizeof(Utf8Name) }
	);

	if ( Result == X509_ERROR ) {
		return 1;
	}
	printf("equal=%s\n", Result == X509_VALUE ? "true" : "false");
	return Result == X509_VALUE ? 0 : 1;
}
