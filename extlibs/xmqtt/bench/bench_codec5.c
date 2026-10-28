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
	const unsigned char Data[]="23.45 degrees"; Packet.Body.Publish.Payload=(xbytesview){Data,sizeof(Data)-1u};
	xmqttproperty Input[2]={0}; Input[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Input[0].Type=XMQTT_PROPERTY_BYTE; Input[0].Value.Number=1;
	Input[1].Id=XMQTT_PROPERTY_USER; Input[1].Type=XMQTT_PROPERTY_STRING_PAIR; Input[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("trace"),XRT_STR_LITERAL("abc")};
	Packet.Properties=(xmqttpropertylist){Input,2}; unsigned char Wire[256], Output[256]; size_t Bytes;
	if (!xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes)) return 3;
	double Start=xrtTimer();
	for (uint64 Index=0; Index<Count; ++Index) {
		xmqttproperty Properties[2]; xmqttcodecbuffers Buffers={Properties,2,NULL,0}; xmqttpacket Parsed; size_t Written;
		if (xmqttPacketRead5((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Parsed,NULL)!=XMQTT_CODEC_DONE ||
			!xmqttPacketWrite5(Output,sizeof(Output),&Parsed,XMQTT_CLIENT_TO_SERVER,NULL,&Written)) return 4;
		Checksum+=Written+Parsed.Identifier+Output[Written-1u];
	}
	double Seconds=xrtTimer()-Start; if (!(Seconds>0)) return 5;
	printf("packets_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
