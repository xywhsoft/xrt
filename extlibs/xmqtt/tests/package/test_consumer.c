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
	xmqttproperty Properties[2]={0}; Properties[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Properties[0].Type=XMQTT_PROPERTY_BYTE; Properties[0].Value.Number=1;
	const unsigned char Data[]={0,255}; Properties[1].Id=XMQTT_PROPERTY_CORRELATION_DATA; Properties[1].Type=XMQTT_PROPERTY_BINARY; Properties[1].Value.Binary=(xbytesview){Data,2};
	xmqttproperty Parsed[2]; xmqttpropertydocument Doc;
	if (!xmqttPropertiesWrite(Output,sizeof(Output),Properties,2,XMQTT_PROPERTIES_PUBLISH,XMQTT_CLIENT_TO_SERVER,NULL,&Written) ||
		xmqttPropertiesRead((xbytesview){Output,Written},XMQTT_PROPERTIES_PUBLISH,XMQTT_CLIENT_TO_SERVER,NULL,Parsed,2,&Doc)!=XMQTT_PROPERTIES_DONE ||
		Doc.Count!=2 || Parsed[0].Value.Number!=1 || Parsed[1].Value.Binary.Size!=2 || memcmp(Parsed[1].Value.Binary.Data,Data,2)) return 4;
	if (xmqttFrameRead((xbytesview){NULL,1},NULL,&Frame) != XMQTT_WIRE_ERROR || xrtErrorDomain(xrtGetError()) == NULL ||
		strcmp(xrtErrorDomain(xrtGetError()),"xrt.mqtt") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 3;
	xrtClearError(); return 0;
}
