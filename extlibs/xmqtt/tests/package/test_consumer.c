#define XMQTT_MODULE_XMQTT
#include <xmqtt.h>
#include <string.h>

int main(void)
{
	unsigned char Output[128]; size_t Written; xmqttframe Frame;
	if (!xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PINGREQ,0,0,NULL,&Written) || Written != 2 ||
		xmqttFrameRead((xbytesview){Output,Written},NULL,&Frame) != XMQTT_WIRE_DONE || Frame.Header.Type != XMQTT_PACKET_PINGREQ) return 1;
	xstrview Text; size_t Read;
	if (!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\xef\xbb\xbf"),0,&Written) ||
		xmqttUtf8StringRead((xbytesview){Output,Written},0,&Text,&Read) != XMQTT_WIRE_DONE || Text.Size != 4 || memcmp(Text.Data,"a\xef\xbb\xbf",4)) return 2;
	if (xmqttFrameRead((xbytesview){NULL,1},NULL,&Frame) != XMQTT_WIRE_ERROR || xrtErrorDomain(xrtGetError()) == NULL ||
		strcmp(xrtErrorDomain(xrtGetError()),"xrt.mqtt") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 3;
	xrtClearError(); return 0;
}
