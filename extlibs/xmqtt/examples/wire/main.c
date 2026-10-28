#include <xmqtt.h>
#include <stdio.h>

int main(void)
{
	/* Offline example: no broker, network, heap-owned result or cleanup needed.
	 * Inspect two coalesced MQTT 5.0 PING packets using borrowed frame views. */
	const unsigned char Input[] = {0xc0,0,0xd0,0}; size_t Position = 0;
	while (Position < sizeof(Input)) {
		xmqttframe Frame;
		if (xmqttFrameRead((xbytesview){Input+Position,sizeof(Input)-Position},NULL,&Frame) != XMQTT_WIRE_DONE) return 1;
		printf("type=%d header=%zu body=%zu\n",(int)Frame.Header.Type,Frame.Header.HeaderBytes,Frame.Body.Size);
		Position += Frame.Header.PacketBytes;
	}
	/* A BOM is part of a string, and is retained by a reader. */
	unsigned char Output[32]; size_t Written, Read; xstrview Text;
	if (!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\xef\xbb\xbf"),0,&Written) ||
		xmqttUtf8StringRead((xbytesview){Output,Written},0,&Text,&Read) != XMQTT_WIRE_DONE || Text.Size != 4) return 2;
	printf("string-bytes=%zu consumed=%zu\n",Text.Size,Read); return 0;
}
