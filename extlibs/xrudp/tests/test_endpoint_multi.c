#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_NATIVE_MULTI_MEMORY
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
#define MULTI_BYTES 8192u
#define MULTI_IDS 128u
#define MULTI_CYCLES 12u
typedef struct native_multi native_multi;
typedef struct multi_stream { size_t Written,Read; bool Active,Fin,Eof,Acked,Retired,EmptyQueued,EmptyRead; } multi_stream;
typedef struct multi_side {
	native_multi* Test; uint32_t Index,OwnOpened,LocalRetired,RemoteRetired,OpenHints,Changes,Closes,PeerCloses;
	bool InCallback,OpenBlocked; multi_stream Stream[MULTI_IDS];
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	uint32_t GameWritten,GameRead;bool GameStarted,GameSeen[48];
#endif
} multi_side;
struct native_multi {
	xnetengine* Engine;xrudpendpoint* Endpoint[2];xrudppeer* Peer[2];xrudpendpointinfo Identity[2];multi_side Side[2];
	xatomic32 Ready,Dialed,Confirmed,Complete,PeerClosed,Closed,Failure;
	unsigned char Private[2][32],Public[2][32];uint32_t Mode;xnetfamily Family;
};
static void require(multi_side* s,bool ok,const char* text)
{
	if(!ok) { fprintf(stderr,"native multi: side=%u mode=%u %s\n",s->Index,s->Test->Mode,text);xrtAtomic32Store(&s->Test->Failure,1,XMEMORY_RELEASE); }
}
static bool wait_for(native_multi* t,xatomic32* value,uint32_t bits)
{
	double deadline=xrtTimer()+10;
	while((xrtAtomic32Load(value,XMEMORY_ACQUIRE)&bits)!=bits) {
		if(xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE) || xrtTimer()>deadline)return false;
		xrtThreadYield();
	}
	return true;
}
static size_t expected(const multi_side* s) { return s->Test->Mode==3 || s->Test->Mode==5?128:MULTI_BYTES; }
static bool message_stream(uint32_t id)
{
#ifdef TEST_NATIVE_MULTI_MESSAGES
	return id!=1;
#else
	(void)id;return false;
#endif
}
static uint64_t wire_size(const multi_side* s,uint32_t id)
{
	uint64_t size=expected(s);return message_stream(id)?size+4+4*((size+255)/256):size;
}
static unsigned char pattern(uint32_t side,uint32_t id,size_t offset) { return (unsigned char)((side*79+id*23+offset*37)^(offset>>6)); }
static bool cancelled(multi_side* s,xrudpresult r)
{
	return r==XRUDP_CLOSED && xrtRudpEndpointState(s->Test->Endpoint[s->Index])!=XRUDP_ENDPOINT_OPEN;
}
static bool native_games(void)
{
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	return true;
#else
	return false;
#endif
}
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
static void write_games(multi_side* s,xrudppeer* p)
{
	while(s->GameWritten<48) {
		unsigned char bytes[32];uint32_t serial=s->GameWritten;for(size_t j=0;j<4;++j)bytes[3-j]=(unsigned char)(serial>>(j*8));
		for(size_t j=4;j<sizeof(bytes);++j)bytes[j]=pattern(s->Index,9,serial*32+j);
		uint64_t seq=999;xrudpresult r=xrtRudpPeerDatagramWrite(p,9,bytes,serial?sizeof(bytes):0,0,&seq);
		if(cancelled(s,r))return;
		require(s,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"game write failed");if(r!=XRUDP_OK)return;
		require(s,seq==0,"independent game sequence was nonzero");++s->GameWritten;
	}
}
static void start_games(xnetworker* worker,ptr context)
{
	multi_side* s=(multi_side*)context;(void)worker;s->GameStarted=true;
#ifdef TEST_NATIVE_DG_TIMEOUT
	if(s->Test->Mode==6 && s->Index==1) {
		xrudppeer* p=s->Test->Peer[1];p->Session->IdleUs=1;p->Session->Activity=0;xrudpdatagraminfo info;
		require(s,xrtRudpPeerDatagramRead(p,NULL,0,&info)==XRUDP_TIMEOUT && xrtRudpPeerState(p)==XRUDP_PEER_CLOSED && p->Cipher==NULL,"timed API retained an invalid cipher alias");
		require(s,xrtRudpPeerAbort(p)==XRUDP_DONE,"abort after timed API close touched retired session");return;
	}
	if(s->Test->Mode==6)return;
#endif
	write_games(s,s->Test->Peer[s->Index]);
}
#endif
static void write_more(multi_side* s,xrudppeer* p,uint32_t id)
{
	multi_stream* m=&s->Stream[id];if(m->Fin)return;
	if(message_stream(id) && !m->EmptyQueued) {
		xrudpresult r=xrtRudpPeerMessageWrite(p,id,NULL,0);if(cancelled(s,r))return;
		require(s,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"empty message write failed");if(r!=XRUDP_OK)return;m->EmptyQueued=true;
	}
	unsigned char data[256];size_t size=expected(s)-m->Written;if(size>sizeof(data))size=sizeof(data);
	for(size_t i=0;i<size;++i)data[i]=pattern(s->Index,id,m->Written+i);
	size_t written;xrudpresult r;
	if(message_stream(id)) { r=xrtRudpPeerMessageWrite(p,id,data,size);written=r==XRUDP_OK?size:0; }
	else r=xrtRudpPeerStreamWrite(p,id,data,size,&written);
	if(cancelled(s,r))return;
	require(s,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"write failed");m->Written+=written;
	if(m->Written==expected(s)) {
		r=xrtRudpPeerStreamFinish(p,id);if(cancelled(s,r))return;require(s,r==XRUDP_OK,"FIN rejected");m->Fin=true;
	}
}
static void open_more(multi_side* s,xrudppeer* p)
{
	uint32_t maximum=s->Test->Mode==3?MULTI_CYCLES:(s->Test->Mode==5?63:2);
	while(s->OwnOpened<maximum) {
		xrudpstreamconfig cfg;xrtRudpStreamConfigInit(&cfg);cfg.Weight=(uint16_t)(1+s->OwnOpened%4);
#ifdef TEST_NATIVE_MULTI_MESSAGES
		cfg.Kind=XRUDP_STREAM_MESSAGES;
#endif
		uint32_t id=999;xrudpresult r=xrtRudpPeerStreamOpen(p,&cfg,&id);
		if(r==XRUDP_BACKPRESSURE) { require(s,id==999,"failed Open changed output");s->OpenBlocked=true;return; }
		if(cancelled(s,r))return;
		require(s,r==XRUDP_OK && id==2+s->Index+s->OwnOpened*2 && id<MULTI_IDS,"wrong role/stream ID");
		if(r!=XRUDP_OK || id>=MULTI_IDS)return;
		++s->OwnOpened;s->OpenBlocked=false;s->Stream[id].Active=true;write_more(s,p,id);
		if(s->Test->Mode==4 && s->Index==1 && id==3) {
			r=xrtRudpPeerStreamReset(p,id,77);if(cancelled(s,r))return;require(s,r==XRUDP_OK,"local RESET rejected");
			s->Stream[id].Fin=true;xrudpstreamstate state;
			require(s,xrtRudpPeerStreamState(p,id,&state)==XRUDP_OK && state.ResetFinal==0 && state.Transmit.Cancelled,"unsent RESET charged credit");
		}
	}
}
static void completion(multi_side* s)
{
	bool done=true;
	if(s->Test->Mode==3)done=s->LocalRetired==MULTI_CYCLES && s->RemoteRetired==MULTI_CYCLES;
	else for(uint32_t id=1;id<=(s->Test->Mode==5?127u:5u);++id) {
		multi_stream* m=&s->Stream[id];if(!m->Active || !m->Eof || !m->Acked || (id!=1 && !m->Retired))done=false;
	}
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	done=done && s->GameStarted && s->GameWritten==48 && s->GameRead==48;
#endif
	if(done)(void)xrtAtomic32FetchOr(&s->Test->Complete,1u<<s->Index,XMEMORY_RELEASE);
}
static void opened(xrudpendpoint* e,void* context)
{
	multi_side* s=(multi_side*)context;require(s,xrtRudpEndpointInfo(e,&s->Test->Identity[s->Index])==XRUDP_OK,"bound identity missing");
	(void)xrtAtomic32FetchOr(&s->Test->Ready,1u<<s->Index,XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
	multi_side* s=(multi_side*)context;(void)e;
	xrudphandshakeinfo info;xnetaddr address;require(s,xrtRudpPeerInfo(p,&info,&address)==XRUDP_OK &&
		info.Role==(s->Index?XRUDP_INITIATOR:XRUDP_RESPONDER),"role not authenticated");
	if(s->Index==0)s->Test->Peer[0]=xrtRudpPeerRef(p);
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	xrudpdatagramchannel channel;xrtRudpDatagramChannelInit(&channel);channel.Id=9;
	require(s,xrtRudpPeerDatagramConfigure(p,&channel)==XRUDP_OK,"native game configure failed");
#endif
	s->Stream[1].Active=true;if(s->Test->Mode!=3)write_more(s,p,1);open_more(s,p);
	(void)xrtAtomic32FetchOr(&s->Test->Confirmed,1u<<s->Index,XMEMORY_RELEASE);
}
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
	multi_side* s=(multi_side*)context;(void)e;require(s,!s->InCallback,"recursive global hint");
	if(events&XRUDP_PEER_STREAMS_READY) { ++s->OpenHints;if(s->OpenBlocked)open_more(s,p); }
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	s->InCallback=true;
	if(s->GameStarted && (events&XRUDP_PEER_DATAGRAM_WRITE_READY))write_games(s,p);
	if(events&XRUDP_PEER_DATAGRAM_READ_READY) {
		unsigned char bytes[32];xrudpdatagraminfo info;xrudpresult r=xrtRudpPeerDatagramRead(p,bytes,sizeof(bytes),&info);
		if(cancelled(s,r)) { s->InCallback=false;return; }
		require(s,r==XRUDP_OK || r==XRUDP_BACKPRESSURE,"native game read failed");
		if(r==XRUDP_OK) {
			uint32_t serial=0;if(info.Size)for(size_t j=0;j<4;++j)serial=(serial<<8)|bytes[j];
			require(s,info.Channel==9 && !info.Sequenced && info.Sequence==0 && serial<48 && !s->GameSeen[serial] && info.Size==(serial?32u:0u),"native game duplicate/invalid identity");
			if(serial<48) { s->GameSeen[serial]=true;++s->GameRead; }
			for(size_t j=4;j<info.Size;++j)require(s,bytes[j]==pattern(1-s->Index,9,serial*32+j),"native game payload corruption");
			if(s->Index==1 && (s->Test->Mode==1 || s->Test->Mode==2)) {
				if(s->Test->Mode==1) { xrtRudpPeerDestroy(s->Test->Peer[1]);s->Test->Peer[1]=NULL;require(s,xrtRudpPeerAbort(p)==XRUDP_OK,"game callback peer abort failed"); }
				else require(s,xrtRudpEndpointClose(e)==XRUDP_OK,"game callback endpoint close failed");
				s->InCallback=false;return;
			}
		}
	}
	completion(s);s->InCallback=false;
#endif
}
static void stream_changed(xrudpendpoint* e,xrudppeer* p,const xrudpstreamevent* event,void* context)
{
	multi_side* s=(multi_side*)context;native_multi* t=s->Test;uint32_t id=event->Id;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return;
	require(s,!s->InCallback && xrtRudpPeerState(p)==XRUDP_PEER_CONFIRMED && id>0 && id<MULTI_IDS,"recursive/premature stream callback");
	if(id==0 || id>=MULTI_IDS)return;
	s->InCallback=true;++s->Changes;multi_stream* m=&s->Stream[id];
	if(event->Events&XRUDP_STREAM_OPENED) {
		xrudpstreamstate state;xrudpresult r=xrtRudpPeerStreamState(p,id,&state);if(cancelled(s,r)) { s->InCallback=false;return; }
			require(s,!m->Active && r==XRUDP_OK && !state.Local && state.Kind==(message_stream(id)?XRUDP_STREAM_MESSAGES:XRUDP_STREAM_BYTES),"duplicate/local/wrong-kind OPEN hint");
		m->Active=true;write_more(s,p,id);
	}
	require(s,m->Active && !m->Retired,"hint after retirement");
	if(event->Events&(XRUDP_STREAM_READ_READY|XRUDP_STREAM_READ_EOF)) {
		for(size_t step=0;step<16;++step) {
			unsigned char bytes[256];size_t count;xrudpresult r;
			if(message_stream(id)) {
				size_t available;xrudpresult peek=xrtRudpPeerMessagePeek(p,id,&available);
				if(peek==XRUDP_OK && available) {
					size_t needed;require(s,xrtRudpPeerMessageRead(p,id,NULL,0,&needed)==XRUDP_TRUNCATED && needed==available,"short message read consumed data");
				}
				r=xrtRudpPeerMessageRead(p,id,bytes,sizeof(bytes),&count);
			} else r=xrtRudpPeerStreamRead(p,id,bytes,73,&count);
			if(cancelled(s,r))break;
			if(t->Mode==4 && s->Index==0 && id==3 && r==XRUDP_CANCELLED) { require(s,m->Read==0 && count==0,"RESET exposed discarded plaintext");m->Eof=true;break; }
			require(s,r==XRUDP_OK || r==XRUDP_DONE || r==XRUDP_BACKPRESSURE,"read failed");
			if(r!=XRUDP_OK && r!=XRUDP_DONE && r!=XRUDP_BACKPRESSURE)break;
			require(s,m->Read+count<=expected(s),"duplicate/excess bytes");
			if(r==XRUDP_OK && message_stream(id) && count==0) { require(s,!m->EmptyRead && m->Read==0,"duplicate/reordered empty message");m->EmptyRead=true; }
			for(size_t i=0;i<count;++i)require(s,bytes[i]==pattern(1-s->Index,id,m->Read+i),"byte corruption/order mismatch");
			m->Read+=count;
			if(!native_games() && (t->Mode==1 || t->Mode==2) && s->Index==1 && id!=1 && count) {
				if(t->Mode==1) { xrtRudpPeerDestroy(t->Peer[1]);t->Peer[1]=NULL;require(s,xrtRudpPeerAbort(p)==XRUDP_OK,"callback peer abort failed"); }
				else require(s,xrtRudpEndpointClose(e)==XRUDP_OK,"callback endpoint close failed");
				s->InCallback=false;return;
			}
			if(r==XRUDP_DONE) { require(s,m->Read==expected(s) && (!message_stream(id) || m->EmptyRead),"early EOF");m->Eof=true;break; }
			if(r==XRUDP_BACKPRESSURE)break;
			if(!message_stream(id)) { require(s,count!=0,"empty successful read would spin");if(!count)break; }
		}
	}
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN) { s->InCallback=false;return; }
	if(event->Events&XRUDP_STREAM_WRITE_READY)write_more(s,p,id);
	if(event->Events&XRUDP_STREAM_WRITE_DONE) {
		xrudpstreamstate state;xrudpresult r=xrtRudpPeerStreamState(p,id,&state);if(cancelled(s,r)) { s->InCallback=false;return; }
		require(s,r==XRUDP_OK && state.WriteDone && state.Transmit.PendingBytes==0 &&
			(state.Transmit.FinAcknowledged || (state.ResetQueued && state.ResetAcknowledged)),"write completed before ACK");m->Acked=true;
	}
	if(event->Events&XRUDP_STREAM_RETIRE_READY) {
		xrudpstreamstate state;xrudpresult r=xrtRudpPeerStreamState(p,id,&state);if(cancelled(s,r)) { s->InCallback=false;return; }
		require(s,r==XRUDP_OK,"retirement state missing");
		if(state.RetireReady) {
			require(s,id!=1 && m->Acked && m->Eof && !state.References,"premature retirement hint");
			r=xrtRudpPeerStreamRetire(p,id);if(cancelled(s,r)) { s->InCallback=false;return; }
			require(s,r==XRUDP_OK,"retirement failed");m->Retired=true;
			if(state.Local)++s->LocalRetired;else ++s->RemoteRetired;
			require(s,xrtRudpPeerStreamState(p,id,&state)==XRUDP_CLOSED,"retired stream still exposed");
			if(t->Mode==3)open_more(s,p);
		}
	}
	completion(s);s->InCallback=false;
}
static void peer_closed(xrudpendpoint* e,xrudppeer* p,xrudpresult reason,void* context)
{
	multi_side* s=(multi_side*)context;(void)e;xrudpsessionstate state;
	require(s,++s->PeerCloses==1 && xrtRudpPeerState(p)==XRUDP_PEER_CLOSED,"duplicate/premature peer close");
	require(s,xrtRudpPeerSessionState(p,&state)==XRUDP_OK && state.Reason!=XRUDP_OK && !state.Sent.PendingPackets && !state.Sent.BytesInFlight,
		"terminal snapshot not drained");
	require(s,reason==XRUDP_CANCELLED || reason==XRUDP_CLOSED || (s->Test->Mode==6 && reason==XRUDP_TIMEOUT),"unexpected peer failure");
	(void)xrtAtomic32FetchOr(&s->Test->PeerClosed,1u<<s->Index,XMEMORY_RELEASE);
}
static void closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{
	multi_side* s=(multi_side*)context;(void)e;(void)error;
	require(s,!s->InCallback && ++s->Closes==1 && reason==XRUDP_CANCELLED,"endpoint close raced stream callback");
	(void)xrtAtomic32FetchOr(&s->Test->Closed,1u<<s->Index,XMEMORY_RELEASE);
}
static void bind_one(xnetworker* worker,ptr context)
{
	multi_side* s=(multi_side*)context;native_multi* t=s->Test;xrudpendpointconfig cfg;xrtRudpEndpointConfigInit(&cfg);
	cfg.StaticPrivate=t->Private[s->Index];cfg.PeerPublic=t->Public[1-s->Index];cfg.Listen=s->Index==0;
	cfg.Session.StreamCapacity=cfg.Session.Local.MaxStreams=t->Mode==3?2:(t->Mode==5?64:3);
	cfg.Session.ReceiveCapacity=cfg.Session.TransmitCapacity=256;cfg.Session.Local.StreamCredit=256;cfg.Session.Local.ConnectionCredit=768;
#ifdef TEST_NATIVE_MULTI_MESSAGES
	cfg.Session.TransmitCapacity=512;cfg.Session.Local.MaxMessageSize=256;cfg.Session.Local.StreamCredit=64;cfg.Session.Local.ConnectionCredit=512;
#endif
	cfg.Session.Local.DatagramSize=512;cfg.Session.Local.MaxAckDelayUs=1000;cfg.Session.InitialRttUs=10000;cfg.Session.KeyUpdatePackets=16;
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	cfg.Session.Datagrams.Channels=1;cfg.Session.Datagrams.SendCapacity=8;cfg.Session.Datagrams.ReceiveCapacity=64;cfg.Session.Datagrams.PayloadCapacity=424;
#endif
	xnetaddr address;require(s,xrtNetAddrLoopback(&address,t->Family,0),"loopback failed");
	xrudpendpointevents events={opened,confirmed,peer_closed,closed,changed,stream_changed};
	require(s,xrtRudpEndpointBind(worker,&address,&cfg,&events,s,&t->Endpoint[s->Index])==XRUDP_OK,"bind failed");
}
static void dial(xnetworker* worker,ptr context)
{
	native_multi* t=(native_multi*)context;(void)worker;multi_side* s=&t->Side[1];
	xrudpconnectconfig cfg={t->Identity[0].Public,t->Identity[0].Scope,NULL};
	require(s,xrtRudpEndpointConnect(t->Endpoint[1],&t->Identity[0].Local,&cfg,&t->Peer[1])==XRUDP_OK,"dial failed");
	xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);uint32_t id=999;xrudpstreamstate state;
	require(s,xrtRudpPeerStreamOpen(t->Peer[1],&stream,&id)==XRUDP_BACKPRESSURE && id==999,"pre-confirmation Open accepted");
	unsigned char byte;size_t count=99;
	require(s,xrtRudpPeerStreamRead(t->Peer[1],1,&byte,1,&count)==XRUDP_BACKPRESSURE && count==0,"pre-confirmation read accepted");
	require(s,xrtRudpPeerStreamWrite(t->Peer[1],1,&byte,1,&count)==XRUDP_BACKPRESSURE && count==0,"pre-confirmation write accepted");
	require(s,xrtRudpPeerStreamState(t->Peer[1],1,&state)==XRUDP_BACKPRESSURE,"pre-confirmation state exposed");
	require(s,xrtRudpPeerStreamFinish(t->Peer[1],1)==XRUDP_BACKPRESSURE,"pre-confirmation FIN accepted");
	require(s,xrtRudpPeerStreamReset(t->Peer[1],1,1)==XRUDP_BACKPRESSURE,"pre-confirmation RESET accepted");
	require(s,xrtRudpPeerStreamRetire(t->Peer[1],2)==XRUDP_BACKPRESSURE,"pre-confirmation retirement accepted");
	xrudpdatagraminfo info;uint64_t seq=999;xrudpdatagramchannel ch;xrtRudpDatagramChannelInit(&ch);ch.Id=9;
	require(s,xrtRudpPeerDatagramWrite(t->Peer[1],9,NULL,0,0,&seq)==XRUDP_BACKPRESSURE && seq==999,"pre-confirmation game accepted");
	require(s,xrtRudpPeerDatagramConfigure(t->Peer[1],&ch)==XRUDP_BACKPRESSURE,"pre-confirmation game channel accepted");
	require(s,xrtRudpPeerDatagramRead(t->Peer[1],NULL,0,&info)==XRUDP_BACKPRESSURE && info.Size==0,"pre-confirmation game read accepted");
	(void)xrtAtomic32FetchOr(&t->Dialed,1,XMEMORY_RELEASE);
}
static int run(xnetfamily family,uint32_t mode)
{
	native_multi t;memset(&t,0,sizeof(t));t.Family=family;t.Mode=mode;
	xrtAtomic32Init(&t.Ready,0);xrtAtomic32Init(&t.Dialed,0);xrtAtomic32Init(&t.Confirmed,0);xrtAtomic32Init(&t.Complete,0);
	xrtAtomic32Init(&t.PeerClosed,0);xrtAtomic32Init(&t.Closed,0);xrtAtomic32Init(&t.Failure,0);
	for(size_t i=0;i<2;++i) {
		t.Side[i].Test=&t;t.Side[i].Index=(uint32_t)i;
		for(size_t j=0;j<32;++j)t.Private[i][j]=(unsigned char)(j+13+i*79);
		CHECK(xrtX25519Public(t.Private[i],t.Public[i]));
	}
	xnetengineconfig cfg;xrtNetEngineConfigInit(&cfg);cfg.Workers=2;cfg.Backend=TEST_RUDP_BACKEND;
	t.Engine=xrtNetEngineCreate(&cfg);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
	for(size_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,bind_one,&t.Side[i]));
	CHECK(wait_for(&t,&t.Ready,3));CHECK(xrtNetEnginePost(t.Engine,1,dial,&t));CHECK(wait_for(&t,&t.Dialed,1));
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
	CHECK(wait_for(&t,&t.Confirmed,3));for(size_t i=0;i<2;++i)CHECK(xrtNetEnginePost(t.Engine,i,start_games,&t.Side[i]));
