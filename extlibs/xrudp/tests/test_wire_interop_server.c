/* Native public-API peer for an independent Python wire/Noise implementation.
 * Synthetic fixed identities only; no private headers, account or model IO. */
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <xrudp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct wire_slot {
    uint32_t Id; size_t Read, Written;
    unsigned MessagesRead, MessagesWritten;
    bool Finished, Eof, Acked;
} wire_slot;
typedef struct wire_test {
    xnetengine* Engine; xrudpendpoint* Endpoint; xrudppeer* Peer;
    xatomic32 Opened, Closed, PeerClosed, AppDone, Failure, CheckPending;
    uint16_t Port, RemotePort; xnetfamily Family; size_t Read;
    unsigned char Pin[32];
    unsigned Confirmed, Closes; bool Eof, Acked;
    bool Typed, Probe, StopControl, StopVerified;
    bool Graceful, ShutdownStarted, ShutdownVerified;
    bool PathTest, PathVerified; uint16_t InitialPeerPort; unsigned PathChanges;
    bool MtuTest, MtuLimited, MtuStarted, MtuVerified; xrudpmtustate Mtu;
    unsigned Games, StreamOpened; xrudpresult CloseReason, ExpectedClose;
    wire_slot Slots[6];
} wire_test;
static void start_business(wire_test* T, xrudppeer* P);
static void require(wire_test* T, bool Ok, const char* What)
{ if (!Ok) { fprintf(stderr,"wire native failure: %s\n",What); xrtAtomic32Store(&T->Failure,1,XMEMORY_RELEASE); } }
static void complete(wire_test* T)
{
    if (!T->Eof || T->Read!=3072 || !T->Acked) return;
    if (T->StopControl && !T->StopVerified) return;
    if (T->Typed) {
        if (T->Games!=3) return;
        for (unsigned I=2;I<6;++I) {
            wire_slot* S=&T->Slots[I];
            if (S->Id!=I || !S->Eof || !S->Acked || (I<4 ? S->Read!=2304u+I:S->MessagesRead!=3)) return;
        }
    }
    if (T->Graceful) {
        if (T->ShutdownStarted) return;
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(T->Peer,&S)==XRUDP_OK,"pre-shutdown state");
        /* Shutdown cancels queued games. Wait for transport receipts for all
         * three echoes; the independent peer also validates their contents. */
        if (S.DatagramAcknowledged<3) return;
        require(T,xrtRudpPeerShutdown(T->Peer,5000)==XRUDP_OK,"graceful shutdown request");
        T->ShutdownStarted=true;
    }
    xrtAtomic32Store(&T->AppDone,1,XMEMORY_RELEASE);
}
static void check_complete(xnetworker* W, ptr Data)
{
    wire_test* T=Data; (void)W;
    if (!xrtAtomic32Load(&T->PeerClosed,XMEMORY_ACQUIRE)) {
        if (T->MtuTest && T->Confirmed && T->Peer && !T->MtuStarted) {
            xrudpsessionstate S={0};
            require(T,xrtRudpPeerSessionState(T->Peer,&S)==XRUDP_OK,"MTU owner snapshot");
            uint32_t Want=T->MtuLimited ? 768u:1200u;
            if (S.Mtu.Enabled && S.Mtu.BaseConfirmed && S.Mtu.Effective==Want &&
                !S.Mtu.ProbePacket && (!T->MtuLimited || S.Mtu.ProbeFailures==3)) {
                require(T,S.Mtu.Ceiling==1200 && S.Mtu.ProbesSent==(T->MtuLimited ? 5u:4u) &&
                    S.Mtu.ProbesAcknowledged==(T->MtuLimited ? 2u:4u) && !S.Mtu.Fallbacks &&
                    S.Mtu.ProbeFailures==(T->MtuLimited ? 3u:0u),"exact authenticated MTU evidence");
                T->MtuStarted=true; start_business(T,T->Peer);
            }
        }
        complete(T);
    }
    xrtAtomic32Store(&T->CheckPending,0,XMEMORY_RELEASE);
}
static void typed_write(wire_test* T, xrudppeer* P, uint32_t Id)
{
    wire_slot* S=&T->Slots[Id]; unsigned char Buffer[8192];
    if (S->Finished) return;
    if (Id<4) {
        size_t Size=1536u+Id, Written=0;
        for (size_t I=0;I<Size;++I) Buffer[I]=(unsigned char)(I*7u+19u+Id);
        xrudpresult R=xrtRudpPeerStreamWrite(P,Id,Buffer+S->Written,Size-S->Written,&Written);
        if (R==XRUDP_BACKPRESSURE) return;
        require(T,R==XRUDP_OK && Written,"typed bytes write"); S->Written+=Written;
        if (S->Written!=Size) return;
    } else for (;S->MessagesWritten<3;++S->MessagesWritten) {
        size_t Size=S->MessagesWritten==0 ? 0:(S->MessagesWritten==1 ? 513:4097);
        for (size_t I=0;I<Size;++I) Buffer[I]=(unsigned char)(I*7u+19u+Id+S->MessagesWritten);
        xrudpresult R=xrtRudpPeerMessageWrite(P,Id,Buffer,Size);
        if (R==XRUDP_BACKPRESSURE) return;
        require(T,R==XRUDP_OK,"typed message write"); if (R!=XRUDP_OK) return;
    }
    require(T,xrtRudpPeerStreamFinish(P,Id)==XRUDP_OK,"typed FIN"); S->Finished=true;
}
static void stream_changed(xrudpendpoint* E, xrudppeer* P, const xrudpstreamevent* V, void* Data)
{
    wire_test* T=Data; (void)E; uint32_t Id=V->Id;
    if (T->Probe) {
        if (Id!=1 && (V->Events&XRUDP_STREAM_OPENED)) ++T->StreamOpened;
        require(T,T->StreamOpened==0,"probe packet exposed an OPEN before full validation");
        require(T,!(V->Events&XRUDP_STREAM_READ_READY),"probe packet exposed stream data");
        return;
    }
    if (!T->Typed || Id==1) return;
    require(T,Id>=2 && Id<6,"typed stream ID"); if (Id<2 || Id>=6) return;
    wire_slot* S=&T->Slots[Id]; S->Id=Id;
    xrudpstreamstate State={0};
    require(T,xrtRudpPeerStreamState(P,Id,&State)==XRUDP_OK,"typed state");
    require(T,State.Kind==(Id<4 ? XRUDP_STREAM_BYTES:XRUDP_STREAM_MESSAGES),"typed stream kind");
    typed_write(T,P,Id);
    if (V->Events&(XRUDP_STREAM_READ_READY|XRUDP_STREAM_READ_EOF|XRUDP_STREAM_READ_DONE)) for (unsigned N=0;N<8;++N) {
        unsigned char Buffer[8192]; size_t Read=0;
        xrudpresult R=Id<4 ? xrtRudpPeerStreamRead(P,Id,Buffer,512,&Read):xrtRudpPeerMessageRead(P,Id,Buffer,sizeof(Buffer),&Read);
        if (R==XRUDP_BACKPRESSURE) break;
        if (R==XRUDP_DONE) { S->Eof=true; break; }
        require(T,R==XRUDP_OK,"typed read"); if (R!=XRUDP_OK) break;
        if (Id<4) {
            require(T,Read!=0 && S->Read+Read<=2304u+Id,"typed byte count");
            for (size_t I=0;I<Read;++I) require(T,Buffer[I]==(unsigned char)((S->Read+I)*11u+3u+Id),"typed input bytes");
            S->Read+=Read;
        } else {
            size_t Want=S->MessagesRead==0 ? 0:(S->MessagesRead==1 ? 513:4097);
            require(T,S->MessagesRead<3 && Read==Want,"whole/empty message count");
            for (size_t I=0;I<Read;++I) require(T,Buffer[I]==(unsigned char)(I*11u+3u+Id+S->MessagesRead),"typed input message");
            ++S->MessagesRead;
        }
    }
    if (V->Events&XRUDP_STREAM_WRITE_DONE) S->Acked=true;
    complete(T);
}
static void opened(xrudpendpoint* E, void* Data)
{
    wire_test* T=Data; xrudpendpointinfo I={0};
    require(T,xrtRudpEndpointInfo(E,&I)==XRUDP_OK,"endpoint info"); T->Port=I.Local.Port;
    if (T->RemotePort) {
        unsigned char Scope[16], Context[32]; memset(Scope,17,sizeof(Scope)); memset(Context,22,sizeof(Context));
        xnetaddr A; require(T,xrtNetAddrLoopback(&A,T->Family,T->RemotePort),"remote address");
        xrudpconnectconfig C={T->Pin,Scope,Context};
        require(T,xrtRudpEndpointConnect(E,&A,&C,&T->Peer)==XRUDP_OK,"connect");
    }
    xrtAtomic32Store(&T->Opened,1,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* E, xrudppeer* P, uint32_t Events, void* Data)
{
    wire_test* T=Data; (void)E;
    if (T->Probe) {
        unsigned char Buffer[512]; size_t Read=0; xrudpdatagraminfo I;
        xrudpresult R=xrtRudpPeerRead(P,Buffer,sizeof(Buffer),&Read);
        T->Read+=Read;
        require(T,R!=XRUDP_OK && Read==0,"probe packet exposed default stream data");
        R=xrtRudpPeerDatagramRead(P,Buffer,sizeof(Buffer),&I);
        if (R==XRUDP_OK) ++T->Games;
        require(T,T->Games==0,"probe packet exposed a game datagram");
        return;
    }
    if (T->PathTest && (Events&XRUDP_PEER_PATH_CHANGED)) {
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK,"changed path state");
        require(T,S.Path.Validations==++T->PathChanges && T->PathChanges<=2,"unique path change notification");
        require(T,(T->PathChanges==1 && S.Path.Active.Port!=T->InitialPeerPort) ||
            (T->PathChanges==2 && S.Path.Active.Port==T->InitialPeerPort),"A to B to A active address");
    }
    if (Events&XRUDP_PEER_READ_READY) for (unsigned N=0;N<8;++N) {
        unsigned char Buffer[512]; size_t Read=0; xrudpresult R=xrtRudpPeerRead(P,Buffer,sizeof(Buffer),&Read);
        if (R==XRUDP_BACKPRESSURE) break;
        if (R==XRUDP_DONE) { T->Eof=true; break; }
        require(T,R==XRUDP_OK && Read!=0,"reliable read"); if (R!=XRUDP_OK || !Read) break;
        for (size_t I=0;I<Read;++I) require(T,Buffer[I]==(unsigned char)((T->Read+I)*11u+3u),"client bytes");
        T->Read+=Read; require(T,T->Read<=3072,"extra/duplicate bytes");
    }
    if (Events&XRUDP_PEER_READ_EOF) {
        unsigned char Byte; size_t Read=0;
        if (xrtRudpPeerRead(P,&Byte,1,&Read)==XRUDP_DONE) T->Eof=true;
    }
    if (Events&XRUDP_PEER_WRITE_DONE) {
        T->Acked=true;
        if (T->StopControl) {
            xrudpstreamstate S={0};
            require(T,xrtRudpPeerStreamState(P,1,&S)==XRUDP_OK,"STOP stream state");
            require(T,S.PeerStopped && S.PeerStopReason==77 && S.ResetQueued && S.ResetAcknowledged &&
                S.ResetFinal==512 && S.ResetReason==77 && S.WriteDone,"STOP RESET acknowledgement/high-water");
            T->StopVerified=true;
        }
    }
    /* Keep game records unread through the producer's reliable FIN fence so
     * LATEST replacement is deterministic rather than racing app consumption. */
    if (T->Typed && T->Eof && !T->ShutdownStarted) for (unsigned N=0;N<8;++N) {
        unsigned char Buffer[128]; xrudpdatagraminfo I; uint64_t Sequence=0;
        xrudpresult R=xrtRudpPeerDatagramRead(P,Buffer,sizeof(Buffer),&I);
        if (R==XRUDP_BACKPRESSURE) break;
        require(T,R==XRUDP_OK,"game read"); if (R!=XRUDP_OK) break;
        if (T->PathTest && T->Games>=3) {
            xrudpsessionstate S={0};
            require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK,"migrated business state");
            require(T,T->Games<5 && I.Channel==7 && !I.Sequenced && I.Sequence==0 && I.Size==8 &&
                !memcmp(Buffer,T->Games==3 ? "migrate1":"migrate2",8) &&
                S.Path.Validations==T->Games-2 && T->PathChanges==T->Games-2,"business only after matching path proof");
        } else require(T,(I.Channel==7 && !I.Sequenced && I.Sequence==0 && I.Size==0) ||
                (I.Channel==8 && I.Sequenced && I.Sequence==5 && I.Size==7 && !memcmp(Buffer,"ordered",7)) ||
                (I.Channel==9 && I.Sequenced && I.Sequence==12 && I.Size==6 && !memcmp(Buffer,"latest",6)),"game mode/sequence/replace");
        require(T,xrtRudpPeerDatagramWrite(P,I.Channel,Buffer,I.Size,0,&Sequence)==XRUDP_OK,"game echo write");
        require(T,Sequence==(I.Channel==7 ? 0u:1u) && ++T->Games<=(T->PathTest ? 5u:3u),"game output sequence/duplicates");
    }
    complete(T);
}
static void start_business(wire_test* T, xrudppeer* P)
{
    unsigned char Buffer[4096]; size_t Written=0;
    for (size_t I=0;I<sizeof(Buffer);++I) Buffer[I]=(unsigned char)(I*7u+19u);
    require(T,xrtRudpPeerWrite(P,Buffer,sizeof(Buffer),&Written)==XRUDP_OK && Written==sizeof(Buffer),"server write");
    require(T,xrtRudpPeerFinish(P)==XRUDP_OK,"server FIN");
    if (T->Typed) {
        for (unsigned I=0;I<3;++I) {
            xrudpdatagramchannel C; xrtRudpDatagramChannelInit(&C); C.Id=7u+I; C.Mode=(uint8_t)I; C.ReplacePending=I==2;
            require(T,xrtRudpPeerDatagramConfigure(P,&C)==XRUDP_OK,"game channel");
        }
        for (unsigned I=0;I<2;++I) {
            xrudpstreamconfig C={(uint8_t)I,(uint8_t)(I+1),1}; uint32_t Id=0;
            require(T,xrtRudpPeerStreamOpen(P,&C,&Id)==XRUDP_OK,"open native typed stream");
            require(T,Id==(T->RemotePort ? 3u:2u)+2u*I,"native role stream IDs");
            if (Id>=2 && Id<6) { T->Slots[Id].Id=Id; typed_write(T,P,Id); }
        }
    }
}
static void confirmed(xrudpendpoint* E, xrudppeer* P, void* Data)
{
    wire_test* T=Data; (void)E;
    require(T,++T->Confirmed==1,"duplicate confirmation");
    if (!T->Peer) T->Peer=xrtRudpPeerRef(P); else require(T,T->Peer==P,"known initiating peer");
    if (T->PathTest) {
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK && S.Path.Enabled && S.Path.Active.Port,"initial path state");
        T->InitialPeerPort=S.Path.Active.Port;
    }
    if (T->Probe) {
        xrudpdatagramchannel C; xrtRudpDatagramChannelInit(&C); C.Id=7;
        require(T,xrtRudpPeerDatagramConfigure(P,&C)==XRUDP_OK,"probe game channel");
        xrtAtomic32Store(&T->AppDone,1,XMEMORY_RELEASE);
        return;
    }
    /* The MTU oracle first proves exact probe sizes independently. No normal
     * application ACK may stand in for the initial padded 512B evidence. */
    if (!T->MtuTest) start_business(T,P);
}
static void peer_closed(xrudpendpoint* E, xrudppeer* P, xrudpresult Reason, void* Data)
{
    wire_test* T=Data; (void)E; (void)P;
    require(T,!xrtAtomic32Load(&T->PeerClosed,XMEMORY_ACQUIRE),"duplicate peer close");
    T->CloseReason=Reason;
    if (T->Probe) require(T,Reason==T->ExpectedClose,"probe close reason");
    if (T->Graceful) {
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK,"graceful terminal snapshot");
        T->ShutdownVerified=T->ShutdownStarted && Reason==XRUDP_CLOSED && S.Shutdown.Succeeded &&
            S.Shutdown.Ready && S.Shutdown.DeclarationAcknowledged && S.Shutdown.DrainedAcknowledged &&
            S.Shutdown.PeerDrained && S.Shutdown.PeerDrainedAcknowledged &&
            S.Shutdown.LocalFinalOrdinal==3 && S.Shutdown.PeerFinalOrdinal==3 &&
            S.ReadDone && S.WriteDone && !S.Transmit.Cancelled;
        require(T,T->ShutdownVerified,"graceful drain/ACK/FIN terminal proof");
    }
    if (T->PathTest) {
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK,"terminal path snapshot");
        T->PathVerified=Reason==XRUDP_CLOSED && T->Games==5 && T->PathChanges==2 &&
            S.Path.Enabled && S.Path.Validations==2 && S.Path.Active.Port==T->InitialPeerPort &&
            S.Path.AddressDropped>=2 && S.Path.SentUnvalidated<=3*S.Path.ReceivedUnvalidated;
        require(T,T->PathVerified,"migration, wrong-source isolation and amplification terminal proof");
    }
    if (T->MtuTest) {
        xrudpsessionstate S={0};
        require(T,xrtRudpPeerSessionState(P,&S)==XRUDP_OK,"terminal MTU snapshot");
        T->Mtu=S.Mtu;
        T->MtuVerified=Reason==XRUDP_CLOSED && T->MtuStarted && S.Mtu.Enabled && S.Mtu.BaseConfirmed &&
            S.Mtu.Effective==(T->MtuLimited ? 768u:1200u) && S.Mtu.Ceiling==1200 && !S.Mtu.Fallbacks &&
            S.Mtu.ProbesSent==(T->MtuLimited ? 5u:4u) && S.Mtu.ProbesAcknowledged==(T->MtuLimited ? 2u:4u) &&
            S.Mtu.ProbeFailures==(T->MtuLimited ? 3u:0u);
        require(T,T->MtuVerified,"MTU terminal evidence and preserved ceiling");
    }
    xrtAtomic32Store(&T->PeerClosed,1,XMEMORY_RELEASE);
}
static void closed(xrudpendpoint* E, xrudpresult Reason, const xerror* Error, void* Data)
{ wire_test* T=Data; (void)E; (void)Reason; (void)Error; ++T->Closes; xrtAtomic32Store(&T->Closed,1,XMEMORY_RELEASE); }
static void bind_peer(xnetworker* W, ptr Data)
{
    wire_test* T=Data; unsigned char Private[32], Client[32], Pin[32], Scope[16], Context[32];
    for (unsigned I=0;I<32;++I) {
        Private[I]=(unsigned char)((T->RemotePort ? 33u:91u)+I);
        Client[I]=(unsigned char)((T->RemotePort ? 91u:33u)+I); Context[I]=22;
    }
    memset(Scope,17,sizeof(Scope)); require(T,xrtX25519Public(Client,Pin),"test pin");
    xrudpendpointconfig C; xrtRudpEndpointConfigInit(&C);
    memcpy(T->Pin,Pin,32);
    C.StaticPrivate=Private; C.PeerPublic=Pin; C.Scope=Scope; C.ApplicationContext=Context; C.Listen=!T->RemotePort;
    C.MaxPeers=C.MaxHalfOpen=1; C.HandshakeTimeoutUs=3000000; C.MemoryLimit=4u*1024u*1024u;
    C.Session.Local.ConnectionCredit=2048; C.Session.Local.StreamCredit=1024;
    C.Session.ReceiveCapacity=1024; C.Session.TransmitCapacity=8192; C.Session.KeyUpdatePackets=8;
    C.Session.DiscoverMtu=T->MtuTest; C.Session.ProgressTimeoutUs=T->MtuTest ? 15000000:5000000;
    if (T->Typed || T->Probe) {
        C.Session.StreamCapacity=C.Session.Local.MaxStreams=3; C.Session.Local.MaxMessageSize=8192;
        xrtRudpDatagramConfigInit(&C.Session.Datagrams); C.Session.Datagrams.Channels=3;
        C.Session.Datagrams.SendCapacity=C.Session.Datagrams.ReceiveCapacity=8; C.Session.Datagrams.PayloadCapacity=128;
    }
    xnetaddr A; require(T,xrtNetAddrLoopback(&A,T->Family,0),"local address");
    xrudpendpointevents V={opened,confirmed,peer_closed,closed,changed,stream_changed};
    require(T,xrtRudpEndpointBind(W,&A,&C,&V,T,&T->Endpoint)==XRUDP_OK,"bind");
    xrtSecureZero(Private,sizeof(Private)); xrtSecureZero(Client,sizeof(Client));
}
static void stop_peer(xnetworker* W, ptr Data)
{ (void)W; wire_test* T=Data; if (T->Endpoint) (void)xrtRudpEndpointClose(T->Endpoint); }
int main(int Argc, char** Argv)
{
    if (Argc!=5 && Argc!=6) return 2;
    wire_test T={0}; T.Family=atoi(Argv[1])==6 ? XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
    int Port=atoi(Argv[2]); if (Port<0 || Port>65535) return 2; T.RemotePort=(uint16_t)Port;
    int Mode=atoi(Argv[3]); if (Mode<0 || Mode>7 || (Mode==2)!=(Argc==6)) return 2;
    T.Typed=Mode==1 || Mode>=3; T.Probe=Mode==2; T.StopControl=Mode==3; T.Graceful=Mode==4; T.PathTest=Mode==5;
    T.MtuTest=Mode==6 || Mode==7; T.MtuLimited=Mode==7;
    if (T.Probe) { T.ExpectedClose=(xrudpresult)atoi(Argv[5]); if (T.ExpectedClose!=XRUDP_MALFORMED && T.ExpectedClose!=XRUDP_CLOSED) return 2; }
    xnetengineconfig C; xrtNetEngineConfigInit(&C); C.Workers=1; C.Backend=XNET_PORT_SELECT;
    if (atoi(Argv[4])) {
#if defined(_WIN32)
        C.Backend=XNET_PORT_IOCP;
#else
        C.Backend=XNET_PORT_EPOLL;
#endif
    }
    T.Engine=xrtNetEngineCreate(&C);
    if (!T.Engine || !xrtNetEngineStart(T.Engine) || !xrtNetEnginePost(T.Engine,0,bind_peer,&T)) return 3;
    double Until=xrtTimer()+30, NextCheck=0; bool Ready=false, Done=false;
    while (xrtTimer()<Until && !xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&T.PeerClosed,XMEMORY_ACQUIRE)) {
        if (!Ready && xrtAtomic32Load(&T.Opened,XMEMORY_ACQUIRE)) { printf("{\"ready\":true,\"port\":%u}\n",T.Port); fflush(stdout); Ready=true; }
        if (!Done && xrtAtomic32Load(&T.AppDone,XMEMORY_ACQUIRE)) { puts("{\"app_done\":true}"); fflush(stdout); Done=true; }
        if ((T.Graceful || T.MtuTest) && xrtTimer()>=NextCheck) {
            if (!xrtAtomic32Exchange(&T.CheckPending,1,XMEMORY_ACQ_REL))
                require(&T,xrtNetEnginePost(T.Engine,0,check_complete,&T),"post owner drain check");
            NextCheck=xrtTimer()+0.01;
        }
        xrtSleep(1);
    }
    (void)xrtNetEnginePost(T.Engine,0,stop_peer,&T); Until=xrtTimer()+5;
    while (!xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE) && xrtTimer()<Until) xrtSleep(1);
    if (!xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE)) return 4;
    Until=xrtTimer()+5;
    while (!xrtNetEngineStop(T.Engine)) { if (xrtTimer()>=Until) return 5; xrtSleep(1); }
    while (!xrtNetEngineDestroy(T.Engine)) { if (xrtTimer()>=Until) return 5; xrtSleep(1); }
    xrudpendpointinfo I={0}; require(&T,xrtRudpEndpointInfo(T.Endpoint,&I)==XRUDP_OK,"terminal info");
    xrtRudpPeerDestroy(T.Peer); xrtRudpEndpointDestroy(T.Endpoint);
    printf("{\"read\":%zu,\"confirmed\":%u,\"invalid\":%llu,\"closes\":%u,\"app_done\":%s,\"streams\":%u,\"messages\":%u,\"games\":%u,\"close_reason\":%u,\"opened\":%u,\"stop_verified\":%s,\"shutdown_verified\":%s,\"path_verified\":%s,\"path_changes\":%u,\"mtu_verified\":%s,\"mtu_effective\":%u,\"mtu_probes\":%llu,\"mtu_acknowledged\":%llu,\"mtu_failures\":%llu,\"joined\":true}\n",
        T.Read,T.Confirmed,(unsigned long long)I.Invalid,T.Closes,Done?"true":"false",T.Typed?5u:1u,T.Slots[4].MessagesRead+T.Slots[5].MessagesRead,T.Games,(unsigned)T.CloseReason,T.StreamOpened,T.StopVerified?"true":"false",T.ShutdownVerified?"true":"false",T.PathVerified?"true":"false",T.PathChanges,T.MtuVerified?"true":"false",T.Mtu.Effective,(unsigned long long)T.Mtu.ProbesSent,(unsigned long long)T.Mtu.ProbesAcknowledged,(unsigned long long)T.Mtu.ProbeFailures);
    return Ready && Done && T.Closes==1 && !xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE) ? 0:6;
}
