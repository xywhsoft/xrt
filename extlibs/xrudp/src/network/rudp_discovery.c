#include <xrudp/discovery.h>
#include <xrt/random.h>
#include <xrt/memory.h>
#include <string.h>

static uint64_t xrudp__discovery_add(uint64_t a,uint64_t b) { return a>UINT64_MAX-b?UINT64_MAX:a+b; }
static bool xrudp__discovery_address(const xrudpaddress* a)
{
	if(a==NULL || !a->Port)return false;
	if(a->Family==4) { if(a->Zone || !a->Address[0] || a->Address[0]>=224)return false;for(size_t i=4;i<16;++i)if(a->Address[i])return false;return true; }
	if(a->Family!=6 || a->Address[0]==255)return false;
	unsigned char any=0;for(size_t i=0;i<16;++i)any|=a->Address[i];if(!any)return false;
	if(a->Address[0]==254 && (a->Address[1]&192)==128 && !a->Zone)return false;
	bool mapped=true;for(size_t i=0;i<10;++i)if(a->Address[i])mapped=false;
	return !(mapped && a->Address[10]==255 && a->Address[11]==255);
}
static bool xrudp__discovery_equal(const xrudpaddress* a,const xrudpaddress* b)
{ return a->Family==b->Family && a->Port==b->Port && a->Zone==b->Zone && memcmp(a->Address,b->Address,16)==0; }
static xrudpresult xrudp__discovery_end(xrudpdiscovery* d,xrudpresult reason)
{
	d->State.Reason=reason;d->State.NextUs=UINT64_MAX;d->_Offered=false;
	xrtSecureZero(d->_Transaction,sizeof(d->_Transaction));xrtSecureZero(d->_Request,sizeof(d->_Request));return reason;
}
static xrudpresult xrudp__discovery_clock(xrudpdiscovery* d,uint64_t now)
{
	d->_Now=now;if(d->State.Reason!=XRUDP_OK)return d->State.Reason;
	if(now>=d->State.DeadlineUs || (d->_FinishAt && now>=d->_FinishAt))return xrudp__discovery_end(d,XRUDP_TIMEOUT);
	return XRUDP_OK;
}
xrudpresult xrtRudpDiscoveryInit(xrudpdiscovery* d,const xrudpaddress* server,uint64_t now,uint32_t timeout,bool fingerprint)
{
	if(d==NULL || !xrudp__discovery_address(server) || (timeout && (timeout<1000 || timeout>60000)))return XRUDP_ARGUMENT;
	xrudpdiscovery value;memset(&value,0,sizeof(value));value.State.Server=*server;value.State.StartedUs=now;value._Now=now;
	value.State.DeadlineUs=xrudp__discovery_add(now,(uint64_t)(timeout?timeout:XRUDP_DISCOVERY_DEFAULT_TIMEOUT_MS)*1000);
	value.State.NextUs=value._SendAt=now;value.State.Fingerprint=fingerprint;
	if(!xrtSecureRandom(value._Transaction,sizeof(value._Transaction))) { xrtSecureZero(&value,sizeof(value));return XRUDP_IO_ERROR; }
	size_t size;xrudpresult r=xrtRudpStunRequest(value._Transaction,fingerprint,value._Request,sizeof(value._Request),&size);
	if(r==XRUDP_OK) { value._Size=(uint8_t)size;*d=value; }
	xrtSecureZero(&value,sizeof(value));return r;
}
xrudpresult xrtRudpDiscoveryNext(xrudpdiscovery* d,uint64_t now,void* output,size_t capacity,size_t* written,uint64_t* next)
{
	if(written!=NULL)*written=0;
	if(d==NULL || output==NULL || written==NULL || next==NULL || now<d->_Now)return XRUDP_ARGUMENT;
	xrudpresult r=xrudp__discovery_clock(d,now);*next=d->State.NextUs;if(r!=XRUDP_OK)return r;
	if(now<d->_SendAt || d->State.Attempts==XRUDP_DISCOVERY_ATTEMPTS_MAX)return XRUDP_TIMEOUT;
	if(capacity<d->_Size)return XRUDP_LIMIT;
	memmove(output,d->_Request,d->_Size);*written=d->_Size;d->_Offered=true;return XRUDP_OK;
}
xrudpresult xrtRudpDiscoverySent(xrudpdiscovery* d,uint64_t now)
{
	if(d==NULL || now<d->_Now || !d->_Offered)return XRUDP_ARGUMENT;
	xrudpresult r=xrudp__discovery_clock(d,now);if(r!=XRUDP_OK)return r;
	d->_Offered=false;++d->State.Attempts;
	if(d->State.Attempts==XRUDP_DISCOVERY_ATTEMPTS_MAX) { d->_FinishAt=xrudp__discovery_add(now,8000000);d->_SendAt=UINT64_MAX; }
	else d->_SendAt=xrudp__discovery_add(now,UINT64_C(500000)<<(d->State.Attempts-1));
	uint64_t due=d->_FinishAt?d->_FinishAt:d->_SendAt;d->State.NextUs=due<d->State.DeadlineUs?due:d->State.DeadlineUs;return XRUDP_OK;
}
xrudpresult xrtRudpDiscoveryInput(xrudpdiscovery* d,const xrudpaddress* source,const void* data,size_t size,uint64_t now)
{
	if(d==NULL || source==NULL || data==NULL || now<d->_Now)return XRUDP_ARGUMENT;
	xrudpresult r=xrudp__discovery_clock(d,now);if(r!=XRUDP_OK)return r;
	if(!d->State.Attempts || !xrudp__discovery_equal(source,&d->State.Server) || size<20 || size>XRUDP_STUN_PACKET_MAX || memcmp((const unsigned char*)data+8,d->_Transaction,12)!=0)goto drop;
	xrudpstunmessage message;if(xrtRudpStunDecode(data,size,&message)!=XRUDP_OK || (d->State.Fingerprint && !message.Fingerprint))goto drop;
	if(message.Type!=XRUDP_STUN_BINDING_SUCCESS && message.Type!=XRUDP_STUN_BINDING_ERROR)goto drop;
	if(message.Type==XRUDP_STUN_BINDING_ERROR && message.ErrorCode)d->State.RemoteError=message.ErrorCode;
	if(message.UnknownCount || message.Integrity)return xrudp__discovery_end(d,XRUDP_PROTOCOL_MISMATCH);
	if(message.Type==XRUDP_STUN_BINDING_ERROR) {
		if(!message.ErrorCode)goto drop;
		d->State.RemoteError=message.ErrorCode;return xrudp__discovery_end(d,XRUDP_PROTOCOL_MISMATCH);
	}
	if(!message.HasMapped)goto drop;
	d->State.Mapped=message.Mapped;return xrudp__discovery_end(d,XRUDP_DONE);
drop:
	if(d->State.Discarded!=UINT64_MAX)++d->State.Discarded;
	return XRUDP_BACKPRESSURE;
}
xrudpresult xrtRudpDiscoveryCancel(xrudpdiscovery* d)
{
	if(d==NULL)return XRUDP_ARGUMENT;
	if(d->State.Reason!=XRUDP_OK)return XRUDP_DONE;
	return xrudp__discovery_end(d,XRUDP_CANCELLED);
}
xrudpresult xrtRudpDiscoveryState(const xrudpdiscovery* d,xrudpdiscoverystate* state)
{
	if(d==NULL || state==NULL)return XRUDP_ARGUMENT;
	*state=d->State;return XRUDP_OK;
}
