#include <xtp/session.h>
#include "../internal/xtp_codec.h"
#include <xrt/random.h>
#if defined(XTP_FEATURE_XTP_SESSION)
#define XTP_NONE SIZE_MAX
typedef union xtpmemory {size_t Size;long double Align;void* Pointer;} xtpmemory;
typedef struct xtppending {
    uint64_t ID,Serial,Deadline;void* Tag;size_t Next,Tx,Lane;
    xtpmessagekind Type;bool Active,Sent;
} xtppending;
typedef struct xtptx {
    unsigned char* Bytes;unsigned char Control[32];
    size_t Capacity,Size,Offset,Charge,Next,Previous,Lane,Pending;
    uint64_t ID,Serial,Deadline;void* Tag;
    xtpmessagekind Type;bool Active,Queued,Incoming,Waiting,Notify,Sent;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    bool Replay,RetireAfterSend,RoundStarted;
    size_t RetainNext,RetainPrevious,Attempts;
    uint64_t RetryAt,KeepUntil,Ticket;
#endif
} xtptx;
#if defined(XTP_FEATURE_XTP_DELIVERY)
typedef struct xtpreceipt {uint64_t ID,Until;size_t Lane;unsigned State;} xtpreceipt;
#define XTP_REPLAY(t) ((t)->Replay)
#define XTP_TICKET(t) ((t)->Ticket)
#else
#define XTP_REPLAY(t) false
#define XTP_TICKET(t) ((t)->Serial)
#endif
typedef struct xtphistory {uint64_t ID;int32_t Status;xtpmessagekind Type;} xtphistory;
typedef struct xtpsessionlane {
    xtplane Config;xtpreader* Reader;xtpsession* Session;size_t Head,Tail,Index;
} xtpsessionlane;
struct xtpsession {
    xtpconfig Config;xtpevents Events;void* User;
    xtppending* Pending;xtptx* Tx;xtphistory* History;xtpsessionlane* Lanes;size_t* Buckets;
    size_t BucketCount,FreePending,FreeTx,PendingCount,TxCount,TxBytes,NormalBytes,NormalFrames,Incoming,Storage,Peak,LaneCount,TurnFrames,NextLane;
    uint64_t Cookie[2],NextID,NextSerial,Binding,LastNow,HighID;
    uint64_t Submitted,Completed,Messages,Unknown,Duplicates;
    bool Bound,Busy,Callback,Closing,Closed,Unbinding,CloseRequested;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    xtpreceipt* Receipts;xtpdeliveryidentity Identity;
    size_t RetainHead,RetainTail,Retained,ReceiptCount;
    uint64_t AuthUntil,Lifetime,ResultFloor,Attempts,Retries,ResultAcks;
    bool DeliveryStarted,Suspended,ReplayRequests;
#endif
};
static void* xtpSessionDefault(void* user,void* memory,size_t n)
{(void)user;if(!n){xrtFree(memory);return NULL;}return xrtRealloc(memory,n);}
/* Every owned allocation, including Reader buffers and temporary growth, shares
 * this ledger. Realloc reserves old+new, never just their difference. */
static void* xtpSessionMemory(void* user,void* memory,size_t n)
{
    xtpsession* s=user;xtpmemory* old=memory?(xtpmemory*)memory-1:NULL;size_t oldn=old?old->Size:0;
    if(!n){if(old){s->Storage-=oldn;s->Config.Realloc(s->Config.AllocatorUser,old,0);}return NULL;}
    if(n>SIZE_MAX-sizeof(xtpmemory))return NULL;
    size_t total=n+sizeof(xtpmemory);
    if(total>s->Config.MaxSessionBytes-s->Storage)return NULL;
    size_t peak=s->Storage+total;if(peak>s->Peak)s->Peak=peak;
    xtpmemory* p=s->Config.Realloc(s->Config.AllocatorUser,old,total);if(!p)return NULL;
    p->Size=total;s->Storage=s->Storage-oldn+total;return p+1;
}
bool xtp__SessionAddress(const xtpsession* s,uintptr_t address,size_t n)
{
    const void* p=(const void*)address;
    if(!xrtMemRangeValid(p,n)||xtp__Overlap(s,sizeof(*s),p,n)||
        xtp__Overlap(s->Tx,s->Config.MaxTxFrames*sizeof(*s->Tx),p,n)||
        xtp__Overlap(s->Pending,s->Config.MaxPending*sizeof(*s->Pending),p,n)||
        xtp__Overlap(s->Lanes,s->Config.MaxLanes*sizeof(*s->Lanes),p,n)||
        xtp__Overlap(s->History,s->Config.DedupWindow*sizeof(*s->History),p,n)||
        xtp__Overlap(s->Buckets,s->BucketCount*sizeof(*s->Buckets),p,n))return false;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts&&xtp__Overlap(s->Receipts,s->Config.DedupWindow*sizeof(*s->Receipts),p,n))return false;
