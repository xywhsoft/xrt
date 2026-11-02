#include <xmqtt.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	/* Offline QoS-1 PUBLISH bytes. No connection/session/acknowledgement effect. */
	xmqttpacket Packet; if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet)) return 1;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("sensors/temperature");
	const unsigned char Payload[]={ '2','3',0,'C' }; Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
	xmqttproperty Property={0}; Property.Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Property.Type=XMQTT_PROPERTY_BYTE; Property.Value.Number=1;
	Packet.Properties=(xmqttpropertylist){&Property,1};
	unsigned char Wire[128]; size_t Required, Written;
	if (!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Required) ||
		!xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) || Required!=Written) return 2;
	xmqttproperty Properties[4]; xmqttcodecbuffers Buffers={Properties,4,NULL,0}; xmqttpacket Parsed; xmqttcodecissue Issue;
	if (xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Parsed,&Issue)!=XMQTT_CODEC_DONE ||
		Parsed.Identifier!=42 || Parsed.Body.Publish.Payload.Size!=sizeof(Payload) || memcmp(Parsed.Body.Publish.Payload.Data,Payload,sizeof(Payload))) return 3;
	printf("MQTT5 PUBLISH id=%u topic=%.*s bytes=%zu properties=%zu\n",(unsigned)Parsed.Identifier,(int)Parsed.Body.Publish.Topic.Size,
		Parsed.Body.Publish.Topic.Data,Parsed.Wire.Size,Parsed.Properties.Count); return 0;
}
