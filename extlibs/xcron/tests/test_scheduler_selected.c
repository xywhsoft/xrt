#include <xcron.h>
#include <xrt/memory.h>
#include <xrt/error.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#include "test.h"
#include "tzif_fixture.h"
#include <limits.h>
static xtime selectedStamp(int Y,int M,int D,int H,int Minute)
{xtime T;testRequire(xrtDateTime(Y,M,D,H,Minute,0,0,&T),"selected scheduler stamp");return T;}
static xcronzone* selectedZone(cstr Rule)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Rule,strlen(Rule)},XCRON_ZONE_TZIF3,&Z,NULL),"selected zone");return Z;}
static xcronjobconfig selectedConfig(xcronzone* Z,cstr Text,xtime Begin,xtime End)
{xcronjobconfig C={.Zone=Z,.Begin=Begin,.End=End,.RunningLimit=1};testRequire(xcronExpressionCompile((xstrview){Text,strlen(Text)},XCRON_SECONDS6,&C.Expression,NULL),"selected expression");return C;}
static xcronscheduler* selectedCreate(uint32 Jobs,uint32 Runs,uint32 Work)
{xcronscheduler* S=NULL;xcronschedulerconfig C={Jobs,Runs,Work,0,SIZE_MAX};testRequire(xcronSchedulerCreateSelected(&C,&S,NULL),"selected kernel factory");return S;}
static xcronjobid selectedAdd(xcronscheduler* S,const xcronjobconfig* C,xcrondeliverypolicy P)
{xcronjobid Id;testRequire(xcronSchedulerAddSelected(S,C,P,&Id,NULL),"bounded selected admission");return Id;}
static xcronselectedjobinfo selectedInfo(xcronscheduler* S,xcronjobid Id)
{xcronselectedjobinfo I;testRequire(xcronSchedulerJobSelected(S,Id,&I,NULL),"selected job info");return I;}
static xcronselectedpoll selectedPoll(xcronscheduler* S,xtime Wall,uint64 Mono,xcronoccurrencebudget B,xcronselectedrun* R,size_t Capacity,uint32 Work)
{xcronselectedpoll P;testRequire(xcronSchedulerPollSelected(S,(xcronclock){Wall,Mono},B,R,Capacity,&P,NULL),"selected budgeted poll");testRequire(P.Poll.Work<=Work && P.Poll.Count<=Capacity && P.Used.Segments<=B.Segments && P.Used.Months<=B.Months && P.Used.RuleYears<=B.RuleYears && P.Used.Checks<=B.Checks && P.Used.Offsets<=B.Offsets,"all five global traversal and owner/output bounds");return P;}
static xcronselectedretired selectedReap(xcronscheduler* S)
{xcronselectedretired R;bool Found=false;testRequire(xcronSchedulerReapSelected(S,&R,&Found,NULL) && Found,"selected retirement");return R;}
static void selectedClean(xcronscheduler* S)
{testRequire(xcronSchedulerClose(S,NULL),"selected close");xcronselectedretired R;bool Found=true;while(Found)testRequire(xcronSchedulerReapSelected(S,&R,&Found,NULL),"selected drain");testRequire(xcronSchedulerDestroy(S,NULL),"selected destroy");}
static void testSelectedFoldAndGap(void)
{
    xcronzone* Z=selectedZone("EST5EDT,M3.2.0,M11.1.0");xtime A=selectedStamp(2024,11,3,5,30),B=A+XRT_TIME_HOUR;
    for(int Fold=0;Fold<4;++Fold)for(int Missing=0;Missing<2;++Missing){xcronscheduler* S=selectedCreate(1,2,4);xcronjobconfig C=selectedConfig(Z,"0 30 1 * * *",A,B);C.Overlap=XCRON_OVERLAP_ALLOW;C.RunningLimit=2;int Data=17;C.Data=&Data;
        xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){Fold,Missing});C.Expression.Fields[0]=0;xcronselectedjobinfo Info=selectedInfo(S,Id);testRequire(Info.State==XCRON_JOB_SEARCHING && !Info.HasNext && !Info.LastIssue.SegmentsExamined,"admission snapshots without recurrence traversal");
        xcronselectedrun R[2];unsigned Count=0,Faults=0,Turns=0;
        do{xcronselectedpoll P=selectedPoll(S,B,Turns,(xcronoccurrencebudget){1,1,1,1,1},R+Count,2-Count,4);Count+=(unsigned)P.Poll.Count;Faults+=P.Poll.Faults;Info=selectedInfo(S,Id);testRequire(++Turns<256,"finite selected fold scheduling");}while(Info.State==XCRON_JOB_SEARCHING || Info.State==XCRON_JOB_SCHEDULED);
        unsigned Expected=Fold==XCRON_FOLD_ALL?2:Fold==XCRON_FOLD_REJECT?0:1;testRequire(Count==Expected && Faults==(unsigned)(Fold==XCRON_FOLD_REJECT),"independent global fold selection in actual leases");
        for(unsigned I=0;I<Count;++I){testRequire(R[I].Run.Job==Id && R[I].Run.Data==&Data && R[I].Run.ScheduledAt==(Fold==XCRON_FOLD_LAST?B:A+(xtime)I*XRT_TIME_HOUR) && R[I].Candidates==2 && R[I].Local.Hour==1 && R[I].Local.Minute==30 && R[I].Local.Offset==0 && R[I].Local.IsDST==-1,"lease snapshots canonical nominal metadata before successor mutation");testRequire(xcronSchedulerComplete(S,R[I].Run.Id,XCRON_RUN_SUCCEEDED,NULL),"selected physical completion");}
        xcronselectedretired Retired=selectedReap(S);testRequire(Retired.Id==Id && Retired.Data==&Data && Retired.Info.Succeeded==Count && Retired.Info.State==(Fold==XCRON_FOLD_REJECT?XCRON_JOB_FAULTED:XCRON_JOB_EXHAUSTED),"selected terminal receipt");
        if(Fold==XCRON_FOLD_REJECT)testRequire(Retired.Info.LastIssue.Code==XCRON_OCCURRENCE_AMBIGUOUS && Retired.Info.LastIssue.When==A && Retired.Info.LastIssue.Candidates==2,"typed initial fold fault is retained without admission rejection");
        selectedClean(S);
    }
    xtime Gap=selectedStamp(2024,3,10,7,0);xcronscheduler* S=selectedCreate(1,1,1);xcronjobconfig C=selectedConfig(Z,"0 30 2 * * *",Gap,Gap);selectedAdd(S,&C,(xcrondeliverypolicy){0,1});unsigned Faults=0;
    for(unsigned T=0;T<64;++T){xcronselectedpoll P=selectedPoll(S,Gap,T,(xcronoccurrencebudget){1,1,1,1,1},NULL,0,1);Faults+=P.Poll.Faults;if(!P.Searching)break;testRequire(T<63,"bounded gap retirement");}
    xcronselectedretired Retired=selectedReap(S);testRequire(Faults==1 && Retired.Info.LastIssue.Code==XCRON_OCCURRENCE_MISSING && Retired.Info.LastIssue.When==Gap && Retired.Info.LastIssue.Local.Hour==2 && Retired.Info.LastIssue.Local.Minute==30,"missing tie rejects before any actual lease and preserves full issue");selectedClean(S);xcronZoneRelease(Z);
}
#include "scheduler_selected_cases.h"
#include "scheduler_selected_model.h"
int main(void)
{testSelectedFoldAndGap();testSelectedFaultLifetime();testSelectedBudgetsAndFairness();testSelectedArgumentsAndWarm();testSelectedPolicyParity();testSelectedEndpointsAndDiscard();testSelectedHistoryModel();
#if defined(XRT_FEATURE_THREAD)
testSelectedHandoff();
#endif
puts("[PASS] selected scheduling: global fold/gap policy, five budgets, fairness,2048 independent history traces, OOM and physical lease receipts");return 0;}