#endif
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&
        xtp__Overlap(s->Tx[i].Bytes,s->Tx[i].Capacity,p,n))return false;
    return true;
}
xtpresult xtp__TransportWrite(xtpsession* s,const xtpoutput* out,
    xtpresult (*write)(void*,const xtpoutput*,size_t*),void* user,size_t* accepted)
{
    if(s->Busy)return XTP_E_STATE;
    s->Busy=true;xtpresult result=write(user,out,accepted);s->Busy=false;return result;
}
uint64_t xtp__SessionTime(const xtpsession* s){return s->LastNow;}
xtpresult xtp__SessionObserve(xtpsession* s,uint64_t now)
{if(s->Busy)return XTP_E_STATE;if(now<s->LastNow)return XTP_E_ARGUMENT;s->LastNow=now;return XTP_OK;}
static uint64_t xtpSessionDue(uint64_t now,uint64_t delay)
{return UINT64_MAX-now<delay?UINT64_MAX:now+delay;}
static void xtpSessionFault(xtpsession* s,xtpresult reason,size_t lane,bool fatal)
{
    if(fatal)s->CloseRequested=true;
    if(s->Events.OnFault){xtpfault f={reason,s->Binding,lane<s->LaneCount?s->Lanes[lane].Config.ID:0,fatal};
        s->Callback=true;s->Events.OnFault(s,&f,s->User);s->Callback=false;}
}
XTP_API void xtpConfigInit(xtpconfig* c)
{
    if(!c)return;
    memset(c,0,sizeof(*c));xtpReaderConfigInit(&c->Reader);
    c->MaxPending=64;c->MaxIncomingRequests=64;c->MaxLanes=4;c->MaxTxFrames=256;c->MaxTxBytes=1048576;
    c->ResponseReserveBytes=262144;c->ResponseReserveFrames=16;c->MaxResponseBytes=65536;
    c->MaxSessionBytes=16777216;c->DedupWindow=1024;
    c->DefaultOperationTimeoutUs=c->IncomingRequestTimeoutUs=5000000;
    xtpDeliveryConfigInit(&c->Delivery);c->Delivery.Enabled=false;
}
static void xtpSessionFreeTables(xtpsession* s)
{
    xtpSessionMemory(s,s->Lanes,0);xtpSessionMemory(s,s->Pending,0);xtpSessionMemory(s,s->Tx,0);
    xtpSessionMemory(s,s->History,0);xtpSessionMemory(s,s->Buckets,0);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    xtpSessionMemory(s,s->Receipts,0);
#endif
}
XTP_API xtpresult xtpCreate(const xtpconfig* input,const xtpevents* events,void* user,xtpsession** out)
{
    xtpconfig c;xtpcodecconfig codec;
    if(!out||!events||!events->OnMessage||!xrtMemRangeValid(out,sizeof(*out))||
        !xrtMemRangeValid(events,sizeof(*events))||xtp__Overlap(out,sizeof(*out),events,sizeof(*events)))return XTP_E_ARGUMENT;
    if(input){if(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(out,sizeof(*out),input,sizeof(*input)))return XTP_E_ARGUMENT;c=*input;}
    else xtpConfigInit(&c);
    if(xtp__CodecConfig(&c.Reader.Codec,&codec)!=XTP_OK||!c.MaxPending||c.MaxPending>4096||
        !c.MaxIncomingRequests||c.MaxIncomingRequests>4096||!c.MaxLanes||c.MaxLanes>64||
        c.MaxTxFrames<2||c.MaxTxFrames>16384||c.MaxResponseBytes<32||c.MaxResponseBytes>codec.MaxFrameBytes||
        c.MaxTxBytes<c.MaxResponseBytes||c.ResponseReserveBytes<32||c.ResponseReserveBytes>=c.MaxTxBytes||
        !c.ResponseReserveFrames||c.ResponseReserveFrames>=c.MaxTxFrames||c.MaxSessionBytes<sizeof(xtpsession)||
        !c.DedupWindow||c.DedupWindow>65536||!c.DefaultOperationTimeoutUs||!c.IncomingRequestTimeoutUs||
        !c.Reader.MaxFrames||c.Reader.MaxFrames>65536||!c.Reader.MaxBad||!c.Reader.MaxConsecutiveBad||
        xtp__ReaderConfig(&c.Reader)!=XTP_OK)return XTP_E_ARGUMENT;
    if(c.Delivery.Enabled){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(!c.Delivery.MaxReplayMessages||c.Delivery.MaxReplayMessages>c.MaxTxFrames||
            !c.Delivery.MaxAttempts||c.Delivery.MaxAttempts>64||!c.Delivery.MaxLifetimeUs||
            c.Delivery.MaxLifetimeUs>3600000000ULL||!c.Delivery.RetryMinUs||
            c.Delivery.RetryMinUs>c.Delivery.RetryMaxUs||c.Delivery.RetryMaxUs>c.Delivery.MaxLifetimeUs)return XTP_E_ARGUMENT;
#else
        return XTP_E_STATE;
#endif
    }
    if(!c.Realloc)c.Realloc=xtpSessionDefault;
    xtpsession* s=c.Realloc(c.AllocatorUser,NULL,sizeof(*s));if(!s)return XTP_E_MEMORY;
    memset(s,0,sizeof(*s));s->Config=c;s->Events=*events;s->User=user;s->Storage=s->Peak=sizeof(*s);
    s->BucketCount=1;while(s->BucketCount<c.MaxPending*2)s->BucketCount*=2;
    s->Lanes=xtpSessionMemory(s,NULL,c.MaxLanes*sizeof(*s->Lanes));
    s->Pending=xtpSessionMemory(s,NULL,c.MaxPending*sizeof(*s->Pending));
    s->Tx=xtpSessionMemory(s,NULL,c.MaxTxFrames*sizeof(*s->Tx));
    s->History=xtpSessionMemory(s,NULL,c.DedupWindow*sizeof(*s->History));
    s->Buckets=xtpSessionMemory(s,NULL,s->BucketCount*sizeof(*s->Buckets));
#if defined(XTP_FEATURE_XTP_DELIVERY)
    s->RetainHead=s->RetainTail=XTP_NONE;s->ResultFloor=1;
    if(c.Delivery.Enabled){s->Receipts=xtpSessionMemory(s,NULL,c.DedupWindow*sizeof(*s->Receipts));
        if(!s->Receipts)goto memory;
        memset(s->Receipts,0,c.DedupWindow*sizeof(*s->Receipts));}
#endif
    if(!s->Lanes||!s->Pending||!s->Tx||!s->History||!s->Buckets)goto memory;
    memset(s->Lanes,0,c.MaxLanes*sizeof(*s->Lanes));memset(s->Pending,0,c.MaxPending*sizeof(*s->Pending));
    memset(s->Tx,0,c.MaxTxFrames*sizeof(*s->Tx));memset(s->History,0,c.DedupWindow*sizeof(*s->History));
    for(size_t i=0;i<c.MaxPending;++i)s->Pending[i].Next=i+1<c.MaxPending?i+1:XTP_NONE;
    for(size_t i=0;i<c.MaxTxFrames;++i)s->Tx[i].Next=i+1<c.MaxTxFrames?i+1:XTP_NONE;
    for(size_t i=0;i<s->BucketCount;++i)s->Buckets[i]=XTP_NONE;
    if(!xrtSecureRandom(s->Cookie,sizeof(s->Cookie))||!s->Cookie[0]||!s->Cookie[1]){
        xtpSessionFreeTables(s);c.Realloc(c.AllocatorUser,s,0);return XTP_E_STATE;}
    s->NextID=s->NextSerial=1;*out=s;return XTP_OK;
memory:
    xtpSessionFreeTables(s);c.Realloc(c.AllocatorUser,s,0);return XTP_E_MEMORY;
}
static size_t xtpSessionLane(xtpsession* s,uint32_t id)
{
    if(!id)return s->LaneCount?0:XTP_NONE;
    for(size_t i=0;i<s->LaneCount;++i)if(s->Lanes[i].Config.ID==id)return i;
    return XTP_NONE;
}
bool xtp__RecordLane(xtpsession* s,uint64_t binding,uint32_t lane)
{
    if(s->Busy||!s->Bound||binding!=s->Binding)return false;
    size_t i=xtpSessionLane(s,lane);return i!=XTP_NONE&&s->Lanes[i].Config.Mode==XTP_INPUT_RECORD&&s->Lanes[i].Config.Reliable;
}
static size_t xtpPendingFind(xtpsession* s,uint64_t id)
{
    size_t i=s->Buckets[(size_t)id&(s->BucketCount-1)];
    while(i!=XTP_NONE){if(s->Pending[i].ID==id)return i;i=s->Pending[i].Next;}return XTP_NONE;
}
static void xtpTxUnqueue(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];if(!t->Queued)return;
    xtpsessionlane* l=&s->Lanes[t->Lane];
    if(t->Previous!=XTP_NONE)s->Tx[t->Previous].Next=t->Next;else l->Head=t->Next;
    if(t->Next!=XTP_NONE)s->Tx[t->Next].Previous=t->Previous;else l->Tail=t->Previous;
    t->Queued=false;
}
static bool xtpTxQueue(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];xtpsessionlane* l=&s->Lanes[t->Lane];
    t->Queued=true;t->Previous=l->Tail;t->Next=XTP_NONE;
    if(l->Tail!=XTP_NONE)s->Tx[l->Tail].Next=i;else l->Head=i;l->Tail=i;return true;
}
static size_t xtpTxTake(xtpsession* s,size_t lane,size_t charge,bool normal)
{
    if(s->FreeTx==XTP_NONE||!s->NextSerial||charge>s->Config.MaxTxBytes-s->TxBytes||
        (normal&&(s->NormalFrames>=s->Config.MaxTxFrames-s->Config.ResponseReserveFrames||
        charge>s->Config.MaxTxBytes-s->Config.ResponseReserveBytes-s->NormalBytes)))return XTP_NONE;
    size_t i=s->FreeTx;xtptx* t=&s->Tx[i];s->FreeTx=t->Next;memset(t,0,sizeof(*t));
    t->Active=true;t->Lane=lane;t->Charge=charge;t->Pending=XTP_NONE;t->Serial=s->NextSerial++;
    t->Next=t->Previous=XTP_NONE;t->Notify=normal; /* normal Type finalized before publishing. */
#if defined(XTP_FEATURE_XTP_DELIVERY)
    t->Ticket=t->Serial;t->RetainNext=t->RetainPrevious=XTP_NONE;
#endif
    ++s->TxCount;s->TxBytes+=charge;if(normal){++s->NormalFrames;s->NormalBytes+=charge;}return i;
}
static void xtpTxFree(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];xtpTxUnqueue(s,i);xtpSessionMemory(s,t->Bytes,0);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay){
        if(t->RetainPrevious!=XTP_NONE)s->Tx[t->RetainPrevious].RetainNext=t->RetainNext;else s->RetainHead=t->RetainNext;
        if(t->RetainNext!=XTP_NONE)s->Tx[t->RetainNext].RetainPrevious=t->RetainPrevious;else s->RetainTail=t->RetainPrevious;
        --s->Retained;}
#endif
    s->TxBytes-=t->Charge;--s->TxCount;
    if(t->Type==XTP_REQUEST||t->Type==XTP_PUSH||t->Notify){s->NormalBytes-=t->Charge;--s->NormalFrames;}
    if(t->Waiting)--s->Incoming;
    t->Active=false;t->Waiting=false;t->Bytes=NULL;t->Next=s->FreeTx;s->FreeTx=i;
}
static void xtpSessionComplete(xtpsession* s,size_t pi,xtpcompletion* c)
{
    xtppending p=s->Pending[pi];size_t bucket=(size_t)p.ID&(s->BucketCount-1),*link=&s->Buckets[bucket];
    while(*link!=pi)link=&s->Pending[*link].Next;
    *link=p.Next;s->Pending[pi].Active=false;s->Pending[pi].Next=s->FreePending;s->FreePending=pi;--s->PendingCount;
    if(p.Tx!=XTP_NONE&&s->Tx[p.Tx].Active){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        xtptx* t=&s->Tx[p.Tx];
        if(t->Replay&&t->Queued&&t->Offset&&t->Offset<t->Size&&!s->Unbinding&&!s->Closing&&!s->CloseRequested){
            t->Pending=XTP_NONE;t->RetireAfterSend=true;
        }else
#endif
        xtpTxFree(s,p.Tx);}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts){xtpreceipt* r=&s->Receipts[p.ID%s->Config.DedupWindow];
        if(r->ID==p.ID&&r->State==1){r->State=0;--s->ReceiptCount;}}