#endif
	if(mode==0 || mode==3 || mode==4 || mode==5) {
		CHECK(wait_for(&t,&t.Complete,3));
		xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);uint32_t id=999;xrudpstreamstate state;unsigned char byte;size_t count=99;
		CHECK(xrtRudpPeerStreamOpen(t.Peer[0],&stream,&id)==XRUDP_ARGUMENT && id==999);
		CHECK(xrtRudpPeerStreamState(t.Peer[0],1,&state)==XRUDP_ARGUMENT);
		CHECK(xrtRudpPeerStreamRead(t.Peer[0],1,&byte,1,&count)==XRUDP_ARGUMENT && count==0);
		CHECK(xrtRudpPeerStreamWrite(t.Peer[0],1,&byte,1,&count)==XRUDP_ARGUMENT && count==0);
		CHECK(xrtRudpPeerStreamFinish(t.Peer[0],1)==XRUDP_ARGUMENT && xrtRudpPeerStreamReset(t.Peer[0],1,1)==XRUDP_ARGUMENT);
		CHECK(xrtRudpPeerStreamRetire(t.Peer[0],2)==XRUDP_ARGUMENT);
		CHECK(xrtRudpPeerMessageWrite(t.Peer[0],2,NULL,0)==XRUDP_ARGUMENT);
		CHECK(xrtRudpPeerMessagePeek(t.Peer[0],2,&count)==XRUDP_ARGUMENT && count==0);
		CHECK(xrtRudpPeerMessageRead(t.Peer[0],2,NULL,0,&count)==XRUDP_ARGUMENT && count==0);
		xrudpdatagraminfo info;uint64_t seq=999;
		CHECK(xrtRudpPeerDatagramRead(t.Peer[0],NULL,0,&info)==XRUDP_ARGUMENT && info.Size==0);
		CHECK(xrtRudpPeerDatagramWrite(t.Peer[0],9,NULL,0,0,&seq)==XRUDP_ARGUMENT && seq==999);
	} else if(mode==1)CHECK(wait_for(&t,&t.PeerClosed,3));
	else if(mode==6)CHECK(wait_for(&t,&t.PeerClosed,2));
	else CHECK(wait_for(&t,&t.Closed,2));
	for(size_t i=0;i<2;++i)CHECK(xrtRudpEndpointClose(t.Endpoint[i])==XRUDP_OK);
	CHECK(wait_for(&t,&t.Closed,3) && wait_for(&t,&t.PeerClosed,3));CHECK(xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
	CHECK(!xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
	for(size_t i=0;i<2;++i) {
		if(t.Peer[i]) {
			xrudpsessionstate session;CHECK(xrtRudpPeerSessionState(t.Peer[i],&session)==XRUDP_OK && session.Reason!=XRUDP_OK);
			uint64_t all=wire_size(&t.Side[i],1)+4*wire_size(&t.Side[i],2);
			if(mode==0)CHECK(session.KeyUpdates>0 && session.SentConnection==all && session.ReceivedConnection==all);
			if(mode==4)CHECK(session.SentConnection==all-(i?wire_size(&t.Side[i],3):0) && session.ReceivedConnection==all-(i?0:wire_size(&t.Side[i],3)));
			if(mode==5)CHECK(session.SentConnection==wire_size(&t.Side[i],1)+126*wire_size(&t.Side[i],2) && session.ReceivedConnection==session.SentConnection);
			if(mode==3)CHECK(t.Side[i].OpenHints>0 && session.SentConnection==MULTI_CYCLES*wire_size(&t.Side[i],2)*2 && session.ReceivedConnection==session.SentConnection);
			xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);uint32_t id=999;xrudpstreamstate state;size_t count=99;unsigned char byte;
			CHECK(xrtRudpPeerStreamOpen(t.Peer[i],&stream,&id)==XRUDP_CLOSED && id==999);
			CHECK(xrtRudpPeerStreamState(t.Peer[i],1,&state)==XRUDP_CLOSED);
			CHECK(xrtRudpPeerStreamRead(t.Peer[i],1,&byte,1,&count)==XRUDP_CLOSED && count==0);
			CHECK(xrtRudpPeerStreamWrite(t.Peer[i],1,&byte,1,&count)==XRUDP_CLOSED && count==0);
			CHECK(xrtRudpPeerStreamFinish(t.Peer[i],1)==XRUDP_CLOSED && xrtRudpPeerStreamReset(t.Peer[i],1,1)==XRUDP_CLOSED);
			CHECK(xrtRudpPeerStreamRetire(t.Peer[i],2)==XRUDP_CLOSED);
			CHECK(xrtRudpPeerMessageWrite(t.Peer[i],2,NULL,0)==XRUDP_CLOSED);
			CHECK(xrtRudpPeerMessagePeek(t.Peer[i],2,&count)==XRUDP_CLOSED && count==0);
			CHECK(xrtRudpPeerMessageRead(t.Peer[i],2,NULL,0,&count)==XRUDP_CLOSED && count==0);
			xrudpdatagraminfo info;uint64_t seq=999;CHECK(xrtRudpPeerDatagramRead(t.Peer[i],NULL,0,&info)==XRUDP_CLOSED && info.Size==0);
			CHECK(xrtRudpPeerDatagramWrite(t.Peer[i],9,NULL,0,0,&seq)==XRUDP_CLOSED && seq==999);
#ifdef TEST_NATIVE_MULTI_DATAGRAMS
			if(mode==0 || mode==3 || mode==4 || mode==5)CHECK(session.Datagrams.Sent==48 && session.Datagrams.Accepted==48 && session.Datagrams.Read==48 && session.Datagrams.Stopped && !session.Datagrams.SendQueued && !session.Datagrams.ReceiveQueued);
#endif
		}
		xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.Endpoint[i]);
	}
	printf("xrudp native multistream: backend=%d family=%d mode=%u passed\n",(int)TEST_RUDP_BACKEND,(int)family,mode);return 0;
}
int main(void)
{
#ifdef TEST_NATIVE_MULTI_MEMORY
	CHECK(run(XNET_FAMILY_IPV4,0)==0);xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
#endif
	CHECK(run(XNET_FAMILY_IPV4,0)==0);CHECK(run(XNET_FAMILY_IPV6,0)==0);
	CHECK(run(XNET_FAMILY_IPV4,1)==0);CHECK(run(XNET_FAMILY_IPV4,2)==0);CHECK(run(XNET_FAMILY_IPV4,3)==0);
	CHECK(run(XNET_FAMILY_IPV4,4)==0);
	CHECK(run(XNET_FAMILY_IPV4,5)==0);
#ifdef TEST_NATIVE_MULTI_MEMORY
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
	CHECK(before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp native multistream: all endpoint/stream/closed-handle lifetimes returned to baseline");
#endif
	return 0;
}
