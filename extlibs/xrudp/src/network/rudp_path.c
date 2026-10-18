#include "../internal/xrudp_path.h"
#include <xrt/random.h>
#include <xrt/crypto.h>
#include <string.h>
static uint64_t xrudp__path_add(uint64_t a, uint64_t b) { return a > UINT64_MAX-b ? UINT64_MAX : a+b; }
bool xrudp__path_equal(const xrudpaddress* a, const xrudpaddress* b)
{
	return a->Family==b->Family && a->Port==b->Port && a->Zone==b->Zone && memcmp(a->Address,b->Address,16)==0;
}
bool xrudp__path_address(const xrudpaddress* a)
{
	if(a==NULL || !a->Port || (a->Family!=4 && a->Family!=6))return false;
	unsigned char any=0; for(size_t i=0;i<16;++i)any|=a->Address[i]; if(!any)return false;
	if(a->Family==4) {
		if(a->Zone || a->Address[0]==0 || a->Address[0]>=224)return false;
		for(size_t i=4;i<16;++i)if(a->Address[i])return false;
	} else {
		if(a->Address[0]==255 || (a->Address[0]==254 && (a->Address[1]&192)==128 && !a->Zone))return false;
		bool mapped=true; for(size_t i=0;i<10;++i)if(a->Address[i])mapped=false;
		if(mapped && a->Address[10]==255 && a->Address[11]==255)return false;
	}
	return true;
}
void xrudp__path_clear(xrudp__path* p)
{
	xrtSecureZero(p->Token,sizeof(p->Token)); xrtSecureZero(p->Echo,sizeof(p->Echo));
	p->State.Validating=false; p->State.DeadlineUs=0; p->State.NextProbeUs=UINT64_MAX;
	p->Trusted=false;
}
void xrudp__path_fail(xrudp__path* p,uint64_t at,xrudpresult reason)
{
	p->State.CooldownUntilUs=xrudp__path_add(at,3000000); p->State.Failure=reason;
	if(p->State.Failures!=UINT64_MAX)++p->State.Failures;
	xrtSecureZero(p->Token,sizeof(p->Token)); xrtSecureZero(&p->Echo[1],sizeof(p->Echo[1]));
	p->State.Validating=false; p->State.DeadlineUs=0; p->State.NextProbeUs=UINT64_MAX;
	p->Trusted=false;
}
void xrudp__path_clock(xrudp__path* p,uint64_t now)
{
	if(p->State.Validating && now>=p->State.DeadlineUs) {
		xrudp__path_fail(p,p->State.DeadlineUs,XRUDP_TIMEOUT);
	}
}
xrudpresult xrudp__path_start(xrudp__path* p,const xrudpaddress* a,uint64_t now,bool local)
{
	if(xrudp__path_equal(a,&p->State.Active))return XRUDP_DUPLICATE;
	if(p->State.Validating)return xrudp__path_equal(a,&p->State.Candidate)?XRUDP_DUPLICATE:XRUDP_BACKPRESSURE;
	if(now<p->State.CooldownUntilUs)return XRUDP_BACKPRESSURE;
	p->State.Candidate=*a; p->State.Attempts=0; p->State.CandidateReceived=p->State.CandidateSent=0;
	p->CandidateNumber=0; p->InitialAllowance=local?336u:0; p->Trusted=local;
	unsigned char token[16]; if(!xrtSecureRandom(token,sizeof(token))) {
		xrtSecureZero(token,sizeof(token)); xrudp__path_fail(p,now,XRUDP_IO_ERROR); return XRUDP_IO_ERROR;
	}
	memcpy(p->Token,token,sizeof(token)); xrtSecureZero(token,sizeof(token));
	p->State.Validating=true;
	p->State.Failure=XRUDP_OK;
	p->State.DeadlineUs=xrudp__path_add(now,3000000); p->State.NextProbeUs=now;
	xrtSecureZero(&p->Echo[1],sizeof(p->Echo[1])); return XRUDP_OK;
}
bool xrudp__path_inspect(xrudp__path* p,const xrudpaddress* a,bool control,uint64_t now)
{
	if((p->State.Validating && !xrudp__path_equal(a,&p->State.Candidate)) ||
		(!p->State.Validating && now<p->State.CooldownUntilUs)) {
		if(p->State.AddressDropped!=UINT64_MAX)++p->State.AddressDropped;
		return false;
	}
	uint64_t second=now/1000000; if(second!=p->InspectSecond) { p->InspectSecond=second; p->Inspections=p->ControlInspections=0; }
	uint32_t* inspections=control?&p->ControlInspections:&p->Inspections;
	if(*inspections>=(control?8u:24u)) { if(p->State.RateDropped!=UINT64_MAX)++p->State.RateDropped; return false; }
	++*inspections; return true;
}
void xrudp__path_credit(xrudp__path* p,uint64_t number,uint32_t size)
{
	if(number<=p->CandidateNumber || number<=p->UnvalidatedNumber || number<=p->ActiveNumber)return;
	p->CandidateNumber=number; p->UnvalidatedNumber=number;
	p->State.CandidateReceived+=size<1200-p->State.CandidateReceived?size:1200-p->State.CandidateReceived;
	p->State.ReceivedUnvalidated=xrudp__path_add(p->State.ReceivedUnvalidated,size);
}
void xrudp__path_echo(xrudp__path* p,const xrudpaddress* a,const void* token)
{
	uint32_t index=xrudp__path_equal(a,&p->State.Active)?0u:1u;
	if(index && (!p->State.Validating || !xrudp__path_equal(a,&p->State.Candidate)))return;
	if(!p->Echo[index].Pending) { p->Echo[index].Address=*a; memcpy(p->Echo[index].Token,token,16); p->Echo[index].Pending=true; }
}
void xrudp__path_promote(xrudp__path* p,uint64_t largest)
{
	p->State.Active=p->State.Candidate; p->State.Validating=false; p->State.DeadlineUs=0; p->State.NextProbeUs=UINT64_MAX;
	p->State.CooldownUntilUs=0; p->ActiveNumber=largest;
	if(largest>p->UnvalidatedNumber)p->UnvalidatedNumber=largest;
	p->Trusted=false;
	if(p->State.Validations!=UINT64_MAX)++p->State.Validations;
	p->Echo[0]=p->Echo[1]; xrtSecureZero(&p->Echo[1],sizeof(p->Echo[1])); xrtSecureZero(p->Token,sizeof(p->Token));
}
uint64_t xrudp__path_due(const xrudp__path* p,uint64_t now)
{
	uint64_t due=p->State.Validating?p->State.DeadlineUs:UINT64_MAX;
	if(p->State.Validating && p->State.Attempts<3) {
		/* An already-due first challenge may be waiting only for the shared
		 * path-control rate gate. Idle sessions need that wake even without
		 * pending echoes, flight, application data or another received packet. */
		uint64_t at=p->State.NextProbeUs>p->ResponseAt?p->State.NextProbeUs:p->ResponseAt;
		if(at>now && at<due)due=at;
	}
	if((p->Echo[0].Pending || p->Echo[1].Pending) && p->ResponseAt>now && p->ResponseAt<due)due=p->ResponseAt;
	return due;
}
