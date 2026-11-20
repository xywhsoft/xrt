#include "../../../dev/bench/bench_common.h"
#include "../examples/common/journal_files.h"
#include "../examples/common/store_compact_online.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
#include <xrt/thread.h>
#include <xrt/time.h>

enum { REQUEST_DEADLINE_MS=5000 };

typedef struct sample {
    double Due,Done;
    uint64 Key;
    xfuturewatch Watch;
    xatomic32 Notified;
    xfuture* Future;
} sample;

typedef struct run {
    xmqttstorewrite* Writer;
    xmqttstoredeltaconfig Delta;
    xbytesview Wire;
    sample* Pending;
    double *All,*CommittedLatency,*DispatchLag;
    size_t Window,Head,Active,PeakActive,Offered,Accepted,Committed;
    size_t WindowDrops,WorkerOverloads,Expired,Timeouts,LateCommits,AlreadyReady;
    uint64 LastKey;
    double LastTerminal;
} run;

static void require(bool OK,const char* Step)
{
    if(OK)return;
    const xerror* Error=xrtGetError();
    fprintf(stderr,"store capacity benchmark failed at %s%s%s\n",Step,
            Error?": ":"",Error?xrtErrorMessage(Error):"");
    exit(1);
}

static bool number(const char* Text,uint64 Min,uint64 Max,uint64* Out)
{
    if(*Text<'0'||*Text>'9')return false;
    char* End;
    errno=0;
    unsigned long long Value=strtoull(Text,&End,10);
    if(errno||*End||Value<Min||Value>Max)return false;
    *Out=(uint64)Value;
    return true;
}

static uint64 processCpu(void)
{
#if defined(_WIN32) || defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;
    require(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");
    ULARGE_INTEGER K,U;
    K.LowPart=Kernel.dwLowDateTime;
    K.HighPart=Kernel.dwHighDateTime;
    U.LowPart=User.dwLowDateTime;
    U.HighPart=User.dwHighDateTime;
    return (K.QuadPart+U.QuadPart)*100u;
#else
    struct timespec Time;
    require(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&Time)==0,"process CPU clock");
    return (uint64)Time.tv_sec*UINT64_C(1000000000)+(uint64)Time.tv_nsec;
#endif
}

