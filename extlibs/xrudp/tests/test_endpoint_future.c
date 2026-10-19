#include <xrudp.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"async failure %u: %s\n",__LINE__,#e); return 1; } } while (0)
typedef struct async_test async_test;
typedef struct async_side { async_test* Test; unsigned Index; } async_side;
struct async_test {
	xnetengine* Engine; xrudpendpoint* Endpoint[2]; xrudppeer* Peer[2]; xrudpendpointinfo Identity[2];
	xatomic32 Ready, Confirmed, Closed, Failure, RemoteStream, Gate, GateRelease, AbortArmed, Dialed;
	unsigned char Private[2][32], Public[2][32]; async_side Side[2]; xnetfamily Family; bool Native;
	bool Unconfirmed, Disabled, CloseOom, TimerOom;
	xrudpresult TimerReason; const xerror* TimerError; bool TimerTriggered;
	xfuture* AbortFuture;
};
static void expect(async_test* t,bool ok,const char* message)
{ if (!ok) { fprintf(stderr,"async callback: %s\n",message); xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE); } }
static bool bits(async_test* t,xatomic32* word,uint32 mask)
{
	double end=xrtTimer()+5;
	while ((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&mask)!=mask) {
		if (xrtTimer()>=end || xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE)) return false;
		xrtSleep(1);
	}
	return true;
}
static bool ready(async_test* t,xfuture* f)
{ return f && xrtFutureWaitFor(f,5000)==XWAIT_OK && !xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE); }
static const xrudpoperationresult* value(async_test* t,xfuture* f)
{ return ready(t,f) && xrtFutureState(f)==XFUTURE_RESOLVED ? xrtFutureValue(f):NULL; }
static void opened(xrudpendpoint* e,void* data)
{
	async_side* s=data; expect(s->Test,xrtRudpEndpointInfo(e,&s->Test->Identity[s->Index])==XRUDP_OK,"identity");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* data)
{
	async_side* s=data; (void)e;
	if (!s->Test->Peer[s->Index]) s->Test->Peer[s->Index]=xrtRudpPeerRef(p);
	(void)xrtAtomic32FetchOr(&s->Test->Confirmed,1u<<s->Index,XMEMORY_RELEASE);
}
static void peer_closed(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* data)
{
	async_side* s=data; async_test* t=s->Test; xrudpasyncstate state; (void)p; (void)reason;
	expect(t,xrtRudpEndpointAsyncState(e,&state)==XRUDP_OK && !state.Pending,"pending work before PeerClose");
	if (s->Index==1 && xrtAtomic32Load(&t->AbortArmed,XMEMORY_ACQUIRE))
		expect(t,t->AbortFuture && xrtFutureState(t->AbortFuture)==XFUTURE_RESOLVED,"admitted ABORT result precedes PeerClose");
}
static void closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* data)
{
	async_side* s=data; xrudpasyncstate state; (void)reason; (void)error;
	expect(s->Test,xrtRudpEndpointAsyncState(e,&state)==XRUDP_OK && !state.Pending,"pending work before endpoint Close");
	if (s->Index==1 && s->Test->CloseOom) {
		expect(s->Test,!xrtMemDebugFailTriggered(),"pending Future settlement allocated during close"); xrtMemDebugFailClear();
	}
	if (s->Index==1 && s->Test->TimerOom) {
		s->Test->TimerReason=reason; s->Test->TimerError=error; s->Test->TimerTriggered=xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
	}
	(void)xrtAtomic32FetchOr(&s->Test->Closed,1u<<s->Index,XMEMORY_RELEASE);
}
static void streams(xrudpendpoint* e,xrudppeer* p,const xrudpstreamevent* event,void* data)
{
	async_side* s=data; (void)e; (void)p;
	if (!s->Index && (event->Events&XRUDP_STREAM_OPENED)) xrtAtomic32Store(&s->Test->RemoteStream,event->Id,XMEMORY_RELEASE);
}
static void bind_side(xnetworker* worker,ptr data)
{
	async_side* s=data; async_test* t=s->Test; unsigned i=s->Index;
	xrudpendpointconfig c; xrtRudpEndpointConfigInit(&c);
	c.StaticPrivate=t->Private[i]; c.PeerPublic=t->Public[1-i]; c.Listen=i==0 && !t->Unconfirmed;
	unsigned char scope[16],context[32]; memset(scope,17,sizeof(scope)); memset(context,22,sizeof(context));
	c.Scope=scope; c.ApplicationContext=context; c.MaxPeers=c.MaxHalfOpen=1; c.MemoryLimit=8u*1024u*1024u;
	if (!t->Disabled) { c.AsyncOperations=16; c.AsyncBytes=128u*1024u; }
	c.Session.DiscoverMtu=false; c.Session.StreamCapacity=c.Session.Local.MaxStreams=3;
	c.Session.Local.MaxMessageSize=8192; c.Session.ReceiveCapacity=1024; c.Session.TransmitCapacity=8192;
	c.Session.Local.StreamCredit=1024; c.Session.Local.ConnectionCredit=4096; c.Session.KeyUpdatePackets=16;
	xrtRudpDatagramConfigInit(&c.Session.Datagrams);
	xnetaddr address; expect(t,xrtNetAddrLoopback(&address,t->Family,0),"local address");
	xrudpendpointevents events={opened,confirmed,peer_closed,closed,NULL,streams};
	expect(t,xrtRudpEndpointBind(worker,&address,&c,&events,s,&t->Endpoint[i])==XRUDP_OK,"bind");
}
static void dial(xnetworker* worker,ptr data)
{
	async_test* t=data; (void)worker; unsigned char context[32]; memset(context,22,sizeof(context));
	xrudpconnectconfig c={t->Public[0],t->Identity[0].Scope,context};
	expect(t,xrtRudpEndpointConnect(t->Endpoint[1],&t->Identity[0].Local,&c,&t->Peer[1])==XRUDP_OK,"dial");
	xrtAtomic32Store(&t->Dialed,1,XMEMORY_RELEASE);
}
static int start_mode(async_test* t,xnetfamily family,bool native,bool unconfirmed,bool disabled,bool no_cache)
{
	memset(t,0,sizeof(*t)); t->Family=family; t->Native=native; t->Unconfirmed=unconfirmed; t->Disabled=disabled;
	xnetengineconfig c; xrtNetEngineConfigInit(&c); c.Workers=2; c.Backend=XNET_PORT_SELECT;
	if (no_cache) c.NodeCacheBytes=0;
	if (native) {
#if defined(_WIN32)
		c.Backend=XNET_PORT_IOCP;
#else
		c.Backend=XNET_PORT_EPOLL;
#endif
	}
	for (unsigned i=0;i<2;++i) {
		for (unsigned j=0;j<32;++j) t->Private[i][j]=(unsigned char)(33u+58u*i+j);
		CHECK(xrtX25519Public(t->Private[i],t->Public[i])); t->Side[i]=(async_side){t,i};
	}
	t->Engine=xrtNetEngineCreate(&c); CHECK(t->Engine && xrtNetEngineStart(t->Engine));
	CHECK(xrtNetEnginePost(t->Engine,0,bind_side,&t->Side[0]) && xrtNetEnginePost(t->Engine,1,bind_side,&t->Side[1]));
	CHECK(bits(t,&t->Ready,3) && xrtNetEnginePost(t->Engine,1,dial,t) && bits(t,&t->Dialed,1));
	if (!unconfirmed) CHECK(bits(t,&t->Confirmed,3));
	return 0;
}
static int start(async_test* t,xnetfamily family,bool native)
{ return start_mode(t,family,native,false,false,false); }
static int stop(async_test* t)
{
	CHECK(xrtRudpEndpointClose(t->Endpoint[0])==XRUDP_OK && xrtRudpEndpointClose(t->Endpoint[1])==XRUDP_OK);
	CHECK(bits(t,&t->Closed,3)); double end=xrtTimer()+5;
	while (!xrtNetEngineStop(t->Engine)) { CHECK(xrtTimer()<end); xrtSleep(1); }
	while (!xrtNetEngineDestroy(t->Engine)) { CHECK(xrtTimer()<end); xrtSleep(1); }
	return 0;
}
static void release(async_test* t)
{ for (unsigned i=0;i<2;++i) { xrtRudpPeerDestroy(t->Peer[i]); xrtRudpEndpointDestroy(t->Endpoint[i]); } }
static xfuture* request(async_test* t,unsigned side,xrudpoperationkind kind,uint32 stream,const void* data,size_t size,size_t capacity,uint32 timeout)
{
	xrudpoperation r; xrtRudpOperationInit(&r); r.Kind=kind; r.Stream=stream; r.Data=data; r.Size=size; r.Capacity=capacity; r.TimeoutMs=timeout;
	return xrtRudpPeerSubmitAsync(t->Peer[side],&r);
}
/* A bounded, test-only owner barrier makes cancellation-before-admission
 * deterministic without changing the library clock or adding production hooks. */
