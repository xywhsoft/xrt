#include <xtp.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct reader_test {
    const unsigned char* Wire;size_t Size,Accepted,Calls,Drops;
    bool Pause,Stop,CheckBorrow;
    xtpresult LastDrop;
} reader_test;
static xtpflow frame_event(xtpreader* r,const xtpframeview* f,void* user)
{
    reader_test* t=user;++t->Calls;
    assert(f->Packet.Size==t->Size&&!memcmp(f->Packet.Data,t->Wire,t->Size));
    if(t->CheckBorrow)assert(f->Packet.Data==t->Wire);
    assert(xtpReaderDestroy(r)==XTP_E_STATE);
    if(t->Pause){t->Pause=false;return XTP_FLOW_PAUSE;}
    ++t->Accepted;return t->Stop?XTP_FLOW_STOP:XTP_FLOW_ACCEPT;
}
static void drop_event(xtpreader* r,xtpresult reason,void* user)
{reader_test* t=user;++t->Drops;t->LastDrop=reason;assert(xtpReaderDestroy(r)==XTP_E_STATE);}
static xtpreader* create(reader_test* t,xtpreaderconfig* c)
{xtpreaderevents e={frame_event,drop_event};xtpreader* r=NULL;assert(xtpReaderCreate(c,&e,t,&r)==XTP_OK);return r;}
static size_t make_frame(unsigned char* out,size_t cap,size_t body)
{
    unsigned char* bytes=malloc(body?body:1);assert(bytes);memset(bytes,0x6e,body);
    xtpfield fields[]={{XRT_STR_INIT("z"),XRT_BYTES_INIT("\1")},{XRT_STR_INIT("a"),XRT_BYTES_INIT("\0\2")}};
    xtppacket p={0};p.Type=XTP_REQUEST;p.MessageID=7;p.Command=xtpCmdName(XRT_STR_LITERAL("move"));
    p.Params=xtpTable(fields,2);p.Body=(xbytesview){bytes,body};size_t n;
    assert(xtpEncode(&p,1,NULL,out,cap,&n)==XTP_OK);free(bytes);return n;
}
static void splits(void)
{
    unsigned char wire[256];size_t n=make_frame(wire,sizeof(wire),33);
    for(size_t k=0;k<=n;++k){reader_test t={0};t.Wire=wire;t.Size=n;xtpreader* r=create(&t,NULL);xtpreadprogress p;
        xtpresult a=xtpReaderInput(r,(xbytesview){wire,k},false,1,&p);assert(a>=0&&p.Used==k);
        a=xtpReaderInput(r,(xbytesview){wire+k,n-k},false,2,&p);assert(a>=0&&p.Used==n-k);
        assert(t.Accepted==1&&!t.Drops);assert(xtpReaderEnd(r,3)==XTP_END);assert(xtpReaderEnd(r,3)==XTP_END);
        assert(xtpReaderInput(r,(xbytesview){wire,n},false,3,&p)==XTP_END&&p.Used==0);
        assert(xtpReaderReset(r)==XTP_OK);assert(xtpReaderDestroy(r)==XTP_OK);}
    reader_test t={0};t.Wire=wire;t.Size=n;xtpreader* r=create(&t,NULL);xtpreadprogress p;
    for(size_t k=0;k<n;++k)assert(xtpReaderInput(r,(xbytesview){wire+k,1},false,k,&p)>=0&&p.Used==1);
    assert(t.Accepted==1);assert(xtpReaderDestroy(r)==XTP_OK);
}
static void coalesced(void)
{
    unsigned char wire[256],batch[1024];size_t n=make_frame(wire,sizeof(wire),5);
    for(size_t i=0;i<4;++i)memcpy(batch+i*n,wire,n);
    xtpreaderconfig c;xtpReaderConfigInit(&c);c.MaxFrames=2;reader_test t={0};t.Wire=wire;t.Size=n;
    xtpreader* r=create(&t,&c);xtpreadprogress p;
    assert(xtpReaderInput(r,(xbytesview){batch,4*n},false,0,&p)==XTP_AGAIN&&p.Used==2*n&&p.Frames==2);
    assert(xtpReaderInput(r,(xbytesview){batch+p.Used,2*n},false,1,&p)==XTP_OK&&p.Used==2*n&&t.Accepted==4);
    assert(xtpReaderDestroy(r)==XTP_OK);
}
static void record_and_pause(void)
{
    unsigned char wire[256],bad[512];size_t n=make_frame(wire,sizeof(wire),5);
    xtpreaderconfig c;xtpReaderConfigInit(&c);c.Mode=XTP_INPUT_RECORD;
    reader_test t={0};t.Wire=wire;t.Size=n;t.Pause=true;xtpreader* r=create(&t,&c);xtpreadprogress p;
    assert(xtpReaderInput(r,(xbytesview){wire,n},true,0,&p)==XTP_PAUSED&&p.Used==0&&t.Calls==1);
    assert(xtpReaderInput(r,(xbytesview){wire,n},true,0,&p)==XTP_PAUSED&&p.Used==0&&t.Calls==1);
    assert(xtpReaderResume(r)==XTP_OK);
    assert(xtpReaderInput(r,(xbytesview){wire,n},true,1,&p)==XTP_OK&&p.Used==n&&t.Accepted==1);
    t.Pause=true;
    assert(xtpReaderInput(r,(xbytesview){wire,1},false,2,&p)==XTP_MORE&&p.Used==1);
    assert(xtpReaderInput(r,(xbytesview){wire+1,n-1},true,3,&p)==XTP_PAUSED&&p.Used==n-1);
    assert(xtpReaderResume(r)==XTP_OK);
    assert(xtpReaderInput(r,(xbytesview){NULL,0},false,4,&p)==XTP_OK&&t.Accepted==2);
    memcpy(bad,wire,n);bad[0]='?';memcpy(bad+n,wire,n);
    assert(xtpReaderInput(r,(xbytesview){bad,2*n},true,5,&p)==XTP_DROPPED&&p.Used==2*n&&t.Accepted==2);
    assert(xtpReaderInput(r,(xbytesview){wire,n},true,6,&p)==XTP_OK&&t.Accepted==3);
    assert(xtpReaderInput(r,(xbytesview){wire,n-1},true,7,&p)==XTP_DROPPED&&t.LastDrop==XTP_E_TRUNCATED);
    assert(xtpReaderInput(r,(xbytesview){wire,n},true,8,&p)==XTP_OK&&t.Accepted==4);
    assert(xtpReaderInput(r,(xbytesview){NULL,0},true,9,&p)==XTP_OK);
    assert(xtpReaderDestroy(r)==XTP_OK);
}
typedef struct alloc_test {size_t Calls,FailAt,Live,Bytes,Peak;} alloc_test;
typedef struct alloc_header {size_t Size;void* Align;} alloc_header;
static void* test_realloc(void* user,void* memory,size_t n)
{
    alloc_test* a=user;alloc_header* old=memory?(alloc_header*)memory-1:NULL;
    if(!n){if(old){a->Bytes-=old->Size;--a->Live;free(old);}return NULL;}
    if(++a->Calls==a->FailAt)return NULL;
    alloc_header* p=malloc(sizeof(*p)+n);if(!p)return NULL;p->Size=n;
    size_t peak=a->Bytes+n;if(peak>a->Peak)a->Peak=peak;
    if(old){memcpy(p+1,memory,old->Size<n?old->Size:n);a->Bytes-=old->Size;free(old);}else ++a->Live;
    a->Bytes+=n;return p+1;
}
static void allocation_and_budget(void)
{
    size_t body=500000;unsigned char* wire=malloc(body+128);assert(wire);size_t n=make_frame(wire,body+128,body);
    alloc_test a={0};xtpreaderconfig c;xtpReaderConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&a;
    reader_test t={0};t.Wire=wire;t.Size=n;t.CheckBorrow=true;xtpreader* r=create(&t,&c);xtpreadprogress p;
    assert(xtpReaderInput(r,(xbytesview){wire,n},false,0,&p)==XTP_OK&&p.Used==n&&a.Calls==1);
    assert(xtpReaderDestroy(r)==XTP_OK&&!a.Live&&!a.Bytes);
    t.CheckBorrow=false;t.Accepted=0;a=(alloc_test){0};c.MaxWorkBytes=20000;r=create(&t,&c);
    assert(xtpReaderInput(r,(xbytesview){wire,1},false,1,&p)==XTP_MORE);
    size_t offset=1;unsigned turns=0;
    while(offset<n){xtpresult result=xtpReaderInput(r,(xbytesview){wire+offset,n-offset},false,2,&p);
        assert(result>=0&&p.WorkBytes<=c.MaxWorkBytes&&p.Used<=n-offset);offset+=p.Used;assert(++turns<100);}
    if(!t.Accepted)assert(xtpReaderInput(r,(xbytesview){NULL,0},false,2,&p)==XTP_OK);
    assert(t.Accepted==1&&turns>10);xtpreaderinfo info={0};assert(xtpReaderInfo(r,&info)==XTP_OK&&info.PeakReservedBytes<=c.MaxStorageBytes);
    assert(xtpReaderDestroy(r)==XTP_OK&&!a.Live&&!a.Bytes);
    for(size_t fail=1;fail<5;++fail){a=(alloc_test){0};a.FailAt=fail;xtpreaderevents events={frame_event,drop_event};r=NULL;
        xtpresult result=xtpReaderCreate(&c,&events,&t,&r);
        if(result==XTP_OK){result=xtpReaderInput(r,(xbytesview){wire,n<200? n:200},false,3,&p);
            if(result>=0)result=xtpReaderInput(r,(xbytesview){wire+200,1000},false,4,&p);
            if(result>=0)result=xtpReaderInput(r,(xbytesview){wire+1200,n-1200},false,5,&p);
            assert(result==XTP_E_MEMORY);assert(xtpReaderDestroy(r)==XTP_OK);}
        else assert(result==XTP_E_MEMORY&&r==NULL);
        assert(!a.Live&&!a.Bytes);}
    free(wire);
}
static void malformed_and_clock(void)
{
    unsigned char wire[256],bad[512];size_t n=make_frame(wire,sizeof(wire),1);
    reader_test t={0};t.Wire=wire;t.Size=n;xtpreaderconfig c;xtpReaderConfigInit(&c);c.ReceiveTimeoutUs=10;
    xtpreader* r=create(&t,&c);xtpreadprogress p;uint64_t due;
    assert(xtpReaderInput(r,(xbytesview){wire,1},false,0,&p)==XTP_MORE);
    assert(xtpReaderDrive(r,9,&due)==XTP_OK&&due==10);
    assert(xtpReaderDrive(r,10,&due)==XTP_E_DEADLINE);
    assert(xtpReaderInput(r,(xbytesview){wire,n},false,11,&p)==XTP_E_DEADLINE&&p.Used==0);
    assert(xtpReaderReset(r)==XTP_OK);
    memcpy(bad,wire,n);bad[0]='?';assert(xtpReaderInput(r,(xbytesview){bad,n},false,12,&p)==XTP_E_MAGIC&&p.Used==0);
    assert(xtpReaderDestroy(r)==XTP_OK);
    r=create(&t,NULL);memcpy(bad,wire,n);bad[17]|=0x80;memcpy(bad+n,wire,n);
    assert(xtpReaderInput(r,(xbytesview){bad,2*n},false,0,&p)==XTP_DROPPED&&p.Used==2*n&&t.Accepted==1&&t.Drops==1);
    assert(xtpReaderDestroy(r)==XTP_OK);
    /* A bad fixed header declares a large Body; no Body storage is allocated. */
    xtppacket empty={0};empty.Type=XTP_REQUEST;empty.MessageID=1;empty.Command=xtpCmdID(1);size_t z;
    assert(xtpEncode(&empty,1,NULL,bad,sizeof(bad),&z)==XTP_OK);bad[17]=0x80;
    bad[4]=0;bad[5]=0;bad[6]=8;bad[7]=0; /* 524288 bytes */
    alloc_test a={0};xtpReaderConfigInit(&c);c.Realloc=test_realloc;c.AllocatorUser=&a;r=create(&t,&c);
    assert(xtpReaderInput(r,(xbytesview){bad,32},false,0,&p)==XTP_MORE&&p.Used==32&&a.Calls==1);
    assert(xtpReaderDestroy(r)==XTP_OK&&!a.Live);
    xtpReaderConfigInit(&c);c.MaxConsecutiveBad=2;c.MaxBad=2;r=create(&t,&c);
    memcpy(bad,wire,n);bad[17]|=0x80;memcpy(bad+n,bad,n);
    assert(xtpReaderInput(r,(xbytesview){bad,2*n},false,0,&p)==XTP_E_VALUE&&p.Dropped==2);
    assert(xtpReaderDestroy(r)==XTP_OK);
}
static void deferred_complete(void)
{
    unsigned char wire[128];size_t n=make_frame(wire,sizeof(wire),0);
    reader_test t={0};t.Wire=wire;t.Size=n;xtpreaderconfig c;xtpReaderConfigInit(&c);
    c.Mode=XTP_INPUT_RECORD;c.Codec.MaxMetadataBytes=n-36;c.Codec.MaxCommandBytes=4;c.MaxWorkBytes=n;
    xtpreader* r=create(&t,&c);xtpreadprogress p;uint64_t due;
    assert(xtpReaderInput(r,(xbytesview){wire,1},false,0,&p)==XTP_MORE);
    assert(xtpReaderInput(r,(xbytesview){wire+1,n-1},true,1,&p)==XTP_AGAIN&&p.Used==n-1);
    assert(xtpReaderEnd(r,2)==XTP_AGAIN);
    /* All bytes arrived: later scheduling delay is not a receive timeout. */
    assert(xtpReaderDrive(r,10000000,&due)==XTP_OK&&t.Accepted==1&&due==0);
    assert(xtpReaderEnd(r,10000000)==XTP_END);
    assert(xtpReaderDestroy(r)==XTP_OK);
}
static void bad_frame_budget(void)
{
    unsigned char wire[256],bad[768];size_t n=make_frame(wire,sizeof(wire),1);
    for(size_t i=0;i<3;++i){memcpy(bad+i*n,wire,n);bad[i*n+17]|=0x80;}
    reader_test t={0};xtpreaderconfig c;xtpReaderConfigInit(&c);c.MaxFrames=1;
    xtpreader* r=create(&t,&c);size_t offset=0,turns=0;xtpreadprogress p;
    while(offset<3*n){assert(xtpReaderInput(r,(xbytesview){bad+offset,3*n-offset},false,0,&p)>=0);
        assert(p.Frames+p.Dropped<=1);offset+=p.Used;assert(++turns<16);}
    assert(t.Drops==3&&!t.Accepted&&xtpReaderEnd(r,0)==XTP_END);assert(xtpReaderDestroy(r)==XTP_OK);
}
int main(void)
{splits();coalesced();record_and_pause();allocation_and_budget();malformed_and_clock();deferred_complete();bad_frame_budget();puts("xtp reader: splits/coalescing/pause/records/budgets/OOM/deadlines PASS");return 0;}
