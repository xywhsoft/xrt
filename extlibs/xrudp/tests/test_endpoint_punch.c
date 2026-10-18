#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
typedef struct punch_test punch_test;
typedef struct punch_side {punch_test* Test;uint32_t Id;} punch_side;
struct punch_test {
	xnetengine* Engine;xrudpendpoint* Endpoint[2];xrudpendpoint* Keep;xrudppeer* Peer[2];xrudpendpointinfo Info[2];
	xnetudp* Gateway[2];xnetaddr Public[2];punch_side Side[4];
	xatomic32 Ready,Started,Results,Confirmed,ReadDone,TimedOut,Closed,GatewayClosed,Failure;
	uint64_t Id[3];uint32_t Mode,Callbacks,Dropped,Indications,Discoveries;size_t Read[2];
	bool Filter[2],InCall,InCallback;unsigned char Private[2][32],Key[2][32];xnetfamily Family;
};
static void expect(punch_test* t,bool ok,const char* why)
{if(!ok){fprintf(stderr,"punch native mode=%u: %s\n",t->Mode,why);xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE);}}
static bool wait_mask(punch_test* t,xatomic32* word,uint32_t bits)
{
	double until=xrtTimer()+10;
	while((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&bits)!=bits){if(xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE) || xrtTimer()>until)return false;xrtThreadYield();}return true;
}
static void open_endpoint(xrudpendpoint* e,void* context)
{
	punch_side* s=(punch_side*)context;expect(s->Test,xrtRudpEndpointInfo(e,&s->Test->Info[s->Id])==XRUDP_OK,"endpoint info");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Id,XMEMORY_RELEASE);
}
static void close_endpoint(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	punch_side* s=(punch_side*)context;punch_test* t=s->Test;(void)e;(void)error;
	expect(t,reason==XRUDP_CANCELLED && !t->InCallback,"close order/reason");
	if(s->Id==1 && (t->Mode==3 || t->Mode==4))expect(t,t->Callbacks==2,"pending callbacks lost on close");
	(void)xrtAtomic32FetchOr(&t->Closed,1u<<s->Id,XMEMORY_RELEASE);
}
static void close_peer(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* context)
{
	punch_side* s=(punch_side*)context;(void)e;(void)p;
	if(s->Test->Mode==0 && s->Id==0){expect(s->Test,reason==XRUDP_TIMEOUT,"closed filter did not timeout");xrtAtomic32Store(&s->Test->TimedOut,1,XMEMORY_RELEASE);}
}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	punch_side* s=(punch_side*)context;punch_test* t=s->Test;(void)e;xrudphandshakeinfo info;xnetaddr remote;
	expect(t,t->Mode!=0 && t->Filter[0] && t->Filter[1],"confirmed through closed filter");
	expect(t,xrtRudpPeerInfo(p,&info,&remote)==XRUDP_OK && xrtNetAddrEqual(&remote,&t->Public[1-s->Id]),"peer bypassed translated address");
	if(s->Id==1){t->Peer[1]=xrtRudpPeerRef(p);expect(t,xrtRudpEndpointPunchCancel(t->Endpoint[1],t->Id[0])==XRUDP_OK,"cancel on real confirmation");}
	size_t n;expect(t,xrtRudpPeerWrite(p,"nat works",9,&n)==XRUDP_OK && n==9 && xrtRudpPeerFinish(p)==XRUDP_OK,"write+FIN");
	(void)xrtAtomic32FetchOr(&t->Confirmed,1u<<s->Id,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	punch_side* s=(punch_side*)context;punch_test* t=s->Test;(void)e;
	if(events&(XRUDP_PEER_READ_READY|XRUDP_PEER_READ_EOF))for(uint32_t step=0;step<8;++step) {
		unsigned char bytes[32];size_t n;xrudpresult r=xrtRudpPeerRead(p,bytes,sizeof(bytes),&n);
		expect(t,r==XRUDP_OK || r==XRUDP_BACKPRESSURE || r==XRUDP_DONE,"read status");if(r==XRUDP_BACKPRESSURE)break;
		if(r!=XRUDP_OK && r!=XRUDP_DONE)break;
		expect(t,t->Read[s->Id]+n<=9,"excess bytes");
		if(t->Read[s->Id]+n<=9)expect(t,!memcmp(bytes,&"nat works"[t->Read[s->Id]],n),"corrupt bytes");
		t->Read[s->Id]+=n;if(r==XRUDP_DONE){expect(t,t->Read[s->Id]==9,"early EOF");(void)xrtAtomic32FetchOr(&t->ReadDone,1u<<s->Id,XMEMORY_RELEASE);break;}
	}
}
static void bind_endpoint(xnetworker* worker,ptr context)
{
	punch_side* s=(punch_side*)context;punch_test* t=s->Test;xrudpendpointconfig c;xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=t->Private[s->Id];c.PeerPublic=t->Key[1-s->Id];c.Listen=s->Id==1;c.MaxPeers=c.MaxHalfOpen=2;c.HandshakeTimeoutUs=1000000;
	xnetaddr local;expect(t,xrtNetAddrLoopback(&local,t->Family,0),"local address");
	xrudpendpointevents events={open_endpoint,confirmed,close_peer,close_endpoint,changed,NULL};
	expect(t,xrtRudpEndpointBind(worker,&local,&c,&events,s,&t->Endpoint[s->Id])==XRUDP_OK,"endpoint bind");
	if(s->Id==1)t->Keep=xrtRudpEndpointRef(t->Endpoint[1]);
}
static void open_gateway(xnetudp* udp,ptr context)
{
	punch_side* s=(punch_side*)context;expect(s->Test,xrtNetUdpLocal(udp,&s->Test->Public[s->Id-2]),"public address");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Id,XMEMORY_RELEASE);
}
/* Two independent port-restricted filters. Outbound opens only that endpoint's
 * filter. Forward on the OTHER gateway socket to rewrite the source address.
 * Indications are deliberately all lost AFTER opening the outbound filter;
 * opening does not prove any peer or receive any response. */
