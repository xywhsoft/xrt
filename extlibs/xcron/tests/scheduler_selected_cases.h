static void testSelectedFaultLifetime(void)
{
    /* The first label's complete legal-offset window ends one second before
     * UNKNOWN coverage. Its successor reaches UNKNOWN even though its actual
     * UTC mapping is known: global completeness may never be waived. */
    tzifblob Blob;int64 Times[]={-1,(int64)INT32_MAX+1};uint8 Indices[]={0,0};tzifkind Types[]={{0,false,"UTC"}};tzifBuild(&Blob,'3',Times,Indices,2,Types,1,"");xcronzone* Z=NULL;
    testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"selected future-global-unknown fixture");xcronscheduler* S=selectedCreate(1,1,4);int Data=83;
    xcronjobconfig C=selectedConfig(Z,"* * * * * *",XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+2000);C.Data=&Data;xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){0,0});xcronZoneRelease(Z);
    xcronselectedrun Run={0};bool Published=false;unsigned Faults=0;
    for(unsigned T=0;T<256;++T){xcronselectedpoll P=selectedPoll(S,C.Begin+2000,T,(xcronoccurrencebudget){1,1,1,1,1},Published?NULL:&Run,Published?0:1,4);Published|=P.Poll.Count!=0;Faults+=P.Poll.Faults;if(Faults)break;testRequire(T<255,"future selected fault terminates bounded search");}
    xcronselectedjobinfo Info=selectedInfo(S,Id);testRequire(Published && Faults==1 && Info.State==XCRON_JOB_FAULTED && Info.Running==1 && Info.LastIssue.Code==XCRON_OCCURRENCE_ZONE && Info.LastIssue.Zone==XCRON_ZONE_UNKNOWN && Info.LastIssue.When==C.Begin+1000,"future global-coverage fault preserves the published physical lease");
    xcronselectedretired R={.Id=777};bool Found=true;testRequire(xcronSchedulerCancel(S,Id,NULL) && xcronSchedulerClose(S,NULL) && xcronSchedulerReapSelected(S,&R,&Found,NULL) && !Found && R.Id==777 && Data==83,"cancel/close preserve fault reason and cannot reap borrowed active data");
    xcronschedulerissue Issue;testRequire(!xcronSchedulerDestroy(S,&Issue) && Issue.Code==XCRON_SCHEDULER_BUSY,"physical lease keeps the selected kernel live");xrtClearError();
    testRequire(xcronSchedulerComplete(S,Run.Run.Id,XCRON_RUN_FAILED,NULL),"physical faulted-job completion");R=selectedReap(S);testRequire(R.Id==Id && R.Data==&Data && R.Info.Failed==1 && R.Info.LastIssue.Code==XCRON_OCCURRENCE_ZONE,"retirement retains full selected fault and original Data");
    testRequire(!xcronSchedulerComplete(S,Run.Run.Id,XCRON_RUN_FAILED,&Issue) && Issue.Code==XCRON_SCHEDULER_STALE,"duplicate selected completion is stale");xrtClearError();testRequire(xcronSchedulerDestroy(S,NULL),"drained selected owner destroys");
}
static void testSelectedBudgetsAndFairness(void)
{
    xcronzone* Z=selectedZone("EST5EDT,M3.2.0,M11.1.0");xtime A=selectedStamp(2024,11,3,5,30);
    for(unsigned Dim=0;Dim<5;++Dim){xcronscheduler* S=selectedCreate(1,1,1);xcronjobconfig C=selectedConfig(Z,"0 30 1 * * *",A,A+XRT_TIME_HOUR);xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){2,1});
        xcronoccurrencebudget B={1,1,1,1,1};if(Dim==0)B.Segments=0;else if(Dim==1)B.Months=0;else if(Dim==2)B.RuleYears=0;else if(Dim==3)B.Checks=0;else B.Offsets=0;
        for(unsigned T=0;T<32;++T)selectedPoll(S,A,T,B,NULL,0,1);
        xcronselectedjobinfo Info=selectedInfo(S,Id);testRequire(Info.State==XCRON_JOB_SEARCHING && !Info.HasNext && !Info.LastIssue.When && !Info.LastIssue.Candidates && !Info.LastIssue.Local.Year,"each independently absent needed dimension yields without provisional payload");
        testRequire(xcronSchedulerCancel(S,Id,NULL),"pending selected cancellation");selectedReap(S);selectedClean(S);
    }xcronZoneRelease(Z);
    Z=selectedZone("UTC0");xcronscheduler* S=selectedCreate(2,1,3);xcronjobconfig Hot=selectedConfig(Z,"* * * * * *",0,INT64_MAX);selectedAdd(S,&Hot,(xcrondeliverypolicy){0,0});
    xcronjobconfig Sparse=selectedConfig(Z,"0 0 0 29 FEB */7",selectedStamp(2024,3,1,0,0),selectedStamp(2040,1,1,0,0));xcronjobid Later=selectedAdd(S,&Sparse,(xcrondeliverypolicy){0,0});
    xcronselectedpoll Zero=selectedPoll(S,0,0,(xcronoccurrencebudget){0},NULL,0,3);testRequire(!Zero.Poll.Work && Zero.Searching==2,"all-zero calculation-only poll commits only the clock");
    unsigned T=0;while(selectedInfo(S,Later).State==XCRON_JOB_SEARCHING){xcronselectedrun R;xcronselectedpoll P=selectedPoll(S,100000,T,(xcronoccurrencebudget){1,1,1,1,1},&R,1,3);if(P.Poll.Count)testRequire(xcronSchedulerComplete(S,R.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"dense fair completion");testRequire(++T<1024,"older sparse query cannot starve beside infinite ready catch-up");}
    testRequire(selectedInfo(S,Later).Next==selectedStamp(2032,2,29,0,0),"fair sparse query produces its independently known leap-day result");selectedClean(S);xcronZoneRelease(Z);
    Z=selectedZone("STD0DST,J60/2,J300/2");xcronzone* UTC=selectedZone("UTC0");S=selectedCreate(9,1,1);xcronjobconfig Heavy=selectedConfig(Z,"0 30 2 1 MAR *",selectedStamp(2000,1,1,0,0),INT64_MAX);
    for(unsigned I=0;I<8;++I)selectedAdd(S,&Heavy,(xcrondeliverypolicy){0,0});
    xcronjobconfig Light=selectedConfig(UTC,"* * * * * *",Heavy.Begin,Heavy.Begin);xcronjobid Id=selectedAdd(S,&Light,(xcrondeliverypolicy){0,0});bool Seen=false;
    for(unsigned Turn=0;Turn<128 && !Seen;++Turn){xcronselectedrun R;xcronselectedpoll P=selectedPoll(S,Heavy.Begin,Turn,(xcronoccurrencebudget){1,1,1,1,1},&R,1,1);if(P.Poll.Count){testRequire(R.Run.Job==Id && xcronSchedulerComplete(S,R.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"light job dispatches beside eight400-year proofs");Seen=true;}}
    testRequire(Seen,"ordered heavy searches cannot starve a later ready label");xcronwake W;testRequire(xcronSchedulerWake(S,(xcronclock){Heavy.Begin,128},1000,&W,NULL) && W.Immediate,"pending selected work has a useful immediate wake");
    uint64 Traversed=0;for(unsigned Turn=0;Turn<16;++Turn){xcronselectedpoll P=selectedPoll(S,Heavy.Begin,128+Turn,(xcronoccurrencebudget){1,1,1,1,1},NULL,0,1);Traversed+=P.Used.Segments+P.Used.Months+P.Used.RuleYears+P.Used.Checks+P.Used.Offsets;}
    testRequire(Traversed>0,"zero-output selected polls perform real bounded work");selectedClean(S);xcronZoneRelease(UTC);xcronZoneRelease(Z);
}
static void testSelectedArgumentsAndWarm(void)
{
    xcronscheduler* S=selectedCreate(1,1,8);xcronzone* Z=selectedZone("UTC0");xcronjobconfig C=selectedConfig(Z,"* * * * * *",0,0);xcronjobid Id=777;xcronschedulerissue I,Old;memset(&I,0xa5,sizeof(I));Old=I;
    testRequire(!xcronSchedulerAddSelected(S,&C,(xcrondeliverypolicy){4,0},&Id,&I) && Id==777 && !memcmp(&I,&Old,sizeof(I)),"invalid policy preserves every output");xrtClearError();
    testRequire(!xcronSchedulerAdd(S,&C,&Id,&I) && Id==777 && !memcmp(&I,&Old,sizeof(I)),"actual Add cannot silently lose selection policy");xrtClearError();
    xcronselectedpoll P,Previous;memset(&P,0x5a,sizeof(P));Previous=P;testRequire(!xcronSchedulerPollSelected(S,(xcronclock){0,0},(xcronoccurrencebudget){1,1,1,1,1},(xcronselectedrun*)&P,1,&P,&I) && !memcmp(&P,&Previous,sizeof(P)) && !memcmp(&I,&Old,sizeof(I)),"selected report/run aliases are atomic");xrtClearError();
    xcronpollbudgeted Actual;testRequire(!xcronSchedulerPollBudgeted(S,(xcronclock){0,0},(xcronzonedbudget){1,1,1},NULL,0,&Actual,&I) && !memcmp(&I,&Old,sizeof(I)),"actual budget API cannot omit selected check/offset accounting");xrtClearError();
    xcronschedulerstats Stats;testRequire(xcronSchedulerInspect(S,&Stats,NULL),"selected byte accounting");xcronschedulerconfig Config={1,1,8,0,Stats.Bytes-1};xcronscheduler* Out=NULL;
    testRequire(!xcronSchedulerCreateSelected(&Config,&Out,&I) && I.Code==XCRON_SCHEDULER_LIMIT && !Out,"complete selected query arena is inside BytesLimit");xrtClearError();Config.BytesLimit=Stats.Bytes;testRequire(xcronSchedulerCreateSelected(&Config,&Out,NULL),"exact selected BytesLimit succeeds");selectedClean(Out);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"selected allocation quota");
    testRequire(!xcronSchedulerCreateSelected(&Config,&Out,&I) && I.Code==XCRON_SCHEDULER_MEMORY,"typed selected factory failure survives diagnostic OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave selected factory OOM");xrtClearError();
    xrtSetErrorInfo(XERR_VALUE,"selected.ambient",33,"preserve identity");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"warm selected quota");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    xcronjobid PreviousId=0;
    for(unsigned T=0;T<1024;++T){Id=selectedAdd(S,&C,(xcrondeliverypolicy){T%4,T%2});testRequire(Id!=PreviousId,"selected slot generation changes");PreviousId=Id;xcronselectedrun R;bool Seen=false;
        for(unsigned N=0;N<8 && !Seen;++N){xcronselectedpoll Q=selectedPoll(S,0,T,(xcronoccurrencebudget){1,1,1,1,1},&R,1,8);Seen=Q.Poll.Count!=0;}
        testRequire(Seen && R.Run.ScheduledAt==0 && R.Candidates==1 && xcronSchedulerComplete(S,R.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"warm selected complete cycle");selectedReap(S);testRequire(xrtGetError()==Ambient,"selected normal lifecycle preserves ambient error identity");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"selected steady lifecycle requests no heap");
#endif
    testRequire(!Scope.Rejections && xrtAllocationBudgetLeave(&Scope),"selected warm lifecycle rejects no allocation");xrtClearError();xrtAllocationBudgetFree(Budget);selectedClean(S);xcronZoneRelease(Z);
}
static void testSelectedPolicyParity(void)
{
    /* Independent expected finite UTC due lists; delayed physical completion
     * drives WAIT/ALLOW/SKIP, each with all three misfire policies. */
    xcronzone* Z=selectedZone("UTC0");
    for(int Misfire=0;Misfire<3;++Misfire)for(int Overlap=0;Overlap<3;++Overlap){xcronscheduler* S=selectedCreate(1,2,16);xcronjobconfig C=selectedConfig(Z,"* * * * * *",0,10000);C.Misfire=Misfire;C.Overlap=Overlap;C.RunningLimit=Overlap==XCRON_OVERLAP_ALLOW?2:1;C.GraceMilliseconds=1000;xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){0,0});
        xcronselectedrun R[2];xcronselectedpoll P=selectedPoll(S,5500,0,(xcronoccurrencebudget){64,64,64,64,64},R,1,16);testRequire(P.Poll.Count==1 && R[0].Run.ScheduledAt==(Misfire==XCRON_MISFIRE_SKIP?5000:0) && R[0].Run.Through==(Misfire==XCRON_MISFIRE_COALESCE?5500:R[0].Run.ScheduledAt),"selected UTC policies match independently enumerated initial misfire result");
        xcronrun First=R[0].Run;P=selectedPoll(S,7000,1,(xcronoccurrencebudget){64,64,64,64,64},R+1,1,16);
        testRequire(P.Poll.Count==(size_t)(Overlap==XCRON_OVERLAP_ALLOW),"selected overlap uses physical running leases");
        testRequire(xcronSchedulerComplete(S,First.Id,XCRON_RUN_SUCCEEDED,NULL),"delayed first physical completion");if(P.Poll.Count)testRequire(xcronSchedulerComplete(S,R[1].Run.Id,XCRON_RUN_SUCCEEDED,NULL),"allowed second physical completion");
        xcronselectedjobinfo Info=selectedInfo(S,Id);testRequire(Info.SkippedRanges==(uint64)(Overlap==XCRON_OVERLAP_SKIP)+(uint64)(Misfire==XCRON_MISFIRE_SKIP),"misfire skip and overlap skip each contribute their independently evaluated discarded range");
        P=selectedPoll(S,-1000,2,(xcronoccurrencebudget){64,64,64,64,64},R,2,16);testRequire(!P.Poll.Count,"wall rollback never replays consumed selected labels");selectedClean(S);
    }xcronZoneRelease(Z);
}
static void testSelectedEndpointsAndDiscard(void)
{
    xcronzone* Z=selectedZone("UTC0");
    for(unsigned High=0;High<2;++High){xcronscheduler* S=selectedCreate(1,1,8);xtime Begin=High?INT64_MAX-1000:INT64_MIN,End=High?INT64_MAX:INT64_MIN+1000;
        xcronjobconfig C=selectedConfig(Z,"* * * * * *",Begin,End);selectedAdd(S,&C,(xcrondeliverypolicy){0,1});xcronselectedrun R;bool Seen=false;
        for(unsigned T=0;T<32 && !Seen;++T){xcronselectedpoll P=selectedPoll(S,End,T,(xcronoccurrencebudget){1,1,1,1,1},&R,1,8);Seen=P.Poll.Count!=0;}
        testRequire(Seen && R.Run.ScheduledAt==(High?INT64_MAX-807:INT64_MIN+808) && R.Candidates==1 && xcronSchedulerComplete(S,R.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"selected scheduler preserves asymmetric full-int64 rounding and physical completion");selectedClean(S);
    }
    xcronscheduler* S=selectedCreate(1,1,4);xcronjobconfig C=selectedConfig(Z,"* * * * * *",0,1000);xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){0,0});
    for(unsigned T=0;selectedInfo(S,Id).State==XCRON_JOB_SEARCHING;++T){selectedPoll(S,0,0,(xcronoccurrencebudget){1,1,1,1,1},NULL,0,4);testRequire(T<16,"prepare known zero-budget lease");}
    xcronselectedrun Run;xcronselectedpoll Zero=selectedPoll(S,0,0,(xcronoccurrencebudget){0},&Run,1,4);testRequire(Zero.Poll.Count==1 && Zero.Searching==1 && !Zero.Used.Segments && !Zero.Used.Months && !Zero.Used.RuleYears && !Zero.Used.Checks && !Zero.Used.Offsets,"known lease publishes at zero query budget with successor deferred");testRequire(xcronSchedulerComplete(S,Run.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"zero-budget selected physical completion");selectedClean(S);xcronZoneRelease(Z);
    Z=selectedZone("EST5EDT,M3.2.0,M11.1.0");xtime First=selectedStamp(2024,3,9,7,30),Gap=selectedStamp(2024,3,10,7,0),Wall=selectedStamp(2024,3,10,12,0),Last=selectedStamp(2024,3,11,6,30);
    for(unsigned Coalesce=0;Coalesce<2;++Coalesce){S=selectedCreate(1,1,128);C=selectedConfig(Z,"0 30 2 * * *",First,Last);C.Misfire=Coalesce?XCRON_MISFIRE_COALESCE:XCRON_MISFIRE_CATCH_UP;Id=selectedAdd(S,&C,(xcrondeliverypolicy){0,1});xcronselectedpoll P=selectedPoll(S,Wall,0,(xcronoccurrencebudget){1024,1024,1024,1024,1024},&Run,1,128);xcronselectedjobinfo Info=selectedInfo(S,Id);
        testRequire(P.Poll.Count==1 && Run.Run.ScheduledAt==First && Run.Local.Day==9 && Run.Local.Hour==2 && Run.Local.Minute==30,"selected lease nominal metadata survives a later gap search");
        testRequire(Coalesce?P.Poll.Faults==0 && Info.State==XCRON_JOB_SCHEDULED && Info.Next==Last && Run.Run.Through==Wall: P.Poll.Faults==1 && Info.State==XCRON_JOB_FAULTED && Info.LastIssue.Code==XCRON_OCCURRENCE_MISSING && Info.LastIssue.When==Gap,"catch-up validates the retained gap; coalescing deliberately discards its interior window");
        testRequire(xcronSchedulerComplete(S,Run.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"discard-policy physical completion");selectedClean(S);
    }xcronZoneRelease(Z);
}
