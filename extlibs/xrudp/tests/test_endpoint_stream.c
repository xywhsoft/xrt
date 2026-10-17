#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_NATIVE_STREAM_MEMORY
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
#define NATIVE_BYTES 32768u
typedef struct stream_case stream_case;
typedef struct stream_side {
	stream_case* Test; unsigned int Index;
	size_t Written, Read; unsigned int Changes, PeerCloses, Closes;
	bool Fin, Eof, Acked, InChanged;
} stream_side;
struct stream_case {
	xnetengine* Engine; xrudpendpoint* Endpoint[2]; xrudppeer* Peer[2];
	xrudpendpointinfo Identity[2]; stream_side Side[2];
	xatomic32 Ready, Dialed, Confirmed, Complete, PeerClosed, Closed, Failure;
	unsigned char Private[2][32], Public[2][32], Input[2][NATIVE_BYTES];
	unsigned int Mode; xnetfamily Family;
};
static void require(stream_side* s, bool ok, const char* message)
{
	if (!ok) { fprintf(stderr,"native stream: side=%u mode=%u %s\n",s->Index,s->Test->Mode,message); xrtAtomic32Store(&s->Test->Failure,1,XMEMORY_RELEASE); }
}
static bool wait_for(stream_case* t, xatomic32* value, uint32 bits)
{
	double deadline = xrtTimer() + 8;
	while ((xrtAtomic32Load(value,XMEMORY_ACQUIRE) & bits) != bits) {
		if (xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE) || xrtTimer() > deadline) return false;
		xrtThreadYield();
	}
	return true;
}
static void write_more(stream_side* s, xrudppeer* p)
{
	if (s->Fin) return;
	size_t written = 0;
	xrudpresult r = xrtRudpPeerWrite(p,s->Test->Input[s->Index] + s->Written,NATIVE_BYTES-s->Written,&written);
	if (r==XRUDP_CLOSED && xrtRudpEndpointState(s->Test->Endpoint[s->Index])!=XRUDP_ENDPOINT_OPEN) return;
	require(s,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"write failed"); s->Written += written;
	if (s->Written == NATIVE_BYTES) { require(s,xrtRudpPeerFinish(p)==XRUDP_OK,"FIN rejected"); s->Fin = true; }
}
static void opened(xrudpendpoint* e, void* context)
{
	stream_side* s = (stream_side*)context;
	require(s,xrtRudpEndpointInfo(e,&s->Test->Identity[s->Index])==XRUDP_OK,"bound identity missing");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e, xrudppeer* p, void* context)
{
	stream_side* s = (stream_side*)context; (void)e;
	xrudpsessionstate state;
	require(s,xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Reason==XRUDP_OK,"Confirmed before data admission");
	if (s->Index == 0) s->Test->Peer[0] = xrtRudpPeerRef(p);
	write_more(s,p);
	(void)xrtAtomic32FetchOr(&s->Test->Confirmed,1u<<s->Index,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* e, xrudppeer* p, uint32 events, void* context)
{
	stream_side* s = (stream_side*)context; stream_case* t = s->Test;
	require(s,!s->InChanged && xrtRudpPeerState(p)==XRUDP_PEER_CONFIRMED,"recursive or premature data callback");
	s->InChanged = true; ++s->Changes;
	if (events & (XRUDP_PEER_READ_READY | XRUDP_PEER_READ_EOF)) {
		unsigned char output[73];
		for (size_t step=0;step<16;++step) {
			size_t count; xrudpresult r = xrtRudpPeerRead(p,output,sizeof(output),&count);
			if (r==XRUDP_CLOSED && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN) break; /* Cross-thread cancel. */
			require(s,r==XRUDP_OK || r==XRUDP_DONE || r==XRUDP_BACKPRESSURE,"read failed");
			if (r!=XRUDP_OK && r!=XRUDP_DONE && r!=XRUDP_BACKPRESSURE) break;
			if (s->Read+count>NATIVE_BYTES) { require(s,false,"duplicate/excess bytes"); break; }
			require(s,memcmp(output,t->Input[1-s->Index]+s->Read,count)==0,"byte corruption/order mismatch"); s->Read+=count;
			if (t->Mode && s->Index==1 && count) {
				if (t->Mode==1) {
					/* Driver must own a temporary even after the user drops its
					 * last peer handle inside the readiness callback. */
					xrtRudpPeerDestroy(t->Peer[1]); t->Peer[1]=NULL;
					require(s,xrtRudpPeerAbort(p)==XRUDP_OK,"callback abort failed");
				} else require(s,xrtRudpEndpointClose(e)==XRUDP_OK,"callback endpoint close failed");
				s->InChanged=false; return;
			}
			if (r==XRUDP_DONE) { require(s,s->Read==NATIVE_BYTES,"premature EOF"); s->Eof=true; break; }
			if (r==XRUDP_BACKPRESSURE) break;
			require(s,count!=0,"successful empty read would spin"); if (!count) break;
		}
	}
	if (xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN) { s->InChanged=false; return; }
	if (events & XRUDP_PEER_WRITE_READY) write_more(s,p);
	if (events & XRUDP_PEER_WRITE_DONE) {
		xrudpsessionstate state;
		require(s,xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Transmit.PendingBytes==0 && state.Transmit.FinAcknowledged,
			"write completion was not fully acknowledged"); s->Acked=true;
	}
	if (s->Acked && s->Eof) (void)xrtAtomic32FetchOr(&t->Complete,1u<<s->Index,XMEMORY_RELEASE);
	s->InChanged=false;
}
static void peer_closed(xrudpendpoint* e, xrudppeer* p, xrudpresult reason, void* context)
{
	stream_side* s=(stream_side*)context; (void)e;
	require(s,++s->PeerCloses==1 && xrtRudpPeerState(p)==XRUDP_PEER_CLOSED,"duplicate/premature peer close");
	xrudpsessionstate state;
	require(s,xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Reason!=XRUDP_OK && state.Sent.PendingPackets==0 && state.Sent.BytesInFlight==0,
		"terminal session snapshot not drained");
	require(s,reason==XRUDP_CANCELLED || reason==XRUDP_CLOSED,"unexpected peer failure");
	(void)xrtAtomic32FetchOr(&s->Test->PeerClosed,1u<<s->Index,XMEMORY_RELEASE);
}
static void closed(xrudpendpoint* e, xrudpresult reason, const xerror* error, void* context)
{
	stream_side* s=(stream_side*)context; (void)e; (void)error;
	require(s,!s->InChanged && ++s->Closes==1 && reason==XRUDP_CANCELLED,"endpoint terminal callback raced readiness");
	(void)xrtAtomic32FetchOr(&s->Test->Closed,1u<<s->Index,XMEMORY_RELEASE);
}
static void bind_one(xnetworker* worker, ptr context)
{
	stream_side* s=(stream_side*)context; stream_case* t=s->Test;
	xrudpendpointconfig cfg; xrtRudpEndpointConfigInit(&cfg);
	cfg.StaticPrivate=t->Private[s->Index]; cfg.PeerPublic=t->Public[1-s->Index]; cfg.Listen=s->Index==0;
	cfg.Session.ReceiveCapacity=cfg.Session.TransmitCapacity=512;
	cfg.Session.Local.StreamCredit=512; cfg.Session.Local.ConnectionCredit=512;
	cfg.Session.Local.DatagramSize=512; cfg.Session.Local.MaxAckDelayUs=1000;
	cfg.Session.InitialRttUs=10000; cfg.Session.KeyUpdatePackets=16;
	xnetaddr local; require(s,xrtNetAddrLoopback(&local,t->Family,0),"loopback failed");
	xrudpendpointevents events={opened,confirmed,peer_closed,closed,changed};
	/* Handshake admission must budget the future session, not just the small
	 * half-open object. This limit cannot hold even its reserved data rings. */
	size_t saved=cfg.MemoryLimit; xrudpsessionconfig small=cfg.Session;
	cfg.Session.ReceiveCapacity=cfg.Session.TransmitCapacity=65536; cfg.MemoryLimit=65536;
	xrudpendpoint* rejected=NULL;
	require(s,xrtRudpEndpointBind(worker,&local,&cfg,&events,s,&rejected)==XRUDP_ARGUMENT && rejected==NULL,"undersized budget admitted");
	cfg.MemoryLimit=saved; cfg.Session=small;
	require(s,xrtRudpEndpointBind(worker,&local,&cfg,&events,s,&t->Endpoint[s->Index])==XRUDP_OK,"bind failed");
}
static void dial(xnetworker* worker, ptr context)
{
	stream_case* t=(stream_case*)context; (void)worker;
	xrudpconnectconfig cfg={t->Identity[0].Public,t->Identity[0].Scope,NULL}; stream_side* s=&t->Side[1];
	require(s,xrtRudpEndpointConnect(t->Endpoint[1],&t->Identity[0].Local,&cfg,&t->Peer[1])==XRUDP_OK,"dial failed");
	unsigned char byte=0; size_t n=42; xrudpsessionstate state;
	require(s,xrtRudpPeerWrite(t->Peer[1],&byte,1,&n)==XRUDP_BACKPRESSURE && n==0,"pre-confirmation write accepted");
	require(s,xrtRudpPeerRead(t->Peer[1],&byte,1,&n)==XRUDP_BACKPRESSURE && n==0,"pre-confirmation read accepted");
	require(s,xrtRudpPeerSessionState(t->Peer[1],&state)==XRUDP_BACKPRESSURE,"pre-confirmation data snapshot exposed");
	(void)xrtAtomic32FetchOr(&t->Dialed,1,XMEMORY_RELEASE);
}
static int run(xnetfamily family, unsigned int mode)
{
	stream_case t; memset(&t,0,sizeof(t)); t.Mode=mode; t.Family=family;
	xrtAtomic32Init(&t.Ready,0); xrtAtomic32Init(&t.Dialed,0); xrtAtomic32Init(&t.Confirmed,0); xrtAtomic32Init(&t.Complete,0);
	xrtAtomic32Init(&t.PeerClosed,0); xrtAtomic32Init(&t.Closed,0); xrtAtomic32Init(&t.Failure,0);
	for (size_t i=0;i<2;++i) {
		t.Side[i].Test=&t; t.Side[i].Index=(unsigned int)i;
		for (size_t k=0;k<32;++k) t.Private[i][k]=(unsigned char)(k+13+i*79);
		CHECK(xrtX25519Public(t.Private[i],t.Public[i]));
		for (size_t k=0;k<NATIVE_BYTES;++k) t.Input[i][k]=(unsigned char)((k*37+i*91)^(k>>9));
	}
	xnetengineconfig cfg; xrtNetEngineConfigInit(&cfg); cfg.Workers=2; cfg.Backend=TEST_RUDP_BACKEND;
	t.Engine=xrtNetEngineCreate(&cfg); CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for (size_t i=0;i<2;++i) CHECK(xrtNetEnginePost(t.Engine,i,bind_one,&t.Side[i]));
	CHECK(wait_for(&t,&t.Ready,3)); CHECK(xrtNetEnginePost(t.Engine,1,dial,&t)); CHECK(wait_for(&t,&t.Dialed,1));
	if (mode==0) {
		CHECK(wait_for(&t,&t.Complete,3));
		unsigned char byte=0; size_t n=42;
		CHECK(xrtRudpPeerWrite(t.Peer[0],&byte,1,&n)==XRUDP_ARGUMENT && n==0); /* Wrong owner. */
	} else if (mode==1) CHECK(wait_for(&t,&t.PeerClosed,3));
	else CHECK(wait_for(&t,&t.Closed,2));
	for (size_t i=0;i<2;++i) CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	CHECK(wait_for(&t,&t.Closed,3) && wait_for(&t,&t.PeerClosed,3));
	CHECK(xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
	CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
	for (size_t i=0;i<2;++i) {
		if (t.Peer[i]) {
			xrudpsessionstate state; CHECK(xrtRudpPeerSessionState(t.Peer[i],&state)==XRUDP_OK && state.Reason!=XRUDP_OK);
			if (mode==0) CHECK(state.KeyUpdates>0 && state.SentConnection==NATIVE_BYTES && state.ReceivedConnection==NATIVE_BYTES);
			size_t n=42; CHECK(xrtRudpPeerRead(t.Peer[i],t.Input[i],1,&n)==XRUDP_CLOSED && n==0);
		}
		xrtRudpPeerDestroy(t.Peer[i]); xrtRudpEndpointDestroy(t.Endpoint[i]);
	}
	printf("xrudp native byte stream: backend=%d family=%d mode=%u passed\n",(int)TEST_RUDP_BACKEND,(int)family,mode); return 0;
}
int main(void)
{
#ifdef TEST_NATIVE_STREAM_MEMORY
	CHECK(run(XNET_FAMILY_IPV4,0)==0); /* Warm global crypto/network facilities. */
	xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
#endif
	CHECK(run(XNET_FAMILY_IPV4,0)==0); CHECK(run(XNET_FAMILY_IPV6,0)==0);
	CHECK(run(XNET_FAMILY_IPV4,1)==0); CHECK(run(XNET_FAMILY_IPV4,2)==0);
#ifdef TEST_NATIVE_STREAM_MEMORY
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
	CHECK(before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp native data: engine/endpoint/session/closed-handle lifetimes returned to baseline");
#endif
	return 0;
}
