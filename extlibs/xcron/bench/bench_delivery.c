#include <xcron.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int deliveryBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Batches=Argc>1?(uint32)strtoul(Argv[1],NULL,10):10000,Path=Argc>2?(uint32)strtoul(Argv[2],NULL,10):1;
    if(!Batches || Batches>100000 || Path>3)return 1;
    xcronzone* Z=NULL;double* Samples=NULL;int Exit=2;uint64 Sum=0;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !(Samples=xrtMalloc(Batches*sizeof(*Samples))))goto Done;
    xdatetime Local={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30,.Millisecond=137};xtime Values[2];size_t N;
    double Start=xrtTimer();
    for(uint32 I=0;I<Batches;++I){double T=xrtTimer();
        for(unsigned J=0;J<32;++J){bool OK;
            if(!Path)OK=xcronZoneResolve(Z,&Local,Values,2,&N,NULL)==XCRON_ZONE_FOUND;
            else OK=xcronDeliveryResolve(Z,&Local,(xcrondeliverypolicy){Path==1?XCRON_FOLD_ALL:Path==2?XCRON_FOLD_FIRST:XCRON_FOLD_LAST,XCRON_MISSING_SKIP},Values,2,&N,NULL)==XCRON_DELIVERY_FOUND;
            if(!OK || N!=(Path<2?2u:1u))goto Done;
            Sum+=(uint64)Values[0]+N;
        }Samples[I]=(xrtTimer()-T)*1e6;
    }
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))goto Done;qsort(Samples,Batches,sizeof(*Samples),deliveryBenchCompare);
    printf("labels_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 32\npath: %u\nchecksum: %" PRIu64 "\n",
        (double)Batches*32/Elapsed,Samples[(Batches-1)/2],Samples[(Batches-1)*95/100],Samples[(Batches-1)*99/100],Samples[(Batches-1)*999/1000],Path,Sum);Exit=0;
Done:
    xrtFree(Samples);xcronZoneRelease(Z);return Exit;
}