#endif
    c->Operation=(xtpoperationtoken){{s->Cookie[0],p.Serial}};c->Type=p.Type;c->MessageID=p.ID;c->Tag=p.Tag;
    ++s->Completed;if(s->Events.OnComplete){s->Callback=true;s->Events.OnComplete(s,c,s->User);s->Callback=false;}
}
static void xtpNotifyComplete(xtpsession* s,size_t i,xtpfinish reason,xtpresult error)
{
    xtptx t=s->Tx[i];xtpcompletion c={0};c.Operation=(xtpoperationtoken){{s->Cookie[0],t.Serial}};
    c.Type=XTP_NOTIFY;c.Tag=t.Tag;c.Reason=reason;c.Knowledge=t.Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;
    c.LocalResult=error;xtpTxFree(s,i);++s->Completed;
    if(s->Events.OnComplete){s->Callback=true;s->Events.OnComplete(s,&c,s->User);s->Callback=false;}
}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static void xtpRetain(xtpsession* s,size_t i,uint64_t until)
{
    xtptx* t=&s->Tx[i];t->Replay=true;t->KeepUntil=until;t->RetainPrevious=s->RetainTail;t->RetainNext=XTP_NONE;
    if(s->RetainTail!=XTP_NONE)s->Tx[s->RetainTail].RetainNext=i;else s->RetainHead=i;
    s->RetainTail=i;++s->Retained;
}
static bool xtpRoundReset(xtpsession* s,xtptx* t)
{
    if(!s->NextSerial){s->CloseRequested=true;return false;}
    t->Offset=0;t->RoundStarted=false;t->RetryAt=0;t->Ticket=s->NextSerial++;return true;
}
static bool xtpKeep(const xtpsession* s,const xtptx* t)
{return s->Unbinding&&!s->Closing&&!s->CloseRequested&&t->Replay&&!t->RetireAfterSend;}
static bool xtpRetriable(const xtpsession* s,const xtptx* t)
{return !t->Replay||(t->Queued&&t->RoundStarted)||t->Attempts<s->Config.Delivery.MaxAttempts;}
static void xtpReceiptExpire(xtpsession* s,xtpreceipt* r)
{
    if(r->ID>=s->ResultFloor)s->ResultFloor=r->ID==UINT64_MAX?UINT64_MAX:r->ID+1;
    r->State=0;--s->ReceiptCount;
}
#else
#define xtpKeep(s,t) false
#define xtpRetriable(s,t) true
#endif
static xtpresult xtpSessionDrain(xtpsession* s)
{
    size_t budget=s->TurnFrames;
    for(size_t i=0;i<s->Config.MaxPending&&budget;++i)if(s->Pending[i].Active){
        if(s->Pending[i].Tx!=XTP_NONE&&xtpKeep(s,&s->Tx[s->Pending[i].Tx]))continue;
        xtpcompletion c={0};c.Reason=XTP_FINISH_CLOSED;c.Knowledge=s->Pending[i].Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;
        xtpSessionComplete(s,i,&c);--budget;}
    for(size_t i=0;i<s->Config.MaxTxFrames&&budget;++i)if(s->Tx[i].Active){
        if(xtpKeep(s,&s->Tx[i])){xtpTxUnqueue(s,i);continue;}
        if(s->Tx[i].Notify)xtpNotifyComplete(s,i,XTP_FINISH_CLOSED,XTP_OK);else xtpTxFree(s,i);--budget;}
    bool remaining=false;
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active&&
        (s->Pending[i].Tx==XTP_NONE||!xtpKeep(s,&s->Tx[s->Pending[i].Tx])))remaining=true;
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&!xtpKeep(s,&s->Tx[i]))remaining=true;
    if(remaining)return XTP_AGAIN;
    for(size_t i=0;i<s->LaneCount;++i){xtpReaderDestroy(s->Lanes[i].Reader);s->Lanes[i].Reader=NULL;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    s->Suspended=s->Unbinding&&s->DeliveryStarted&&!s->Closing&&!s->CloseRequested;
    if(!s->Suspended)s->LaneCount=0;
    if(s->Closing||s->CloseRequested){if(s->Receipts)memset(s->Receipts,0,s->Config.DedupWindow*sizeof(*s->Receipts));s->ReceiptCount=0;}
#else
    s->LaneCount=0;
#endif
    s->Bound=false;s->Unbinding=false;
    if(s->CloseRequested)s->Closing=true;
    if(s->Closing)s->Closed=true;
    return s->Closed?XTP_CLOSED:XTP_OK;
}
static xtpresult xtpSessionLeave(xtpsession* s,xtpresult result)
{
    if(s->CloseRequested){s->Closing=true;s->Bound=false;}
    if(s->Closing||s->Unbinding)result=xtpSessionDrain(s);
    s->Busy=false;return result;
}
static void xtpHistoryPut(xtpsession* s,uint64_t id,xtpmessagekind type,int32_t status)
{
    if(id>s->HighID)s->HighID=id;
    s->History[id%s->Config.DedupWindow]=(xtphistory){id,status,type};
}
static bool xtpDecisionValid(xtpdecision d)
{
    if(d.Action==XTP_ACTION_ACCEPT||d.Action==XTP_ACTION_PAUSE)return d.Status==0;
    if(d.Action==XTP_ACTION_BUSY)return d.Status==XTP_STATUS_BUSY;
    return d.Action==XTP_ACTION_REJECT&&xtp__Status(d.Status)&&d.Status!=0&&
        d.Status!=XTP_STATUS_BUSY&&d.Status!=XTP_STATUS_STATE_LOST&&d.Status!=XTP_STATUS_DEADLINE_EXCEEDED;
}
static bool xtpControl(xtpsession* s,size_t i,xtpmessagekind type,uint64_t id,int32_t status)
{
    xtptx* t=&s->Tx[i];xtppacket p={0};p.Type=type;p.MessageID=id;p.Status=status;size_t n;
    if(t->Waiting){t->Waiting=false;--s->Incoming;}
    xtpSessionMemory(s,t->Bytes,0);t->Bytes=NULL;t->Capacity=0;
    s->TxBytes-=t->Charge-32;t->Charge=t->Size=32;t->ID=id;t->Type=type;
    t->Deadline=xtpSessionDue(s->LastNow,s->Config.DefaultOperationTimeoutUs);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay)t->Deadline=t->KeepUntil;
