#include <xmqtt.h>
#include "../src/internal/xmqtt_internal.h"
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Measures the private ASCII filter grammar helper only. Caller has already
 * checked MQTT UTF-8/length. This is not a subscribe/codec/network benchmark. */
int main(int argc, char** argv)
{
	uint64 Count=argc>1 ? strtoull(argv[1],NULL,10) : 100000u, Checksum=0;
	if (!Count || Count>1000000000u) return 1;
	const xstrview Filters[]={XRT_STR_LITERAL("devices/+/status"),XRT_STR_LITERAL("$share/workers/tasks/#"),XRT_STR_LITERAL("invalid+filter"),XRT_STR_LITERAL("a//b/#")};
	double Start=xrtTimer();
	for (uint64 Index=0; Index<Count; ++Index) { bool Shared; bool Valid=xmqttInternalTopic(Filters[Index%4u],true,true,&Shared); Checksum+=(Valid ? 1u : 0u)+(Shared ? 3u : 0u); }
	double Seconds=xrtTimer()-Start; if (!(Seconds>0)) return 2;
	printf("filters_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