static void receive_gateway(xnetudp* udp,const xnetudpmessage* message,ptr context)
{
	punch_side* s=(punch_side*)context;punch_test* t=s->Test;uint32_t destination=s->Id-2,source=1-destination;
	if(!xrtNetAddrEqual(&message->Remote,&t->Info[source].Local)){++t->Dropped;return;}
	t->Filter[source]=true;xrudpstunmessage m;
	if(xrtRudpStunDecode(message->Data,message->Size,&m)==XRUDP_OK) {
		if(m.Type==XRUDP_STUN_BINDING_INDICATION){
			expect(t,source==1 && m.Fingerprint && message->Size==28,"indication source/format");++t->Indications;
			/* One mode deliberately delivers every indication to a real endpoint:
			 * any reflected response would return here and fail unexpected STUN. */
			if(t->Mode==5)expect(t,xrtNetUdpSendTo(t->Gateway[source],&t->Info[destination].Local,message->Data,message->Size)==XNET_RESULT_OK,"one-way input");
			return;
		}
		if(m.Type==XRUDP_STUN_BINDING_REQUEST && t->Mode==7) {
			xrudpaddress mapped={0};mapped.Family=(uint8_t)t->Family;mapped.Port=message->Remote.Port;memcpy(mapped.Address,message->Remote.Address,16);
			unsigned char response[64];size_t n;expect(t,xrtRudpStunResponse(m.Transaction,&mapped,true,response,sizeof(response),&n)==XRUDP_OK && xrtNetUdpSendTo(udp,&message->Remote,response,n)==XNET_RESULT_OK,"mapping response");return;
		}
		expect(t,false,"unexpected STUN");return;
	}
	if(!t->Filter[destination]){++t->Dropped;return;}
	expect(t,xrtNetUdpSendTo(t->Gateway[source],&t->Info[destination].Local,message->Data,message->Size)==XNET_RESULT_OK,"translated forward");
}
static void close_gateway(xnetudp* udp,xnetresult result,const xerror* error,ptr context)
{
	punch_side* s=(punch_side*)context;(void)udp;(void)result;(void)error;(void)xrtAtomic32FetchOr(&s->Test->GatewayClosed,1u<<(s->Id-2),XMEMORY_RELEASE);
}
static void bind_gateways(xnetworker* worker,ptr context)
{
	punch_test* t=(punch_test*)context;xnetaddr local;expect(t,xrtNetAddrLoopback(&local,t->Family,0),"gateway address");
	xnetudpconfig config;xrtNetUdpConfigInit(&config);config.IPv6Only=true;config.ReceiveSize=XRUDP_PACKET_MAX;
	xnetudpevents events={0};events.Open=open_gateway;events.Receive=receive_gateway;events.Close=close_gateway;
	for(uint32_t i=0;i<2;++i){t->Gateway[i]=xrtNetUdpBind(t->Engine,&local,xrtNetWorkerIndex(worker),&config,&events,&t->Side[i+2]);expect(t,t->Gateway[i]!=NULL,"gateway bind");}
}
static void punched(xrudpendpoint* e,uint64_t id,const xrudppunchstate* result,const xerror* error,void* context)
{
	punch_test* t=(punch_test*)context;expect(t,!t->InCall && !t->InCallback && id>=1 && id<=3,"recursive/early callback");t->InCallback=true;
	uint32_t bit=1u<<(id-1);expect(t,!(xrtAtomic32Load(&t->Results,XMEMORY_ACQUIRE)&bit),"duplicate callback");++t->Callbacks;
	xrudpresult expected=t->Mode==5?XRUDP_DONE:t->Mode==6?XRUDP_TIMEOUT:t->Mode==10?XRUDP_IO_ERROR:XRUDP_CANCELLED;
	expect(t,result->Reason==expected && (t->Mode==10?(error!=NULL && xrtErrorKind(error)==XERR_IO):error==NULL),"result reason/error");
	if(t->Mode==5)expect(t,result->Sent==6 && result->Completed==1 && !result->Failed,"burst completion was not local admission");
	if(t->Mode==6)expect(t,result->Sent>=2 && result->Sent<=3 && !result->Completed,"timeout renewed or extra indications");
	if(t->Mode==10)expect(t,result->Sent==6 && result->Failed==1 && result->Completed==1,"route error killed second candidate");
	if(t->Mode==3 && id==1){expect(t,xrtRudpEndpointPunchCancel(e,t->Id[1])==XRUDP_OK && xrtRudpEndpointClose(e)==XRUDP_OK,"cancel sibling/close");xrtRudpEndpointDestroy(t->Endpoint[1]);t->Endpoint[1]=NULL;}
	if(t->Mode==2 && id==1){expect(t,xrtRudpEndpointPunch(e,&t->Public[0],1,1000,punched,t,&t->Id[2])==XRUDP_OK && t->Id[2]==3,"reused ID/slot");expect(t,xrtRudpEndpointPunchCancel(e,t->Id[2])==XRUDP_OK,"cancel new job");}
	t->InCallback=false;(void)xrtAtomic32FetchOr(&t->Results,bit,XMEMORY_RELEASE);
}
static void discovered(xrudpendpoint* e,uint64_t id,const xrudpdiscoverystate* result,const xerror* error,void* context)
{
	punch_test* t=(punch_test*)context;(void)e;(void)id;expect(t,!t->InCall && result->Reason==XRUDP_DONE && error==NULL,"shared discovery failed");++t->Discoveries;
}
static bool uses_peer(uint32_t mode) {return mode==0 || mode==1 || mode==7 || mode==8 || mode==9;}
static void start(xnetworker* worker,ptr context)
{
	punch_test* t=(punch_test*)context;(void)worker;t->InCall=true;
#if defined(TEST_NATIVE_PUNCH_SEND_FAULTS)
	punch_native_fault_endpoint(t->Endpoint[1]);
#endif
	if(t->Mode!=0) {
		xrudpendpointinfo before,after;expect(t,xrtRudpEndpointInfo(t->Endpoint[1],&before)==XRUDP_OK,"before info");
		uint32_t count=(t->Mode>=2 && t->Mode<=4)?2:1;
		for(uint32_t i=0;i<count;++i)expect(t,xrtRudpEndpointPunch(t->Endpoint[1],t->Public,t->Mode==10?2:1,t->Mode==6?1000:0,punched,t,&t->Id[i])==XRUDP_OK && t->Id[i]==i+1,"start punch");
		if(count==2){uint64_t id=919;expect(t,xrtRudpEndpointPunch(t->Endpoint[1],t->Public,1,0,punched,t,&id)==XRUDP_BACKPRESSURE && id==919,"fixed capacity");}
		xnetaddr duplicate[2]={t->Public[0],t->Public[0]};uint64_t id=919;
		if(count==1)expect(t,xrtRudpEndpointPunch(t->Endpoint[1],duplicate,2,0,punched,t,&id)==XRUDP_ARGUMENT && id==919,"duplicate candidates");
		xnetaddr wrong;expect(t,xrtNetAddrLoopback(&wrong,t->Family==XNET_FAMILY_IPV4?XNET_FAMILY_IPV6:XNET_FAMILY_IPV4,42000),"wrong family fixture");
		expect(t,xrtRudpEndpointPunch(t->Endpoint[1],&wrong,1,0,punched,t,&id)==XRUDP_ARGUMENT && id==919,"family boundary");
		expect(t,xrtRudpEndpointInfo(t->Endpoint[1],&after)==XRUDP_OK && after.Punches==count && before.ReservedBytes==after.ReservedBytes && before.Peers==after.Peers && !t->Callbacks,"peer allocation or early callback");
		if(t->Mode==2)for(uint32_t i=0;i<count;++i)expect(t,xrtRudpEndpointPunchCancel(t->Endpoint[1],t->Id[i])==XRUDP_OK && xrtRudpEndpointPunchCancel(t->Endpoint[1],t->Id[i])==XRUDP_DONE,"cancel semantics");
		if(t->Mode==3)expect(t,xrtRudpEndpointPunchCancel(t->Endpoint[1],t->Id[0])==XRUDP_OK,"first cancellation");
		if(t->Mode==4)expect(t,xrtRudpEndpointClose(t->Endpoint[1])==XRUDP_OK,"close pending");
		if(t->Mode==7)expect(t,xrtRudpEndpointDiscover(t->Endpoint[1],&t->Public[0],1000,true,discovered,t,&id)==XRUDP_OK,"shared discovery");
	}
	if(uses_peer(t->Mode)){xrudpconnectconfig c={t->Info[1].Public,t->Info[1].Scope,NULL};expect(t,xrtRudpEndpointConnect(t->Endpoint[0],&t->Public[1],&c,&t->Peer[0])==XRUDP_OK,"connect through NAT");}
	t->InCall=false;xrtAtomic32Store(&t->Started,1,XMEMORY_RELEASE);
}
static int run(xnetfamily family,uint32_t mode)
{
	punch_test t;memset(&t,0,sizeof(t));t.Family=family;t.Mode=mode;
#if defined(TEST_NATIVE_PUNCH_SEND_FAULTS)
	punch_native_fault_reset(mode);
#endif
	for(uint32_t i=0;i<4;++i){t.Side[i].Test=&t;t.Side[i].Id=i;}
	for(uint32_t i=0;i<2;++i){for(size_t k=0;k<32;++k)t.Private[i][k]=(unsigned char)(29+i*67+k);CHECK(xrtX25519Public(t.Private[i],t.Key[i]));}
	xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=1;c.Backend=TEST_RUDP_BACKEND;t.Engine=xrtNetEngineCreate(&c);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,0,bind_endpoint,&t.Side[i]));
	CHECK(xrtNetEnginePost(t.Engine,0,bind_gateways,&t) && wait_mask(&t,&t.Ready,15));
	uint64_t invalid=919;CHECK(xrtRudpEndpointPunch(t.Endpoint[1],t.Public,1,0,punched,&t,&invalid)==XRUDP_ARGUMENT && invalid==919);
	CHECK(xrtNetEnginePost(t.Engine,0,start,&t) && wait_mask(&t,&t.Started,1));
	if(mode==0)CHECK(wait_mask(&t,&t.TimedOut,1) && !xrtAtomic32Load(&t.Confirmed,XMEMORY_ACQUIRE));
	else {CHECK(wait_mask(&t,&t.Results,mode==2?7:(mode==3 || mode==4)?3:1));if(uses_peer(mode))CHECK(wait_mask(&t,&t.Confirmed,3) && wait_mask(&t,&t.ReadDone,3));}
	for(uint32_t i=0;i<2;++i)if(t.Endpoint[i])CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetUdpAbort(t.Gateway[i]));
	CHECK(wait_mask(&t,&t.Closed,3) && wait_mask(&t,&t.GatewayClosed,3) && xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
	xrudpendpointinfo final;CHECK(xrtRudpEndpointInfo(t.Keep,&final)==XRUDP_OK && !final.Punches && final.PunchStarted==(mode==0?0u:mode==2?3u:(mode==3 || mode==4)?2u:1u) && final.PunchCompleted+final.PunchFailed==final.PunchStarted);
	CHECK(xrtRudpEndpointPunchCancel(t.Keep,1)==XRUDP_CLOSED && xrtRudpEndpointPunch(t.Keep,t.Public,1,0,punched,&t,&invalid)==XRUDP_CLOSED);
	if(mode==0)CHECK(t.Dropped>0 && !t.Filter[1]);
	if(mode==1 || mode==7 || mode==8 || mode==9)CHECK(t.Indications>=1 && t.Filter[0] && t.Filter[1]);
	if(mode==5 || mode==6)CHECK(!final.Peers && !xrtAtomic32Load(&t.Confirmed,XMEMORY_ACQUIRE));
	if(mode==7)CHECK(t.Discoveries==1 && final.DiscoverySucceeded==1);
#if defined(TEST_NATIVE_PUNCH_SEND_FAULTS)
	if(mode==8 || mode==9)CHECK(final.PunchBackpressure==1);
#endif
	CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
	for(uint32_t i=0;i<2;++i){xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.Endpoint[i]);xrtNetUdpDestroy(t.Gateway[i]);}xrtRudpEndpointDestroy(t.Keep);
	printf("xrudp punch native: backend=%d family=%u mode=%u bounded callbacks and NAT filtering passed\n",(int)TEST_RUDP_BACKEND,(unsigned)family,mode);return 0;
}
int main(void)
{
	CHECK(run(XNET_FAMILY_IPV4,1)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	for(uint32_t mode=0;mode<=7;++mode)CHECK(run(XNET_FAMILY_IPV4,mode)==0);
	CHECK(run(XNET_FAMILY_IPV6,1)==0);
#if defined(TEST_NATIVE_PUNCH_SEND_FAULTS)
	for(uint32_t mode=8;mode<=11;++mode)CHECK(run(XNET_FAMILY_IPV4,mode)==0);
#endif
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);return 0;
}
