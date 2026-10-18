#define main path_fixture_main
#include "test_endpoint_path.c"
#undef main

static void mtu_native_confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	path_side* s=(path_side*)context;(void)e;if(s->Id==0)s->Test->Peer[0]=xrtRudpPeerRef(p);
	(void)xrtAtomic32FetchOr(&s->Test->Confirmed,1u<<s->Id,XMEMORY_RELEASE);
}
static void mtu_native_changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	path_side* s=(path_side*)context;path_native* t=s->Test;
	demand(t,!s->InCallback,"recursive MTU callback");s->InCallback=true;
	if(events&XRUDP_PEER_MTU_CHANGED) {
		xrudpsessionstate state;demand(t,xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Mtu.Enabled,"MTU publication missing");
		if(state.Mtu.Effective==1200)(void)xrtAtomic32FetchOr(&t->PathChanges,1u<<s->Id,XMEMORY_RELEASE);
		if(state.Mtu.Fallbacks) {
			demand(t,state.Mtu.Effective==512,"size fallback not published before callback");
			(void)xrtAtomic32FetchOr(&t->PathFailed,1u<<s->Id,XMEMORY_RELEASE);
			if(s->Id==0 && t->Mode==1) { xrtRudpPeerDestroy(t->Peer[0]);t->Peer[0]=NULL;demand(t,xrtRudpPeerAbort(p)==XRUDP_OK,"MTU callback drop+abort failed"); }
			if(s->Id==0 && t->Mode==2)demand(t,xrtRudpEndpointClose(e)==XRUDP_OK,"MTU callback endpoint close failed");
		}
	}
	s->InCallback=false;
}
static void mtu_native_bind(xnetworker* worker,ptr context)
{
	path_side* s=(path_side*)context;path_native* t=s->Test;xrudpendpointconfig c;xrtRudpEndpointConfigInit(&c);
	demand(t,c.Session.DiscoverMtu,"native defaults disable MTU discovery");
	c.StaticPrivate=t->Private[s->Id];c.PeerPublic=t->Public[1-s->Id];c.Listen=s->Id==0;
	c.Session.StreamCapacity=c.Session.Local.MaxStreams=2;c.Session.ReceiveCapacity=c.Session.TransmitCapacity=8192;
	c.Session.Local.StreamCredit=4096;c.Session.Local.ConnectionCredit=8192;c.Session.Local.MaxMessageSize=256;
	c.Session.Local.DatagramSize=1200;c.Session.Local.MaxAckDelayUs=1000;c.Session.InitialRttUs=10000;c.Session.KeyUpdatePackets=16;
	xnetaddr local;demand(t,xrtNetAddrLoopback(&local,t->Family,0),"MTU loopback failed");
	xrudpendpointevents events={path_opened,mtu_native_confirmed,path_peer_closed,path_closed,mtu_native_changed,path_stream_changed};
	demand(t,xrtRudpEndpointBind(worker,&local,&c,&events,s,&t->Endpoint[s->Id])==XRUDP_OK,"MTU bind/DF option failed");
}
static void mtu_proxy_receive(xnetudp* udp,const xnetudpmessage* m,ptr context)
{
	path_proxy* p=(path_proxy*)context;path_native* t=p->Test;(void)udp;
	if(m->Size>xrtAtomic32Load(&t->ActiveBack,XMEMORY_ACQUIRE)) { t->ProxyBytes+=m->Size;return; }
	xnetudp* out=p->Id==0?t->Udp[1]:t->Udp[0];const xnetaddr* destination=p->Id==0?&t->Info[0].Local:&t->Info[1].Local;
	if(xrtNetUdpState(out)!=XNET_UDP_OPEN)return;
	demand(t,xrtNetUdpSendTo(out,destination,m->Data,m->Size)==XNET_RESULT_OK,"MTU proxy forward failed");
}
static void mtu_proxy_bind(xnetworker* worker,ptr context)
{
	path_native* t=(path_native*)context;xnetaddr local;demand(t,xrtNetAddrLoopback(&local,t->Family,0),"MTU proxy address failed");
	xnetudpevents events={0};events.Open=proxy_opened;events.Receive=mtu_proxy_receive;events.Close=proxy_closed;
	xnetudpconfig config;xrtNetUdpConfigInit(&config);config.IPv6Only=true;config.ReceiveSize=1200;config.ReceiveBatch=8;
	config.SendLimit=65536;config.SendPacketLimit=64;config.SendHighWater=49152;config.SendLowWater=16384;
	for(uint32_t i=0;i<2;++i) { t->Proxy[i].Test=t;t->Proxy[i].Id=i;t->Udp[i]=xrtNetUdpBind(t->Engine,&local,xrtNetWorkerIndex(worker),&config,&events,&t->Proxy[i]);demand(t,t->Udp[i]!=NULL,"MTU proxy bind failed"); }
}
static void mtu_native_start(xnetworker* worker,ptr context)
{
	path_side* s=(path_side*)context;(void)worker;xrudppeer* p=s->Test->Peer[s->Id];s->Live[0]=true;path_write(s,p,0);
	if(s->Id==1) { xrudpstreamconfig config;xrtRudpStreamConfigInit(&config);config.Kind=XRUDP_STREAM_MESSAGES;uint32_t id=0;
		demand(s->Test,xrtRudpPeerStreamOpen(p,&config,&id)==XRUDP_OK && id==3,"MTU message open failed");s->Live[1]=true;path_write(s,p,1); }
}
static int mtu_native_run(xnetfamily family,uint32_t mode)
{
	path_native t;memset(&t,0,sizeof(t));t.Family=family;t.Mode=mode;xrtAtomic32Init(&t.ActiveBack,1200);
	for(uint32_t i=0;i<2;++i) { t.Side[i].Test=&t;t.Side[i].Id=i;for(size_t j=0;j<32;++j)t.Private[i][j]=(unsigned char)(23+i*79+j);CHECK(xrtX25519Public(t.Private[i],t.Public[i])); }
	xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=3;c.Backend=TEST_RUDP_BACKEND;t.Engine=xrtNetEngineCreate(&c);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,mtu_native_bind,&t.Side[i]));
	CHECK(xrtNetEnginePost(t.Engine,2,mtu_proxy_bind,&t) && path_wait(&t,&t.Ready,15));
	CHECK(xrtNetEnginePost(t.Engine,1,path_dial,&t) && path_wait(&t,&t.Dialed,1) && path_wait(&t,&t.PathChanges,3));
	xrtAtomic32Store(&t.ActiveBack,512,XMEMORY_RELEASE);
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,mtu_native_start,&t.Side[i]));
	CHECK(path_wait(&t,&t.PathFailed,mode?1u:3u));
	if(mode==0) {
		CHECK(path_wait(&t,&t.Complete,3));
		for(uint32_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,path_shutdown,&t.Side[i]));
		CHECK(path_wait(&t,&t.PeerClosed,3));
	} else if(mode==1)CHECK(path_wait(&t,&t.PeerClosed,3));
	else CHECK(path_wait(&t,&t.Closed,1));
	for(uint32_t i=0;i<2;++i)CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	for(uint32_t i=0;i<2;++i)CHECK(xrtNetUdpAbort(t.Udp[i]));
	CHECK(path_wait(&t,&t.Closed,3) && path_wait(&t,&t.PeerClosed,3) && path_wait(&t,&t.ProxyClosed,3));
	CHECK(xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine) && !xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE) && t.ProxyBytes>0);
	for(uint32_t i=0;i<2;++i) {
		if(t.Peer[i]) {
			xrudpsessionstate state;CHECK(xrtRudpPeerSessionState(t.Peer[i],&state)==XRUDP_OK && state.Mtu.Enabled);
			if(mode==0)CHECK(state.Mtu.Effective==512 && state.Mtu.BaseConfirmed && state.Mtu.Fallbacks>=1 && state.Path.Validations==0 && state.SentConnection==PATH_BYTES+PATH_MESSAGES+4*(PATH_MESSAGES/128) && state.ReceivedConnection==state.SentConnection);
		}
		xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.Endpoint[i]);xrtNetUdpDestroy(t.Udp[i]);
	}
	printf("xrudp native MTU: backend=%d family=%u mode=%u real size black hole recovered\n",(int)TEST_RUDP_BACKEND,(unsigned)family,mode);return 0;
}
int main(void)
{
	CHECK(mtu_native_run(XNET_FAMILY_IPV4,0)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(mtu_native_run(XNET_FAMILY_IPV4,0)==0 && mtu_native_run(XNET_FAMILY_IPV6,0)==0 && mtu_native_run(XNET_FAMILY_IPV4,1)==0 && mtu_native_run(XNET_FAMILY_IPV4,2)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);return 0;
}
