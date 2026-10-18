#include <xrudp.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_MULTI_MEMORY
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
typedef struct raw_packet { unsigned char Data[1200]; size_t Size; } raw_packet;
static struct {
	xrudpcipher* Raw; xrudpsession* Session;
	raw_packet Packet[8]; size_t Count; uint64_t Now, Forbidden, Wire[2];
	xrudpackset Received; bool FalseAck, Confirmed; uint64_t ResetFinal; uint32_t ResetId, StreamsLimit;
} raw;
static void put32(unsigned char* p,uint32_t v) { for(size_t i=0;i<4;++i)p[3-i]=(unsigned char)(v>>(i*8)); }
static void put64(unsigned char* p,uint64_t v) { for(size_t i=0;i<8;++i)p[7-i]=(unsigned char)(v>>(i*8)); }
static uint32_t get32(const unsigned char* p) { uint32_t v=0;for(size_t i=0;i<4;++i)v=(v<<8)|p[i];return v; }
static uint64_t get64(const unsigned char* p) { uint64_t v=0;for(size_t i=0;i<8;++i)v=(v<<8)|p[i];return v; }
static xrudpresult capture(const void* data,size_t size,void* context)
{
	(void)context;if(raw.Count==8)return XRUDP_IO_ERROR;
	raw_packet* p=&raw.Packet[raw.Count++];p->Size=size;memcpy(p->Data,data,size);return XRUDP_OK;
}
static int pair(xrudpcipher* cipher[2])
{
	unsigned char key[2][32]={{3},{11}},id[2][16]={{7},{19}},packet[2][100],plain[100];size_t size[2],read;
	unsigned char proof[12]={XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	for(size_t i=0;i<2;++i) {
		CHECK(xrtRudpCipherCreate(key[i],key[1-i],id[i],id[1-i],0,&cipher[i])==XRUDP_OK);
		CHECK(xrtRudpCipherSeal(cipher[i],proof,12,packet[i],100,&size[i])==XRUDP_OK);
	}
	for(size_t i=0;i<2;++i)CHECK(xrtRudpCipherOpen(cipher[i],packet[1-i],size[1-i],plain,100,&read)==XRUDP_OK);
	return 0;
}
static void configure(xrudpsessionconfig* c)
{
	xrtRudpSessionConfigInit(c);c->Role=XRUDP_RESPONDER;c->StreamCapacity=3;c->Local.MaxStreams=c->Peer.MaxStreams=3;
	c->ReceiveCapacity=c->TransmitCapacity=256;c->Local.StreamCredit=c->Peer.StreamCredit=256;
	c->Local.ConnectionCredit=c->Peer.ConnectionCredit=1024;c->PacketCapacity=32;c->InitialRttUs=10000;
	c->Local.MaxAckDelayUs=c->Peer.MaxAckDelayUs=1000;c->ControlPerSecond=4096;
}
static int start(const xrudpsessionconfig* c)
{
	memset(&raw,0,sizeof(raw));xrudpcipher* cipher[2]={NULL,NULL};CHECK(pair(cipher)==0);
	raw.Raw=cipher[0];xrtRudpAckSetInit(&raw.Received);
	CHECK(xrtRudpSessionCreate(c,cipher[1],0,capture,NULL,&raw.Session)==XRUDP_OK);return 0;
}
static void stop(void) { xrtRudpSessionDestroy(raw.Session);xrtRudpCipherDestroy(raw.Raw);memset(&raw,0,sizeof(raw)); }
static int append(unsigned char* p,size_t* used,xrudpframetype type,uint8_t flags,const void* data,size_t size)
{
	size_t n;CHECK(xrtRudpFrameAppend(p+*used,1136-*used,&n,type,flags,data,size)==XRUDP_OK);*used+=n;return 0;
}
static int open_frame(unsigned char* p,size_t* used,uint32_t id,uint16_t weight)
{
	unsigned char d[16];put32(d,id);put64(d+4,256);d[12]=0;d[13]=1;d[14]=(unsigned char)(weight>>8);d[15]=(unsigned char)weight;
	return append(p,used,XRUDP_FRAME_OPEN,0,d,16);
}
static int stream_frame(unsigned char* p,size_t* used,uint32_t id,uint64_t offset,bool fin)
{
	unsigned char d[13];put32(d,id);put64(d+4,offset);d[12]=42;
	return append(p,used,XRUDP_FRAME_STREAM,fin?XRUDP_STREAM_FIN:0,d,fin?12:13);
}
static int ack_frame(unsigned char* p,size_t* used,uint64_t number)
{
	xrudpackset set;xrtRudpAckSetInit(&set);CHECK(xrtRudpAckSetRecord(&set,number,raw.Now)==XRUDP_OK);
	size_t n;CHECK(xrtRudpAckSetEncode(&set,raw.Now,p+*used,1136-*used,&n)==XRUDP_OK);*used+=n;return 0;
}
static xrudpresult deliver(const void* plain,size_t used)
{
	unsigned char packet[1200];size_t size;
	if(xrtRudpCipherSeal(raw.Raw,plain,used,packet,1200,&size)!=XRUDP_OK)return XRUDP_IO_ERROR;
	raw.Now+=1000;return xrtRudpSessionInput(raw.Session,packet,size,raw.Now);
}
static int poll(bool acknowledge)
{
	uint64_t due;raw.Now+=1000;raw.Count=0;CHECK(xrtRudpSessionPoll(raw.Session,raw.Now,&due)==XRUDP_OK && due>raw.Now);
	for(size_t i=0;i<raw.Count;++i) {
		unsigned char plain[1200];size_t size,cursor=0;xrudpwirepacket parsed;
		CHECK(xrtRudpPacketDecode(raw.Packet[i].Data,raw.Packet[i].Size,&parsed)==XRUDP_OK);
		CHECK(xrtRudpCipherOpen(raw.Raw,raw.Packet[i].Data,raw.Packet[i].Size,plain,1200,&size)==XRUDP_OK);
		CHECK(xrtRudpAckSetRecord(&raw.Received,parsed.Header.PacketNumber,raw.Now)==XRUDP_OK);
		while(cursor<size) {
			size_t before=cursor;xrudpwireframe f;CHECK(xrtRudpFrameNext(plain,size,&cursor,&f)==XRUDP_OK);
			if(f.Type==XRUDP_FRAME_ACK) {
				xrudpack ack;CHECK(xrtRudpAckDecode(plain+before,cursor-before,&ack)==XRUDP_OK);
				for(size_t j=0;j<ack.Count;++j)if(raw.Forbidden>=ack.Ranges[j].First && raw.Forbidden<=ack.Ranges[j].Last)raw.FalseAck=true;
			}
			if(f.Type==XRUDP_FRAME_KEY_CONFIRM && get32(f.Data)==2)raw.Confirmed=true;
			if(f.Type==XRUDP_FRAME_MAX_STREAMS)raw.StreamsLimit=get32(f.Data);
			if(f.Type==XRUDP_FRAME_RESET) { raw.ResetId=get32(f.Data);raw.ResetFinal=get64(f.Data+4); }
			if(f.Type==XRUDP_FRAME_STREAM) {
				uint32_t id=get32(f.Data);if(id==2 || id==4)raw.Wire[id==4]+=raw.Packet[i].Size;
			}
		}
	}
	if(acknowledge && raw.Count) {
		unsigned char plain[1136];size_t used;CHECK(xrtRudpAckSetEncode(&raw.Received,raw.Now,plain,1136,&used)==XRUDP_OK);
		CHECK(deliver(plain,used)==XRUDP_OK);
	}
	return 0;
}
static int ordered_retirement(void)
{
	xrudpsessionconfig c;configure(&c);CHECK(start(&c)==0);
#ifdef TEST_MULTI_MEMORY
	CHECK(xrtMemDebugFailAfter(0));
#endif
	unsigned char plain[1136];size_t used=0;CHECK(open_frame(plain,&used,5,1)==0);
	CHECK(xrtRudpCipherUpdateKey(raw.Raw)==XRUDP_OK);raw.Forbidden=2;
	CHECK(deliver(plain,used)==XRUDP_BACKPRESSURE);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK);
	CHECK(state.ActiveStreams==1 && !state.AckPending && state.LastActivityUs==0 && state.ReceivedConnection==0);
	CHECK(poll(false)==0 && raw.Confirmed && !raw.FalseAck);
	xrudpcipherstate crypto;CHECK(xrtRudpCipherState(raw.Raw,&crypto)==XRUDP_OK && !crypto.SendKeyPending);
	used=0;CHECK(stream_frame(plain,&used,3,0,true)==0);CHECK(open_frame(plain,&used,3,65535)==0);
	CHECK(deliver(plain,used)==XRUDP_OK);
	xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(raw.Session,3,&stream)==XRUDP_OK && stream.Weight==65535 && stream.Receive.HasFinal);
	CHECK(xrtRudpSessionStreamFinish(raw.Session,3)==XRUDP_OK);
	for(size_t i=0;i<50;++i) {
		CHECK(poll(true)==0);CHECK(xrtRudpSessionStreamState(raw.Session,3,&stream)==XRUDP_OK);
		if(stream.WriteDone && !stream.References)break;
	}
	CHECK(stream.WriteDone && !stream.References && !raw.FalseAck);
	CHECK(xrtRudpSessionStreamRetire(raw.Session,3)==XRUDP_OK);
	CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ActiveStreams==1 && state.IncomingLimit==4);
	used=0;CHECK(open_frame(plain,&used,3,1)==0);CHECK(stream_frame(plain,&used,3,99,false)==0);
	CHECK(deliver(plain,used)==XRUDP_OK);
	CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ActiveStreams==1 && state.ReceivedConnection==0);
	CHECK(xrtRudpSessionStreamState(raw.Session,3,&stream)==XRUDP_CLOSED);
	used=0;CHECK(open_frame(plain,&used,7,1)==0);CHECK(deliver(plain,used)==XRUDP_BACKPRESSURE);
	used=0;CHECK(open_frame(plain,&used,5,1)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	used=0;CHECK(open_frame(plain,&used,7,1)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ActiveStreams==3);
	stop();
#ifdef TEST_MULTI_MEMORY
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
#endif
	return 0;
}
static int atomic_rejection(void)
{
	for(unsigned int mode=0;mode<5;++mode) {
		xrudpsessionconfig c;configure(&c);c.Local.ConnectionCredit=256;CHECK(start(&c)==0);
		size_t written;CHECK(xrtRudpSessionWrite(raw.Session,"abc",3,&written)==XRUDP_OK && written==3);CHECK(poll(false)==0);
		xrudpsessionstate before,after;CHECK(xrtRudpSessionState(raw.Session,&before)==XRUDP_OK && before.Sent.LargestSent>=2);
		unsigned char plain[1136];size_t used=0;CHECK(open_frame(plain,&used,3,1)==0);CHECK(stream_frame(plain,&used,3,159,false)==0);
		CHECK(ack_frame(plain,&used,2)==0);
		if(mode==0) { CHECK(open_frame(plain,&used,5,1)==0);CHECK(stream_frame(plain,&used,5,159,false)==0); }
		if(mode==1)CHECK(ack_frame(plain,&used,999)==0);
		if(mode==2)CHECK(open_frame(plain,&used,2,1)==0); /* Wrong initiator parity. */
		if(mode==3) { CHECK(open_frame(plain,&used,3,2)==0); } /* Conflicting OPEN. */
		if(mode==4) { unsigned char value[4];put32(value,UINT32_MAX);CHECK(append(plain,&used,XRUDP_FRAME_MAX_STREAMS,0,value,4)==0); }
		CHECK(deliver(plain,used)==XRUDP_MALFORMED);
		CHECK(xrtRudpSessionState(raw.Session,&after)==XRUDP_OK && after.Reason==XRUDP_MALFORMED);
		CHECK(after.ActiveStreams==1 && after.ReceivedConnection==0 && after.Sent.LargestAcked==before.Sent.LargestAcked && after.PeerLimit==before.PeerLimit);
		stop();
	}
	return 0;
}
static int weighted_service(void)
{
	xrudpsessionconfig c;configure(&c);c.TransmitCapacity=65536;c.Peer.StreamCredit=65536;c.Peer.ConnectionCredit=1048576;
	CHECK(start(&c)==0);xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);uint32_t id;
	stream.Weight=4;CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_OK && id==2);
	stream.Weight=1;CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_OK && id==4);
	unsigned char bytes[4096]={0};size_t written;
	for(size_t i=0;i<200;++i) {
		for(uint32_t j=2;j<=4;j+=2) {
			xrudpresult r=xrtRudpSessionStreamWrite(raw.Session,j,bytes,sizeof(bytes),&written);CHECK(r==XRUDP_OK || r==XRUDP_BACKPRESSURE);
		}
		CHECK(poll(true)==0);
		/* Raw peer consumes all bytes and grants ample new credit. */
		unsigned char plain[1136],body[12];size_t used=0;
		for(uint32_t j=2;j<=4;j+=2) { put32(body,j);put64(body+4,1048576);CHECK(append(plain,&used,XRUDP_FRAME_MAX_STREAM_DATA,0,body,12)==0); }
		CHECK(deliver(plain,used)==XRUDP_OK);
		if(raw.Wire[0]+raw.Wire[1]>300000)break;
	}
	CHECK(raw.Wire[0]+raw.Wire[1]>300000 && raw.Wire[1]>30000);
	CHECK(raw.Wire[0]>raw.Wire[1]*3 && raw.Wire[0]<raw.Wire[1]*5);
	printf("xrudp weighted wire bytes: 4:1 => %llu:%llu\n",(unsigned long long)raw.Wire[0],(unsigned long long)raw.Wire[1]);stop();return 0;
}
static int lifetime_cycles(void)
{
	xrudpsessionconfig c;configure(&c);CHECK(start(&c)==0);
	unsigned char plain[1136],bytes[256]={0};size_t used=0,written,read;
	CHECK(open_frame(plain,&used,3,1)==0);CHECK(deliver(plain,used)==XRUDP_OK); /* Keep this stream open throughout. */
#ifdef TEST_MULTI_MEMORY
	xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);CHECK(xrtMemDebugFailAfter(0));
