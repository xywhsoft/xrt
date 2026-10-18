#define TEST_SESSION_FAULTS
#define main xrudp_session_datagram_helpers_main
#include "test_session.c"
#undef main

static void game_config(xrudpsessionconfig* c)
{
	configure(c,512,512); c->Datagrams.Channels=3; c->Datagrams.SendCapacity=8; c->Datagrams.ReceiveCapacity=1;
	c->Datagrams.PayloadCapacity=XRUDP_DATAGRAM_PAYLOAD_MAX; c->MaximumWindow=12000; c->ProgressTimeoutUs=1000000;
}
static int game_channels(xrudpsession* s)
{
	for (uint32_t i=1;i<=3;++i) {
		xrudpdatagramchannel c; xrtRudpDatagramChannelInit(&c); c.Id=i; c.Mode=(uint8_t)(i-1); c.ReplacePending=i==3;
		CHECK(xrtRudpSessionDatagramConfigure(s,&c)==XRUDP_OK);
	}
	return 0;
}
static int game_frame(unsigned char* plain,size_t* size,uint32_t id,bool sequenced,uint64_t sequence,const void* data,size_t count)
{
	unsigned char body[XRUDP_PACKET_MAX]; put32(body,id); put64(body+4,sequence); if(count)memcpy(body+12,data,count);
	return append(plain,size,XRUDP_FRAME_DATAGRAM,sequenced?XRUDP_DATAGRAM_SEQUENCED:0,body,12+count);
}
static int inspect_output(xrudpcipher* peer,uint32_t* games,uint64_t* largest)
{
	for(size_t i=0;i<TEST_QUEUE;++i)if(net.Queue[i].Used) {
		session_packet* p=&net.Queue[i]; unsigned char plain[1200]; size_t length,cursor=0; xrudpwirepacket packet;
		CHECK(xrtRudpPacketDecode(p->Data,p->Size,&packet)==XRUDP_OK && p->Size<=512);
		CHECK(xrtRudpCipherOpen(peer,p->Data,p->Size,plain,sizeof(plain),&length)==XRUDP_OK);
		if(packet.Header.PacketNumber>*largest)*largest=packet.Header.PacketNumber;
		xrudpwireframe frame; while(cursor<length) {
			CHECK(xrtRudpFrameNext(plain,length,&cursor,&frame)==XRUDP_OK);
			if(frame.Type==XRUDP_FRAME_DATAGRAM) { ++*games;CHECK(frame.Size==12+424 && frame.Data[12]==0xa5); }
		}
		p->Used=false;
	}
	return 0;
}
static int game_path(void)
{
	memset(&net,0,sizeof(net)); xrudpsessionconfig c;game_config(&c);c.PacketCapacity=1;
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer,&s,&c)==0 && game_channels(s)==0);
	unsigned char data[425];memset(data,0xa5,sizeof(data));uint64_t sequence=99,due;xrudpdatagraminfo info;
	CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpSessionDatagramWrite(s,1,data,425,0,0,&sequence)==XRUDP_LIMIT && sequence==99);
	CHECK(xrtRudpSessionDatagramWrite(s,1,data,424,0,0,&sequence)==XRUDP_OK && sequence==0);
	CHECK(xrtRudpSessionDatagramWrite(s,2,NULL,0,0,1001,&sequence)==XRUDP_OK && sequence==1);
	net.Output[0].Block=true;CHECK(xrtRudpSessionPoll(s,1000,&due)==XRUDP_OK && due<=1001);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.DatagramSendLimit==424 && state.Datagrams.SendQueued==2 && state.Datagrams.Sent==0 && state.OutputBackpressure==1 && state.Sent.StoredPackets==0);
	net.Output[0].Block=false;CHECK(xrtRudpSessionPoll(s,1001,&due)==XRUDP_OK);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Datagrams.Sent==1 && state.Datagrams.SendExpired==1 && state.Datagrams.SendQueued==0 && state.Sent.BytesInFlight==state.Congestion.BytesInFlight && state.Sent.BytesInFlight<=state.Congestion.Window);
	uint32_t games=0;uint64_t largest=0;CHECK(inspect_output(peer,&games,&largest)==0 && games==1);
	/* The dropped datagram is never copied into PTO. Probe ACK releases its
	 * normal ledger slot and loss accounting, not a payload retransmission. */
	CHECK(xrtRudpSessionPoll(s,200000,&due)==XRUDP_OK);CHECK(inspect_output(peer,&games,&largest)==0 && games==1 && largest>3);
	unsigned char plain[1200],body[24]={0};size_t size=0;body[5]=1;put64(body+8,largest);put64(body+16,largest);
	CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && deliver_raw(s,peer,plain,size,400000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.DatagramLost==1 && state.Datagrams.Sent==1 && state.Datagrams.SendQueued==0);
	size=0;CHECK(game_frame(plain,&size,2,true,10,"ok",2)==0 && deliver_raw(s,peer,plain,size,400001,XRUDP_OK)==0);
	CHECK(xrtRudpSessionDatagramRead(s,data,1,400001,&info)==XRUDP_TRUNCATED && info.Size==2);
	CHECK(xrtRudpSessionDatagramRead(s,data,sizeof(data),400001,&info)==XRUDP_OK && info.Sequence==10 && memcmp(data,"ok",2)==0);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int game_transactions(void)
{
	for(unsigned int mode=0;mode<3;++mode) {
		memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);if(mode==2)memset(&c.Datagrams,0,sizeof(c.Datagrams));
		xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0);if(mode!=2)CHECK(game_channels(s)==0);
		unsigned char plain[1200],body[32]={0};size_t size=0;
		CHECK(game_frame(plain,&size,1,false,0,"first",5)==0);
		CHECK(game_frame(plain,&size,77,false,0,"unknown",7)==0);
		CHECK(game_frame(plain,&size,2,false,0,"mismatch",8)==0);
		CHECK(game_frame(plain,&size,1,false,0,"full",4)==0);
		put32(body,1);memcpy(body+12,"safe",4);CHECK(append(plain,&size,XRUDP_FRAME_STREAM,0,body,16)==0);
		if(mode==0) { memset(body,0,sizeof(body));body[5]=1;put64(body+8,999);put64(body+16,999);CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0); }
		CHECK(xrtMemDebugFailAfter(0));CHECK(deliver_raw(s,peer,plain,size,1000,mode==0?XRUDP_MALFORMED:XRUDP_OK)==0);
		xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK);
		if(mode==0) CHECK(state.Datagrams.Received==0 && state.Datagrams.Accepted==0 && state.Receive.HighestOffset==0 && state.ReceivedConnection==0 && state.ReceivedPackets==0);
		else {
			CHECK(state.ReceivedConnection==4 && state.Receive.HighestOffset==4);
			if(mode==1)CHECK(state.Datagrams.Received==4 && state.Datagrams.Accepted==1 && state.Datagrams.DroppedUnknown==1 && state.Datagrams.DroppedMode==1 && state.Datagrams.DroppedCapacity==1);
			else CHECK(state.DatagramDisabledDropped==4 && state.Datagrams.ReceiveQueued==0);
			size_t count;CHECK(xrtRudpSessionRead(s,body,sizeof(body),&count)==XRUDP_OK && count==4 && memcmp(body,"safe",4)==0);
		}
		CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);
	}
	return 0;
}
static int game_allocation_and_clock(void)
{
	for(uint64_t budget=0;budget<=5;++budget) {
		memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);xrudpcipher* cipher[2];CHECK(raw_pair(cipher)==0);
		xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);xrudpsession* s=(xrudpsession*)(uintptr_t)1;
		CHECK(xrtMemDebugFailAfter(budget));xrudpresult r=xrtRudpSessionCreate(&c,cipher[0],0,send_packet,&net.Output[0],&s);bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
		if(budget<5) {
			CHECK(r==XRUDP_NO_MEMORY && failed && s==(xrudpsession*)(uintptr_t)1);xrtClearError();xrtMemDebugSnapshot(&after);
			CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);xrudpcipherstate state;CHECK(xrtRudpCipherState(cipher[0],&state)==XRUDP_OK && state.NextPacketNumber==2);xrtRudpCipherDestroy(cipher[0]);
		} else {
			CHECK(r==XRUDP_OK && !failed);size_t bytes;CHECK(xrtRudpSessionAllocationSize(&c,&bytes)==XRUDP_OK);xrtMemDebugSnapshot(&after);CHECK(after.LiveBytes-before.LiveBytes==bytes-xrtRudpCipherAllocationSize());
			CHECK(game_channels(s)==0);uint64_t seq,due;CHECK(xrtRudpSessionDatagramWrite(s,1,"expires",7,0,10000,&seq)==XRUDP_OK);
			net.Output[0].Block=true;CHECK(xrtRudpSessionPoll(s,1000,&due)==XRUDP_OK);CHECK(xrtRudpSessionPoll(s,2000000,&due)==XRUDP_OK);
			xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_OK && state.Datagrams.SendExpired==1 && state.Datagrams.SendQueued==0);
			CHECK(xrtRudpSessionDatagramWrite(s,1,NULL,0,2000001,2000000,&seq)==XRUDP_TIMEOUT);
			CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_OK);xrtRudpSessionDestroy(s);
		}
		xrtRudpCipherDestroy(cipher[1]);xrtClearError();
	}
	return 0;
}
static int game_cwnd(void)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);c.PacketCapacity=64;c.Datagrams.SendCapacity=64;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && game_channels(s)==0);
	unsigned char bytes[424]={0};uint64_t sequence,due;
	for(size_t i=0;i<64;++i)CHECK(xrtRudpSessionDatagramWrite(s,1,bytes,sizeof(bytes),0,0,&sequence)==XRUDP_OK);
	for(uint64_t now=0;now<50000;now+=2000)CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK);
	CHECK(state.Datagrams.Sent>0 && state.Datagrams.SendQueued>0 && state.Sent.PendingPackets==state.Datagrams.Sent);
	CHECK(state.Congestion.BytesInFlight==state.Sent.BytesInFlight && state.Sent.BytesInFlight<=state.Congestion.Window && state.Congestion.Window==12000);
	CHECK(state.Datagrams.Sent*512==state.Sent.BytesInFlight && state.Sent.BytesInFlight+512>state.Congestion.Window);
	CHECK(state.ProbeTimeouts==0 && state.SentConnection==0);xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int game_reliable_turn(bool ackOnly)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);c.PacketCapacity=128;c.Datagrams.SendCapacity=200;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && game_channels(s)==0);
	unsigned char data[256]={0};uint64_t seq,due;
	for(size_t i=0;i<200;++i)CHECK(xrtRudpSessionDatagramWrite(s,1,data,32,0,0,&seq)==XRUDP_OK);
	for(uint64_t now=0;now<100000;now+=1000)CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Congestion.BytesInFlight==12000 && state.Datagrams.Sent==100);
	size_t written;CHECK(xrtRudpSessionWrite(s,data,sizeof(data),&written)==XRUDP_OK && written==sizeof(data));
	/* Fixed cwnd: each ACK releases just 120B, while a reliable packet needs
	 * 344B. Filling every small opening with another game packet starves it. */
	for(uint64_t pn=2;pn<=(ackOnly?5u:4u);++pn) {
		unsigned char plain[1200],body[24]={0};size_t size=0;body[5]=1;put64(body+8,pn);put64(body+16,pn);
		uint64_t now=100000+(pn-2)*2000;
		CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && deliver_raw(s,peer,plain,size,now,XRUDP_OK)==0);
		if(ackOnly) { size=0; CHECK(append(plain,&size,XRUDP_FRAME_PING,0,NULL,0)==0 && deliver_raw(s,peer,plain,size,now,XRUDP_OK)==0); }
		CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	}
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK);
	if(state.Datagrams.Sent!=100 || state.Transmit.HighestSent!=256 || state.DatagramLost!=0)
		fprintf(stderr,"reliable turn ACK-only=%u game=%llu reliable=%llu lost=%llu flight=%llu cwnd=%llu\n",(unsigned)ackOnly,(unsigned long long)state.Datagrams.Sent,(unsigned long long)state.Transmit.HighestSent,(unsigned long long)state.DatagramLost,(unsigned long long)state.Congestion.BytesInFlight,(unsigned long long)state.Congestion.Window);
	CHECK(state.Datagrams.Sent==100 && state.Transmit.HighestSent==256 && state.DatagramLost==0);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int game_datagram_turn(void)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);c.PacketCapacity=128;
	c.ReceiveCapacity=c.TransmitCapacity=8192;
	c.Local.StreamCredit=c.Peer.StreamCredit=c.Local.ConnectionCredit=c.Peer.ConnectionCredit=8192;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && game_channels(s)==0);
	unsigned char data[256]={0};uint64_t seq,due;size_t written;
	/* Symmetric case: 100 small reliable packets fill the fixed window. The
	 * next game packet needs 344B, so small reliable writes must now wait. */
	for(uint64_t now=0;now<100000;now+=1000) {
		CHECK(xrtRudpSessionWrite(s,data,32,&written)==XRUDP_OK && written==32);
		CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	}
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Congestion.BytesInFlight==12000 && state.Transmit.HighestSent==3200);
	CHECK(xrtRudpSessionDatagramWrite(s,1,data,sizeof(data),100000,0,&seq)==XRUDP_OK);
	CHECK(xrtRudpSessionWrite(s,data,32,&written)==XRUDP_OK && written==32);
	for(uint64_t pn=2;pn<=4;++pn) {
		unsigned char plain[1200],body[24]={0};size_t size=0;body[5]=1;put64(body+8,pn);put64(body+16,pn);
		uint64_t now=100000+(pn-2)*2000;
		CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && deliver_raw(s,peer,plain,size,now,XRUDP_OK)==0);
		CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	}
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Datagrams.Sent==1 && state.Transmit.HighestSent==3200 && state.DatagramLost==0);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int game_window_growth(bool flowBlocked,size_t payload)
{
	memset(&net,0,sizeof(net));xrudpsessionconfig c;game_config(&c);
	c.PacketCapacity=256;c.Datagrams.SendCapacity=200;c.MaximumWindow=24000;
	if(flowBlocked)c.Peer.ConnectionCredit=c.Peer.StreamCredit=1;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&c)==0 && game_channels(s)==0);
	unsigned char data[32]={0};uint64_t seq,due;size_t written;
	if(flowBlocked)CHECK(xrtRudpSessionWrite(s,data,1,&written)==XRUDP_OK && written==1);
	for(size_t i=0;i<200;++i)CHECK(xrtRudpSessionDatagramWrite(s,1,data,payload,0,0,&seq)==XRUDP_OK);
	for(uint64_t now=0;now<100000;now+=1000)CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Congestion.Window==12000 && state.Datagrams.SendQueued>0);
	CHECK(state.Congestion.BytesInFlight+payload+88>12000);
	if(flowBlocked)CHECK(state.SentConnection==state.PeerLimit && state.SentConnection==1);
	/* Waiting game packets count as congestion demand, including empty wire
	 * records, even if all reliable flow credit has already been consumed. */
	unsigned char plain[1200],body[24]={0};size_t size=0;body[5]=1;put64(body+8,2);put64(body+16,2);
	CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && deliver_raw(s,peer,plain,size,100000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Congestion.Window>12000 && state.DatagramLost==0);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
int main(void)
{
	randomState=1;xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(game_path()==0 && game_transactions()==0 && game_allocation_and_clock()==0 && game_cwnd()==0 && game_reliable_turn(false)==0 && game_reliable_turn(true)==0 && game_datagram_turn()==0);
	CHECK(game_window_growth(false,32)==0 && game_window_growth(true,32)==0 && game_window_growth(false,0)==0 && game_window_growth(true,0)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
	CHECK(before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp game session: real packet admission, shared ledger/cwnd, no payload retry, atomic receive/policy drops and fixed budgets passed");return 0;
}
