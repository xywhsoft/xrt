#include <xtp.h>
#include <assert.h>
typedef struct fuzz_reader_state {bool Pause;} fuzz_reader_state;
static xtpflow fuzz_frame(xtpreader* reader,const xtpframeview* frame,void* user)
{
    fuzz_reader_state* state=user;xtpframeview decoded;(void)reader;
    xtpcodecconfig c;xtpCodecConfigInit(&c);c.MaxFrameBytes=4096;c.MaxMetadataBytes=1024;c.MaxCommandBytes=64;
    assert(xtpDecode(frame->Packet,&c,&decoded)==XTP_OK&&decoded.MessageID==frame->MessageID);
    for(size_t i=0;i<frame->Params.Count;++i){xbytesview value;assert(xtpParamAt(&frame->Params,i,&value)==XTP_PARAM_FOUND);}
    if(state->Pause){state->Pause=false;return XTP_FLOW_PAUSE;}
    return XTP_FLOW_ACCEPT;
}
int LLVMFuzzerTestOneInput(const unsigned char* data,size_t size)
{
    if(!size)return 0;
    xtpreaderconfig c;xtpReaderConfigInit(&c);c.Mode=data[0]&1?XTP_INPUT_RECORD:XTP_INPUT_STREAM;
    c.Codec.MaxFrameBytes=4096;c.Codec.MaxMetadataBytes=1024;c.Codec.MaxCommandBytes=64;c.MaxWorkBytes=2048;
    fuzz_reader_state state={(data[0]&4)!=0};xtpreaderevents events={fuzz_frame,NULL};xtpreader* r=NULL;
    if(xtpReaderCreate(&c,&events,&state,&r)!=XTP_OK)return 0;
    size_t offset=1,turns=0;uint64_t now=0,due;
    while(offset<size){size_t take=1+data[offset]%64;if(take>size-offset)take=size-offset;
        bool end=c.Mode==XTP_INPUT_RECORD && (offset+take==size || ((data[0]&2)&&take%7==0));
        xtpreadprogress p;xtpresult result=xtpReaderInput(r,(xbytesview){data+offset,take},end,now++,&p);
        assert(p.Used<=take&&p.WorkBytes<=c.MaxWorkBytes);offset+=p.Used;
        if(result<0||result==XTP_END||result==XTP_CLOSED)break;
        if(result==XTP_PAUSED)assert(xtpReaderResume(r)==XTP_OK);
        result=xtpReaderDrive(r,now++,&due);if(result<0)break;
        assert(++turns<size*4+4096);
    }
    (void)xtpReaderEnd(r,now);assert(xtpReaderDestroy(r)==XTP_OK);return 0;
}