static void gate(xnetworker* worker,ptr data)
{
	async_test* t=data; (void)worker; double end=xrtTimer()+3;
	xrtAtomic32Store(&t->Gate,1,XMEMORY_RELEASE);
	while (!xrtAtomic32Load(&t->GateRelease,XMEMORY_ACQUIRE) && xrtTimer()<end) xrtSleep(1);
	expect(t,xrtAtomic32Load(&t->GateRelease,XMEMORY_ACQUIRE)!=0,"test owner gate timed out");
	if (t->TimerOom) expect(t,xrtMemDebugFailAfter(0),"arm owner timer allocation failure");
}
static int byte_and_cancel(async_test* t,xfuture** retained)
{
	unsigned char input[257],expected[257]; for (size_t i=0;i<sizeof(input);++i) input[i]=(unsigned char)i;
	memcpy(expected,input,sizeof(input));
	xfuture* read=request(t,0,XRUDP_OPERATION_READ,1,NULL,0,17,1000);
	xfuture* write=request(t,1,XRUDP_OPERATION_WRITE,1,input,sizeof(input),0,1000); memset(input,0,sizeof(input));
	const xrudpoperationresult* v=value(t,write); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(input));
	v=value(t,read); CHECK(v && v->Result==XRUDP_OK && v->Size==17 && !memcmp(v->Data,expected,17));
	*retained=read; xrtFutureDestroy(write);
	read=request(t,0,XRUDP_OPERATION_READ,1,NULL,0,256,1000); v=value(t,read);
	CHECK(v && v->Result==XRUDP_OK && v->Size==240 && !memcmp(v->Data,expected+17,240)); xrtFutureDestroy(read);
	read=request(t,1,XRUDP_OPERATION_READ,1,NULL,0,16,40); v=value(t,read);
	CHECK(v && v->Result==XRUDP_TIMEOUT && v->Size==0 && v->Data==NULL && xrtRudpPeerState(t->Peer[1])==XRUDP_PEER_CONFIRMED); xrtFutureDestroy(read);
	CHECK(xrtNetEnginePost(t->Engine,1,gate,t) && bits(t,&t->Gate,1));
	write=request(t,1,XRUDP_OPERATION_WRITE,1,"forbidden",9,0,1000); CHECK(write && xrtFutureCancel(write));
	read=request(t,1,XRUDP_OPERATION_READ,1,NULL,0,8,1000); CHECK(read && xrtFutureCancel(read));
	xfuture* expired=request(t,1,XRUDP_OPERATION_WRITE,1,"expired",7,0,25); CHECK(expired); xrtSleep(50);
	xrtAtomic32Store(&t->GateRelease,1,XMEMORY_RELEASE);
	CHECK(ready(t,write) && xrtFutureState(write)==XFUTURE_CANCELLED && ready(t,read) && xrtFutureState(read)==XFUTURE_CANCELLED);
	xrtFutureDestroy(write); xrtFutureDestroy(read);
	v=value(t,expired); CHECK(v && v->Result==XRUDP_TIMEOUT && !v->Size && !v->Data); xrtFutureDestroy(expired);
	write=request(t,1,XRUDP_OPERATION_WRITE,1,"allowed",7,0,1000); v=value(t,write); CHECK(v && v->Result==XRUDP_OK && v->Size==7);
	(void)xrtFutureCancel(write); CHECK(xrtFutureState(write)==XFUTURE_RESOLVED); xrtFutureDestroy(write);
	read=request(t,0,XRUDP_OPERATION_READ,1,NULL,0,16,1000); v=value(t,read);
	CHECK(v && v->Result==XRUDP_OK && v->Size==7 && !memcmp(v->Data,"allowed",7)); xrtFutureDestroy(read);
	write=request(t,1,XRUDP_OPERATION_WRITE,1,"unobserved",10,0,1000); CHECK(write); xrtFutureDestroy(write);
	read=request(t,0,XRUDP_OPERATION_READ,1,NULL,0,16,1000); v=value(t,read);
	CHECK(v && v->Result==XRUDP_OK && v->Size==10 && !memcmp(v->Data,"unobserved",10)); xrtFutureDestroy(read);
	return 0;
}
static int messages_and_quota(async_test* t,uint32* id)
{
	xrudpoperation r; xrtRudpOperationInit(&r); r.Kind=XRUDP_OPERATION_STREAM_OPEN;
	xrtRudpStreamConfigInit(&r.StreamConfig); r.StreamConfig.Kind=XRUDP_STREAM_MESSAGES;
	xfuture* f=xrtRudpPeerSubmitAsync(t->Peer[1],&r); const xrudpoperationresult* v=value(t,f);
	CHECK(v && v->Result==XRUDP_OK); *id=v->Stream; xrtFutureDestroy(f);
	double end=xrtTimer()+3;
	while (xrtAtomic32Load(&t->RemoteStream,XMEMORY_ACQUIRE)!=*id) { CHECK(xrtTimer()<end); xrtSleep(1); }
	unsigned char message[2048]; for (size_t i=0;i<sizeof(message);++i) message[i]=(unsigned char)(i*19u);
	xfuture* small=request(t,0,XRUDP_OPERATION_MESSAGE_READ,*id,NULL,0,32,2000);
	xfuture* full=request(t,0,XRUDP_OPERATION_MESSAGE_READ,*id,NULL,0,4096,2000);
	xfuture* write=request(t,1,XRUDP_OPERATION_MESSAGE_WRITE,*id,message,sizeof(message),0,2000);
	v=value(t,write); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(message)); xrtFutureDestroy(write);
	v=value(t,small); CHECK(v && v->Result==XRUDP_TRUNCATED && v->Size==sizeof(message) && v->Data==NULL); xrtFutureDestroy(small);
	v=value(t,full); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(message) && !memcmp(v->Data,message,sizeof(message))); xrtFutureDestroy(full);
	write=request(t,1,XRUDP_OPERATION_MESSAGE_WRITE,*id,NULL,0,0,1000);
	full=request(t,0,XRUDP_OPERATION_MESSAGE_READ,*id,NULL,0,0,1000);
	CHECK(value(t,write) && value(t,write)->Result==XRUDP_OK && value(t,full) && value(t,full)->Result==XRUDP_OK && !value(t,full)->Size);
	xrtFutureDestroy(write); xrtFutureDestroy(full);
	xfuture* held[16]={0}; unsigned count=0;
	xrtRudpOperationInit(&r); r.Kind=XRUDP_OPERATION_DATAGRAM_CONFIGURE; xrtRudpDatagramChannelInit(&r.ChannelConfig); r.ChannelConfig.Id=11;
	while (count<16 && (held[count]=xrtRudpPeerSubmitAsync(t->Peer[0],&r))!=NULL) { CHECK(value(t,held[count])); ++count; }
	CHECK(count==15 && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_AGAIN);
	xrudpasyncstate state; CHECK(xrtRudpEndpointAsyncState(t->Endpoint[0],&state)==XRUDP_OK && state.Retained==16 && !state.Pending);
	for (unsigned i=0;i<count;++i) xrtFutureDestroy(held[i]);
	CHECK(xrtRudpEndpointAsyncState(t->Endpoint[0],&state)==XRUDP_OK && state.Retained==1 && !state.Pending);
	r.Kind=XRUDP_OPERATION_READ; r.Capacity=128u*1024u;
	CHECK(xrtRudpPeerSubmitAsync(t->Peer[0],&r)==NULL && xrtErrorKind(xrtGetError())==XERR_RANGE);
	return 0;
}
static int lifecycle(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0); xfuture* retained=NULL; uint32 id=0;
	CHECK(byte_and_cancel(&t,&retained)==0 && messages_and_quota(&t,&id)==0);
	xfuture* pending0=request(&t,0,XRUDP_OPERATION_MESSAGE_READ,id,NULL,0,256,0);
	xfuture* pending1=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,256,0);
	CHECK(pending0 && pending1 && stop(&t)==0);
	const xrudpoperationresult* v=value(&t,pending0); CHECK(v && (v->Result==XRUDP_CLOSED || v->Result==XRUDP_CANCELLED));
	v=value(&t,pending1); CHECK(v && (v->Result==XRUDP_CLOSED || v->Result==XRUDP_CANCELLED));
	xrtFutureDestroy(pending0); xrtFutureDestroy(pending1);
	v=value(&t,retained); CHECK(v && v->Size==17 && ((const unsigned char*)v->Data)[16]==16);
	xrudpoperation r; xrtRudpOperationInit(&r); CHECK(xrtRudpPeerSubmitAsync(t.Peer[0],&r)==NULL && xrtErrorKind(xrtGetError())==XERR_CLOSED);
	xrtFutureDestroy(retained); xrudpasyncstate state;
	CHECK(xrtRudpEndpointAsyncState(t.Endpoint[0],&state)==XRUDP_OK && !state.Retained && !state.RetainedBytes && !state.Pending);
	release(&t); return 0;
}
static int abort_order(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0);
	CHECK(xrtNetEnginePost(t.Engine,1,gate,&t) && bits(&t,&t.Gate,1));
	xfuture* read=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,64,1000);
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_ABORT; op.Reason=XRUDP_CANCELLED;
	t.AbortFuture=xrtRudpPeerSubmitAsync(t.Peer[1],&op); CHECK(read && t.AbortFuture);
	xrtAtomic32Store(&t.AbortArmed,1,XMEMORY_RELEASE); xrtAtomic32Store(&t.GateRelease,1,XMEMORY_RELEASE);
	const xrudpoperationresult* v=value(&t,t.AbortFuture); CHECK(v && v->Result==XRUDP_OK);
	v=value(&t,read); CHECK(v && (v->Result==XRUDP_CLOSED || v->Result==XRUDP_CANCELLED));
	CHECK(stop(&t)==0); xrtFutureDestroy(read); xrtFutureDestroy(t.AbortFuture); release(&t); return 0;
}
static int datagram_futures(async_test* t)
{
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_DATAGRAM_CONFIGURE;
	xrtRudpDatagramChannelInit(&op.ChannelConfig); op.ChannelConfig.Id=37;
	for (unsigned i=0;i<2;++i) {
		xfuture* f=xrtRudpPeerSubmitAsync(t->Peer[i],&op); const xrudpoperationresult* v=value(t,f);
		CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(f);
	}
	unsigned char bytes[300]; memset(bytes,83,sizeof(bytes));
	xfuture* small=request(t,0,XRUDP_OPERATION_DATAGRAM_READ,1,NULL,0,11,2000);
	xfuture* full=request(t,0,XRUDP_OPERATION_DATAGRAM_READ,1,NULL,0,sizeof(bytes),2000);
	op.Kind=XRUDP_OPERATION_DATAGRAM_WRITE; op.Channel=37; op.Data=bytes; op.Size=sizeof(bytes);
	xfuture* write=xrtRudpPeerSubmitAsync(t->Peer[1],&op); const xrudpoperationresult* v=value(t,write);
	CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(bytes)); uint64 sequence=v->Sequence; xrtFutureDestroy(write);
	v=value(t,small); CHECK(v && v->Result==XRUDP_TRUNCATED && v->Size==sizeof(bytes) && !v->Data && v->Datagram.Channel==37); xrtFutureDestroy(small);
	v=value(t,full); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(bytes) && v->Datagram.Sequence==sequence && !memcmp(v->Data,bytes,sizeof(bytes))); xrtFutureDestroy(full);
	op.Data=NULL; op.Size=0; write=xrtRudpPeerSubmitAsync(t->Peer[1],&op);
	full=request(t,0,XRUDP_OPERATION_DATAGRAM_READ,1,NULL,0,0,2000);
	CHECK(value(t,write) && value(t,write)->Result==XRUDP_OK && value(t,full) && value(t,full)->Result==XRUDP_OK && !value(t,full)->Size);
	xrtFutureDestroy(write); xrtFutureDestroy(full);
	unsigned char oversized[2048]={0}; op.Data=oversized; op.Size=sizeof(oversized); write=xrtRudpPeerSubmitAsync(t->Peer[1],&op);
	v=value(t,write); CHECK(v && v->Result==XRUDP_LIMIT && !v->Size && !v->Sequence); xrtFutureDestroy(write);
	full=request(t,0,XRUDP_OPERATION_DATAGRAM_READ,1,NULL,0,32,25); v=value(t,full);
	CHECK(v && v->Result==XRUDP_TIMEOUT && !v->Size && !v->Data); xrtFutureDestroy(full);
	CHECK(xrtNetEnginePost(t->Engine,1,gate,t) && bits(t,&t->Gate,1));
	op.Data=bytes; op.Size=sizeof(bytes); op.DatagramLifetimeUs=25000;
	write=xrtRudpPeerSubmitAsync(t->Peer[1],&op); CHECK(write); xrtSleep(50);
	xrtAtomic32Store(&t->GateRelease,1,XMEMORY_RELEASE); v=value(t,write);
	CHECK(v && v->Result==XRUDP_TIMEOUT && !v->Size && !v->Sequence); xrtFutureDestroy(write);
	op.Data="fresh"; op.Size=5; op.DatagramLifetimeUs=500000;
	write=xrtRudpPeerSubmitAsync(t->Peer[1],&op);
	full=request(t,0,XRUDP_OPERATION_DATAGRAM_READ,1,NULL,0,32,2000);
	CHECK(value(t,write) && value(t,write)->Result==XRUDP_OK); v=value(t,full);
	CHECK(v && v->Result==XRUDP_OK && v->Size==5 && !memcmp(v->Data,"fresh",5));
	xrtFutureDestroy(write); xrtFutureDestroy(full);
	return 0;
}
static int shutdown_fifo(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0);
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_STREAM_OPEN;
	xrtRudpStreamConfigInit(&op.StreamConfig); op.StreamConfig.Kind=XRUDP_STREAM_MESSAGES;
	xfuture* opened_future=xrtRudpPeerSubmitAsync(t.Peer[1],&op); const xrudpoperationresult* v=value(&t,opened_future);
	CHECK(v && v->Result==XRUDP_OK); uint32 id=v->Stream; xrtFutureDestroy(opened_future);
	CHECK(xrtNetEnginePost(t.Engine,1,gate,&t) && bits(&t,&t.Gate,1));
	unsigned char message[4096]; xfuture* writes[12];
	for (unsigned i=0;i<12;++i) { memset(message,(int)i,sizeof(message)); writes[i]=request(&t,1,XRUDP_OPERATION_MESSAGE_WRITE,id,message,sizeof(message),0,5000); CHECK(writes[i]); }
	op.Kind=XRUDP_OPERATION_SHUTDOWN; op.DrainTimeoutMs=5000;
	xfuture* shutdown=xrtRudpPeerSubmitAsync(t.Peer[1],&op); CHECK(shutdown);
	xfuture* late=request(&t,1,XRUDP_OPERATION_MESSAGE_WRITE,id,"late",4,0,5000);
	xfuture* done=request(&t,1,XRUDP_OPERATION_WAIT_WRITE_DONE,id,NULL,0,0,5000); CHECK(late && done);
	xrtAtomic32Store(&t.GateRelease,1,XMEMORY_RELEASE); xrtSleep(80);
	/* 48 KiB cannot be admitted into the 8 KiB transmit ring plus bounded
	 * unread receiver. Graceful shutdown must wait behind the queued writes. */
	CHECK(xrtFutureState(shutdown)==XFUTURE_PENDING && xrtFutureState(late)==XFUTURE_PENDING);
	for (unsigned i=0;i<12;++i) {
		xfuture* read=request(&t,0,XRUDP_OPERATION_MESSAGE_READ,id,NULL,0,sizeof(message),5000); v=value(&t,read);
		memset(message,(int)i,sizeof(message)); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(message) && !memcmp(v->Data,message,sizeof(message))); xrtFutureDestroy(read);
		v=value(&t,writes[i]); CHECK(v && v->Result==XRUDP_OK && v->Size==sizeof(message)); xrtFutureDestroy(writes[i]);
	}
	v=value(&t,shutdown); CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(shutdown);
	v=value(&t,late); CHECK(v && v->Result==XRUDP_CLOSED && !v->Size); xrtFutureDestroy(late);
	xfuture* eof=request(&t,0,XRUDP_OPERATION_MESSAGE_READ,id,NULL,0,sizeof(message),2000); v=value(&t,eof);
	CHECK(v && v->Result==XRUDP_DONE && !v->Size); xrtFutureDestroy(eof);
	v=value(&t,done); CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(done);
	CHECK(stop(&t)==0); release(&t); return 0;
}
static int negotiation_abort(xnetfamily family,bool native)
{
	async_test t; CHECK(start_mode(&t,family,native,true,false,false)==0 && xrtRudpPeerState(t.Peer[1])==XRUDP_PEER_NEGOTIATING);
	xfuture* read=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,8,35); const xrudpoperationresult* v=value(&t,read);
	CHECK(v && v->Result==XRUDP_TIMEOUT && xrtRudpPeerState(t.Peer[1])==XRUDP_PEER_NEGOTIATING); xrtFutureDestroy(read);
	read=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,8,0);
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_ABORT;
	xfuture* abort=xrtRudpPeerSubmitAsync(t.Peer[1],&op); CHECK(abort && xrtFutureWaitFor(abort,500)==XWAIT_OK);
	v=value(&t,abort); CHECK(v && v->Result==XRUDP_OK); v=value(&t,read); CHECK(v && v->Result==XRUDP_CANCELLED);
	CHECK(stop(&t)==0); xrtFutureDestroy(read); xrtFutureDestroy(abort); release(&t); return 0;
}
static int operation_faults(async_test* t)
{
	CHECK(xrtMemDebugFailAfter(1000)); xrtMemDebugFailClear();
	xmemdebugsnapshot before,after; xrtClearError(); xrtMemDebugSnapshot(&before);
	unsigned failures=0;
	for (unsigned budget=0;budget<18;++budget) {
		CHECK(xrtMemDebugFailAfter(budget));
		xfuture* f=request(t,1,XRUDP_OPERATION_READ,1,NULL,0,8,1);
		bool triggered=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (triggered) { CHECK(f==NULL && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_MEMORY); ++failures; }
		else { const xrudpoperationresult* v=value(t,f); CHECK(v && v->Result==XRUDP_TIMEOUT); xrtFutureDestroy(f); }
		xrtClearError(); double end=xrtTimer()+1; xrudpasyncstate state;
		do { CHECK(xrtRudpEndpointAsyncState(t->Endpoint[1],&state)==XRUDP_OK); if (!state.Retained && !state.Pending && !state.RetainedBytes) break; CHECK(xrtTimer()<end); xrtSleep(1); } while (true);
	}
	xrtMemDebugSnapshot(&after);
	CHECK(failures>=4 && after.DoubleFreeCount==before.DoubleFreeCount && after.InvalidFreeCount==before.InvalidFreeCount && after.UseAfterFreeCount==before.UseAfterFreeCount);
	return 0;
}
static int engine_stop_pending(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0 && datagram_futures(&t)==0 && operation_faults(&t)==0);
	xfuture* a=request(&t,0,XRUDP_OPERATION_READ,1,NULL,0,8,0);
	xfuture* b=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,8,0); CHECK(a && b);
	/* Engine rejects stopping live transports without changing their state.
	 * Verify that rejection preserves outstanding work and normal I/O. */
	CHECK(!xrtNetEngineStop(t.Engine) && xrtNetEngineState(t.Engine)==XNET_ENGINE_RUNNING && xrtErrorKind(xrtGetError())==XERR_STATE);
	xrtClearError(); CHECK(xrtFutureState(a)==XFUTURE_PENDING && xrtFutureState(b)==XFUTURE_PENDING);
	xfuture* write=request(&t,1,XRUDP_OPERATION_WRITE,1,"live",4,0,1000);
	CHECK(value(&t,write) && value(&t,write)->Result==XRUDP_OK && value(&t,a) && value(&t,a)->Size==4); xrtFutureDestroy(write);
	CHECK(stop(&t)==0 && value(&t,b)); xrtFutureDestroy(a); xrtFutureDestroy(b); release(&t); return 0;
}
static int disabled_adapter(xnetfamily family,bool native)
{
	async_test t; CHECK(start_mode(&t,family,native,false,true,false)==0);
	xrudpasyncstate state; CHECK(xrtRudpEndpointAsyncState(t.Endpoint[0],&state)==XRUDP_OK && !state.Limit && !state.ByteLimit && !state.Retained);
	CHECK(request(&t,0,XRUDP_OPERATION_READ,1,NULL,0,8,10)==NULL && xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
	xrtClearError(); CHECK(stop(&t)==0); release(&t); return 0;
}
static void close_oom(xnetworker* worker,ptr context)
{
	async_test* t=context; (void)worker;
	expect(t,xrtMemDebugFailAfter(0) && xrtRudpEndpointClose(t->Endpoint[1])==XRUDP_OK,"allocation-free asynchronous endpoint close");
}
static int close_pending_oom(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0); t.CloseOom=true;
	xfuture* read=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,32,0); CHECK(read);
	CHECK(xrtNetEnginePost(t.Engine,1,close_oom,&t));
	const xrudpoperationresult* v=value(&t,read); CHECK(v && v->Result==XRUDP_CANCELLED);
	CHECK(stop(&t)==0); xrtFutureDestroy(read); release(&t); return 0;
}
#define RACE_SENDERS 4u
#define RACE_MESSAGES 64u
#define RACE_BYTES 128u
typedef struct producer {
	async_test* Test; xrudppeer* Peer; uint32 Stream, Id;
	xatomic32* Done; xatomic32* Posted; xatomic32* Accepted;
	unsigned char Outcome[RACE_MESSAGES];
} producer;
static int32 produce(ptr context)
{
	producer* p=context; int32 result=1; xfuture* future=NULL;
	for (unsigned sequence=0;sequence<RACE_MESSAGES;++sequence) {
		unsigned char bytes[RACE_BYTES];
		for (unsigned i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)(7u*p->Id+sequence+i);
		bytes[0]=(unsigned char)p->Id; bytes[1]=(unsigned char)sequence;
		xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_MESSAGE_WRITE;
		op.Stream=p->Stream; op.Data=bytes; op.Size=sizeof(bytes); op.TimeoutMs=4000;
		future=xrtRudpPeerSubmitAsync(p->Peer,&op);
		if (!future) goto done;
		memset(bytes,0,sizeof(bytes));
		if (!(sequence&1u)) (void)xrtFutureCancel(future);
		if (!sequence) (void)xrtAtomic32FetchOr(p->Posted,1u<<p->Id,XMEMORY_RELEASE);
		if (xrtFutureWaitFor(future,5000)!=XWAIT_OK) goto done;
		if (xrtFutureState(future)==XFUTURE_CANCELLED) p->Outcome[sequence]=1;
		else {
			const xrudpoperationresult* v=xrtFutureValue(future);
			if (!v || v->Result!=XRUDP_OK || v->Size!=RACE_BYTES) goto done;
			p->Outcome[sequence]=2; (void)xrtAtomic32FetchAdd(p->Accepted,1,XMEMORY_RELEASE);
		}
		xrtFutureDestroy(future); future=NULL;
	}
	result=0;
done:
	xrtFutureDestroy(future); xrtRudpPeerDestroy(p->Peer); xrtClearError();
	if (result) expect(p->Test,false,"concurrent producer failed");
	(void)xrtAtomic32FetchOr(p->Done,1u<<p->Id,XMEMORY_RELEASE); return result;
}
static int concurrent_cancellation(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0);
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_STREAM_OPEN;
	xrtRudpStreamConfigInit(&op.StreamConfig); op.StreamConfig.Kind=XRUDP_STREAM_MESSAGES;
	xfuture* f=xrtRudpPeerSubmitAsync(t.Peer[1],&op); const xrudpoperationresult* v=value(&t,f);
	CHECK(v && v->Result==XRUDP_OK); uint32 id=v->Stream; xrtFutureDestroy(f);
	double end=xrtTimer()+3;
	while (xrtAtomic32Load(&t.RemoteStream,XMEMORY_ACQUIRE)!=id) { CHECK(xrtTimer()<end); xrtSleep(1); }
	CHECK(xrtNetEnginePost(t.Engine,1,gate,&t) && bits(&t,&t.Gate,1));
	producer senders[RACE_SENDERS]={0}; xthread* threads[RACE_SENDERS]={0};
	xatomic32 done,posted,accepted; xrtAtomic32Init(&done,0); xrtAtomic32Init(&posted,0); xrtAtomic32Init(&accepted,0);
	for (unsigned i=0;i<RACE_SENDERS;++i) {
		senders[i].Test=&t; senders[i].Peer=xrtRudpPeerRef(t.Peer[1]); senders[i].Stream=id; senders[i].Id=i;
		senders[i].Done=&done; senders[i].Posted=&posted; senders[i].Accepted=&accepted;
		threads[i]=xrtThreadCreate(produce,&senders[i],0); CHECK(threads[i]);
	}
	CHECK(bits(&t,&posted,(1u<<RACE_SENDERS)-1u)); xrtAtomic32Store(&t.GateRelease,1,XMEMORY_RELEASE);
	bool received[RACE_SENDERS][RACE_MESSAGES]={{false}}; unsigned count=0; end=xrtTimer()+12;
	while (xrtAtomic32Load(&done,XMEMORY_ACQUIRE)!=(1u<<RACE_SENDERS)-1u || count!=xrtAtomic32Load(&accepted,XMEMORY_ACQUIRE)) {
		CHECK(xrtTimer()<end);
		f=request(&t,0,XRUDP_OPERATION_MESSAGE_READ,id,NULL,0,RACE_BYTES,50); v=value(&t,f); CHECK(v);
		if (v->Result==XRUDP_OK) {
			const unsigned char* bytes=v->Data; CHECK(v->Size==RACE_BYTES && bytes[0]<RACE_SENDERS && bytes[1]<RACE_MESSAGES && !received[bytes[0]][bytes[1]]);
			for (unsigned i=2;i<RACE_BYTES;++i) CHECK(bytes[i]==(unsigned char)(7u*bytes[0]+bytes[1]+i));
			received[bytes[0]][bytes[1]]=true; ++count;
		} else CHECK(v->Result==XRUDP_TIMEOUT);
		xrtFutureDestroy(f);
	}
	unsigned cancelled=0;
	for (unsigned i=0;i<RACE_SENDERS;++i) {
		CHECK(xrtThreadWaitFor(threads[i],2000)==XWAIT_OK && xrtThreadExitCode(threads[i])==0); xrtThreadDestroy(threads[i]);
		for (unsigned j=0;j<RACE_MESSAGES;++j) {
			CHECK(senders[i].Outcome[j]==1 || senders[i].Outcome[j]==2);
			CHECK(received[i][j]==(senders[i].Outcome[j]==2)); if (senders[i].Outcome[j]==1) ++cancelled;
		}
	}
	CHECK(cancelled>=RACE_SENDERS && count>=RACE_SENDERS*RACE_MESSAGES/2);
	CHECK(stop(&t)==0); release(&t); return 0;
}
static int stream_controls(xnetfamily family,bool native)
{
	async_test t; CHECK(start(&t,family,native)==0);
	xrudpoperation op; xrtRudpOperationInit(&op); op.Kind=XRUDP_OPERATION_STREAM_OPEN; xrtRudpStreamConfigInit(&op.StreamConfig);
	xfuture* f=xrtRudpPeerSubmitAsync(t.Peer[1],&op); const xrudpoperationresult* v=value(&t,f);
	CHECK(v && v->Result==XRUDP_OK); uint32 id=v->Stream; xrtFutureDestroy(f); double end=xrtTimer()+3;
	while (xrtAtomic32Load(&t.RemoteStream,XMEMORY_ACQUIRE)!=id) { CHECK(xrtTimer()<end); xrtSleep(1); }
	xfuture* read=request(&t,0,XRUDP_OPERATION_READ,id,NULL,0,32,2000);
	op.Kind=XRUDP_OPERATION_STREAM_STOP; op.Stream=id; op.Reason=77;
	f=xrtRudpPeerSubmitAsync(t.Peer[0],&op); v=value(&t,f); CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(f);
	v=value(&t,read); CHECK(v && v->Result==XRUDP_CANCELLED); xrtFutureDestroy(read);
	for (unsigned side=0;side<2;++side) {
		op.Kind=XRUDP_OPERATION_STREAM_RESET; op.Reason=88;
		f=xrtRudpPeerSubmitAsync(t.Peer[side],&op); v=value(&t,f); CHECK(v && (v->Result==XRUDP_OK || v->Result==XRUDP_DONE)); xrtFutureDestroy(f);
	}
	read=request(&t,1,XRUDP_OPERATION_READ,id,NULL,0,32,2000); v=value(&t,read);
	CHECK(v && v->Result==XRUDP_CANCELLED); xrtFutureDestroy(read);
	for (unsigned side=0;side<2;++side) {
		op.Kind=XRUDP_OPERATION_STREAM_RETIRE; op.TimeoutMs=3000;
		f=xrtRudpPeerSubmitAsync(t.Peer[side],&op); v=value(&t,f); CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(f);
	}
	op.Kind=XRUDP_OPERATION_STREAM_OPEN;
	f=xrtRudpPeerSubmitAsync(t.Peer[1],&op); v=value(&t,f); CHECK(v && v->Result==XRUDP_OK && v->Stream>id); id=v->Stream; xrtFutureDestroy(f);
	end=xrtTimer()+3; while (xrtAtomic32Load(&t.RemoteStream,XMEMORY_ACQUIRE)!=id) { CHECK(xrtTimer()<end); xrtSleep(1); }
	xfuture* write=request(&t,1,XRUDP_OPERATION_WRITE,id,"finish",6,0,1000);
	f=request(&t,1,XRUDP_OPERATION_STREAM_FINISH,id,NULL,0,0,1000);
	CHECK(value(&t,write) && value(&t,write)->Result==XRUDP_OK && value(&t,f) && value(&t,f)->Result==XRUDP_OK);
	xrtFutureDestroy(write); xrtFutureDestroy(f);
	read=request(&t,0,XRUDP_OPERATION_READ,id,NULL,0,16,2000); v=value(&t,read);
	CHECK(v && v->Result==XRUDP_OK && v->Size==6 && !memcmp(v->Data,"finish",6)); xrtFutureDestroy(read);
	read=request(&t,0,XRUDP_OPERATION_READ,id,NULL,0,16,2000); v=value(&t,read); CHECK(v && v->Result==XRUDP_DONE); xrtFutureDestroy(read);
	f=request(&t,1,XRUDP_OPERATION_WAIT_WRITE_DONE,id,NULL,0,0,2000); v=value(&t,f); CHECK(v && v->Result==XRUDP_OK); xrtFutureDestroy(f);
	CHECK(stop(&t)==0); release(&t); return 0;
}
static int timer_failure_cause(xnetfamily family,bool native)
{
	async_test t; CHECK(start_mode(&t,family,native,false,false,true)==0); xrtSleep(100); t.TimerOom=true;
	CHECK(xrtNetEnginePost(t.Engine,1,gate,&t) && bits(&t,&t.Gate,1));
	xfuture* write=request(&t,1,XRUDP_OPERATION_WRITE,1,"x",1,0,0);
	xfuture* read=request(&t,1,XRUDP_OPERATION_READ,1,NULL,0,32,0); CHECK(write && read);
	xrtAtomic32Store(&t.GateRelease,1,XMEMORY_RELEASE);
	const xrudpoperationresult* v=value(&t,read);
	bool valid=v && v->Result==XRUDP_NO_MEMORY && v->Error && xrtErrorKind(v->Error)==XERR_MEMORY;
	CHECK(stop(&t)==0);
	if (!valid) {
		fprintf(stderr,"timer fault result=%u close=%u triggered=%u\n",v?(unsigned)v->Result:999u,(unsigned)t.TimerReason,t.TimerTriggered?1u:0u);
		const xerror* error=t.TimerError;
		for (unsigned depth=0;error && depth<8;++depth,error=xrtErrorCause(error)) fprintf(stderr,"timer cause kind=%u message=%s\n",(unsigned)xrtErrorKind(error),xrtErrorMessage(error));
	}
	CHECK(valid && xrtErrorKind(v->Error)==XERR_MEMORY && t.TimerTriggered && t.TimerReason==XRUDP_NO_MEMORY);
	CHECK(value(&t,write) && value(&t,write)->Result==XRUDP_OK && value(&t,write)->Size==1);
	xrtFutureDestroy(write); xrtFutureDestroy(read); release(&t); return 0;
}
int main(int argc,char** argv)
{
	xnetfamily family=argc>1 && atoi(argv[1])==6 ? XNET_FAMILY_IPV6:XNET_FAMILY_IPV4;
	bool native=argc>2 && atoi(argv[2])!=0;
	CHECK(xrtMemDebugFailAfter(1000)); xrtMemDebugFailClear(); CHECK(lifecycle(family,native)==0); xrtClearError();
	xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before); CHECK(before.Enabled);
	if (argc>3 && !strcmp(argv[3],"negotiation")) CHECK(negotiation_abort(family,native)==0);
	else CHECK(lifecycle(family,native)==0 && abort_order(family,native)==0 && shutdown_fifo(family,native)==0 && negotiation_abort(family,native)==0 && engine_stop_pending(family,native)==0 && disabled_adapter(family,native)==0 && close_pending_oom(family,native)==0 && concurrent_cancellation(family,native)==0 && stream_controls(family,native)==0 && timer_failure_cause(family,native)==0);
	xrtClearError(); xrtMemDebugSnapshot(&after);
	if (after.LiveCount!=before.LiveCount || after.LiveBytes!=before.LiveBytes)
		fprintf(stderr,"async logical allocation delta: count=%lld bytes=%lld\n",(long long)after.LiveCount-(long long)before.LiveCount,(long long)after.LiveBytes-(long long)before.LiveBytes);
	CHECK(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes && after.DoubleFreeCount==before.DoubleFreeCount && after.InvalidFreeCount==before.InvalidFreeCount && after.UseAfterFreeCount==before.UseAfterFreeCount);
	printf("xrudp async: family=%u native=%u lifecycle/bytes/messages/timeout/cancel/quota/abort-order/Engine-safe results passed\n",family==XNET_FAMILY_IPV6?6u:4u,native?1u:0u);
	return 0;
}
