static uint32 SelectedModelSeed=UINT32_C(0x73451acd);
static uint32 selectedModelRandom(void)
{SelectedModelSeed^=SelectedModelSeed<<13;SelectedModelSeed^=SelectedModelSeed>>17;SelectedModelSeed^=SelectedModelSeed<<5;return SelectedModelSeed;}
static int selectedModelOffset(int64 T,const int64* Times,const uint8* Indices,const int32* Offsets)
{unsigned I=0;while(I<8 && Times[I]<=T)++I;return Offsets[I?Indices[I-1]:0];}
static bool selectedModelMatch(int64 Local)
{int S=(int)(Local%60);if(S<0)S+=60;return S%17==0;}
static size_t selectedModelCount(int64 Local,const int64* Times,const uint8* Indices,const int32* Offsets,int64* First,int64* Last)
{size_t N=0;for(unsigned I=0;I<5;++I){int64 T=Local-Offsets[I];if(selectedModelOffset(T,Times,Indices,Offsets)==Offsets[I]){if(!N || T<*First)*First=T;if(!N || T>*Last)*Last=T;++N;}}return N;}
static void selectedModelLabel(const xdatetime* D,int64 Local)
{
    int64 Day=Local>=0?Local/86400:(Local+1)/86400-1;int S=(int)(Local-Day*86400);
    testRequire(Day>=-1 && Day<=0,"small independent civil model bounds");
    testRequire(D->Year==(Day?1969:1970) && D->Month==(Day?12:1) && D->Day==(Day?31:1) && D->Hour==S/3600 && D->Minute==S/60%60 && D->Second==S%60 && D->Weekday==(Day?3:4) && D->YearDay==(Day?365:1) && !D->Offset && D->IsDST==-1 && !D->Millisecond,"independent Gregorian nominal metadata");
}
static void testSelectedHistoryModel(void)
{
    const int32 Offsets[]={0,60,-60,120,-120};const tzifkind Types[]={{0,false,"UTC"},{60,false,"AAA"},{-60,false,"BBB"},{120,false,"CCC"},{-120,false,"DDD"}};xcronscheduler* S=selectedCreate(1,1,3);uint64 Mono=0;
    for(unsigned H=0;H<32;++H){int64 Times[8];uint8 Indices[8];for(unsigned I=0;I<8;++I){Times[I]=-100+(int64)I*50;Indices[I]=(uint8)(selectedModelRandom()%5);}Indices[7]=0;tzifblob Blob;tzifBuild(&Blob,'3',Times,Indices,8,Types,5,"UTC0");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"independent selected history fixture");
        for(unsigned W=0;W<8;++W){int64 Begin=-160+(int64)(selectedModelRandom()%400),End=Begin+1+selectedModelRandom()%80;uint32 Flags=W%2?XCRON_SEARCH_EXCLUSIVE:0;
            for(int Fold=0;Fold<4;++Fold)for(int Missing=0;Missing<2;++Missing){int64 Expected[128],Locals[128],FaultWhen=0,FaultLocal=0;size_t Counts[128],Count=0,FaultCount=0;xcronoccurrenceerror Fault=0;
                for(int64 T=Begin+(Flags?1:0);T<=End;++T){
                    if(Missing)for(unsigned J=0;J<8;++J)if(Times[J]==T){int Before=J?Offsets[Indices[J-1]]:Offsets[0],After=Offsets[Indices[J]];
                        for(int64 L=T+Before;L<T+After;++L){int64 First=0,Last=0;if(selectedModelMatch(L) && !selectedModelCount(L,Times,Indices,Offsets,&First,&Last)){Fault=XCRON_OCCURRENCE_MISSING;FaultWhen=T;FaultLocal=L;break;}}
                    }
                    if(Fault)break;
                    int64 L=T+selectedModelOffset(T,Times,Indices,Offsets);if(!selectedModelMatch(L))continue;int64 First=0,Last=0;size_t N=selectedModelCount(L,Times,Indices,Offsets,&First,&Last);
                    if(Fold==XCRON_FOLD_REJECT && N>1){Fault=XCRON_OCCURRENCE_AMBIGUOUS;FaultWhen=T;FaultLocal=L;FaultCount=N;break;}
                    if((Fold==XCRON_FOLD_FIRST && T!=First) || (Fold==XCRON_FOLD_LAST && T!=Last))continue;
                    testRequire(Count<128,"finite expected selected lease list");Expected[Count]=T;Locals[Count]=L;Counts[Count++]=N;
                }
                xcronjobconfig C=selectedConfig(Z,"*/17 * * * * *",XRT_TIME_UNIX_EPOCH+Begin*1000,XRT_TIME_UNIX_EPOCH+End*1000);C.SearchFlags=Flags;xcronjobid Id=selectedAdd(S,&C,(xcrondeliverypolicy){Fold,Missing});size_t Seen=0;unsigned Faults=0,Turns=0;xcronselectedjobinfo Info;
                do{xcronselectedrun R;xcronselectedpoll P=selectedPoll(S,C.End,Mono++,(xcronoccurrencebudget){1,1,1,1,1},&R,1,3);Faults+=P.Poll.Faults;
                    if(P.Poll.Count){testRequire(Seen<Count && R.Run.ScheduledAt==XRT_TIME_UNIX_EPOCH+Expected[Seen]*1000 && R.Run.Through==R.Run.ScheduledAt && R.Candidates==Counts[Seen],"independent ordered global selection produces every expected lease");selectedModelLabel(&R.Local,Locals[Seen]);++Seen;testRequire(xcronSchedulerComplete(S,R.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"independent trace physical completion");}
                    Info=selectedInfo(S,Id);testRequire(++Turns<4096,"finite budgeted independent selected trace");
                }while(Info.State==XCRON_JOB_SEARCHING || Info.State==XCRON_JOB_SCHEDULED);
                testRequire(Seen==Count && Faults==(unsigned)(Fault!=0) && Info.State==(Fault?XCRON_JOB_FAULTED:XCRON_JOB_EXHAUSTED) && Info.LastIssue.Code==Fault,"independent terminal selected trace status and fault count");
                if(Fault){testRequire(Info.LastIssue.When==XRT_TIME_UNIX_EPOCH+FaultWhen*1000 && Info.LastIssue.Candidates==FaultCount,"independent earliest fault UTC anchor/global multiplicity");selectedModelLabel(&Info.LastIssue.Local,FaultLocal);}
                xcronselectedretired R=selectedReap(S);testRequire(R.Info.Dispatched==Count && R.Info.Succeeded==Count && !R.Info.Running,"independent trace physical retirement counters");
            }
        }xcronZoneRelease(Z);
    }selectedClean(S);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct selectedhandoff {xcronscheduler* S;xtime Wall;xcronselectedrun Run;bool Found;} selectedhandoff;
static int32 selectedHandoffWorker(ptr Raw)
{selectedhandoff* H=Raw;for(unsigned T=1;T<256;++T){xcronselectedpoll P=selectedPoll(H->S,H->Wall,T,(xcronoccurrencebudget){1,1,1,1,1},&H->Run,1,1);if(P.Poll.Count){H->Found=true;break;}}return 0;}
static void testSelectedHandoff(void)
{
    xcronzone* Z=selectedZone("EST5EDT,M3.2.0,M11.1.0");xtime A=selectedStamp(2024,11,3,5,30);xcronscheduler* S=selectedCreate(1,1,1);xcronjobconfig C=selectedConfig(Z,"0 30 1 * * *",A,A+XRT_TIME_HOUR);selectedAdd(S,&C,(xcrondeliverypolicy){2,1});xcronZoneRelease(Z);
    xcronselectedpoll P=selectedPoll(S,C.End,0,(xcronoccurrencebudget){1,1,1,1,1},NULL,0,1);testRequire(P.Searching,"real pending selected query before serialized native handoff");selectedhandoff H={.S=S,.Wall=C.End};xthread* T=xrtThreadCreate(selectedHandoffWorker,&H,0);
    testRequire(T && xrtThreadWaitFor(T,15000)==XWAIT_OK && !xrtThreadExitCode(T) && H.Found && H.Run.Run.ScheduledAt==C.End && H.Run.Candidates==2,"serialized selected owner handoff retains complete metadata");xrtThreadDestroy(T);testRequire(xcronSchedulerComplete(S,H.Run.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"handoff physical completion");selectedClean(S);
}
#endif