#endif
    if(xtpEncode(&p,1,&s->Config.Reader.Codec,t->Control,32,&n)!=XTP_OK)return false;
    if(s->Bound)xtpTxQueue(s,i);
    return true;
}
static xtpflow xtpSendControl(xtpsession* s,size_t lane,const xtpframeview* f,int32_t status)
{
    size_t i=xtpTxTake(s,lane,32,false);if(i==XTP_NONE)return XTP_FLOW_PAUSE;
    if(!xtpControl(s,i,f->Type==XTP_PUSH?XTP_ACK:XTP_RESPONSE,f->MessageID,status)){
        xtpTxFree(s,i);xtpSessionFault(s,XTP_E_STATE,lane,true);return XTP_FLOW_STOP;}
    return XTP_FLOW_ACCEPT;
}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static bool xtpResultAck(xtpsession* s,size_t lane,uint64_t id)
{
    /* Coalesce queued receipts without owning another queue. */
    for(size_t j=0;j<s->Config.MaxTxFrames;++j){xtptx* q=&s->Tx[j];
        if(q->Active&&q->Type==XTP_ACK&&q->ID==id&&q->Lane==lane&&
            !q->Bytes&&(q->Control[17]&XTP_FLAG_RESULT_ACK))return true;}
    size_t i=xtpTxTake(s,lane,32,false);if(i==XTP_NONE)return false;
    if(!xtpControl(s,i,XTP_ACK,id,0)){xtpTxFree(s,i);return false;}
    s->Tx[i].Control[17]|=XTP_FLAG_RESULT_ACK;++s->ResultAcks;return true;
}
static void xtpRetireResult(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];
    if(t->Queued&&t->Offset&&t->Offset<t->Size)t->RetireAfterSend=true;else xtpTxFree(s,i);
}
static void xtpRetryBusy(xtpsession* s,size_t i)
{
    xtptx* t=&s->Tx[i];
    /* A duplicate BUSY cannot restart a partial round or extend its deadline. */
    if(t->Queued||t->RetryAt)return;
    uint64_t delay=s->Config.Delivery.RetryMinUs;
    for(size_t k=1;k<t->Attempts&&delay<s->Config.Delivery.RetryMaxUs;++k){
        delay=delay>s->Config.Delivery.RetryMaxUs/2?s->Config.Delivery.RetryMaxUs:delay*2;}
    uint64_t jitter=(t->ID^s->Cookie[0]^t->Attempts)%(delay/4+1);
    if(xtpRoundReset(s,t)){t->RetryAt=xtpSessionDue(s->LastNow,delay-delay/4+jitter);++s->Retries;}
}
#endif
static xtpflow xtpSessionFrame(xtpreader* reader,const xtpframeview* f,void* user)
{
    xtpsessionlane* lane=user;xtpsession* s=lane->Session;size_t li=lane->Index;(void)reader;
    if(s->TurnFrames)--s->TurnFrames;
    if(s->CloseRequested)return XTP_FLOW_STOP;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&s->LastNow>=s->AuthUntil){xtpSessionFault(s,XTP_E_DEADLINE,li,true);return XTP_FLOW_STOP;}
#endif
    if(!lane->Config.Reliable&&f->Type!=XTP_NOTIFY){xtpSessionFault(s,XTP_E_TYPE,li,false);return XTP_FLOW_ACCEPT;}
    if(f->Type==XTP_RESPONSE||f->Type==XTP_ACK){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->ResultAck){
            for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){xtptx* t=&s->Tx[i];
                if(t->Incoming&&t->ID==f->MessageID&&t->Lane==li&&!t->Waiting&&t->Sent){xtpRetireResult(s,i);return XTP_FLOW_ACCEPT;}}
            ++s->Unknown;return XTP_FLOW_ACCEPT;}
#endif
        size_t pi=xtpPendingFind(s,f->MessageID);
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(pi==XTP_NONE&&f->Type==XTP_RESPONSE&&s->Receipts){xtpreceipt* r=&s->Receipts[f->MessageID%s->Config.DedupWindow];
            if(r->State==2&&r->ID==f->MessageID&&r->Lane==li&&r->Until>s->LastNow){
                if(!xtpResultAck(s,li,f->MessageID))return XTP_FLOW_PAUSE;
                ++s->Duplicates;return XTP_FLOW_ACCEPT;}}
#endif
        if(f->ResultAck||pi==XTP_NONE||s->Pending[pi].Lane!=li||
            (s->Pending[pi].Type==XTP_REQUEST)!=(f->Type==XTP_RESPONSE)){
            ++s->Unknown;return XTP_FLOW_ACCEPT;}
        xtppending* p=&s->Pending[pi];
        /* Host reports transport acceptance before delivering its resulting
         * network events. An unsolicited result cannot complete an unsent ID. */
        if(p->Tx!=XTP_NONE&&!(XTP_REPLAY(&s->Tx[p->Tx])&&p->Sent)){++s->Unknown;
            if(s->Tx[p->Tx].Offset){xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}
            return XTP_FLOW_ACCEPT;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(p->Tx!=XTP_NONE&&s->Tx[p->Tx].Replay&&s->LastNow<p->Deadline){
            if(f->Status==XTP_STATUS_BUSY){xtpRetryBusy(s,p->Tx);return XTP_FLOW_ACCEPT;}
            if(f->Type==XTP_RESPONSE){
                if(!xtpResultAck(s,li,f->MessageID))return XTP_FLOW_PAUSE;
                xtpreceipt* r=&s->Receipts[p->ID%s->Config.DedupWindow];r->State=2;
                r->Until=xtpSessionDue(s->LastNow,s->Lifetime);if(r->Until>s->AuthUntil)r->Until=s->AuthUntil;}}
#endif
        xtpcompletion c={0};c.Reason=f->Type==XTP_RESPONSE?XTP_FINISH_RESPONSE:XTP_FINISH_ACK;
        c.HasRemoteStatus=true;c.Status=f->Status;
        c.Knowledge=f->Status==XTP_STATUS_STATE_LOST?XTP_KNOW_UNKNOWN:
            f->Type==XTP_RESPONSE?XTP_KNOW_REPLIED:f->Status?XTP_KNOW_REJECTED:XTP_KNOW_ACCEPTED;
        if(f->Type==XTP_RESPONSE){c.Params=f->Params;c.Body=f->Body;c.Packet=f->Packet;}
        if(s->LastNow>=p->Deadline){c=(xtpcompletion){0};c.Reason=XTP_FINISH_TIMEOUT;
            c.LocalResult=XTP_E_DEADLINE;c.Knowledge=XTP_KNOW_UNKNOWN;}
        xtpSessionComplete(s,pi,&c);return s->CloseRequested?XTP_FLOW_STOP:XTP_FLOW_ACCEPT;
    }
    if(f->Type!=XTP_NOTIFY){
        xtphistory* h=&s->History[f->MessageID%s->Config.DedupWindow];
        for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active&&s->Tx[i].Incoming&&s->Tx[i].ID==f->MessageID){
            ++s->Duplicates;
#if defined(XTP_FEATURE_XTP_DELIVERY)
            xtptx* t=&s->Tx[i];
            if(t->Replay&&!t->Waiting&&!t->Queued&&t->KeepUntil>s->LastNow&&t->Lane==li&&xtpRetriable(s,t)){
                if(xtpRoundReset(s,t))xtpTxQueue(s,i);}
#endif
            return XTP_FLOW_ACCEPT;}
        if(h->ID==f->MessageID){++s->Duplicates;
            return xtpSendControl(s,li,f,h->Type==XTP_PUSH&&f->Type==XTP_PUSH?h->Status:XTP_STATUS_STATE_LOST);}
        if(s->HighID>=s->Config.DedupWindow&&f->MessageID<=s->HighID-s->Config.DedupWindow){
            ++s->Duplicates;return xtpSendControl(s,li,f,XTP_STATUS_STATE_LOST);}
        if(f->Replayable){
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(!s->Config.Delivery.Enabled||!s->DeliveryStarted||s->LastNow>=s->AuthUntil||
                (f->Type==XTP_REQUEST&&!s->ReplayRequests))
#endif
            return xtpSendControl(s,li,f,XTP_STATUS_UNSUPPORTED);}
    }
    size_t ti=XTP_NONE;
    if(f->Type==XTP_PUSH){ti=xtpTxTake(s,li,32,false);if(ti==XTP_NONE)return XTP_FLOW_PAUSE;}
    if(f->Type==XTP_REQUEST){
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->Replayable&&s->Retained>=s->Config.Delivery.MaxReplayMessages)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
#endif
        if(s->Incoming>=s->Config.MaxIncomingRequests)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
        ti=xtpTxTake(s,li,s->Config.MaxResponseBytes,false);if(ti==XTP_NONE)return xtpSendControl(s,li,f,XTP_STATUS_BUSY);
        xtptx* t=&s->Tx[ti];t->Bytes=xtpSessionMemory(s,NULL,s->Config.MaxResponseBytes);
        if(!t->Bytes){xtpTxFree(s,ti);return xtpSendControl(s,li,f,XTP_STATUS_BUSY);}
        t->Capacity=s->Config.MaxResponseBytes;t->Type=XTP_RESPONSE;t->ID=f->MessageID;t->Incoming=t->Waiting=true;
        t->Deadline=xtpSessionDue(s->LastNow,s->Config.IncomingRequestTimeoutUs);++s->Incoming;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(f->Replayable){uint64_t until=xtpSessionDue(s->LastNow,s->Lifetime);if(until>s->AuthUntil)until=s->AuthUntil;
            xtpRetain(s,ti,until);if(t->Deadline>until)t->Deadline=until;}
