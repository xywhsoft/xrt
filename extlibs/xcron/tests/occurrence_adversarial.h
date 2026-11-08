static void testOccurrenceHistory(void)
{
    uint8 Raw[8192]={0};memcpy(Raw,"TZif3",5);tzifU32(Raw+36,1);tzifU32(Raw+40,1);size_t H=51;memcpy(Raw+H,"TZif3",5);tzifU32(Raw+H+32,255);tzifU32(Raw+H+36,256);tzifU32(Raw+H+40,4);
    size_t Times=H+44,Indices=Times+255*8,Types=Indices+255,Names=Types+256*6;
    for(unsigned I=0;I<255;++I){tzifTime(Raw+Times+I*8,I+1,8);Raw[Indices+I]=(uint8)(I+1);}
    for(unsigned I=0;I<256;++I)tzifU32(Raw+Types+I*6,256-I);
    memcpy(Raw+Names,"AAA",4);cstr Footer="\nAAA-0:00:01\n";size_t Size=Names+4+strlen(Footer);memcpy(Raw+Names+4,Footer,strlen(Footer));xcronzone* Z=NULL;
    testRequire(xcronZoneCreateTzif((xbytesview){Raw,Size},&Z,NULL),"256 real repeated instants");memset(Raw,0,sizeof(Raw));xcronexpression E=occurrenceExpr("16 4 0 * * *");
    for(unsigned K=0;K<3;++K){xtime Begin=XRT_TIME_UNIX_EPOCH+(K==0?0:K==1?127:255)*1000,End=XRT_TIME_UNIX_EPOCH+255000;
        occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){0,0},XCRON_OCCURRENCE_FOUND,0,Begin,256);
        occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){1,0},K?XCRON_OCCURRENCE_NONE:XCRON_OCCURRENCE_FOUND,0,K?0:Begin,K?0:256);
        occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){2,0},XCRON_OCCURRENCE_FOUND,0,End,256);
        occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){3,0},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_AMBIGUOUS,Begin,256);
    }xcronZoneRelease(Z);
    tzifblob Blob;int64 T[1000];uint8 I[1000];const tzifkind K[]={{0,false,"STD"},{3600,true,"DST"}};
    for(unsigned Y=0;Y<500;++Y){T[Y*2]=(occurrenceTime(2000+Y,3,1,2,0,0)-XRT_TIME_UNIX_EPOCH)/1000;T[Y*2+1]=(occurrenceTime(2000+Y,10,1,1,0,0)-XRT_TIME_UNIX_EPOCH)/1000;I[Y*2]=1;I[Y*2+1]=0;}
    tzifBuild(&Blob,'3',T,I,1000,K,2,"STD0");testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"500-year nonperiodic prefix");E=occurrenceExpr("0 30 2 1 MAR *");xtime Begin=occurrenceTime(2000,1,1,0,0,0),End=occurrenceTime(2600,1,1,0,0,0);
    occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){0,0},XCRON_OCCURRENCE_FOUND,0,occurrenceTime(2500,3,1,2,30,0),1);
    occurrenceExpect(&E,Z,Begin,End,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_MISSING,occurrenceTime(2000,3,1,2,0,0),0);xcronZoneRelease(Z);
    Z=occurrenceZone("STD0DST,J60/2,J300/2");occurrenceExpect(&E,Z,Begin,INT64_MAX,0,(xcrondeliverypolicy){0,0},XCRON_OCCURRENCE_NONE,0,0,0);
    occurrenceExpect(&E,Z,Begin,INT64_MAX,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_MISSING,occurrenceTime(2000,3,1,2,0,0),0);xcronZoneRelease(Z);
    const tzifkind Fixed[]={{0,false,"STD"}};tzifBuild(&Blob,'3',NULL,NULL,0,Fixed,1,"STD0<-00>,59/0,J60/1");testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"rare unspecified footer");E=occurrenceExpr("* * * * * *");xtime At=occurrenceTime(2097,1,1,0,0,0)+(int64)INT32_MAX*1000;
    occurrenceExpect(&E,Z,At,At,0,(xcrondeliverypolicy){0,0},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_ZONE,At,0);xcronZoneRelease(Z);
}
static void testOccurrenceArguments(void)
{
    xcronzone* Z=occurrenceZone("UTC0"),*Other=occurrenceZone("UTC0");xcronexpression E=occurrenceExpr("* * * * * *"),Bad=E;Bad.Fields[XCRON_FIELD_MINUTE]=0;
    xcronoccurrencecursor C,Before;memset(&C,0x5a,sizeof(C));memcpy(&Before,&C,sizeof(C));xcronoccurrence Out,Old;memset(&Out,0x5a,sizeof(Out));memcpy(&Old,&Out,sizeof(Out));xcronoccurrenceissue I,Previous;memset(&I,0x5a,sizeof(I));memcpy(&Previous,&I,sizeof(I));
    testRequire(!xcronOccurrenceCursorInit(&C,&E,Z,0,1000,2,(xcrondeliverypolicy){0,0},&I) && !memcmp(&I,&Previous,sizeof(I)) && !memcmp(&C,&Before,sizeof(C)),"invalid flags are structural and atomic");xrtClearError();
    testRequire(!xcronOccurrenceCursorInit(&C,(const xcronexpression*)&C,Z,0,1000,0,(xcrondeliverypolicy){0,0},&I) && !memcmp(&I,&Previous,sizeof(I)) && !memcmp(&C,&Before,sizeof(C)),"init expression/cursor alias");xrtClearError();
    testRequire(!xcronOccurrenceCursorInit(&C,&E,Z,0,1000,0,(xcrondeliverypolicy){4,0},&I) && !memcmp(&I,&Previous,sizeof(I)),"invalid fold policy");xrtClearError();
    testRequire(!xcronOccurrenceCursorInit(&C,&E,Z,0,1000,0,(xcrondeliverypolicy){0,2},&I) && !memcmp(&I,&Previous,sizeof(I)),"invalid missing policy");xrtClearError();
    testRequire(!xcronOccurrenceCursorInit(&C,&Bad,Z,0,1000,0,(xcrondeliverypolicy){0,0},&I) && I.Code==XCRON_OCCURRENCE_EXPRESSION && I.Field==XCRON_FIELD_MINUTE && !memcmp(&C,&Before,sizeof(C)),"typed invalid expression preserves cursor");xrtClearError();
    testRequire(!xcronOccurrenceCursorInit(&C,&E,Z,1000,0,0,(xcrondeliverypolicy){0,0},&I) && I.Code==XCRON_OCCURRENCE_WINDOW && !memcmp(&C,&Before,sizeof(C)),"reversed window preserves cursor");xrtClearError();
    testRequire(xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,&I)==XCRON_OCCURRENCE_ERROR && I.Code==XCRON_OCCURRENCE_STATE && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&Out,&Old,sizeof(Out)),"uninitialized cursor typed state");xrtClearError();
    testRequire(xcronOccurrenceCursorInit(&C,&E,Z,0,1000,0,(xcrondeliverypolicy){0,0},NULL),"argument fixture init");memcpy(&Before,&C,sizeof(C));memset(&I,0x5a,sizeof(I));memcpy(&Previous,&I,sizeof(I));
    testRequire(xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},(xcronoccurrence*)C.Storage,&I)==XCRON_OCCURRENCE_ERROR && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Previous,sizeof(I)),"opaque/public result alias");xrtClearError();
    testRequire(xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,(xcronoccurrenceissue*)&Out)==XCRON_OCCURRENCE_ERROR && !memcmp(&Out,&Old,sizeof(Out)),"result/issue alias");xrtClearError();
    testRequire(xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},(xcronoccurrence*)(UINTPTR_MAX-31),&I)==XCRON_OCCURRENCE_ERROR && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Previous,sizeof(I)),"wrapped result span");xrtClearError();
    testRequire(xcronOccurrenceCursorStep(&Before,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,&I)==XCRON_OCCURRENCE_ERROR && I.Code==XCRON_OCCURRENCE_STATE,"copied cursor rejected");xrtClearError();
    testRequire(xcronOccurrenceCursorStep(&C,Other,(xcronoccurrencebudget){1,1,1,1,1},&Out,&I)==XCRON_OCCURRENCE_ERROR && I.Code==XCRON_OCCURRENCE_STATE && !memcmp(&C,&Before,sizeof(C)),"wrong exact zone rejected without advancement");xrtClearError();
    xcronoccurrencebudget Steps[]={{0,1,1,1,1},{1,0,1,1,1},{1,1,0,0,0},{0,0,0,1,0},{0,0,0,0,1}};
    for(unsigned J=0;J<5;++J){testRequire(xcronOccurrenceCursorStep(&C,Z,Steps[J],&Out,&I)==(J==4?XCRON_OCCURRENCE_FOUND:XCRON_OCCURRENCE_PENDING),"each independent needed budget can block/yield");}
    xcronZoneRelease(Other);xcronZoneRelease(Z);
}
static void testOccurrenceWarmAndOOM(void)
{
    xcronzone* Z=occurrenceZone("UTC0");xcronexpression E=occurrenceExpr("* * * * * *");xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B),"occurrence allocation quota");xrtSetErrorInfo(XERR_VALUE,"occurrence.ambient",11,"preserve identity");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(B,&Scope),"warm allocation scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned J=0;J<1024;++J){xcronoccurrence Out;xcronoccurrencecursor C;xcronoccurrenceissue I;testRequire(xcronOccurrenceNext(&E,Z,0,1000,0,(xcrondeliverypolicy){0,J%2},&Out,NULL)==XCRON_OCCURRENCE_FOUND && xcronOccurrenceCursorInit(&C,&E,Z,0,1000,0,(xcrondeliverypolicy){0,J%2},NULL),"warmed synchronous/init calls");xcronoccurrencestatus S;
        do{S=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,&I);testRequire(xrtGetError()==Ambient,"normal result preserves ambient error identity");}while(S==XCRON_OCCURRENCE_PENDING);testRequire(S==XCRON_OCCURRENCE_FOUND && Out.When==0 && Out.Candidates==1,"warm exact result");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"all normal search calls request no heap");
