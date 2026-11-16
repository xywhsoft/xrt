#include <xtp/reader.h>
#include "../internal/xtp_codec.h"
#if defined(XTP_FEATURE_XTP_READER)
struct xtpreader {
    xtpreaderconfig Config;xtpreaderevents Events;void* User;
    xtpframeview Validated;
    unsigned char Header[32];unsigned char* Buffer;
    size_t Used,Capacity,Need,Prefix,Discard,Peak;
    uint64_t LastNow,Deadline,Frames,Dropped;
    uint32_t Consecutive;
    xtpresult Failure,DropReason;
    bool PrefixChecked,Paused,Ended,Busy,DropRecord,PendingEnd;
};
static void* xtpReaderMemory(void* user,void* memory,size_t n)
{(void)user;if(!n){xrtFree(memory);return NULL;}return xrtRealloc(memory,n);}
XTP_API void xtpReaderConfigInit(xtpreaderconfig* c)
{
    if(!c)return;
    memset(c,0,sizeof(*c));xtpCodecConfigInit(&c->Codec);c->RetainBytes=65536;
    c->MaxStorageBytes=3u*1024*1024;c->MaxWorkBytes=262144;c->MaxFrames=64;
    c->MaxConsecutiveBad=8;c->MaxBad=64;c->ReceiveTimeoutUs=5000000;
}
XTP_API xtpresult xtpReaderCreate(const xtpreaderconfig* input,const xtpreaderevents* events,void* user,xtpreader** out)
{
    xtpreaderconfig c;
    if(!out||!events||!events->Frame||!xrtMemRangeValid(out,sizeof(*out))||
        !xrtMemRangeValid(events,sizeof(*events))||xtp__Overlap(out,sizeof(*out),events,sizeof(*events)))return XTP_E_ARGUMENT;
    if(input){if(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(out,sizeof(*out),input,sizeof(*input)))return XTP_E_ARGUMENT;c=*input;}
    else xtpReaderConfigInit(&c);
    if(xtp__ReaderConfig(&c)!=XTP_OK)return XTP_E_ARGUMENT;
    if(!c.Realloc)c.Realloc=xtpReaderMemory;
    xtpreader* r=c.Realloc(c.AllocatorUser,NULL,sizeof(*r));if(!r)return XTP_E_MEMORY;
    memset(r,0,sizeof(*r));r->Config=c;r->Events=*events;r->User=user;r->Peak=sizeof(*r);*out=r;return XTP_OK;
}
xtpresult xtp__ReaderConfig(const xtpreaderconfig* c)
{
    xtpcodecconfig checked;
    return xtp__CodecConfig(&c->Codec,&checked)!=XTP_OK || c->Mode>XTP_INPUT_RECORD || c->Mode<XTP_INPUT_STREAM ||
        c->MaxStorageBytes<sizeof(xtpreader)+32 || !c->MaxFrames || !c->MaxBad || !c->MaxConsecutiveBad ||
        !c->ReceiveTimeoutUs || c->MaxWorkBytes<(uint64_t)32+c->Codec.MaxCommandBytes+c->Codec.MaxMetadataBytes?XTP_E_ARGUMENT:XTP_OK;
}
static void xtpReaderRelease(xtpreader* r)
{if(r->Buffer)r->Config.Realloc(r->Config.AllocatorUser,r->Buffer,0);r->Buffer=NULL;r->Capacity=0;}
XTP_API xtpresult xtpReaderDestroy(xtpreader* r)
{
    if(!r)return XTP_OK;
    if(r->Busy)return XTP_E_STATE;
    r->Busy=true;
    xtpreallocproc f=r->Config.Realloc;void* u=r->Config.AllocatorUser;xtpReaderRelease(r);f(u,r,0);return XTP_OK;
}
static xtpresult xtpReaderReserve(xtpreader* r,size_t need)
{
    if(need<=r->Capacity)return XTP_OK;
    size_t n=r->Capacity?r->Capacity:256;
    if(n>r->Need)n=r->Need;
    while(n<need){if(n>r->Need/2){n=r->Need;break;}n*=2;}
    if(n>r->Config.MaxStorageBytes-sizeof(*r) || r->Capacity>r->Config.MaxStorageBytes-sizeof(*r)-n)return XTP_E_LIMIT;
    size_t peak=sizeof(*r)+r->Capacity+n;if(peak>r->Peak)r->Peak=peak;
    bool fresh=r->Buffer==NULL;void* p=r->Config.Realloc(r->Config.AllocatorUser,r->Buffer,n);
    if(!p)return XTP_E_MEMORY;
    r->Buffer=p;r->Capacity=n;if(fresh)memcpy(r->Buffer,r->Header,32);return XTP_OK;
}
static void xtpReaderClear(xtpreader* r)
{
    r->Used=r->Need=r->Prefix=r->Discard=0;r->PrefixChecked=false;r->Deadline=0;
    if(r->Capacity>r->Config.RetainBytes)xtpReaderRelease(r);
}
static xtpresult xtpReaderBad(xtpreader* r,xtpresult reason,xtpreadprogress* p)
{
    ++r->Dropped;++r->Consecutive;if(p)++p->Dropped;
    if(r->Events.Drop)r->Events.Drop(r,reason,r->User);
    if(r->Dropped>=r->Config.MaxBad||r->Consecutive>=r->Config.MaxConsecutiveBad){r->Failure=reason;return reason;}
    return XTP_DROPPED;
}
static xtpresult xtpReaderClock(xtpreader* r,uint64_t now)
{
    if(now<r->LastNow)return XTP_E_ARGUMENT;
    r->LastNow=now;
    if(r->Failure!=XTP_OK)return r->Failure;
    if(r->Ended)return XTP_END;
    if(r->Deadline && now>=r->Deadline){r->Failure=XTP_E_DEADLINE;return r->Failure;}
    return XTP_OK;
}
static void xtpReaderStart(xtpreader* r,uint64_t now)
{if(!r->Deadline)r->Deadline=UINT64_MAX-now<r->Config.ReceiveTimeoutUs?UINT64_MAX:now+r->Config.ReceiveTimeoutUs;}
static const unsigned char* xtpReaderData(xtpreader* r)
{return r->Buffer?r->Buffer:r->Header;}
static xtpresult xtpReaderDispatch(xtpreader* r,const xtpframeview* f)
{
    xtpflow flow=r->Events.Frame(r,f,r->User);
    if(flow==XTP_FLOW_PAUSE){r->Paused=true;r->Deadline=0;return XTP_PAUSED;}
    if(flow!=XTP_FLOW_ACCEPT&&flow!=XTP_FLOW_STOP){r->Failure=XTP_E_STATE;return r->Failure;}
    ++r->Frames;r->Consecutive=0;
    if(flow==XTP_FLOW_STOP){r->Ended=true;return XTP_CLOSED;}
    return XTP_OK;
}
static xtpresult xtpReaderRun(xtpreader* r,xbytesview in,bool end,uint64_t now,xtpreadprogress* p)
{
    xtpresult result=xtpReaderClock(r,now);if(result<0||result==XTP_END)return result;
    if(r->Paused){if(end&&!in.Size)r->PendingEnd=true;return XTP_PAUSED;}
    if(r->PendingEnd && in.Size)return XTP_E_STATE;
    bool boundary=end||r->PendingEnd;
    while(p->Frames<r->Config.MaxFrames && p->Dropped<r->Config.MaxFrames-p->Frames && p->WorkBytes<r->Config.MaxWorkBytes){
        size_t available=in.Size-p->Used,budget=r->Config.MaxWorkBytes-p->WorkBytes;
        const unsigned char* input=available?in.Data+p->Used:NULL;
        if(r->DropRecord){size_t take=available<budget?available:budget;p->Used+=take;p->WorkBytes+=take;break;}
        if(r->Discard){size_t take=r->Discard<available?r->Discard:available;if(take>budget)take=budget;
            p->Used+=take;p->WorkBytes+=take;r->Discard-=take;if(r->Discard)break;
            xtpReaderClear(r);continue;}
        if(!r->Used && available>=32){
            size_t frame=0;result=xtp__Envelope(input,&r->Config.Codec,&frame);
            if(result!=XTP_OK){if(r->Config.Mode==XTP_INPUT_STREAM){r->Failure=result;return result;}
                xtpReaderStart(r,now);
                r->DropRecord=true;result=xtpReaderBad(r,result,p);if(result<0)return result;continue;}
            size_t prefix=32+xtp__U16(input+18)+xtp__U32(input+20);
            if(available>=frame && budget>=prefix){xtpframeview f;result=xtp__DecodePrefix(input,frame,&r->Config.Codec,&f);p->WorkBytes+=prefix;
                if(result!=XTP_OK){xtpReaderStart(r,now);r->Discard=frame-prefix;p->Used+=prefix;r->DropReason=result;
                    result=xtpReaderBad(r,result,p);if(result<0)return result;
                    if(!r->Discard)xtpReaderClear(r);
                    continue;}
                if(available>=frame){result=xtpReaderDispatch(r,&f);
                    if(result==XTP_PAUSED)return result;
                    p->Used+=frame;++p->Frames;if(result!=XTP_OK)return result;continue;}
            }
        }
        if(!r->Used && !available)break;
        if(!r->Used)xtpReaderStart(r,now);
        size_t target=r->Used<32?32:(!r->PrefixChecked?r->Prefix:r->Need);
        if(r->Used<target){size_t take=target-r->Used;if(take>available)take=available;if(take>budget)take=budget;
            if(!take)break;
            if(r->Used<32)memcpy(r->Header+r->Used,input,take);
            else {result=xtpReaderReserve(r,r->Used+take);if(result!=XTP_OK){r->Failure=result;return result;}
                memcpy(r->Buffer+r->Used,input,take);}
            r->Used+=take;p->Used+=take;p->WorkBytes+=take;
            if(r->Need && r->Used==r->Need)r->Deadline=0;
            if(r->Used<target)continue;
        }
        if(r->Used==32&&!r->Need){result=xtp__Envelope(r->Header,&r->Config.Codec,&r->Need);
            if(result!=XTP_OK){if(r->Config.Mode==XTP_INPUT_STREAM){r->Failure=result;return result;}
                uint64_t deadline=r->Deadline;xtpReaderClear(r);r->Deadline=deadline;
                r->DropRecord=true;result=xtpReaderBad(r,result,p);if(result<0)return result;continue;}
            r->Prefix=32+xtp__U16(r->Header+18)+xtp__U32(r->Header+20);
            if(r->Used==r->Need)r->Deadline=0;
            if(r->Buffer)memcpy(r->Buffer,r->Header,32);
        }
        if(!r->PrefixChecked && r->Used==r->Prefix){
            if(r->Config.MaxWorkBytes-p->WorkBytes<r->Prefix)break;
            result=xtp__DecodePrefix(xtpReaderData(r),r->Need,&r->Config.Codec,&r->Validated);p->WorkBytes+=r->Prefix;
            if(result!=XTP_OK){r->Discard=r->Need-r->Used;r->DropReason=result;
                result=xtpReaderBad(r,result,p);if(result<0)return result;
                if(!r->Discard)xtpReaderClear(r);
                continue;}
            r->PrefixChecked=true;
        }
        if(r->PrefixChecked && r->Used==r->Need){
            const unsigned char* data=xtpReaderData(r);xtpframeview f=r->Validated;
            f.Packet=(xbytesview){data,r->Need};f.Body=(xbytesview){data+r->Prefix,r->Need-r->Prefix};
            if(f.Command.Kind==XTP_COMMAND_NAME)f.Command.As.Name.Data=(const char*)data+32;
            f.Params._Wire.Data=data+32+xtp__U16(r->Header+18);
            result=xtpReaderDispatch(r,&f);
            if(result==XTP_PAUSED)return result;
            ++p->Frames;xtpReaderClear(r);if(result!=XTP_OK)return result;
        }
    }
    if(boundary && p->Used==in.Size){
        if(r->Used && !r->Discard && r->Need && r->Used==r->Need){r->PendingEnd=true;return XTP_AGAIN;}
        r->PendingEnd=false;
        if(r->Used&&!r->Discard){result=xtpReaderBad(r,XTP_E_TRUNCATED,p);if(result<0)return result;}
        if(r->Used||r->Discard||r->DropRecord)xtpReaderClear(r);
        r->DropRecord=false;
    }
    if(p->Used<in.Size)return XTP_AGAIN;
    if(!r->Discard && r->Used && r->Need && (r->Used==r->Need||(!r->PrefixChecked&&r->Used==r->Prefix)))return XTP_AGAIN;
    if(r->Used||r->Discard)return XTP_MORE;
    return p->Dropped?XTP_DROPPED:XTP_OK;
}
/* This validates an ADDRESS, never reads output contents. Passing the address
 * as an integer also avoids GCC's const-void* uninitialized-read inference in
 * optimized amalgamated callers whose output storage has not been filled yet. */
