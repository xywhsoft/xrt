/* Public-API state model. Virtual time explores interleavings; this is not a
 * native I/O, independent wire implementation, or real-duration acceptance. */
#define TEST_SESSION_FAULTS
#define main xrudp_default_session_reference_main
#include "test_session.c"
#undef main
#include <stdlib.h>

#define MODEL_IDS 64u
#define MODEL_OPENS 12u
typedef struct model_half {
    uint64_t Written, Read, HighestSent, HighestReceived;
    uint64_t StopDeadline, ResetDeadline;
    uint32_t MessagesWritten, MessagesRead, Target;
    bool Active, Retired, Fin, Cancelled, Eof;
} model_half;
typedef struct model_trace { uint32_t At, Side, Id, Op; } model_trace;
static struct {
    model_half Half[2][MODEL_IDS]; uint8_t Kind[MODEL_IDS]; bool Created[MODEL_IDS], Protected[MODEL_IDS];
    uint32_t Seed, Opened[2], TraceCount;
    uint64_t Opens, Retires, Stops, Resets, Writes, Reads, Truncated, Forgeries, NoMemory, Backpressure;
    model_trace Trace[32];
} model;
static int model_failure(unsigned Line, const char* Expression)
{
    fprintf(stderr,"model seed=%u tick=%u line=%u: %s\n",model.Seed,tick,Line,Expression);
    uint32_t Count=model.TraceCount<32 ? model.TraceCount:32;
    for (uint32_t I=0;I<Count;++I) {
        const model_trace* T=&model.Trace[(model.TraceCount-Count+I)%32];
        fprintf(stderr,"  at=%u side=%u stream=%u op=%u\n",T->At,T->Side,T->Id,T->Op);
    }
    return 1;
}
#undef CHECK
#define CHECK(E) do { if (!(E)) return model_failure(__LINE__,#E); } while (0)
static void trace(unsigned Side,unsigned Id,unsigned Op)
{ model.Trace[model.TraceCount++%32]=(model_trace){tick,Side,Id,Op}; }
static unsigned char model_byte(unsigned Side,unsigned Id,uint64_t Position)
{ return (unsigned char)((Side*59u+Id*83u+Position*37u)^(Position>>7)); }
static size_t model_message(uint32_t Number)
{ const unsigned Size[4]={0,1,63,127}; return Size[Number%4]; }
static xrudpresult model_output(const void* Data,size_t Size,void* Context)
{
    session_output* O=Context;
    if (O->OutOfMemory) ++model.NoMemory;
    return send_packet(Data,Size,Context);
}
static int open_stream(unsigned Side,bool First)
{
    if (model.Opened[Side]>=MODEL_OPENS) return 0;
    xrudpstreamconfig C; xrtRudpStreamConfigInit(&C);
    C.Kind=First ? XRUDP_STREAM_MESSAGES:(uint8_t)(random_next()%2);
    C.Priority=(uint8_t)(random_next()%4); C.Weight=(uint16_t)(1+random_next()%16);
    uint32_t Id=0; xrudpresult R=xrtRudpSessionStreamOpen(net.Session[Side],&C,&Id);
    if (R==XRUDP_BACKPRESSURE) { ++model.Backpressure; return 0; }
    CHECK(R==XRUDP_OK && Id>1 && Id<MODEL_IDS && !model.Created[Id]);
    model.Created[Id]=true; model.Kind[Id]=C.Kind; model.Protected[Id]=First;
    for (unsigned I=0;I<2;++I) model.Half[I][Id].Target=C.Kind==XRUDP_STREAM_MESSAGES ? 12:512;
    model.Half[Side][Id].Active=true; ++model.Opened[Side]; ++model.Opens; trace(Side,Id,10);
    return 0;
}
static int events(unsigned Side)
{
    xrudpstreamevent E;
    for (unsigned I=0;I<32;++I) {
        xrudpresult R=xrtRudpSessionStreamNextEvent(net.Session[Side],&E);
        if (R==XRUDP_DONE) return 0;
        CHECK(R==XRUDP_OK && E.Id>0 && E.Id<MODEL_IDS && model.Created[E.Id]);
        CHECK(!model.Half[Side][E.Id].Retired);
        model.Half[Side][E.Id].Active=true;
    }
    CHECK(false); return 1;
}
static int write_data(unsigned Side,unsigned Id)
{
    model_half* M=&model.Half[Side][Id]; unsigned char Bytes[128];
    if (M->Fin || M->Cancelled) return 0;
    size_t Size; uint64_t Position;
    if (model.Kind[Id]==XRUDP_STREAM_MESSAGES) {
        if (M->MessagesWritten==M->Target) return 0;
        Size=model_message(M->MessagesWritten); Position=(uint64_t)M->MessagesWritten*131;
    } else {
        if (M->Written==M->Target) return 0;
        Size=1+random_next()%sizeof(Bytes); if (Size>M->Target-M->Written) Size=(size_t)(M->Target-M->Written);
        Position=M->Written;
    }
    for (size_t I=0;I<Size;++I) Bytes[I]=model_byte(Side,Id,Position+I);
    size_t Written=0; xrudpresult R;
    if (model.Kind[Id]==XRUDP_STREAM_MESSAGES) {
        R=xrtRudpSessionMessageWrite(net.Session[Side],Id,Bytes,Size);
        if (R==XRUDP_OK) { Written=Size; ++M->MessagesWritten; }
    } else R=xrtRudpSessionStreamWrite(net.Session[Side],Id,Bytes,Size,&Written);
    memset(Bytes,0,sizeof(Bytes));
    if (R==XRUDP_BACKPRESSURE) { CHECK(Written==0); ++model.Backpressure; return 0; }
    if (R==XRUDP_CANCELLED) { CHECK(model.Half[Side][Id].Cancelled && Written==0); return 0; }
    CHECK(R==XRUDP_OK); M->Written+=Written; ++model.Writes;
    return 0;
}
static int read_data(unsigned Side,unsigned Id)
{
    model_half* M=&model.Half[Side][Id]; const model_half* Sender=&model.Half[1-Side][Id];
    if (M->Eof) return 0;
    unsigned char Bytes[128]; size_t Count=0,Capacity=1+random_next()%sizeof(Bytes);
    xrudpresult R=model.Kind[Id]==XRUDP_STREAM_MESSAGES ?
        xrtRudpSessionMessageRead(net.Session[Side],Id,Bytes,Capacity,&Count):
        xrtRudpSessionStreamRead(net.Session[Side],Id,Bytes,Capacity,&Count);
    if (R==XRUDP_BACKPRESSURE) { CHECK(Count==0); return 0; }
    if (R==XRUDP_TRUNCATED) {
        CHECK(model.Kind[Id]==XRUDP_STREAM_MESSAGES && Count>Capacity && Count==model_message(M->MessagesRead));
        size_t Needed=0; CHECK(xrtRudpSessionMessagePeek(net.Session[Side],Id,&Needed)==XRUDP_OK && Needed==Count);
        ++model.Truncated; return 0;
    }
    if (R==XRUDP_DONE || R==XRUDP_CANCELLED) {
        CHECK(Count==0);
        if (R==XRUDP_CANCELLED) CHECK(Sender->Cancelled);
        if (!Sender->Cancelled) CHECK(M->Read==Sender->Written && M->MessagesRead==Sender->MessagesWritten);
        M->Eof=true; return 0;
    }
    CHECK(R==XRUDP_OK && Count<=Capacity && M->Read+Count<=Sender->Written);
    uint64_t Position=M->Read;
    if (model.Kind[Id]==XRUDP_STREAM_MESSAGES) {
        CHECK(M->MessagesRead<Sender->MessagesWritten && Count==model_message(M->MessagesRead));
        Position=(uint64_t)M->MessagesRead*131; ++M->MessagesRead;
    } else CHECK(Count>0);
    for (size_t I=0;I<Count;++I) CHECK(Bytes[I]==model_byte(1-Side,Id,Position+I));
    M->Read+=Count; ++model.Reads; return 0;
}
static int control(unsigned Side,unsigned Id,unsigned Op)
{
    if (model.Protected[Id]) return 0;
    model_half* M=&model.Half[Side][Id]; xrudpstreamstate S;
    CHECK(xrtRudpSessionStreamState(net.Session[Side],Id,&S)==XRUDP_OK);
    if (Op==6 && S.Receive.Stopped) {
        CHECK(xrtRudpSessionStreamStop(net.Session[Side],Id,17)==XRUDP_DUPLICATE);
        CHECK(xrtRudpSessionStreamStop(net.Session[Side],Id,18)==XRUDP_ARGUMENT);
        return 0;
    }
    if (Op==7 && S.ResetQueued) {
        CHECK(xrtRudpSessionStreamReset(net.Session[Side],Id,S.ResetReason)==XRUDP_DUPLICATE);
        CHECK(xrtRudpSessionStreamReset(net.Session[Side],Id,S.ResetReason+1)==XRUDP_ARGUMENT);
        return 0;
    }
    if (Op==6 && !M->Eof && !S.Receive.Stopped && !S.Receive.Reset) {
        xrudpresult R=xrtRudpSessionStreamStop(net.Session[Side],Id,17);
        CHECK(R==XRUDP_OK || R==XRUDP_DUPLICATE);
        if (R==XRUDP_OK) {
            model.Half[1-Side][Id].Cancelled=true; ++model.Stops;
            CHECK(xrtRudpSessionStreamStop(net.Session[Side],Id,17)==XRUDP_DUPLICATE);
            CHECK(xrtRudpSessionStreamStop(net.Session[Side],Id,18)==XRUDP_ARGUMENT);
        }
    } else if (Op==7 && !S.ResetQueued && !S.WriteDone) {
        CHECK(xrtRudpSessionStreamReset(net.Session[Side],Id,19)==XRUDP_OK);
        M->Cancelled=true; ++model.Resets;
        CHECK(xrtRudpSessionStreamReset(net.Session[Side],Id,19)==XRUDP_DUPLICATE);
        CHECK(xrtRudpSessionStreamReset(net.Session[Side],Id,20)==XRUDP_ARGUMENT);
    } else if (Op==5 && !M->Fin && !M->Cancelled) {
        CHECK(xrtRudpSessionStreamFinish(net.Session[Side],Id)==XRUDP_OK); M->Fin=true;
    }
    return 0;
}
static int invariants(unsigned Side)
{
    uint64_t Received=0,Sent=0; unsigned Active=0,StopWaiters=0,ResetWaiters=0;
    for (unsigned Id=1;Id<MODEL_IDS;++Id) {
        model_half* M=&model.Half[Side][Id];
        if (M->Active) {
            xrudpstreamstate S; CHECK(xrtRudpSessionStreamState(net.Session[Side],Id,&S)==XRUDP_OK);
            CHECK(S.Kind==model.Kind[Id] && S.Receive.BufferedBytes<=S.Receive.Capacity && S.Transmit.PendingBytes<=S.Transmit.Capacity);
            CHECK(S.Receive.HighestOffset>=M->HighestReceived && S.Transmit.HighestSent>=M->HighestSent);
            CHECK(S.Transmit.HighestSent<=M->Written+(uint64_t)M->MessagesWritten*4);
            if (M->StopDeadline && S.ReceiveStopDeadlineUs) CHECK(M->StopDeadline==S.ReceiveStopDeadlineUs);
            if (M->ResetDeadline && S.ResetDeadlineUs) CHECK(M->ResetDeadline==S.ResetDeadlineUs);
            CHECK(!(S.Receive.Stopped && !S.Receive.HasFinal) || S.ReceiveStopDeadlineUs);
            CHECK(!S.ResetQueued || S.ResetAcknowledged || S.ResetDeadlineUs);
            M->StopDeadline=S.ReceiveStopDeadlineUs; M->ResetDeadline=S.ResetDeadlineUs;
            StopWaiters+=S.ReceiveStopDeadlineUs!=0; ResetWaiters+=S.ResetDeadlineUs!=0;
            M->HighestReceived=S.Receive.HighestOffset; M->HighestSent=S.Transmit.HighestSent; ++Active;
            if (Id>1 && S.RetireReady && M->Eof) {
                CHECK(S.ReadDone && S.WriteDone);
                CHECK(xrtRudpSessionStreamRetire(net.Session[Side],Id)==XRUDP_OK);
                M->Active=false; M->Retired=true; --Active; ++model.Retires; trace(Side,Id,11);
            }
        }
        Received+=M->HighestReceived; Sent+=M->HighestSent;
    }
    xrudpsessionstate S; CHECK(xrtRudpSessionState(net.Session[Side],&S)==XRUDP_OK && S.Reason==XRUDP_OK);
    CHECK(S.ActiveStreams==Active && Active<=7 && S.PendingStreamEvents<=Active);
    CHECK(S.ReceivedConnection==Received && S.SentConnection==Sent);
    CHECK(S.ReceivedConnection<=S.ReceiveLimit && S.SentConnection<=S.PeerLimit);
    CHECK(S.PendingReceiveStops==StopWaiters && S.PendingSendResets==ResetWaiters);
    CHECK(S.Sent.BytesInFlight==S.Congestion.BytesInFlight);
    CHECK(S.Sent.NormalCapacity==16 && S.Sent.TotalCapacity==16+XRUDP_SENT_PROBE_RESERVE);
    CHECK(S.Sent.PendingPackets<=S.Sent.TotalCapacity && S.Sent.StoredPackets<=S.Sent.TotalCapacity && S.OutstandingProbes<=2);
    return 0;
}
static int deliver(uint64_t Now)
{
    for (size_t I=0;I<TEST_QUEUE;++I) if (net.Queue[I].Used && net.Queue[I].Due<=tick) {
        session_packet P=net.Queue[I]; net.Queue[I].Used=false;
        if (random_next()%11==0) {
            unsigned char Bad[XRUDP_PACKET_MAX]; memcpy(Bad,P.Data,P.Size); Bad[random_next()%P.Size]^=1;
            xrudpsessionstate Before,After;
            CHECK(xrtRudpSessionState(net.Session[P.Target],&Before)==XRUDP_OK);
            CHECK(xrtRudpSessionInput(net.Session[P.Target],Bad,P.Size,Now)==XRUDP_AUTH_FAILED);
            CHECK(xrtRudpSessionState(net.Session[P.Target],&After)==XRUDP_OK && After.Reason==XRUDP_OK);
            CHECK(Before.ReceivedConnection==After.ReceivedConnection && Before.ReceivedPackets==After.ReceivedPackets && Before.PeerLimit==After.PeerLimit);
            ++model.Forgeries;
        }
        xrudpresult R=xrtRudpSessionInput(net.Session[P.Target],P.Data,P.Size,Now);
        CHECK(R==XRUDP_OK || R==XRUDP_DUPLICATE || R==XRUDP_TOO_OLD || R==XRUDP_BACKPRESSURE);
    }
    return 0;
}
static bool drained(void)
{
    for (unsigned Side=0;Side<2;++Side) for (unsigned Id=1;Id<MODEL_IDS;++Id) if (model.Created[Id]) {
        const model_half* M=&model.Half[Side][Id];
        if (!M->Active && !M->Retired) return false;
        if (M->Active) { xrudpstreamstate S; if (xrtRudpSessionStreamState(net.Session[Side],Id,&S)!=XRUDP_OK || !M->Eof || !S.WriteDone || !S.ReadDone) return false; }
    }
    return true;
}
static int run_model(uint32_t Seed,unsigned Steps)
{
    xmemdebugsnapshot Before,After; xrtMemDebugSnapshot(&Before);
    memset(&model,0,sizeof(model)); memset(&net,0,sizeof(net)); model.Seed=randomState=Seed; tick=0;
    xrudpsessionconfig C; configure(&C,512,512);
    C.StreamCapacity=C.Local.MaxStreams=C.Peer.MaxStreams=4;
    C.Local.MaxMessageSize=C.Peer.MaxMessageSize=127;
    C.Local.StreamCredit=C.Peer.StreamCredit=128;
    C.Local.ConnectionCredit=C.Peer.ConnectionCredit=384;
    C.PacketCapacity=16; C.ProgressTimeoutUs=30000000;
    xrudpcipher* Cipher[2]; CHECK(raw_pair(Cipher)==0);
    for (unsigned Side=0;Side<2;++Side) {
        C.Role=Side ? XRUDP_RESPONDER:XRUDP_INITIATOR; net.Output[Side].Index=Side; net.Output[Side].Impair=true;
        CHECK(xrtRudpSessionCreate(&C,Cipher[Side],0,model_output,&net.Output[Side],&net.Session[Side])==XRUDP_OK);
        model.Half[Side][1].Active=true; model.Half[Side][1].Target=4096;
    }
    model.Created[1]=model.Protected[1]=true;
    CHECK(xrtMemDebugFailAfter(0)); CHECK(open_stream(0,true)==0); CHECK(open_stream(1,false)==0);
    bool Done=false;
    for (tick=1;tick<=Steps+30000 && !Done;++tick) {
        uint64_t Now=(uint64_t)tick*1000; CHECK(deliver(Now)==0);
        for (unsigned Side=0;Side<2;++Side) {
            CHECK(events(Side)==0);
            net.Output[Side].OutOfMemory=tick<Steps && tick%131<3;
            if (tick<Steps && random_next()%11==0) CHECK(open_stream(Side,false)==0);
            if (tick<Steps) {
                unsigned Id=1+random_next()%(MODEL_IDS-1),Op=random_next()%8;
                if (model.Half[Side][Id].Active) {
                    trace(Side,Id,Op);
                    if (Op<3) CHECK(write_data(Side,Id)==0);
                    else if (Op<5) CHECK(read_data(Side,Id)==0);
                    else CHECK(control(Side,Id,Op)==0);
                }
            } else for (unsigned Id=1;Id<MODEL_IDS;++Id) if (model.Half[Side][Id].Active) {
                model_half* M=&model.Half[Side][Id]; CHECK(write_data(Side,Id)==0); CHECK(read_data(Side,Id)==0);
                bool Queued=model.Kind[Id]==XRUDP_STREAM_MESSAGES ? M->MessagesWritten==M->Target:M->Written==M->Target;
                if (Queued && !M->Fin && !M->Cancelled) { CHECK(xrtRudpSessionStreamFinish(net.Session[Side],Id)==XRUDP_OK); M->Fin=true; }
            }
            uint64_t Due; size_t Calls=net.Output[Side].Calls;
            CHECK(xrtRudpSessionPoll(net.Session[Side],Now,&Due)==XRUDP_OK && Due>Now);
            CHECK(net.Output[Side].Calls-Calls<=XRUDP_SESSION_POLL_BURST);
            CHECK(events(Side)==0); CHECK(invariants(Side)==0);
        }
        CHECK(!net.Overflow); Done=tick>=Steps && drained();
    }
    CHECK(Done && model.Opened[0]==MODEL_OPENS && model.Opened[1]==MODEL_OPENS);
    CHECK(model.Stops && model.Resets && model.Retires>12 && model.Truncated && model.Forgeries && model.NoMemory);
    CHECK(net.Dropped && net.Duplicated && net.Blocked && model.Backpressure);
    for (unsigned Side=0;Side<2;++Side) {
        CHECK(model.Half[Side][1].Read==4096);
        CHECK(xrtRudpSessionAbort(net.Session[Side],XRUDP_CANCELLED)==XRUDP_OK);
    }
    CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair(); xrtClearError();
    xrtMemDebugSnapshot(&After); CHECK(Before.LiveCount==After.LiveCount && Before.LiveBytes==After.LiveBytes);
    printf("model seed=%u steps=%u ticks=%u open=%llu retire=%llu stop=%llu reset=%llu reads=%llu writes=%llu forged=%llu local-nomem=%llu balanced=1\n",
        Seed,Steps,tick,(unsigned long long)model.Opens,(unsigned long long)model.Retires,(unsigned long long)model.Stops,
        (unsigned long long)model.Resets,(unsigned long long)model.Reads,(unsigned long long)model.Writes,
        (unsigned long long)model.Forgeries,(unsigned long long)model.NoMemory); fflush(stdout); return 0;
}
int main(int Argc,char** Argv)
{
    uint32_t Seed=Argc>1 ? (uint32_t)strtoul(Argv[1],NULL,0):0x87165943;
    unsigned Steps=Argc>2 ? (unsigned)strtoul(Argv[2],NULL,10):20000;
    if (!Seed || Steps<1000 || Steps>200000) return 2;
    return run_model(Seed,Steps);
}
