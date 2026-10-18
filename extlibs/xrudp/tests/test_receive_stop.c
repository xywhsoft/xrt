#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while (0)
static unsigned char payload[256];

static int zero_plaintext(xrudpreceive* r)
{
#ifdef XRUDP_IMPLEMENTATION
	for(size_t i=0;i<r->Capacity+r->Capacity/8;++i)CHECK(r->Data[i]==0);
#else
	(void)r;
#endif
	return 0;
}
static int boundaries(void)
{
	xrudpreceive* r; xrudpreceivechange change,saved; xrudpreceivestate state,before;
	unsigned char output[256];size_t count;
	CHECK(xrtRudpReceiveCreate(256,256,&r)==XRUDP_OK);
	CHECK(xrtRudpReceivePush(r,0,payload,20,false,&change)==XRUDP_OK);
	CHECK(xrtRudpReceiveRead(r,output,7,&count)==XRUDP_OK && count==7);
	CHECK(xrtRudpReceivePush(r,100,payload,20,false,&change)==XRUDP_OK);
	CHECK(xrtRudpReceiveStop(r,42,&change)==XRUDP_OK && change.ConsumedDelta==113 && change.HighestDelta==0 && change.NewBytes==0 && change.NewFinal==0);
	CHECK(xrtRudpReceiveState(r,&state)==XRUDP_OK && state.Stopped && state.StopReason==42 && !state.Reset && !state.HasFinal);
	CHECK(state.ReadOffset==120 && state.HighestOffset==120 && state.Limit==256 && !state.BufferedBytes && xrtRudpReceiveReady(r));
	CHECK(zero_plaintext(r)==0);
	memset(output,0xa5,sizeof(output));CHECK(xrtRudpReceiveRead(r,output,sizeof(output),&count)==XRUDP_CANCELLED && count==0 && output[0]==0xa5);
	CHECK(xrtRudpReceivePeek(r,120,output,sizeof(output),&count)==XRUDP_CANCELLED && count==0);
	CHECK(xrtRudpReceiveStop(r,42,&change)==XRUDP_DUPLICATE && change.ConsumedDelta==0);
	memset(&change,0xa5,sizeof(change));saved=change;before=state;
	CHECK(xrtRudpReceiveStop(r,43,&change)==XRUDP_ARGUMENT && memcmp(&change,&saved,sizeof(change))==0);
	CHECK(xrtRudpReceiveExtend(r,257)==XRUDP_CANCELLED && xrtRudpReceiveRestart(r,256)==XRUDP_BACKPRESSURE);
	CHECK(xrtRudpReceiveState(r,&state)==XRUDP_OK && memcmp(&state,&before,sizeof(state))==0);
	CHECK(xrtRudpReceivePush(r,255,payload,2,false,&change)==XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r,UINT64_MAX,payload,1,false,&change)==XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r,119,NULL,0,true,&change)==XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r,200,payload,20,false,&change)==XRUDP_OK && change.HighestDelta==100 && change.ConsumedDelta==100 && change.NewBytes==0);
	CHECK(xrtRudpReceivePush(r,0,payload,256,false,&change)==XRUDP_OK && change.HighestDelta==36 && change.ConsumedDelta==36);
	CHECK(xrtRudpReceivePush(r,0,output,256,false,&change)==XRUDP_DUPLICATE && change.ConsumedDelta==0);
	CHECK(zero_plaintext(r)==0 && xrtRudpReceiveReset(r,255,7,&change)==XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r,256,7,&change)==XRUDP_OK && change.ConsumedDelta==0);
	CHECK(xrtRudpReceiveState(r,&state)==XRUDP_OK && state.Reset && state.ResetReason==7 && state.Stopped && state.StopReason==42);
	CHECK(xrtRudpReceiveRestart(r,256)==XRUDP_OK && !xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceiveStop(r,9,&change)==XRUDP_OK && change.ConsumedDelta==0);
	CHECK(xrtRudpReceiveRestart(r,256)==XRUDP_BACKPRESSURE); /* Fresh stop is not quiescent without a real final. */
	CHECK(xrtRudpReceivePush(r,0,NULL,0,true,&change)==XRUDP_OK && change.NewFinal && change.ConsumedDelta==0);
	CHECK(xrtRudpReceiveRead(r,output,1,&count)==XRUDP_CANCELLED && xrtRudpReceiveRestart(r,256)==XRUDP_OK);
	CHECK(xrtRudpReceivePush(r,0,NULL,0,true,&change)==XRUDP_OK);
	CHECK(xrtRudpReceiveStop(r,7,&change)==XRUDP_DUPLICATE && xrtRudpReceiveRead(r,output,1,&count)==XRUDP_DONE);
	CHECK(xrtRudpReceiveRestart(r,256)==XRUDP_OK && xrtRudpReceiveReset(r,10,8,&change)==XRUDP_OK);
	CHECK(xrtRudpReceiveStop(r,7,&change)==XRUDP_DUPLICATE);
	CHECK(xrtRudpReceiveState(r,&state)==XRUDP_OK && !state.Stopped && state.ResetReason==8);
	xrtRudpReceiveDestroy(r);return 0;
}
static int transaction(void)
{
	xrudpreceive *a,*b;xrudpreceivechange change;xrudpreceivebatchchange batch,saved;
	xrudpreceivestate state,beforeA,beforeB;xrudpresult result;
	CHECK(xrtRudpReceiveCreate(256,256,&a)==XRUDP_OK && xrtRudpReceiveCreate(256,256,&b)==XRUDP_OK);
	CHECK(xrtRudpReceivePush(a,90,payload,10,false,&change)==XRUDP_OK);
	CHECK(xrtRudpReceiveStop(a,7,&change)==XRUDP_OK && change.ConsumedDelta==100);
	xrudpreceiveframe frames[4]={{a,120,payload,10,false},{a,125,payload+30,20,false},{a,140,payload,10,false},{b,0,payload,3,false}};
	CHECK(xrtRudpReceiveState(a,&beforeA)==XRUDP_OK && xrtRudpReceiveState(b,&beforeB)==XRUDP_OK);
	memset(&batch,0xa5,sizeof(batch));saved=batch;
	CHECK(xrtRudpReceivePushBatch(frames,4,52,&batch)==XRUDP_MALFORMED && memcmp(&batch,&saved,sizeof(batch))==0);
	CHECK(xrtRudpReceiveState(a,&state)==XRUDP_OK && memcmp(&state,&beforeA,sizeof(state))==0);
	CHECK(xrtRudpReceiveState(b,&state)==XRUDP_OK && memcmp(&state,&beforeB,sizeof(state))==0);
	CHECK(xrtRudpReceiveCheckBatch(frames,4,53,&batch)==XRUDP_OK && batch.HighestDelta==53 && batch.ConsumedDelta==50 && batch.NewBytes==3 && batch.Streams==2);
	CHECK(xrtRudpReceiveState(a,&state)==XRUDP_OK && memcmp(&state,&beforeA,sizeof(state))==0);
	frames[3].Offset=256;
	CHECK(xrtRudpReceivePushBatch(frames,4,53,&batch)==XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveState(a,&state)==XRUDP_OK && memcmp(&state,&beforeA,sizeof(state))==0);
	frames[3].Offset=0;
	CHECK(xrtRudpReceivePushBatch(frames,4,53,&batch)==XRUDP_OK && batch.HighestDelta==53 && batch.ConsumedDelta==50 && batch.NewBytes==3);
	CHECK(xrtRudpReceiveState(a,&state)==XRUDP_OK && state.ReadOffset==150 && state.HighestOffset==150 && !state.HasFinal && !state.BufferedBytes && state.Limit==256);
	CHECK(zero_plaintext(a)==0);
	result=xrtRudpReceivePushBatch(frames,4,0,&batch);CHECK(result==XRUDP_DUPLICATE && !batch.HighestDelta && !batch.ConsumedDelta && !batch.NewBytes);
	frames[0]=(xrudpreceiveframe){a,149,NULL,0,true};
	CHECK(xrtRudpReceivePushBatch(frames,1,0,&batch)==XRUDP_MALFORMED);
	frames[0]=(xrudpreceiveframe){a,150,NULL,0,true};
	CHECK(xrtRudpReceivePushBatch(frames,1,0,&batch)==XRUDP_OK && batch.NewFinals==1 && !batch.ConsumedDelta);
	CHECK(xrtRudpReceiveRestart(a,256)==XRUDP_OK);
	xrtRudpReceiveDestroy(a);xrtRudpReceiveDestroy(b);return 0;
}
static uint32_t rng=0x9e54731u;
static uint32_t next(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static int model(void)
{
	xrudpreceive* r;CHECK(xrtRudpReceiveCreate(256,256,&r)==XRUDP_OK);
	for(size_t run=0;run<128;++run) {
		xrudpreceivechange single;CHECK(xrtRudpReceiveStop(r,7,&single)==XRUDP_OK);
		uint64_t high=0,total=0;
		for(size_t step=0;step<64;++step) {
			xrudpreceiveframe frames[4];size_t count=1+next()%4;uint64_t maximum=high;
			for(size_t j=0;j<count;++j) {
				uint64_t offset=next()%(high+8);if(offset>256)offset=256;
				size_t size=next()%8;if(size>256-offset)size=(size_t)(256-offset);
				frames[j]=(xrudpreceiveframe){r,offset,payload+j,size,false};if(offset+size>maximum)maximum=offset+size;
			}
			uint64_t delta=maximum-high;xrudpreceivebatchchange batch;
			if(delta)CHECK(xrtRudpReceivePushBatch(frames,count,delta-1,&batch)==XRUDP_MALFORMED);
			xrudpresult expected=delta?XRUDP_OK:XRUDP_DUPLICATE;
			CHECK(xrtRudpReceivePushBatch(frames,count,delta,&batch)==expected && batch.HighestDelta==delta && batch.ConsumedDelta==delta && batch.NewBytes==0);
			total+=batch.ConsumedDelta;high=maximum;xrudpreceivestate state;
			CHECK(xrtRudpReceiveState(r,&state)==XRUDP_OK && state.HighestOffset==high && state.ReadOffset==high && total==high && !state.BufferedBytes && !state.HasFinal && !state.Reset && state.Limit==256 && state.Stopped);
		}
		CHECK(xrtRudpReceiveReset(r,256,1,&single)==XRUDP_OK && single.ConsumedDelta==256-total);
		CHECK(zero_plaintext(r)==0 && xrtRudpReceiveRestart(r,256)==XRUDP_OK);
	}
	xrtRudpReceiveDestroy(r);return 0;
}
int main(void)
{
	for(size_t i=0;i<sizeof(payload);++i)payload[i]=(unsigned char)(i*37+5);
	CHECK(boundaries()==0 && transaction()==0 && model()==0);
	puts("xrudp receive stop: no plaintext/early reuse, frozen credit, real final-size validation and 8192-step discard accounting passed");return 0;
}
