#include <xrudp/punch.h>
#include <xrt/random.h>
#include <xrt/memory.h>
#include <string.h>

static uint64_t xrudp__punch_add(uint64_t a,uint64_t b) { return a>UINT64_MAX-b?UINT64_MAX:a+b; }
static bool xrudp__punch_address(const xrudpaddress* a)
{
	if(!a->Port)return false;
	if(a->Family==4) {
		if(a->Zone || !a->Address[0] || a->Address[0]>=224)return false;
		for(size_t i=4;i<16;++i)if(a->Address[i])return false;
		return true;
	}
	if(a->Family!=6 || a->Address[0]==255)return false;
	unsigned char any=0;for(size_t i=0;i<16;++i)any|=a->Address[i];if(!any)return false;
	if(a->Address[0]==254 && (a->Address[1]&192)==128 && !a->Zone)return false;
	bool mapped=true;for(size_t i=0;i<10;++i)if(a->Address[i])mapped=false;
	return !(mapped && a->Address[10]==255 && a->Address[11]==255);
}
static bool xrudp__punch_equal(const xrudpaddress* a,const xrudpaddress* b)
{ return a->Family==b->Family && a->Port==b->Port && a->Zone==b->Zone && memcmp(a->Address,b->Address,16)==0; }
static bool xrudp__punch_valid(const xrudppunch* p)
{ return p!=NULL && p->State.Count!=0 && p->State.Count<=XRUDP_PUNCH_CANDIDATES_MAX; }
static xrudpresult xrudp__punch_end(xrudppunch* p,xrudpresult reason)
{
	for(uint32_t i=0;i<p->State.Count;++i)if(p->State.Candidates[i].Reason==XRUDP_OK)p->State.Candidates[i].Reason=reason;
	p->State.Reason=reason;p->State.NextUs=UINT64_MAX;p->_Offered=XRUDP_PUNCH_CANDIDATES_MAX;
	xrtSecureZero(p->_Transactions,sizeof(p->_Transactions));return reason;
}
static xrudpresult xrudp__punch_clock(xrudppunch* p,uint64_t now)
{
	p->_Now=now;if(p->State.Reason!=XRUDP_OK)return p->State.Reason;
	if(now>=p->State.DeadlineUs)return xrudp__punch_end(p,XRUDP_TIMEOUT);
	return XRUDP_OK;
}
static xrudpresult xrudp__punch_due(xrudppunch* p)
{
	if(p->State.Completed+p->State.Failed==p->State.Count)return xrudp__punch_end(p,p->State.Failed?XRUDP_IO_ERROR:XRUDP_DONE);
	uint64_t due=p->State.DeadlineUs;
	for(uint32_t i=0;i<p->State.Count;++i)if(p->State.Candidates[i].Reason==XRUDP_OK) {
		uint64_t value=p->_Due[i]>p->_SendAt?p->_Due[i]:p->_SendAt;
		if(value<due)due=value;
	}
	p->State.NextUs=due;return XRUDP_OK;
}
xrudpresult xrtRudpPunchInit(xrudppunch* p,const xrudpaddress* candidates,size_t count,uint64_t now,uint32_t timeout)
{
	if(p==NULL || candidates==NULL || !count || count>XRUDP_PUNCH_CANDIDATES_MAX || (timeout && (timeout<1000 || timeout>10000)))return XRUDP_ARGUMENT;
	for(size_t i=0;i<count;++i) {
		if(!xrudp__punch_address(&candidates[i]))return XRUDP_ARGUMENT;
		for(size_t j=0;j<i;++j)if(xrudp__punch_equal(&candidates[i],&candidates[j]))return XRUDP_ARGUMENT;
	}
	xrudppunch value;memset(&value,0,sizeof(value));value.State.Count=(uint32_t)count;value.State.StartedUs=value._Now=now;
	value.State.DeadlineUs=xrudp__punch_add(now,(uint64_t)(timeout?timeout:XRUDP_PUNCH_DEFAULT_TIMEOUT_MS)*1000);
	value.State.NextUs=value._SendAt=now;value._Offered=XRUDP_PUNCH_CANDIDATES_MAX;
	for(size_t i=0;i<count;++i) { value.State.Candidates[i].Address=candidates[i];value._Due[i]=now; }
	if(!xrtSecureRandom(value._Transactions,count*sizeof(value._Transactions[0]))) {
		xrtSecureZero(&value,sizeof(value));return XRUDP_IO_ERROR;
	}
	*p=value;xrtSecureZero(&value,sizeof(value));return XRUDP_OK;
}
xrudpresult xrtRudpPunchNext(xrudppunch* p,uint64_t now,void* output,size_t capacity,size_t* written,xrudpaddress* target,uint32_t* index,uint64_t* next)
{
	if(written!=NULL)*written=0;
	if(!xrudp__punch_valid(p) || output==NULL || written==NULL || target==NULL || index==NULL || next==NULL || now<p->_Now)return XRUDP_ARGUMENT;
	xrudpresult r=xrudp__punch_clock(p,now);*next=p->State.NextUs;if(r!=XRUDP_OK)return r;
	if(now<p->State.NextUs)return XRUDP_TIMEOUT;
	uint32_t chosen=p->_Offered;
	if(chosen==XRUDP_PUNCH_CANDIDATES_MAX) {
		for(uint32_t n=0;n<p->State.Count;++n) {
			uint32_t i=(p->_Cursor+n)%p->State.Count;
			if(p->State.Candidates[i].Reason==XRUDP_OK && p->_Due[i]<=now) { chosen=i;break; }
		}
	}
	if(chosen==XRUDP_PUNCH_CANDIDATES_MAX)return XRUDP_TIMEOUT;
	r=xrtRudpStunIndication(p->_Transactions[chosen][p->State.Candidates[chosen].Sent],true,output,capacity,written);
	if(r!=XRUDP_OK)return r;
	*target=p->State.Candidates[chosen].Address;*index=chosen;p->_Offered=chosen;return XRUDP_OK;
}
static xrudpresult xrudp__punch_admission(xrudppunch* p,uint32_t index,uint64_t now,bool sent)
{
	if(!xrudp__punch_valid(p) || now<p->_Now)return XRUDP_ARGUMENT;
	if(p->State.Reason!=XRUDP_OK)return p->State.Reason;
	if(index>=p->State.Count || p->_Offered!=index)return XRUDP_ARGUMENT;
	xrudpresult r=xrudp__punch_clock(p,now);if(r!=XRUDP_OK)return r;
	xrudppunchcandidate* c=&p->State.Candidates[index];p->_Offered=XRUDP_PUNCH_CANDIDATES_MAX;p->_Cursor=(index+1)%p->State.Count;
	if(sent) {
		xrtSecureZero(p->_Transactions[index][c->Sent],XRUDP_STUN_TRANSACTION_SIZE);++c->Sent;++p->State.Sent;
		p->_SendAt=xrudp__punch_add(now,XRUDP_PUNCH_INTERVAL_US);
		if(c->Sent==XRUDP_PUNCH_INDICATIONS_MAX) { c->Reason=XRUDP_DONE;++p->State.Completed; }
		else {
			uint64_t delay=UINT64_C(200000)<<(c->Sent-1);if(delay>2000000)delay=2000000;
			p->_Due[index]=xrudp__punch_add(now,delay);
		}
	} else {
		c->Reason=XRUDP_IO_ERROR;++p->State.Failed;xrtSecureZero(p->_Transactions[index],sizeof(p->_Transactions[index]));
	}
	return xrudp__punch_due(p);
}
xrudpresult xrtRudpPunchSent(xrudppunch* p,uint32_t index,uint64_t now)
{ return xrudp__punch_admission(p,index,now,true); }
xrudpresult xrtRudpPunchReject(xrudppunch* p,uint32_t index,uint64_t now)
{ return xrudp__punch_admission(p,index,now,false); }
xrudpresult xrtRudpPunchCancel(xrudppunch* p)
{
	if(!xrudp__punch_valid(p))return XRUDP_ARGUMENT;
	if(p->State.Reason!=XRUDP_OK)return XRUDP_DONE;
	return xrudp__punch_end(p,XRUDP_CANCELLED);
}
xrudpresult xrtRudpPunchState(const xrudppunch* p,xrudppunchstate* state)
{
	if(!xrudp__punch_valid(p) || state==NULL)return XRUDP_ARGUMENT;
	*state=p->State;return XRUDP_OK;
}
