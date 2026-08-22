#include <xrt.h>

#include <stdio.h>



/* 写出并解析一个带 UTF-8 原因的 Close 控制帧负载。 */
int main(void)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	xwsclose Close;
	xbytesview Input;
	size_t iSize;

	if ( !xrtWsCloseWrite(
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("shutdown"),
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		return 1;
	}
	Input.Data = Payload;
	Input.Size = iSize;
	if ( !xrtWsCloseParse(Input, &Close) ) {
		return 2;
	}
	printf(
		"code=%u reason=%.*s\n",
		(unsigned)Close.Code,
		(int)Close.Reason.Size,
		Close.Reason.Data
	);
	return 0;
}
