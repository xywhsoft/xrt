/* Native public-API peer for an independent Python wire/Noise implementation.
 * Synthetic fixed identities only; no private headers, account or model IO. */
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <xrudp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct wire_test {
    xnetengine* Engine; xrudpendpoint* Endpoint; xrudppeer* Peer;
    xatomic32 Opened, Closed, PeerClosed, AppDone, Failure;
    uint16_t Port, RemotePort; xnetfamily Family; size_t Read;
    unsigned char Pin[32];
    unsigned Confirmed, Closes; bool Eof, Acked;
} wire_test;
static void require(wire_test* T, bool Ok, const char* What)
{ if (!Ok) { fprintf(stderr,"wire native failure: %s\n",What); xrtAtomic32Store(&T->Failure,1,XMEMORY_RELEASE); } }
static void opened(xrudpendpoint* E, void* Data)
{
    wire_test* T=Data; xrudpendpointinfo I;
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
    if (Events&XRUDP_PEER_WRITE_DONE) T->Acked=true;
    if (T->Eof && T->Read==3072 && T->Acked) xrtAtomic32Store(&T->AppDone,1,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* E, xrudppeer* P, void* Data)
{
    wire_test* T=Data; unsigned char Buffer[4096]; size_t Written=0; (void)E;
    require(T,++T->Confirmed==1,"duplicate confirmation");
    if (!T->Peer) T->Peer=xrtRudpPeerRef(P); else require(T,T->Peer==P,"known initiating peer");
    for (size_t I=0;I<sizeof(Buffer);++I) Buffer[I]=(unsigned char)(I*7u+19u);
    require(T,xrtRudpPeerWrite(P,Buffer,sizeof(Buffer),&Written)==XRUDP_OK && Written==sizeof(Buffer),"server write");
    require(T,xrtRudpPeerFinish(P)==XRUDP_OK,"server FIN");
}
static void peer_closed(xrudpendpoint* E, xrudppeer* P, xrudpresult Reason, void* Data)
{
    wire_test* T=Data; (void)E; (void)P; (void)Reason;
    require(T,!xrtAtomic32Load(&T->PeerClosed,XMEMORY_ACQUIRE),"duplicate peer close");
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
    C.Session.DiscoverMtu=false; C.Session.ProgressTimeoutUs=5000000;
    xnetaddr A; require(T,xrtNetAddrLoopback(&A,T->Family,0),"local address");
    xrudpendpointevents V={opened,confirmed,peer_closed,closed,changed,NULL};
    require(T,xrtRudpEndpointBind(W,&A,&C,&V,T,&T->Endpoint)==XRUDP_OK,"bind");
    xrtSecureZero(Private,sizeof(Private)); xrtSecureZero(Client,sizeof(Client));
}
static void stop_peer(xnetworker* W, ptr Data)
{ (void)W; wire_test* T=Data; if (T->Endpoint) (void)xrtRudpEndpointClose(T->Endpoint); }
int main(int Argc, char** Argv)
{
    if (Argc!=2 && Argc!=3) return 2;
    wire_test T={0}; T.Family=atoi(Argv[1])==6 ? XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
    if (Argc==3) { int Port=atoi(Argv[2]); if (Port<1 || Port>65535) return 2; T.RemotePort=(uint16_t)Port; }
    xnetengineconfig C; xrtNetEngineConfigInit(&C); C.Workers=1; C.Backend=XNET_PORT_SELECT;
    T.Engine=xrtNetEngineCreate(&C);
    if (!T.Engine || !xrtNetEngineStart(T.Engine) || !xrtNetEnginePost(T.Engine,0,bind_peer,&T)) return 3;
    double Until=xrtTimer()+20; bool Ready=false, Done=false;
    while (xrtTimer()<Until && !xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&T.PeerClosed,XMEMORY_ACQUIRE)) {
        if (!Ready && xrtAtomic32Load(&T.Opened,XMEMORY_ACQUIRE)) { printf("{\"ready\":true,\"port\":%u}\n",T.Port); fflush(stdout); Ready=true; }
        if (!Done && xrtAtomic32Load(&T.AppDone,XMEMORY_ACQUIRE)) { puts("{\"app_done\":true}"); fflush(stdout); Done=true; }
        xrtSleep(1);
    }
    (void)xrtNetEnginePost(T.Engine,0,stop_peer,&T); Until=xrtTimer()+5;
    while (!xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE) && xrtTimer()<Until) xrtSleep(1);
    if (!xrtAtomic32Load(&T.Closed,XMEMORY_ACQUIRE)) return 4;
    Until=xrtTimer()+5;
    while (!xrtNetEngineStop(T.Engine)) { if (xrtTimer()>=Until) return 5; xrtSleep(1); }
    while (!xrtNetEngineDestroy(T.Engine)) { if (xrtTimer()>=Until) return 5; xrtSleep(1); }
    xrudpendpointinfo I; require(&T,xrtRudpEndpointInfo(T.Endpoint,&I)==XRUDP_OK,"terminal info");
    xrtRudpPeerDestroy(T.Peer); xrtRudpEndpointDestroy(T.Endpoint);
    printf("{\"read\":%zu,\"confirmed\":%u,\"invalid\":%llu,\"closes\":%u,\"app_done\":%s}\n",T.Read,T.Confirmed,(unsigned long long)I.Invalid,T.Closes,Done?"true":"false");
    return Ready && Done && T.Closes==1 && !xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE) ? 0:6;
}
