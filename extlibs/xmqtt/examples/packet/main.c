#include <xmqtt.h>
#include <stdio.h>

int main(void)
{
	xmqttpacket Packet; xmqttcodecconfig Config;
	if (!xmqttPacketInit(XMQTT_PACKET_PUBREL,&Packet) || !xmqttCodecConfigInit(&Config)) return 1;
	Packet.Identifier=42;
	printf("PUBREL flags=%u identifier=%u packet-cap=%zu\n",(unsigned)Packet.Flags,(unsigned)Packet.Identifier,Config.MaxPacketBytes);
	return 0;
}
