#include <xcron.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int deliveryCursorBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Batches=Argc>1?(uint32)strtoul(Argv[1],NULL,10):10000,Mode=Argc>2?(uint32)strtoul(Argv[2],NULL,10):0;if(!Batches || Batches>100000 || Mode>2)return 1;
    xcronzone* Z=NULL;double* Samples=NULL;int Exit=2;uint64 Sum=0,Steps=0,Checks=0,Offsets=0;xcrondeliverycursor C;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !(Samples=xrtMalloc(Batches*sizeof(*Samples))))goto Done;
    xdatetime Local={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30,.Millisecond=137};double Start=xrtTimer();
    for(uint32 I=0;I<Batches;++I){double T=xrtTimer();
        for(unsigned J=0;J<32;++J){xtime Out[2];size_t Count=777;xcrondeliverystatus S;
            if(Mode==2)S=xcronDeliveryResolve(Z,&Local,(xcrondeliverypolicy){0,0},Out,2,&Count,NULL);
            else {xcrondeliverycursorissue Work;if(!xcronDeliveryCursorInit(&C,Z,&Local,(xcrondeliverypolicy){0,0},NULL))goto Done;
                do {S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,Mode?1u:258u,403},Out,2,&Count,&Work);++Steps;}while(S==XCRON_DELIVERY_PENDING);
                if(Work.ChecksExamined!=1 || Work.OffsetsExamined!=2 || Work.RuleYearsExamined || Work.Issue.Candidates!=2)goto Done;
                Checks+=Work.ChecksExamined;Offsets+=Work.OffsetsExamined;
            }
            if(S!=XCRON_DELIVERY_FOUND || Count!=2 || Out[1]-Out[0]!=3600000)goto Done;
            Sum+=(uint64)Out[0]+Count;
        }Samples[I]=(xrtTimer()-T)*1e6;
    }
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))goto Done;qsort(Samples,Batches,sizeof(*Samples),deliveryCursorBenchCompare);
    printf("labels_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 32\nmode: %u\nsteps: %" PRIu64 "\nchecks: %" PRIu64 "\noffsets: %" PRIu64 "\nchecksum: %" PRIu64 "\n",
        (double)Batches*32/Elapsed,Samples[(Batches-1)/2],Samples[(Batches-1)*95/100],Samples[(Batches-1)*99/100],Samples[(Batches-1)*999/1000],Mode,Steps,Checks,Offsets,Sum);Exit=0;
Done:
    xrtFree(Samples);xcronZoneRelease(Z);return Exit;
}
