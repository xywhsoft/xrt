#include <stdio.h>
#include <xmail.h>



/* 生成可以直接作为 SMTP DATA 内容发送的线路字节。 */
int main(void)
{
	bytes pWire;
	size_t iSize;

	pWire = xrtMailDot(
		XRT_STR_LITERAL("Subject: wire\r\n\r\n.line"),
		true,
		&iSize
	);
	if ( pWire == NULL ) {
		return 1;
	}
	fwrite(pWire, 1u, iSize, stdout);
	xrtFree(pWire);
	return 0;
}
