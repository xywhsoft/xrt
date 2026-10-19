/* Persistent public-API native UDP soak. Synthetic loopback identities only.
 * The companion driver enforces real wall time and process resource bounds. */
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <xrudp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct soak_test soak_test;
typedef struct soak_stream {
    uint64_t Written, Read, Target, MessagesWritten, MessagesRead, MessagesTarget;
    bool Active, Fin, Eof, Acked;
} soak_stream;
typedef struct soak_side {
    soak_test* Test; unsigned Index;
    soak_stream Stream[6];
    uint64_t GamesWritten[3], GamesRead[3], LastGame[3], Seen[3][128];
    uint64_t Backpressure, Hints, Pauses; bool Shutdown;
} soak_side;
/* Owner publishes immutable work; main submits across threads; owner alone
 * consumes the result and updates stream counters. No raw/async mixing on
 * the message directions. Release/acquire state protects both hand-offs. */
enum { JOB_IDLE, JOB_SUBMIT, JOB_CLAIMED, JOB_PENDING };
typedef struct soak_future_job {
    xatomic32 State;
    xrudpoperation Request;
    xfuture* Future;
    unsigned char Bytes[4097];
} soak_future_job;
struct soak_test {
    xnetengine* Engine; xrudpendpoint* Endpoint[2]; xrudppeer* Peer[2];
    xrudpendpointinfo Info[2]; soak_side Side[2];
    unsigned char Private[2][32], Public[2][32];
    xatomic32 Ready, Confirmed, PeerClosed, Closed, Pending, Failed;
    double Start, LastTick, FeedAt, GameAt, ReportAt, IdleAt, IdleUntil, PauseAt, PauseUntil;
    unsigned Seconds, IdleEvery, IdleSeconds, PauseEvery, ReportEvery, Idles;
    xnetfamily Family; bool Closing, FutureMode; unsigned PeerCloses[2], EndpointCloses[2];
    soak_future_job Jobs[2][2][2]; /* side, message stream 4/5, read/write */
};

