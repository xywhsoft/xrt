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
	xmqttpacket Packet, Decoded; xmqttcodecconfig Config; xmqttcodecissue Issue;
	if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet) || !xmqttCodecConfigInit(&Config)) return 5;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("pkg/test");
	const unsigned char Payload[]={'x',0}; Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
	Packet.Properties=(xmqttpropertylist){Properties,2}; size_t Required;
	xmqttcodecbuffers Buffers={Parsed,2,NULL,0};
	if (!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) ||
		!xmqttPacketWrite5(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) || Required!=Written ||
		xmqttPacketRead5((xbytesview){Output,Written},XMQTT_CLIENT_TO_SERVER,&Config,&Buffers,&Decoded,&Issue)!=XMQTT_CODEC_DONE ||
		Decoded.Identifier!=42 || Decoded.Body.Publish.Payload.Size!=sizeof(Payload) || memcmp(Decoded.Body.Publish.Payload.Data,Payload,sizeof(Payload)) ||
		Decoded.Wire.Data!=Output || Decoded.Wire.Size!=Written || Issue.Error!=XMQTT_CODEC_OK) return 6;
	Packet.Properties=(xmqttpropertylist){NULL,0}; const unsigned char Raw[]={255,0,0xc0,0x80}; Packet.Body.Publish.Payload=(xbytesview){Raw,sizeof(Raw)};
	Config.MaxPropertyBytes=0; Config.MaxProperties=0;
	if (!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) ||
		!xmqttPacketWrite311(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) || Written!=Required ||
		xmqttPacketRead311((xbytesview){Output,Written},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Decoded,&Issue)!=XMQTT_CODEC_DONE ||
		Decoded.Identifier!=42 || Decoded.Body.Publish.Payload.Size!=sizeof(Raw) || memcmp(Decoded.Body.Publish.Payload.Data,Raw,sizeof(Raw)) ||
		Decoded.Properties.Count!=0 || Issue.Error!=XMQTT_CODEC_OK) return 7;
	xmqttpacket Connect,Connack; xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect); xmqttPacketInit(XMQTT_PACKET_CONNACK,&Connack);
	Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("package");xmqttnegotiated Peer;
	if(xmqttNegotiate(&Connect,&Connack,NULL,&Peer,NULL)!=XMQTT_NEGOTIATION_DONE || !Peer.Accepted || Peer.SendReceiveMaximum!=65535 ||
	   xmqttNegotiatedPacketCheck(&Peer,&Packet,NULL,&Required,NULL)!=XMQTT_NEGOTIATION_DONE)return 8;
	if (xmqttFrameRead((xbytesview){NULL,1},NULL,&Frame) != XMQTT_WIRE_ERROR || xrtErrorDomain(xrtGetError()) == NULL ||
		strcmp(xrtErrorDomain(xrtGetError()),"xrt.mqtt") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 3;
	xrtClearError(); return 0;
}
