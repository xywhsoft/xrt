#include <xmqtt.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count=argc>1 ? strtoull(argv[1],NULL,10) : 100000u, Checksum=0;
	if (!Count || Count>1000000000u) return 1;
	xmqttproperty Input[3]={0}; Input[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Input[0].Type=XMQTT_PROPERTY_BYTE; Input[0].Value.Number=1;
	Input[1].Id=XMQTT_PROPERTY_USER; Input[1].Type=XMQTT_PROPERTY_STRING_PAIR;
	Input[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("trace"),XRT_STR_LITERAL("abc")};
	Input[2].Id=XMQTT_PROPERTY_SUBSCRIPTION_ID; Input[2].Type=XMQTT_PROPERTY_VARIABLE; Input[2].Value.Number=7;
	unsigned char Wire[128], Output[128]; size_t Bytes;
	if (!xmqttPropertiesWrite(Wire,sizeof(Wire),Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Bytes)) return 2;
	double Start=xrtTimer();
	for (uint64 Index=0; Index<Count; ++Index) {
		xmqttproperty Parsed[3]; xmqttpropertydocument Doc; size_t Written;
		if (xmqttPropertiesRead((xbytesview){Wire,Bytes},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,3,&Doc)!=XMQTT_PROPERTIES_DONE ||
			!xmqttPropertiesWrite(Output,sizeof(Output),Parsed,Doc.Count,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written)) return 3;
		Checksum+=Written+Doc.Count+Parsed[2].Value.Number+Output[0];
	}
	double Seconds=xrtTimer()-Start; if (!(Seconds>0.0)) return 4;
	printf("sections_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
