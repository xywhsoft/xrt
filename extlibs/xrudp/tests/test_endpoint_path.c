#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while(0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
#define PATH_BYTES 32768u
#define PATH_MESSAGES 8192u
typedef struct path_native path_native;
typedef struct path_side {
	path_native* Test; uint32_t Id; bool Live[2],Finished[2],Eof[2],Acked[2],InCallback;
	size_t Written[2],Read[2]; uint32_t PathHints,Closes,PeerCloses;
} path_side;
typedef struct path_proxy { path_native* Test; uint32_t Id; } path_proxy;
struct path_native {
	xnetengine* Engine; xrudpendpoint* Endpoint[2]; xrudppeer* Peer[2]; xrudpendpointinfo Info[2];
	xnetudp* Udp[4]; xnetaddr UdpAddress[4]; path_proxy Proxy[4]; path_side Side[2];
	xatomic32 Ready,Dialed,Confirmed,Complete,PeerClosed,Closed,ProxyClosed,ActiveBack,PathChanges,PathFailed,Failure;
	uint64_t ProxyBytes; unsigned char Private[2][32],Public[2][32]; xnetfamily Family; uint32_t Mode;
};
static void demand(path_native* t,bool ok,const char* text)
{
	if(!ok) { fprintf(stderr,"native path: mode=%u %s\n",t->Mode,text);xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE); }
}
static bool path_wait(path_native* t,xatomic32* word,uint32_t bits)
{
	double until=xrtTimer()+12;
	while((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&bits)!=bits) { if(xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE) || xrtTimer()>until)return false;xrtThreadYield(); }
	return true;
}
static unsigned char path_pattern(uint32_t side,uint32_t stream,size_t offset) { return (unsigned char)(side*71+stream*29+offset*37+(offset>>8)); }
static void path_write(path_side* s,xrudppeer* p,uint32_t kind)
{
	if(!s->Live[kind] || s->Finished[kind])return;
	size_t maximum=kind?PATH_MESSAGES:PATH_BYTES; uint32_t id=kind?3:1;
	for(uint32_t step=0;step<8 && s->Written[kind]<maximum;++step) {
		unsigned char bytes[128];size_t n=maximum-s->Written[kind];if(n>sizeof(bytes))n=sizeof(bytes);
		for(size_t i=0;i<n;++i)bytes[i]=path_pattern(s->Id,kind,s->Written[kind]+i);
		size_t written=0;xrudpresult r;
		if(kind) { r=xrtRudpPeerMessageWrite(p,id,bytes,n);if(r==XRUDP_OK)written=n; }
		else r=xrtRudpPeerWrite(p,bytes,n,&written);
		if(r==XRUDP_CLOSED && xrtRudpEndpointState(s->Test->Endpoint[s->Id])!=XRUDP_ENDPOINT_OPEN)return;
		demand(s->Test,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"write rejected");if(r!=XRUDP_OK)return;s->Written[kind]+=written;
	}
	if(s->Written[kind]==maximum) { demand(s->Test,xrtRudpPeerStreamFinish(p,id)==XRUDP_OK,"finish rejected");s->Finished[kind]=true; }
}
static void path_completion(path_side* s)
{
	if(s->Eof[0] && s->Eof[1] && s->Acked[0] && s->Acked[1]) (void)xrtAtomic32FetchOr(&s->Test->Complete,1u<<s->Id,XMEMORY_RELEASE);
}
static void path_opened(xrudpendpoint* e,void* context)
{
	path_side* s=(path_side*)context;demand(s->Test,xrtRudpEndpointInfo(e,&s->Test->Info[s->Id])==XRUDP_OK,"identity unavailable");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Id,XMEMORY_RELEASE);
}
static void path_confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	path_side* s=(path_side*)context;(void)e;if(s->Id==0)s->Test->Peer[0]=xrtRudpPeerRef(p);
	s->Live[0]=true;path_write(s,p,0);
	if(s->Id==1) { xrudpstreamconfig config;xrtRudpStreamConfigInit(&config);config.Kind=XRUDP_STREAM_MESSAGES;uint32_t id=0;
		demand(s->Test,xrtRudpPeerStreamOpen(p,&config,&id)==XRUDP_OK && id==3,"message open failed");s->Live[1]=true;path_write(s,p,1); }
	(void)xrtAtomic32FetchOr(&s->Test->Confirmed,1u<<s->Id,XMEMORY_RELEASE);
}
static void path_peer_closed(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* context)
{
	path_side* s=(path_side*)context;(void)e;xrudpsessionstate state;
	demand(s->Test,++s->PeerCloses==1 && xrtRudpPeerSessionState(p,&state)==XRUDP_OK && !state.Sent.PendingPackets && !state.Sent.BytesInFlight,"close snapshot owns flight");
	if(s->Test->Mode==0 || s->Test->Mode==3 || s->Test->Mode==4)demand(s->Test,reason==XRUDP_CLOSED && state.Shutdown.Succeeded,"clean drain failed after rebinding");
	else demand(s->Test,!state.Shutdown.Succeeded && (reason==XRUDP_CANCELLED || reason==XRUDP_CLOSED),"callback abort became success");
	(void)xrtAtomic32FetchOr(&s->Test->PeerClosed,1u<<s->Id,XMEMORY_RELEASE);
}
static void path_closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	path_side* s=(path_side*)context;(void)e;(void)error;
	demand(s->Test,++s->Closes==1 && !s->InCallback && reason==XRUDP_CANCELLED,"endpoint close contract failed");
	(void)xrtAtomic32FetchOr(&s->Test->Closed,1u<<s->Id,XMEMORY_RELEASE);
}
static void path_changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	path_side* s=(path_side*)context;path_native* t=s->Test;demand(t,!s->InCallback,"recursive path callback");s->InCallback=true;
	if(events&XRUDP_PEER_PATH_FAILED) {
		xrudpsessionstate state;bool snapshot=xrtRudpPeerSessionState(p,&state)==XRUDP_OK;
		if(t->Mode!=3 && snapshot)fprintf(stderr,"unexpected path failure: mode=%u side=%u reason=%u active=%u candidate=%u attempts=%u rx=%u tx=%u validations=%llu now=%llu rate=%llu invalid=%llu address=%llu packets=%llu keys=%llu\n",t->Mode,s->Id,(unsigned)state.Path.Failure,state.Path.Active.Port,state.Path.Candidate.Port,state.Path.Attempts,state.Path.CandidateReceived,state.Path.CandidateSent,(unsigned long long)state.Path.Validations,(unsigned long long)state.LastNowUs,(unsigned long long)state.Path.RateDropped,(unsigned long long)state.Path.Invalid,(unsigned long long)state.Path.AddressDropped,(unsigned long long)state.ReceivedPackets,(unsigned long long)state.KeyUpdates);
		demand(t,t->Mode==3 && s->Id==0 && snapshot && state.Reason==XRUDP_OK && state.Path.Failures==1 && state.Path.Failure==XRUDP_TIMEOUT && !state.Path.Validating && state.Path.Active.Port==t->UdpAddress[2].Port,"failed candidate damaged active path");
		xrtAtomic32Store(&t->PathFailed,1,XMEMORY_RELEASE);
	}
	if(events&XRUDP_PEER_PATH_CHANGED) {
		xrudpsessionstate state;xrudphandshakeinfo info;xnetaddr address;
		uint32_t expected=t->Mode==4 && s->PathHints==1?1u:s->PathHints+2;
		demand(t,expected<4 && s->Id==0 && xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Path.Validations==s->PathHints+1 && xrtRudpPeerInfo(p,&info,&address)==XRUDP_OK && address.Port==t->UdpAddress[expected].Port,"path publication/address mismatch");
		++s->PathHints;(void)xrtAtomic32FetchOr(&t->PathChanges,1u<<(s->PathHints-1),XMEMORY_RELEASE);
		if(t->Mode==1) { xrtRudpPeerDestroy(t->Peer[0]);t->Peer[0]=NULL;demand(t,xrtRudpPeerAbort(p)==XRUDP_OK,"drop+abort from path callback failed"); }
		if(t->Mode==2)demand(t,xrtRudpEndpointClose(e)==XRUDP_OK,"endpoint close from path callback failed");
	}
	s->InCallback=false;
}
static void path_stream_changed(xrudpendpoint* e,xrudppeer* p,const xrudpstreamevent* event,void* context)
{
	path_side* s=(path_side*)context;path_native* t=s->Test;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return;
	demand(t,!s->InCallback && (event->Id==1 || event->Id==3),"recursive/unknown stream");s->InCallback=true;uint32_t kind=event->Id==3?1u:0u;
	if(event->Events&XRUDP_STREAM_OPENED) { demand(t,s->Id==0 && kind==1 && !s->Live[1],"duplicate/unexpected stream open");s->Live[1]=true;path_write(s,p,1); }
	if(event->Events&(XRUDP_STREAM_READ_READY|XRUDP_STREAM_READ_EOF)) for(uint32_t step=0;step<16;++step) {
		unsigned char bytes[256];size_t n;xrudpresult r=kind?xrtRudpPeerMessageRead(p,3,bytes,sizeof(bytes),&n):xrtRudpPeerRead(p,bytes,sizeof(bytes),&n);
		if(r==XRUDP_CLOSED && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)break;
		demand(t,r==XRUDP_OK || r==XRUDP_BACKPRESSURE || r==XRUDP_DONE,"read failed");if(r!=XRUDP_OK && r!=XRUDP_DONE)break;
		for(size_t i=0;i<n;++i)demand(t,bytes[i]==path_pattern(1-s->Id,kind,s->Read[kind]+i),"rebinding duplicated/corrupted payload");
		s->Read[kind]+=n;demand(t,s->Read[kind]<=(kind?PATH_MESSAGES:PATH_BYTES),"excess payload");
		if(r==XRUDP_DONE) { demand(t,s->Read[kind]==(kind?PATH_MESSAGES:PATH_BYTES),"early EOF");s->Eof[kind]=true;break; }
	}
	if(event->Events&XRUDP_STREAM_WRITE_READY)path_write(s,p,kind);
	if(event->Events&XRUDP_STREAM_WRITE_DONE)s->Acked[kind]=true;
	path_completion(s);s->InCallback=false;
}
static void path_bind(xnetworker* worker,ptr context)
{
	path_side* s=(path_side*)context;path_native* t=s->Test;xrudpendpointconfig c;xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=t->Private[s->Id];c.PeerPublic=t->Public[1-s->Id];c.Listen=s->Id==0;
	c.Session.StreamCapacity=c.Session.Local.MaxStreams=2;c.Session.ReceiveCapacity=c.Session.TransmitCapacity=512;
	c.Session.Local.StreamCredit=128;c.Session.Local.ConnectionCredit=512;c.Session.Local.MaxMessageSize=256;
	c.Session.Local.DatagramSize=512;c.Session.Local.MaxAckDelayUs=1000;c.Session.InitialRttUs=10000;c.Session.KeyUpdatePackets=16;
	xnetaddr local;demand(t,xrtNetAddrLoopback(&local,t->Family,0),"loopback failed");
	xrudpendpointevents events={path_opened,path_confirmed,path_peer_closed,path_closed,path_changed,path_stream_changed};
	demand(t,xrtRudpEndpointBind(worker,&local,&c,&events,s,&t->Endpoint[s->Id])==XRUDP_OK,"bind failed");
}
static void proxy_opened(xnetudp* udp,ptr context)
{
	path_proxy* p=(path_proxy*)context;demand(p->Test,xrtNetUdpLocal(udp,&p->Test->UdpAddress[p->Id]),"proxy address failed");
	(void)xrtAtomic32FetchOr(&p->Test->Ready,1u<<(p->Id+2),XMEMORY_RELEASE);
}
static void proxy_receive(xnetudp* udp,const xnetudpmessage* message,ptr context)
{
	path_proxy* p=(path_proxy*)context;path_native* t=p->Test;(void)udp;
	if(xrtNetUdpState(t->Udp[0])!=XNET_UDP_OPEN)return;
	if(t->Mode==3 && p->Id==3)return; /* A real bound but unresponsive alternate path. */
	if(p->Id==0) {
		t->ProxyBytes+=message->Size;
		if(t->ProxyBytes>=6000 && xrtAtomic32Load(&t->Confirmed,XMEMORY_ACQUIRE)==3 && xrtAtomic32Load(&t->ActiveBack,XMEMORY_ACQUIRE)==1)
			xrtAtomic32Store(&t->ActiveBack,2,XMEMORY_RELEASE);
		uint32_t back=xrtAtomic32Load(&t->ActiveBack,XMEMORY_ACQUIRE);
		if(back==4)back=1; /* Return mapping; do not trigger the first move again. */
		if(xrtNetUdpState(t->Udp[back])!=XNET_UDP_OPEN)return;
		demand(t,xrtNetUdpSendTo(t->Udp[back],&t->Info[0].Local,message->Data,message->Size)==XNET_RESULT_OK,"proxy client forward failed");
	} else demand(t,xrtNetUdpSendTo(t->Udp[0],&t->Info[1].Local,message->Data,message->Size)==XNET_RESULT_OK,"proxy server forward failed");
}
static void proxy_closed(xnetudp* udp,xnetresult result,const xerror* error,ptr context)
{
	path_proxy* p=(path_proxy*)context;(void)udp;(void)result;(void)error;
	(void)xrtAtomic32FetchOr(&p->Test->ProxyClosed,1u<<p->Id,XMEMORY_RELEASE);
}
static void proxy_bind(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;xnetaddr local;demand(t,xrtNetAddrLoopback(&local,t->Family,0),"proxy loopback failed");
	xnetudpevents events={0};events.Open=proxy_opened;events.Receive=proxy_receive;events.Close=proxy_closed;
	xnetudpconfig config;xrtNetUdpConfigInit(&config);config.IPv6Only=true;config.ReceiveSize=1200;config.ReceiveBatch=8;
	config.SendLimit=65536;config.SendPacketLimit=64;config.SendHighWater=49152;config.SendLowWater=16384;
	for(uint32_t i=0;i<4;++i) { t->Proxy[i].Test=t;t->Proxy[i].Id=i;t->Udp[i]=xrtNetUdpBind(t->Engine,&local,xrtNetWorkerIndex(worker),&config,&events,&t->Proxy[i]);demand(t,t->Udp[i]!=NULL,"proxy bind failed"); }
}
static void path_dial(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;(void)worker;xrudpconnectconfig c={t->Info[0].Public,t->Info[0].Scope,NULL};
	demand(t,xrtRudpEndpointConnect(t->Endpoint[1],&t->UdpAddress[0],&c,&t->Peer[1])==XRUDP_OK,"dial failed");
	demand(t,xrtRudpPeerProbePath(t->Peer[1],&t->UdpAddress[1])==XRUDP_BACKPRESSURE,"probe before confirmation accepted");
	xrtAtomic32Store(&t->Dialed,1,XMEMORY_RELEASE);
}
static void path_probe_second(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;(void)worker;
	xrtAtomic32Store(&t->ActiveBack,3,XMEMORY_RELEASE);
	xrudpresult r=xrtRudpPeerProbePath(t->Peer[0],&t->UdpAddress[3]);demand(t,r==XRUDP_OK || r==XRUDP_DUPLICATE,"explicit second path failed");
}
static void path_shutdown(xnetworker* worker,ptr context)
{
	path_side* s=(path_side*)context;(void)worker;demand(s->Test,xrtRudpPeerShutdown(s->Test->Peer[s->Id],5000)==XRUDP_OK,"shutdown failed");
}
static void path_probe_failed(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;(void)worker;demand(t,xrtRudpPeerProbePath(t->Peer[0],&t->UdpAddress[3])==XRUDP_OK,"failed-path probe was not admitted");
}
static void path_return_mapping(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;(void)worker;xrtAtomic32Store(&t->ActiveBack,4,XMEMORY_RELEASE);
}
static int path_run(xnetfamily family,uint32_t mode)
{
	path_native t;memset(&t,0,sizeof(t));t.Family=family;t.Mode=mode;xrtAtomic32Init(&t.ActiveBack,1);
	for(uint32_t i=0;i<2;++i) { t.Side[i].Test=&t;t.Side[i].Id=i;for(size_t j=0;j<32;++j)t.Private[i][j]=(unsigned char)(19+i*71+j);CHECK(xrtX25519Public(t.Private[i],t.Public[i])); }
	xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=3;c.Backend=TEST_RUDP_BACKEND;t.Engine=xrtNetEngineCreate(&c);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,path_bind,&t.Side[i]));
	CHECK(xrtNetEnginePost(t.Engine,2,proxy_bind,&t) && path_wait(&t,&t.Ready,63));
	CHECK(xrtNetEnginePost(t.Engine,1,path_dial,&t) && path_wait(&t,&t.Dialed,1) && path_wait(&t,&t.PathChanges,1));
	if(mode==0 || mode==3 || mode==4) {
		CHECK(xrtRudpPeerProbePath(t.Peer[0],&t.UdpAddress[3])==XRUDP_ARGUMENT);
		if(mode==0)CHECK(xrtNetEnginePost(t.Engine,0,path_probe_second,&t) && path_wait(&t,&t.PathChanges,3));
		else if(mode==3)CHECK(xrtNetEnginePost(t.Engine,0,path_probe_failed,&t) && path_wait(&t,&t.PathFailed,1));
		else { double began=xrtTimer();CHECK(xrtNetEnginePost(t.Engine,2,path_return_mapping,&t) && path_wait(&t,&t.PathChanges,3) && xrtTimer()-began<5); }
		CHECK(path_wait(&t,&t.Complete,3));
		for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,path_shutdown,&t.Side[i]));
		CHECK(path_wait(&t,&t.PeerClosed,3));
	} else if(mode==1)CHECK(path_wait(&t,&t.PeerClosed,3));
	else CHECK(path_wait(&t,&t.Closed,1));
	for(uint32_t i=0;i<2;++i)CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	for(uint32_t i=0;i<4;++i)CHECK(xrtNetUdpAbort(t.Udp[i]));
	CHECK(path_wait(&t,&t.Closed,3) && path_wait(&t,&t.PeerClosed,3) && path_wait(&t,&t.ProxyClosed,15));
	CHECK(xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine) && !xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
	for(uint32_t i=0;i<2;++i) {
		if(t.Peer[i]) {
			xrudpsessionstate state;CHECK(xrtRudpPeerSessionState(t.Peer[i],&state)==XRUDP_OK && state.Path.Enabled);
			if(mode==0 || mode==3 || mode==4) { CHECK(state.Path.Validations==(i?0u:(mode==3?1u:2u)) && state.SentConnection==PATH_BYTES+PATH_MESSAGES+4*(PATH_MESSAGES/128) && state.ReceivedConnection==state.SentConnection); }
			CHECK(xrtRudpPeerProbePath(t.Peer[i],&t.UdpAddress[3])==XRUDP_CLOSED);
		}
		xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.Endpoint[i]);
	}
	for(uint32_t i=0;i<4;++i)xrtNetUdpDestroy(t.Udp[i]);
	printf("xrudp native path: backend=%d family=%u mode=%u real NAT port rebind passed\n",(int)TEST_RUDP_BACKEND,(unsigned)family,mode);return 0;
}
int main(void)
{
	CHECK(path_run(XNET_FAMILY_IPV4,0)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(path_run(XNET_FAMILY_IPV4,0)==0 && path_run(XNET_FAMILY_IPV6,0)==0 && path_run(XNET_FAMILY_IPV4,1)==0 && path_run(XNET_FAMILY_IPV4,2)==0 && path_run(XNET_FAMILY_IPV4,3)==0 && path_run(XNET_FAMILY_IPV4,4)==0 && path_run(XNET_FAMILY_IPV6,4)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	return 0;
}