static void require(soak_test* T, bool Ok, const char* Why)
{
    if (!Ok && !xrtAtomic32Exchange(&T->Failed,1,XMEMORY_ACQ_REL)) {
        fprintf(stderr,"native soak: %s\n",Why); fflush(stderr);
    }
}
static unsigned char pattern(unsigned Side, unsigned Id, uint64_t Offset)
{ return (unsigned char)((Side*79u+Id*23u+Offset*37u)^(Offset>>6)); }
static size_t message_size(uint64_t Number)
{ return Number%3==0 ? 0:(Number%3==1 ? 513:4097); }
static void opened(xrudpendpoint* E, void* Data)
{
    soak_side* S=Data; soak_test* T=S->Test;
    require(T,xrtRudpEndpointInfo(E,&T->Info[S->Index])==XRUDP_OK,"endpoint info");
    (void)xrtAtomic32FetchOr(&T->Ready,1u<<S->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* E, xrudppeer* P, void* Data)
{
    soak_side* S=Data; soak_test* T=S->Test; (void)E;
    require(T,!(xrtAtomic32Load(&T->Confirmed,XMEMORY_ACQUIRE)&(1u<<S->Index)),"peer confirmed twice");
    if (!T->Peer[S->Index]) T->Peer[S->Index]=xrtRudpPeerRef(P);
    else require(T,T->Peer[S->Index]==P,"initiating peer changed");
    S->Stream[1].Active=true;
    for (unsigned I=0;I<3;++I) {
        xrudpdatagramchannel C; xrtRudpDatagramChannelInit(&C);
        C.Id=7+I; C.Mode=(uint8_t)I; C.ReplacePending=I==2; C.ReceiveLifetimeUs=500000;
        require(T,xrtRudpPeerDatagramConfigure(P,&C)==XRUDP_OK,"configure games");
    }
    for (unsigned I=0;I<2;++I) {
        xrudpstreamconfig C; xrtRudpStreamConfigInit(&C); C.Kind=(uint8_t)I; C.Priority=(uint8_t)I; C.Weight=(uint16_t)(I+1);
        uint32_t Id=0; require(T,xrtRudpPeerStreamOpen(P,&C,&Id)==XRUDP_OK,"open persistent stream");
        require(T,Id==2+S->Index+2*I,"origin stream ID");
        if (Id>=2 && Id<6) S->Stream[Id].Active=true;
    }
    (void)xrtAtomic32FetchOr(&T->Confirmed,1u<<S->Index,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* E, xrudppeer* P, uint32_t Events, void* Data)
{ soak_side* S=Data; (void)E; (void)P; (void)Events; ++S->Hints; }
static void stream_changed(xrudpendpoint* E, xrudppeer* P, const xrudpstreamevent* V, void* Data)
{
    soak_side* S=Data; (void)E; ++S->Hints;
    require(S->Test,V->Id>0 && V->Id<6,"unexpected stream"); if (!V->Id || V->Id>=6) return;
    xrudpstreamstate State={0};
    require(S->Test,xrtRudpPeerStreamState(P,V->Id,&State)==XRUDP_OK,"stream state");
    require(S->Test,State.Kind==(V->Id<4 ? XRUDP_STREAM_BYTES:XRUDP_STREAM_MESSAGES),"stream kind");
    S->Stream[V->Id].Active=true;
    if (V->Events&XRUDP_STREAM_WRITE_DONE) S->Stream[V->Id].Acked=true;
}
static void peer_closed(xrudpendpoint* E, xrudppeer* P, xrudpresult Reason, void* Data)
{
    soak_side* S=Data; soak_test* T=S->Test; (void)E; xrudpsessionstate State;
    require(T,++T->PeerCloses[S->Index]==1,"duplicate peer close");
    require(T,T->Closing && Reason==XRUDP_CLOSED,"peer closed before final drain");
    require(T,xrtRudpPeerSessionState(P,&State)==XRUDP_OK && State.Shutdown.Succeeded &&
        !State.Sent.PendingPackets && !State.Sent.BytesInFlight,"no graceful drain proof");
    (void)xrtAtomic32FetchOr(&T->PeerClosed,1u<<S->Index,XMEMORY_RELEASE);
}
static void closed(xrudpendpoint* E, xrudpresult Reason, const xerror* Error, void* Data)
{
    soak_side* S=Data; xrudpasyncstate State; (void)Error;
    require(S->Test,Reason==XRUDP_CANCELLED && ++S->Test->EndpointCloses[S->Index]==1,"endpoint terminal");
    require(S->Test,xrtRudpEndpointAsyncState(E,&State)==XRUDP_OK && !State.Pending && !State.Retained && !State.RetainedBytes,
        "Future quota retained at endpoint close");
    (void)xrtAtomic32FetchOr(&S->Test->Closed,1u<<S->Index,XMEMORY_RELEASE);
}
static void bind_one(xnetworker* W, ptr Data)
{
    soak_side* S=Data; soak_test* T=S->Test; xrudpendpointconfig C; xrtRudpEndpointConfigInit(&C);
    C.StaticPrivate=T->Private[S->Index]; C.PeerPublic=T->Public[1-S->Index]; C.Listen=S->Index==0;
    if (T->FutureMode) { C.AsyncOperations=8; C.AsyncBytes=65536; }
    C.MaxPeers=C.MaxHalfOpen=2; C.Session.StreamCapacity=C.Session.Local.MaxStreams=3;
    C.Session.Local.MaxMessageSize=4097; C.Session.ReceiveCapacity=2048; C.Session.TransmitCapacity=8192;
    C.Session.Local.StreamCredit=1024; C.Session.Local.ConnectionCredit=4096;
    C.Session.Local.IdleTimeoutMs=30000; C.Session.KeepAliveUs=5000000; C.Session.ProgressTimeoutUs=10000000;
    C.Session.KeyUpdatePackets=128; C.Session.Local.MaxAckDelayUs=1000;
    C.Session.Datagrams.Channels=3; C.Session.Datagrams.SendCapacity=C.Session.Datagrams.ReceiveCapacity=8;
    C.Session.Datagrams.PayloadCapacity=128;
    xnetaddr A; require(T,xrtNetAddrLoopback(&A,T->Family,0),"loopback");
    xrudpendpointevents V={opened,confirmed,peer_closed,closed,changed,stream_changed};
    require(T,xrtRudpEndpointBind(W,&A,&C,&V,S,&T->Endpoint[S->Index])==XRUDP_OK,"bind");
}
static void dial(xnetworker* W, ptr Data)
{
    soak_test* T=Data; (void)W; xrudpconnectconfig C={T->Info[0].Public,T->Info[0].Scope,NULL};
    require(T,xrtRudpEndpointConnect(T->Endpoint[1],&T->Info[0].Local,&C,&T->Peer[1])==XRUDP_OK,"dial");
}
static void read_stream(soak_side* S, unsigned Id)
{
    soak_test* T=S->Test; soak_stream* V=&S->Stream[Id]; unsigned char Bytes[8192];
    if (V->Eof) return;
    for (unsigned Round=0;Round<8;++Round) {
        size_t Count=0; xrudpresult R=Id<4 ? xrtRudpPeerStreamRead(T->Peer[S->Index],Id,Bytes,512,&Count):
            xrtRudpPeerMessageRead(T->Peer[S->Index],Id,Bytes,sizeof(Bytes),&Count);
        if (R==XRUDP_BACKPRESSURE) break;
        if (R==XRUDP_DONE) { V->Eof=true; break; }
        require(T,R==XRUDP_OK,"read persistent stream"); if (R!=XRUDP_OK) break;
        uint64_t Offset=Id<4 ? V->Read:V->MessagesRead*4097;
        require(T,Id>=4 ? Count==message_size(V->MessagesRead):Count>0,"message boundary or empty bytes");
        for (size_t I=0;I<Count;++I) require(T,Bytes[I]==pattern(1-S->Index,Id,Offset+I),"corrupt/duplicate stream bytes");
        V->Read+=Count; if (Id>=4) ++V->MessagesRead;
    }
}
static void write_stream(soak_side* S, unsigned Id)
{
    soak_test* T=S->Test; soak_stream* V=&S->Stream[Id]; unsigned char Bytes[8192];
    if (V->Fin) return;
    size_t Size; uint64_t Offset;
    if (Id<4) { Size=(size_t)(V->Target-V->Written); if (Size>sizeof(Bytes)) Size=sizeof(Bytes); Offset=V->Written; }
    else { if (V->MessagesWritten==V->MessagesTarget) return; Size=message_size(V->MessagesWritten); Offset=V->MessagesWritten*4097; }
    if (Id<4 && !Size) return;
    for (size_t I=0;I<Size;++I) Bytes[I]=pattern(S->Index,Id,Offset+I);
    size_t Written=0; xrudpresult R;
    if (Id<4) R=xrtRudpPeerStreamWrite(T->Peer[S->Index],Id,Bytes,Size,&Written);
    else { R=xrtRudpPeerMessageWrite(T->Peer[S->Index],Id,Bytes,Size); if (R==XRUDP_OK) { Written=Size; ++V->MessagesWritten; } }
    if (R==XRUDP_BACKPRESSURE) { ++S->Backpressure; return; }
    require(T,R==XRUDP_OK,"write persistent stream"); if (R==XRUDP_OK) V->Written+=Written;
}
static void future_result(soak_side* S, unsigned Id, unsigned Direction)
{
    soak_test* T=S->Test; soak_stream* V=&S->Stream[Id]; soak_future_job* J=&T->Jobs[S->Index][Id-4][Direction];
    if (xrtAtomic32Load(&J->State,XMEMORY_ACQUIRE)!=JOB_PENDING || xrtFutureState(J->Future)==XFUTURE_PENDING) return;
    const xrudpoperationresult* R=xrtFutureState(J->Future)==XFUTURE_RESOLVED ? xrtFutureValue(J->Future):NULL;
    require(T,R && (R->Result==XRUDP_OK || (!Direction && R->Result==XRUDP_DONE)),"Future message outcome");
    if (R && R->Result==XRUDP_OK) {
        uint64_t N=Direction ? V->MessagesWritten:V->MessagesRead; size_t Size=message_size(N);
        require(T,R->Size==Size,"Future message boundary");
        if (Direction) { V->Written+=R->Size; ++V->MessagesWritten; }
        else {
            require(T,!Size || R->Data!=NULL,"Future read owns payload");
            if (R->Data) for (size_t I=0;I<R->Size;++I)
                require(T,((const unsigned char*)R->Data)[I]==pattern(1-S->Index,Id,N*4097+I),"Future corrupt/duplicate message");
            V->Read+=R->Size; ++V->MessagesRead;
        }
    } else if (R && R->Result==XRUDP_DONE) V->Eof=true;
    xrtFutureDestroy(J->Future); J->Future=NULL;
    xrtAtomic32Store(&J->State,JOB_IDLE,XMEMORY_RELEASE);
}
static void future_message(soak_side* S, unsigned Id, bool Read)
{
    soak_test* T=S->Test; soak_stream* V=&S->Stream[Id];
    for (unsigned Direction=0;Direction<2;++Direction) {
        future_result(S,Id,Direction);
        soak_future_job* J=&T->Jobs[S->Index][Id-4][Direction];
        if (xrtAtomic32Load(&J->State,XMEMORY_ACQUIRE)!=JOB_IDLE) continue;
        if (Direction ? (V->Fin || V->MessagesWritten==V->MessagesTarget):(!Read || V->Eof)) continue;
        xrtRudpOperationInit(&J->Request); J->Request.Stream=Id;
        if (Direction) {
            J->Request.Kind=XRUDP_OPERATION_MESSAGE_WRITE; J->Request.Size=message_size(V->MessagesWritten);
            for (size_t I=0;I<J->Request.Size;++I) J->Bytes[I]=pattern(S->Index,Id,V->MessagesWritten*4097+I);
            J->Request.Data=J->Bytes;
        } else { J->Request.Kind=XRUDP_OPERATION_MESSAGE_READ; J->Request.Capacity=sizeof(J->Bytes); }
        xrtAtomic32Store(&J->State,JOB_SUBMIT,XMEMORY_RELEASE);
    }
}
static void submit_futures(soak_test* T)
{
    if (!T->FutureMode) return;
    for (unsigned Side=0;Side<2;++Side) for (unsigned Stream=0;Stream<2;++Stream) for (unsigned Direction=0;Direction<2;++Direction) {
        soak_future_job* J=&T->Jobs[Side][Stream][Direction]; uint32 Expected=JOB_SUBMIT;
        if (!xrtAtomic32CompareExchange(&J->State,&Expected,JOB_CLAIMED,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) continue;
        J->Future=xrtRudpPeerSubmitAsync(T->Peer[Side],&J->Request);
        require(T,J->Future!=NULL,"cross-thread Future submission");
        if (Direction) memset(J->Bytes,0,sizeof(J->Bytes)); /* submission must copy input */
        xrtAtomic32Store(&J->State,J->Future ? JOB_PENDING:JOB_IDLE,XMEMORY_RELEASE);
    }
}
static void games(soak_side* S, bool Send, bool Read)
{
    soak_test* T=S->Test; unsigned char Bytes[64];
    if (Send) for (unsigned C=0;C<3;++C) {
        uint64_t N=S->GamesWritten[C]+1, Sequence=0;
        for (unsigned I=0;I<8;++I) Bytes[I]=(unsigned char)(N>>(I*8));
        for (size_t I=8;I<sizeof(Bytes);++I) Bytes[I]=pattern(S->Index,7+C,N*64+I);
        xrudpresult R=xrtRudpPeerDatagramWrite(T->Peer[S->Index],7+C,Bytes,sizeof(Bytes),200000,&Sequence);
        if (R==XRUDP_BACKPRESSURE) { ++S->Backpressure; continue; }
        require(T,R==XRUDP_OK && Sequence==(C ? N:0),"game write/sequence"); if (R==XRUDP_OK) ++S->GamesWritten[C];
    }
    if (Read) for (unsigned Round=0;Round<16;++Round) {
        xrudpdatagraminfo I; xrudpresult R=xrtRudpPeerDatagramRead(T->Peer[S->Index],Bytes,sizeof(Bytes),&I);
        if (R==XRUDP_BACKPRESSURE) break;
        if (T->Closing && S->Shutdown && (R==XRUDP_DONE || R==XRUDP_CLOSED)) break;
        require(T,R==XRUDP_OK && I.Channel>=7 && I.Channel<=9 && I.Size==sizeof(Bytes),"game read"); if (R!=XRUDP_OK || I.Channel<7 || I.Channel>9 || I.Size!=sizeof(Bytes)) break;
        unsigned C=I.Channel-7; uint64_t N=0; for (unsigned J=0;J<8;++J) N|=(uint64_t)Bytes[J]<<(J*8);
        require(T,N>0 && N<=T->Side[1-S->Index].GamesWritten[C] && S->Seen[C][N%128]!=N,"game duplicate/unknown");
        require(T,C ? I.Sequenced && I.Sequence==N && N>S->LastGame[C]:!I.Sequenced && !I.Sequence,"game mode/order");
        for (size_t J=8;J<sizeof(Bytes);++J) require(T,Bytes[J]==pattern(1-S->Index,7+C,N*64+J),"game payload");
        S->Seen[C][N%128]=N; S->LastGame[C]=N; ++S->GamesRead[C];
    }
}
static void report(soak_test* T, double Now)
{
    uint64_t Read=0, Written=0, Messages=0, Games=0, Backpressure=0, Rekeys=0, Packets=0;
    uint64_t Submitted=0, Completed=0; uint32 Pending=0, Retained=0; size_t Bytes=0;
    for (unsigned Side=0;Side<2;++Side) {
        soak_side* S=&T->Side[Side]; xrudpsessionstate State={0};
        require(T,xrtRudpPeerSessionState(T->Peer[Side],&State)==XRUDP_OK,"session snapshot");
        require(T,(State.ActiveStreams==5 || T->Closing) && State.Datagrams.SendQueued<=8 && State.Datagrams.ReceiveQueued<=8,"fixed resource bounds");
        Rekeys+=State.KeyUpdates; Packets+=State.SentPackets; Backpressure+=S->Backpressure;
        for (unsigned Id=1;Id<6;++Id) { Read+=S->Stream[Id].Read; Written+=S->Stream[Id].Written; Messages+=S->Stream[Id].MessagesRead; }
        for (unsigned C=0;C<3;++C) Games+=S->GamesRead[C];
        xrudpasyncstate Async={0};
        require(T,xrtRudpEndpointAsyncState(T->Endpoint[Side],&Async)==XRUDP_OK && Async.Pending<=Async.Retained &&
            Async.Retained<=Async.Limit && Async.RetainedBytes<=Async.ByteLimit,"Future resource bounds");
        Submitted+=Async.Submitted; Completed+=Async.Completed; Pending+=Async.Pending; Retained+=Async.Retained; Bytes+=Async.RetainedBytes;
    }
    printf("{\"phase\":\"%s\",\"elapsed\":%.3f,\"read\":%llu,\"written\":%llu,\"messages\":%llu,\"games\":%llu,\"backpressure\":%llu,\"rekeys\":%llu,\"packets\":%llu,\"idle_checks\":%u,"
        "\"future_mode\":%s,\"future_submitted\":%llu,\"future_completed\":%llu,\"future_pending\":%u,\"future_retained\":%u,\"future_bytes\":%zu}\n",
        T->Closing ? "draining":(T->IdleUntil ? "idle":"running"),Now-T->Start,
        (unsigned long long)Read,(unsigned long long)Written,(unsigned long long)Messages,(unsigned long long)Games,
        (unsigned long long)Backpressure,(unsigned long long)Rekeys,(unsigned long long)Packets,T->Idles,T->FutureMode ? "true":"false",
        (unsigned long long)Submitted,(unsigned long long)Completed,Pending,Retained,Bytes); fflush(stdout);
}
static void tick(xnetworker* W, ptr Data)
{
    soak_test* T=Data; (void)W; double Now=xrtTimer();
    require(T,!T->LastTick || Now-T->LastTick<15,"worker execution gap"); T->LastTick=Now;
    if (xrtAtomic32Load(&T->Confirmed,XMEMORY_ACQUIRE)!=3 || xrtAtomic32Load(&T->PeerClosed,XMEMORY_ACQUIRE)) goto done;
    for (unsigned Side=0;Side<2;++Side) for (unsigned Id=1;Id<6;++Id) if (!T->Side[Side].Stream[Id].Active) goto done;
    if (!T->Start) {
        T->Start=Now; T->FeedAt=T->GameAt=T->ReportAt=Now; T->IdleAt=Now+T->IdleEvery; T->PauseAt=Now+T->PauseEvery;
        puts("{\"phase\":\"ready\",\"persistent_streams\":5,\"peers\":2}"); fflush(stdout);
    }
    if (!T->Closing && Now-T->Start>=T->Seconds) {
        T->Closing=true; puts("{\"phase\":\"drain_begin\"}"); fflush(stdout);
    }
    if (!T->Closing && !T->IdleUntil && Now>=T->IdleAt) { T->IdleUntil=Now+T->IdleSeconds; T->FeedAt=T->GameAt=T->IdleUntil; }
    if (T->IdleUntil && Now>=T->IdleUntil) { T->IdleUntil=0; T->IdleAt=Now+T->IdleEvery; ++T->Idles; }
    if (!T->Closing && Now>=T->PauseAt) { T->PauseUntil=Now+2; T->PauseAt=Now+T->PauseEvery; ++T->Side[1].Pauses; }
    bool Feed=!T->Closing && !T->IdleUntil && Now>=T->FeedAt;
    bool SendGames=!T->Closing && !T->IdleUntil && Now>=T->GameAt;
    if (Feed) T->FeedAt=Now+.5;
    if (SendGames) T->GameAt=Now+.1;
    bool Drained=true;
    for (unsigned Side=0;Side<2;++Side) {
        soak_side* S=&T->Side[Side]; bool Read=T->Closing || Side==0 || Now>=T->PauseUntil;
        for (unsigned Id=1;Id<6;++Id) {
            soak_stream* V=&S->Stream[Id];
            if (Feed) {
                if (Id<4 && V->Target-V->Written<8192) V->Target+=4096;
                if (Id>=4 && V->MessagesTarget-V->MessagesWritten<4) ++V->MessagesTarget;
            }
            if (T->FutureMode && Id>=4) future_message(S,Id,Read);
            else { write_stream(S,Id); if (Read) read_stream(S,Id); }
            bool Queued=Id<4 ? V->Written==V->Target:V->MessagesWritten==V->MessagesTarget;
            if (T->Closing && Queued && !V->Fin) { require(T,xrtRudpPeerStreamFinish(T->Peer[Side],Id)==XRUDP_OK,"final FIN"); V->Fin=true; }
            if (!V->Eof || !V->Acked || !V->Fin) Drained=false;
            if (V->Eof) require(T,V->Read==T->Side[1-Side].Stream[Id].Written &&
                V->MessagesRead==T->Side[1-Side].Stream[Id].MessagesWritten,"final exact delivery");
        }
        games(S,SendGames,Read);
    }
    if (Now>=T->ReportAt) { report(T,Now); T->ReportAt=Now+T->ReportEvery; }
    if (T->Closing && Drained) for (unsigned Side=0;Side<2;++Side) if (!T->Side[Side].Shutdown) {
        T->Side[Side].Shutdown=true; require(T,xrtRudpPeerShutdown(T->Peer[Side],5000)==XRUDP_OK,"graceful shutdown");
    }
done:
    xrtAtomic32Store(&T->Pending,0,XMEMORY_RELEASE);
}
static void stop(xnetworker* W, ptr Data)
{
    soak_test* T=Data; (void)W;
    for (unsigned Side=0;Side<2;++Side) for (unsigned Stream=0;Stream<2;++Stream) for (unsigned Direction=0;Direction<2;++Direction) {
        soak_future_job* J=&T->Jobs[Side][Stream][Direction];
        xrtFutureDestroy(J->Future); J->Future=NULL; xrtAtomic32Store(&J->State,JOB_IDLE,XMEMORY_RELEASE);
    }
    for (unsigned I=0;I<2;++I) if (T->Endpoint[I]) require(T,xrtRudpEndpointClose(T->Endpoint[I])==XRUDP_OK,"endpoint close request");
}
static bool wait_mask(soak_test* T, xatomic32* Value, unsigned Mask, double Seconds)
{
    double Until=xrtTimer()+Seconds;
    while ((xrtAtomic32Load(Value,XMEMORY_ACQUIRE)&Mask)!=Mask) {
        if (xrtTimer()>=Until || xrtAtomic32Load(&T->Failed,XMEMORY_ACQUIRE)) return false;
        xrtSleep(1);
    }
    return true;
}
int main(int Argc, char** Argv)
{
    if (Argc!=9 && Argc!=10) return 2;
    soak_test T={0}; T.Seconds=(unsigned)strtoul(Argv[1],NULL,10); T.Family=atoi(Argv[2])==6 ? XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
    T.FutureMode=Argc==10 && atoi(Argv[9])!=0;
    T.IdleEvery=(unsigned)atoi(Argv[4]); T.IdleSeconds=(unsigned)atoi(Argv[5]); T.PauseEvery=(unsigned)atoi(Argv[6]); T.ReportEvery=(unsigned)atoi(Argv[7]);
    unsigned ProxyPort=(unsigned)atoi(Argv[8]); if (ProxyPort>65535) return 2;
    if (T.Seconds<20 || T.Seconds>604800 || !T.IdleEvery || !T.IdleSeconds || !T.PauseEvery || !T.ReportEvery) return 2;
    xrtAtomic32Init(&T.Ready,0); xrtAtomic32Init(&T.Confirmed,0); xrtAtomic32Init(&T.PeerClosed,0);
    xrtAtomic32Init(&T.Closed,0); xrtAtomic32Init(&T.Pending,0); xrtAtomic32Init(&T.Failed,0);
    for (unsigned Side=0;Side<2;++Side) for (unsigned Stream=0;Stream<2;++Stream) for (unsigned Direction=0;Direction<2;++Direction)
        xrtAtomic32Init(&T.Jobs[Side][Stream][Direction].State,JOB_IDLE);
    for (unsigned I=0;I<2;++I) {
        T.Side[I].Test=&T; T.Side[I].Index=I;
        for (unsigned J=0;J<32;++J) T.Private[I][J]=(unsigned char)(J+13+I*79);
        if (!xrtX25519Public(T.Private[I],T.Public[I])) return 3;
    }
    xnetengineconfig C; xrtNetEngineConfigInit(&C); C.Workers=1; C.Backend=XNET_PORT_SELECT;
    if (atoi(Argv[3])) {
#if defined(_WIN32)
        C.Backend=XNET_PORT_IOCP;
#else
        C.Backend=XNET_PORT_EPOLL;
#endif
    }
    T.Engine=xrtNetEngineCreate(&C); if (!T.Engine || !xrtNetEngineStart(T.Engine)) return 3;
    for (unsigned I=0;I<2;++I) require(&T,xrtNetEnginePost(T.Engine,0,bind_one,&T.Side[I]),"post bind");
    if (wait_mask(&T,&T.Ready,3,10)) {
        printf("{\"phase\":\"bound\",\"ports\":[%u,%u]}\n",T.Info[0].Local.Port,T.Info[1].Local.Port); fflush(stdout);
        if (ProxyPort) {
            char Go[8]={0}; require(&T,fgets(Go,sizeof(Go),stdin)!=NULL && !strcmp(Go,"GO\n"),"proxy startup barrier");
            T.Info[0].Local.Port=(uint16_t)ProxyPort;
        }
        if (!xrtAtomic32Load(&T.Failed,XMEMORY_ACQUIRE)) require(&T,xrtNetEnginePost(T.Engine,0,dial,&T),"post dial");
    } else require(&T,false,"bind deadline");
    double Until=xrtTimer()+T.Seconds+40;
    while (!xrtAtomic32Load(&T.Failed,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&T.PeerClosed,XMEMORY_ACQUIRE) && xrtTimer()<Until) {
        submit_futures(&T);
        if (!xrtAtomic32Exchange(&T.Pending,1,XMEMORY_ACQ_REL)) require(&T,xrtNetEnginePost(T.Engine,0,tick,&T),"post tick");
        xrtSleep(20);
    }
    /* Do not read owner-only counters until the final owner task has drained. */
    bool AllPeers=wait_mask(&T,&T.PeerClosed,3,10);
    require(&T,AllPeers,"graceful peer closure deadline");
    require(&T,xrtNetEnginePost(T.Engine,0,stop,&T),"post stop");
    double End=xrtTimer()+10;
    while (xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE)!=3 && xrtTimer()<End) xrtSleep(1);
    if (xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE)!=3) return 4;
    End=xrtTimer()+10;
    while (!xrtNetEngineStop(T.Engine)) { if (xrtTimer()>=End) return 5; xrtSleep(1); }
    while (!xrtNetEngineDestroy(T.Engine)) { if (xrtTimer()>=End) return 5; xrtSleep(1); }
    require(&T,T.Start>0 && T.Closing && T.PeerCloses[0]==1 && T.PeerCloses[1]==1,"persistent session did not finish");
    if (T.Start>0 && T.Peer[0] && T.Peer[1]) report(&T,T.LastTick);
    for (unsigned I=0;I<2;++I) { xrtRudpPeerDestroy(T.Peer[I]); xrtRudpEndpointDestroy(T.Endpoint[I]); }
    xrtSecureZero(T.Private,sizeof(T.Private));
    bool Ok=!xrtAtomic32Load(&T.Failed,XMEMORY_ACQUIRE);
    printf("{\"phase\":\"terminal\",\"passed\":%s,\"joined\":true,\"duration_passed\":%s,\"idle_checks\":%u}\n",
        Ok ? "true":"false",T.Start>0 && T.LastTick-T.Start>=T.Seconds ? "true":"false",T.Idles); fflush(stdout);
    return Ok ? 0:6;
}
