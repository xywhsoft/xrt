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
typedef struct discovery_test discovery_test;
typedef struct discovery_side {discovery_test* Test;uint32_t Id;} discovery_side;
struct discovery_test {
	xnetengine* Engine;xrudpendpoint* Endpoint[2];xrudpendpoint* Keep;xrudppeer* Peer[2];xrudpendpointinfo Info[2];
	xnetudp* Server[2];xnetaddr ServerAddress[2];discovery_side Side[4];
	xatomic32 Ready,Started,Results,Confirmed,ReadDone,Closed,ServerClosed,Failure;
	uint64_t Id[5];uint32_t Mode,Callbacks,ServerRequests;uint16_t ObservedPort;
	bool InCall,InCallback;unsigned char Private[2][32],Public[2][32],Transaction[12];
	size_t Read[2];xnetfamily Family;
};
static void expect(discovery_test* t,bool ok,const char* why)
{if(!ok){fprintf(stderr,"discovery native mode=%u: %s\n",t->Mode,why);xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE);}}
static bool wait_mask(discovery_test* t,xatomic32* word,uint32_t bits)
{
	double until=xrtTimer()+8;
	while((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&bits)!=bits){if(xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE) || xrtTimer()>until)return false;xrtThreadYield();}return true;
}
static void open_endpoint(xrudpendpoint* e,void* context)
{
	discovery_side* s=(discovery_side*)context;expect(s->Test,xrtRudpEndpointInfo(e,&s->Test->Info[s->Id])==XRUDP_OK,"endpoint info");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Id,XMEMORY_RELEASE);
}
static void close_endpoint(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	discovery_side* s=(discovery_side*)context;discovery_test* t=s->Test;(void)e;(void)error;
	expect(t,reason==XRUDP_CANCELLED && !t->InCallback,"close ordering/reason");
	if(s->Id==1 && (t->Mode==5 || t->Mode==6))expect(t,t->Callbacks==4,"close preceded discovery callbacks");
	(void)xrtAtomic32FetchOr(&t->Closed,1u<<s->Id,XMEMORY_RELEASE);
}
static void close_peer(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* context)
{(void)e;(void)p;(void)reason;(void)context;}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	discovery_side* s=(discovery_side*)context;discovery_test* t=s->Test;(void)e;
	if(s->Id==0) {
		t->Peer[0]=xrtRudpPeerRef(p);xnetaddr remote;xrudphandshakeinfo info;
		expect(t,xrtRudpPeerInfo(p,&info,&remote)==XRUDP_OK && remote.Port==t->Info[1].Local.Port,"handshake changed UDP source port");
	}
	size_t n;expect(t,xrtRudpPeerWrite(p,"same socket",11,&n)==XRUDP_OK && n==11 && xrtRudpPeerFinish(p)==XRUDP_OK,"reliable write");
	(void)xrtAtomic32FetchOr(&t->Confirmed,1u<<s->Id,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	discovery_side* s=(discovery_side*)context;discovery_test* t=s->Test;(void)e;
	if(events&(XRUDP_PEER_READ_READY|XRUDP_PEER_READ_EOF))for(uint32_t step=0;step<8;++step) {
		unsigned char bytes[32];size_t n;xrudpresult r=xrtRudpPeerRead(p,bytes,sizeof(bytes),&n);
		expect(t,r==XRUDP_OK || r==XRUDP_BACKPRESSURE || r==XRUDP_DONE,"read status");if(r==XRUDP_BACKPRESSURE)break;
		if(r!=XRUDP_OK && r!=XRUDP_DONE)break;
		expect(t,s->Test->Read[s->Id]+n<=11,"read excess");
		if(t->Read[s->Id]+n<=11)expect(t,!memcmp(bytes,&"same socket"[t->Read[s->Id]],n),"read corruption");
		t->Read[s->Id]+=n;if(r==XRUDP_DONE){expect(t,t->Read[s->Id]==11,"early EOF");(void)xrtAtomic32FetchOr(&t->ReadDone,1u<<s->Id,XMEMORY_RELEASE);break;}
	}
}
static void bind_endpoint(xnetworker* worker,ptr context)
{
	discovery_side* s=(discovery_side*)context;discovery_test* t=s->Test;xrudpendpointconfig c;xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=t->Private[s->Id];c.PeerPublic=t->Public[1-s->Id];c.Listen=s->Id==0;c.MaxPeers=c.MaxHalfOpen=2;
	xnetaddr local;expect(t,xrtNetAddrLoopback(&local,t->Family,0),"loopback");
	xrudpendpointevents events={open_endpoint,confirmed,close_peer,close_endpoint,changed,NULL};
	expect(t,xrtRudpEndpointBind(worker,&local,&c,&events,s,&t->Endpoint[s->Id])==XRUDP_OK,"bind endpoint");
	if(s->Id==1)t->Keep=xrtRudpEndpointRef(t->Endpoint[1]);
}
static void open_server(xnetudp* udp,ptr context)
{
	discovery_side* s=(discovery_side*)context;expect(s->Test,xrtNetUdpLocal(udp,&s->Test->ServerAddress[s->Id-2]),"server address");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Id,XMEMORY_RELEASE);
}
static void receive_server(xnetudp* udp,const xnetudpmessage* message,ptr context)
{
	discovery_side* s=(discovery_side*)context;discovery_test* t=s->Test;xrudpstunmessage m;
	expect(t,s->Id==2 && xrtRudpStunDecode(message->Data,message->Size,&m)==XRUDP_OK && m.Type==XRUDP_STUN_BINDING_REQUEST,"server request");
	++t->ServerRequests;t->ObservedPort=message->Remote.Port;
	expect(t,t->ObservedPort==t->Info[1].Local.Port,"discovery used another socket");
	if(t->Mode==1 && t->ServerRequests==1){memcpy(t->Transaction,m.Transaction,12);return;}
	if(t->Mode==1)expect(t,!memcmp(t->Transaction,m.Transaction,12),"retry changed transaction");
	if(t->Mode==3 || t->Mode==6)return;
	xrudpaddress mapped;memset(&mapped,0,sizeof(mapped));mapped.Family=(uint8_t)message->Remote.Family;mapped.Port=message->Remote.Port;memcpy(mapped.Address,message->Remote.Address,16);
	unsigned char response[128];size_t n;
	if(t->Mode==11)expect(t,xrtRudpStunError(m.Transaction,500,NULL,0,m.Fingerprint,response,sizeof(response),&n)==XRUDP_OK,"error response");
	else expect(t,xrtRudpStunResponse(m.Transaction,&mapped,m.Fingerprint,response,sizeof(response),&n)==XRUDP_OK,"encode mapping");
	/* Matching nonce from wrong source, then malformed/wrong transaction from
	 * real source, must not replace the actual result. All queues bounded. */
	expect(t,xrtNetUdpSendTo(t->Server[1],&message->Remote,response,n)==XNET_RESULT_OK,"wrong-source send");
	response[8]^=1;expect(t,xrtNetUdpSendTo(udp,&message->Remote,response,n)==XNET_RESULT_OK,"wrong-transaction send");response[8]^=1;
	response[3]^=4;expect(t,xrtNetUdpSendTo(udp,&message->Remote,response,n)==XNET_RESULT_OK,"malformed send");response[3]^=4;
	expect(t,xrtNetUdpSendTo(udp,&message->Remote,response,n)==XNET_RESULT_OK,"valid response send");
}
static void close_server(xnetudp* udp,xnetresult result,const xerror* error,ptr context)
{
	discovery_side* s=(discovery_side*)context;(void)udp;(void)result;(void)error;
	(void)xrtAtomic32FetchOr(&s->Test->ServerClosed,1u<<(s->Id-2),XMEMORY_RELEASE);
}
static void bind_servers(xnetworker* worker,ptr context)
{
	discovery_test* t=(discovery_test*)context;xnetaddr local;expect(t,xrtNetAddrLoopback(&local,t->Family,0),"server loopback");
	xnetudpevents events={0};events.Open=open_server;events.Receive=receive_server;events.Close=close_server;
	xnetudpconfig config;xrtNetUdpConfigInit(&config);config.IPv6Only=true;config.ReceiveSize=512;
	for(uint32_t i=0;i<2;++i){t->Server[i]=xrtNetUdpBind(t->Engine,&local,xrtNetWorkerIndex(worker),&config,&events,&t->Side[i+2]);expect(t,t->Server[i]!=NULL,"bind server");}
}
static void dial(discovery_test* t)
{
	xrudpconnectconfig c={t->Info[0].Public,t->Info[0].Scope,NULL};
	expect(t,xrtRudpEndpointConnect(t->Endpoint[1],&t->Info[0].Local,&c,&t->Peer[1])==XRUDP_OK,"dial after discovery");
}
static void discovered(xrudpendpoint* e,uint64_t id,const xrudpdiscoverystate* result,const xerror* error,void* context)
{
	discovery_test* t=(discovery_test*)context;expect(t,!t->InCall && !t->InCallback && id>=1 && id<=5,"recursive/early discovery callback");t->InCallback=true;
	uint32_t bit=1u<<(id-1);expect(t,!(xrtAtomic32Load(&t->Results,XMEMORY_ACQUIRE)&bit),"duplicate result");++t->Callbacks;
	bool cancelled=t->Mode==2 || t->Mode==4 || t->Mode==6 || t->Mode==14 || (t->Mode==5 && id!=1) || (t->Mode==7 && id==2);
	xrudpresult expected=cancelled?XRUDP_CANCELLED:t->Mode==3?XRUDP_TIMEOUT:t->Mode==11?XRUDP_PROTOCOL_MISMATCH:t->Mode==12?XRUDP_IO_ERROR:XRUDP_DONE;
	expect(t,result->Reason==expected && (t->Mode==12?(error!=NULL && xrtErrorKind(error)==XERR_IO):error==NULL),"result reason/error");
	if(expected==XRUDP_DONE)expect(t,result->Mapped.Port==t->Info[1].Local.Port && result->Mapped.Family==(uint8_t)t->Family,"wrong mapping");
	if(t->Mode==1)expect(t,result->Attempts==2,"loss did not retry exactly once");
	if(t->Mode==11)expect(t,result->RemoteError==500,"remote error hidden");
#if defined(TEST_NATIVE_DISCOVERY_SEND_FAULTS)
	if(t->Mode==13)expect(t,result->Attempts==1,"temporary memory rejection consumed attempts");
#endif
	if((t->Mode==0 || t->Mode==1) && id==1)dial(t);
	if(t->Mode==5 && id==1){expect(t,xrtRudpEndpointClose(e)==XRUDP_OK,"close in discovery");xrtRudpEndpointDestroy(t->Endpoint[1]);t->Endpoint[1]=NULL;}
	if(t->Mode==7 && id==1) {
		expect(t,xrtRudpEndpointDiscoveryCancel(e,t->Id[1])==XRUDP_OK,"cancel sibling in callback");
		expect(t,xrtRudpEndpointDiscover(e,&t->ServerAddress[0],1000,true,discovered,t,&t->Id[4])==XRUDP_OK && t->Id[4]==5,"reused slot ID/callback");
	}
	t->InCallback=false;(void)xrtAtomic32FetchOr(&t->Results,bit,XMEMORY_RELEASE);
}
static void start(xnetworker* worker,ptr context)
{
	discovery_test* t=(discovery_test*)context;(void)worker;t->InCall=true;
#if defined(TEST_NATIVE_DISCOVERY_SEND_FAULTS)
	discovery_native_fault_endpoint(t->Endpoint[1]);
#endif
	xrudpendpointinfo before,after;expect(t,xrtRudpEndpointInfo(t->Endpoint[1],&before)==XRUDP_OK,"snapshot before discovery");
	uint32_t count=(t->Mode>=4 && t->Mode<=7)?4:1;
	for(uint32_t i=0;i<count;++i)expect(t,xrtRudpEndpointDiscover(t->Endpoint[1],&t->ServerAddress[0],1000,t->Mode!=0,discovered,t,&t->Id[i])==XRUDP_OK && t->Id[i]==i+1,"start discovery");
	if(count==4){uint64_t id=919;expect(t,xrtRudpEndpointDiscover(t->Endpoint[1],&t->ServerAddress[0],1000,true,discovered,t,&id)==XRUDP_BACKPRESSURE && id==919,"capacity/output changed");}
	if(t->Mode==2 || t->Mode==4)for(uint32_t i=0;i<count;++i){expect(t,xrtRudpEndpointDiscoveryCancel(t->Endpoint[1],t->Id[i])==XRUDP_OK && xrtRudpEndpointDiscoveryCancel(t->Endpoint[1],t->Id[i])==XRUDP_DONE,"cancel semantics");}
	expect(t,xrtRudpEndpointInfo(t->Endpoint[1],&after)==XRUDP_OK && after.Discoveries==count && before.ReservedBytes==after.ReservedBytes && before.Peers==after.Peers && !t->Callbacks,"discovery allocated peer or callback early");
	if(t->Mode==6)expect(t,xrtRudpEndpointClose(t->Endpoint[1])==XRUDP_OK,"close pending");
	if(t->Mode>=10 && t->Mode!=14)dial(t);
	t->InCall=false;xrtAtomic32Store(&t->Started,1,XMEMORY_RELEASE);
}
static int run(xnetfamily family,uint32_t mode)
{
	discovery_test t;memset(&t,0,sizeof(t));t.Family=family;t.Mode=mode;
#if defined(TEST_NATIVE_DISCOVERY_SEND_FAULTS)
	discovery_native_fault_reset(mode);
#endif
	for(uint32_t i=0;i<4;++i){t.Side[i].Test=&t;t.Side[i].Id=i;}
	for(uint32_t i=0;i<2;++i){for(size_t k=0;k<32;++k)t.Private[i][k]=(unsigned char)(19+i*73+k);CHECK(xrtX25519Public(t.Private[i],t.Public[i]));}
	xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=3;c.Backend=TEST_RUDP_BACKEND;t.Engine=xrtNetEngineCreate(&c);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,bind_endpoint,&t.Side[i]));
	CHECK(xrtNetEnginePost(t.Engine,2,bind_servers,&t) && wait_mask(&t,&t.Ready,15));
	uint64_t invalid=919;CHECK(xrtRudpEndpointDiscover(t.Endpoint[1],&t.ServerAddress[0],1000,false,discovered,&t,&invalid)==XRUDP_ARGUMENT && invalid==919);
	CHECK(xrtNetEnginePost(t.Engine,1,start,&t) && wait_mask(&t,&t.Started,1));
	uint32_t bits=mode==7?31:(mode>=4 && mode<=6)?15:1;CHECK(wait_mask(&t,&t.Results,bits));
	if(mode==0 || mode==1 || (mode>=10 && mode!=14))CHECK(wait_mask(&t,&t.Confirmed,3) && wait_mask(&t,&t.ReadDone,3));
	for(uint32_t i=0;i<2;++i)if(t.Endpoint[i])CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetUdpAbort(t.Server[i]));
	CHECK(wait_mask(&t,&t.Closed,3) && wait_mask(&t,&t.ServerClosed,3) && xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
	xrudpendpointinfo final;CHECK(xrtRudpEndpointInfo(t.Keep,&final)==XRUDP_OK && !final.Discoveries && final.DiscoveryStarted==(mode==7?5u:(bits==15?4u:1u)) && final.DiscoverySucceeded+final.DiscoveryFailed==final.DiscoveryStarted);
	CHECK(xrtRudpEndpointDiscoveryCancel(t.Keep,1)==XRUDP_CLOSED && xrtRudpEndpointDiscover(t.Keep,&t.ServerAddress[0],1000,false,discovered,&t,&invalid)==XRUDP_CLOSED);
	CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
	for(uint32_t i=0;i<2;++i){xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.Endpoint[i]);xrtNetUdpDestroy(t.Server[i]);}xrtRudpEndpointDestroy(t.Keep);
	printf("xrudp discovery native: backend=%d family=%u mode=%u same socket/port and bounded callbacks passed\n",(int)TEST_RUDP_BACKEND,(unsigned)family,mode);return 0;
}
int main(void)
{
	CHECK(run(XNET_FAMILY_IPV4,0)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(run(XNET_FAMILY_IPV4,0)==0 && run(XNET_FAMILY_IPV6,0)==0);
	for(uint32_t mode=1;mode<=7;++mode)CHECK(run(XNET_FAMILY_IPV4,mode)==0);
	CHECK(run(XNET_FAMILY_IPV4,10)==0 && run(XNET_FAMILY_IPV6,11)==0);
#if defined(TEST_NATIVE_DISCOVERY_SEND_FAULTS)
	CHECK(run(XNET_FAMILY_IPV4,12)==0 && run(XNET_FAMILY_IPV4,13)==0 && run(XNET_FAMILY_IPV4,14)==0);
#endif
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);return 0;
}