#endif
	uint64_t received=0,sent=0;
	for(uint32_t cycle=0;cycle<80;++cycle) {
		uint32_t id=5+cycle*2;used=0;CHECK(open_frame(plain,&used,id,1)==0);put64(plain+8,32); /* Raw OPEN grants only 32 bytes. */
		if(cycle%3==1) {
			CHECK(stream_frame(plain,&used,id,31,false)==0);
			unsigned char reset[16];put32(reset,id);put64(reset+4,32);put32(reset+12,123);
			CHECK(append(plain,&used,XRUDP_FRAME_RESET,0,reset,16)==0);received+=32;
		} else CHECK(stream_frame(plain,&used,id,0,true)==0);
		CHECK(deliver(plain,used)==XRUDP_OK);
		if(cycle%3==0) {
			CHECK(xrtRudpSessionStreamWrite(raw.Session,id,bytes,256,&written)==XRUDP_OK && written==256);
			CHECK(poll(false)==0);xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(raw.Session,id,&stream)==XRUDP_OK);
			CHECK(stream.Transmit.QueuedEnd==256 && stream.Transmit.HighestSent==32);
			CHECK(xrtRudpSessionStreamReset(raw.Session,id,123)==XRUDP_OK);sent+=32;
			xrudpsessionstate before,after;CHECK(xrtRudpSessionState(raw.Session,&before)==XRUDP_OK);
			unsigned char credit[12];put32(credit,id);put64(credit+4,1024);used=0;
			CHECK(append(plain,&used,XRUDP_FRAME_MAX_STREAM_DATA,0,credit,12)==0);CHECK(deliver(plain,used)==XRUDP_OK);
			CHECK(xrtRudpSessionState(raw.Session,&after)==XRUDP_OK && after.ProgressAtUs==before.ProgressAtUs);
			CHECK(xrtRudpSessionStreamState(raw.Session,id,&stream)==XRUDP_OK && stream.Transmit.PeerLimit==32);
		} else CHECK(xrtRudpSessionStreamFinish(raw.Session,id)==XRUDP_OK);
		CHECK(xrtRudpSessionStreamRetire(raw.Session,id)==XRUDP_BACKPRESSURE);
		xrudpstreamstate stream;
		for(size_t attempt=0;attempt<100;++attempt) {
			CHECK(poll(true)==0);CHECK(xrtRudpSessionStreamState(raw.Session,id,&stream)==XRUDP_OK);
			if(stream.WriteDone && !stream.References)break;
		}
		CHECK(stream.WriteDone && !stream.References);
		if(cycle%3==0)CHECK(stream.ResetAcknowledged && stream.ResetFinal==32 && raw.ResetId==id && raw.ResetFinal==32);
		if(cycle%3==1) {
			CHECK(stream.Receive.Reset && stream.Receive.FinalOffset==32 && stream.Receive.ReadOffset==32);
			CHECK(xrtRudpSessionStreamRead(raw.Session,id,bytes,256,&read)==XRUDP_CANCELLED && read==0);
		}
		CHECK(xrtRudpSessionStreamRetire(raw.Session,id)==XRUDP_OK);
		xrudpsessionstate state;CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ActiveStreams==2 && state.IncomingLimit==4+cycle);
		CHECK(state.ReceivedConnection==received && state.SentConnection==sent && state.ReceiveLimit==c.Local.ConnectionCredit+received);
		if(cycle%7==0) {
			used=0;CHECK(open_frame(plain,&used,id,1)==0);CHECK(stream_frame(plain,&used,id,200,false)==0);CHECK(deliver(plain,used)==XRUDP_OK);
			CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ReceivedConnection==received && state.ActiveStreams==2);
		}
		CHECK(xrtRudpSessionStreamState(raw.Session,3,&stream)==XRUDP_OK && !stream.Receive.HasFinal && !stream.WriteDone);
		CHECK(poll(true)==0); /* Reliably publish the next cumulative stream grant. */
	}
	for(size_t attempt=0;attempt<100 && raw.StreamsLimit!=83;++attempt)CHECK(poll(true)==0);
	CHECK(raw.StreamsLimit==83);
