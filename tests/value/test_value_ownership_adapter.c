#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef VALUE_OWNERSHIP_ADAPTER_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

static unsigned traces,finalizers,drops;
static bool admit(xrtownershipref ref,ptr context)
{(void)context;return xrtValueOwnershipAdapterV1(ref)!=NULL;}
static bool foreignTrace(const xvalue* value,xrtownershipvisitor visit,ptr context)
{(void)value;(void)visit;(void)context;++traces;return true;}
static void foreignDrop(ptr data,ptr context)
{(void)data;(void)context;++drops;}
static void foreignFinalizer(xvalue* value,ptr context)
{(void)value;(void)context;++finalizers;}
static const xvaluehandleops foreignOps={NULL,foreignDrop,NULL,NULL};
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount,"physical adapter memory balance");
}
static void weakCheck(xvalue* weak,bool available)
{
    xvalue* value=xrtValueWeakRefLock(weak);int64 integer=0;
    testRequire(value!=NULL,"weak promotion returns a value");
    testRequire(available?(xrtValueGetInt(value,&integer) && integer==42):xrtValueType(value)==XVALUE_NULL,
        "weak promotion respects quarantine");
    xrtValueRelease(value);
}
static void transaction(bool abortPlan)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* array=xrtValueArray();xvalue* item=xrtValueInt(42);xvalue* weak=xrtValueWeakRef(item);
    testRequire(array && item && weak && xrtValueArrayAppendNew(array,xrtValueRetain(item)) &&
        xrtValueArrayAppendNew(array,xrtValueRetain(item)),"actual duplicate owning slots");
    xvalue* alias=xrtValueClone(array);testRequire(alias!=NULL,"actual COW shell");xrtValueRelease(item);
    /* TCC cannot initialize this automatic struct array from struct-returning
     * calls. Separate assignments preserve the exact two real owning slots. */
    xrtownershipref slots[2];slots[0]=xrtValueOwnership(array);slots[1]=xrtValueOwnership(alias);
    xrtownershipsnapshot* snapshot=NULL;xrtownershipscope freeze={0};unsigned token=0;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) &&
        xrtOwnershipSnapshotCreate(NULL,0,slots,2,admit,NULL,&snapshot),"snapshot exact retiring owner slots");
    size_t nodes=xrtOwnershipSnapshotNodeCount(snapshot);testRequire(nodes==4,"two shells, backing, repeated scalar");
    for(size_t i=0;i<nodes;++i){
        xrtownershipnode node;testRequire(xrtOwnershipSnapshotNode(snapshot,i,&node) && !node.Reachable,"unrooted physical node");
        const xrtownershipadapterv1* adapter=xrtValueOwnershipAdapterV1(node.Reference);
        testRequire(adapter && adapter->size==sizeof(*adapter) && adapter->Hold(node.Reference.Data) &&
            adapter->Claim(node.Reference.Data,&token),"real hold then quarantine");
    }
    testRequire(xrtOwnershipScopeEnd(&freeze),"leave admission");weakCheck(weak,false);
    testRequire(!xrtValueWeakRefExpired(weak),"quarantine is not terminal expiry");
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"reenter exclusive clear");
    /* The only owning fields outside the snapshot are these exact caller
     * slots; no foreign callback runs between this test's two freezes. */
    if(!abortPlan){xrtValueRelease(array);xrtValueRelease(alias);array=alias=NULL;}
    const xrtownershipadapterv1* adapters[4];xrtownershipnode records[4];
    for(size_t i=0;i<nodes;++i){
        testRequire(xrtOwnershipSnapshotNode(snapshot,i,&records[i]),"read prepared node");
        adapters[i]=xrtValueOwnershipAdapterV1(records[i].Reference);testRequire(adapters[i]!=NULL,"stable adapter");
    }
    testRequire(xrtMemDebugFailAfter(0),"fail next allocation for allocation-free methods");
    for(size_t i=0;i<nodes;++i){
        if(abortPlan)adapters[i]->Restore(records[i].Reference.Data,&token);
        else adapters[i]->Clear(records[i].Reference.Data,&token);
    }
    testRequire(!xrtMemDebugFailTriggered(),"restore and physical clear allocate nothing");xrtMemDebugFailClear();
    testRequire(xrtOwnershipScopeEnd(&freeze),"leave clear before releases");
    for(size_t i=nodes;i--!=0;)adapters[i]->Drop(records[i].Reference.Data);
    xrtOwnershipSnapshotDestroy(snapshot);
    if(abortPlan){weakCheck(weak,true);testRequire(xrtValueCount(array)==2 && xrtValueCount(alias)==2,"abort fields intact");}
    xrtValueRelease(array);xrtValueRelease(alias);testRequire(xrtValueWeakRefExpired(weak),"terminal expiry after all real owners end");
    xrtValueRelease(weak);balance(&before);
}
static void opaque(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned payload=0;ptr handle=&payload;
    xvalue* value=xrtValueHandleTake(&handle,&foreignOps,NULL);testRequire(value!=NULL,"opaque handle");
    testRequire(xrtValueHandleOwnershipBind(value,foreignTrace),"trace exists but is not certification");
    xrtownershipsnapshot* output=NULL;xrtownershipref root=xrtValueOwnership(value);xrtownershipscope freeze={0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) &&
        !xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,NULL,&output) && output==NULL && traces==0,
        "opaque handle refused before its trace callback");
    testRequire(xrtOwnershipScopeEnd(&freeze),"end refusal");xrtClearError();xrtValueRelease(value);
    value=xrtValueObject();testRequire(value && xrtValueObjectFinalizerBind(value,foreignFinalizer,NULL),"custom finalizer with NULL data");
    root=xrtValueOwnership(value);unsigned ran=finalizers;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) &&
        !xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,NULL,&output) && !output && finalizers==ran,
        "NULL context does not certify foreign finalizer code");
    testRequire(xrtOwnershipScopeEnd(&freeze),"end finalizer refusal");xrtClearError();xrtValueRelease(value);
    testRequire(finalizers==ran+1,"original terminal finalizer still executes exactly once");balance(&before);
}
static void blobs(bool abortPlan)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const unsigned char bytesSource[3]={'a',0,'b'};
    str takenText=xrtMalloc(4);bytes takenBytes=xrtMalloc(3);
    testRequire(takenText&&takenBytes,"allocate actual transferred byte buffers");
    memcpy(takenText,"abc",4);memcpy(takenBytes,bytesSource,3);
    xvalue* values[6];
    values[0]=xrtValueString(XRT_STR_LITERAL("inline"));
    values[1]=xrtValueBytes((xbytesview){bytesSource,3});
    values[2]=xrtValueString(XRT_STR_LITERAL(""));
    values[3]=xrtValueBytes((xbytesview){NULL,0});
    values[4]=xrtValueStringTake(&takenText,3);
    values[5]=xrtValueBytesTake(&takenBytes,3);
    testRequire(!takenText&&!takenBytes,"take consumes each actual buffer");
    xvalue* array=xrtValueArray();testRequire(array!=NULL,"blob container");
    for(unsigned i=0;i<6;++i){
        testRequire(values[i]!=NULL,"all inline, empty and separately owned shapes");
        xvalue* copy=xrtValueClone(values[i]);
        testRequire(copy==values[i]&&copy,"immutable Clone retains the same physical blob");
        if(i==0||i==2||i==4){
            const char* text=i==0?"inline":i==2?"":"abc";size_t size=i==0?6u:i==2?0u:3u;xstrview actual={0};
            testRequire(xrtValueGetString(copy,&actual)&&actual.Size==size&&
                (!size||!memcmp(actual.Data,text,size)),"string bytes preserved across Clone");
        }else{
            size_t size=i==3?0u:3u;xbytesview actual={0};
            testRequire(xrtValueGetBytes(copy,&actual)&&actual.Size==size&&
                (!size||!memcmp(actual.Data,bytesSource,size)),"binary and embedded zero preserved across Clone");
        }
        testRequire(xrtValueArrayAppendNew(array,values[i])&&xrtValueArrayAppendNew(array,copy),"transfer actual blob slots");
    }
    xrtownershipscope freeze={0};xrtownershipsnapshot* snapshot=NULL;unsigned token=0;
    xrtownershipref slot=xrtValueOwnership(array);
    testRequire(xrtOwnershipFreezeTryBegin(&freeze)&&
        xrtOwnershipSnapshotCreate(NULL,0,&slot,1,admit,NULL,&snapshot),"complete blob graph admission");
    testRequire(xrtOwnershipSnapshotNodeCount(snapshot)==8,"one shell and backing plus six twice-owned blob shells");
    xrtownershipnode records[8];const xrtownershipadapterv1* adapters[8];
    for(unsigned i=0;i<8;++i){
        testRequire(xrtOwnershipSnapshotNode(snapshot,i,&records[i])&&!records[i].Reachable,"actual unrooted blob node");
        testRequire(records[i].StrongCount==records[i].InternalCount&&
            records[i].StrongCount==(i<2?1u:2u),"Clone contributes an actual second owning edge, not another node");
        adapters[i]=xrtValueOwnershipAdapterV1(records[i].Reference);
        testRequire(adapters[i]&&adapters[i]->Hold(records[i].Reference.Data)&&
            adapters[i]->Claim(records[i].Reference.Data,&token),"hold and claim exact blob identity");
    }
    if(!abortPlan){xrtValueRelease(array);array=NULL;}
    testRequire(xrtMemDebugFailAfter(0),"arm allocation-free blob transaction");
    for(unsigned i=0;i<8;++i){
        if(abortPlan)adapters[i]->Restore(records[i].Reference.Data,&token);
        else adapters[i]->Clear(records[i].Reference.Data,&token);
    }
    testRequire(!xrtMemDebugFailTriggered(),"blob restore and clear allocate nothing");xrtMemDebugFailClear();
    testRequire(xrtOwnershipScopeEnd(&freeze),"end blob freeze before physical releases");
    for(unsigned i=8;i--!=0;)adapters[i]->Drop(records[i].Reference.Data);
    xrtOwnershipSnapshotDestroy(snapshot);
    if(abortPlan)testRequire(xrtValueCount(array)==12,"restored blob fields remain intact");
    xrtValueRelease(array);balance(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"allocator ledger");xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<200;++i){transaction(false);transaction(true);opaque();}
    testRequire(drops==200 && finalizers==200 && traces==0,"exact unchanged foreign cleanup counts");balance(&before);
    puts("Value ownership adapters: 200 physical clears, 200 intact aborts, 400 pre-callback custom-policy refusals; weak quarantine/restore, allocation-free clear, zero live delta");
    for(unsigned i=0;i<200;++i)blobs(i%2!=0);
    balance(&before);
    puts("Blob ownership adapters: 200 actual container graphs, 1200 inline/empty/taken string and byte identities with 2400 real Clone owning slots, 100 clears and 100 intact aborts, allocation-free lifecycle and zero live delta");return 0;
}