#endif
    }
    xtpmessageview m={0};m.Type=f->Type;m.MessageID=f->MessageID;m.Command=f->Command;m.Params=f->Params;m.Body=f->Body;m.Packet=f->Packet;
    if(f->Type==XTP_REQUEST)m.Request=(xtprequesttoken){{s->Cookie[0],s->Cookie[1],ti+1,s->Tx[ti].Serial}};
    s->Callback=true;xtpdecision d=s->Events.OnMessage(s,&m,s->User);s->Callback=false;++s->Messages;
    bool replied=ti!=XTP_NONE&&f->Type==XTP_REQUEST&&!s->Tx[ti].Waiting;
    if(!xtpDecisionValid(d)||(replied&&d.Action!=XTP_ACTION_ACCEPT)){
        xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}
    if(s->CloseRequested)return XTP_FLOW_STOP;
    if(d.Action==XTP_ACTION_PAUSE){if(ti!=XTP_NONE)xtpTxFree(s,ti);return XTP_FLOW_PAUSE;}
    if(f->Type==XTP_NOTIFY)return XTP_FLOW_ACCEPT;
    if(d.Action!=XTP_ACTION_BUSY)xtpHistoryPut(s,f->MessageID,f->Type,d.Status);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(d.Action==XTP_ACTION_BUSY&&ti!=XTP_NONE&&s->Tx[ti].Replay){
        /* BUSY has no accepted result to retain. Reuse the released slot. */
        xtpTxFree(s,ti);ti=xtpTxTake(s,li,32,false);
        if(ti==XTP_NONE){xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}}
#endif
    if(f->Type==XTP_PUSH||d.Action!=XTP_ACTION_ACCEPT){
        if(!xtpControl(s,ti,f->Type==XTP_PUSH?XTP_ACK:XTP_RESPONSE,f->MessageID,d.Status)){
            xtpSessionFault(s,XTP_E_STATE,li,true);return XTP_FLOW_STOP;}}
    return XTP_FLOW_ACCEPT;
}
static void xtpSessionDrop(xtpreader* reader,xtpresult reason,void* user)
{xtpsessionlane* lane=user;(void)reader;if(lane->Session->TurnFrames)--lane->Session->TurnFrames;
    xtpSessionFault(lane->Session,reason,lane->Index,false);}
#if defined(XTP_FEATURE_XTP_DELIVERY)
static bool xtpIdentityPart(const unsigned char* bytes,size_t n)
{unsigned char any=0;for(size_t i=0;i<n;++i)any|=bytes[i];return any!=0;}
static bool xtpProof(xtpsession* s,const xtpbinding* b,uint64_t now)
{
    const xtpdeliveryproof* p=b->Delivery;
    if(!s->Config.Delivery.Enabled)return p==NULL;
    if(!b->Authenticated||!p||!xrtMemRangeValid(p,sizeof(*p))||
        p->UntilUs<=now||p->UntilUs==UINT64_MAX||!p->LifetimeUs||p->LifetimeUs>s->Config.Delivery.MaxLifetimeUs||
        !p->PeerReceiveFloor||!p->PeerResultFloor||!xtpIdentityPart(p->Identity.SessionID,16)||
        !xtpIdentityPart(p->Identity.Domain,16)||!xtpIdentityPart(p->Identity.Local,32)||!xtpIdentityPart(p->Identity.Peer,32))return false;
    if(!s->DeliveryStarted)return !p->Resume&&p->StateRetained;
    if(!s->Suspended||!p->Resume||!p->StateRetained||!p->OldBindingDetached||s->AuthUntil<=now||
        memcmp(&p->Identity,&s->Identity,sizeof(s->Identity))||p->LifetimeUs!=s->Lifetime||
        (p->AllowRequests&&s->Config.Delivery.AllowRequests)!=s->ReplayRequests)return false;
    for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){const xtptx* t=&s->Tx[i];
        if((!t->Incoming&&t->Type==XTP_PUSH&&t->ID<p->PeerReceiveFloor)||
            (t->Incoming&&!t->Waiting&&t->ID<p->PeerResultFloor))return false;}
    return true;
}
#endif
XTP_API xtpresult xtpBind(xtpsession* s,const xtpbinding* b,uint64_t now,uint64_t* binding)
{
    if(!s||s->Busy||s->Bound||s->Closing||s->CloseRequested||s->Closed||s->Unbinding||!b||
        !xrtMemRangeValid(b,sizeof(*b))||!b->Lanes||!b->LaneCount||b->LaneCount>s->Config.MaxLanes||
        !xrtMemRangeValid(b->Lanes,b->LaneCount*sizeof(*b->Lanes))||(!b->Authenticated&&!b->AllowPlaintext)||
        !xtp__SessionAddress(s,(uintptr_t)binding,sizeof(*binding))||xtp__Overlap(binding,sizeof(*binding),b,sizeof(*b))||
        xtp__Overlap(binding,sizeof(*binding),b->Lanes,b->LaneCount*sizeof(*b->Lanes))||s->Binding==UINT64_MAX||now<s->LastNow)return XTP_E_ARGUMENT;
    for(size_t i=0;i<b->LaneCount;++i){const xtplane* l=&b->Lanes[i];
        if(!l->ID||(l->Mode!=XTP_INPUT_STREAM&&l->Mode!=XTP_INPUT_RECORD)||
            (!l->Reliable&&l->Mode!=XTP_INPUT_RECORD)||
            (l->Mode==XTP_INPUT_RECORD&&(l->RecordLimit<32||(l->Reliable&&l->RecordLimit<s->Config.MaxResponseBytes))))return XTP_E_ARGUMENT;
        for(size_t j=0;j<i;++j)if(b->Lanes[j].ID==l->ID)return XTP_E_ARGUMENT;}
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(b->Delivery&&(!xrtMemRangeValid(b->Delivery,sizeof(*b->Delivery))||
        xtp__Overlap(binding,sizeof(*binding),b->Delivery,sizeof(*b->Delivery))))return XTP_E_ARGUMENT;
    if(!xtpProof(s,b,now)){
        if(s->DeliveryStarted)s->CloseRequested=true;
        return XTP_E_STATE;}
    if(s->Suspended){
        if(b->LaneCount!=s->LaneCount)return XTP_E_ARGUMENT;
        for(size_t i=0;i<s->LaneCount;++i){xtplane a=s->Lanes[i].Config,z=b->Lanes[i];
            if(a.ID!=z.ID||a.Mode!=z.Mode||a.Reliable!=z.Reliable||a.RecordLimit!=z.RecordLimit)return XTP_E_ARGUMENT;}}
#else
    if(b->Delivery)return XTP_E_STATE;
#endif
    s->Busy=true;size_t made=0;xtpresult result=XTP_OK;
    for(;made<b->LaneCount;++made){xtpsessionlane* l=&s->Lanes[made];memset(l,0,sizeof(*l));
        l->Config=b->Lanes[made];l->Session=s;l->Index=made;l->Head=l->Tail=XTP_NONE;
        xtpreaderconfig c=s->Config.Reader;c.Mode=l->Config.Mode;c.Realloc=xtpSessionMemory;c.AllocatorUser=s;
        if(c.Mode==XTP_INPUT_RECORD&&c.Codec.MaxFrameBytes>l->Config.RecordLimit)c.Codec.MaxFrameBytes=l->Config.RecordLimit;
        xtpreaderevents e={xtpSessionFrame,xtpSessionDrop};result=xtpReaderCreate(&c,&e,l,&l->Reader);if(result!=XTP_OK)break;}
    if(result!=XTP_OK){for(size_t i=0;i<made;++i){xtpReaderDestroy(s->Lanes[i].Reader);s->Lanes[i].Reader=NULL;}}
    else {s->LaneCount=made;s->Bound=true;++s->Binding;s->LastNow=now;*binding=s->Binding;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(s->Config.Delivery.Enabled){const xtpdeliveryproof* p=b->Delivery;
            if(!s->DeliveryStarted){s->Identity=p->Identity;s->AuthUntil=p->UntilUs;s->Lifetime=p->LifetimeUs;
                s->ReplayRequests=p->AllowRequests&&s->Config.Delivery.AllowRequests;s->DeliveryStarted=true;}
            else if(p->UntilUs<s->AuthUntil)s->AuthUntil=p->UntilUs;
            s->Suspended=false;
            /* Cached results precede resubmissions. Retained order is stable. */
            for(unsigned pass=0;pass<2;++pass)for(size_t i=s->RetainHead;i!=XTP_NONE;i=s->Tx[i].RetainNext){xtptx* t=&s->Tx[i];
                if((pass==0)!=t->Incoming||t->Waiting)continue;
                t->Queued=false;
                if(t->Deadline>now&&xtpRetriable(s,t)&&xtpRoundReset(s,t))xtpTxQueue(s,i);}}
#endif
    }
    s->Busy=false;return result;
}
XTP_API xtpresult xtpUnbind(xtpsession* s,uint64_t binding,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||s->Binding!=binding||now<s->LastNow)return XTP_E_STATE;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    s->Bound=false;s->Unbinding=true;return xtpSessionLeave(s,XTP_OK);
}
XTP_API xtpresult xtpInput(xtpsession* s,uint64_t binding,uint32_t id,xbytesview in,bool end,uint64_t now,xtpreadprogress* p)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE||!xtp__SessionAddress(s,(uintptr_t)p,sizeof(*p))||
        !xtp__SessionAddress(s,(uintptr_t)in.Data,in.Size))return XTP_E_ARGUMENT;
    s->Busy=true;s->LastNow=now;s->TurnFrames=s->Config.Reader.MaxFrames;
    xtpresult r=xtpReaderInput(s->Lanes[li].Reader,in,end,now,p);
    if(r<0&&r!=XTP_E_ARGUMENT)xtpSessionFault(s,r,li,true);
    return xtpSessionLeave(s,r);
}
XTP_API xtpresult xtpEnd(xtpsession* s,uint64_t binding,uint32_t id,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE)return XTP_E_ARGUMENT;
    s->Busy=true;s->LastNow=now;s->TurnFrames=s->Config.Reader.MaxFrames;
    xtpresult r=xtpReaderEnd(s->Lanes[li].Reader,now);
    if(r<0)xtpSessionFault(s,r,li,true);
    return xtpSessionLeave(s,r);
}
XTP_API xtpresult xtpResume(xtpsession* s,uint64_t binding,uint32_t id,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||binding!=s->Binding||s->CloseRequested||now<s->LastNow)return XTP_E_STATE;
    size_t li=xtpSessionLane(s,id);if(li==XTP_NONE)return XTP_E_ARGUMENT;
    s->LastNow=now;return xtpReaderResume(s->Lanes[li].Reader);
}
/* Operations output must never overwrite a descriptor or any borrowed input. */
static bool xtpSubmitOutput(const xtpmessage* m,const xtpsubmitoptions* o,size_t count,
    xtpoperationtoken* out)
{
    if(!out)return true;
    size_t n=count*sizeof(*out);
    if(xtp__Overlap(out,n,m,count*sizeof(*m))||(o&&xtp__Overlap(out,n,o,count*sizeof(*o))))return false;
    for(size_t i=0;i<count;++i){
        if(xtp__Overlap(out,n,m[i].Body.Data,m[i].Body.Size)||
            (m[i].Command.Kind==XTP_COMMAND_NAME&&xtp__Overlap(out,n,m[i].Command.As.Name.Data,m[i].Command.As.Name.Size)))return false;
        const xtpparams* p=&m[i].Params;
        if(p->Kind==XTP_PARAMS_ARRAY){
            if(xtp__Overlap(out,n,p->As.Array,p->Count*sizeof(*p->As.Array)))return false;
            for(size_t j=0;j<p->Count;++j)if(xtp__Overlap(out,n,p->As.Array[j].Data,p->As.Array[j].Size))return false;
        }else{
            if(xtp__Overlap(out,n,p->As.Table,p->Count*sizeof(*p->As.Table)))return false;
            for(size_t j=0;j<p->Count;++j)if(xtp__Overlap(out,n,p->As.Table[j].Key.Data,p->As.Table[j].Key.Size)||
                xtp__Overlap(out,n,p->As.Table[j].Value.Data,p->As.Table[j].Value.Size))return false;
        }
    }return true;
}
XTP_API xtpresult xtpSubmitBatch(xtpsession* s,const xtpmessage* messages,size_t count,
    const xtpsubmitoptions* options,uint64_t now,xtpoperationtoken* operations)
{
    if(!s||!s->Bound||s->Closing||s->CloseRequested||s->Closed||s->Unbinding||
        (s->Busy&&!s->Callback)||now<s->LastNow)return XTP_E_STATE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&now>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!count||count>s->Config.Reader.Codec.MaxBatch||!messages||
        !xrtMemRangeValid(messages,count*sizeof(*messages))||
        (options&&!xrtMemRangeValid(options,count*sizeof(*options)))||
        (operations&&!xtp__SessionAddress(s,(uintptr_t)operations,count*sizeof(*operations))))return XTP_E_ARGUMENT;
    xtppacket packets[XTP_BATCH_MAX];size_t sizes[XTP_BATCH_MAX],lanes[XTP_BATCH_MAX],taken[XTP_BATCH_MAX];
    uint64_t due[XTP_BATCH_MAX],id=s->NextID;size_t reliable=0,total=0;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    size_t replay=0;
