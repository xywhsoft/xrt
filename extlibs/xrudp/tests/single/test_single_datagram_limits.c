#define main xrudp_datagram_reference_main
#include "test_single_datagram.c"
#undef main

int main(void)
{
	xrudpdatagramconfig cfg = {1,2,2,8}; xrudpdatagrams* d; uint64_t sequence; xrudpdatagramview view;
	xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
	CHECK(xrtRudpDatagramCreate(&cfg,&d) == XRUDP_OK && configure(d,1,XRUDP_DATAGRAM_LATEST,true,10) == 0);
	d->Channel[0].SendSequence = UINT64_MAX-1;
	CHECK(xrtRudpDatagramWrite(d,1,"last",4,0,0,&sequence) == XRUDP_OK && sequence == UINT64_MAX);
	xrudpdatagramstate state, saved; CHECK(xrtRudpDatagramState(d,&saved) == XRUDP_OK);
	sequence = 99; CHECK(xrtRudpDatagramWrite(d,1,"wrap",4,0,0,&sequence) == XRUDP_LIMIT && sequence == 99);
	CHECK(xrtRudpDatagramState(d,&state) == XRUDP_OK && memcmp(&state,&saved,sizeof(state)) == 0);
	CHECK(xrtRudpDatagramPeekSend(d,&view) == XRUDP_OK && memcmp(view.Data,"last",4) == 0);
	CHECK(xrtRudpDatagramSent(d,view.Info.Ticket,92) == XRUDP_OK);
	d->Channel[0].SendSequence = 0; d->SendTicket = UINT64_MAX;
	CHECK(xrtRudpDatagramWrite(d,1,NULL,0,0,0,&sequence) == XRUDP_LIMIT);
	CHECK(xrtRudpDatagramInput(d,1,true,1,"secret!!",8,UINT64_MAX-2) == XRUDP_OK);
	xrudpdatagraminfo info; CHECK(xrtRudpDatagramPeek(d,&info) == XRUDP_OK && info.DeadlineUs == UINT64_MAX);
	CHECK(xrtRudpDatagramInput(d,1,true,2,"ok",2,UINT64_MAX-2) == XRUDP_OK);
	uint32_t slot = d->Channel[0].Latest; unsigned char* payload = xrudp__dg_data(d,slot);
	CHECK(payload[0] == 'o' && payload[1] == 'k'); for (size_t i=2;i<8;++i) CHECK(payload[i] == 0);
	CHECK(xrtRudpDatagramMaintain(d,UINT64_MAX) == XRUDP_ARGUMENT);
	CHECK(xrtRudpDatagramAbort(d) == XRUDP_OK); for (size_t i=0;i<8;++i) CHECK(payload[i] == 0);
	xrtRudpDatagramDestroy(d);
	cfg.Channels = XRUDP_DATAGRAM_CHANNELS_MAX; cfg.SendCapacity = cfg.ReceiveCapacity = XRUDP_DATAGRAM_QUEUE_MAX; cfg.PayloadCapacity = XRUDP_DATAGRAM_PAYLOAD_MAX;
	CHECK(xrtRudpDatagramCreate(&cfg,&d) == XRUDP_OK);
	for (uint32_t i=1;i<=cfg.Channels;++i) CHECK(configure(d,i,XRUDP_DATAGRAM_INDEPENDENT,false,0) == 0);
	unsigned char bytes[XRUDP_STREAM_CHUNK_MAX]; memset(bytes,0xab,sizeof(bytes)); CHECK(xrtMemDebugFailAfter(0));
	for (uint32_t i=0;i<XRUDP_DATAGRAM_QUEUE_MAX;++i) {
		CHECK(xrtRudpDatagramWrite(d,1+i%cfg.Channels,bytes,cfg.PayloadCapacity,1,0,&sequence) == XRUDP_OK);
		CHECK(xrtRudpDatagramInput(d,1+i%cfg.Channels,false,0,bytes,cfg.PayloadCapacity,1) == XRUDP_OK);
	}
	CHECK(xrtRudpDatagramWrite(d,1,bytes,1,1,0,&sequence) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpDatagramInput(d,1,false,0,bytes,sizeof(bytes),1) == XRUDP_OK);
	CHECK(xrtRudpDatagramState(d,&state) == XRUDP_OK && state.SendQueued == 256 && state.ReceiveQueued == 256 &&
		state.SendBytes == 256*XRUDP_DATAGRAM_PAYLOAD_MAX && state.ReceiveBytes == state.SendBytes && state.DroppedCapacity == 1);
	CHECK(xrtRudpDatagramAbort(d) == XRUDP_OK);
	for (size_t i=0;i<(size_t)512*cfg.PayloadCapacity;++i) CHECK(d->Data[i] == 0);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpDatagramDestroy(d);
	xrtClearError(); xrtMemDebugSnapshot(&after); CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
	CHECK(before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
	puts("xrudp datagram limits: no counter/clock wrap, no stale plaintext, maximum channels and full fixed queues balance"); return 0;
}