static int compare(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

static double percentile(const double* Values,size_t N,size_t PerMille)
{
    if(!N)return 0;
    return Values[(N*PerMille+999)/1000-1];
}

static void notify(ptr Data)
{
    sample* S=Data;
    S->Done=xrtTimer();
    xrtAtomic32Store(&S->Notified,1,XMEMORY_RELEASE);
}

static void terminal(run* R,double Due,double Done)
{
    R->All[R->Offered++]=(Done-Due)*1e6;
    if(Done>R->LastTerminal)R->LastTerminal=Done;
}

static void retire(run* R)
{
    while(R->Active){
        sample* S=R->Pending+R->Head;
        if(!xrtAtomic32Load(&S->Notified,XMEMORY_ACQUIRE))break;
        xrtFutureWatchRemove(S->Future,&S->Watch);
        const xmqttcommitresult* Result=xrtFutureValue(S->Future);
        require(Result&&(Result->Status==XMQTT_COMMIT_OK||Result->Status==XMQTT_COMMIT_TIMEOUT),
            "terminal writer status");
        if(Result->Effect==XMQTT_JOURNAL_COMMITTED){
            require(Result->Sequence&&Result->AppendCalled,"committed transaction");
            require(S->Key>R->LastKey,"committed FIFO key");
            R->LastKey=S->Key;
            R->CommittedLatency[R->Committed++]=(S->Done-S->Due)*1e6;
            if(Result->Status==XMQTT_COMMIT_TIMEOUT)++R->LateCommits;
        }else require(Result->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&Result->Status==XMQTT_COMMIT_TIMEOUT,
            "definite deadline miss");
        if(Result->Status==XMQTT_COMMIT_TIMEOUT)++R->Timeouts;
        terminal(R,S->Due,S->Done);
        xrtFutureDestroy(S->Future);
        R->Head=(R->Head+1)%R->Window;
        --R->Active;
    }
}

static void drain(run* R)
{
    double Deadline=xrtTimer()+15;
    while(R->Active){
        retire(R);
        require(xrtTimer()<Deadline,"drain accepted observations");
        if(R->Active)xrtThreadYield();
    }
}

static void rotate(xmqttstorewrite* Writer,const mqttExampleCompactConfig* Config)
{
    xfuture* Future=NULL;
    require(mqttExampleCompactAsync(Writer,Config,10000,&Future)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(Future,10000)==XWAIT_OK,"submit APP maintenance at the transaction cap");
    const xmqttstorecheckpointresult* Result=xrtFutureValue(Future);
    require(Result&&Result->Status==XMQTT_COMMIT_OK&&Result->Effect==XMQTT_JOURNAL_COMMITTED&&
        Result->Sequence==1,"compact to one durable session checkpoint");
    xrtFutureDestroy(Future);
}

static void offer(run* R,uint64 Key,double Due)
{
    double Start=xrtTimer();
    R->DispatchLag[Key-1]=(Start-Due)*1e6;
    if(R->Active==R->Window){
        ++R->WindowDrops;
        terminal(R,Due,Start);
        return;
    }
    const xmqttstoredelta Ops[]={
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=Key,.Data=R->Wire},
        {.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=Key}
    };
    unsigned char Batch[768];
    size_t Bytes=0;
    require(xmqttStoreDeltaWrite(&R->Delta,Ops,2,NULL,0,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE,
        "encode one acceptance and ACK");
    sample* S=R->Pending+(R->Head+R->Active)%R->Window;
    xrtAtomic32Store(&S->Notified,0,XMEMORY_RELAXED);
    S->Due=Due;
    S->Key=Key;
    double Attempt=xrtTimer();
    double Left=REQUEST_DEADLINE_MS-(Attempt-Due)*1000.0;
    if(Left<1){
        ++R->Expired;
        terminal(R,Due,Attempt);
        return;
    }
    S->Future=NULL;
    xmqttjournaladmission Status=xmqttStoreWriteAppendAsync(R->Writer,(xbytesview){Batch,Bytes},(int64)Left,&S->Future);
    if(Status==XMQTT_JWORK_OVERLOADED){
        require(!S->Future,"refusal leaves Future untouched");
        ++R->WorkerOverloads;
        terminal(R,Due,xrtTimer());
        return;
    }
    require(Status==XMQTT_JWORK_ACCEPTED&&S->Future,"one bounded admission");
    require(xrtFutureWatchInit(&S->Watch,notify,NULL,S),"initialize completion watch");
    xfuturewatchresult Watch=xrtFutureWatchAdd(S->Future,&S->Watch);
    require(Watch!=XFUTURE_WATCH_ERROR,"register completion watch");
    if(Watch==XFUTURE_WATCH_READY){
        ++R->AlreadyReady;
        notify(S);
    }
    ++R->Accepted;
    if(++R->Active>R->PeakActive)R->PeakActive=R->Active;
}

static void establish(xmqttstorewrite* Writer,const xmqttstoredeltaconfig* Delta)
{
    const xmqttstoredelta Seed[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=2,.Data=XRT_BYTES_LITERAL("bench-capacity")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.Expiry=UINT32_MAX,.WallMilliseconds=2}
    };
    unsigned char Encoded[256];
    size_t Bytes=0;
    xfuture* Future=NULL;
    require(xmqttStoreDeltaWrite(Delta,Seed,2,NULL,0,Encoded,sizeof(Encoded),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttStoreWriteAppendAsync(Writer,(xbytesview){Encoded,Bytes},5000,&Future)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(Future,5000)==XWAIT_OK,"establish typed durable session");
    const xmqttcommitresult* Open=xrtFutureValue(Future);
    require(Open&&Open->Status==XMQTT_COMMIT_OK&&Open->Effect==XMQTT_JOURNAL_COMMITTED&&Open->Sequence==1,
        "verify BIND and OPEN");
    xrtFutureDestroy(Future);

    double Deadline=xrtTimer()+5;
    for(;;){
        xmqttstorewriteinfo Info;
        require(xmqttStoreWriteInfo(Writer,&Info)&&!Info.Worker.Failed,"query initial pump");
        if(!Info.Worker.Tasks&&!Info.Worker.Requests&&!Info.Worker.Busy)return;
        require(xrtTimer()<Deadline,"retire initial pump");
        xrtThreadYield();
    }
}

int main(int Argc,char** Argv)
{
    uint64 Count=1024,Rate=300,Window=16,Cap=0,Version=5,Instrument=0;
    if(Argc!=1&&(Argc!=7||!number(Argv[1],1,100000,&Count)||!number(Argv[2],1,50000,&Rate)||
       !number(Argv[3],1,128,&Window)||!number(Argv[4],0,100000,&Cap)||Cap==1||
       !number(Argv[5],4,5,&Version)||!number(Argv[6],0,1,&Instrument))){
        fputs("usage: bench_store_capacity COUNT RATE WINDOW TRANSACTION_CAP VERSION(4|5) INSTRUMENT(0|1)\n",stderr);
        return 2;
    }
    xbenchApplyCpuPinFromEnv();
    sample* Samples=calloc((size_t)Window,sizeof(*Samples));
    double *All=malloc((size_t)Count*sizeof(*All)),*Committed=malloc((size_t)Count*sizeof(*Committed)),
        *Dispatch=malloc((size_t)Count*sizeof(*Dispatch));
    require(Samples&&All&&Committed&&Dispatch,"allocate bounded observation storage");
    for(size_t I=0;I<Window;++I)xrtAtomic32Init(&Samples[I].Notified,0);

    const char* Parent=getenv("XMQTT_BENCH_DIRECTORY");
    str Path=xrtDirTemp(Parent,"xmqtt-capacity-",NULL);
    xroot Root=Path?xrtRootOpen(Path):NULL;
    require(Root!=NULL,"open native temp directory");
    const char* const Paths[]={"journal.slot","journal.0","journal.1"};
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    for(size_t I=0;I<3;++I){
        xfile File=xrtRootFileOpen(Root,Paths[I],&Options);
        require(File&&xrtClose(File),"provision exact slot files");
    }

    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Version=(xmqttversion)Version;
    JC.Generation=1;
    JC.Binding=XRT_BYTES_LITERAL("bench/native-capacity");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    JC.MaxTransactionBytes=8192;
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Binding=JC.Binding;
    SC.ClientId=XRT_STR_LITERAL("bench-capacity");
    SC.Generation=1;
    SC.WallMilliseconds=2;
    SC.Delta.Version=JC.Version;
    SC.Delta.MaxOperations=2;
    SC.Delta.Codec.MaxProperties=0;
    SC.MaxOutgoing=SC.MaxIncoming=0;
    SC.MaxInbox=1;
    SC.MaxTransactions=Cap?Cap:Count+1;
    xmqttjournalslotfiles Files={0};
    xmqttjournalslot* Slot=NULL;
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    require(mqttExampleOpenNativeSlot(Root,&JC,true,&Files,&Slot)==XMQTT_JOURNAL_DONE&&
        xmqttJournalSlotOpenCurrent(Slot,&Journal)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE,"recover empty native slot");
    xtaskpoolconfig PoolConfig={.Threads=1,.QueueLimit=8};
    xtaskpool* Pool=xrtTaskPoolCreate(&PoolConfig);
    xmqttstorewriteconfig Config;
    xmqttStoreWriteConfigInit(&Config);
    xmqttstorewrite* Writer=NULL;
    require(Pool&&xmqttStoreWriteCreate(Pool,Journal,State,&Config,&Writer),"create bounded Store Writer");
    establish(Writer,&SC.Delta);
    mqttExampleCompactConfig Compact;
    mqttExampleCompactConfigInit(&Compact);
    Compact.MinReclaimBytes=1;
    Compact.MaxEntries=1;
    Compact.MaxScratchBytes=256;
    uint64 Rotations=0;

    unsigned char Payload[256]={0},Wire[512];
    xmqttpacket Packet;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
    Packet.Body.Publish.Topic=XRT_STR_LITERAL("bench/capacity");
    Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    size_t WireBytes=0;
    require((Version==5?xmqttPacketWrite5: xmqttPacketWrite311)(Wire,sizeof(Wire),&Packet,
            XMQTT_SERVER_TO_CLIENT,NULL,&WireBytes),"encode canonical QoS 0 payload");
    run R={.Writer=Writer,.Delta=SC.Delta,.Wire={Wire,WireBytes},.Pending=Samples,.All=All,
        .CommittedLatency=Committed,.DispatchLag=Dispatch,.Window=(size_t)Window};
    xtaskpoolstats PoolBefore,PoolAfter;
    require(xrtTaskPoolGet(Pool,&PoolBefore),"read initial task counters");
    xmemstats Before={0},After={0};
    if(Instrument){
        xrtMemStatsEnable(true);
        xrtMemStatsGet(&Before);
    }

    uint64 Cpu=processCpu();
    double Start=xrtTimer();
    for(uint64 Key=1;Key<=Count;++Key){
        double Due=Start+(double)(Key-1)/(double)Rate;
        for(;;){
            retire(&R);
            double Remaining=Due-xrtTimer();
            if(Remaining<=0)break;
            if(Remaining>.020)xrtSleep(1);
            else xrtThreadYield();
        }
        if(Cap&&R.Committed+1+R.Active-Rotations*(Cap-1)>=Cap){
            /* Pending writes count against available room. Their completion
             * can expose capacity without a checkpoint. The arrival clock
             * keeps running while the APP drains or rotates. */
            drain(&R);
            if(R.Committed+1-Rotations*(Cap-1)>=Cap){
                rotate(Writer,&Compact);
                ++Rotations;
            }
        }
        offer(&R,Key,Due);
    }
    drain(&R);
    require(xmqttStoreWriteClose(Writer)&&xmqttStoreWriteWaitFor(Writer,10000)==XWAIT_OK,"retire all native tasks");
    double Elapsed=xrtTimer()-Start;
    Cpu=processCpu()-Cpu;
    if(Instrument){
        xrtMemStatsGet(&After);
        xrtMemStatsEnable(false);
    }
    xmqttstorewriteinfo Info;
    require(xmqttStoreWriteInfo(Writer,&Info)&&xrtTaskPoolGet(Pool,&PoolAfter),
        "read final attendance and maintenance counters");
    require(!Info.Worker.Failed&&!Info.Worker.Requests&&!Info.Worker.Tasks&&!Info.Worker.Objects&&
        Info.Worker.Accepted==R.Accepted+1+Rotations&&Info.Worker.Completed==Info.Worker.Accepted&&
        R.Offered==Count&&R.Accepted+R.WindowDrops+R.WorkerOverloads+R.Expired==Count&&
        R.Committed+R.Timeouts-R.LateCommits==R.Accepted&&R.Committed!=0&&
        Info.State.LastKey==R.LastKey&&!Info.State.Inbox,"reconcile every scheduled operation");
    uint64 ExpectedTransactions=R.Committed+1;
    if(Cap){
        require(Rotations<=ExpectedTransactions/(Cap-1),"check APP checkpoint outcomes");
        ExpectedTransactions-=Rotations*(Cap-1);
    }
    require(Info.State.Transactions==ExpectedTransactions&&Info.Worker.Journal.Sequence==ExpectedTransactions,
        "check selected journal sequence after foreground commits");

    require(xmqttStoreWriteDestroy(Writer)&&mqttExampleCloseNativeSlot(Slot,&Files)&&
        xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool),
        "release native writer and task pool");
    double Reopen=xrtTimer();
    require(mqttExampleOpenNativeSlot(Root,&JC,false,&Files,&Slot)==XMQTT_JOURNAL_DONE,"cold reopen selector");
    xmqttjournalslotinfo Selected;
    require(xmqttJournalSlotInfo(Slot,&Selected)&&Selected.Generation==Rotations+1,"recover selected generation");
    SC.Generation=Selected.Generation;
    require(xmqttJournalSlotOpenCurrent(Slot,&Journal)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE,"cold replay at unchanged bounds");
    double ReplayUs=(xrtTimer()-Reopen)*1e6;
    xmqttstorestateinfo Recovered;
    require(xmqttStoreStateInfo(State,&Recovered)&&Recovered.LastKey==R.LastKey&&
        Recovered.Transactions==ExpectedTransactions&&!Recovered.Inbox,"verify final recovered cut");
    xmqttStoreStateDestroy(State);
    require(xmqttJournalDestroy(Journal)&&mqttExampleCloseNativeSlot(Slot,&Files),"retire recovery owners");

    qsort(All,(size_t)Count,sizeof(*All),compare);
    qsort(Committed,R.Committed,sizeof(*Committed),compare);
    qsort(Dispatch,(size_t)Count,sizeof(*Dispatch),compare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\n"
           "latency_p999_us: %.3f\nlatency_max_us: %.3f\nall_arrival_p99_us: %.3f\ndispatch_lag_p99_us: %.3f\n"
           "process_cpu_ns_per_op: %.3f\ncold_recovery_us: %.3f\n",
           R.Committed/Elapsed,percentile(Committed,R.Committed,500),percentile(Committed,R.Committed,950),
           percentile(Committed,R.Committed,990),percentile(Committed,R.Committed,999),Committed[R.Committed-1],
           percentile(All,(size_t)Count,990),percentile(Dispatch,(size_t)Count,990),(double)Cpu/R.Committed,ReplayUs);
    printf("offered: %" PRIu64 "\noffer_rate_hz: %" PRIu64 "\nwindow: %" PRIu64 "\ntransaction_cap: %" PRIu64
           "\naccepted: %zu\ncommitted: %zu\nwindow_drops: %zu\nworker_overloads: %zu\nexpired_before_admission: %zu\n"
           "timeouts: %zu\nlate_commits: %zu\nalready_ready: %zu\npeak_in_flight: %zu\ncheckpoint_attempts: %" PRIu64
           "\nrotations: %" PRIu64 "\nselected_file_bytes: %" PRIu64 "\nrecovered_transactions: %" PRIu64
           "\nrecovered_generation: %" PRIu64 "\nprocess_cpu_ns: %" PRIu64 "\nphysical_phase_seconds: %.9f\n"
           "terminal_phase_seconds: %.9f\nnative_tasks: %" PRIu64 "\nmodel_peak_bytes: %zu\n"
           "observer_bytes: %zu\nwire_bytes: %zu\ndeadline_ms: %d\nbench_path: %s\n",
           Count,Rate,Window,SC.MaxTransactions,R.Accepted,R.Committed,R.WindowDrops,R.WorkerOverloads,R.Expired,
           R.Timeouts,R.LateCommits,R.AlreadyReady,R.PeakActive,Rotations,Rotations,
           Info.Worker.Journal.CommittedEnd,ExpectedTransactions,Selected.Generation,Cpu,Elapsed,
           R.LastTerminal-Start,(uint64)(PoolAfter.Submitted-PoolBefore.Submitted),Info.State.PeakMemoryBytes,
           (size_t)Window*sizeof(*Samples)+(size_t)Count*3*sizeof(*All),WireBytes,REQUEST_DEADLINE_MS,Path);
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",
        (double)(After.BlockAllocCalls-Before.BlockAllocCalls)/R.Committed,
        (double)(After.BlockAllocBytes-Before.BlockAllocBytes)/R.Committed);
    for(size_t I=0;I<3;++I)require(xrtRootRemove(Root,Paths[I]),"remove exact slot file");
    require(xrtRootClose(Root)&&xrtDirRemove(Path),"remove native scratch directory");
    xrtFree(Path);
    free(Samples);
    free(All);
    free(Committed);
    free(Dispatch);
    return 0;
}
