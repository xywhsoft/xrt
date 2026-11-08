static void testTzdbArguments(void)
{
    xcrontzdbconfig C={8,2,1024*1024};xcrontzdbissue I,Before;memset(&I,0x5a,sizeof(I));Before=I;
    testRequire(!xcronTzdbCreate(&C,(xcrontzdb**)&C,&I) && !memcmp(&I,&Before,sizeof(I)),"factory alias preserves every output");xrtClearError();
    xcrontzdb* D=tzdbCreate(1024*1024);xcrontzdbentry E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});uint64 R=777;
    testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,SIZE_MAX,0,&R,&I) && R==777 && !memcmp(&I,&Before,sizeof(I)),"wrapped input count rejected atomically");xrtClearError();
    testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,(uint64*)&I,&I) && !memcmp(&I,&Before,sizeof(I)),"revision/issue alias");xrtClearError();
    union {uint64 Align;char Text[96];} Text;memset(&Text,0x33,sizeof(Text));memcpy(Text.Text,"UTC",3);char Copy[96];memcpy(Copy,Text.Text,sizeof(Copy));
    E.Name=(xstrview){Text.Text,3};testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,&R,(xcrontzdbissue*)Text.Text) && !memcmp(Copy,Text.Text,sizeof(Copy)),"nested name/issue alias never writes immutable input");xrtClearError();
    E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});testRequire(!xcronTzdbLoad(D,(xstrview){NULL,1},&E,1,0,&R,&I) && R==777 && !memcmp(&I,&Before,sizeof(I)),"invalid version span");xrtClearError();
    cstr Bad[]={"", "/UTC","UTC/",".","..","../UTC","A/../UTC","A//B","A/./B","A/-B","A/B.","C:UTC","A\\B","UTC ","A/\x7f","A/\xc3\xa9"};
    for(size_t N=0;N<sizeof(Bad)/sizeof(Bad[0]);++N){E.Name=(xstrview){Bad[N],strlen(Bad[N])};testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,&R,&I) && I.Code==XCRON_TZDB_NAME && I.Index==0 && R==777 && !tzdbStats(D).Revision,"unsafe/unsupported name is a typed atomic rejection");xrtClearError();}
    E.Name=XRT_STR_LITERAL("UTC");testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("bad version"),&E,1,0,&R,&I) && I.Code==XCRON_TZDB_VERSION,"visible release version required");xrtClearError();
    xcrontzdbentry Duplicates[]={E,E};testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),Duplicates,2,0,&R,&I) && I.Code==XCRON_TZDB_DUPLICATE && I.Index<2 && !tzdbStats(D).Revision,"duplicates reject before any publication");xrtClearError();
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("empty"),NULL,0,0,&R,NULL) && R==1,"empty manifest is an explicit revision");xcrontzdbsnapshot* S=tzdbAcquire(D);xcronzone* Z=(xcronzone*)(uintptr_t)1;
    testRequire(xcronTzdbFind(S,XRT_STR_LITERAL("UTC"),&Z,NULL)==XCRON_TZDB_NONE && Z==(xcronzone*)(uintptr_t)1,"empty catalog returns no guessed UTC");
    xcrontzdbsnapshotinfo Info;testRequire(xcronTzdbSnapshotInfo(S,&Info,NULL) && !Info.Names,"empty snapshot info");
    memset(&I,0x5a,sizeof(I));testRequire(!xcronTzdbSnapshotInfo(S,(xcrontzdbsnapshotinfo*)Info.Version.Data,&I) && !memcmp(&I,&Before,sizeof(I)),"snapshot arena cannot be a writable output");xrtClearError();
    xcronTzdbSnapshotRelease(S);testRequire(xcronTzdbClose(D,NULL),"close empty database");testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,1,&R,&I) && I.Code==XCRON_TZDB_CLOSED && R==1,"closed rejects new valid load");xrtClearError();testRequire(xcronTzdbDestroy(D,NULL),"destroy closed empty database");
}
static void testTzdbQuota(void)
{
    xcrontzdb* D=tzdbCreate(1024*1024);xcrontzdbentry E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,&R,NULL),"quota initial generation");
    xcrontzdbsnapshot* S=tzdbAcquire(D);xcrontzdbsnapshotinfo Info;testRequire(xcronTzdbSnapshotInfo(S,&Info,NULL),"snapshot requested bytes");xcronzone* Z=tzdbFind(S,"UTC");xcrontzdbstats Before=tzdbStats(D);
    testRequire(Before.LiveBytes==sizeof(*D)+Info.Bytes && Before.LiveBytes<=Before.BytesLimit,"manager/snapshot/decoded/footer storage counted exactly");
    testRequire(xrtAllocationBudgetSetLimit(D->Budget,Before.LiveBytes+Info.Bytes-1),"leave one byte less than complete replacement");uint64 Out=888;xcrontzdbissue I;
    testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("b"),&E,1,1,&Out,&I) && I.Code==XCRON_TZDB_LIMIT && Out==888,"peak includes old and candidate, not only new published bytes");xrtClearError();
    xcrontzdbstats After=tzdbStats(D);testRequire(After.Revision==1 && After.LiveBytes==Before.LiveBytes && After.Rejections>Before.Rejections,"failed replacement releases every partial allocation");
    testRequire(xrtAllocationBudgetSetLimit(D->Budget,1024*1024),"restore declared quota");testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("b"),&E,1,1,&R,NULL),"replace with retained old owners");
    size_t Arena=S->ArenaBytes,Zone=xcronZoneOwnedBytes(Z);Before=tzdbStats(D);xcronTzdbSnapshotRelease(S);After=tzdbStats(D);
    testRequire(After.LiveBytes+Arena==Before.LiveBytes,"old retained zone outlives freed old catalog arena");xcronZoneRelease(Z);testRequire(tzdbStats(D).LiveBytes+Zone==After.LiveBytes,"independent zone release uncharges old generation");
    S=tzdbAcquire(D);testRequire(xrtAllocationBudgetSetLimit(D->Budget,tzdbStats(D).LiveBytes+S->ArenaBytes),"admit catalog but deny error diagnostic/zone allocation");
    xcrontzdbentry Bad=tzdbEntry("UTC",(xbytesview){(const uint8*)"bad",3});xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"outer diagnostic OOM");
    testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("c"),&Bad,1,R,&Out,&I) && I.Code==XCRON_TZDB_ZONE && I.Zone.Code==XCRON_ZONE_TZIF && I.Index==0 && Out==888 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed malformed data survives both inner and outward diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave outer diagnostic quota");xrtClearError();xrtAllocationBudgetFree(B);xcronTzdbSnapshotRelease(S);tzdbEnd(D);
}
static void testTzdbAllocationPrefixes(void)
{
    tzifblob Blob;tzifkind K[]={{0,false,"STD"}};tzifBuild(&Blob,'3',NULL,NULL,0,K,1,"STD0DST,M3.2.0,M11.1.0");
    char Names[8][24];xcrontzdbentry Entries[8];for(unsigned I=0;I<8;++I){int N=snprintf(Names[I],sizeof(Names[I]),"Zone/Prefix_%u",I);testRequire(N>0,"prefix name");Entries[I]=tzdbEntry(Names[I],(xbytesview){Blob.Data,Blob.Size});}
    xcrontzdb* D=tzdbCreate(1024*1024);uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("prefix"),Entries,8,0,&R,NULL),"measure actual complete allocation graph");
    xcrontzdbsnapshot* S=tzdbAcquire(D);size_t Required=sizeof(*D)+S->Bytes;xcronTzdbSnapshotRelease(S);tzdbEnd(D);
    unsigned Seen=0,Footer=0,History=0,Arena=0,Attempts=0;
    for(size_t Limit=sizeof(*D)+1;Limit<Required;Limit+=97){xcrontzdbconfig C={8,1,Limit};D=NULL;testRequire(xcronTzdbCreate(&C,&D,NULL),"prefix manager fits");uint64 Out=777;xcrontzdbissue I;
        testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("prefix"),Entries,8,0,&Out,&I) && I.Code==XCRON_TZDB_LIMIT && Out==777,"every observed allocation prefix rolls back publication");
        if(I.Index==XCRON_TZDB_NO_INDEX)++Arena;
        else {testRequire(I.Index<8 && I.Zone.Code==XCRON_ZONE_MEMORY,"allocation prefix retains typed footer/history failure");Seen|=1u<<I.Index;
            if(I.Zone.Offset==Blob.Size-1)Footer|=1u<<I.Index;
            if(I.Zone.Offset==Blob.End)History|=1u<<I.Index;}
        xrtClearError();xcrontzdbstats Stats=tzdbStats(D);testRequire(!Stats.Revision && !Stats.Loading && Stats.LiveBytes==sizeof(*D) && Stats.PeakBytes<=Limit && Stats.Rejections,"failed prefix releases arena/all successful earlier zones/footer rules");tzdbEnd(D);++Attempts;
    }
    testRequire(Seen==255 && Footer==255 && History==255 && Arena && Attempts>100,"all eight actual footer/history allocation positions and arena failure exercised");
    printf("[PASS] tzdb allocation prefixes: %u quota boundaries, all8 footer/history positions\n",Attempts);
}
static void testTzdbOrderAndWarm(void)
{
    enum{Count=257};char Names[Count][32];xcrontzdbentry Entries[Count];
    for(unsigned I=0;I<Count;++I){unsigned K=Count-I-1;int N=snprintf(Names[I],sizeof(Names[I]),"Region/Name_%03u",K);testRequire(N>0,"manifest name formatting");Entries[I]=tzdbEntry(Names[I],(xbytesview){TzdbUTC,sizeof(TzdbUTC)});}
    xcrontzdb* D=tzdbCreate(1024*1024);uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("order"),Entries,Count,0,&R,NULL),"reverse manifest uses bounded in-place sort");
    memset(Names,0,sizeof(Names));xcrontzdbsnapshot* S=tzdbAcquire(D);xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"lookup one-byte quota");
    xrtSetErrorInfo(XERR_VALUE,"tzdb.ambient",3,"preserve");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter lookup quota");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned I=0;I<10000;++I){char Name[32];int N=snprintf(Name,sizeof(Name),"Region/Name_%03u",I%Count);xcronzone* Z=NULL;
        testRequire(xcronTzdbFind(S,(xstrview){Name,(size_t)N},&Z,NULL)==XCRON_TZDB_FOUND && !tzdbOffset(Z),"all sorted names survive mutated source and zero-heap lookup");xcronZoneRelease(Z);
        xcronzone* Keep=(xcronzone*)(uintptr_t)1;testRequire(xcronTzdbFind(S,XRT_STR_LITERAL("Region/Missing"),&Keep,NULL)==XCRON_TZDB_NONE && Keep==(xcronzone*)(uintptr_t)1 && xrtGetError()==Ambient,"negative lookup preserves pointer and ambient error");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"named positive/negative lookups request no heap");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave lookup quota");xrtClearError();xrtAllocationBudgetFree(Budget);
    uint32 Refs=xrtAtomic32Load(&S->Refs,XMEMORY_RELAXED);xrtAtomic32Store(&S->Refs,UINT32_MAX/2u,XMEMORY_RELAXED);xcrontzdbsnapshot* Out=NULL;xcrontzdbissue Issue;
    testRequire(!xcronTzdbSnapshotRetain(S) && xrtAtomic32Load(&S->Refs,XMEMORY_RELAXED)==UINT32_MAX/2u,"snapshot reference saturation cannot wrap");xrtClearError();
    testRequire(!xcronTzdbAcquire(D,&Out,&Issue) && Issue.Code==XCRON_TZDB_LIMIT && !Out,"acquire publishes nothing on retain saturation");xrtClearError();xrtAtomic32Store(&S->Refs,Refs,XMEMORY_RELAXED);xcronTzdbSnapshotRelease(S);
    D->Revision=UINT64_MAX-2;xcrontzdbentry E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("last"),&E,1,UINT64_MAX-2,&R,NULL) && R==UINT64_MAX-1,"last representable revision");uint64 Keep=R;
    testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("next"),&E,1,XCRON_TZDB_ANY_REVISION,&Keep,&Issue) && Issue.Code==XCRON_TZDB_LIMIT && Keep==R,"revision neither wraps nor occupies ANY sentinel");xrtClearError();tzdbEnd(D);
}
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
typedef struct tzdbgate {xcrontzdb* D;xstrview Name;xbytesview Data;xatomic32 Entered,Go;uint64 Expected,Revision;bool OK;xcrontzdbissue Issue;} tzdbgate;
static void tzdbAwait(const xatomic32* A,uint32 N)
{double End=xrtTimer()+15;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)!=N){testRequire(xrtTimer()<End,"tzdb controlled thread rendezvous");xrtThreadYield();}}
static bool tzdbReadGate(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{tzdbgate* G=Raw;(void)Name;(void)Work;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);tzdbAwait(&G->Go,1);*Data=G->Data;*Owned=NULL;return true;}
static int32 tzdbGateWorker(ptr Raw)
{tzdbgate* G=Raw;xcrontzdbsource Source={.Names=&G->Name,.Count=1,.Read=tzdbReadGate,.Context=G};G->OK=xcronTzdbLoadSource(G->D,XRT_STR_LITERAL("gate"),&Source,G->Expected,&G->Revision,&G->Issue);xrtClearError();return 0;}
static tzdbgate tzdbGate(xcrontzdb* D,uint64 Expected)
{tzdbgate G={.D=D,.Name=XRT_STR_LITERAL("UTC"),.Data={TzdbUTC,sizeof(TzdbUTC)},.Expected=Expected,.Revision=777};xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Go,0);return G;}
static void tzdbJoin(xthread* T)
{testRequire(xrtThreadWaitFor(T,15000)==XWAIT_OK && !xrtThreadExitCode(T),"physical tzdb loader join");xrtThreadDestroy(T);}
static void testTzdbConcurrentPublication(void)
{
    xcrontzdbconfig C={8,1,1024*1024};xcrontzdb* D=NULL;testRequire(xcronTzdbCreate(&C,&D,NULL),"one-builder bounded database");xcrontzdbentry E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,&R,NULL),"concurrent base revision");
    tzdbgate G=tzdbGate(D,1);xthread* T=xrtThreadCreate(tzdbGateWorker,&G,0);testRequire(T!=NULL,"start held cold reader");tzdbAwait(&G.Entered,1);
    xcrontzdbsnapshot* S=tzdbAcquire(D);xcronzone* Z=tzdbFind(S,"UTC");testRequire(!tzdbOffset(Z) && tzdbStats(D).Loading==1,"reader progresses while cold load is blocked outside publication lock");xcronZoneRelease(Z);xcronTzdbSnapshotRelease(S);
    xcrontzdbissue I;uint64 Out=888;testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("b"),&E,1,1,&Out,&I) && I.Code==XCRON_TZDB_LIMIT && Out==888,"builder capacity rejects without queueing");xrtClearError();
    testRequire(xcronTzdbClose(D,NULL) && !xcronTzdbDestroy(D,&I) && I.Code==XCRON_TZDB_BUSY,"close rejects publication but cannot destroy a held builder");xrtClearError();
    xrtAtomic32Store(&G.Go,1,XMEMORY_RELEASE);tzdbJoin(T);testRequire(!G.OK && G.Issue.Code==XCRON_TZDB_CLOSED && G.Revision==777 && !tzdbStats(D).Loading,"closing build rolls back its decoded candidate");testRequire(xcronTzdbDestroy(D,NULL),"destroy after loader physical join");
    D=tzdbCreate(1024*1024);R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("a"),&E,1,0,&R,NULL),"CAS race base");tzdbgate A=tzdbGate(D,1),B=tzdbGate(D,1);
    xthread* First=xrtThreadCreate(tzdbGateWorker,&A,0);xthread* Second=xrtThreadCreate(tzdbGateWorker,&B,0);testRequire(First && Second,"two candidate builders");tzdbAwait(&A.Entered,1);tzdbAwait(&B.Entered,1);
    xrtAtomic32Store(&B.Go,1,XMEMORY_RELEASE);tzdbJoin(Second);testRequire(B.OK && B.Revision==2,"one actual candidate wins CAS");xrtAtomic32Store(&A.Go,1,XMEMORY_RELEASE);tzdbJoin(First);
    testRequire(!A.OK && A.Issue.Code==XCRON_TZDB_STALE && A.Revision==777 && tzdbStats(D).Revision==2,"late older expected revision cannot overwrite the winner");tzdbEnd(D);
}
typedef struct tzdbreader {xcrontzdb* D;uint32 Reads;} tzdbreader;
static int32 tzdbReaderWorker(ptr Raw)
{tzdbreader* F=Raw;for(unsigned I=0;I<F->Reads;++I){xcrontzdbsnapshot* S=tzdbAcquire(F->D);xcrontzdbsnapshotinfo Info;testRequire(xcronTzdbSnapshotInfo(S,&Info,NULL),"concurrent coherent manifest");xcronzone* Z=tzdbFind(S,"UTC");testRequire(tzdbOffset(Z)==(Info.Revision%2?0:3600),"lookup and manifest belong to the same published version");xcronTzdbSnapshotRelease(S);testRequire(tzdbOffset(Z)==(Info.Revision%2?0:3600),"zone survives snapshot release during concurrent reload");xcronZoneRelease(Z);}return 0;}
static void testTzdbReaders(void)
{
    xcrontzdb* D=tzdbCreate(1024*1024);tzifblob Plus;tzifkind K[]={{3600,false,"PLUS"}};tzifBuild(&Plus,0,NULL,NULL,0,K,1,"");xcrontzdbentry E=tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)});uint64 R=0;testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("odd"),&E,1,0,&R,NULL),"reader initial publication");
    tzdbreader F={D,2000};xthread* Threads[4];for(unsigned I=0;I<4;++I){Threads[I]=xrtThreadCreate(tzdbReaderWorker,&F,0);testRequire(Threads[I]!=NULL,"immutable snapshot reader start");}
    for(unsigned I=2;I<=250;++I){E.Tzif=I%2?(xbytesview){TzdbUTC,sizeof(TzdbUTC)}:(xbytesview){Plus.Data,Plus.Size};testRequire(xcronTzdbLoad(D,I%2?XRT_STR_LITERAL("odd"):XRT_STR_LITERAL("even"),&E,1,I-1,&R,NULL) && R==I,"real repeated concurrent reload");}
    for(unsigned I=0;I<4;++I)tzdbJoin(Threads[I]);
    xcrontzdbsnapshot* S=tzdbAcquire(D);xcrontzdbsnapshotinfo Info;testRequire(xcronTzdbSnapshotInfo(S,&Info,NULL) && tzdbStats(D).LiveBytes==sizeof(*D)+Info.Bytes,"all obsolete generations released after readers join");xcronTzdbSnapshotRelease(S);tzdbEnd(D);
}
#endif
