/* Real cookie/Noise sessions and UDP delivery. Interpose only server UDP
 * admission to reproduce queue pressure independent of NIC/OS buffer sizing;
 * no packet is falsely accepted, and all admitted ciphertext uses real I/O.
 * Different refill periods catch scheduling biased by retry timing. */
#ifndef XRUDP_MODULE_RUDP_ENDPOINT
#define XRUDP_MODULE_RUDP_ENDPOINT
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../../include/xrudp/features.h"
#define xrtNetUdpSendTo fair_original_sendto
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#undef xrtNetUdpSendTo
#include <xrudp.h>
static xnetudp* quotaUdp;
static bool quotaEnabled;
static double quotaBegin;
static uint64_t quotaTick,quotaDenied;
static unsigned int quotaLeft;
static uint32_t quotaPeriodMs;
XRT_API xnetresult xrtNetUdpSendTo(xnetudp* udp,const xnetaddr* remote,const void* data,size_t size)
{
	if(udp==quotaUdp && quotaEnabled) {
		uint64_t tick=(uint64_t)((xrtTimer()-quotaBegin)*1000/quotaPeriodMs);
		if(tick!=quotaTick) { quotaTick=tick;quotaLeft=4; }
		if(!quotaLeft) { ++quotaDenied;return XNET_RESULT_AGAIN; }
		xnetresult result=fair_original_sendto(udp,remote,data,size);if(result==XNET_RESULT_OK)--quotaLeft;return result;
	}
	return fair_original_sendto(udp,remote,data,size);
}
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"line %d: %s\n",__LINE__,#e);return 1; } } while(0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
#define CLIENTS 8u
#define ALL ((1u<<CLIENTS)-1)
typedef struct fair_context fair_context;
typedef struct fair_side { fair_context* Test;uint32_t Index; } fair_side;
struct fair_context {
	xnetengine* Engine;xrudpendpoint* Endpoint[CLIENTS+1];xrudppeer* ServerPeer[CLIENTS];xrudppeer* ClientPeer[CLIENTS];
	xrudpendpointinfo Identity[CLIENTS+1];fair_side Side[CLIENTS+1];xatomic32 Open,ServerReady,ClientReady,Started,Closed,Failure,Removed;
	unsigned char Private[2][32],Public[2][32];size_t Written[CLIENTS],Read[CLIENTS];uint32_t PeriodMs,Victim;
	uint64_t AtCancel[CLIENTS];
};
static fair_context test;
static void require(fair_context* t,bool ok,const char* text) { if(!ok) { fprintf(stderr,"fairness: %s\n",text);xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE); } }
static bool wait_bits(xatomic32* value,uint32_t bits)
{
	double deadline=xrtTimer()+10;
	while((xrtAtomic32Load(value,XMEMORY_ACQUIRE)&bits)!=bits) {
		if(xrtTimer()>deadline || xrtAtomic32Load(&test.Failure,XMEMORY_ACQUIRE))return false;
		xrtThreadYield();
	}
	return true;
}
static void write_more(fair_context* t,uint32_t index,xrudppeer* p)
{
	unsigned char bytes[16384];for(size_t i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)((t->Written[index]+i)*31);
	size_t n;xrudpresult r=xrtRudpPeerWrite(p,bytes,sizeof(bytes),&n);
	if(r==XRUDP_CLOSED && xrtRudpEndpointState(t->Endpoint[0])!=XRUDP_ENDPOINT_OPEN)return;
	require(t,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"write failed");t->Written[index]+=n;
}
static void opened(xrudpendpoint* e,void* context)
{
	fair_side* s=(fair_side*)context;fair_context* t=s->Test;require(t,xrtRudpEndpointInfo(e,&t->Identity[s->Index])==XRUDP_OK,"identity failed");
	if(!s->Index)quotaUdp=e->Udp;
	(void)xrtAtomic32FetchOr(&t->Open,1u<<s->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	fair_side* s=(fair_side*)context;fair_context* t=s->Test;(void)e;
	if(s->Index) { (void)xrtAtomic32FetchOr(&t->ClientReady,1u<<(s->Index-1),XMEMORY_RELEASE);return; }
	/* Client endpoint identities were published by the other Worker before
	 * dialing. Acquire that publication before mapping their source ports. */
	(void)xrtAtomic32Load(&t->Open,XMEMORY_ACQUIRE);
	xrudphandshakeinfo info;xnetaddr address;require(t,xrtRudpPeerInfo(p,&info,&address)==XRUDP_OK,"peer info failed");
	uint32_t i;for(i=0;i<CLIENTS && address.Port!=t->Identity[i+1].Local.Port;++i){}
	require(t,i<CLIENTS,"unknown peer port");if(i==CLIENTS)return;
	t->ServerPeer[i]=xrtRudpPeerRef(p);(void)xrtAtomic32FetchOr(&t->ServerReady,1u<<i,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	fair_side* s=(fair_side*)context;fair_context* t=s->Test;
	if(s->Index) {
		if(events&XRUDP_PEER_READ_READY)for(unsigned int step=0;step<16;++step) {
			unsigned char bytes[4096];size_t n;xrudpresult r=xrtRudpPeerRead(p,bytes,sizeof(bytes),&n);
			if(r==XRUDP_CLOSED && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return;
			require(t,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"read failed");
			for(size_t i=0;i<n;++i)require(t,bytes[i]==(unsigned char)((t->Read[s->Index-1]+i)*31),"corrupt stream");
			t->Read[s->Index-1]+=n;if(r==XRUDP_BACKPRESSURE || !n)break;
		}
	} else if(events&XRUDP_PEER_WRITE_READY) {
		uint32_t i;for(i=0;i<CLIENTS && t->ServerPeer[i]!=p;++i){}
		require(t,i<CLIENTS,"unmapped writer");if(i<CLIENTS)write_more(t,i,p);
	}
}
static void peer_closed(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* context)
{
	fair_side* s=(fair_side*)context;(void)e;(void)p;require(s->Test,reason==XRUDP_CANCELLED || reason==XRUDP_CLOSED,"unexpected peer failure");
}
static void closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	fair_side* s=(fair_side*)context;(void)e;(void)error;require(s->Test,reason==XRUDP_CANCELLED,"unexpected endpoint failure");
	require(s->Test,e->WaitingPeers==0,"closed endpoint retained output waiters");
	(void)xrtAtomic32FetchOr(&s->Test->Closed,1u<<s->Index,XMEMORY_RELEASE);
}
static void bind_one(xnetworker* worker,ptr context)
{
	fair_side* s=(fair_side*)context;fair_context* t=s->Test;xrudpendpointconfig cfg;xrtRudpEndpointConfigInit(&cfg);
	cfg.StaticPrivate=t->Private[s->Index?1:0];cfg.PeerPublic=t->Public[s->Index?0:1];cfg.Listen=!s->Index;
	cfg.MaxPeers=cfg.MaxHalfOpen=CLIENTS;cfg.HandshakesPerSecond=cfg.AddressHandshakesPerSecond=512;
	cfg.Session.InitialRttUs=10000;cfg.Session.Local.MaxAckDelayUs=1000;
	xnetaddr address;require(t,xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0),"loopback failed");
	xrudpendpointevents events={opened,confirmed,peer_closed,closed,changed,NULL};
	require(t,xrtRudpEndpointBind(worker,&address,&cfg,&events,s,&t->Endpoint[s->Index])==XRUDP_OK,"bind failed");
}
static void dial_all(xnetworker* worker,ptr context)
{
	fair_context* t=(fair_context*)context;(void)worker;xrudpconnectconfig cfg={t->Identity[0].Public,t->Identity[0].Scope,NULL};
	for(uint32_t i=0;i<CLIENTS;++i)require(t,xrtRudpEndpointConnect(t->Endpoint[i+1],&t->Identity[0].Local,&cfg,&t->ClientPeer[i])==XRUDP_OK,"dial failed");
}
static void start(xnetworker* worker,ptr context)
{
	fair_context* t=(fair_context*)context;(void)worker;quotaPeriodMs=t->PeriodMs;quotaDenied=0;quotaBegin=xrtTimer();quotaTick=UINT64_MAX;quotaLeft=0;quotaEnabled=true;
	/* Equivalent to a late ACK completing a refused retransmission: a stale
	 * wait registration on a quiescent session must not hold the queue. */
	xrudp__peer_waiting(t->ServerPeer[0],true);
	xrudp__peer_pump(t->ServerPeer[0],xrudp__endpoint_now(t->Endpoint[0]));
	require(t,!t->ServerPeer[0]->OutputWaiting && t->Endpoint[0]->WaitingPeers==0,"quiescent waiter held queue");
	for(uint32_t i=0;i<CLIENTS;++i)for(unsigned int n=0;n<4;++n)write_more(t,i,t->ServerPeer[i]);
	xrtAtomic32Store(&t->Started,1,XMEMORY_RELEASE);
}
static void cancel_waiter(xnetworker* worker,ptr context)
{
	fair_context* t=(fair_context*)context;(void)worker;xrudpendpoint* e=t->Endpoint[0];
	uint32_t slot=xrudp__endpoint_waiter(e);xrudppeer* p=slot==e->Config.MaxPeers?t->ServerPeer[0]:e->Slots[slot];
	/* Force the same registration if the exact callback turn happened to be
	 * quiescent; cancellation still exercises the queued-head lifetime. */
	xrudp__peer_waiting(p,true);
	for(uint32_t i=0;i<CLIENTS;++i) {
		xrudpsessionstate state;require(t,xrtRudpPeerSessionState(t->ServerPeer[i],&state)==XRUDP_OK,"pre-cancel snapshot missing");
		t->AtCancel[i]=state.SentConnection;if(t->ServerPeer[i]==p)t->Victim=i;
	}
	require(t,t->Victim<CLIENTS && xrtRudpPeerAbort(p)==XRUDP_OK,"queued head cancellation failed");
	xrtAtomic32Store(&t->Removed,1,XMEMORY_RELEASE);
}
static int run(uint32_t period,double duration,bool verify)
{
	quotaUdp=NULL;quotaEnabled=false;memset(&test,0,sizeof(test));test.PeriodMs=period;test.Victim=CLIENTS;
	xrtAtomic32Init(&test.Open,0);xrtAtomic32Init(&test.ServerReady,0);xrtAtomic32Init(&test.ClientReady,0);xrtAtomic32Init(&test.Removed,0);
	xrtAtomic32Init(&test.Started,0);xrtAtomic32Init(&test.Closed,0);xrtAtomic32Init(&test.Failure,0);
	for(size_t i=0;i<2;++i) { for(size_t j=0;j<32;++j)test.Private[i][j]=(unsigned char)(13+i*79+j);CHECK(xrtX25519Public(test.Private[i],test.Public[i])); }
	xnetengineconfig cfg;xrtNetEngineConfigInit(&cfg);cfg.Workers=2;cfg.Backend=TEST_RUDP_BACKEND;test.Engine=xrtNetEngineCreate(&cfg);CHECK(test.Engine && xrtNetEngineStart(test.Engine));
	for(uint32_t i=0;i<=CLIENTS;++i) { test.Side[i].Test=&test;test.Side[i].Index=i;CHECK(xrtNetEnginePost(test.Engine,i?1:0,bind_one,&test.Side[i])); }
	CHECK(wait_bits(&test.Open,(1u<<(CLIENTS+1))-1));CHECK(xrtNetEnginePost(test.Engine,1,dial_all,&test));
	CHECK(wait_bits(&test.ServerReady,ALL) && wait_bits(&test.ClientReady,ALL));CHECK(xrtNetEnginePost(test.Engine,0,start,&test));CHECK(wait_bits(&test.Started,1));
	double began=xrtTimer(),deadline=began+duration;bool cancelled=false;
	while(xrtTimer()<deadline && !xrtAtomic32Load(&test.Failure,XMEMORY_ACQUIRE)) {
		if(verify && period==10 && !cancelled && xrtTimer()-began>duration/2) {
			CHECK(xrtNetEnginePost(test.Engine,0,cancel_waiter,&test));cancelled=true;
		}
		xrtThreadYield();
	}
	if(cancelled)CHECK(wait_bits(&test.Removed,1));
	for(uint32_t i=0;i<=CLIENTS;++i)CHECK(xrtRudpEndpointClose(test.Endpoint[i])==XRUDP_OK);
	CHECK(wait_bits(&test.Closed,(1u<<(CLIENTS+1))-1));CHECK(xrtNetEngineStop(test.Engine) && xrtNetEngineDestroy(test.Engine));
	CHECK(!xrtAtomic32Load(&test.Failure,XMEMORY_ACQUIRE));uint64_t minimum=UINT64_MAX,maximum=0,total=0;
	for(uint32_t i=0;i<CLIENTS;++i) {
		xrudpsessionstate state;CHECK(xrtRudpPeerSessionState(test.ServerPeer[i],&state)==XRUDP_OK);
		printf("peer=%u sent=%llu read=%zu pressure=%llu\n",i,(unsigned long long)state.SentConnection,test.Read[i],(unsigned long long)state.OutputBackpressure);
		if(i!=test.Victim) {
			if(cancelled)CHECK(state.SentConnection>test.AtCancel[i]);
			if(state.SentConnection<minimum)minimum=state.SentConnection;
			if(state.SentConnection>maximum)maximum=state.SentConnection;
			total+=state.SentConnection;
		}
		xrtRudpPeerDestroy(test.ServerPeer[i]);xrtRudpPeerDestroy(test.ClientPeer[i]);
	}
	for(uint32_t i=0;i<=CLIENTS;++i)xrtRudpEndpointDestroy(test.Endpoint[i]);
	printf("xrudp shared UDP: backend=%u period=%ums min=%llu max=%llu total=%llu rejected=%llu\n",(unsigned int)TEST_RUDP_BACKEND,period,(unsigned long long)minimum,(unsigned long long)maximum,(unsigned long long)total,(unsigned long long)quotaDenied);
	if(verify) { CHECK(quotaDenied>0 && total>=CLIENTS*4096 && minimum>=1024);CHECK(maximum<=minimum*4 && minimum*CLIENTS*3>=total); }
	return 0;
}
int main(void)
{
	CHECK(run(1,0.1,false)==0);xrtClearError();xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
	CHECK(run(1,1.0,true)==0);CHECK(run(3,1.0,true)==0);CHECK(run(5,1.0,true)==0);CHECK(run(10,1.0,true)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
	CHECK(before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp shared UDP: real authenticated streams all progress under bounded admission; final handles/memory balanced");return 0;
}
