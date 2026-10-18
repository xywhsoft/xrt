#include <xrudp.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
static xrudpaddress server={4,3478,0,{192,0,2,1}},mapped={4,42000,0,{198,51,100,2}},other={4,3479,0,{192,0,2,1}};
static int request(xrudpdiscovery* d,uint64_t now,unsigned char wire[512],xrudpstunmessage* m)
{
	size_t n;uint64_t due;CHECK(xrtRudpDiscoveryNext(d,now,wire,512,&n,&due)==XRUDP_OK && xrtRudpStunDecode(wire,n,m)==XRUDP_OK && m->Type==XRUDP_STUN_BINDING_REQUEST);return 0;
}
static int schedule(void)
{
	xrudpdiscovery d;CHECK(xrtRudpDiscoveryInit(&d,&server,0,0,false)==XRUDP_OK && xrtMemDebugFailAfter(0));
	const uint64_t times[]={0,500000,1500000,3500000,7500000,15500000,31500000};unsigned char wire[512],original[512];xrudpstunmessage m;size_t n;uint64_t next;
	CHECK(xrtRudpDiscoverySent(&d,0)==XRUDP_ARGUMENT);
	for(uint32_t i=0;i<7;++i) {
		CHECK(request(&d,times[i],wire,&m)==0);if(!i)memcpy(original,wire,20);else CHECK(!memcmp(original,wire,20));
		CHECK(xrtRudpDiscoverySent(&d,times[i])==XRUDP_OK && d.State.Attempts==i+1);
		CHECK(xrtRudpDiscoverySent(&d,times[i])==XRUDP_ARGUMENT);
		CHECK(xrtRudpDiscoveryNext(&d,times[i]+1,wire,sizeof(wire),&n,&next)==XRUDP_TIMEOUT && !n && d.State.Reason==XRUDP_OK && next==(i==6?39500000:times[i+1]));
	}
	CHECK(xrtRudpDiscoveryNext(&d,39500000,wire,sizeof(wire),&n,&next)==XRUDP_TIMEOUT && !n && next==UINT64_MAX && d.State.Reason==XRUDP_TIMEOUT);
	for(size_t i=0;i<sizeof(d._Request);++i)CHECK(!d._Request[i]);
	for(size_t i=0;i<sizeof(d._Transaction);++i)CHECK(!d._Transaction[i]);
	CHECK(xrtRudpDiscoveryCancel(&d)==XRUDP_DONE && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();return 0;
}
static int admission_and_deadline(void)
{
	xrudpdiscovery d,before;CHECK(xrtRudpDiscoveryInit(&d,&server,100,1000,true)==XRUDP_OK && xrtMemDebugFailAfter(0));
	unsigned char p[512],original[512];xrudpstunmessage m;CHECK(request(&d,100,p,&m)==0);memcpy(original,p,28);
	CHECK(request(&d,500000,p,&m)==0 && !memcmp(original,p,28) && !d.State.Attempts && d.State.DeadlineUs==1000100);
	CHECK(xrtRudpDiscoverySent(&d,500000)==XRUDP_OK && d.State.Attempts==1 && d.State.NextUs==1000000);
	before=d;size_t n;uint64_t due=123;CHECK(xrtRudpDiscoveryNext(&d,499999,p,sizeof(p),&n,&due)==XRUDP_ARGUMENT && !n && due==123 && !memcmp(&d,&before,sizeof(d)));
	CHECK(xrtRudpDiscoveryNext(&d,1000000,p,1,&n,&due)==XRUDP_LIMIT && !n && d.State.Attempts==1);
	CHECK(request(&d,1000000,p,&m)==0 && xrtRudpDiscoverySent(&d,1000100)==XRUDP_TIMEOUT && d.State.Attempts==1 && d.State.Reason==XRUDP_TIMEOUT);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	CHECK(xrtRudpDiscoveryInit(&d,&server,UINT64_MAX-5,1000,false)==XRUDP_OK && request(&d,UINT64_MAX-5,p,&m)==0 && xrtRudpDiscoverySent(&d,UINT64_MAX)==XRUDP_TIMEOUT);
	return 0;
}
static int input(void)
{
	for(uint32_t mode=0;mode<6;++mode) {
		xrudpdiscovery d;CHECK(xrtRudpDiscoveryInit(&d,&server,0,10000,true)==XRUDP_OK && xrtMemDebugFailAfter(0));
		unsigned char p[512],response[512];xrudpstunmessage m;size_t n;CHECK(request(&d,0,p,&m)==0);
		CHECK(xrtRudpStunResponse(m.Transaction,&mapped,true,response,sizeof(response),&n)==XRUDP_OK);
		CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,0)==XRUDP_BACKPRESSURE && !d.State.Attempts);
		CHECK(xrtRudpDiscoverySent(&d,0)==XRUDP_OK);
		CHECK(xrtRudpDiscoveryInput(&d,&other,response,n,1000)==XRUDP_BACKPRESSURE);
		response[8]^=1;CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,1000)==XRUDP_BACKPRESSURE);response[8]^=1;
		response[n-1]^=1;CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,1000)==XRUDP_BACKPRESSURE);response[n-1]^=1;
		/* Even a matching indication isn't a discovery response or proof. */
		CHECK(xrtRudpStunIndication(m.Transaction,true,p,sizeof(p),&n)==XRUDP_OK && xrtRudpDiscoveryInput(&d,&server,p,n,1000)==XRUDP_BACKPRESSURE && d.State.Reason==XRUDP_OK);
		CHECK(xrtRudpStunResponse(m.Transaction,&mapped,true,response,sizeof(response),&n)==XRUDP_OK);
		CHECK(d.State.Attempts==1 && d.State.DeadlineUs==10000000 && d.State.Discarded==5);
		if(mode==0) {
			CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,2000)==XRUDP_DONE && d.State.Reason==XRUDP_DONE && d.State.Mapped.Port==mapped.Port);
			CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,3000)==XRUDP_DONE && d.State.Discarded==5);
		} else if(mode==1) {
			CHECK(xrtRudpStunError(m.Transaction,500,NULL,0,true,response,sizeof(response),&n)==XRUDP_OK && xrtRudpDiscoveryInput(&d,&server,response,n,2000)==XRUDP_PROTOCOL_MISMATCH && d.State.RemoteError==500);
		} else if(mode==2) {
			CHECK(xrtRudpDiscoveryCancel(&d)==XRUDP_CANCELLED && xrtRudpDiscoveryCancel(&d)==XRUDP_DONE && xrtRudpDiscoveryInput(&d,&server,response,n,2000)==XRUDP_CANCELLED);
		} else if(mode==3) {
			CHECK(xrtRudpDiscoveryInput(&d,&server,response,n,10000000)==XRUDP_TIMEOUT && !d.State.Mapped.Port && d.State.Attempts==1);
		} else if(mode==4) {
			CHECK(xrtRudpStunResponse(m.Transaction,&mapped,false,response,sizeof(response),&n)==XRUDP_OK && xrtRudpDiscoveryInput(&d,&server,response,n,2000)==XRUDP_BACKPRESSURE && d.State.Reason==XRUDP_OK);
		} else {
			/* A server redirect is a terminal result, never a new destination. */
			CHECK(xrtRudpStunError(m.Transaction,300,NULL,0,true,response,sizeof(response),&n)==XRUDP_OK && xrtRudpDiscoveryInput(&d,&server,response,n,2000)==XRUDP_PROTOCOL_MISMATCH && d.State.Server.Port==3478);
		}
		CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	}
	return 0;
}
int main(void)
{
	xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(schedule()==0 && admission_and_deadline()==0 && input()==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount);
	puts("xrudp discovery: admitted retries, exact source+transaction, nonrenewable deadlines, bounded completion and no I/O allocations passed");return 0;
}