#endif
    for(size_t i=0;i<count;++i){const xtpmessage* m=&messages[i];xtpsubmitoptions o={0};if(options)o=options[i];
        if(m->Type!=XTP_REQUEST&&m->Type!=XTP_PUSH&&m->Type!=XTP_NOTIFY)return XTP_E_TYPE;
        if(o.Replay){
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(!s->Config.Delivery.Enabled||!s->DeliveryStarted||m->Type==XTP_NOTIFY||
                (m->Type==XTP_REQUEST&&!s->ReplayRequests))return XTP_E_STATE;
            ++replay;
#else
            return XTP_E_STATE;
#endif
        }
        lanes[i]=xtpSessionLane(s,o.LaneID);if(lanes[i]==XTP_NONE)return XTP_E_ARGUMENT;
        if(m->Type!=XTP_NOTIFY&&!s->Lanes[lanes[i]].Config.Reliable)return XTP_E_STATE;
        due[i]=o.DeadlineUs?o.DeadlineUs:xtpSessionDue(now,s->Config.DefaultOperationTimeoutUs);
        if(due[i]<=now)return XTP_E_DEADLINE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(o.Replay&&(due[i]>s->AuthUntil||due[i]>xtpSessionDue(now,s->Lifetime)))return XTP_E_LIMIT;
        if(o.Replay&&m->Type==XTP_REQUEST){
            if(s->Receipts[id%s->Config.DedupWindow].State)return XTP_E_LIMIT;
            for(size_t j=0;j<i;++j)if(options[j].Replay&&messages[j].Type==XTP_REQUEST&&
                packets[j].MessageID%s->Config.DedupWindow==id%s->Config.DedupWindow)return XTP_E_LIMIT;}
#endif
        if(m->Type!=XTP_NOTIFY){if(!id)return XTP_E_LIMIT;++reliable;}
        packets[i]=(xtppacket){0};packets[i].Type=m->Type;packets[i].Command=m->Command;
        packets[i].MessageID=m->Type==XTP_NOTIFY?0:id++;packets[i].Params=m->Params;packets[i].Body=m->Body;
        packets[i].Replayable=o.Replay;
        xtpresult r=xtpEncodeSize(&packets[i],1,&s->Config.Reader.Codec,&sizes[i]);if(r!=XTP_OK)return r;
        if(s->Lanes[lanes[i]].Config.Mode==XTP_INPUT_RECORD&&sizes[i]>s->Lanes[lanes[i]].Config.RecordLimit)return XTP_E_LIMIT;
        if(sizes[i]>s->Config.MaxTxBytes-total)return XTP_E_LIMIT;
        total+=sizes[i];
    }
    if(!xtpSubmitOutput(messages,options,count,operations))return XTP_E_ARGUMENT;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(replay>s->Config.Delivery.MaxReplayMessages-s->Retained)return XTP_E_LIMIT;
