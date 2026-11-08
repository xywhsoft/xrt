#include <xcron.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put32(uint8* P,uint32 N){for(unsigned I=0;I<4;++I)P[I]=(uint8)(N>>(24-8*I));}
int main(int Argc,char** Argv)
{
    const uint32 Entries=4096;uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):1000000,Sum=0;
    if(!Count || Count>1000000000)return 1;
    size_t Bytes=51+44+(size_t)Entries*9+12+8+6;uint8* Data=xrtCalloc(1,Bytes);if(!Data)return 2;
    memcpy(Data,"TZif3",5);put32(Data+36,1);put32(Data+40,1);uint8* H=Data+51;memcpy(H,"TZif3",5);put32(H+32,Entries);put32(H+36,2);put32(H+40,8);
    uint8* Times=H+44;uint8* Indices=Times+(size_t)Entries*8;uint8* Types=Indices+Entries;uint8* Names=Types+12;
    for(uint32 I=0;I<Entries;++I){uint64 S=(uint64)I*86400;for(unsigned K=0;K<8;++K)Times[(size_t)I*8+K]=(uint8)(S>>((7-K)*8));Indices[I]=(uint8)(1-(I&1));}
    put32(Types+6,3600);Types[11]=4;memcpy(Names,"AAA\0BBB\0",8);memcpy(Names+8,"\nAAA0\n",6);
    xcronzone* Zone=NULL;bool OK=xcronZoneCreateTzif((xbytesview){Data,Bytes},&Zone,NULL);xrtFree(Data);if(!OK)return 3;
    double Start=xrtTimer();for(uint64 I=0;I<Count;++I){xcronzonestate S;xtime T=XRT_TIME_UNIX_EPOCH+(xtime)((I*9973)%((uint64)Entries*86400))*1000;
        if(!xcronZoneAt(Zone,T,&S,NULL)){xcronZoneRelease(Zone);return 4;}Sum+=(uint64)S.Offset;}
    double Elapsed=xrtTimer()-Start;xcronZoneRelease(Zone);if(!(Elapsed>0))return 5;
    printf("lookups_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Sum);return 0;
}
