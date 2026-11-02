#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):1000000,Sum=0;if(!Count || Count>1000000000)return 1;xcronzone* Z=NULL;
    xtime Begin;
    if(!xrtDate(2024,1,1,&Begin) || !xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL))return 2;
    double Start=xrtTimer();for(uint64 I=0;I<Count;++I){xcronzonestate S;xtime T=Begin+(xtime)((I*9973)%31622400)*1000;if(!xcronZoneAt(Z,T,&S,NULL))return 3;Sum+=(uint64)(int64)S.Offset+(S.IsDST?1u:0u);}
    double Seconds=xrtTimer()-Start;xcronZoneRelease(Z);if(!(Seconds>0))return 4;printf("lookups_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Sum);return 0;
}
