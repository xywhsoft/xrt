static void execBatchSame(const xcronrun* A,const xcronrun* B)
{testRequire(A->Id==B->Id && A->Job==B->Job && A->Data==B->Data && A->ScheduledAt==B->ScheduledAt && A->Through==B->Through && A->Observed.Wall==B->Observed.Wall && A->Observed.MonotonicMilliseconds==B->Observed.MonotonicMilliseconds && A->Flags==B->Flags,"batch copied every semantic plain run field");}
static void execBatchReceipts(xcronexecutor* B,const xcronrun* Expected,size_t Count,bool Cancelled,bool Failed)
{
    uint64 Seen=0;
    for(size_t I=0;I<Count;++I){xcroncompletion D=execReceipt(B);size_t J=0;while(J<Count && Expected[J].Id!=D.Run.Id)++J;
        testRequire(J<Count && !(Seen&(UINT64_C(1)<<J)),"batch receipts are exactly once and unordered");Seen|=UINT64_C(1)<<J;execBatchSame(&D.Run,&Expected[J]);
        testRequire(D.Code==XCRON_EXECUTOR_OK && D.Result==(Failed?XCRON_RUN_FAILED:Cancelled?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED),"batch individual physical result");
        if(Failed)testRequire(D.Error && xrtErrorCode(D.Error)==17 && !strcmp(xrtErrorDomain(D.Error),"test.cron.worker"),"batch transfers each owned worker error");
        else testRequire(!D.Error,"batch no stale worker error");
        xrtErrorFree(D.Error);
    }
}
static int32 execBatchForeign(ptr Raw)
{execcontext* C=Raw;xcronrun R=execLease(UINT64_MAX,1,C);xcronexecutorissue I;execError(xcronExecutorSubmitBatch(C->Adapter,&R,1,&I),&I,XCRON_EXECUTOR_THREAD,"foreign batch controller rejected before reservation");return 0;}
static void testExecBatches(void)
{
    xexecutor* E=execPool(4,128);execcontext C={.Block=true};xcronexecutor* B=execAdapter(E,4,&C,false);xcronexecutorissue I;xcronexecutorstats S;
    xcronrun R[4],Original[4];for(unsigned J=0;J<4;++J){R[J]=execLease(J+1,7,&C);R[J].ScheduledAt=1000+J;R[J].Through=2000+J;R[J].Observed=(xcronclock){3000+J,4000+J};R[J].Flags=J%2?XCRON_RUN_COALESCED:0;Original[J]=R[J];}
    xcronrun Bad[3]={R[0],R[1],R[0]};execError(xcronExecutorSubmitBatch(B,Bad,3,&I),&I,XCRON_EXECUTOR_DUPLICATE,"duplicate batch rolls back reserved prefix");
    testRequire(xcronExecutorInspect(B,&S,NULL) && !S.Active && !S.Producers && !S.Submitted && !S.Peak && S.Rejected==3,"failed batch never admitted a prefix");
    testRequire(xcronExecutorSubmitBatch(B,R,2,NULL),"atomic two-lease batch");execUntil(&C.Started,2,"both plain batch bodies hold copied input");
    Bad[0]=R[2];Bad[1]=R[0];execError(xcronExecutorSubmitBatch(B,Bad,2,&I),&I,XCRON_EXECUTOR_DUPLICATE,"active duplicate rolls back new prefix only");
    execError(xcronExecutorSubmitBatch(B,R,3,&I),&I,XCRON_EXECUTOR_LIMIT,"whole batch exceeds remaining adapter capacity");
    testRequire(xcronExecutorSubmit(B,&R[3],NULL) && xcronExecutorSubmitBatch(B,&R[2],1,NULL),"scalar and singleton batch reuse rollback slots");execUntil(&C.Started,4,"four copied plain bodies entered");memset(R,0xa5,sizeof(R));
    testRequire(xcronExecutorClose(B,false,NULL),"batch close preserves active bodies");execError(xcronExecutorSubmitBatch(B,Original,2,&I),&I,XCRON_EXECUTOR_CLOSED,"closed batch counts every rejected lease");
    xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);execBatchReceipts(B,Original,4,false,false);
    testRequire(xcronExecutorInspect(B,&S,NULL) && S.Submitted==4 && S.Drained==4 && S.Rejected==10 && S.Peak==4 && !S.Active,"batch counters count leases and preserve older submissions");execAdapterEnd(B);execPoolEnd(E);

    E=execPool(4,128);C=(execcontext){0};B=execAdapter(E,64,&C,false);xcronrun Runs[64],Duplicates[64];
    for(unsigned J=0;J<64;++J)Runs[J]=execLease(J+1,1+J%7,&C);
    memset(&I,0x5a,sizeof(I));xcronexecutorissue Prior=I;
    testRequire(!xcronExecutorSubmitBatch(B,Runs,0,&I) && !memcmp(&I,&Prior,sizeof(I)) && !xcronExecutorSubmitBatch(B,Runs,65,&I) && !memcmp(&I,&Prior,sizeof(I)),"batch count arguments preserve outputs");xrtClearError();
    testRequire(!xcronExecutorSubmitBatch(B,Runs,2,(xcronexecutorissue*)Runs) && !xcronExecutorSubmitBatch(B,(const xcronrun*)B,1,&I) && !memcmp(&I,&Prior,sizeof(I)),"batch input/output and owned-arena aliases rejected");xrtClearError();
    memcpy(Duplicates,Runs,sizeof(Runs));Duplicates[63].Id=0;testRequire(!xcronExecutorSubmitBatch(B,Duplicates,64,&I) && !memcmp(&I,&Prior,sizeof(I)),"invalid last run rejects before reservation");xrtClearError();
    xthread* Foreign=xrtThreadCreate(execBatchForeign,&C,0);testRequire(Foreign && xrtThreadWaitFor(Foreign,10000)==XWAIT_OK,"foreign batch controller joined");xrtThreadDestroy(Foreign);
    for(unsigned Position=1;Position<64;++Position){memcpy(Duplicates,Runs,sizeof(Runs));Duplicates[Position]=Runs[0];execError(xcronExecutorSubmitBatch(B,Duplicates,64,&I),&I,XCRON_EXECUTOR_DUPLICATE,"every duplicate position rolls back its whole prefix");
        testRequire(xcronExecutorInspect(B,&S,NULL) && !S.Active && !S.Producers,"all prefix slots returned");testRequire(xcronExecutorSubmitBatch(B,Runs,64,NULL),"complete capacity reused after each prefix rollback");execBatchReceipts(B,Runs,64,false,false);}
    for(unsigned Position=0;Position<63;++Position){testRequire(xcronExecutorSubmit(B,&Runs[0],NULL),"older scalar lease survives failed batch");memcpy(Duplicates,Runs+1,63*sizeof(*Runs));Duplicates[Position]=Runs[0];execError(xcronExecutorSubmitBatch(B,Duplicates,63,&I),&I,XCRON_EXECUTOR_DUPLICATE,"every active duplicate position preserves existing lease");
        testRequire(xcronExecutorInspect(B,&S,NULL) && S.Active==1 && xcronExecutorSubmitBatch(B,Runs+1,63,NULL),"all remaining slots reusable around older lease");execBatchReceipts(B,Runs,64,false,false);}
    C.Mode=1;testRequire(xcronExecutorSubmitBatch(B,Runs,4,NULL),"per-lease worker failures in batch");execBatchReceipts(B,Runs,4,false,true);execAdapterEnd(B);execPoolEnd(E);

    E=execPool(1,2);C=(execcontext){0};B=execAdapter(E,64,&C,false);execError(xcronExecutorSubmitBatch(B,Runs,3,&I),&I,XCRON_EXECUTOR_REJECTED,"native atomic queue limit rejects every lease");testRequire(I.Cause==XERR_AGAIN && !xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE) && xcronExecutorInspect(B,&S,NULL) && !S.Active && !S.Producers,"native rejection returns capacity and no physical duties");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"batch rejection diagnostic OOM budget");testRequire(!xcronExecutorSubmitBatch(B,Runs,3,&I) && I.Code==XCRON_EXECUTOR_REJECTED && I.Cause==XERR_AGAIN && xrtErrorKind(xrtGetError())==XERR_MEMORY,"batch native rejection retains its original cause despite diagnostic OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave batch rejection quota");xrtClearError();xrtAllocationBudgetFree(Budget);
    testRequire(xcronExecutorSubmitBatch(B,Runs,2,NULL),"reuse all native-rejected IDs");execBatchReceipts(B,Runs,2,false,false);execAdapterEnd(B);execPoolEnd(E);

    E=execPool(4,2);execcontext Blockers[4]={{0}};for(unsigned J=0;J<4;++J)testRequire(xrtExecutorSubmit(E,execBlock,&Blockers[J],NULL,NULL),"occupy every native worker");
    for(unsigned J=0;J<4;++J)execUntil(&Blockers[J].Started,1,"all native workers occupied");
    for(unsigned J=0;J<8;++J)testRequire(xrtExecutorSubmit(E,execBlock,&Blockers[0],NULL,NULL),"fill every native worker queue");
    C=(execcontext){0};B=execAdapter(E,64,&C,false);
    for(unsigned Attempt=0;Attempt<2;++Attempt){execError(xcronExecutorSubmitBatch(B,Runs,2,&I),&I,XCRON_EXECUTOR_REJECTED,"native capacity rejection after reserving core live-job references");testRequire(I.Cause==XERR_AGAIN && xcronExecutorInspect(B,&S,NULL) && !S.Active && !S.Producers && !S.Submitted && !S.Peak && !xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE),"full-queue rejection rolls back all IDs/credits and no callback");}
    testRequire(xrtExecutorCancel(E),"cancel foreign queued fixtures after rejected batches");for(unsigned J=0;J<4;++J)xrtAtomic32Store(&Blockers[J].Gate,1,XMEMORY_RELEASE);
    execAdapterEnd(B);execPoolEnd(E);

    E=execPool(1,8);execcontext Blocker={.Block=true};testRequire(xrtExecutorSubmit(E,execBlock,&Blocker,NULL,NULL),"park native worker before batch cancellation");execUntil(&Blocker.Started,1,"native blocker active");C=(execcontext){.ProbeNotify=true};B=execAdapter(E,4,&C,true);
    testRequire(xcronExecutorSubmitBatch(B,Runs,4,NULL) && xrtExecutorCancel(E),"cancel one accepted native batch without Proc");execBatchReceipts(B,Runs,4,true,false);testRequire(!xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE) && xrtAtomic32Load(&C.Reentry,XMEMORY_ACQUIRE)==4,"each queued batch receipt retains owner Notify reentry guard");xrtAtomic32Store(&Blocker.Gate,1,XMEMORY_RELEASE);execAdapterEnd(B);execPoolEnd(E);

    E=execPool(4,128);C=(execcontext){0};B=execAdapter(E,64,&C,false);testRequire(xcronExecutorSubmitBatch(B,Runs,64,NULL),"warm maximum batch");execBatchReceipts(B,Runs,64,false,false);
    Budget=NULL;Scope=(xallocationscope){0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"batch warmed owner quota");xrtSetErrorInfo(XERR_STATE,"batch.owner",19,"retain ambient error");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter batch warm quota");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned Round=0;Round<512;++Round){for(unsigned J=0;J<64;++J)Runs[J].Id=(uint64)Round*64+J+65;testRequire(xcronExecutorSubmitBatch(B,Runs,64,NULL),"maximum batch with no requested heap");execBatchReceipts(B,Runs,64,false,false);testRequire(xrtGetError()==Ambient,"batch success preserves owner ambient error identity");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"batch descriptors/hash/worker/drop request no warm heap");
#endif
    testRequire(!Scope.Rejections && xrtAllocationBudgetLeave(&Scope),"maximum batches rejected no allocation");xrtClearError();xrtAllocationBudgetFree(Budget);execAdapterEnd(B);execPoolEnd(E);
}
