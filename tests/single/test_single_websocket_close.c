#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Close 负载的无分配往返能力。 */
int main(void)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	xwsclose Close;
	xbytesview Input;
	size_t iSize;

	if ( !xrtWsCloseWrite(
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("single"),
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		return 1;
	}
	Input.Data = Payload;
	Input.Size = iSize;
	if ( !xrtWsCloseParse(Input, &Close) ||
		(Close.Code != XWS_CLOSE_NORMAL) ||
		(Close.Reason.Size != 6u) ) {
		return 2;
	}
	return 0;
}
