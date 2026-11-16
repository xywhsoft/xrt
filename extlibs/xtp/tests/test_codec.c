#include <xtp.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static void ref16(unsigned char* p,uint16_t n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);}
static void ref32(unsigned char* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i));}
/* Independent fixture writer; deliberately does not include private headers. */
static size_t reference(const xtppacket* p,unsigned char* out)
{
    size_t cmd=p->Command.Kind==XTP_COMMAND_NAME?p->Command.As.Name.Size:0;
    size_t n=p->Params.Count,stride=p->Params.Kind==XTP_PARAMS_TABLE?6:4,meta=n?2+n*stride:0;
    for(size_t i=0;i<n;++i)meta+=(p->Params.Kind==XTP_PARAMS_TABLE?p->Params.As.Table[i].Key.Size+p->Params.As.Table[i].Value.Size:p->Params.As.Array[i].Size);
    size_t total=32+cmd+meta+p->Body.Size;
    memset(out,0,32);out[0]=0x78;out[1]=0x74;out[2]=0x70;out[3]=3;ref32(out+4,(uint32_t)total);
    for(unsigned i=0;i<8;++i)out[8+i]=(unsigned char)(p->MessageID>>(8*i));
    out[16]=(unsigned char)p->Type;out[17]=(unsigned char)((p->Command.Kind==XTP_COMMAND_ID?1:0)+(p->Params.Kind==XTP_PARAMS_ARRAY?2:0)+(p->ResultAck?4:0)+(p->Replayable?8:0));
    ref16(out+18,(uint16_t)cmd);ref32(out+20,(uint32_t)meta);ref32(out+24,(uint32_t)p->Status);
    if(p->Command.Kind==XTP_COMMAND_ID)ref32(out+28,p->Command.As.ID);
    if(cmd)memcpy(out+32,p->Command.As.Name.Data,cmd);
    if(n){unsigned char* table=out+32+cmd;unsigned char* data=table+2+stride*n;size_t end=0;ref16(table,(uint16_t)n);
        for(size_t i=0;i<n;++i){xbytesview v;
            if(p->Params.Kind==XTP_PARAMS_TABLE){xstrview k=p->Params.As.Table[i].Key;v=p->Params.As.Table[i].Value;
                ref16(table+2+i*6,(uint16_t)k.Size);memcpy(data+end,k.Data,k.Size);end+=k.Size;
            }else v=p->Params.As.Array[i];
            if(v.Size)memcpy(data+end,v.Data,v.Size);
            end+=v.Size;ref32(table+2+i*stride+(stride==6?2:0),(uint32_t)end);}}
    if(p->Body.Size)memcpy(out+32+cmd+meta,p->Body.Data,p->Body.Size);
    return total;
}
static void same(xbytesview a,xbytesview b){assert(a.Size==b.Size);assert(!a.Size||!memcmp(a.Data,b.Data,a.Size));}
static void view_check(const xtppacket* p,const xtpframeview* f)
{
    assert(f->Type==p->Type&&f->MessageID==p->MessageID&&f->Status==p->Status);
    assert(f->Command.Kind==p->Command.Kind&&f->Params.Count==p->Params.Count&&f->Params.Kind==p->Params.Kind);
    assert(f->Replayable==p->Replayable&&f->ResultAck==p->ResultAck);same(f->Body,p->Body);
    if(p->Command.Kind==XTP_COMMAND_ID)assert(f->Command.As.ID==p->Command.As.ID);
    if(p->Command.Kind==XTP_COMMAND_NAME)assert(xtpCommandIsName(&f->Command,p->Command.As.Name));
    xtpparamiter it=xtpParams(&f->Params);xtpparamentry e;
    for(size_t i=0;i<p->Params.Count;++i){xbytesview v;
        xbytesview expected=p->Params.Kind==XTP_PARAMS_TABLE?p->Params.As.Table[i].Value:p->Params.As.Array[i];
        assert(xtpParamAt(&f->Params,i,&v)==XTP_PARAM_FOUND);same(v,expected);
        assert(xtpParamNext(&it,&e)==XTP_PARAM_FOUND&&e.Index==i);same(e.Value,expected);
        assert(e.HasKey==(p->Params.Kind==XTP_PARAMS_TABLE));
        if(e.HasKey){xstrview k=p->Params.As.Table[i].Key;assert(e.Key.Size==k.Size&&!memcmp(k.Data,e.Key.Data,k.Size));
            assert(xtpParamFind(&f->Params,k,&v)==XTP_PARAM_FOUND);same(v,expected);}}
    assert(xtpParamNext(&it,&e)==XTP_PARAM_MISSING&&e.Value.Size==0);
    xbytesview v={f->Packet.Data,99};assert(xtpParamAt(&f->Params,p->Params.Count,&v)==XTP_PARAM_MISSING&&!v.Data&&!v.Size);
    assert(xtpParamFind(&f->Params,XRT_STR_LITERAL("absent"),&v)==(p->Params.Kind==XTP_PARAMS_ARRAY?XTP_PARAM_WRONG_MODE:XTP_PARAM_MISSING));
}
static void golden(void)
{
    static const unsigned char request[]={0x78,0x74,0x70,3,32,0,0,0,1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static const unsigned char array[]={3,0,1,0,0,0,3,0,0,0,3,0,0,0,1,2,3};
    static const unsigned char table[]={2,0,1,0,3,0,0,0,1,0,5,0,0,0,'b',2,3,'a',1};
    unsigned char out[128],in[128];size_t size=99,written=99;xtpframeview f;
    xtppacket p={0};p.Type=XTP_REQUEST;p.MessageID=1;p.Command=xtpCmdID(0);
    assert(xtpEncodeSize(&p,1,NULL,&size)==XTP_OK&&size==32);
    assert(xtpEncode(&p,1,NULL,out,sizeof(out),&written)==XTP_OK&&written==32&&!memcmp(out,request,32));
    assert(xtpDecode((xbytesview){request,sizeof(request)},NULL,&f)==XTP_OK);view_check(&p,&f);
    xbytesview values[]={{(const unsigned char*)"\1",1},{(const unsigned char*)"\2\3",2},{NULL,0}};
    p.Params=xtpArray(values,3);reference(&p,in);
    assert(!memcmp(in+32,array,sizeof(array)));
    assert(xtpEncode(&p,1,NULL,out,sizeof(out),&written)==XTP_OK&&!memcmp(out+32,array,sizeof(array)));
    assert(xtpDecode((xbytesview){out,written},NULL,&f)==XTP_OK);view_check(&p,&f);
    xtpfield fields[]={{XRT_STR_INIT("b"),{(const unsigned char*)"\2\3",2}},{XRT_STR_INIT("a"),{(const unsigned char*)"\1",1}}};
    p.Params=xtpTable(fields,2);reference(&p,in);
    assert(!memcmp(in+32,table,sizeof(table)));
    assert(xtpEncode(&p,1,NULL,out,sizeof(out),&written)==XTP_OK&&!memcmp(out+32,table,sizeof(table)));
    assert(xtpDecode((xbytesview){out,written},NULL,&f)==XTP_OK);view_check(&p,&f);
}
static uint32_t rng=0x10293847u;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void randomized(void)
{
    unsigned char ref[8192],out[8193],bytes[512];char keys[32][8];xbytesview values[32];xtpfield fields[32];
    for(size_t round=0;round<2000;++round){
        for(size_t i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)random32();
        size_t count=random32()%32;
        for(size_t i=0;i<count;++i){snprintf(keys[i],sizeof(keys[i]),"k%03u",(unsigned)((i*7)%32));
            values[i]=(xbytesview){bytes+i*8,random32()%9};fields[i]=(xtpfield){{keys[i],4},values[i]};}
        xtppacket p={0};p.Type=(xtpmessagekind)(1+random32()%5);p.MessageID=p.Type==XTP_NOTIFY?0:((uint64_t)random32()<<32)|random32()|1;
        p.Params=random32()%2?xtpArray(values,count):xtpTable(fields,count);
        p.Body=(xbytesview){bytes,random32()%513};
        if(p.Type==XTP_REQUEST||p.Type==XTP_PUSH||p.Type==XTP_NOTIFY)p.Command=random32()%2?xtpCmdID(random32()):xtpCmdName(XRT_STR_LITERAL("player.move"));
        if(p.Type==XTP_RESPONSE||p.Type==XTP_ACK)p.Status=random32()%2?-(int32_t)(random32()%9):(int32_t)(random32()&INT32_MAX);
        if(p.Type==XTP_ACK){p.Params=xtpTable(NULL,0);p.Body=(xbytesview){NULL,0};p.Status=0;p.ResultAck=(random32()%2)!=0;}
        p.Replayable=(p.Type==XTP_REQUEST||p.Type==XTP_PUSH)&&(random32()%2);
        size_t expected=reference(&p,ref),n=0;xtpframeview f;
        assert(xtpEncode(&p,1,NULL,out+1,sizeof(out)-1,&n)==XTP_OK&&n==expected&&!memcmp(ref,out+1,n));
        assert(xtpDecode((xbytesview){out+1,n},NULL,&f)==XTP_OK);view_check(&p,&f);
        /* Every prefix is truncated or a deterministic earlier validation error. */
        if(round<32)for(size_t i=0;i<n;++i)assert(xtpDecode((xbytesview){out+1,i},NULL,&f)!=XTP_OK);
    }
}
static void failure(void)
{
    unsigned char good[128],bad[128],sentinel[128];xtppacket p={0};p.Type=XTP_REQUEST;p.MessageID=1;p.Command=xtpCmdID(5);
    xbytesview a[]={{(const unsigned char*)"x",1},{NULL,0}};p.Params=xtpArray(a,2);
    size_t n=0,w=123;assert(xtpEncode(&p,1,NULL,good,sizeof(good),&n)==XTP_OK);
    memset(sentinel,0xa5,sizeof(sentinel));xtppacket batch[2];batch[0]=p;batch[1]=p;batch[1].MessageID=0;
    assert(xtpEncode(batch,2,NULL,sentinel,sizeof(sentinel),&w)==XTP_E_VALUE&&w==123);
    for(size_t i=0;i<sizeof(sentinel);++i)assert(sentinel[i]==0xa5);
    assert(xtpEncode(&p,1,NULL,sentinel,4,&w)==XTP_E_CAPACITY&&w==123&&sentinel[0]==0xa5);
    xtpframeview f,saved;memset(&saved,0x5a,sizeof(saved));f=saved;
    #define BAD_AT(offset,value) do{memcpy(bad,good,n);bad[offset]=(value);assert(xtpDecode((xbytesview){bad,n},NULL,&f)!=XTP_OK);assert(!memcmp(&f,&saved,sizeof(f)));}while(0)
    BAD_AT(0,'q');BAD_AT(3,2);BAD_AT(4,31);BAD_AT(17,0x80);BAD_AT(16,0);BAD_AT(24,1);
    BAD_AT(32,0);BAD_AT(34,200);BAD_AT(38,0);
    #undef BAD_AT
    memcpy(bad,good,n);bad[n]=0;
    assert(xtpDecode((xbytesview){bad,n+1},NULL,&f)==XTP_E_LENGTH);
    assert(xtpDecode((xbytesview){NULL,SIZE_MAX},NULL,&f)==XTP_E_ARGUMENT);
    assert(xtpDecode((xbytesview){good,n},NULL,(xtpframeview*)good)==XTP_E_ARGUMENT);
    xtpcodecconfig c;xtpCodecConfigInit(&c);c.MaxFrameBytes=31;
    assert(xtpEncodeSize(&p,1,&c,&w)==XTP_E_ARGUMENT&&w==123);
    xtpCodecConfigInit(&c);c.MaxParams=1;
    assert(xtpDecode((xbytesview){good,n},&c,&f)==XTP_E_LIMIT);
    p.Body=(xbytesview){sentinel,1};assert(xtpEncode(&p,1,NULL,sentinel,sizeof(sentinel),&w)==XTP_E_ARGUMENT);
    p.Body=(xbytesview){NULL,0};xtpfield fields[]={{XRT_STR_INIT("same"),{NULL,0}},{XRT_STR_INIT("same"),{NULL,0}}};
    p.Params=xtpTable(fields,2);assert(xtpEncodeSize(&p,1,NULL,&w)==XTP_E_VALUE);
    fields[1].Key=XRT_STR_LITERAL("else");assert(xtpEncode(&p,1,NULL,bad,sizeof(bad),&n)==XTP_OK);
    /* Change equal-size key bytes to duplicate after valid offset checks. */
    memcpy(bad+32+2+12+4,"same",4);
    assert(xtpDecode((xbytesview){bad,n},NULL,&f)==XTP_E_VALUE);
}
static void limits(void)
{
    xtpfield fields[256];char keys[256][8];unsigned char out[4096];
    for(size_t i=0;i<256;++i){snprintf(keys[i],8,"k%03u",(unsigned)(255-i));fields[i]=(xtpfield){{keys[i],4},{NULL,0}};}
    xtppacket p={0};p.Type=XTP_PUSH;p.MessageID=UINT64_MAX;p.Command=xtpCmdID(UINT32_MAX);p.Params=xtpTable(fields,256);
    xtpcodecconfig c;xtpCodecConfigInit(&c);c.MaxParams=256;size_t n=0;xtpframeview f;
    assert(xtpEncode(&p,1,&c,out,sizeof(out),&n)==XTP_OK);
    assert(xtpDecode((xbytesview){out,n},&c,&f)==XTP_OK);view_check(&p,&f);
    c.MaxFrameBytes=n;assert(xtpDecode((xbytesview){out,n},&c,&f)==XTP_OK);
    c.MaxFrameBytes=n-1;assert(xtpDecode((xbytesview){out,n},&c,&f)==XTP_E_LIMIT);
    c.MaxFrameBytes=n;c.MaxParams=0;assert(xtpDecode((xbytesview){out,n},&c,&f)==XTP_E_LIMIT);
    fields[255].Key=fields[0].Key;c.MaxParams=256;assert(xtpEncodeSize(&p,1,&c,&n)==XTP_E_VALUE);
}
static void retired_result_receipt(void)
{
    const unsigned char expected[32]={0x78,0x74,0x70,3,32,0,0,0,1,0,0,0,0,0,0,0,
        4,4,0,0,0,0,0,0,0xf8,0xff,0xff,0xff,0,0,0,0};
    xtppacket p={0};p.Type=XTP_ACK;p.MessageID=1;p.ResultAck=true;p.Status=XTP_STATUS_STATE_LOST;
    unsigned char wire[32];size_t n;xtpframeview f;
    assert(xtpEncode(&p,1,NULL,wire,32,&n)==XTP_OK&&!memcmp(expected,wire,32));
    assert(xtpDecode((xbytesview){expected,32},NULL,&f)==XTP_OK&&f.ResultAck&&f.Status==XTP_STATUS_STATE_LOST);
    p.Status=123;assert(xtpEncodeSize(&p,1,NULL,&n)==XTP_E_VALUE);
}
int main(void){golden();randomized();failure();limits();retired_result_receipt();puts("xtp codec: golden/order/modes/random/adversarial/limits PASS");return 0;}
