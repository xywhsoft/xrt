#include <xtp.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const unsigned char* data,size_t size)
{
    xtpcodecconfig c;xtpCodecConfigInit(&c);c.MaxFrameBytes=65536;c.MaxMetadataBytes=32768;
    xtpframeview f;
    if(xtpDecode((xbytesview){data,size},&c,&f)!=XTP_OK)return 0;
    xbytesview values[256];xtpfield fields[256];xtpparamentry e;xtpparamiter i=xtpParams(&f.Params);
    size_t count=0;
    while(xtpParamNext(&i,&e)==XTP_PARAM_FOUND){assert(e.Index==count&&count<256);
        values[count]=e.Value;fields[count]=(xtpfield){e.Key,e.Value};++count;}
    assert(count==f.Params.Count);
    xtppacket p={0};p.MessageID=f.MessageID;p.Type=f.Type;p.Status=f.Status;p.Command=f.Command;
    p.Body=f.Body;p.Replayable=f.Replayable;p.ResultAck=f.ResultAck;
    p.Params=f.Params.Kind==XTP_PARAMS_ARRAY?xtpArray(values,count):xtpTable(fields,count);
    unsigned char* out=malloc(size);if(!out)return 0;size_t written=0;
    assert(xtpEncode(&p,1,&c,out,size,&written)==XTP_OK&&written==size&&!memcmp(out,data,size));
    free(out);return 0;
}
