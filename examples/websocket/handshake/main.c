#include <stdio.h>

#include <xrt.h>



/* 展示无需网络对象的 RFC Accept 计算与子协议协商。 */
int main(void)
{
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Selected;

	if ( !xrtWsAccept(
		XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
		Accept,
		sizeof(Accept)
	) ) {
		return 1;
	}
	if ( !xrtWsProtocolSelect(
		XRT_STR_LITERAL("chat, superchat"),
		XRT_STR_LITERAL("superchat, binary"),
		&Selected
	) ) {
		return 2;
	}
	printf(
		"accept=%s protocol=%.*s\n",
		Accept,
		(int)Selected.Size,
		Selected.Data != NULL ? Selected.Data : ""
	);
	return 0;
}