#endif
    testRequire(!Scope.Rejections && xrtAllocationBudgetLeave(&Scope),"normal operations cause no quota rejection");xrtClearError();xcronZoneRelease(Z);
    Z=occurrenceZone("EST5EDT,M3.2.0,M11.1.0");E=occurrenceExpr("0 30 2 * * *");xtime Gap=occurrenceTime(2024,3,10,7,0,0);xcronoccurrence Out,Old;memset(&Out,0x5a,sizeof(Out));memcpy(&Old,&Out,sizeof(Old));xcronoccurrenceissue I;
    testRequire(xrtAllocationBudgetEnter(B,&Scope),"missing diagnostic OOM scope");testRequire(xcronOccurrenceNext(&E,Z,Gap,Gap,0,(xcrondeliverypolicy){0,1},&Out,&I)==XCRON_OCCURRENCE_ERROR && I.Code==XCRON_OCCURRENCE_MISSING && I.When==Gap && I.Local.Hour==2 && I.Local.Minute==30 && !memcmp(&Out,&Old,sizeof(Out)),"missing error metadata/atomicity survives diagnostic OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave OOM scope");xrtClearError();xrtAllocationBudgetFree(B);xcronZoneRelease(Z);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct occurrencehandoff {xcronoccurrencecursor* C;xcronzone* Z;xcronoccurrence Out;xcronoccurrencestatus Status;} occurrencehandoff;
