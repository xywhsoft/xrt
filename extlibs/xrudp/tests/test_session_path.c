#define TEST_SESSION_FAULTS
#define main xrudp_path_helpers_main
#include "test_session.c"
#undef main
typedef struct path_packet { unsigned char Data[1200]; size_t Size; xrudpaddress To; } path_packet;
static path_packet pathPackets[256]; static uint32_t pathCount; static bool pathBlock,pathCandidateError,pathAllError;
static xrudpaddress oldAddress={4,41000,0,{192,0,2,2}},newAddress={4,42000,0,{192,0,2,2}},otherAddress={4,43000,0,{192,0,2,2}};
static xrudpresult path_send(const void* data,size_t size,const xrudpaddress* to,void* context)
{
	(void)context;if(pathBlock)return XRUDP_BACKPRESSURE;
	if(pathAllError || (pathCandidateError && to->Port==newAddress.Port))return XRUDP_IO_ERROR;
	if(pathCount==256 || size>1200)return XRUDP_IO_ERROR;
	path_packet* p=&pathPackets[pathCount++];p->Size=size;p->To=*to;memcpy(p->Data,data,size);return XRUDP_OK;
}
static bool address_equal(const xrudpaddress* a,const xrudpaddress* b) { return a->Family==b->Family && a->Port==b->Port && a->Zone==b->Zone && memcmp(a->Address,b->Address,16)==0; }
static int path_raw(xrudpcipher** peer,xrudpsession** s,uint32_t capacity)
{
	memset(&net,0,sizeof(net));pathCount=0;pathBlock=pathCandidateError=pathAllError=false;xrudpsessionconfig c;configure(&c,256,512);c.PacketCapacity=capacity;c.KeyUpdatePackets=XRUDP_KEY_PACKETS_MAX;
	CHECK(raw_session(peer,s,&c)==0 && xrtRudpSessionEnablePaths(*s,&oldAddress,path_send)==XRUDP_OK);return 0;
}
static int path_stream(unsigned char* plain,size_t* size,uint64_t at,const char* data,size_t count)
{
	unsigned char body[1200];put32(body,1);put64(body+4,at);memcpy(body+12,data,count);return append(plain,size,XRUDP_FRAME_STREAM,0,body,count+12);
}
static int path_deliver(xrudpsession* s,xrudpcipher* peer,const xrudpaddress* from,const void* plain,size_t size,uint64_t now,xrudpresult expected)
{
	unsigned char packet[1200];size_t n;CHECK(xrtRudpCipherSeal(peer,plain,size,packet,sizeof(packet),&n)==XRUDP_OK);
	xrudpresult r=xrtRudpSessionPathInput(s,from,packet,n,now);
	if(r!=expected)fprintf(stderr,"path input expected=%u actual=%u\n",(unsigned)expected,(unsigned)r);
	CHECK(r==expected);return 0;
}
static int path_challenge(xrudpcipher* peer,unsigned char token[16],uint32_t* decoded)
{
	bool found=false;
	for(;*decoded<pathCount;++*decoded) {
		path_packet* p=&pathPackets[*decoded];unsigned char plain[1200];size_t size,at=0;xrudpwireframe frame;
		CHECK(xrtRudpCipherOpen(peer,p->Data,p->Size,plain,sizeof(plain),&size)==XRUDP_OK);
		while(xrtRudpFrameNext(plain,size,&at,&frame)==XRUDP_OK)if(frame.Type==XRUDP_FRAME_PATH_CHALLENGE) { CHECK(address_equal(&p->To,&newAddress));memcpy(token,frame.Data,16);found=true; }
	}
	CHECK(found);return 0;
}
static int path_quarantine_and_migration(bool original,uint32_t capacity)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,capacity)==0);
	CHECK(xrtMemDebugFailAfter(0));uint64_t due;size_t n;unsigned char bytes[40]={0};
	CHECK(xrtRudpSessionWrite(s,bytes,sizeof(bytes),&n)==XRUDP_OK && n==sizeof(bytes) && xrtRudpSessionPoll(s,0,&due)==XRUDP_OK);
	CHECK(pathCount==1 && address_equal(&pathPackets[0].To,&oldAddress));
	unsigned char plain[1200],packet[1200];size_t size=0,packetSize;
	CHECK(path_stream(plain,&size,0,"new",3)==0 && xrtRudpCipherUpdateKey(peer)==XRUDP_OK && xrtRudpCipherSeal(peer,plain,size,packet,sizeof(packet),&packetSize)==XRUDP_OK);
	CHECK(xrtRudpSessionInput(s,packet,packetSize,1000)==XRUDP_ARGUMENT);
	packet[packetSize-1]^=1;CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,packetSize,1000)==XRUDP_AUTH_FAILED);packet[packetSize-1]^=1;
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.Path.Validating && state.ReceivedConnection==0 && !state.AckPending);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,packetSize,2000)==XRUDP_BACKPRESSURE);
	for(uint32_t i=0;i<5;++i)CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,packetSize,2000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validating && state.Path.CandidateReceived==packetSize && state.Path.DeadlineUs==3002000 && state.ReceivedConnection==0 && !state.AckPending);
	CHECK(xrtRudpSessionPathInput(s,&otherAddress,packet,packetSize,2000)==XRUDP_BACKPRESSURE);
	if(original) {
		CHECK(xrtRudpSessionPathInput(s,&oldAddress,packet,packetSize,3000)==XRUDP_OK);
		CHECK(xrtRudpSessionRead(s,bytes,sizeof(bytes),&n)==XRUDP_OK && n==3 && memcmp(bytes,"new",3)==0);
	}
	pathBlock=true;CHECK(xrtRudpSessionPoll(s,3000,&due)==XRUDP_OK);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Attempts==0 && state.Path.CandidateSent==0);
	uint64_t base=capacity==1?200000:4000;pathBlock=false;CHECK(xrtRudpSessionPoll(s,base,&due)==XRUDP_OK);
	unsigned char token[16];uint32_t decoded=0;CHECK(path_challenge(peer,token,&decoded)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Attempts==1 && state.Path.CandidateSent<=3*state.Path.CandidateReceived);
	if(capacity==1)CHECK(state.ProbeTimeouts==1 && state.OutstandingProbes==0 && state.Sent.StoredPackets==3);
	/* A matching response prefix cannot smuggle business through quarantine. */
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && path_stream(plain,&size,original?3:0,"bad",3)==0);
	CHECK(path_deliver(s,peer,&newAddress,plain,size,base+1000,XRUDP_BACKPRESSURE)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.Path.Validations && state.ReceivedConnection==(original?3u:0u));
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && path_deliver(s,peer,&otherAddress,plain,size,base+2000,XRUDP_BACKPRESSURE)==0);
	CHECK(path_deliver(s,peer,&newAddress,plain,size,base+3000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && !state.Path.Validating && address_equal(&state.Path.Active,&newAddress));
	CHECK(state.Transmit.BaseOffset==0 && state.Transmit.PendingBytes==40 && state.SentConnection==40 && !state.Sent.PendingPackets && !state.Congestion.BytesInFlight && state.Congestion.Window==XRUDP_CC_INITIAL_WINDOW && state.Rtt.SmoothedUs==30000);
	CHECK(xrtRudpSessionPoll(s,base+4000,&due)==XRUDP_OK);
	for(uint32_t i=decoded;i<pathCount;++i)CHECK(address_equal(&pathPackets[i].To,&newAddress));
	/* ACK an abandoned old-path packet: no delivery, RTT sample or growth. */
	unsigned char body[24]={0};body[5]=1;put64(body+8,2);put64(body+16,2);size=0;CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && path_deliver(s,peer,&newAddress,plain,size,base+5000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Transmit.BaseOffset==0 && state.Rtt.SmoothedUs==30000);
	size=0;CHECK(path_stream(plain,&size,original?3:0,"old",3)==0 && xrtRudpCipherSeal(peer,plain,size,packet,sizeof(packet),&packetSize)==XRUDP_OK);
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,packet,packetSize,base+6000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,packetSize,base+7000)==XRUDP_OK);
	CHECK(xrtRudpSessionRead(s,bytes,sizeof(bytes),&n)==XRUDP_OK && n==3 && memcmp(bytes,"old",3)==0);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_bounds(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0);
	unsigned char plain[1200],packet[1200];size_t size=0,n;uint64_t due;
	CHECK(path_stream(plain,&size,0,"x",1)==0 && xrtRudpCipherSeal(peer,plain,size,packet,sizeof(packet),&n)==XRUDP_OK);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,0)==XRUDP_BACKPRESSURE);
	for(uint32_t i=0;i<80;++i)CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,0)==XRUDP_BACKPRESSURE);
	for(uint64_t now=0;now<3000000;now+=10000)CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Attempts==2 && state.Path.CandidateSent<=3*n && state.Path.CandidateReceived==n && state.Path.RateDropped);
	CHECK(xrtRudpSessionPoll(s,3000000,&due)==XRUDP_OK && xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_OK && state.Path.Failures==1 && state.Path.Failure==XRUDP_TIMEOUT && !state.Path.Validating && state.Path.CooldownUntilUs==6000000);
	CHECK(xrtRudpSessionProbePath(s,&otherAddress,3000000)==XRUDP_BACKPRESSURE && xrtRudpSessionProbePath(s,&oldAddress,3000000)==XRUDP_DUPLICATE);
	CHECK(xrtRudpSessionProbePath(s,&newAddress,6000000)==XRUDP_OK);
	CHECK(xrtRudpSessionProbePath(s,&newAddress,6000000)==XRUDP_DUPLICATE && xrtRudpSessionProbePath(s,&otherAddress,6000000)==XRUDP_BACKPRESSURE);
	for(uint64_t now=6000000;now<9000000;now+=10000)CHECK(xrtRudpSessionPoll(s,now,&due)==XRUDP_OK);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Attempts==3 && state.Path.CandidateReceived==0 && state.Path.CandidateSent<=336);
	CHECK(xrtRudpSessionShutdown(s,1000)==XRUDP_OK && xrtRudpSessionProbePath(s,&newAddress,9000000)==XRUDP_CLOSED);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_active_transaction(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0);
	unsigned char plain[1200],token[16]={1},body[24]={0};size_t size=0;
	CHECK(append(plain,&size,XRUDP_FRAME_PATH_CHALLENGE,0,token,16)==0 && path_stream(plain,&size,0,"safe",4)==0);
	body[5]=1;put64(body+8,999);put64(body+16,999);CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0);
	CHECK(path_deliver(s,peer,&oldAddress,plain,size,1000,XRUDP_MALFORMED)==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.ReceivedConnection && !state.Path.Validations && !pathCount);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_output_failure(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0);pathCandidateError=true;
	CHECK(xrtRudpSessionProbePath(s,&newAddress,0)==XRUDP_OK);size_t written;uint64_t due;
	CHECK(xrtRudpSessionWrite(s,"active",6,&written)==XRUDP_OK && written==6);
	xrudpresult result=xrtRudpSessionPoll(s,0,&due);
	if(result!=XRUDP_OK)fprintf(stderr,"candidate route failure closed active session: %u\n",(unsigned)result);
	CHECK(result==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_OK && state.Path.Failures==1 && state.Path.Failure==XRUDP_IO_ERROR && !state.Path.Validating && !state.Path.Attempts && !state.Path.CandidateSent && state.SentConnection==6);
	CHECK(pathCount==1 && address_equal(&pathPackets[0].To,&oldAddress));
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);
	CHECK(path_raw(&peer,&s,32)==0);pathAllError=true;
	CHECK(xrtRudpSessionWrite(s,"active",6,&written)==XRUDP_OK && xrtRudpSessionPoll(s,0,&due)==XRUDP_IO_ERROR);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Reason==XRUDP_IO_ERROR && !state.Path.Failures);
	xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_challenge_to(xrudpcipher* peer,const xrudpaddress* to,unsigned char token[16],uint32_t* decoded)
{
	bool found=false;
	for(;*decoded<pathCount;++*decoded) {
		path_packet* p=&pathPackets[*decoded];unsigned char plain[1200];size_t size,at=0;xrudpwireframe frame;
		CHECK(xrtRudpCipherOpen(peer,p->Data,p->Size,plain,sizeof(plain),&size)==XRUDP_OK);
		while(xrtRudpFrameNext(plain,size,&at,&frame)==XRUDP_OK)if(frame.Type==XRUDP_FRAME_PATH_CHALLENGE && address_equal(&p->To,to)) { memcpy(token,frame.Data,16);found=true; }
	}
	CHECK(found);return 0;
}
static int path_fast_return(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	unsigned char ping[4]={XRUDP_FRAME_PING,0,0,0},packet[1200],held[1200],plain[1200],first[16],second[16];
	size_t n,heldSize,size;uint64_t due;uint32_t decoded=0;
	CHECK(path_deliver(s,peer,&newAddress,ping,sizeof(ping),0,XRUDP_BACKPRESSURE)==0);
	/* A reordered old-path packet remains uncommitted, but cannot start a
	 * migration after a newer path proof has been accepted. */
	CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),held,sizeof(held),&heldSize)==XRUDP_OK);
	CHECK(xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && path_challenge_to(peer,&newAddress,first,&decoded)==0);
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,first,16)==0 && path_deliver(s,peer,&newAddress,plain,size,1000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,held,heldSize,2000)==XRUDP_BACKPRESSURE);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && !state.Path.Validating);
	CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),packet,sizeof(packet),&n)==XRUDP_OK);
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,packet,n,3000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK);
	if(!state.Path.Validating)fprintf(stderr,"fresh NAT return suppressed at 3ms: validations=%llu active=%u\n",(unsigned long long)state.Path.Validations,state.Path.Active.Port);
	CHECK(state.Path.Validating && state.Path.Validations==1 && address_equal(&state.Path.Candidate,&oldAddress));
	uint32_t credit=state.Path.CandidateReceived;
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,packet,n,4000)==XRUDP_BACKPRESSURE && xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.CandidateReceived==credit);
	CHECK(xrtRudpSessionPoll(s,60000,&due)==XRUDP_OK && path_challenge_to(peer,&oldAddress,second,&decoded)==0 && memcmp(first,second,16)!=0);
	/* Even a fresh PN carrying the previous token cannot select the path. */
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,first,16)==0 && path_deliver(s,peer,&oldAddress,plain,size,61000,XRUDP_BACKPRESSURE)==0);
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,second,16)==0 && xrtRudpCipherSeal(peer,plain,size,held,sizeof(held),&heldSize)==XRUDP_OK);
	/* A matching but delayed PASSIVE response loses to newer active traffic. */
	size=0;CHECK(path_stream(plain,&size,0,"live",4)==0 && path_deliver(s,peer,&newAddress,plain,size,62000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,held,heldSize,63000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && state.Path.Validating && state.ReceivedConnection==4 && address_equal(&state.Path.Active,&newAddress));
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,second,16)==0 && path_deliver(s,peer,&oldAddress,plain,size,64000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==2 && !state.Path.Validating && address_equal(&state.Path.Active,&oldAddress));
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,65000)==XRUDP_BACKPRESSURE && xrtRudpSessionState(s,&state)==XRUDP_OK && !state.Path.Validating);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);
	puts("xrudp path return: fresh A-B-A in64ms, old nonce/token and stale passive proof cannot bounce the active path");return 0;
}
static int path_replay_after_failure(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	unsigned char ping[4]={XRUDP_FRAME_PING,0,0,0},packet[1200];size_t n;uint64_t due;
	CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),packet,sizeof(packet),&n)==XRUDP_OK && xrtRudpSessionPathInput(s,&newAddress,packet,n,0)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && xrtRudpSessionPoll(s,3000000,&due)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Failures==1 && !state.Path.Validating);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,6000000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.Path.Validating && state.Path.Failures==1);
	CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),packet,sizeof(packet),&n)==XRUDP_OK && xrtRudpSessionPathInput(s,&newAddress,packet,n,6001000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validating && state.Path.CandidateReceived==n && state.Path.Attempts==0);
	/* One captured nonce cannot regain credit even after a trusted restart. */
	CHECK(xrtRudpSessionPoll(s,9001000,&due)==XRUDP_OK && xrtRudpSessionProbePath(s,&newAddress,12001000)==XRUDP_OK);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,12002000)==XRUDP_BACKPRESSURE && xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.CandidateReceived==0);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_trusted_preference(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	uint64_t due;unsigned char token[16],plain[1200],held[1200],ping[4]={XRUDP_FRAME_PING,0,0,0};size_t size=0,n;uint32_t decoded=0;
	CHECK(xrtRudpSessionProbePath(s,&newAddress,0)==XRUDP_OK && xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && path_challenge_to(peer,&newAddress,token,&decoded)==0);
	CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && xrtRudpCipherSeal(peer,plain,size,held,sizeof(held),&n)==XRUDP_OK);
	CHECK(path_deliver(s,peer,&oldAddress,ping,sizeof(ping),1000,XRUDP_OK)==0 && xrtRudpSessionPathInput(s,&newAddress,held,n,2000)==XRUDP_OK);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && address_equal(&state.Path.Active,&newAddress));
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_idle_probe_deadline(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	unsigned char token[16]={7},plain[1200],body[24]={0};size_t size=0;uint64_t due;uint32_t decoded=0;
	CHECK(append(plain,&size,XRUDP_FRAME_PATH_CHALLENGE,0,token,16)==0 && path_deliver(s,peer,&oldAddress,plain,size,0,XRUDP_OK)==0);
	CHECK(xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && xrtRudpSessionPoll(s,25000,&due)==XRUDP_OK && pathCount==2);
	for(;decoded<pathCount;++decoded) { size_t n;CHECK(xrtRudpCipherOpen(peer,pathPackets[decoded].Data,pathPackets[decoded].Size,plain,sizeof(plain),&n)==XRUDP_OK); }
	body[5]=1;put64(body+8,2);put64(body+16,3);size=0;
	CHECK(append(plain,&size,XRUDP_FRAME_ACK,0,body,24)==0 && path_deliver(s,peer,&oldAddress,plain,size,26000,XRUDP_OK)==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.Sent.PendingPackets && !state.Sent.BytesInFlight);
	CHECK(xrtRudpSessionProbePath(s,&newAddress,26000)==XRUDP_OK && xrtRudpSessionPoll(s,26000,&due)==XRUDP_OK && pathCount==2);
	if(due>50000)fprintf(stderr,"idle probe slept past rate limit: due=%llu expected=50000\n",(unsigned long long)due);
	CHECK(due==50000 && xrtRudpSessionPoll(s,due,&due)==XRUDP_OK && pathCount>=3);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Attempts==1 && state.Path.CandidateSent>0);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_busy_candidate(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	unsigned char plain[1200],token[16];size_t size=0;uint64_t due;uint32_t decoded=0;
	CHECK(path_stream(plain,&size,0,"busy",4)==0);
	for(uint32_t i=0;i<40;++i)CHECK(path_deliver(s,peer,&newAddress,plain,size,0,XRUDP_BACKPRESSURE)==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.RateDropped && !state.ReceivedConnection);
	CHECK(xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && path_challenge_to(peer,&newAddress,token,&decoded)==0);
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0);
	CHECK(path_deliver(s,peer,&newAddress,plain,size,1000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && !state.ReceivedConnection);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_control_budget(void)
{
	xrudpcipher* peer;xrudpsession* s;CHECK(path_raw(&peer,&s,32)==0 && xrtMemDebugFailAfter(0));
	unsigned char plain[1200],packet[1200],token[16],wrong[16];size_t size=0,n;uint64_t due;uint32_t decoded=0;
	CHECK(xrtRudpSessionProbePath(s,&newAddress,0)==XRUDP_OK && xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && path_challenge_to(peer,&newAddress,token,&decoded)==0);
	memcpy(wrong,token,16);wrong[0]^=1;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,wrong,16)==0);
	CHECK(xrtRudpCipherSeal(peer,plain,size,packet,sizeof(packet),&n)==XRUDP_OK && packet[8]==XRUDP_PACKET_PATH);
	packet[n-1]^=1;
	for(uint32_t i=0;i<8;++i)CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,1000)==XRUDP_AUTH_FAILED);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,packet,n,1000)==XRUDP_BACKPRESSURE);
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && path_deliver(s,peer,&newAddress,plain,size,1000,XRUDP_BACKPRESSURE)==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Invalid==8 && state.Path.RateDropped==2 && state.Path.DeadlineUs==3000000 && !state.Path.CandidateReceived);
	CHECK(path_deliver(s,peer,&newAddress,plain,size,1000000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && state.Path.CandidateReceived==0);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
static int path_deferred_active_evidence(void)
{
	memset(&net,0,sizeof(net));pathCount=0;pathBlock=pathCandidateError=pathAllError=false;
	xrudpsessionconfig config;configure(&config,256,512);config.StreamCapacity=config.Local.MaxStreams=config.Peer.MaxStreams=2;
	xrudpcipher* peer;xrudpsession* s;CHECK(raw_session(&peer,&s,&config)==0 && xrtRudpSessionEnablePaths(s,&oldAddress,path_send)==XRUDP_OK && xrtMemDebugFailAfter(0));
	unsigned char ping[4]={XRUDP_FRAME_PING,0,0,0},plain[1200],held[1200],token[16],body[16]={0},bad[1200];size_t size=0,n,badSize;uint64_t due;uint32_t decoded=0;
	CHECK(path_deliver(s,peer,&newAddress,ping,sizeof(ping),0,XRUDP_BACKPRESSURE)==0 && xrtRudpSessionPoll(s,0,&due)==XRUDP_OK && path_challenge_to(peer,&newAddress,token,&decoded)==0);
	CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && xrtRudpCipherSeal(peer,plain,size,held,sizeof(held),&n)==XRUDP_OK);
	/* PN4 is authentic current-path traffic, deferred until remote OPEN2.
	 * PN3's valid candidate response was produced before that newer evidence. */
	put32(body,2);memcpy(body+12,"busy",4);size=0;CHECK(append(plain,&size,XRUDP_FRAME_STREAM,0,body,sizeof(body))==0);
	CHECK(path_deliver(s,peer,&oldAddress,plain,size,1000,XRUDP_BACKPRESSURE)==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && !state.ReceivedPackets && !state.ReceivedConnection && !state.AckPending && state.ActiveStreams==1 && state.LastActivityUs==0 && state.ProgressAtUs==0);
	CHECK(xrtRudpSessionPathInput(s,&newAddress,held,n,2000)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validating && !state.Path.Validations && state.Path.DeadlineUs==3000000 && address_equal(&state.Path.Active,&oldAddress));
	/* A forged higher-numbered packet cannot poison this watermark. */
	CHECK(xrtRudpCipherSeal(peer,plain,size,bad,sizeof(bad),&badSize)==XRUDP_OK);bad[badSize-1]^=1;
	CHECK(xrtRudpSessionPathInput(s,&oldAddress,bad,badSize,2500)==XRUDP_AUTH_FAILED);
	size=0;CHECK(append(plain,&size,XRUDP_FRAME_PATH_RESPONSE,0,token,16)==0 && path_deliver(s,peer,&newAddress,plain,size,3000,XRUDP_OK)==0);
	CHECK(xrtRudpSessionState(s,&state)==XRUDP_OK && state.Path.Validations==1 && address_equal(&state.Path.Active,&newAddress) && !state.ReceivedConnection && state.ActiveStreams==1);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(s);xrtRudpCipherDestroy(peer);return 0;
}
int main(void)
{
	CHECK(path_quarantine_and_migration(false,32)==0 && path_quarantine_and_migration(true,32)==0 && path_quarantine_and_migration(false,1)==0 && path_bounds()==0 && path_active_transaction()==0 && path_output_failure()==0);
	CHECK(path_fast_return()==0 && path_replay_after_failure()==0 && path_trusted_preference()==0);
	CHECK(path_idle_probe_deadline()==0 && path_busy_candidate()==0 && path_control_budget()==0 && path_deferred_active_evidence()==0);
	puts("xrudp paths: address quarantine, fresh-byte amplification, fixed deadlines, real migration and conservative recovery passed");return 0;
}
