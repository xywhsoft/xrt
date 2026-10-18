#define TEST_SESSION_FAULTS
#define main xrudp_stop_helpers_main
#include "test_session.c"
#undef main

static void stop_config(xrudpsessionconfig* c)
{
	configure(c,256,512);c->StreamCapacity=c->Local.MaxStreams=c->Peer.MaxStreams=3;
	c->Local.MaxMessageSize=c->Peer.MaxMessageSize=256;c->ProgressTimeoutUs=1000000;
}
static int stop_frame(unsigned char* plain,size_t* size,uint32_t id,uint32_t reason)
{
	unsigned char body[8];put32(body,id);put32(body+4,reason);
	return append(plain,size,XRUDP_FRAME_STOP_SENDING,0,body,sizeof(body));
}
static int stop_stream(unsigned char* plain,size_t* size,uint32_t id,uint64_t offset,const void* data,size_t count)
{
	unsigned char body[1200];put32(body,id);put64(body+4,offset);if(count)memcpy(body+12,data,count);
	return append(plain,size,XRUDP_FRAME_STREAM,0,body,count+12);
}
static int stop_open(xrudpsession* s,xrudpcipher* peer,bool messages)
{
	unsigned char plain[1200],body[16]={0};size_t size=0;put32(body,2);put64(body+4,256);body[12]=messages?1:0;body[13]=1;body[15]=1;
	CHECK(append(plain,&size,XRUDP_FRAME_OPEN,0,body,sizeof(body))==0 && deliver_raw(s,peer,plain,size,0,XRUDP_OK)==0);return 0;
}
static int stop_ack(xrudpsession* s,xrudpcipher* peer,uint64_t pn,uint64_t now)
{
	unsigned char plain[40],body[24]={0};size_t size=0;body[5]=1;put64(body+8,pn);put64(body+16,pn);
	CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,sizeof(body))==0 && deliver_raw(s,peer,plain,size,now,XRUDP_OK)==0);return 0;
}
static int peer_stop_transaction(void)
{
	for(uint32_t bad=0;bad<3;++bad) {
		memset(&net,0,sizeof(net));xrudpsessionconfig c;stop_config(&c);xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0);
		size_t written;uint64_t due;unsigned char data[200]={0},plain[1200],body[24]={0};size_t size=0;
		CHECK(xrtRudpSessionWrite(s,"prefix",6,&written)==XRUDP_OK && written==6 && xrtRudpSessionPoll(s,0,&due)==XRUDP_OK);
		CHECK(xrtRudpSessionWrite(s,data,sizeof(data),&written)==XRUDP_OK && written==sizeof(data));
		CHECK(stop_stream(plain,&size,1,0,"safe",4)==0 && stop_frame(plain,&size,1,71)==0);
		put64(body,512);CHECK(append(plain,&size,XRUDP_FRAME_MAX_DATA,0,body,8)==0);
		if(bad==2)CHECK(stop_frame(plain,&size,1,72)==0);
		memset(body,0,sizeof(body));body[5]=1;put64(body+8,bad==1?999:2);put64(body+16,bad==1?999:2);
		CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0);
		CHECK(xrtMemDebugFailAfter(0));CHECK(deliver_raw(s,peer,plain,size,1000,bad?XRUDP_MALFORMED:XRUDP_OK)==0);
		xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK);
		if(bad)CHECK(!state.WriteStopped && state.ReceivedConnection==0 && state.PeerLimit==256 && state.Receive.HighestOffset==0);
		else {
			xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(s,1,&stream)==XRUDP_OK && stream.PeerStopped && stream.PeerStopReason==71 && stream.ResetQueued && stream.ResetFinal==6);
			CHECK(state.WriteStopped && state.WriteStopReason==71 && state.SentConnection==6 && state.Transmit.PendingBytes==0 && state.ReceivedConnection==4);
			CHECK(xrtRudpSessionWrite(s,"x",1,&written)==XRUDP_CLOSED && written==0);
			CHECK(xrtRudpSessionRead(s,data,sizeof(data),&written)==XRUDP_OK && written==4 && memcmp(data,"safe",4)==0);
			xrudpstreamevent event;CHECK(xrtRudpSessionStreamNextEvent(s,&event)==XRUDP_OK && event.Id==1 && (event.Events&XRUDP_STREAM_WRITE_STOPPED));
			size=0;CHECK(stop_frame(plain,&size,1,71)==0 && deliver_raw(s,peer,plain,size,2000,XRUDP_OK)==0);
			CHECK(xrtRudpSessionStreamNextEvent(s,&event)==XRUDP_DONE);
			size=0;CHECK(stop_stream(plain,&size,1,4,"bad",3)==0 && stop_frame(plain,&size,1,72)==0);
			CHECK(deliver_raw(s,peer,plain,size,3000,XRUDP_MALFORMED)==0);
			CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.ReceivedConnection==4 && state.WriteStopReason==71);
		}
		CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);xrtClearError();
	}
	return 0;
}
static int local_stop_credit(bool messages)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;stop_config(&c);xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0);
	uint32_t id=messages?2:1;if(messages)CHECK(stop_open(s,peer,true)==0);
	unsigned char plain[1200],data[256]={0};size_t size=0;
	if(messages) { put32(data,20);CHECK(stop_stream(plain,&size,id,0,data,14)==0); }
	else CHECK(stop_stream(plain,&size,id,100,data,20)==0);
	CHECK(deliver_raw(s,peer,plain,size,1000,XRUDP_OK)==0);
	CHECK(xrtMemDebugFailAfter(0));CHECK(xrtRudpSessionStreamStop(s,id,7)==XRUDP_OK);
	xrudpsessionstate state;xrudpstreamstate stream;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && xrtRudpSessionStreamState(s,id,&stream)==XRUDP_OK);
	uint64_t high=messages?14:120,limit=messages?270:256;
	CHECK(state.ReceivedConnection==high && state.ReceiveLimit==256+high && stream.Receive.Stopped && stream.Receive.StopReason==7 && !stream.Receive.HasFinal && stream.Receive.ReadOffset==high && stream.Receive.Limit==limit);
	CHECK(stream.StopQueued && !stream.StopSent && !stream.StopComplete && !stream.Receive.BufferedBytes);
	if(messages)CHECK(stream.Message.Reason==XRUDP_CANCELLED && !stream.Message.Buffered && !stream.Message.HeaderBytes);
	size_t read;CHECK(xrtRudpSessionStreamStop(s,id,7)==XRUDP_DUPLICATE && xrtRudpSessionStreamStop(s,id,8)==XRUDP_ARGUMENT);
	if(messages)CHECK(xrtRudpSessionMessagePeek(s,id,&read)==XRUDP_CANCELLED && read==0 && xrtRudpSessionMessageRead(s,id,data,sizeof(data),&read)==XRUDP_CANCELLED);
	else CHECK(xrtRudpSessionRead(s,data,sizeof(data),&read)==XRUDP_CANCELLED && read==0);
	memset(data,0xff,sizeof(data));size=0;CHECK(stop_stream(plain,&size,id,high,data,50)==0 && stop_stream(plain,&size,id,high+25,data,50)==0);
	CHECK(deliver_raw(s,peer,plain,size,2000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.ReceivedConnection==high+75 && state.ReceiveLimit==256+high+75);
	CHECK(xrtRudpSessionStreamState(s,id,&stream)==XRUDP_OK && stream.Receive.Limit==limit && !stream.Receive.BufferedBytes && stream.Receive.ReadOffset==high+75);
	/* A later invalid final size must not partially return more credit. */
	size=0;CHECK(stop_stream(plain,&size,id,high+75,data,10)==0);put32(data,id);put64(data+4,high+80);put32(data+12,9);
	CHECK(append(plain,&size,XRUDP_FRAME_RESET,0,data,16)==0 && deliver_raw(s,peer,plain,size,3000,XRUDP_MALFORMED)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.ReceivedConnection==high+75 && state.ReceiveLimit==256+high+75);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);xrtClearError();return 0;
}
static int stop_recovery(void)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;stop_config(&c);c.PacketCapacity=1;xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0);
	CHECK(xrtRudpSessionStop(s,7)==XRUDP_OK);net.Output[0].Block=true;uint64_t due;
	CHECK(xrtRudpSessionPoll(s,1000,&due)==XRUDP_OK);xrudpstreamstate stream;
	CHECK(xrtRudpSessionStreamState(s,1,&stream)==XRUDP_OK && !stream.StopSent && !stream.References);
	net.Output[0].Block=false;CHECK(xrtRudpSessionPoll(s,2000,&due)==XRUDP_OK);
	CHECK(xrtRudpSessionStreamState(s,1,&stream)==XRUDP_OK && stream.StopSent && !stream.StopComplete && stream.References==1);
	CHECK(xrtRudpSessionPoll(s,200000,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.ProbeTimeouts==1 && state.OutstandingProbes==0);
	uint32_t stops=0;uint64_t largest=0;
	for(size_t i=0;i<TEST_QUEUE;++i)if(net.Queue[i].Used) {
		unsigned char plain[1200];size_t size,offset=0;xrudpwirepacket packet;xrudpwireframe frame;
		CHECK(xrtRudpPacketDecode(net.Queue[i].Data,net.Queue[i].Size,&packet)==XRUDP_OK);
		if(packet.Header.PacketNumber>largest)largest=packet.Header.PacketNumber;
		CHECK(xrtRudpCipherOpen(peer,net.Queue[i].Data,net.Queue[i].Size,plain,sizeof(plain),&size)==XRUDP_OK);
		while(offset<size) { CHECK(xrtRudpFrameNext(plain,size,&offset,&frame)==XRUDP_OK);if(frame.Type==XRUDP_FRAME_STOP_SENDING) { CHECK(frame.Size==8 && frame.Data[3]==1 && frame.Data[7]==7);++stops; } }
	}
	CHECK(stops==3 && largest==5); /* Failed admission consumed PN2, success PN3 plus two bounded probes. */
	CHECK(stop_ack(s,peer,largest,201000)==0);
	CHECK(xrtRudpSessionStreamState(s,1,&stream)==XRUDP_OK && stream.StopComplete && !stream.Receive.HasFinal);
	unsigned char ping[4]={1,0,0,0};CHECK(deliver_raw(s,peer,ping,sizeof(ping),500000,XRUDP_OK)==0 && deliver_raw(s,peer,ping,sizeof(ping),1000000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionPoll(s,2000000,&due)==XRUDP_TIMEOUT); /* ACKed request cannot wait forever for a real final. */
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int stop_retirement(void)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;stop_config(&c);xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && stop_open(s,peer,false)==0);
	CHECK(xrtRudpSessionStreamStop(s,2,7)==XRUDP_OK && xrtRudpSessionStreamFinish(s,2)==XRUDP_OK);
	uint64_t due;CHECK(xrtRudpSessionPoll(s,2000,&due)==XRUDP_OK && stop_ack(s,peer,2,3000)==0);
	xrudpstreamstate state;CHECK(xrtRudpSessionStreamState(s,2,&state)==XRUDP_OK && state.WriteDone && state.StopComplete && !state.Receive.HasFinal);
	CHECK(xrtRudpSessionStreamRetire(s,2)==XRUDP_BACKPRESSURE);
	unsigned char plain[1200],body[16]={0};size_t size=0;put32(body,2);put32(body+12,9);
	CHECK(append(plain,&size,XRUDP_FRAME_RESET,0,body,16)==0 && deliver_raw(s,peer,plain,size,4000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionStreamState(s,2,&state)==XRUDP_OK && state.RetireReady && state.Receive.StopReason==7 && state.Receive.ResetReason==9);
	CHECK(xrtRudpSessionStreamRetire(s,2)==XRUDP_OK);size=0;CHECK(stop_frame(plain,&size,2,999)==0 && deliver_raw(s,peer,plain,size,5000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionStreamState(s,2,&state)==XRUDP_CLOSED);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
typedef struct crossed_link { xrudpsession* Peer; uint64_t* Now; } crossed_link;
static xrudpresult crossed_output(const void* data,size_t size,void* context)
{
	crossed_link* link=(crossed_link*)context;unsigned char copy[1200];memcpy(copy,data,size);
	xrudpresult r=xrtRudpSessionInput(link->Peer,copy,size,*link->Now);
	return r==XRUDP_OK || r==XRUDP_DUPLICATE || r==XRUDP_TOO_OLD ? XRUDP_OK : XRUDP_IO_ERROR;
}
static int crossed_stop(void)
{
	xrudpcipher* cipher[2];CHECK(raw_pair(cipher)==0);xrudpsession* sessions[2];crossed_link link[2];uint64_t now=0,due;bool done=false;
	for(uint32_t i=0;i<2;++i) {
		xrudpsessionconfig c;stop_config(&c);c.Role=i?XRUDP_RESPONDER:XRUDP_INITIATOR;link[i].Now=&now;
		CHECK(xrtRudpSessionCreate(&c,cipher[i],0,crossed_output,&link[i],&sessions[i])==XRUDP_OK);
	}
	for(uint32_t i=0;i<2;++i) {
		link[i].Peer=sessions[1-i];unsigned char data[64]={0};size_t written;
		CHECK(xrtRudpSessionWrite(sessions[i],data,sizeof(data),&written)==XRUDP_OK && written==sizeof(data));
		CHECK(xrtRudpSessionStop(sessions[i],17+i)==XRUDP_OK);
	}
	CHECK(xrtMemDebugFailAfter(0));
	for(now=1000;now<500000 && !done;now+=1000) {
		done=true;
		for(uint32_t i=0;i<2;++i) {
			CHECK(xrtRudpSessionPoll(sessions[i],now,&due)==XRUDP_OK);xrudpstreamstate state;
			CHECK(xrtRudpSessionStreamState(sessions[i],1,&state)==XRUDP_OK);
			if(!state.WriteDone || !state.ReadDone || !state.PeerStopped || !state.StopComplete || state.References)done=false;
		}
	}
	CHECK(done && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	xrudpsessionstate a,b;CHECK(xrtRudpSessionState(sessions[0],&a)==XRUDP_OK && xrtRudpSessionState(sessions[1],&b)==XRUDP_OK);
	CHECK(a.WriteStopReason==18 && b.WriteStopReason==17 && a.Receive.StopReason==17 && b.Receive.StopReason==18);
	CHECK(a.SentConnection==b.ReceivedConnection && b.SentConnection==a.ReceivedConnection && a.ReceiveLimit==256+a.ReceivedConnection && b.ReceiveLimit==256+b.ReceivedConnection);
	CHECK(a.PendingReceiveStops==0 && b.PendingReceiveStops==0);
	for(size_t i=0;i<2;++i)xrtRudpSessionDestroy(sessions[i]);
	return 0;
}
static int stop_independent_deadline(void)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;stop_config(&c);c.KeyUpdatePackets=XRUDP_KEY_PACKETS_MAX;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && stop_open(s,peer,false)==0);
	CHECK(xrtRudpSessionStreamStop(s,2,71)==XRUDP_OK);uint64_t due;xrudpstreamstate stream;xrudpsessionstate state;
	CHECK(xrtRudpSessionStreamState(s,2,&stream)==XRUDP_OK && stream.ReceiveStopDeadlineUs==0);
	CHECK(xrtRudpSessionPoll(s,2000000,&due)==XRUDP_OK && stop_ack(s,peer,2,2001000)==0);
	CHECK(xrtRudpSessionStreamState(s,2,&stream)==XRUDP_OK && stream.StopComplete && !stream.ReadDone && stream.ReceiveStopDeadlineUs==3000000);
	for(uint32_t i=0;i<4;++i) {
		unsigned char plain[100],byte='a';size_t size=0,read;
		CHECK(stop_stream(plain,&size,1,i,&byte,1)==0 && deliver_raw(s,peer,plain,size,2200000+i*200000,XRUDP_OK)==0);
		CHECK(xrtRudpSessionRead(s,&byte,1,&read)==XRUDP_OK && read==1);
		CHECK(xrtRudpSessionStreamStop(s,2,71)==XRUDP_DUPLICATE);
	}
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.PendingReceiveStops==1 && state.ProgressAtUs==2800000);
	CHECK(xrtRudpSessionPoll(s,3000000,&due)==XRUDP_TIMEOUT);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_TIMEOUT && !state.PendingReceiveStops && state.LastNowUs-state.ProgressAtUs<c.ProgressTimeoutUs);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
int main(void)
{
	xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(peer_stop_transaction()==0 && local_stop_credit(false)==0 && local_stop_credit(true)==0 && stop_recovery()==0 && stop_retirement()==0 && crossed_stop()==0 && stop_independent_deadline()==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp stop session: atomic peer requests, independent halves, frozen credit, staged-message discard, bounded retries and final-before-retire passed");return 0;
}
