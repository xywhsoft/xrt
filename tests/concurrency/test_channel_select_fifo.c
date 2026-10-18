#include "../../src/internal/xrt_channel.h"
#include "../test.h"

static unsigned wakes;
static void wake(ptr data) { ++wakes; if(data) ++*(unsigned*)data; }

static void test_abort_peer_notification(void)
{
    xchannel shared,other;
    xrt_channel_select_state states[2]={0};
    xrt_channel_select_waiter receiving[2]={0},sending={0};
    unsigned counts[2]={0};
    ptr outputs[2]={0};
    testRequire(xrtChannelInit(&shared,0u) && xrtChannelInit(&other,0u),"abort peer channels init failed");
    for(size_t i=0;i<2u;i++) {
        xrtAtomic32Init(&states[i].Winner,XRT_CHANNEL_SELECT_NONE);
        states[i].Wake=wake; states[i].WakeData=&counts[i];
    }
    receiving[0]=(xrt_channel_select_waiter){.Select=&states[0],.Channel=&shared,
        .Output=&outputs[0],.Operation=XCHANNEL_OP_RECV};
    receiving[1]=(xrt_channel_select_waiter){.Select=&states[0],.Channel=&other,
        .Output=&outputs[1],.Index=1u,.Operation=XCHANNEL_OP_RECV};
    sending=(xrt_channel_select_waiter){.Select=&states[1],.Channel=&shared,
        .Value=(ptr)(uintptr_t)42u,.Operation=XCHANNEL_OP_SEND};
    testRequire(__xrtChannelSelectRegister(&receiving[0]) &&
        __xrtChannelSelectRegister(&receiving[1]) && __xrtChannelSelectRegister(&sending),"abort peer registration failed");
    /* A is testing its other unavailable case while B tests the shared peer.
     * B must notify A after abandoning CLAIMING, not let both drivers sleep. */
    testRequire(__xrtChannelSelectClaim(&receiving[1]),"simulate A's other-case claim failed");
    counts[0]=counts[1]=0;
    __xrtChannelSelectTryWaiters(&sending,1u,0u);
    testRequire(counts[0]==1u && counts[1]==0u,"abort lost peer wake or woke itself into polling");
    xrt_channel_impl* impl=__xrtChannelLock(&other);
    testRequire(impl!=NULL,"abort other channel lock failed");
    __xrtChannelSelectAbort(&receiving[1]);
    __xrtChannelUnlock(impl);
    __xrtChannelSelectTryWaiters(receiving,2u,0u);
    testRequire(outputs[0]==(ptr)(uintptr_t)42u &&
        xrtAtomic32Load(&states[0].Winner,XMEMORY_ACQUIRE)==0u &&
        xrtAtomic32Load(&states[1].Winner,XMEMORY_ACQUIRE)==0u,"reawakened peer failed to commit once");
    __xrtChannelSelectUnregisterAll(receiving,2u);
    __xrtChannelSelectUnregisterAll(&sending,1u);
    testRequire(xrtChannelUnit(&shared) && xrtChannelUnit(&other),"abort peer nodes not unlinked");
}

static void check(xchannel* channel,xrt_channel_select_waiter* nodes,size_t count)
{
    xrt_channel_impl* impl=__xrtChannelLock(channel);
    size_t found=0;
    bool sending=false;
    xrt_channel_select_waiter* firstSend=NULL;
    testRequire(impl!=NULL,"FIFO probe lock failed");
    xrt_channel_select_waiter* node=impl->SelectWaiters;
    if(node) do {
        testRequire(node>=nodes && node<nodes+count && node->Registered &&
            node->Next->Previous==node && node->Previous->Next==node,"intrusive FIFO link corrupted");
        if(node->Operation==XCHANNEL_OP_SEND) {
            if(!firstSend) firstSend=node;
            sending=true;
        } else testRequire(!sending,"receive/send segments interleaved");
        node=node->Next;
        testRequire(++found<=count,"FIFO circular traversal did not terminate");
    } while(node!=impl->SelectWaiters);
    size_t expected=0;
    for(size_t i=0;i<count;i++) expected+=nodes[i].Registered ? 1u : 0u;
    testRequire(found==expected && firstSend==impl->SelectSendWaiters,"FIFO segment boundary/count wrong");
    __xrtChannelUnlock(impl);
}

int main(void)
{
    xchannel channel;
    xrt_channel_select_waiter nodes[8]={0};
    xrt_channel_select_state states[8]={0};
    ptr outputs[8]={0};
    const xchannelop operations[8]={XCHANNEL_OP_SEND,XCHANNEL_OP_RECV,XCHANNEL_OP_SEND,
        XCHANNEL_OP_SEND,XCHANNEL_OP_RECV,XCHANNEL_OP_SEND,XCHANNEL_OP_RECV,XCHANNEL_OP_RECV};
    testRequire(xrtChannelInit(&channel,0u),"FIFO channel init failed");
    for(size_t i=0;i<8u;i++) {
        xrtAtomic32Init(&states[i].Winner,XRT_CHANNEL_SELECT_NONE);
        states[i].Wake=wake;
        nodes[i]=(xrt_channel_select_waiter){.Select=&states[i],.Channel=&channel,
            .Value=(ptr)(uintptr_t)(20u+i),.Output=&outputs[i],.Index=(uint32)i,.Operation=operations[i]};
        testRequire(__xrtChannelSelectRegister(&nodes[i]),"FIFO append failed");
        check(&channel,nodes,8u);
    }
    const size_t receiveOrder[4]={1u,4u,6u,7u},sendOrder[4]={0u,2u,3u,5u};
    for(size_t i=0;i<4u;i++) {
        testRequire(xrtChannelTrySend(&channel,(ptr)(uintptr_t)91u)==XCHANNEL_OK,"FIFO receive peer failed");
        testRequire(outputs[receiveOrder[i]]==(ptr)(uintptr_t)91u,"receive FIFO order changed");
    }
    for(size_t i=0;i<4u;i++) {
        ptr output=NULL;
        testRequire(xrtChannelTryRecv(&channel,&output)==XCHANNEL_OK &&
            (uintptr_t)output==20u+sendOrder[i],"send FIFO order changed");
    }
    const size_t removal[8]={3u,0u,7u,4u,2u,6u,1u,5u};
    unsigned before=wakes;
    for(size_t i=0;i<8u;i++) {
        __xrtChannelSelectUnregisterAll(&nodes[removal[i]],1u);
        check(&channel,nodes,8u);
    }
    testRequire(wakes==before,"unlink caused a same-direction wake storm");
    testRequire(xrtChannelUnit(&channel),"FIFO channel unit failed");
    test_abort_peer_notification();
    puts("[PASS] segmented intrusive FIFO, exact peer order and arbitrary constant-time unlink");
    return 0;
}
