#include <xrudp.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
static const xrudpaddress targets[8]={
	{4,42000,0,{192,0,2,1}}, {4,42001,0,{192,0,2,1}}, {4,42000,0,{198,51,100,2}},
	{6,42000,0,{0x20,1,0xd,0xb8,0,0,0,0,0,0,0,0,0,0,0,1}},
	{6,42000,7,{0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{4,42002,0,{192,0,2,1}}, {4,42003,0,{192,0,2,1}}, {4,42004,0,{192,0,2,1}}
};
static bool equal_address(const xrudpaddress* a,const xrudpaddress* b)
{return a->Family==b->Family && a->Port==b->Port && a->Zone==b->Zone && !memcmp(a->Address,b->Address,16);}
static bool erased(const xrudppunch* p)
{for(size_t i=0;i<sizeof(p->_Transactions);++i)if(((const unsigned char*)p->_Transactions)[i])return false;return true;}
static int one_schedule(void)
{
	xrudppunch p;unsigned char wire[28],saved[28],last[12]={0};xrudpaddress target;uint32_t index;size_t n;uint64_t next;
	CHECK(xrtRudpPunchInit(&p,targets,1,0,0)==XRUDP_OK && xrtMemDebugFailAfter(0));
	const uint64_t times[]={0,200000,600000,1400000,3000000,5000000};
	CHECK(xrtRudpPunchSent(&p,0,0)==XRUDP_ARGUMENT);
	for(uint32_t i=0;i<6;++i) {
		CHECK(xrtRudpPunchNext(&p,times[i],wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && n==28 && index==0 && equal_address(&target,targets));
		xrudpstunmessage m;CHECK(xrtRudpStunDecode(wire,n,&m)==XRUDP_OK && m.Type==XRUDP_STUN_BINDING_INDICATION && m.Fingerprint && !m.HasMapped && !m.Integrity);
		CHECK(memcmp(last,m.Transaction,12));memcpy(last,m.Transaction,12);memcpy(saved,wire,28);
		CHECK(xrtRudpPunchNext(&p,times[i],wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && !memcmp(saved,wire,28) && p.State.Sent==i);
		CHECK(xrtRudpPunchSent(&p,index,times[i])==(i==5?XRUDP_DONE:XRUDP_OK));
		CHECK(p.State.Sent==i+1 && p.State.Candidates[0].Sent==i+1);
		CHECK(xrtRudpPunchSent(&p,index,times[i])==(i==5?XRUDP_DONE:XRUDP_ARGUMENT));
		if(i!=5)CHECK(xrtRudpPunchNext(&p,times[i]+1,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_TIMEOUT && !n && next==times[i+1] && p.State.Reason==XRUDP_OK);
	}
	CHECK(p.State.Reason==XRUDP_DONE && p.State.Completed==1 && !p.State.Failed && p.State.NextUs==UINT64_MAX && erased(&p));
	xrudppunchstate snapshot;CHECK(xrtRudpPunchState(&p,&snapshot)==XRUDP_OK && !memcmp(&snapshot,&p.State,sizeof(snapshot)) && xrtRudpPunchCancel(&p)==XRUDP_DONE);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();return 0;
}
static int fairness(void)
{
	xrudppunch p;CHECK(xrtRudpPunchInit(&p,targets,8,100,0)==XRUDP_OK && xrtMemDebugFailAfter(0));
	unsigned char wire[28],transactions[48][12];xrudpaddress target;uint32_t index,sequence=0;uint64_t now=100,last=0,next;size_t n;
	while(p.State.Reason==XRUDP_OK) {
		xrudpresult r=xrtRudpPunchNext(&p,now,wire,sizeof(wire),&n,&target,&index,&next);
		if(r==XRUDP_TIMEOUT) {CHECK(next>now);now=next;continue;}
		CHECK(r==XRUDP_OK && n==28 && index==sequence%8 && sequence<48 && equal_address(&target,&targets[index]));
		if(sequence)CHECK(now-last>=50000);
		for(uint32_t i=0;i<sequence;++i)CHECK(memcmp(transactions[i],wire+8,12));
		memcpy(transactions[sequence],wire+8,12);last=now;++sequence;
		r=xrtRudpPunchSent(&p,index,now);CHECK(r==(sequence==48?XRUDP_DONE:XRUDP_OK));
	}
	CHECK(sequence==48 && p.State.Completed==8 && !p.State.Failed && p.State.Sent==48 && now<8000100 && erased(&p));
	for(size_t i=0;i<8;++i)CHECK(p.State.Candidates[i].Sent==6 && p.State.Candidates[i].Reason==XRUDP_DONE);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();return 0;
}
static int failures(void)
{
	xrudppunch p,before;unsigned char wire[28],saved[28];xrudpaddress target;size_t n;uint32_t index;uint64_t next;
	CHECK(xrtRudpPunchInit(&p,targets,3,100,1000)==XRUDP_OK && xrtMemDebugFailAfter(0));
	CHECK(xrtRudpPunchNext(&p,100,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && index==0);memcpy(saved,wire,28);
	CHECK(xrtRudpPunchNext(&p,500000,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && !memcmp(saved,wire,28) && !p.State.Sent && p.State.DeadlineUs==1000100);
	before=p;CHECK(xrtRudpPunchSent(&p,1,500000)==XRUDP_ARGUMENT && !memcmp(&p,&before,sizeof(p)));
	CHECK(xrtRudpPunchReject(&p,1,500000)==XRUDP_ARGUMENT && !memcmp(&p,&before,sizeof(p)));
	CHECK(xrtRudpPunchSent(&p,0,500000)==XRUDP_OK && p.State.NextUs==550000);
	CHECK(xrtRudpPunchNext(&p,550000,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && index==1 && xrtRudpPunchReject(&p,1,550000)==XRUDP_OK && p.State.Sent==1 && p.State.Failed==1 && p.State.Candidates[1].Reason==XRUDP_IO_ERROR);
	CHECK(xrtRudpPunchNext(&p,550000,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && index==2 && xrtRudpPunchSent(&p,2,550000)==XRUDP_OK);
	/* Delayed owner cannot renew the absolute bound or spend its old offer. */
	CHECK(xrtRudpPunchNext(&p,700000,wire,sizeof(wire),&n,&target,&index,&next)==XRUDP_OK && index==0);
	CHECK(xrtRudpPunchSent(&p,index,1000100)==XRUDP_TIMEOUT && p.State.Reason==XRUDP_TIMEOUT && p.State.Sent==2 && p.State.Failed==1 && p.State.Candidates[0].Reason==XRUDP_TIMEOUT && erased(&p));
	CHECK(xrtRudpPunchCancel(&p)==XRUDP_DONE);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	/* A route error does not bypass the last real send's aggregate gate. */
	CHECK(xrtRudpPunchInit(&p,targets,2,0,0)==XRUDP_OK && xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&next)==XRUDP_OK && xrtRudpPunchSent(&p,index,0)==XRUDP_OK);
	CHECK(xrtRudpPunchNext(&p,50000,wire,28,&n,&target,&index,&next)==XRUDP_OK && xrtRudpPunchReject(&p,index,50000)==XRUDP_OK && p.State.NextUs==200000);
	while(p.State.Reason==XRUDP_OK) {uint64_t now=p.State.NextUs;CHECK(xrtRudpPunchNext(&p,now,wire,28,&n,&target,&index,&next)==XRUDP_OK && index==0);xrudpresult r=xrtRudpPunchSent(&p,index,now);CHECK(r==XRUDP_OK || r==XRUDP_IO_ERROR);}
	CHECK(p.State.Reason==XRUDP_IO_ERROR && p.State.Completed==1 && p.State.Failed==1 && p.State.Sent==6 && erased(&p));
	CHECK(xrtRudpPunchInit(&p,targets,2,0,0)==XRUDP_OK && xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&next)==XRUDP_OK && xrtRudpPunchCancel(&p)==XRUDP_CANCELLED && erased(&p));
	CHECK(xrtRudpPunchSent(&p,index,0)==XRUDP_CANCELLED && xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&next)==XRUDP_CANCELLED && !n);
	CHECK(xrtRudpPunchInit(&p,targets,1,UINT64_MAX-10,1000)==XRUDP_OK && xrtRudpPunchNext(&p,UINT64_MAX-10,wire,28,&n,&target,&index,&next)==XRUDP_OK && xrtRudpPunchSent(&p,index,UINT64_MAX-5)==XRUDP_OK && p.State.NextUs==UINT64_MAX);
	CHECK(xrtRudpPunchNext(&p,UINT64_MAX,wire,28,&n,&target,&index,&next)==XRUDP_TIMEOUT && p.State.Sent==1 && erased(&p));return 0;
}
static int arguments(void)
{
	xrudppunch p,before;memset(&p,0xa5,sizeof(p));before=p;
	CHECK(xrtRudpPunchInit(&p,targets,0,0,0)==XRUDP_ARGUMENT && xrtRudpPunchInit(&p,targets,9,0,0)==XRUDP_ARGUMENT);
	CHECK(xrtRudpPunchInit(&p,targets,1,0,999)==XRUDP_ARGUMENT && xrtRudpPunchInit(&p,targets,1,0,10001)==XRUDP_ARGUMENT);
	xrudpaddress a[2]={targets[0],targets[0]};CHECK(xrtRudpPunchInit(&p,a,2,0,0)==XRUDP_ARGUMENT);
	for(size_t i=0;i<10;++i) {
		a[0]=targets[0];
		if(i==0)a[0].Family=3;else if(i==1)a[0].Port=0;else if(i==2)a[0].Zone=1;else if(i==3)a[0].Address[4]=1;
		else if(i==4)a[0].Address[0]=224;else if(i==5)a[0].Address[0]=0;
		else {a[0]=targets[3];memset(a[0].Address,0,16);if(i==6)a[0].Address[0]=255;else if(i==7){a[0].Address[0]=254;a[0].Address[1]=128;}else if(i==8){a[0].Address[10]=a[0].Address[11]=255;a[0].Address[12]=192;a[0].Address[15]=1;}}
		CHECK(xrtRudpPunchInit(&p,a,1,0,0)==XRUDP_ARGUMENT && !memcmp(&p,&before,sizeof(p)));
	}
	CHECK(xrtRudpPunchInit(&p,targets,1,100,0)==XRUDP_OK);before=p;
	unsigned char wire[28];memset(wire,0xa5,28);size_t n=999;xrudpaddress target=targets[7];uint32_t index=99;uint64_t next=99;
	CHECK(xrtRudpPunchNext(&p,99,wire,28,&n,&target,&index,&next)==XRUDP_ARGUMENT && !n && next==99 && index==99 && equal_address(&target,&targets[7]) && !memcmp(&p,&before,sizeof(p)));
	CHECK(xrtRudpPunchNext(&p,100,wire,27,&n,&target,&index,&next)==XRUDP_LIMIT && !n && index==99 && equal_address(&target,&targets[7]) && !memcmp(&p,&before,sizeof(p)));
	for(size_t i=0;i<28;++i)CHECK(wire[i]==0xa5);
	memset(&p,0,sizeof(p));CHECK(xrtRudpPunchCancel(&p)==XRUDP_ARGUMENT && xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&next)==XRUDP_ARGUMENT);return 0;
}
static int bounded_schedules(void)
{
	uint32_t seed=0x95e72341;unsigned iterations=0;
	for(uint32_t trial=0;trial<512;++trial) {
		xrudppunch p;uint32_t count=trial%8+1;CHECK(xrtRudpPunchInit(&p,targets,count,0,1000+(trial%10)*1000)==XRUDP_OK && xrtMemDebugFailAfter(0));
		uint64_t now=0,last=0;uint32_t total=0;uint32_t admitted[8]={0};
		while(p.State.Reason==XRUDP_OK) {
			seed=seed*1664525u+1013904223u;if(now<p.State.NextUs)now=p.State.NextUs;now+=seed%100001;unsigned char wire[28],copy[28];xrudpaddress target;size_t n;uint32_t index;uint64_t next;
			xrudpresult r=xrtRudpPunchNext(&p,now,wire,28,&n,&target,&index,&next);++iterations;
			if(r==XRUDP_TIMEOUT) {CHECK(p.State.Reason==XRUDP_TIMEOUT && erased(&p));break;}
			CHECK(r==XRUDP_OK && n==28 && index<count && admitted[index]<6 && equal_address(&target,&targets[index]));memcpy(copy,wire,28);
			/* Identical failed local offers cannot skip or spend a candidate. */
			CHECK(xrtRudpPunchNext(&p,now,wire,28,&n,&target,&index,&next)==XRUDP_OK && !memcmp(copy,wire,28) && p.State.Sent==total);
			if(seed%13==0)r=xrtRudpPunchReject(&p,index,now);
			else {if(total)CHECK(now-last>=50000);last=now;++admitted[index];++total;r=xrtRudpPunchSent(&p,index,now);}
			CHECK(r==XRUDP_OK || r==XRUDP_DONE || r==XRUDP_IO_ERROR);CHECK(p.State.Sent==total && total<=count*6 && total<=48);
		}
		CHECK(erased(&p) && p.State.NextUs==UINT64_MAX);for(uint32_t i=0;i<count;++i)CHECK(p.State.Candidates[i].Sent==admitted[i]);
		CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	}
	CHECK(iterations>8000);return 0;
}
int main(void)
{
	xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(one_schedule()==0 && fairness()==0 && failures()==0 && arguments()==0 && bounded_schedules()==0);
	xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount);
	puts("xrudp punch: six fresh one-way indications, eight bounded targets, admission fairness, cancellation, route isolation, deadlines and512 varied schedules passed");return 0;
}
