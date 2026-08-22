#include <stdio.h>

#include <xrt.h>



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
