/* Finite public-API endpoint fixture for local hostile UDP ingress tests.
 * Eight authenticated streams, two real Workers, no UDP/core interposition. */
#include <xrt.h>
#include <xrudp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLIENTS 8u
#define ALL ((1u<<(CLIENTS+1))-1)
#define PEERS ((1u<<CLIENTS)-1)
#define CHECK(v) do { if (!(v)) { fprintf(stderr,"ingress:%d: %s\n",__LINE__,#v); return 1; } } while (0)
typedef struct ingress_side { unsigned int Index; } ingress_side;
static struct {
	xnetengine* Engine; xrudpendpoint* Endpoint[CLIENTS+1]; xrudppeer* Peer[CLIENTS+1];
	xrudpendpointinfo Identity[CLIENTS+1], Snapshot;
	xrudphandshakeinfo Handshake;
	ingress_side Side[CLIENTS+1];
	xatomic32 Open, ServerReady, ClientReady, Failure, Closed, Sampled, Started, StopTick;
	unsigned char Private[2][32], Public[2][32];
	uint64_t Written[CLIENTS], Read[CLIENTS], SampleWritten[CLIENTS], SampleRead[CLIENTS];
	uint64_t TickId, Ticks; double LastTick, MaxTickGap;
	xnetfamily Family; unsigned int Seconds;
} T;
static void require(bool ok,const char* text)
{ if (!ok) { fprintf(stderr,"ingress: %s\n",text); xrtAtomic32Store(&T.Failure,1,XMEMORY_RELEASE); } }
static bool wait_bits(xatomic32* word,uint32_t bits)
{
	double until=xrtTimer()+12;
	while ((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&bits)!=bits) {
		if (xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE) || xrtTimer()>until) return false;
		xrtThreadYield();
	}
	return true;
}
static unsigned int index_of(xrudppeer* p)
{
	for (unsigned int i=0;i<CLIENTS;++i) if (T.Peer[i+1]==p) return i;
	return CLIENTS;
}
static unsigned char pattern(uint64_t offset,unsigned int stream)
{
	/* Include every offset byte; a duplicated 256-byte block must not match
	 * the oracle merely because a byte counter wrapped. */
	return (unsigned char)((offset*31)^(offset>>8)^(offset>>16)^(offset>>24)^
		(offset>>32)^(offset>>40)^(offset>>48)^(offset>>56)^(stream*73u));
}
static void write_more(unsigned int i,xrudppeer* p)
{
	unsigned char bytes[4096]; for (size_t n=0;n<sizeof(bytes);++n) bytes[n]=pattern(T.Written[i]+n,i);
	size_t written=0; xrudpresult result=xrtRudpPeerWrite(p,bytes,sizeof(bytes),&written);
	if (result==XRUDP_CLOSED && xrtRudpEndpointState(T.Endpoint[0])!=XRUDP_ENDPOINT_OPEN) return;
	require(result==XRUDP_OK || result==XRUDP_BACKPRESSURE,"write result"); T.Written[i]+=written;
}
static void opened(xrudpendpoint* e,void* context)
{
	ingress_side* s=context; require(xrtRudpEndpointInfo(e,&T.Identity[s->Index])==XRUDP_OK,"endpoint identity");
	(void)xrtAtomic32FetchOr(&T.Open,1u<<s->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	ingress_side* s=context; (void)e;
	if (s->Index) { (void)xrtAtomic32FetchOr(&T.ClientReady,1u<<(s->Index-1),XMEMORY_RELEASE); return; }
	(void)xrtAtomic32Load(&T.Open,XMEMORY_ACQUIRE);
	xrudphandshakeinfo info; xnetaddr address;
	require(xrtRudpPeerInfo(p,&info,&address)==XRUDP_OK,"authenticated peer info");
	unsigned int i; for (i=0;i<CLIENTS && address.Port!=T.Identity[i+1].Local.Port;++i) {}
	require(i<CLIENTS,"unknown authenticated peer"); if (i==CLIENTS) return;
	/* Server and client handles have different owners, so keep separate arrays
	 * in the peer slots: server slots below, client handles are held by Dial. */
	T.Peer[i+1]=xrtRudpPeerRef(p);
	(void)xrtAtomic32FetchOr(&T.ServerReady,1u<<i,XMEMORY_RELEASE);
}
static xrudppeer* clients[CLIENTS];
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	ingress_side* s=context;
	if (!s->Index) {
		unsigned int i=index_of(p); require(i<CLIENTS,"unmapped stream writer");
		if (i<CLIENTS && (events&XRUDP_PEER_WRITE_READY) && xrtAtomic32Load(&T.Started,XMEMORY_ACQUIRE)) write_more(i,p);
		return;
	}
	if (events&XRUDP_PEER_READ_READY) for (unsigned int step=0;step<16;++step) {
		unsigned char bytes[4096]; size_t count=0; xrudpresult result=xrtRudpPeerRead(p,bytes,sizeof(bytes),&count);
		if (result==XRUDP_CLOSED && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN) return;
		require(result==XRUDP_OK || result==XRUDP_BACKPRESSURE,"stream read failed");
		unsigned int i=s->Index-1;
		for (size_t n=0;n<count;++n) if (bytes[n]!=pattern(T.Read[i]+n,i)) {
			require(false,"corrupt/duplicate stream content"); break;
		}
		T.Read[i]+=count; if (!count || result==XRUDP_BACKPRESSURE) break;
	}
}
static void peer_closed(xrudpendpoint* e,xrudppeer* p,xrudpresult result,void* context)
{
	(void)e; (void)p; (void)context; require(result==XRUDP_CANCELLED || result==XRUDP_CLOSED,"peer failed under ingress");
}
static void closed(xrudpendpoint* e,xrudpresult result,const xerror* error,void* context)
{
	ingress_side* s=context; (void)e;
	require(result==XRUDP_CANCELLED && error==NULL,"endpoint failed under ingress");
	(void)xrtAtomic32FetchOr(&T.Closed,1u<<s->Index,XMEMORY_RELEASE);
}
static void bind_one(xnetworker* worker,ptr context)
{
	ingress_side* s=context; xrudpendpointconfig c; xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=T.Private[s->Index?1:0]; c.PeerPublic=T.Public[s->Index?0:1]; c.Listen=!s->Index;
	c.MaxPeers=c.MaxHalfOpen=CLIENTS; c.Session.StreamCapacity=c.Session.Local.MaxStreams=1;
	c.Session.ReceiveCapacity=c.Session.TransmitCapacity=8192;
	c.Session.Local.StreamCredit=c.Session.Local.ConnectionCredit=4096;
	c.Session.PacketCapacity=32; c.Session.InitialRttUs=10000;
	c.Session.Local.MaxAckDelayUs=1000; c.Session.Local.DatagramSize=512;
	c.Session.Local.IdleTimeoutMs=30000; c.Session.ProgressTimeoutUs=5000000; c.Session.DiscoverMtu=false;
	xnetaddr local; require(xrtNetAddrLoopback(&local,T.Family,0),"loopback address");
	xrudpendpointevents events={opened,confirmed,peer_closed,closed,changed,NULL};
	require(xrtRudpEndpointBind(worker,&local,&c,&events,s,&T.Endpoint[s->Index])==XRUDP_OK,"endpoint bind");
}
static void dial(xnetworker* worker,ptr context)
{
	(void)worker; (void)context; xrudpconnectconfig c={T.Identity[0].Public,T.Identity[0].Scope,NULL};
	for (unsigned int i=0;i<CLIENTS;++i) require(xrtRudpEndpointConnect(T.Endpoint[i+1],&T.Identity[0].Local,&c,&clients[i])==XRUDP_OK,"native dial");
}
static void tick(xnetworker* worker,uint64 id,xnetresult result,ptr context)
{
	(void)worker; (void)id; (void)context;
	T.TickId=0; if (xrtAtomic32Load(&T.StopTick,XMEMORY_ACQUIRE)) return;
	require(result==XNET_RESULT_OK,"heartbeat timer error");
	double now=xrtTimer(); if (T.LastTick && now-T.LastTick>T.MaxTickGap) T.MaxTickGap=now-T.LastTick;
	T.LastTick=now; ++T.Ticks;
	T.TickId=xrtNetEngineAfter(T.Engine,0,10,tick,NULL); require(T.TickId!=0,"heartbeat timer admission");
}
static void start(xnetworker* worker,ptr context)
{
	(void)worker; (void)context; xrtAtomic32Store(&T.Started,1,XMEMORY_RELEASE);
	for (unsigned int i=0;i<CLIENTS;++i) write_more(i,T.Peer[i+1]);
	T.TickId=xrtNetEngineAfter(T.Engine,0,10,tick,NULL); require(T.TickId!=0,"initial timer admission");
}
static void sample(xnetworker* worker,ptr context)
{
	ingress_side* side=context; (void)worker;
	if (!side->Index) {
		require(xrtRudpEndpointInfo(T.Endpoint[0],&T.Snapshot)==XRUDP_OK,"endpoint sample");
		require(T.Snapshot.Peers==CLIENTS && T.Snapshot.HalfOpen==0 && T.Snapshot.ReservedBytes<=T.Snapshot.MemoryLimit,"admission limits");
		xnetaddr remote; require(xrtRudpPeerInfo(T.Peer[1],&T.Handshake,&remote)==XRUDP_OK,"first peer CID sample");
		for (unsigned int i=0;i<CLIENTS;++i) {
			xrudpsessionstate state; require(xrtRudpPeerSessionState(T.Peer[i+1],&state)==XRUDP_OK && state.Sent.PendingPackets<=64,"bounded live send ledger");
			T.SampleWritten[i]=T.Written[i];
		}
	} else memcpy(T.SampleRead,T.Read,sizeof(T.Read));
	(void)xrtAtomic32FetchOr(&T.Sampled,side->Index?2u:1u,XMEMORY_RELEASE);
}
static int snapshot(void)
{
	xrtAtomic32Store(&T.Sampled,0,XMEMORY_RELEASE);
	CHECK(xrtNetEnginePost(T.Engine,0,sample,&T.Side[0]) && xrtNetEnginePost(T.Engine,1,sample,&T.Side[1]));
	CHECK(wait_bits(&T.Sampled,3)); return 0;
}
static void hex(const unsigned char* p,size_t size)
{ for (size_t i=0;i<size;++i) printf("%02x",p[i]); }
static void stop_tick(xnetworker* worker,ptr context)
{
	(void)worker; (void)context; xrtAtomic32Store(&T.StopTick,1,XMEMORY_RELEASE);
	if (T.TickId) (void)xrtNetEngineTimerCancelCurrent(T.Engine,T.TickId);
	T.TickId=0;
}
int main(int argc,char** argv)
{
	CHECK(argc==4); T.Family=atoi(argv[1])==6?XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
	bool native=atoi(argv[2])!=0; T.Seconds=(unsigned int)atoi(argv[3]); CHECK(T.Seconds>=2 && T.Seconds<=20);
	xrtAtomic32Init(&T.Open,0); xrtAtomic32Init(&T.ServerReady,0); xrtAtomic32Init(&T.ClientReady,0);
	xrtAtomic32Init(&T.Failure,0); xrtAtomic32Init(&T.Closed,0); xrtAtomic32Init(&T.Sampled,0);
	xrtAtomic32Init(&T.Started,0); xrtAtomic32Init(&T.StopTick,0);
	for (size_t i=0;i<32;++i) { T.Private[0][i]=(unsigned char)(11+i); T.Private[1][i]=(unsigned char)(97+i); }
	CHECK(xrtX25519Public(T.Private[0],T.Public[0]) && xrtX25519Public(T.Private[1],T.Public[1]));
	xnetengineconfig c; xrtNetEngineConfigInit(&c); c.Workers=2; c.Backend=XNET_PORT_SELECT;
#if defined(_WIN32)
	if (native) c.Backend=XNET_PORT_IOCP;
#else
	if (native) c.Backend=XNET_PORT_EPOLL;
#endif
	T.Engine=xrtNetEngineCreate(&c); CHECK(T.Engine && xrtNetEngineStart(T.Engine));
	for (unsigned int i=0;i<=CLIENTS;++i) { T.Side[i].Index=i; CHECK(xrtNetEnginePost(T.Engine,i?1:0,bind_one,&T.Side[i])); }
	CHECK(wait_bits(&T.Open,ALL)); CHECK(xrtNetEnginePost(T.Engine,1,dial,NULL));
	CHECK(wait_bits(&T.ServerReady,PEERS) && wait_bits(&T.ClientReady,PEERS));
	CHECK(xrtNetEnginePost(T.Engine,0,start,NULL)); CHECK(snapshot()==0);
	size_t reserved=T.Snapshot.ReservedBytes;
	printf("{\"ready\":true,\"port\":%u,\"scope\":\"",T.Identity[0].Local.Port); hex(T.Identity[0].Scope,16);
	printf("\",\"public\":\""); hex(T.Identity[0].Public,32); printf("\",\"cid\":\""); hex(T.Handshake.LocalId,16);
	printf("\",\"clients\":8,\"reserved\":%llu}\n",(unsigned long long)reserved); fflush(stdout);
	double begin=xrtTimer();
	for (unsigned int step=0;step<T.Seconds*2;++step) {
		xrtSleep(500); CHECK(snapshot()==0); CHECK(T.Snapshot.ReservedBytes==reserved);
		printf("{\"sample\":true,\"elapsed\":%.6f,\"received\":%llu,\"invalid\":%llu,\"rate_dropped\":%llu,\"address_dropped\":%llu,\"reserved\":%llu,\"reads\":[",xrtTimer()-begin,(unsigned long long)T.Snapshot.Received,(unsigned long long)T.Snapshot.Invalid,(unsigned long long)T.Snapshot.RateDropped,(unsigned long long)T.Snapshot.AddressDropped,(unsigned long long)T.Snapshot.ReservedBytes);
		for (unsigned int i=0;i<CLIENTS;++i) printf("%s%llu",i?",":"",(unsigned long long)T.SampleRead[i]);
		printf("]}\n"); fflush(stdout);
	}
	CHECK(xrtNetEnginePost(T.Engine,0,stop_tick,NULL));
	for (unsigned int i=0;i<=CLIENTS;++i) CHECK(xrtRudpEndpointClose(T.Endpoint[i])==XRUDP_OK);
	CHECK(wait_bits(&T.Closed,ALL)); CHECK(xrtNetEngineStop(T.Engine) && xrtNetEngineDestroy(T.Engine));
	CHECK(xrtAtomic32Load(&T.Failure,XMEMORY_ACQUIRE)==0);
	for (unsigned int i=0;i<CLIENTS;++i) { xrtRudpPeerDestroy(T.Peer[i+1]); xrtRudpPeerDestroy(clients[i]); }
	for (unsigned int i=0;i<=CLIENTS;++i) xrtRudpEndpointDestroy(T.Endpoint[i]);
	CHECK(T.MaxTickGap<1.0 && T.Ticks>T.Seconds*10);
	printf("{\"terminal\":true,\"joined\":true,\"ticks\":%llu,\"max_tick_gap\":%.6f}\n",(unsigned long long)T.Ticks,T.MaxTickGap); fflush(stdout);
	return 0;
}