static int32 occurrenceWorker(ptr Raw)
{occurrencehandoff* H=Raw;do{H->Status=xcronOccurrenceCursorStep(H->C,H->Z,(xcronoccurrencebudget){1,1,1,1,1},&H->Out,NULL);}while(H->Status==XCRON_OCCURRENCE_PENDING);return 0;}
static void testOccurrenceHandoff(void)
{xcronzone* Z=occurrenceZone("EST5EDT,M3.2.0,M11.1.0");xcronexpression E=occurrenceExpr("0 30 1 * * *");xtime A=occurrenceTime(2024,11,3,5,30,0);xcronoccurrencecursor C;occurrencehandoff H={.C=&C,.Z=Z};testRequire(xcronOccurrenceCursorInit(&C,&E,Z,A,A+3600000,0,(xcrondeliverypolicy){2,1},NULL) && xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&H.Out,NULL)==XCRON_OCCURRENCE_PENDING,"actual pending computation before serialized handoff");xthread* T=xrtThreadCreate(occurrenceWorker,&H,0);testRequire(T && xrtThreadWaitFor(T,15000)==XWAIT_OK && !xrtThreadExitCode(T) && H.Status==XCRON_OCCURRENCE_FOUND && H.Out.When==A+3600000 && H.Out.Candidates==2,"same-address native handoff and physical join");xrtThreadDestroy(T);xcronZoneRelease(Z);}
#endif
