#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供自适应 TLS 握手重组。 */
int main(void)
{
	static const uint8 Data[] = {
		20, 0, 0, 4, 'd', 'o', 'n', 'e'
	};
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iConsumed = 0;
	int iResult = 1;

	if ( xrtTlsHandshakeReaderInit(&Reader, NULL) &&
		(xrtTlsHandshakeReaderRead(
			&Reader, (xbytesview) { Data, 3u },
			&iConsumed, &Message
		) == XTLS_AGAIN) &&
		(xrtTlsHandshakeReaderRead(
			&Reader, (xbytesview) { Data + 3u, sizeof(Data) - 3u },
			&iConsumed, &Message
		) == XTLS_OK) && (Message.Body.Size == 4u) &&
		(memcmp(Message.Body.Data, "done", 4u) == 0) ) {
		iResult = 0;
	}
	xrtTlsHandshakeReaderUnit(&Reader);
	return iResult;
}
