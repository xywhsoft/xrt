#include <xrudp.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT_FUTURE)
#error "This test must omit the optional endpoint Future module"
#endif
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"disabled future failure: %u: %s\n",__LINE__,#e); return 1; } } while (0)
typedef struct disabled_test { xnetengine* Engine; xatomic32 Done,Failure; xnetfamily Family; } disabled_test;
static void expect(disabled_test* t,bool ok)
{ if (!ok) xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE); }
static void opened(xrudpendpoint* e,void* context)
{ expect(context,xrtRudpEndpointClose(e)==XRUDP_OK); }
static void closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	disabled_test* t=context; (void)error; expect(t,reason==XRUDP_CANCELLED);
	xrtRudpEndpointDestroy(e); xrtAtomic32Store(&t->Done,1,XMEMORY_RELEASE);
}
static void bind_endpoint(xnetworker* worker,ptr context)
{
	disabled_test* t=context; unsigned char private_key[32]={7}; xrudpendpointconfig c; xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=private_key; c.MaxPeers=c.MaxHalfOpen=1;
	xnetaddr local; expect(t,xrtNetAddrLoopback(&local,t->Family,0));
	xrudpendpointevents events={opened,NULL,NULL,closed,NULL,NULL}; xrudpendpoint* endpoint=NULL;
	c.AsyncOperations=1; c.AsyncBytes=4096;
	expect(t,xrtRudpEndpointBind(worker,&local,&c,&events,t,&endpoint)==XRUDP_ARGUMENT && !endpoint);
	c.AsyncOperations=0;
	expect(t,xrtRudpEndpointBind(worker,&local,&c,&events,t,&endpoint)==XRUDP_ARGUMENT && !endpoint);
	c.AsyncBytes=0;
	expect(t,xrtRudpEndpointBind(worker,&local,&c,&events,t,&endpoint)==XRUDP_OK && endpoint);
}
int main(int argc,char** argv)
{
	disabled_test t; memset(&t,0,sizeof(t)); t.Family=argc>1 && atoi(argv[1])==6 ? XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
	xnetengineconfig c; xrtNetEngineConfigInit(&c); c.Workers=1; c.Backend=XNET_PORT_SELECT;
	if (argc>2 && atoi(argv[2])) {
#if defined(_WIN32)
		c.Backend=XNET_PORT_IOCP;
#else
		c.Backend=XNET_PORT_EPOLL;
#endif
	}
	t.Engine=xrtNetEngineCreate(&c); CHECK(t.Engine && xrtNetEngineStart(t.Engine) && xrtNetEnginePost(t.Engine,0,bind_endpoint,&t));
	double end=xrtTimer()+5;
	while (!xrtAtomic32Load(&t.Done,XMEMORY_ACQUIRE)) { CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE) && xrtTimer()<end); xrtSleep(1); }
	CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE) && xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
	puts("xrudp optional Future omitted: strict config admission and native endpoint lifecycle passed"); return 0;
}
