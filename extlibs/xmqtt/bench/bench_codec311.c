#include <xmqtt.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count=argc>1 ? strtoull(argv[1],NULL,10) : 100000u, Checksum=0;
	if (!Count || Count>1000000000u) return 1;
	xmqttpacket Packet; if (!xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet)) return 2;
	Packet.Flags=2; Packet.Identifier=42; Packet.Body.Publish.Topic=XRT_STR_LITERAL("sensors/temperature");
	const unsigned char Payload[]={0,255,'2','3','.','4','5'}; Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
	unsigned char Wire[256], Output[256]; size_t Bytes;
	if (!xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes)) return 3;
	double Start=xrtTimer();
	for (uint64 Index=0; Index<Count; ++Index) {
		xmqttpacket Parsed; size_t Written;
		if (xmqttPacketRead311((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,NULL)!=XMQTT_CODEC_DONE ||
			!xmqttPacketWrite311(Output,sizeof(Output),&Parsed,XMQTT_CLIENT_TO_SERVER,NULL,&Written)) return 4;
		Checksum+=Written+Parsed.Identifier+Output[Written-1u];
	}
	double Seconds=xrtTimer()-Start; if (!(Seconds>0)) return 5;
	printf("packets_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
