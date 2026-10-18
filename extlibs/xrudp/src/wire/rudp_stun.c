#include <xrudp/stun.h>
#include <xrt/crc32.h>
#include <string.h>

static uint16_t xrudp__stun_u16(const unsigned char* p) { return (uint16_t)((uint16_t)p[0]<<8 | p[1]); }
static uint32_t xrudp__stun_u32(const unsigned char* p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static void xrudp__stun_put16(unsigned char* p,uint16_t n) { p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n; }
static void xrudp__stun_put32(unsigned char* p,uint32_t n) { for(size_t i=0;i<4;++i)p[i]=(unsigned char)(n>>(24-8*i)); }
static bool xrudp__stun_address(const xrudpaddress* a)
{
	if(a==NULL || !a->Port || a->Zone)return false;
	if(a->Family==4) {
		if(!a->Address[0] || a->Address[0]>=224)return false;
		for(size_t i=4;i<16;++i)if(a->Address[i])return false;
		return true;
	}
	if(a->Family!=6 || a->Address[0]==255)return false;
	unsigned char any=0;for(size_t i=0;i<16;++i)any|=a->Address[i];
	if(!any || (a->Address[0]==254 && (a->Address[1]&192)==128))return false;
	bool mapped=true;for(size_t i=0;i<10;++i)if(a->Address[i])mapped=false;
	return !(mapped && a->Address[10]==255 && a->Address[11]==255);
}
static void xrudp__stun_header(unsigned char* p,uint16_t type,const void* transaction)
{
	memset(p,0,XRUDP_STUN_HEADER_SIZE);xrudp__stun_put16(p,type);
	xrudp__stun_put32(p+4,UINT32_C(0x2112a442));memcpy(p+8,transaction,12);
}
static xrudpresult xrudp__stun_finish(unsigned char* p,size_t size,bool fingerprint,void* output,size_t capacity,size_t* written)
{
	size_t total=size+(fingerprint?8u:0u);
	if(capacity<total)return XRUDP_LIMIT;
	xrudp__stun_put16(p+2,(uint16_t)(total-20));
	if(fingerprint) {
		xrudp__stun_put16(p+size,0x8028);xrudp__stun_put16(p+size+2,4);
		xrudp__stun_put32(p+size+4,xrtCrc32(p,size)^UINT32_C(0x5354554e));
	}
	memmove(output,p,total);*written=total;return XRUDP_OK;
}
xrudpresult xrtRudpStunRequest(const void* transaction,bool fingerprint,void* output,size_t capacity,size_t* written)
{
	if(written!=NULL)*written=0;
	if(transaction==NULL || output==NULL || written==NULL)return XRUDP_ARGUMENT;
	unsigned char p[28];xrudp__stun_header(p,XRUDP_STUN_BINDING_REQUEST,transaction);
	return xrudp__stun_finish(p,20,fingerprint,output,capacity,written);
}
xrudpresult xrtRudpStunIndication(const void* transaction,bool fingerprint,void* output,size_t capacity,size_t* written)
{
	if(written!=NULL)*written=0;
	if(transaction==NULL || output==NULL || written==NULL)return XRUDP_ARGUMENT;
	unsigned char p[28];xrudp__stun_header(p,XRUDP_STUN_BINDING_INDICATION,transaction);
	return xrudp__stun_finish(p,20,fingerprint,output,capacity,written);
}
xrudpresult xrtRudpStunResponse(const void* transaction,const xrudpaddress* mapped,bool fingerprint,void* output,size_t capacity,size_t* written)
{
	if(written!=NULL)*written=0;
	if(transaction==NULL || output==NULL || written==NULL || !xrudp__stun_address(mapped))return XRUDP_ARGUMENT;
	unsigned char p[52];xrudp__stun_header(p,XRUDP_STUN_BINDING_SUCCESS,transaction);
	size_t count=mapped->Family==4?4u:16u;
	xrudp__stun_put16(p+20,0x0020);xrudp__stun_put16(p+22,(uint16_t)(count+4));
	p[24]=0;p[25]=mapped->Family==4?1:2;xrudp__stun_put16(p+26,mapped->Port^0x2112);
	for(size_t i=0;i<count;++i)p[28+i]=mapped->Address[i]^p[4+i];
	return xrudp__stun_finish(p,28+count,fingerprint,output,capacity,written);
}
xrudpresult xrtRudpStunError(const void* transaction,uint16_t code,const uint16_t* unknown,size_t count,bool fingerprint,void* output,size_t capacity,size_t* written)
{
	if(written!=NULL)*written=0;
	if(transaction==NULL || output==NULL || written==NULL || code<300 || code>699 || count>XRUDP_STUN_UNKNOWN_MAX ||
		(code==420?(count==0 || unknown==NULL):(count!=0)))return XRUDP_ARGUMENT;
	for(size_t i=0;i<count;++i)if(unknown[i]>=0x8000)return XRUDP_ARGUMENT;
	unsigned char p[72];xrudp__stun_header(p,XRUDP_STUN_BINDING_ERROR,transaction);
	xrudp__stun_put16(p+20,0x0009);xrudp__stun_put16(p+22,4);p[24]=p[25]=0;p[26]=(unsigned char)(code/100);p[27]=(unsigned char)(code%100);
	size_t size=28;
	if(count) {
		xrudp__stun_put16(p+size,0x000a);xrudp__stun_put16(p+size+2,(uint16_t)(count*2));size+=4;
		for(size_t i=0;i<count;++i)xrudp__stun_put16(p+size+i*2,unknown[i]);
		size+=count*2;while(size%4)p[size++]=0;
	}
	return xrudp__stun_finish(p,size,fingerprint,output,capacity,written);
}
xrudpresult xrtRudpStunDecode(const void* data,size_t size,xrudpstunmessage* output)
{
	if(data==NULL || output==NULL)return XRUDP_ARGUMENT;
	if(size>XRUDP_STUN_PACKET_MAX)return XRUDP_LIMIT;
	if(size<20)return XRUDP_TRUNCATED;
	const unsigned char* p=(const unsigned char*)data;uint16_t type=xrudp__stun_u16(p),length=xrudp__stun_u16(p+2);
	if(xrudp__stun_u32(p+4)!=UINT32_C(0x2112a442) ||
		(type!=XRUDP_STUN_BINDING_REQUEST && type!=XRUDP_STUN_BINDING_INDICATION && type!=XRUDP_STUN_BINDING_SUCCESS && type!=XRUDP_STUN_BINDING_ERROR))return XRUDP_PROTOCOL_MISMATCH;
	if(length%4)return XRUDP_MALFORMED;
	if((size_t)length>size-20)return XRUDP_TRUNCATED;
	if((size_t)length!=size-20)return XRUDP_MALFORMED;
	xrudpstunmessage result;memset(&result,0,sizeof(result));result.Type=type;memcpy(result.Transaction,p+8,12);
	bool tail=false,error=false;size_t at=20;uint32_t attributes=0;
	while(at<size) {
		if(++attributes>XRUDP_STUN_ATTRIBUTES_MAX)return XRUDP_LIMIT;
		if(size-at<4)return XRUDP_TRUNCATED;
		uint16_t kind=xrudp__stun_u16(p+at),n=xrudp__stun_u16(p+at+2);size_t padded=((size_t)n+3)&~(size_t)3;
		if(padded>size-at-4)return XRUDP_MALFORMED;
		const unsigned char* value=p+at+4;
		if(kind==0x8028) {
			if(n!=4 || at+8!=size || result.Fingerprint || xrudp__stun_u32(value)!=(xrtCrc32(p,at)^UINT32_C(0x5354554e)))return XRUDP_MALFORMED;
			result.Fingerprint=true;
		} else if(kind==0x0008 || kind==0x001c) {
			if(kind==0x0008?n!=20:(n<16 || n>32 || n%4))return XRUDP_MALFORMED;
			result.Integrity=true;tail=true;
		} else if(!tail) {
			if(kind==0x0020 && !result.HasMapped) {
				if(n!=8 && n!=20)return XRUDP_MALFORMED;
				xrudpaddress a;memset(&a,0,sizeof(a));a.Family=value[1]==1?4:value[1]==2?6:0;
				if(!a.Family)return XRUDP_PROTOCOL_MISMATCH;
				if(n!=(a.Family==4?8:20))return XRUDP_MALFORMED;
				a.Port=xrudp__stun_u16(value+2)^0x2112;
				for(size_t i=0;i<(a.Family==4?4u:16u);++i)a.Address[i]=value[4+i]^p[4+i];
				if(!xrudp__stun_address(&a))return XRUDP_MALFORMED;
				result.Mapped=a;result.HasMapped=true;
			} else if(kind==0x0009 && !error) {
				if(n<4 || (value[2]&7)<3 || (value[2]&7)>6 || value[3]>99)return XRUDP_MALFORMED;
				result.ErrorCode=(uint16_t)((value[2]&7)*100+value[3]);error=true;
			} else if(kind==0x000a) {
				if(n%2)return XRUDP_MALFORMED;
			} else if(kind!=0x0020 && kind!=0x0009 && kind<0x8000) {
				bool duplicate=false;for(uint8_t i=0;i<result.UnknownCount;++i)if(result.Unknown[i]==kind)duplicate=true;
				if(!duplicate) { if(result.UnknownCount==XRUDP_STUN_UNKNOWN_MAX)return XRUDP_LIMIT;result.Unknown[result.UnknownCount++]=kind; }
			}
		}
		at+=4+padded;
	}
	*output=result;return XRUDP_OK;
}
