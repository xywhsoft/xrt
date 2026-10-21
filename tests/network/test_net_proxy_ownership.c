/* Offline ownership only: no sockets, resolver, malformed traffic or fuzzing. */
#ifndef XRT_MODULE_NET_PROXY
#define XRT_MODULE_NET_PROXY
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef NET_PROXY_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include "../test_thread.h"

static xnetproxy* create(void)
{
    char host[]="127.0.0.1";uint8 user[]={1,0,2},password[]={3,0,4};
    xnetproxyconfig config;xrtNetProxyConfigInit(&config);
    config.Host=(xstrview){host,sizeof(host)-1};config.Port=1080;
    config.Username=(xbytesview){user,sizeof(user)};config.Password=(xbytesview){password,sizeof(password)};
    xnetproxy* proxy=xrtNetProxyCreate(&config);testRequire(proxy!=NULL,"owned configuration created");
    memset(host,'x',sizeof(host));memset(user,0,sizeof(user));memset(password,0,sizeof(password));return proxy;
}
static bool noEdges(xrtownershipref ref,ptr context)
{(void)ref;(void)context;testRequire(false,"immutable configuration has no outgoing node");return false;}
static size_t count(xrtownershipref ref)
{size_t result=0;testRequire(ref.Ops&&ref.Ops->Count(ref.Data,&result),"real native count");return result;}
static void frozen(xrtownershipscope* freeze)
{
    for(unsigned spin=0;spin<1000000;++spin){if(xrtOwnershipFreezeTryBegin(freeze))return;testThreadYield();}
    testRequire(false,"bounded graph freeze admission");
}
static void physical(void)
{
    xnetproxy* proxy=create();xrtownershipref ref=xrtNetProxyOwnership(proxy),nil=xrtNetProxyOwnership(NULL);
    xrtownershipscope freeze={0};frozen(&freeze);
    testRequire(ref.Data==proxy&&!nil.Data&&!nil.Ops&&count(ref)==1,"canonical physical identity");
    testRequire(ref.Ops->Trace(ref.Data,noEdges,NULL)&&!ref.Ops->Trace(ref.Data,NULL,NULL),"leaf trace validates visitor");
    size_t untouched=91;testRequire(!ref.Ops->Count(NULL,&untouched)&&untouched==91&&
        !ref.Ops->Count(ref.Data,NULL),"count refusal preserves output");
    xrtownershipops copy=*ref.Ops;
    testRequire(!xrtNetProxyOwnershipAdapterV1((xrtownershipref){proxy,&copy})&&
        !xrtNetProxyOwnershipAdapterV1(nil)&&!xrtNetProxyOwnershipAdapterV1((xrtownershipref){NULL,ref.Ops}),
        "adapter admits exact native Ops only");
    const xrtownershipadapterv1* adapter=xrtNetProxyOwnershipAdapterV1(ref);int token=0;
    testRequire(adapter&&adapter->size==sizeof(*adapter)&&!adapter->Finalize&&!adapter->Finish,"immutable adapter shape");
    testRequire(!adapter->Claim(proxy,NULL)&&adapter->Hold(proxy)&&count(ref)==2,"hold is a real native reference");
    testRequire(adapter->Claim(proxy,&token),"leaf claim");adapter->Restore(proxy,&token);
    adapter->Drop(proxy);testRequire(count(ref)==1,"drop consumes the actual pin");
    testRequire(adapter->Claim(proxy,&token),"leaf reclaim");adapter->Clear(proxy,&token);
    testRequire(count(ref)==1,"leaf clear does not consume creator");
    testRequire(xrtOwnershipScopeEnd(&freeze),"physical freeze end");
    xnetproxyinfo info;testRequire(xrtNetProxyInfo(proxy,&info)&&info.Host.Size==9&&
        !memcmp(info.Host.Data,"127.0.0.1",9)&&info.Username.Size==3&&info.Username.Data[0]==1&&
        info.Username.Data[1]==0&&info.Password.Data[2]==4,"copied binary credentials unchanged");
    xrtNetProxyRelease(proxy);
}
typedef struct Work {xnetproxy* proxy;xatomic32 stage;bool release;} Work;
static int transition(ptr raw)
{
    Work* work=raw;xrtAtomic32Store(&work->stage,1,XMEMORY_RELEASE);
    if(work->release)xrtNetProxyRelease(work->proxy);
    else testRequire(xrtNetProxyRetain(work->proxy)==work->proxy,"thread real retain");
    xrtAtomic32Store(&work->stage,2,XMEMORY_RELEASE);
    if(!work->release)xrtNetProxyRelease(work->proxy);
    return 0;
}
static void excluded(bool release)
{
    Work work={0};work.proxy=create();work.release=release;xrtAtomic32Init(&work.stage,0);
    if(release)testRequire(xrtNetProxyRetain(work.proxy)==work.proxy,"worker actual release credit");
    xrtownershipref ref=xrtNetProxyOwnership(work.proxy);xrtownershipscope freeze={0};frozen(&freeze);
    testthread thread={0};thread.Proc=transition;thread.Data=&work;testThreadsStart(&thread,1);
    while(!xrtAtomic32Load(&work.stage,XMEMORY_ACQUIRE))testThreadYield();
    for(unsigned spin=0;spin<100000;++spin){
        testRequire(xrtAtomic32Load(&work.stage,XMEMORY_ACQUIRE)==1,
            "native RC transition cannot escape another thread's freeze");testThreadYield();
    }
    testRequire(count(ref)==(release?2u:1u),"frozen count cannot change under a real transition");
    testRequire(xrtOwnershipScopeEnd(&freeze),"release transition exclusion");testThreadsJoin(&thread,1);
    frozen(&freeze);testRequire(thread.Result==0&&count(ref)==1,"transition completes after freeze");
    testRequire(xrtOwnershipScopeEnd(&freeze),"completed transition freeze end");xrtNetProxyRelease(work.proxy);
}
typedef struct Shared {xnetproxy* proxy;xatomic32 ready,start,done;} Shared;
static int churn(ptr raw)
{
    Shared* work=raw;xrtAtomic32FetchAdd(&work->ready,1,XMEMORY_ACQ_REL);
    while(!xrtAtomic32Load(&work->start,XMEMORY_ACQUIRE))testThreadYield();
    for(unsigned i=0;i<2000;++i){
        testRequire(xrtNetProxyRetain(work->proxy)==work->proxy,"concurrent retain");
        xnetproxyinfo info;testRequire(xrtNetProxyInfo(work->proxy,&info)&&info.Port==1080,"immutable concurrent view");
        xrtNetProxyRelease(work->proxy);
    }
    xrtAtomic32FetchAdd(&work->done,1,XMEMORY_RELEASE);return 0;
}
static void concurrent(void)
{
    Shared work={0};work.proxy=create();xrtAtomic32Init(&work.ready,0);xrtAtomic32Init(&work.start,0);xrtAtomic32Init(&work.done,0);
    testthread threads[4]={{0}};for(unsigned i=0;i<4;++i){threads[i].Proc=churn;threads[i].Data=&work;}
    testThreadsStart(threads,4);while(xrtAtomic32Load(&work.ready,XMEMORY_ACQUIRE)!=4)testThreadYield();
    xrtAtomic32Store(&work.start,1,XMEMORY_RELEASE);
    do{
        xrtownershipscope freeze={0};if(xrtOwnershipFreezeTryBegin(&freeze)){
            size_t refs=count(xrtNetProxyOwnership(work.proxy));testRequire(refs>=1&&refs<=5,"only physical thread references counted");
            testRequire(xrtOwnershipScopeEnd(&freeze),"concurrent observation end");
        }testThreadYield();
    }while(xrtAtomic32Load(&work.done,XMEMORY_ACQUIRE)!=4);
    testThreadsJoin(threads,4);
    xrtownershipscope freeze={0};frozen(&freeze);testRequire(count(xrtNetProxyOwnership(work.proxy))==1,"all transient references returned");
    testRequire(xrtOwnershipScopeEnd(&freeze),"final count freeze end");xrtNetProxyRelease(work.proxy);
}
int main(void)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);physical();excluded(false);excluded(true);concurrent();
    testRequire(!xrtGetError(),"no hidden diagnostic");xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount,"proxy ownership exact allocation balance");
    testMemoryDebugDrain("proxy ownership debug drain");
    puts("Proxy configuration ownership: real RC, immutable leaf, cross-thread freeze, 8000 balanced transitions");return 0;
}
