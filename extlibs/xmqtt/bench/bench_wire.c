#include <xmqtt.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1],NULL,10) : 100000u, Checksum = 0;
	if (Count == 0 || Count > 1000000000u) return 1;
	static const unsigned char Input[] = {0x32,0x0f,0,3,'a','/','b',0,1,0,'p','a','y','l','o','a','d'};
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		xmqttframe Frame; xstrview Topic; size_t Read, Written; unsigned char Output[5];
		if (xmqttFrameRead((xbytesview){Input,sizeof(Input)},NULL,&Frame) != XMQTT_WIRE_DONE ||
			xmqttUtf8StringRead(Frame.Body,0,&Topic,&Read) != XMQTT_WIRE_DONE ||
			!xmqttHeaderWrite(Output,sizeof(Output),Frame.Header.Type,Frame.Header.Flags,Frame.Header.RemainingLength,NULL,&Written)) return 2;
		Checksum += Frame.Wire.Size + Topic.Size + Read + Written + Output[0];
	}
	double Seconds = xrtTimer()-Start; if (!(Seconds > 0.0)) return 3;
	printf("frames_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
