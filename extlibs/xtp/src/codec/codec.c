#include <xtp/codec.h>
#include "../internal/xtp_codec.h"
#if defined(XTP_FEATURE_XTP_CODEC)

XTP_API void xtpCodecConfigInit(xtpcodecconfig* c)
{
    if(c)*c=(xtpcodecconfig){1048576u,16384u,256u,64u,64u,64u};
}
xtpresult xtp__CodecConfig(const xtpcodecconfig* input,xtpcodecconfig* c)
{
    if(input){if(!xrtMemRangeValid(input,sizeof(*input)))return XTP_E_ARGUMENT;*c=*input;}
    else xtpCodecConfigInit(c);
    if(c->MaxFrameBytes<32 || c->MaxFrameBytes>UINT32_MAX || c->MaxMetadataBytes>UINT32_MAX ||
        c->MaxCommandBytes>UINT16_MAX || c->MaxParams>XTP_PARAMS_MAX ||
        c->MaxKeyBytes>UINT16_MAX || !c->MaxBatch || c->MaxBatch>XTP_BATCH_MAX)return XTP_E_ARGUMENT;
    return XTP_OK;
}
static bool xtpKeyValid(xstrview key)
{
    if(!key.Size)return false;
    for(size_t i=0;i<key.Size;++i){unsigned char x=(unsigned char)key.Data[i];
        if(!((x>='a'&&x<='z')||(x>='0'&&x<='9')||x=='_'||x=='.'||x=='-'))return false;}
    return true;
}
static bool xtpCommandValid(xstrview name)
{
    if(!name.Size)return false;
    for(size_t i=0;i<name.Size;++i)if((unsigned char)name.Data[i]<0x21 || (unsigned char)name.Data[i]>0x7e)return false;
    return true;
}
/* A stack-only key source for duplicate validation. Fields are NEVER reordered.
 * Index heap sorting bounds comparison count even for adversarial input order. */