#endif
    if(reliable>s->Config.MaxPending-s->PendingCount||count>s->Config.MaxTxFrames-s->TxCount||
        count>s->Config.MaxTxFrames-s->Config.ResponseReserveFrames-s->NormalFrames||
        total>s->Config.MaxTxBytes-s->TxBytes||total>s->Config.MaxTxBytes-s->Config.ResponseReserveBytes-s->NormalBytes||
        !s->NextSerial||count-1>UINT64_MAX-s->NextSerial)return XTP_E_LIMIT;
    bool outerbusy=s->Busy,outercallback=s->Callback;
    s->Busy=true;s->Callback=false; /* Allocator callbacks cannot reenter. */
    size_t made=0;xtpresult result=XTP_E_MEMORY;
    for(;made<count;++made){taken[made]=xtpTxTake(s,lanes[made],sizes[made],true);
        if(taken[made]==XTP_NONE){result=XTP_E_LIMIT;break;}
        xtptx* t=&s->Tx[taken[made]];t->Bytes=xtpSessionMemory(s,NULL,sizes[made]);
        if(!t->Bytes){xtpTxFree(s,taken[made]);break;}
        t->Capacity=t->Size=sizes[made];size_t written;
        result=xtpEncode(&packets[made],1,&s->Config.Reader.Codec,t->Bytes,t->Capacity,&written);
        if(result!=XTP_OK){xtpTxFree(s,taken[made]);break;}
    }
    if(made<count){while(made)xtpTxFree(s,taken[--made]);s->Busy=outerbusy;s->Callback=outercallback;
        return result==XTP_OK?XTP_E_MEMORY:result;}
    for(size_t i=0;i<count;++i){xtptx* t=&s->Tx[taken[i]];t->Type=messages[i].Type;t->ID=packets[i].MessageID;
        t->Deadline=due[i];t->Notify=t->Type==XTP_NOTIFY;t->Tag=options?options[i].Tag:NULL;
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(packets[i].Replayable){xtpRetain(s,taken[i],due[i]);
            if(t->Type==XTP_REQUEST){xtpreceipt* r=&s->Receipts[t->ID%s->Config.DedupWindow];
                *r=(xtpreceipt){t->ID,0,t->Lane,1};++s->ReceiptCount;}}
#endif
        if(!t->Notify){size_t pi=s->FreePending;xtppending* p=&s->Pending[pi];s->FreePending=p->Next;
            memset(p,0,sizeof(*p));p->Active=true;p->ID=t->ID;p->Type=t->Type;p->Serial=t->Serial;
            p->Deadline=t->Deadline;p->Tag=t->Tag;p->Lane=t->Lane;p->Tx=taken[i];t->Pending=pi;
            size_t bucket=(size_t)p->ID&(s->BucketCount-1);p->Next=s->Buckets[bucket];s->Buckets[bucket]=pi;++s->PendingCount;}
        xtpTxQueue(s,taken[i]);if(operations)operations[i]=(xtpoperationtoken){{s->Cookie[0],t->Serial}};
    }
    s->NextID=id;s->LastNow=now;s->Submitted+=count;s->Busy=outerbusy;s->Callback=outercallback;return XTP_OK;
}
XTP_API xtpresult xtpSubmit(xtpsession* s,const xtpmessage* m,const xtpsubmitoptions* o,uint64_t now,xtpoperationtoken* operation)
{return xtpSubmitBatch(s,m,1,o,now,operation);}
XTP_API xtpresult xtpReply(xtpsession* s,xtprequesttoken token,const xtpresponse* response,uint64_t now)
{
    if(!s||s->Closing||s->CloseRequested||(s->Busy&&!s->Callback)||now<s->LastNow||
        token._Opaque[0]!=s->Cookie[0]||token._Opaque[1]!=s->Cookie[1]||!token._Opaque[2]||token._Opaque[2]>s->Config.MaxTxFrames)return XTP_E_STATE;
    size_t i=(size_t)token._Opaque[2]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Incoming||!t->Waiting||t->Serial!=token._Opaque[3])return XTP_E_STATE;
    if(!s->Bound&&!XTP_REPLAY(t))return XTP_E_STATE;
    if(now>=t->Deadline)return XTP_E_DEADLINE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&now>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!response||!xrtMemRangeValid(response,sizeof(*response)))return XTP_E_ARGUMENT;
    if(XTP_REPLAY(t)&&response->Status==XTP_STATUS_BUSY)return XTP_E_ARGUMENT;
    xtppacket p={0};p.Type=XTP_RESPONSE;p.MessageID=t->ID;p.Status=response->Status;p.Params=response->Params;p.Body=response->Body;
    size_t n;xtpresult r=xtpEncodeSize(&p,1,&s->Config.Reader.Codec,&n);if(r!=XTP_OK)return r;
    if(n>t->Capacity)return XTP_E_LIMIT;
    r=xtpEncode(&p,1,&s->Config.Reader.Codec,t->Bytes,t->Capacity,&n);if(r!=XTP_OK)return r;
    s->LastNow=now;s->TxBytes-=t->Charge-n;t->Charge=t->Size=n;t->Waiting=false;--s->Incoming;
    t->Deadline=xtpSessionDue(now,s->Config.DefaultOperationTimeoutUs);
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay)t->Deadline=t->KeepUntil;
#endif
    if(s->Bound)xtpTxQueue(s,i);
    return XTP_OK;
}
XTP_API xtpresult xtpOutput(xtpsession* s,uint32_t id,xtpoutput* out)
{
    if(!s||s->Busy||!s->Bound||s->Closing||s->CloseRequested)return XTP_E_STATE;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&s->LastNow>=s->AuthUntil)return XTP_E_DEADLINE;
#endif
    if(!xtp__SessionAddress(s,(uintptr_t)out,sizeof(*out)))return XTP_E_ARGUMENT;
    size_t i=XTP_NONE,li=xtpSessionLane(s,id);if(id&&li==XTP_NONE)return XTP_E_ARGUMENT;
    if(id)i=s->Lanes[li].Head;
    else for(size_t j=0;j<s->LaneCount;++j){size_t h=s->Lanes[(s->NextLane+j)%s->LaneCount].Head;
        if(h!=XTP_NONE&&s->Tx[h].Deadline>s->LastNow&&xtpRetriable(s,&s->Tx[h])){i=h;break;}}
    if(i==XTP_NONE)return XTP_AGAIN;
    xtptx* t=&s->Tx[i];xtplane* l=&s->Lanes[t->Lane].Config;xtpoutput o={0};
    if(t->Deadline<=s->LastNow||!xtpRetriable(s,t))return XTP_AGAIN; /* Drive retires expired heads. */
    o.Data=(xbytesview){(t->Bytes?t->Bytes:t->Control)+t->Offset,t->Size-t->Offset};
    o.Ticket=(xtpoutputticket){{s->Binding,i+1,XTP_TICKET(t),t->Offset}};o.BindingID=s->Binding;
    o.DeadlineUs=t->Deadline;o.LaneID=l->ID;o.Mode=l->Mode;o.Reliable=l->Reliable;
    o.Priority=t->Incoming||t->Type==XTP_ACK?1:0;*out=o;return XTP_OK;
}
XTP_API xtpresult xtpAdvance(xtpsession* s,xtpoutputticket ticket,size_t accepted,uint64_t now)
{
    if(!s||s->Busy||!s->Bound||s->CloseRequested||ticket._Opaque[0]!=s->Binding||
        !ticket._Opaque[1]||ticket._Opaque[1]>s->Config.MaxTxFrames||now<s->LastNow)return XTP_E_STATE;
    size_t i=(size_t)ticket._Opaque[1]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Queued||s->Lanes[t->Lane].Head!=i||XTP_TICKET(t)!=ticket._Opaque[2]||
        t->Offset!=ticket._Opaque[3]||accepted>t->Size-t->Offset||
        (s->Lanes[t->Lane].Config.Mode==XTP_INPUT_RECORD&&accepted&&accepted!=t->Size))return XTP_E_ARGUMENT;
    s->LastNow=now;if(!accepted)return XTP_AGAIN;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    s->NextLane=(t->Lane+1)%s->LaneCount;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(t->Replay&&!t->RoundStarted){t->RoundStarted=true;++t->Attempts;++s->Attempts;}
