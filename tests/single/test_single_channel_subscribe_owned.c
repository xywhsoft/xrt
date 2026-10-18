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
    if(busy->Ops!=ref.Ops || busy->Busy(ref.Data) ||
        !xrtChannelSubscribeOwnershipAdapterV1(ref,policies,1u)) return 3;
    if(!xrtOwnershipScopeEnd(&freeze) || !xrtChannelSubscribeStart(subscription)) return 4;
    if(xrtChannelTrySend(&channel,NULL)!=XCHANNEL_OK || data.notified!=1u) return 5;
    xrtChannelSubscribeDestroy(subscription);
    if(data.refs || !xrtChannelUnit(&channel)) return 6;
    return 0;
}