typedef struct xtpkeys { const xtpfield* Fields;const unsigned char* Table;const unsigned char* Data; } xtpkeys;
static xstrview xtpKey(const xtpkeys* k,size_t i)
{
    if(k->Fields)return k->Fields[i].Key;
    size_t start=i?xtp__U32(k->Table+(i-1)*6+2):0;
    return (xstrview){(const char*)k->Data+start,xtp__U16(k->Table+i*6)};
}
static int xtpKeyCompare(const xtpkeys* k,size_t a,size_t b)
{
    xstrview x=xtpKey(k,a),y=xtpKey(k,b);size_t n=x.Size<y.Size?x.Size:y.Size;
    int r=memcmp(x.Data,y.Data,n);return r?r:(x.Size>y.Size)-(x.Size<y.Size);
}
static void xtpKeySift(const xtpkeys* k,uint16_t* index,size_t root,size_t n)
{
    for(;;){size_t child=root*2+1;if(child>=n)return;
        if(child+1<n && xtpKeyCompare(k,index[child],index[child+1])<0)++child;
        if(xtpKeyCompare(k,index[root],index[child])>=0)return;
        uint16_t t=index[root];index[root]=index[child];index[child]=t;root=child;}
}
static bool xtpUnique(const xtpkeys* k,size_t n)
{
    uint16_t index[XTP_PARAMS_MAX];
    for(size_t i=0;i<n;++i)index[i]=(uint16_t)i;
    for(size_t i=n/2;i>0;--i)xtpKeySift(k,index,i-1,n);
    for(size_t i=n;i>1;--i){uint16_t t=index[0];index[0]=index[i-1];index[i-1]=t;xtpKeySift(k,index,0,i-1);}
    for(size_t i=1;i<n;++i)if(!xtpKeyCompare(k,index[i-1],index[i]))return false;
    return true;
}
static xtpresult xtpFields(xtpmessagekind type,uint64_t id,xtpcommandkind route,
    size_t cmd,uint32_t command,size_t params,xtpparamkind pk,size_t body,int32_t status,bool replay,bool ack)
{
    bool routed=type==XTP_REQUEST||type==XTP_PUSH||type==XTP_NOTIFY;
    if(type<XTP_REQUEST||type>XTP_NOTIFY)return XTP_E_TYPE;
    if(pk!=XTP_PARAMS_ARRAY&&pk!=XTP_PARAMS_TABLE)return XTP_E_VALUE;
    if(!xtp__Status(status))return XTP_E_VALUE;
    if(type==XTP_NOTIFY ? id!=0 : id==0)return XTP_E_VALUE;
    if(routed){
        if(status || (route!=XTP_COMMAND_ID&&route!=XTP_COMMAND_NAME))return XTP_E_VALUE;
        if(route==XTP_COMMAND_ID ? cmd!=0 : (!cmd||command!=0))return XTP_E_VALUE;
    }else if(route!=XTP_COMMAND_NONE||cmd||command)return XTP_E_VALUE;
    if(replay && type!=XTP_REQUEST && type!=XTP_PUSH)return XTP_E_VALUE;
    if(ack && (type!=XTP_ACK||(status&&status!=XTP_STATUS_STATE_LOST)))return XTP_E_VALUE;
    if(type==XTP_ACK && (params||pk!=XTP_PARAMS_TABLE||body||status==XTP_STATUS_DEADLINE_EXCEEDED))return XTP_E_VALUE;
    return XTP_OK;
}
typedef struct xtpmeasure {uint32_t Size,Meta;uint16_t Command;} xtpmeasure;
static xtpresult xtpMeasure(const xtppacket* p,const xtpcodecconfig* c,xtpmeasure* f,
    const void* out,size_t outn,const void* scalar,size_t scalarn)
{
    size_t cmd=p->Command.Kind==XTP_COMMAND_NAME?p->Command.As.Name.Size:0;
    uint32_t id=p->Command.Kind==XTP_COMMAND_ID?p->Command.As.ID:0;
    xtpresult r=xtpFields(p->Type,p->MessageID,p->Command.Kind,cmd,id,p->Params.Count,p->Params.Kind,
        p->Body.Size,p->Status,p->Replayable,p->ResultAck);
    if(r!=XTP_OK)return r;
    if(cmd>UINT16_MAX||p->Body.Size>UINT32_MAX||p->Params.Count>c->MaxParams||cmd>c->MaxCommandBytes)return XTP_E_LIMIT;
    if(!xtp__Bytes(p->Body))return XTP_E_ARGUMENT;
    const void* desc=p->Params.Kind==XTP_PARAMS_ARRAY?(const void*)p->Params.As.Array:(const void*)p->Params.As.Table;
    size_t dn=p->Params.Count*(p->Params.Kind==XTP_PARAMS_ARRAY?sizeof(xbytesview):sizeof(xtpfield));
    if(!xrtMemRangeValid(desc,dn))return XTP_E_ARGUMENT;
    if(xtp__Overlap(out,outn,desc,dn)||xtp__Overlap(scalar,scalarn,desc,dn)||
       xtp__Overlap(out,outn,p->Body.Data,p->Body.Size)||xtp__Overlap(scalar,scalarn,p->Body.Data,p->Body.Size))return XTP_E_ARGUMENT;
    if(cmd){xstrview s=p->Command.As.Name;if(!xtp__String(s))return XTP_E_ARGUMENT;
        if(xtp__Overlap(out,outn,s.Data,s.Size)||xtp__Overlap(scalar,scalarn,s.Data,s.Size))return XTP_E_ARGUMENT;
        if(!xtpCommandValid(s))return XTP_E_VALUE;}
    uint64_t meta=p->Params.Count?2+p->Params.Count*(p->Params.Kind==XTP_PARAMS_ARRAY?4u:6u):0;
    for(size_t i=0;i<p->Params.Count;++i){
        xbytesview v=p->Params.Kind==XTP_PARAMS_ARRAY?p->Params.As.Array[i]:p->Params.As.Table[i].Value;
        if(!xtp__Bytes(v))return XTP_E_ARGUMENT;
        if(v.Size>UINT32_MAX||xtp__Overlap(out,outn,v.Data,v.Size)||xtp__Overlap(scalar,scalarn,v.Data,v.Size))return v.Size>UINT32_MAX?XTP_E_LIMIT:XTP_E_ARGUMENT;
        meta+=v.Size;
        if(p->Params.Kind==XTP_PARAMS_TABLE){xstrview k=p->Params.As.Table[i].Key;
            if(k.Size>c->MaxKeyBytes)return XTP_E_LIMIT;
            if(!xtp__String(k)||xtp__Overlap(out,outn,k.Data,k.Size)||xtp__Overlap(scalar,scalarn,k.Data,k.Size))return XTP_E_ARGUMENT;
            if(!xtpKeyValid(k))return XTP_E_VALUE;
            meta+=k.Size;}
        if(meta>c->MaxMetadataBytes)return XTP_E_LIMIT;
    }
    if(meta>c->MaxMetadataBytes || (uint64_t)32+cmd+meta+p->Body.Size>c->MaxFrameBytes)return XTP_E_LIMIT;
    if(p->Params.Kind==XTP_PARAMS_TABLE && p->Params.Count>1){xtpkeys k={p->Params.As.Table,NULL,NULL};
        if(!xtpUnique(&k,p->Params.Count))return XTP_E_VALUE;}
    f->Command=(uint16_t)cmd;f->Meta=(uint32_t)meta;f->Size=(uint32_t)(32+cmd+meta+p->Body.Size);return XTP_OK;
}
static xtpresult xtpPreflight(const xtppacket* p,size_t n,const xtpcodecconfig* input,
    xtpmeasure* frames,size_t* total,void* out,size_t outn,void* scalar,size_t scalarn)
{
    xtpcodecconfig c;xtpresult r=xtp__CodecConfig(input,&c);if(r!=XTP_OK)return r;
    if(!n||n>c.MaxBatch||!xrtMemRangeValid(p,n*sizeof(*p)))return XTP_E_ARGUMENT;
    if(!xrtMemRangeValid(scalar,scalarn)||!xrtMemRangeValid(out,outn))return XTP_E_ARGUMENT;
    if(xtp__Overlap(scalar,scalarn,p,n*sizeof(*p))||xtp__Overlap(out,outn,p,n*sizeof(*p))||
       xtp__Overlap(out,outn,scalar,scalarn)||xtp__Overlap(scalar,scalarn,input,input?sizeof(*input):0)||
       xtp__Overlap(out,outn,input,input?sizeof(*input):0))return XTP_E_ARGUMENT;
    uint64_t size=0;
    for(size_t i=0;i<n;++i){r=xtpMeasure(p+i,&c,frames+i,out,outn,scalar,scalarn);if(r!=XTP_OK)return r;
        size+=frames[i].Size;if(size>SIZE_MAX)return XTP_E_LIMIT;}
    *total=(size_t)size;return XTP_OK;
}
XTP_API xtpresult xtpEncodeSize(const xtppacket* p,size_t n,const xtpcodecconfig* c,size_t* required)
{
    xtpmeasure frames[XTP_BATCH_MAX];size_t size;
    if(!required)return XTP_E_ARGUMENT;
    xtpresult r=xtpPreflight(p,n,c,frames,&size,NULL,0,required,sizeof(*required));
    if(r==XTP_OK)*required=size;
    return r;
}
static void xtpCopy(unsigned char* out,const void* data,size_t n) {if(n)memcpy(out,data,n);}
static void xtpEmit(const xtppacket* p,const xtpmeasure* f,unsigned char* out)
{
    memcpy(out,"xtp\3",4);xtp__P32(out+4,f->Size);xtp__P64(out+8,p->MessageID);out[16]=(unsigned char)p->Type;
    out[17]=(unsigned char)((p->Command.Kind==XTP_COMMAND_ID?1:0)|(p->Params.Kind==XTP_PARAMS_ARRAY?2:0)|
        (p->ResultAck?4:0)|(p->Replayable?8:0));
    xtp__P16(out+18,f->Command);xtp__P32(out+20,f->Meta);xtp__P32(out+24,(uint32_t)p->Status);
    xtp__P32(out+28,p->Command.Kind==XTP_COMMAND_ID?p->Command.As.ID:0);
    if(f->Command)xtpCopy(out+32,p->Command.As.Name.Data,f->Command);
    unsigned char* m=out+32+f->Command;size_t count=p->Params.Count;
    if(count){size_t stride=p->Params.Kind==XTP_PARAMS_ARRAY?4:6,end=0;unsigned char* data=m+2+stride*count;xtp__P16(m,(uint16_t)count);
        for(size_t i=0;i<count;++i){xbytesview v=p->Params.Kind==XTP_PARAMS_ARRAY?p->Params.As.Array[i]:p->Params.As.Table[i].Value;
            if(p->Params.Kind==XTP_PARAMS_TABLE){xstrview k=p->Params.As.Table[i].Key;xtp__P16(m+2+i*6,(uint16_t)k.Size);
                xtpCopy(data+end,k.Data,k.Size);end+=k.Size;}
            xtpCopy(data+end,v.Data,v.Size);end+=v.Size;xtp__P32(m+2+i*stride+(stride==6?2:0),(uint32_t)end);}}
    xtpCopy(out+32+f->Command+f->Meta,p->Body.Data,p->Body.Size);
}
XTP_API xtpresult xtpEncode(const xtppacket* p,size_t n,const xtpcodecconfig* c,void* output,size_t capacity,size_t* written)
{
    xtpmeasure frames[XTP_BATCH_MAX];size_t size;
    if(!written||!output)return XTP_E_ARGUMENT;
    xtpresult r=xtpPreflight(p,n,c,frames,&size,output,capacity,written,sizeof(*written));if(r!=XTP_OK)return r;
    if(size>capacity)return XTP_E_CAPACITY;
    unsigned char* out=output;for(size_t i=0;i<n;++i){xtpEmit(p+i,frames+i,out);out+=frames[i].Size;}
    *written=size;return XTP_OK;
}
static xtpresult xtpMetadata(xbytesview wire,xtpparamkind kind,const xtpcodecconfig* c,xtpparamsview* out)
{
    *out=(xtpparamsview){kind,0,wire};if(!wire.Size)return XTP_OK;
    if(wire.Size<2)return XTP_E_LENGTH;
    size_t n=xtp__U16(wire.Data),stride=kind==XTP_PARAMS_ARRAY?4:6,head=2+stride*n,prev=0;
    if(!n)return XTP_E_VALUE;
    if(n>c->MaxParams)return XTP_E_LIMIT;
    if(head>wire.Size)return XTP_E_LENGTH;
    const unsigned char* table=wire.Data+2;const unsigned char* data=wire.Data+head;size_t bytes=wire.Size-head;
    for(size_t i=0;i<n;++i){size_t end=xtp__U32(table+i*stride+(stride==6?2:0));
        if(end<prev||end>bytes)return XTP_E_LENGTH;
        if(kind==XTP_PARAMS_TABLE){size_t key=xtp__U16(table+i*6);
            if(key>c->MaxKeyBytes)return XTP_E_LIMIT;
            if(key>end-prev)return XTP_E_LENGTH;
            if(!xtpKeyValid((xstrview){(const char*)data+prev,key}))return XTP_E_VALUE;}
        prev=end;}
    if(prev!=bytes)return XTP_E_LENGTH;
    if(kind==XTP_PARAMS_TABLE&&n>1){xtpkeys k={NULL,table,data};if(!xtpUnique(&k,n))return XTP_E_VALUE;}
    out->Count=n;return XTP_OK;
}
xtpresult xtp__DecodePrefix(const unsigned char* p,size_t size,const xtpcodecconfig* config,xtpframeview* frame)
{
    unsigned flags=p[17];size_t cmd=xtp__U16(p+18),meta=xtp__U32(p+20);xtpresult r;
    if(flags&~15u)return XTP_E_VALUE;
    xtpframeview f;memset(&f,0,sizeof(f));f.Type=(xtpmessagekind)p[16];f.MessageID=xtp__U64(p+8);
    uint32_t status=xtp__U32(p+24);f.Status=status<=INT32_MAX?(int32_t)status:-(int32_t)(UINT32_MAX-status)-1;
    f.Command.Kind=(f.Type==XTP_REQUEST||f.Type==XTP_PUSH||f.Type==XTP_NOTIFY)?
        (flags&1?XTP_COMMAND_ID:XTP_COMMAND_NAME):XTP_COMMAND_NONE;
    f.Replayable=(flags&8)!=0;f.ResultAck=(flags&4)!=0;
    if(f.Command.Kind==XTP_COMMAND_ID)f.Command.As.ID=xtp__U32(p+28);
    else if(f.Command.Kind==XTP_COMMAND_NAME)f.Command.As.Name=(xstrview){(const char*)p+32,cmd};
    f.Body=(xbytesview){p+32+cmd+meta,size-32-cmd-meta};f.Packet=(xbytesview){p,size};
    if(f.Command.Kind==XTP_COMMAND_NONE && (flags&1))return XTP_E_VALUE;
    r=xtpFields(f.Type,f.MessageID,f.Command.Kind,cmd,xtp__U32(p+28),meta,
        flags&2?XTP_PARAMS_ARRAY:XTP_PARAMS_TABLE,f.Body.Size,f.Status,f.Replayable,f.ResultAck);
    if(r!=XTP_OK)return r;
    if(f.Command.Kind==XTP_COMMAND_NAME&&!xtpCommandValid(f.Command.As.Name))return XTP_E_VALUE;
    r=xtpMetadata((xbytesview){p+32+cmd,meta},flags&2?XTP_PARAMS_ARRAY:XTP_PARAMS_TABLE,config,&f.Params);
    if(r!=XTP_OK)return r;
    *frame=f;return XTP_OK;
}
XTP_API xtpresult xtpDecode(xbytesview in,const xtpcodecconfig* input,xtpframeview* frame)
{
    xtpcodecconfig c;size_t size;xtpresult r=xtp__CodecConfig(input,&c);if(r!=XTP_OK)return r;
    if(!xtp__Bytes(in)||!xrtMemRangeValid(frame,sizeof(*frame))||
        xtp__Overlap(in.Data,in.Size,frame,sizeof(*frame))||xtp__Overlap(input,input?sizeof(*input):0,frame,sizeof(*frame)))return XTP_E_ARGUMENT;
    if(in.Size<32)return XTP_E_TRUNCATED;
    r=xtp__Envelope(in.Data,&c,&size);if(r!=XTP_OK)return r;
    if(in.Size<size)return XTP_E_TRUNCATED;
    if(in.Size>size)return XTP_E_LENGTH;
    return xtp__DecodePrefix(in.Data,size,&c,frame);
}
static bool xtpParamOutput(const xtpparamsview* p,void* out,size_t bytes)
{
    return p&&xrtMemRangeValid(p,sizeof(*p))&&xrtMemRangeValid(out,bytes)&&
        xtp__Bytes(p->_Wire)&&!xtp__Overlap(out,bytes,p,sizeof(*p))&&!xtp__Overlap(out,bytes,p->_Wire.Data,p->_Wire.Size);
}
static xtpparamentry xtpEntry(const xtpparamsview* p,size_t i)
{
    size_t stride=p->Kind==XTP_PARAMS_ARRAY?4:6;const unsigned char* table=p->_Wire.Data+2;
    const unsigned char* data=table+stride*p->Count;size_t start=i?xtp__U32(table+(i-1)*stride+(stride==6?2:0)):0;
    size_t end=xtp__U32(table+i*stride+(stride==6?2:0)),key=stride==6?xtp__U16(table+i*stride):0;
    xtpparamentry e={i,stride==6,{NULL,0},{data+start+key,end-start-key}};
    if(key)e.Key=(xstrview){(const char*)data+start,key};
    return e;
}
XTP_API xtpparamresult xtpParamAt(const xtpparamsview* p,size_t i,xbytesview* value)
{
    if(!xtpParamOutput(p,value,sizeof(*value)))return XTP_PARAM_ARGUMENT;
    *value=(xbytesview){NULL,0};
    if(p->Kind!=XTP_PARAMS_ARRAY&&p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    if(i>=p->Count)return XTP_PARAM_MISSING;
    *value=xtpEntry(p,i).Value;return XTP_PARAM_FOUND;
}
XTP_API xtpparamresult xtpParamFind(const xtpparamsview* p,xstrview key,xbytesview* value)
{
    if(!xtpParamOutput(p,value,sizeof(*value))||!xtp__String(key)||xtp__Overlap(value,sizeof(*value),key.Data,key.Size))return XTP_PARAM_ARGUMENT;
    *value=(xbytesview){NULL,0};
    if(p->Kind==XTP_PARAMS_ARRAY)return XTP_PARAM_WRONG_MODE;
    if(p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    for(size_t i=0;i<p->Count;++i){xtpparamentry e=xtpEntry(p,i);
        if(e.Key.Size==key.Size&&(!key.Size||!memcmp(e.Key.Data,key.Data,key.Size))){*value=e.Value;return XTP_PARAM_FOUND;}}
    return XTP_PARAM_MISSING;
}
XTP_API xtpparamiter xtpParams(const xtpparamsview* p)
{ xtpparamiter i={p,0};return i; }
XTP_API xtpparamresult xtpParamNext(xtpparamiter* i,xtpparamentry* e)
{
    if(!i||!xtpParamOutput(i->_Params,e,sizeof(*e))||xtp__Overlap(i,sizeof(*i),e,sizeof(*e)))return XTP_PARAM_ARGUMENT;
    memset(e,0,sizeof(*e));const xtpparamsview* p=i->_Params;
    if(p->Kind!=XTP_PARAMS_ARRAY&&p->Kind!=XTP_PARAMS_TABLE)return XTP_PARAM_ARGUMENT;
    if(i->_Next>=p->Count)return XTP_PARAM_MISSING;
    *e=xtpEntry(p,i->_Next++);return XTP_PARAM_FOUND;
}
#endif
