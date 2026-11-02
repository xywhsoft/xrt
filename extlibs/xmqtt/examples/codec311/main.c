#include <xmqtt.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	/* Offline binary QoS-1 packet; no broker or session acknowledgement. */
	xmqttpacket Packet, Parsed; if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet)) return 1;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("sensors/raw");
	const unsigned char Data[]={0,255,0xc0,0x80}; Packet.Body.Publish.Payload=(xbytesview){Data,sizeof(Data)};
	unsigned char Wire[128]; size_t Size, Written;
	if (!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Size) ||
		!xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) || Size!=Written ||
		xmqttPacketRead311((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,NULL)!=XMQTT_CODEC_DONE ||
		Parsed.Body.Publish.Payload.Size!=sizeof(Data) || memcmp(Parsed.Body.Publish.Payload.Data,Data,sizeof(Data))) return 2;
	printf("MQTT311 PUBLISH id=%u topic=%.*s payload-bytes=%zu wire-bytes=%zu\n",(unsigned)Parsed.Identifier,(int)Parsed.Body.Publish.Topic.Size,
		Parsed.Body.Publish.Topic.Data,Parsed.Body.Publish.Payload.Size,Parsed.Wire.Size); return 0;
}
