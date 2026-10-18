/* Local test client for the independent Python STUN oracle. Synthetic key;
 * no account credentials, peer data, additional socket or protocol dependency. */
#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct probe {xnetengine* engine;xrudpendpoint* endpoint;xnetaddr server;xatomic32 closed;uint16_t local,mapped;int reason,fp;} probe;
static void result(xrudpendpoint* e,uint64_t id,const xrudpdiscoverystate* s,const xerror* error,void* data)
{probe* p=(probe*)data;(void)id;(void)error;p->reason=(int)s->Reason;p->mapped=s->Mapped.Port;(void)xrtRudpEndpointClose(e);}
static void opened(xrudpendpoint* e,void* data)
{probe* p=(probe*)data;xrudpendpointinfo info;uint64_t id;if(xrtRudpEndpointInfo(e,&info)!=XRUDP_OK){p->reason=99;(void)xrtRudpEndpointClose(e);return;}p->local=info.Local.Port;if(xrtRudpEndpointDiscover(e,&p->server,3000,p->fp!=0,result,p,&id)!=XRUDP_OK){p->reason=98;(void)xrtRudpEndpointClose(e);}}
static void closed(xrudpendpoint* e,xrudpresult r,const xerror* error,void* data)
{probe* p=(probe*)data;(void)e;(void)r;(void)error;xrtAtomic32Store(&p->closed,1,XMEMORY_RELEASE);}
static void bind_probe(xnetworker* worker,ptr data)
{probe* p=(probe*)data;unsigned char key[32]={19};xrudpendpointconfig cfg;xrtRudpEndpointConfigInit(&cfg);cfg.StaticPrivate=key;xnetaddr local;(void)xrtNetAddrLoopback(&local,p->server.Family,0);xrudpendpointevents ev={0};ev.Open=opened;ev.Close=closed;if(xrtRudpEndpointBind(worker,&local,&cfg,&ev,p,&p->endpoint)!=XRUDP_OK){p->reason=97;xrtAtomic32Store(&p->closed,1,XMEMORY_RELEASE);}}
int main(int argc,char** argv)
{if(argc!=4)return 2;probe p={0};p.fp=atoi(argv[3]);if(!xrtNetAddrParse(&p.server,argv[1],(uint16_t)atoi(argv[2])))return 3;xnetengineconfig cfg;xrtNetEngineConfigInit(&cfg);cfg.Workers=1;cfg.Backend=XNET_PORT_SELECT;p.engine=xrtNetEngineCreate(&cfg);if(!p.engine || !xrtNetEngineStart(p.engine) || !xrtNetEnginePost(p.engine,0,bind_probe,&p))return 4;double until=xrtTimer()+8;while(!xrtAtomic32Load(&p.closed,XMEMORY_ACQUIRE) && xrtTimer()<until)xrtThreadYield();if(!xrtAtomic32Load(&p.closed,XMEMORY_ACQUIRE))return 5;(void)xrtNetEngineStop(p.engine);(void)xrtNetEngineDestroy(p.engine);xrtRudpEndpointDestroy(p.endpoint);printf("{\"local\":%u,\"mapped\":%u,\"reason\":%d}\n",p.local,p.mapped,p.reason);return p.reason==XRUDP_DONE && p.local && p.local==p.mapped?0:6;}