static bool xtpReaderOutput(xtpreader* r,uintptr_t address,size_t n)
{
    const void* output=(const void*)address;
    return xrtMemRangeValid(output,n)&&!xtp__Overlap(r,sizeof(*r),output,n)&&
        !xtp__Overlap(r->Buffer,r->Capacity,output,n);
}
XTP_API xtpresult xtpReaderInput(xtpreader* r,xbytesview in,bool end,uint64_t now,xtpreadprogress* progress)
{
    if(!r||r->Busy||!xtpReaderOutput(r,(uintptr_t)progress,sizeof(*progress))||!xtp__Bytes(in)||
        xtp__Overlap(in.Data,in.Size,r,sizeof(*r))||xtp__Overlap(in.Data,in.Size,r->Buffer,r->Capacity)||
        xtp__Overlap(in.Data,in.Size,progress,sizeof(*progress))||
        (end&&r->Config.Mode!=XTP_INPUT_RECORD)||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpreadprogress p={0};r->Busy=true;xtpresult result=xtpReaderRun(r,in,end,now,&p);
    if(result==XTP_PAUSED&&end&&p.Used==in.Size)r->PendingEnd=true;
    r->Busy=false;*progress=p;return result;
}
XTP_API xtpresult xtpReaderResume(xtpreader* r)
{if(!r||r->Busy)return XTP_E_STATE;r->Paused=false;return r->Failure;}
XTP_API xtpresult xtpReaderEnd(xtpreader* r,uint64_t now)
{
    if(!r||r->Busy||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpresult result=xtpReaderClock(r,now);if(result!=XTP_OK)return result;
    if(r->Paused)return XTP_PAUSED;
    if(!r->Discard&&r->Need&&r->Used==r->Need)return XTP_AGAIN;
    r->LastNow=now;r->Ended=true;
    if(r->Used||r->Discard||r->DropRecord){r->Failure=XTP_E_TRUNCATED;return r->Failure;}
    return XTP_END;
}
XTP_API xtpresult xtpReaderReset(xtpreader* r)
{
    if(!r||r->Busy)return XTP_E_STATE;
    r->Busy=true;
    xtpReaderClear(r);r->Failure=XTP_OK;r->Paused=r->Ended=r->DropRecord=r->PendingEnd=false;
    r->Frames=r->Dropped=0;r->Consecutive=0;r->Busy=false;return XTP_OK;
}
XTP_API xtpresult xtpReaderDrive(xtpreader* r,uint64_t now,uint64_t* due)
{
    if(!r||r->Busy||!xtpReaderOutput(r,(uintptr_t)due,sizeof(*due))||now<r->LastNow)return XTP_E_ARGUMENT;
    xtpreadprogress p={0};r->Busy=true;
    xtpresult result=xtpReaderRun(r,(xbytesview){NULL,0},false,now,&p);
    r->Busy=false;*due=r->Deadline;return result==XTP_MORE?XTP_OK:result;
}
/* Session maintenance shares one work budget across independent lanes. */
xtpresult xtp__ReaderDriveBudget(xtpreader* r,uint64_t now,size_t frames,size_t* work,uint64_t* due)
{
    size_t oldframes=r->Config.MaxFrames,oldwork=r->Config.MaxWorkBytes;
    r->Config.MaxFrames=frames<oldframes?frames:oldframes;
    r->Config.MaxWorkBytes=*work<oldwork?*work:oldwork;
    xtpreadprogress p={0};r->Busy=true;
    xtpresult result=xtpReaderRun(r,(xbytesview){NULL,0},false,now,&p);
    r->Busy=false;*due=r->Deadline;*work-=p.WorkBytes;
    r->Config.MaxFrames=oldframes;r->Config.MaxWorkBytes=oldwork;
    return result==XTP_MORE?XTP_OK:result;
}
XTP_API xtpresult xtpReaderInfo(const xtpreader* r,xtpreaderinfo* info)
{
    if(!r||!xtpReaderOutput((xtpreader*)r,(uintptr_t)info,sizeof(*info)))return XTP_E_ARGUMENT;
    *info=(xtpreaderinfo){r->Used,r->Capacity,sizeof(*r)+r->Capacity,r->Peak,r->Frames,r->Dropped,r->Deadline,
        r->Consecutive,r->Failure,r->Paused,r->Ended,r->DropRecord};return XTP_OK;
}
#endif