#endif
    t->Sent=true;t->Offset+=accepted;
    if(t->Pending!=XTP_NONE)s->Pending[t->Pending].Sent=true;
    if(t->Offset==t->Size){if(t->Notify){--s->TurnFrames;xtpNotifyComplete(s,i,now>=t->Deadline?XTP_FINISH_TIMEOUT:XTP_FINISH_SENT,now>=t->Deadline?XTP_E_DEADLINE:XTP_OK);}
        else{
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(t->Replay&&!t->RetireAfterSend)xtpTxUnqueue(s,i);else
#endif
            {if(t->Pending!=XTP_NONE)s->Pending[t->Pending].Tx=XTP_NONE;xtpTxFree(s,i);}}}
    return xtpSessionLeave(s,XTP_OK);
}
xtpresult xtp__DropNotify(xtpsession* s,const xtpoutputticket* ticket,xtpresult reason,uint64_t now)
{
    if(s->Busy||!s->Bound||s->CloseRequested||ticket->_Opaque[0]!=s->Binding||!ticket->_Opaque[1]||
        ticket->_Opaque[1]>s->Config.MaxTxFrames||now<s->LastNow)return XTP_E_STATE;
    size_t i=(size_t)ticket->_Opaque[1]-1;xtptx* t=&s->Tx[i];
    if(!t->Active||!t->Notify||!t->Queued||t->Offset||s->Lanes[t->Lane].Head!=i||
        XTP_TICKET(t)!=ticket->_Opaque[2]||ticket->_Opaque[3])return XTP_E_STATE;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames-1;
    xtpNotifyComplete(s,i,reason==XTP_E_DEADLINE?XTP_FINISH_TIMEOUT:XTP_FINISH_LOCAL_ERROR,reason);
    return xtpSessionLeave(s,XTP_OK);
}
static void xtpDueMin(uint64_t* next,uint64_t due)
{if(due&&(!*next||due<*next))*next=due;}
XTP_API xtpresult xtpDrive(xtpsession* s,uint64_t now,uint64_t* next)
{
    if(!s||s->Busy||!xtp__SessionAddress(s,(uintptr_t)next,sizeof(*next))||now<s->LastNow)return XTP_E_ARGUMENT;
    s->LastNow=now;s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;
    uint64_t due=0;size_t work=s->Config.Reader.MaxWorkBytes;bool again=false;
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->DeliveryStarted&&!s->Closed){if(now>=s->AuthUntil)s->CloseRequested=true;else xtpDueMin(&due,s->AuthUntil);}
#endif
    if(s->Closing||s->Unbinding||s->CloseRequested){xtpresult r=xtpSessionLeave(s,XTP_OK);*next=0;return r;}
    if(s->Closed){s->Busy=false;*next=0;return XTP_CLOSED;}
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active){xtppending* p=&s->Pending[i];
        if(p->Deadline<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
            if(p->Tx!=XTP_NONE&&s->Tx[p->Tx].Queued&&s->Tx[p->Tx].Offset&&s->Tx[p->Tx].Offset<s->Tx[p->Tx].Size)
                xtpSessionFault(s,XTP_E_DEADLINE,p->Lane,true);
            xtpcompletion c={0};c.Reason=XTP_FINISH_TIMEOUT;c.LocalResult=XTP_E_DEADLINE;
            c.Knowledge=p->Sent?XTP_KNOW_UNKNOWN:XTP_KNOW_NOT_SENT;xtpSessionComplete(s,i,&c);
        }else {
            xtpDueMin(&due,p->Deadline);
#if defined(XTP_FEATURE_XTP_DELIVERY)
            if(p->Tx!=XTP_NONE){xtptx* t=&s->Tx[p->Tx];
                if(t->Replay&&t->RetryAt){
                    if(t->RetryAt<=now&&s->Bound){
                        if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
                        if(t->Attempts>=s->Config.Delivery.MaxAttempts){xtpcompletion c={0};c.Reason=XTP_FINISH_LOCAL_ERROR;
                            c.LocalResult=XTP_E_LIMIT;c.Knowledge=XTP_KNOW_REJECTED;xtpSessionComplete(s,i,&c);}
                        else {t->RetryAt=0;xtpTxQueue(s,p->Tx);}}
                    else if(s->Bound)xtpDueMin(&due,t->RetryAt);}}
#endif
        }if(s->CloseRequested)break;}
    for(size_t i=0;i<s->Config.MaxTxFrames&&!s->CloseRequested;++i)if(s->Tx[i].Active){xtptx* t=&s->Tx[i];
        if(t->Pending!=XTP_NONE)continue;
        if(t->Deadline<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;
            if(t->Queued&&t->Offset&&t->Offset<t->Size)xtpSessionFault(s,XTP_E_DEADLINE,t->Lane,true);
            if(t->Waiting){
#if defined(XTP_FEATURE_XTP_DELIVERY)
                if(t->Replay&&t->KeepUntil<=now){xtpTxFree(s,i);continue;}
#endif
                if(!xtpControl(s,i,XTP_RESPONSE,t->ID,XTP_STATUS_DEADLINE_EXCEEDED))xtpSessionFault(s,XTP_E_STATE,t->Lane,true);}
            else if(t->Notify)xtpNotifyComplete(s,i,XTP_FINISH_TIMEOUT,XTP_E_DEADLINE);else xtpTxFree(s,i);
        }else xtpDueMin(&due,t->Deadline);}
    for(size_t i=0;s->Bound&&i<s->LaneCount&&!s->CloseRequested;++i){uint64_t rd=0;
        xtpresult r=xtp__ReaderDriveBudget(s->Lanes[i].Reader,now,s->TurnFrames,&work,&rd);xtpDueMin(&due,rd);
        if(r==XTP_AGAIN)again=true;else if(r<0)xtpSessionFault(s,r,i,true);}
    /* A callback may have submitted into a previously visited slot, and an
     * expired incoming request may just have become a queued timeout response. */
    for(size_t i=0;i<s->Config.MaxPending;++i)if(s->Pending[i].Active)xtpDueMin(&due,s->Pending[i].Deadline);
    for(size_t i=0;i<s->Config.MaxTxFrames;++i)if(s->Tx[i].Active){xtpDueMin(&due,s->Tx[i].Deadline);
#if defined(XTP_FEATURE_XTP_DELIVERY)
        if(s->Bound)xtpDueMin(&due,s->Tx[i].RetryAt);
#endif
    }
#if defined(XTP_FEATURE_XTP_DELIVERY)
    if(s->Receipts)for(size_t i=0;i<s->Config.DedupWindow;++i){xtpreceipt* r=&s->Receipts[i];
        if(r->State!=2)continue;
        if(r->Until<=now){if(!s->TurnFrames){again=true;continue;}--s->TurnFrames;xtpReceiptExpire(s,r);}
        else xtpDueMin(&due,r->Until);}
#endif
    xtpresult result=xtpSessionLeave(s,again?XTP_AGAIN:XTP_OK);*next=s->Closed?0:due;return result;
}
XTP_API xtpresult xtpClose(xtpsession* s,uint64_t now)
{
    if(!s||now<s->LastNow)return XTP_E_ARGUMENT;
    if(s->Busy&&!s->Callback)return XTP_E_STATE;
    s->LastNow=now;if(s->Closed)return XTP_CLOSED;s->CloseRequested=true;
    if(s->Callback)return XTP_OK;
    s->Busy=true;s->TurnFrames=s->Config.Reader.MaxFrames;return xtpSessionLeave(s,XTP_OK);
}
XTP_API xtpresult xtpDestroy(xtpsession* s)
{
    if(!s)return XTP_OK;
    if(s->Busy||s->Callback||!s->Closed)return XTP_E_STATE;
    s->Busy=true;
    xtpreallocproc f=s->Config.Realloc;void* u=s->Config.AllocatorUser;xtpSessionFreeTables(s);f(u,s,0);return XTP_OK;
}
XTP_API xtpresult xtpInfo(const xtpsession* s,xtpinfo* info)
{
    if(!s||!xtp__SessionAddress(s,(uintptr_t)info,sizeof(*info)))return XTP_E_ARGUMENT;
    *info=(xtpinfo){s->Binding,s->NextID,s->Submitted,s->Completed,s->Messages,s->Unknown,s->Duplicates,
        s->PendingCount,s->Incoming,s->TxCount,s->TxBytes,s->Storage,s->Peak,s->Bound,s->Closing||s->CloseRequested,s->Closed,
        0,0,s->HighID>=s->Config.DedupWindow?s->HighID-s->Config.DedupWindow+1:1,1,0,0,0,false,false};
#if defined(XTP_FEATURE_XTP_DELIVERY)
    info->Retained=s->Retained;info->Receipts=s->ReceiptCount;info->ResultFloor=s->ResultFloor;
    info->Attempts=s->Attempts;info->Retries=s->Retries;info->ResultAcks=s->ResultAcks;
    info->Suspended=s->Suspended;info->ReplayRequests=s->ReplayRequests;
#endif
    return XTP_OK;
}
#undef XTP_NONE
#undef XTP_REPLAY
#undef XTP_TICKET
#if !defined(XTP_FEATURE_XTP_DELIVERY)
#undef xtpKeep
#undef xtpRetriable
#endif
#endif
