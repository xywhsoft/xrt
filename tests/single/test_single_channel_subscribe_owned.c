#define XRT_MODULE_CHANNEL_SUBSCRIBE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

typedef struct context { size_t refs,notified; } context;
static bool count(const void* data,size_t* refs)
{ *refs=((const context*)data)->refs; return *refs!=0; }
static bool trace(const void* data,xrtownershipvisitor visit,ptr opaque)
{ (void)data; (void)visit; (void)opaque; return true; }
static const xrtownershipops ops={count,trace};
static void notify(xchannelsubscription* subscription,ptr data)
{
    xchannelselectresult selected;
    ptr value;
    if(xrtChannelSubscribeResult(subscription,&selected,&value) && selected.Wait==XWAIT_OK &&
        selected.Result==XCHANNEL_OK && !value) ++((context*)data)->notified;
}
static void drop(const void* data) { --((context*)data)->refs; }
static const xchannelsubscribeownershipv1 policy={sizeof(policy),notify,drop,&ops};

typedef struct errorprobe { size_t calls; bool outside; } errorprobe;
static void on_error(const xerror* error,ptr data)
{
    errorprobe* probe=data;
    xrtownershipscope freeze={0};
    (void)error;
    ++probe->calls;
    probe->outside=xrtOwnershipFreezeTryBegin(&freeze);
    if(probe->outside && !xrtOwnershipScopeEnd(&freeze)) abort();
}

/* Single implementation mode deliberately permits this white-box saturation
 * probe. Do not expose a public refcount setter or allocate billions of refs. */
static bool retention_boundaries(xchannelsubscription* subscription,
    const xrtownershipadapterv1* adapter)
{
    errorprobe errors={0};
    xrtownershipscope freeze={0},mutation={0};
    bool valid;
    xrtClearError();
    xrtSetErrorHandler(on_error,&errors);
    if(!xrtOwnershipFreezeTryBegin(&freeze)) return false;
    xrtAtomic32Store(&subscription->References,UINT32_MAX-2u,XMEMORY_RELAXED);
    valid=!adapter->Hold(subscription) && !errors.calls && !xrtGetError() &&
        xrtAtomic32Load(&subscription->References,XMEMORY_RELAXED)==UINT32_MAX-2u;
    xrtAtomic32Store(&subscription->References,UINT32_MAX-3u,XMEMORY_RELAXED);
    valid=valid && adapter->Hold(subscription) && !errors.calls && !xrtGetError() &&
        xrtAtomic32Load(&subscription->References,XMEMORY_RELAXED)==UINT32_MAX-2u;
    if(!xrtOwnershipScopeEnd(&freeze)) return false;
    valid=valid && !xrtChannelSubscribeRef(subscription) && errors.calls==1u && errors.outside;
    xrtClearError();
    valid=valid && !xrtChannelSubscribeStart(subscription) && errors.calls==2u && errors.outside;
    xrtClearError();
    valid=valid && !xrtChannelSubscribeStop(subscription,XWAIT_CANCELLED) && errors.calls==3u && errors.outside;
    xrtClearError();
    valid=valid && !xrtAtomic32Load(&subscription->Started,XMEMORY_RELAXED) &&
        !xrtAtomic32Load(&subscription->Stop,XMEMORY_RELAXED) &&
        xrtAtomic32Load(&subscription->External,XMEMORY_RELAXED)==1u &&
        xrtAtomic32Load(&subscription->References,XMEMORY_RELAXED)==UINT32_MAX-2u;
    if(!xrtOwnershipMutationBegin(&mutation)) return false;
    xrtAtomic32Store(&subscription->References,1u,XMEMORY_RELAXED);
    if(!xrtOwnershipScopeEnd(&mutation)) return false;
    xrtSetErrorHandler(NULL,NULL);
    return valid;
}

int main(void)
{
    xchannel channel;
    context data={1u,0u};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    const xchannelsubscribeownershipv1* policies[]={&policy};
    xrtownershipscope freeze={0};
    if(!xrtChannelInit(&channel,0u)) return 1;
    xchannelsubscription* subscription=xrtChannelSubscribeCreateOwnedV1(&item,1u,&data,&policy);
    if(!subscription || !xrtOwnershipFreezeTryBegin(&freeze)) return 2;
    xrtownershipref ref=xrtChannelSubscribeOwnership(subscription);
    const xrtownershipquiescencev1* busy=xrtChannelSubscribeOwnershipQuiescenceV1Get();
    const xrtownershipadapterv1* adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,policies,1u);
    if(busy->Ops!=ref.Ops || busy->Busy(ref.Data) || !adapter) return 3;
    if(!xrtOwnershipScopeEnd(&freeze) || !retention_boundaries(subscription,adapter) ||
        !xrtChannelSubscribeStart(subscription)) return 4;
    if(xrtChannelTrySend(&channel,NULL)!=XCHANNEL_OK || data.notified!=1u) return 5;
    xrtChannelSubscribeDestroy(subscription);
    if(data.refs || !xrtChannelUnit(&channel)) return 6;
    return 0;
}
