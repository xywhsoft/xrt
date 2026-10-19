#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr,"quota:%d: %s\n",__LINE__,#v); return 1; } } while (0)

typedef struct quota_test {
	xnetengine* Engine;
	xrudpendpoint* Endpoint;
	xatomic32 Done, Closed, Failed;
	xnetfamily Family;
	xnetportbackend Backend;
	size_t Base, Peak, Handle, Limit;
	unsigned int PeerCloses;
	bool Pressure;
} quota_test;

static void peer_close(xrudpendpoint* e, xrudppeer* p, xrudpresult reason, void* context)
{
	quota_test* t=(quota_test*)context; (void)e;
	if (reason!=XRUDP_CANCELLED || xrtRudpPeerState(p)!=XRUDP_PEER_CLOSED)
		xrtAtomic32Store(&t->Failed,1,XMEMORY_RELEASE);
	++t->PeerCloses;
}
static void endpoint_close(xrudpendpoint* e, xrudpresult reason, const xerror* error, void* context)
{
	quota_test* t=(quota_test*)context; (void)e;
	if (reason!=XRUDP_CANCELLED || error!=NULL) xrtAtomic32Store(&t->Failed,1,XMEMORY_RELEASE);
	xrtAtomic32Store(&t->Closed,1,XMEMORY_RELEASE);
}
static int measure(quota_test* t, xrudpendpoint* e)
{
	xrudpendpointinfo before, active, closed, released;
	CHECK(xrtRudpEndpointInfo(e,&before)==XRUDP_OK);
	xrudpconnectconfig dial={before.Public,before.Scope,NULL}; xrudppeer* p=NULL;
	/* A non-listener socket cannot authenticate its own dial; measure real
	 * native admission and abort it before any untrusted input can allocate. */
	CHECK(xrtRudpEndpointConnect(e,&before.Local,&dial,&p)==XRUDP_OK && p!=NULL);
	CHECK(xrtRudpEndpointInfo(e,&active)==XRUDP_OK && active.Peers==1 && active.HalfOpen==1);
	CHECK(xrtRudpPeerAbort(p)==XRUDP_OK && xrtRudpPeerAbort(p)==XRUDP_DONE);
	CHECK(xrtRudpEndpointInfo(e,&closed)==XRUDP_OK && closed.Peers==0 && closed.HalfOpen==0);
	CHECK(active.ReservedBytes>closed.ReservedBytes && closed.ReservedBytes>before.ReservedBytes);
	t->Base=before.ReservedBytes; t->Peak=active.ReservedBytes-before.ReservedBytes;
	t->Handle=closed.ReservedBytes-before.ReservedBytes;
	xrtRudpPeerDestroy(p);
	CHECK(xrtRudpEndpointInfo(e,&released)==XRUDP_OK && released.ReservedBytes==t->Base);
	return 0;
}
static int pressure(quota_test* t, xrudpendpoint* e)
{
	xrudpendpointinfo info;
	CHECK(xrtRudpEndpointInfo(e,&info)==XRUDP_OK && info.ReservedBytes==t->Base && info.MemoryLimit==t->Limit);
	xrudpconnectconfig dial={info.Public,info.Scope,NULL}; xrudppeer* held[2]={NULL,NULL};
	for (unsigned int round=0;round<64;++round) {
		for (size_t i=0;i<2;++i) {
			CHECK(xrtRudpEndpointConnect(e,&info.Local,&dial,&held[i])==XRUDP_OK && held[i]!=NULL);
			CHECK(xrtRudpPeerAbort(held[i])==XRUDP_OK);
		}
		CHECK(xrtRudpEndpointInfo(e,&info)==XRUDP_OK && info.Peers==0 && info.HalfOpen==0);
		CHECK(info.ReservedBytes==t->Base+2*t->Handle && info.ReservedBytes<=info.MemoryLimit);
		xrudppeer* rejected=NULL;
		CHECK(xrtRudpEndpointConnect(e,&info.Local,&dial,&rejected)==XRUDP_BACKPRESSURE && rejected==NULL);
		xrudpendpointinfo after;
		CHECK(xrtRudpEndpointInfo(e,&after)==XRUDP_OK && after.ReservedBytes==info.ReservedBytes && after.Peers==0 && after.HalfOpen==0);
		/* Slots are free. The retained closed handle is the only reason for
		 * rejection; dropping it must immediately restore real dial admission. */
		xrtRudpPeerDestroy(held[0]); held[0]=NULL;
		CHECK(xrtRudpEndpointConnect(e,&info.Local,&dial,&held[0])==XRUDP_OK && held[0]!=NULL);
		CHECK(xrtRudpPeerAbort(held[0])==XRUDP_OK);
		for (size_t i=0;i<2;++i) { xrtRudpPeerDestroy(held[i]); held[i]=NULL; }
		CHECK(xrtRudpEndpointInfo(e,&info)==XRUDP_OK && info.ReservedBytes==t->Base);
	}
	CHECK(t->PeerCloses==64*3);
	return 0;
}
static void opened(xrudpendpoint* e, void* context)
{
	quota_test* t=(quota_test*)context;
	int failed=t->Pressure?pressure(t,e):measure(t,e);
	if (failed) xrtAtomic32Store(&t->Failed,1,XMEMORY_RELEASE);
	xrtAtomic32Store(&t->Done,1,XMEMORY_RELEASE);
}
static void bind_endpoint(xnetworker* worker, ptr context)
{
	quota_test* t=(quota_test*)context; unsigned char key[32]={27};
	xrudpendpointconfig cfg; xrtRudpEndpointConfigInit(&cfg); cfg.StaticPrivate=key;
	cfg.MaxPeers=cfg.MaxHalfOpen=2;
	if (t->Pressure) cfg.MemoryLimit=t->Limit;
	xnetaddr local;
	if (!xrtNetAddrLoopback(&local,t->Family,0)) goto fail;
	xrudpendpointevents events={opened,NULL,peer_close,endpoint_close,NULL,NULL};
	if (xrtRudpEndpointBind(worker,&local,&cfg,&events,t,&t->Endpoint)!=XRUDP_OK) goto fail;
	return;
fail:
	xrtAtomic32Store(&t->Failed,1,XMEMORY_RELEASE); xrtAtomic32Store(&t->Done,1,XMEMORY_RELEASE);
}
static int run(quota_test* t)
{
	xrtAtomic32Init(&t->Done,0); xrtAtomic32Init(&t->Closed,0); xrtAtomic32Init(&t->Failed,0); t->PeerCloses=0;
	xnetengineconfig cfg; xrtNetEngineConfigInit(&cfg); cfg.Workers=1; cfg.Backend=t->Backend;
	t->Engine=xrtNetEngineCreate(&cfg); CHECK(t->Engine!=NULL && xrtNetEngineStart(t->Engine));
	CHECK(xrtNetEnginePost(t->Engine,0,bind_endpoint,t));
	double deadline=xrtTimer()+10;
	while (!xrtAtomic32Load(&t->Done,XMEMORY_ACQUIRE)) { CHECK(xrtTimer()<deadline); xrtThreadYield(); }
	CHECK(xrtAtomic32Load(&t->Failed,XMEMORY_ACQUIRE)==0);
	CHECK(xrtRudpEndpointClose(t->Endpoint)==XRUDP_OK);
	while (!xrtAtomic32Load(&t->Closed,XMEMORY_ACQUIRE)) { CHECK(xrtTimer()<deadline); xrtThreadYield(); }
	CHECK(xrtNetEngineStop(t->Engine) && xrtNetEngineDestroy(t->Engine));
	CHECK(xrtAtomic32Load(&t->Failed,XMEMORY_ACQUIRE)==0);
	xrudpendpointinfo info;
	CHECK(xrtRudpEndpointInfo(t->Endpoint,&info)==XRUDP_OK && info.Peers==0 && info.HalfOpen==0 && info.ReservedBytes==t->Base);
	xrtRudpEndpointDestroy(t->Endpoint); t->Endpoint=NULL;
	return 0;
}
int main(int argc, char** argv)
{
	quota_test t; memset(&t,0,sizeof(t));
	t.Family=argc>1 && atoi(argv[1])==6?XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
	t.Backend=XNET_PORT_SELECT;
	if (argc>2 && atoi(argv[2])) {
#if defined(_WIN32)
		t.Backend=XNET_PORT_IOCP;
#else
		t.Backend=XNET_PORT_EPOLL;
#endif
	}
	CHECK(run(&t)==0 && t.Peak>t.Handle && t.Handle>0);
	t.Limit=t.Base+t.Peak+2*t.Handle-1; t.Pressure=true;
	CHECK(run(&t)==0);
	printf("xrudp endpoint quota: family=%u backend=%u 64 retained-handle cycles, 192 closes, recovery and joined Engine passed\n",(unsigned)t.Family,(unsigned)t.Backend);
	return 0;
}