#ifdef TEST_MULTI_MEMORY
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
#endif
	stop();puts("xrudp lifetime: 80 slot reuses beside a long-lived stream, sent-only RESET final size and discarded-hole credit passed");return 0;
}
static int maximum_pool(void)
{
	xrudpsessionconfig c;configure(&c);c.StreamCapacity=c.Local.MaxStreams=c.Peer.MaxStreams=64;CHECK(start(&c)==0);
#ifdef TEST_MULTI_MEMORY
	CHECK(xrtMemDebugFailAfter(0));
#endif
	unsigned char plain[1136];xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);size_t written;
	for(uint32_t n=1;n<64;++n) {
		uint32_t id;CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_OK && id==n*2);
		size_t used=0;CHECK(open_frame(plain,&used,n*2+1,1)==0);CHECK(stream_frame(plain,&used,n*2+1,0,true)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	}
	uint32_t id=999;CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_BACKPRESSURE && id==999);
	for(id=1;id<128;++id) {
		CHECK(xrtRudpSessionStreamWrite(raw.Session,id,"x",1,&written)==XRUDP_OK && written==1);
		CHECK(xrtRudpSessionStreamFinish(raw.Session,id)==XRUDP_OK);
	}
	bool done=false;
	for(size_t attempt=0;attempt<1000 && !done;++attempt) {
		CHECK(poll(true)==0);done=true;
		for(id=1;id<128;++id) { xrudpstreamstate state;CHECK(xrtRudpSessionStreamState(raw.Session,id,&state)==XRUDP_OK);if(!state.WriteDone || state.References)done=false; }
	}
	CHECK(done);xrudpsessionstate state;CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.ActiveStreams==127 && state.SentConnection==127);
	bool seen[128]={false};size_t events=0,opened=0;xrudpstreamevent event;xrudpresult r;
	while((r=xrtRudpSessionStreamNextEvent(raw.Session,&event))==XRUDP_OK) {
		CHECK(event.Id>0 && event.Id<128 && !seen[event.Id]);seen[event.Id]=true;++events;
		if(event.Events&XRUDP_STREAM_OPENED)++opened;
	}
	CHECK(r==XRUDP_DONE && events==127 && opened==63);
	/* A free local slot alone cannot bypass cumulative peer credit. */
	size_t used=0;CHECK(stream_frame(plain,&used,2,0,true)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	CHECK(xrtRudpSessionStreamRetire(raw.Session,2)==XRUDP_OK);
	id=999;CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_BACKPRESSURE && id==999);
	unsigned char value[4];put32(value,65);used=0;CHECK(append(plain,&used,XRUDP_FRAME_MAX_STREAMS,0,value,4)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	put32(value,64);used=0;CHECK(append(plain,&used,XRUDP_FRAME_MAX_STREAMS,0,value,4)==0);CHECK(deliver(plain,used)==XRUDP_OK);
	CHECK(xrtRudpSessionState(raw.Session,&state)==XRUDP_OK && state.OutgoingLimit==65);
	CHECK(xrtRudpSessionStreamOpen(raw.Session,&stream,&id)==XRUDP_OK && id==128);
	stop();
#ifdef TEST_MULTI_MEMORY
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
#endif
	puts("xrudp maximum pool: both origins filled, 127 concurrent streams completed with bounded event draining");return 0;
}
#ifdef TEST_MULTI_MEMORY
static int allocation_faults(void)
{
	for(uint64_t budget=0;budget<=13;++budget) {
		xrudpcipher* cipher[2]={NULL,NULL};CHECK(pair(cipher)==0);xrudpsessionconfig c;configure(&c);
		xrudpsession* session=(xrudpsession*)(uintptr_t)1;xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
		CHECK(xrtMemDebugFailAfter(budget));xrudpresult r=xrtRudpSessionCreate(&c,cipher[1],0,capture,NULL,&session);
		bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
		if(budget<12) {
			CHECK(r==XRUDP_NO_MEMORY && triggered && session==(xrudpsession*)(uintptr_t)1);
			xrudpcipherstate state;CHECK(xrtRudpCipherState(cipher[1],&state)==XRUDP_OK && state.NextPacketNumber==2);
			xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
			xrtRudpCipherDestroy(cipher[1]);
		} else {
			CHECK(r==XRUDP_OK && !triggered);size_t reserved;CHECK(xrtRudpSessionAllocationSize(&c,&reserved)==XRUDP_OK);
			xrtMemDebugSnapshot(&after);CHECK(after.LiveBytes-before.LiveBytes==reserved-xrtRudpCipherAllocationSize());xrtRudpSessionDestroy(session);
		}
		xrtRudpCipherDestroy(cipher[0]);xrtClearError();
	}
	return 0;
}
#endif
int main(void)
{
	CHECK(ordered_retirement()==0);CHECK(atomic_rejection()==0);CHECK(weighted_service()==0);CHECK(lifetime_cycles()==0);CHECK(maximum_pool()==0);
#ifdef TEST_MULTI_MEMORY
	CHECK(allocation_faults()==0);
#endif
	puts("xrudp multistream transactions: ordered OPEN, deferred epoch confirmation, retired IDs, ACK/flow atomicity and bounded weights passed");return 0;
}
