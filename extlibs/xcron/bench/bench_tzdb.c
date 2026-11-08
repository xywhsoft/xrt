#include <xcron.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static int tzdbBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    static const uint8 UTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
    uint32 Batches=Argc>1?(uint32)strtoul(Argv[1],NULL,10):10000,Names=Argc>2?(uint32)strtoul(Argv[2],NULL,10):1024;
    bool Acquire=Argc>3 && strtoul(Argv[3],NULL,10)!=0;if(!Batches || Batches>100000 || !Names || Names>1024)return 1;
    char Text[1024][24];xcrontzdbentry Entries[1024];
    for(uint32 I=0;I<Names;++I){int N=snprintf(Text[I],sizeof(Text[I]),"Zone/Entry_%04u",I);if(N<=0 || (size_t)N>=sizeof(Text[I]))return 2;Entries[I]=(xcrontzdbentry){{Text[I],(size_t)N},{UTC,sizeof(UTC)}};}
    xcrontzdb* D=NULL;xcrontzdbsnapshot* S=NULL;double* Samples=NULL;uint64 R=0,Sum=0;int Exit=2;xcrontzdbconfig C={1024,1,64*1024*1024};
    if(!xcronTzdbCreate(&C,&D,NULL) || !xcronTzdbLoad(D,XRT_STR_LITERAL("benchmark"),Entries,Names,0,&R,NULL) || !xcronTzdbAcquire(D,&S,NULL) || !(Samples=xrtMalloc((size_t)Batches*sizeof(*Samples))))goto Done;
    double Start=xrtTimer();
    for(uint32 I=0;I<Batches;++I){double T=xrtTimer();
        for(uint32 J=0;J<128;++J){uint32 Index=((I*128+J)*131)%Names;xcrontzdbsnapshot* View=S;xcronzone* Z=NULL;
            if(Acquire && !xcronTzdbAcquire(D,&View,NULL))goto Done;
            xcrontzdbstatus Status=xcronTzdbFind(View,Entries[Index].Name,&Z,NULL);
            if(Acquire)xcronTzdbSnapshotRelease(View);
            if(Status!=XCRON_TZDB_FOUND)goto Done;
            Sum+=Index+1;xcronZoneRelease(Z);
        }
        Samples[I]=(xrtTimer()-T)*1e6;
    }
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))goto Done;qsort(Samples,Batches,sizeof(*Samples),tzdbBenchCompare);
    printf("lookups_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 128\nnames: %u\nacquire_per_lookup: %u\nchecksum: %" PRIu64 "\n",
        (double)Batches*128/Elapsed,Samples[(Batches-1)/2],Samples[(Batches-1)*95/100],Samples[(Batches-1)*99/100],Samples[(Batches-1)*999/1000],Names,Acquire?1u:0u,Sum);Exit=0;
Done:
    xrtFree(Samples);xcronTzdbSnapshotRelease(S);if(D && (!xcronTzdbClose(D,NULL) || !xcronTzdbDestroy(D,NULL)))Exit=3;
    xrtClearError();return Exit;
}
