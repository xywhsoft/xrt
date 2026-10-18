#define TEST_SESSION_FAULTS
#define main session_foundation_main
#define send_packet foundation_send
#include "test_session.c"
#undef send_packet
#undef main

static uint32_t wireLimit[2], sizeDrops[2];
static bool addressed;
static xrudpaddress mtuAddress[2]={{4,40000,0,{192,0,2,1}},{4,40001,0,{192,0,2,2}}};
static xrudpresult mtu_send(const void* data, size_t size, void* context)
{
	session_output* out = (session_output*)context;
	if (size > wireLimit[out->Index]) { ++sizeDrops[out->Index]; ++out->Calls; return XRUDP_OK; }
	return foundation_send(data, size, context);
}
static xrudpresult mtu_path_send(const void* data,size_t size,const xrudpaddress* destination,void* context)
{
	(void)destination;return mtu_send(data,size,context);
}
static int mtu_pair(uint32_t ceiling, uint32_t slots, uint32_t limit)
{
	memset(&net, 0, sizeof(net)); memset(sizeDrops, 0, sizeof(sizeDrops)); tick = 0; addressed = false; mtuAddress[1].Port=40001;
	xrudpcipher* cipher[2]; CHECK(raw_pair(cipher) == 0);
	xrudpsessionconfig config; configure(&config, 65536, ceiling); config.DiscoverMtu = true;
	config.PacketCapacity = slots; config.Local.MaxStreams = config.Peer.MaxStreams = config.StreamCapacity = 2;
	config.Local.MaxMessageSize = config.Peer.MaxMessageSize = 8192;
	xrtRudpDatagramConfigInit(&config.Datagrams); config.Datagrams.Channels = 3; config.Datagrams.SendCapacity = 256;
	for (uint32_t i = 0; i < 2; ++i) {
		wireLimit[i] = limit; net.Output[i].Index = i; config.Role = i ? XRUDP_RESPONDER : XRUDP_INITIATOR;
		CHECK(xrtRudpSessionCreate(&config, cipher[i], 0, mtu_send, &net.Output[i], &net.Session[i]) == XRUDP_OK);
		for (uint32_t channel = 1; channel <= 3; ++channel) {
			xrudpdatagramchannel c; xrtRudpDatagramChannelInit(&c); c.Id = channel; c.Mode = (uint8_t)(channel - 1);
			CHECK(xrtRudpSessionDatagramConfigure(net.Session[i], &c) == XRUDP_OK);
		}
	}
	return 0;
}
static int step(void)
{
	uint64_t now = (uint64_t)tick * 1000, due;
	for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used && net.Queue[i].Due <= tick) {
		session_packet p = net.Queue[i]; net.Queue[i].Used = false;
		xrudpresult r = addressed ? xrtRudpSessionPathInput(net.Session[p.Target], &mtuAddress[1-p.Target], p.Data, p.Size, now) : xrtRudpSessionInput(net.Session[p.Target], p.Data, p.Size, now);
		CHECK(r == XRUDP_OK || r == XRUDP_DUPLICATE || r == XRUDP_TOO_OLD || (addressed && r == XRUDP_BACKPRESSURE));
	}
	for (uint32_t i = 0; i < 2; ++i) {
		size_t before = net.Output[i].Calls;
		CHECK(xrtRudpSessionPoll(net.Session[i], now, &due) == XRUDP_OK && due > now);
		CHECK(net.Output[i].Calls - before <= XRUDP_SESSION_POLL_BURST);
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && state.Sent.BytesInFlight == state.Congestion.BytesInFlight && state.OutstandingProbes <= 2);
	}
	CHECK(!net.Overflow); return 0;
}
static int grow(uint32_t ceiling, uint32_t limit, uint32_t expected)
{
	CHECK(mtu_pair(ceiling, 1, limit) == 0 && xrtMemDebugFailAfter(0));
	for (tick = 1; tick < 12000; ++tick) CHECK(step() == 0);
	for (uint32_t i = 0; i < 2; ++i) {
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK);
		CHECK(state.Mtu.Enabled && state.Mtu.BaseConfirmed && state.Mtu.Effective == expected && state.Mtu.Failure == XRUDP_OK && !state.Mtu.ProbePacket);
		CHECK(state.Mtu.ProbesAcknowledged > 0 && state.DatagramSendLimit == expected - XRUDP_DATAGRAM_OVERHEAD);
		printf("MTU growth ceiling=%u limit=%u side=%u effective=%u sent=%llu ack=%llu fail=%llu attempts=%u drop=%u next=%llu\n", ceiling,limit,i,state.Mtu.Effective,(unsigned long long)state.Mtu.ProbesSent,(unsigned long long)state.Mtu.ProbesAcknowledged,(unsigned long long)state.Mtu.ProbeFailures,state.Mtu.Attempts,sizeDrops[i],(unsigned long long)state.Mtu.NextProbeUs);
		if (expected < ceiling) CHECK(state.Mtu.ProbeFailures == 3 && sizeDrops[i] == 3 && state.Mtu.NextProbeUs > 60000000);
		else CHECK(state.Mtu.ProbeFailures == 0 && state.Mtu.NextProbeUs == UINT64_MAX);
		CHECK(!state.Mtu.Fallbacks && !state.LostPackets && state.Congestion.Window == XRUDP_CC_INITIAL_WINDOW);
	}
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair(); return 0;
}
static int blocked(void)
{
	CHECK(mtu_pair(1200, 1, 1200) == 0 && xrtMemDebugFailAfter(0));
	net.Output[0].Block = net.Output[1].Block = true;
	for (tick = 1; tick < 700; ++tick) CHECK(step() == 0);
	for (uint32_t i = 0; i < 2; ++i) {
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && !state.Mtu.Attempts && !state.Mtu.ProbesSent && !state.SentPackets && state.OutputBackpressure > 0);
	}
	net.Output[0].Block = net.Output[1].Block = false;
	for (; tick < 8000; ++tick) CHECK(step() == 0);
	for (uint32_t i = 0; i < 2; ++i) { xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && state.Mtu.Effective == 1200 && state.Mtu.ProbesSent == 4); }
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair(); return 0;
}
static int shrink(uint32_t slots)
{
	CHECK(mtu_pair(1200, slots, 1200) == 0 && xrtMemDebugFailAfter(0));
	for (tick = 1; tick < 8000; ++tick) CHECK(step() == 0);
	uint32_t started = tick;
	xrudpsessionstate warm; CHECK(xrtRudpSessionState(net.Session[0], &warm) == XRUDP_OK && warm.Mtu.Effective == 1200);
	unsigned char source[32768], message[8192], game[800];
	for (size_t i = 0; i < sizeof(source); ++i) source[i] = (unsigned char)(i*37+(i>>8));
	for (size_t i = 0; i < sizeof(message); ++i) message[i] = (unsigned char)(i*71+(i>>7));
	memset(game, 0x5a, sizeof(game)); uint64_t seq;
	for (uint32_t i = 0; i < 256; ++i) { put32(game, i); CHECK(xrtRudpSessionDatagramWrite(net.Session[0], 1+i%3, game, sizeof(game), (uint64_t)tick*1000, 0, &seq) == XRUDP_OK); }
	size_t written; CHECK(xrtRudpSessionWrite(net.Session[0], source, sizeof(source), &written) == XRUDP_OK && written == sizeof(source));
	CHECK(xrtRudpSessionFinish(net.Session[0]) == XRUDP_OK);
	xrudpstreamconfig config; xrtRudpStreamConfigInit(&config); config.Kind = XRUDP_STREAM_MESSAGES; uint32_t id;
	CHECK(xrtRudpSessionStreamOpen(net.Session[0], &config, &id) == XRUDP_OK && id == 3);
	CHECK(xrtRudpSessionMessageWrite(net.Session[0], id, message, sizeof(message)) == XRUDP_OK && xrtRudpSessionStreamFinish(net.Session[0], id) == XRUDP_OK);
	wireLimit[0] = wireLimit[1] = 512;
	size_t read = 0, reverseRead = 0, reverseWritten = 0; bool gotMessage = false, fell = false; uint32_t gamesRead = 0;
	uint64_t fallbackAt = 0, lastActivity = 0;
	for (; tick < started + 16000; ++tick) {
		if (tick % 10 == 0) { unsigned char small[8] = {0}; size_t n; CHECK(xrtRudpSessionWrite(net.Session[1], small, sizeof(small), &n) == XRUDP_OK && n == sizeof(small)); reverseWritten += n; }
		CHECK(step() == 0);
		unsigned char output[8192]; size_t n; xrudpresult r = xrtRudpSessionRead(net.Session[1], output, sizeof(output), &n);
		CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE || r == XRUDP_DONE);
		CHECK(read+n <= sizeof(source) && memcmp(output, source+read, n) == 0); read += n;
		r = xrtRudpSessionRead(net.Session[0], output, sizeof(output), &n); CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE); reverseRead += n;
		xrudpdatagraminfo info; r = xrtRudpSessionDatagramRead(net.Session[1], output, sizeof(output), (uint64_t)tick*1000, &info);
		CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE);
		if (r == XRUDP_OK) { CHECK(fell && ++gamesRead == 1 && info.Channel == 2 && info.Size == 5 && memcmp(output,"small",5) == 0); }
		if (!gotMessage) {
			r = xrtRudpSessionMessageRead(net.Session[1], id, output, sizeof(output), &n);
			CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE || r == XRUDP_ARGUMENT);
			if (r == XRUDP_OK) { CHECK(n == sizeof(message) && memcmp(output,message,n) == 0); gotMessage = true; }
		}
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[0], &state) == XRUDP_OK);
		if (!fell && state.Mtu.Fallbacks) {
			fell = true; fallbackAt = (uint64_t)tick*1000; lastActivity = state.LastActivityUs;
			CHECK(state.Mtu.Effective == 512 && state.DatagramSendLimit == 424 && state.Datagrams.SendSizeDropped > 0 && state.Datagrams.SendQueued == 0);
			seq = 999; CHECK(xrtRudpSessionDatagramWrite(net.Session[0], 2, game, 425, fallbackAt, 0, &seq) == XRUDP_LIMIT && seq == 999);
			CHECK(xrtRudpSessionDatagramWrite(net.Session[0], 2, "small", 5, fallbackAt, 0, &seq) == XRUDP_OK);
		}
		if (fell && gamesRead == 1 && gotMessage && read == sizeof(source) && state.WriteDone && state.SentConnection == sizeof(source)+sizeof(message)+4) break;
	}
	CHECK(tick < started+16000 && fell && fallbackAt < (uint64_t)started*1000+4000000 && fallbackAt-lastActivity < 100000 && reverseRead > 0 && reverseRead <= reverseWritten && sizeDrops[0] > 0);
	xrudpsessionstate final; CHECK(xrtRudpSessionState(net.Session[0], &final) == XRUDP_OK && final.Mtu.BaseConfirmed && final.Mtu.Effective == 512 && final.Mtu.RaiseAfterUs > 60000000);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair();
	printf("xrudp MTU shrink: slots=%u fallback=%llu us, reliable bytes/messages complete while reverse small traffic stays live\n", slots, (unsigned long long)fallbackAt); return 0;
}
static int unsupported(void)
{
	CHECK(mtu_pair(1200, 1, 480) == 0 && xrtMemDebugFailAfter(0));
	bool failed = false;
	for (tick = 1; tick < 12000 && !failed; ++tick) {
		uint64_t now = (uint64_t)tick*1000, due;
		for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used && net.Queue[i].Due <= tick) { session_packet p = net.Queue[i]; net.Queue[i].Used = false; xrudpresult r = xrtRudpSessionInput(net.Session[p.Target], p.Data, p.Size, now); CHECK(good(r) || r == XRUDP_TOO_OLD); }
		for (uint32_t i = 0; i < 2; ++i) {
			xrudpresult r = xrtRudpSessionPoll(net.Session[i], now, &due);
			if (r == XRUDP_TIMEOUT) { xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && state.Mtu.Failure == XRUDP_TIMEOUT && state.Mtu.ProbesSent == 3 && !state.Mtu.BaseConfirmed); failed = true; break; }
			CHECK(r == XRUDP_OK);
		}
	}
	CHECK(failed && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair(); return 0;
}
static int migrate(void)
{
	CHECK(mtu_pair(1200,32,1200)==0);addressed=true;
	for(uint32_t i=0;i<2;++i)CHECK(xrtRudpSessionEnablePaths(net.Session[i],&mtuAddress[1-i],mtu_path_send)==XRUDP_OK);
	CHECK(xrtMemDebugFailAfter(0));
	for(tick=1;tick<8000;++tick)CHECK(step()==0);
	xrudpsessionstate state;CHECK(xrtRudpSessionState(net.Session[0],&state)==XRUDP_OK && state.Mtu.Effective==1200);
	unsigned char payload[800]={0};uint64_t seq;
	for(uint32_t i=0;i<256;++i)CHECK(xrtRudpSessionDatagramWrite(net.Session[0],1+i%3,payload,sizeof(payload),(uint64_t)tick*1000,0,&seq)==XRUDP_OK);
	mtuAddress[1].Port=41001;wireLimit[0]=wireLimit[1]=768;
	CHECK(xrtRudpSessionProbePath(net.Session[0],&mtuAddress[1],(uint64_t)tick*1000)==XRUDP_OK);
	bool migrated=false;
	for(;tick<9000;++tick) {
		CHECK(step()==0 && xrtRudpSessionState(net.Session[0],&state)==XRUDP_OK);
		if(state.Path.Validations) {
			CHECK(state.Path.Validations==1 && state.Mtu.Effective==512 && !state.Mtu.BaseConfirmed && !state.Mtu.ProbePacket && !state.Mtu.Failure && state.DatagramSendLimit==424 && state.Datagrams.SendSizeDropped>0);
			migrated=true;++tick;break;
		}
	}
	CHECK(migrated);
	for(;tick<20000;++tick)CHECK(step()==0);
	CHECK(xrtRudpSessionState(net.Session[0],&state)==XRUDP_OK && state.Mtu.BaseConfirmed && state.Mtu.Effective==768 && state.Path.Validations==1);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();destroy_pair();return 0;
}
int main(void)
{
	xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
	CHECK(grow(1200,512,512) == 0 && grow(1200,768,768) == 0 && grow(1200,1024,1024) == 0 && grow(1200,1200,1200) == 0 && grow(700,700,700) == 0);
	CHECK(blocked() == 0 && shrink(32) == 0 && shrink(1) == 0 && unsupported() == 0 && migrate() == 0);
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes && before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
	puts("xrudp session MTU: padded ACK proof, failed growth, base failure, backpressure, size black hole and zero I/O allocations passed"); return 0;
}
