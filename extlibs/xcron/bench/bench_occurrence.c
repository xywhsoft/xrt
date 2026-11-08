#include <xcron.h>
#include <xrt/error.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int occurrenceBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
/* Explicit original-public-API composition baseline for LAST+SKIP. It performs
 * full resolution for EACH actual match, and skips nonselected actual instants.
 * This comparator has no gap rejection/budget facility; kind3 uses only LAST. */
static bool occurrenceBenchBaseline(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,xcronoccurrenceissue* I)
{
    uint32 Flags=0;
    for(;;){xtime Actual,Selected;size_t Count;xcrondeliveryissue D;xdatetime Local;
        if(xcronZonedNext(E,Z,Begin,End,Flags,&Actual,NULL)!=XCRON_ZONED_FOUND || !xcronZoneSplit(Z,Actual,&Local,NULL) || xcronDeliveryResolve(Z,&Local,(xcrondeliverypolicy){XCRON_FOLD_LAST,0},&Selected,1,&Count,&D)!=XCRON_DELIVERY_FOUND)return false;
        if(Selected==Actual){*I=(xcronoccurrenceissue){.When=Actual,.Candidates=D.Candidates,.Local=Local};I->Local.Offset=0;I->Local.IsDST=-1;return true;}
        Begin=Actual;Flags=XCRON_SEARCH_EXCLUSIVE;
    }
}
int main(int Argc,char** Argv)
{
    uint32 Batches=Argc>1?(uint32)strtoul(Argv[1],NULL,10):10000,Kind=Argc>2?(uint32)strtoul(Argv[2],NULL,10):0,Budget=Argc>3?(uint32)strtoul(Argv[3],NULL,10):0;
    if(!Batches || Batches>100000 || Kind>3 || (Kind==3 && Budget))return 1;
    xcronzone* Z=NULL;xcronexpression E;double* Samples=NULL;int Exit=2;xtime Begin,End,Expected;uint64 Sum=0,Steps=0,Segments=0,Months=0,Years=0,Checks=0,Offsets=0,Labels=0;
    xstrview Rule=Kind==2?XRT_STR_LITERAL("STD0DST,J60/2,J300/2"):XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0");xstrview Text=Kind==2?XRT_STR_LITERAL("0 30 2 1 MAR *"):Kind==1?XRT_STR_LITERAL("0 30 2 * * *"):XRT_STR_LITERAL("0 30 1 * * *");
    if(!xcronZoneCreatePosix(Rule,XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(Text,XCRON_SECONDS6,&E,NULL) || !(Samples=xrtMalloc(Batches*sizeof(*Samples))))goto Done;
    if(Kind==2){if(!xrtDateTime(2000,1,1,0,0,0,0,&Begin) || !xrtDateTime(2000,3,1,2,0,0,0,&Expected))goto Done;End=INT64_MAX;}
    else if(Kind==1){if(!xrtDateTime(2024,3,10,6,0,0,0,&Begin) || !xrtDateTime(2024,3,10,7,0,0,0,&Expected))goto Done;End=Begin+2*XRT_TIME_DAY;}
    else {if(!xrtDateTime(2024,11,3,5,30,0,0,&Begin) || !xrtDateTime(2024,11,3,6,30,0,0,&Expected))goto Done;End=Expected;}
    double Start=xrtTimer();
    for(uint32 K=0;K<Batches;++K){double T=xrtTimer();
        for(unsigned J=0;J<32;++J){xcronoccurrence Out={.When=777};xcronoccurrenceissue I;xcronoccurrencestatus S;xcrondeliverypolicy Policy=Kind==1 || Kind==2?(xcrondeliverypolicy){0,1}:(xcrondeliverypolicy){2,0};
            if(Kind==3){if(!occurrenceBenchBaseline(&E,Z,Begin,End,&I))goto Done;S=XCRON_OCCURRENCE_FOUND;Out.When=I.When;Out.Local=I.Local;Out.Candidates=I.Candidates;}
            else if(!Budget)S=xcronOccurrenceNext(&E,Z,Begin,End,0,Policy,&Out,&I);
            else {xcronoccurrencecursor C;if(!xcronOccurrenceCursorInit(&C,&E,Z,Begin,End,0,Policy,NULL))goto Done;
                do{S=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){Budget,Budget,Budget,Budget,Budget},&Out,&I);++Steps;}while(S==XCRON_OCCURRENCE_PENDING);
            }
            if(I.When!=Expected || ((Kind==1 || Kind==2)?S!=XCRON_OCCURRENCE_ERROR || I.Code!=XCRON_OCCURRENCE_MISSING || Out.When!=777:S!=XCRON_OCCURRENCE_FOUND || I.Candidates!=2 || Out.When!=Expected))goto Done;
            if(I.Local.Offset || I.Local.IsDST!=-1 || I.Local.Millisecond)goto Done;
            Sum+=(uint64)I.When+I.Candidates+I.Code+(uint64)I.Local.Year+(uint64)(I.Local.Month+I.Local.Day+I.Local.Hour+I.Local.Minute+I.Local.Second+I.Local.YearDay+I.Local.Weekday);Segments+=I.SegmentsExamined;Months+=I.MonthsExamined;Years+=I.RuleYearsExamined;Checks+=I.ChecksExamined;Offsets+=I.OffsetsExamined;Labels+=I.LabelsExamined;
            if(S==XCRON_OCCURRENCE_ERROR)xrtClearError();
        }Samples[K]=(xrtTimer()-T)*1e6;
    }
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))goto Done;qsort(Samples,Batches,sizeof(*Samples),occurrenceBenchCompare);
    printf("queries_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 32\nkind: %u\nbudget: %u\nqueries: %" PRIu64 "\nsteps: %" PRIu64 "\nsegments: %" PRIu64 "\nmonths: %" PRIu64 "\nrule_years: %" PRIu64 "\nchecks: %" PRIu64 "\noffsets: %" PRIu64 "\nlabels: %" PRIu64 "\nchecksum: %" PRIu64 "\n",
        (double)Batches*32/Elapsed,Samples[(Batches-1)/2],Samples[(Batches-1)*95/100],Samples[(Batches-1)*99/100],Samples[(Batches-1)*999/1000],Kind,Budget,(uint64)Batches*32,Steps,Segments,Months,Years,Checks,Offsets,Labels,Sum);Exit=0;
Done:xrtClearError();xrtFree(Samples);xcronZoneRelease(Z);return Exit;
}
