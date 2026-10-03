#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef OBJECT_DISCOVERY_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct Lifetime { unsigned releases, finalizers; size_t expectedFields; } Lifetime;
/* An actual uniquely owned native field bridges the class reference cycle;
 * ordinary XRT Value containers deliberately reject direct tree cycles. */
typedef struct Link { xvalue* value; } Link;
static void link_drop(ptr data, ptr unused)
{ (void)unused; Link* link=data; xrtValueRelease(link->value); xrtFree(link); }
static const xvaluehandleops link_ops = {.Drop=link_drop};
static bool link_trace(const xvalue* value, xrtownershipvisitor visit, ptr context)
{
    ptr data=NULL; const xvaluehandleops* ops=NULL;
    return xrtValueGetHandle(value,&data,&ops,NULL) && ops==&link_ops && data &&
        visit(xrtValueOwnership(((const Link*)data)->value),context);
}
static unsigned visits, copiedBackings, reclaimed;
static bool trace_empty(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; return true; }
static void release_lifetime(ptr data)
{ if (data) ++((Lifetime*)data)->releases; }
static void release_finalizer(ptr data) { (void)data; }
static void finalize(xvalue* object, ptr data)
{
    Lifetime* context=data; xrtownershipscope freeze={0};
    testRequire(context && !context->finalizers && !context->releases &&
        xrtValueCount(object) == context->expectedFields,"exactly once semantic finalizer sees intact fields");
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze),"finalizer outside freeze");
    ++context->finalizers;
}
static bool finalize_checked(xvalue* object, ptr data)
{ finalize(object,data); return true; }
static const xvalueobjectownershipv1 family = {
    sizeof(family),finalize,trace_empty,release_finalizer,trace_empty,release_lifetime,finalize_checked
};
static const xvalueobjectownershipv1 other = {
    sizeof(other),finalize,trace_empty,release_finalizer,trace_empty,release_lifetime,finalize_checked
};
static xvalue* create_prepared(const xvalueobjectownershipv1* policy, Lifetime* context, bool finalized)
{
    xrtownershipscope mutation = {0};
    testRequire(xrtOwnershipMutationBegin(&mutation),"creation mutation");
    xvalue* result = xrtValueObjectLifo();
    if (result && (!xrtValueObjectLifetimeBindOwned(result,context,trace_empty,release_lifetime) ||
        !(finalized ? xrtValueObjectFinalizerPrepareOwned(result,finalize,context,trace_empty,release_finalizer)
                    : xrtValueObjectConstructionPrepare(result)) || !xrtValueObjectOwnershipBindV1(result,policy) ||
        !xrtValueObjectConstructionCommit(result))) {
        xerror* error = xrtTakeError(); xrtValueRelease(result); result = NULL;
        xrtClearError(); xrtSetErrorTake(error);
    }
    testRequire(xrtOwnershipScopeEnd(&mutation),"creation end"); return result;
}
static xvalue* create(const xvalueobjectownershipv1* policy, Lifetime* context)
{ return create_prepared(policy,context,false); }
typedef struct Anchors { xrtownershipref refs[64]; const void* contexts[64]; size_t count; bool stop; } Anchors;
static bool discover(xrtownershipref ref, const void* data, ptr context)
{
    Anchors* anchors = context;
    testRequire(ref.Data && ref.Ops && anchors->count < 64,"borrowed actual backing");
    for (size_t i=0; i<anchors->count; ++i)
        testRequire(anchors->refs[i].Data != ref.Data,"one anchor per physical backing");
    anchors->refs[anchors->count] = ref; anchors->contexts[anchors->count++] = data;
    ++visits; return !anchors->stop;
}
static Anchors enumerate(const xvalueobjectownershipv1* policy)
{
    xrtownershipscope freeze = {0}; Anchors anchors = {0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"exclusive discovery");
    testRequire(xrtMemDebugFailAfter(0),"allocation-free enumeration");
    testRequire(xrtValueObjectOwnershipDiscoverV1(policy,discover,&anchors) &&
        !xrtMemDebugFailTriggered(),"no allocation, retain or producer callback during discovery");
    xrtMemDebugFailClear(); testRequire(xrtOwnershipScopeEnd(&freeze),"discovery end"); return anchors;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount,
        "exact allocator balance");
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref)
{
    const xrtownershipadapterv1* adapter = xrtValueObjectOwnershipAdapterV1(ref,&family);
    if (!adapter) adapter = xrtValueObjectOwnershipAdapterV1(ref,&other);
    if (!adapter) adapter = xrtValueHandleOwnershipAdapterV1(ref,&link_ops,link_trace);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static bool admit(xrtownershipref ref, ptr context)
{ (void)context; return resolve(ref) != NULL; }
static void copies(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Lifetime context = {0}, separate = {0};
    xvalue* ordinary = xrtValueObject(); xvalue* first = create(&family,&context);
    xvalue* foreign = create(&other,&separate); testRequire(ordinary && first && foreign,"independent policies");
    Anchors anchors = enumerate(&family); testRequire(anchors.count == 1 && anchors.contexts[0] == &context,"exact policy identity");
    testRequire(enumerate(&other).count == 1,"identical callbacks do not conflate policy identities");
    xvalue* shallow = xrtValueClone(first); testRequire(shallow,"COW shell clone");
    testRequire(enumerate(&family).count == 1,"shared shell has no new backing");
    testRequire(xrtValueObjectSetNew(shallow,XRT_STR_LITERAL("n"),xrtValueInt(42)),"COW detach");
    testRequire(enumerate(&family).count == 2,"COW backing enrolled independently");
    xvalue* deep = xrtValueDeepClone(shallow); testRequire(deep,"deep clone");
    anchors = enumerate(&family); testRequire(anchors.count == 3,"deep backing enrolled independently");
    for (size_t i=0; i<anchors.count; ++i) testRequire(anchors.contexts[i] == &context,"shared actual lifetime context");
    copiedBackings += 2;
    xrtValueRelease(first); testRequire(enumerate(&family).count == 2 && !context.releases,"original shell ends before clones");
    xrtValueRelease(shallow); testRequire(enumerate(&family).count == 1 && !context.releases,"COW source ends before deep clone");
    xrtValueRelease(deep); testRequire(enumerate(&family).count == 0 && context.releases == 1,"last backing removes anchor and releases lifetime once");
    xrtValueRelease(foreign); xrtValueRelease(ordinary);
    testRequire(!context.finalizers && !separate.finalizers && separate.releases == 1,"ordinary classes add no synthetic finalizer");
    testRequire(enumerate(&other).count == 0,"separate policy cleanup"); balance(&before);
}
static void failed_copies(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Lifetime context = {0};
    xvalue* source = create(&family,&context); testRequire(source,"OOM source");
    testRequire(xrtValueObjectSetNew(source,XRT_STR_LITERAL("n"),xrtValueInt(42)),"OOM source field");
    bool terminal = false;
    for (size_t point=0; point<64; ++point) {
        xmemdebugsnapshot start; xrtMemDebugSnapshot(&start);
        testRequire(xrtMemDebugFailAfter(point),"clone OOM budget");
        xvalue* copy = xrtValueDeepClone(source); bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        testRequire(hit ? !copy && xrtErrorKind(xrtGetError()) == XERR_MEMORY : copy != NULL,"exact failed or successful clone");
        xrtClearError(); xrtValueRelease(copy);
        testRequire(enumerate(&family).count == 1 && !context.releases && xrtValueCount(source) == 1,
            "failed partial backing leaves no stale anchor, preserves source");
        balance(&start); if (!hit) { terminal = true; break; }
    }
    testRequire(terminal,"complete actual clone OOM prefix");
    xrtValueRelease(source); testRequire(context.releases == 1 && !enumerate(&family).count,"OOM source tail"); balance(&before);
}
static void failed_creation(void)
{
    bool terminal=false;
    for (size_t point=0; point<64; ++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Lifetime context={0};
        testRequire(xrtMemDebugFailAfter(point),"creation OOM budget");
        xvalue* object=create(&family,&context); bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        testRequire(hit ? !object && xrtErrorKind(xrtGetError()) == XERR_MEMORY : object != NULL,"factory OOM primary diagnostic");
        xrtClearError(); xrtValueRelease(object);
        testRequire(context.releases <= 1 && !context.finalizers && !enumerate(&family).count,"unpublished rollback removes anchors");
        balance(&before); if (!hit) { terminal=true; break; }
    }
    testRequire(terminal,"complete actual creation OOM prefix");
}
static void cycle(bool abort_collection)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Lifetime context = {0};
    xvalue* object = create_prepared(&family,&context,true); testRequire(object,"cycle object");
    xrtownershipscope mutation = {0}; testRequire(xrtOwnershipMutationBegin(&mutation),"native edge mutation");
    Link* link=xrtMalloc(sizeof(*link)); testRequire(link,"real native field"); link->value=xrtValueRetain(object);
    ptr owned=link; xvalue* field=xrtValueHandleTake(&owned,&link_ops,NULL);
    testRequire(field && !owned && xrtValueHandleOwnershipBindPhased(field,link_trace) &&
        xrtValueObjectSetNew(object,XRT_STR_LITERAL("self"),field),"actual native owning slot");
    testRequire(xrtOwnershipScopeEnd(&mutation),"native edge end");
    xvalue* alias = abort_collection ? xrtValueRetain(object) : NULL;
    xrtValueRelease(object); object = NULL;
    Anchors anchors = {0}; xrtownershipscope freeze = {0}; xrtownershipsnapshot* snapshot = NULL;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze) && xrtValueObjectOwnershipDiscoverV1(&family,discover,&anchors),"discover detached cycle under freeze");
    testRequire(anchors.count == 1 && xrtOwnershipSnapshotCreate(anchors.refs,anchors.count,NULL,0,admit,NULL,&snapshot),
        "anchors are NOT subtracted as internal owning slots");
    testRequire(xrtOwnershipSnapshotNodeCount(snapshot) == 4,"actual backing, lifetime, native field and self shell graph");
    xrtownershipnode nodes[4]; const xrtownershipadapterv1* adapters[4]; unsigned token = 0;
    for (size_t i=0; i<4; ++i) {
        testRequire(xrtOwnershipSnapshotNode(snapshot,i,&nodes[i]) && nodes[i].Reachable == abort_collection,
            "real external alias roots; borrowed anchor never does");
        adapters[i] = resolve(nodes[i].Reference);
        testRequire(adapters[i] && adapters[i]->Hold(nodes[i].Reference.Data) &&
            adapters[i]->Claim(nodes[i].Reference.Data,&token),"real pins and quarantine");
    }
    if (abort_collection) {
        for (size_t i=0; i<4; ++i) adapters[i]->Restore(nodes[i].Reference.Data,&token);
    } else {
        testRequire(xrtOwnershipScopeEnd(&freeze),"leave freeze for semantic duties");
        context.expectedFields=1;
        for (size_t i=0; i<4; ++i) if (adapters[i]->Finalize)
            testRequire(adapters[i]->Finalize(nodes[i].Reference.Data,&token),"semantic finalizer on REAL pinned shell");
        testRequire(context.finalizers == 1 && xrtOwnershipFreezeTryBegin(&freeze),"reenter clear only after finalizers");
        testRequire(xrtMemDebugFailAfter(0),"infallible clear");
        for (size_t i=0; i<4; ++i) adapters[i]->Clear(nodes[i].Reference.Data,&token);
        testRequire(!xrtMemDebugFailTriggered(),"clear no allocation"); xrtMemDebugFailClear();
        Anchors empty = {0};
        testRequire(xrtValueObjectOwnershipDiscoverV1(&family,discover,&empty) && !empty.count,"Clear removes before lifetime retirement");
    }
    testRequire(xrtOwnershipScopeEnd(&freeze),"callbacks outside freeze");
    if (!abort_collection) for (size_t i=0; i<4; ++i)
        if (adapters[i]->Finish) testRequire(adapters[i]->Finish(nodes[i].Reference.Data,&token),"finish");
    for (size_t i=4; i--!=0;) adapters[i]->Drop(nodes[i].Reference.Data);
    xrtOwnershipSnapshotDestroy(snapshot);
    if (abort_collection) {
        testRequire(alias && xrtValueCount(alias) == 1 && !context.releases && enumerate(&family).count == 1,"abort preserves fields and enrollment");
        /* Detach before the custom native Drop releases this object alias;
         * never request a release of the receiver during its BUSY mutation. */
        xvalue* detached=xrtValueObjectTake(alias,XRT_STR_LITERAL("self"));
        testRequire(detached,"detach caller-owned native field"); xrtValueRelease(detached);
        context.expectedFields=0;
        xrtValueRelease(alias);
    }
    Anchors remaining=enumerate(&family);
    if (context.releases != 1 || remaining.count)
        fprintf(stderr,"cycle abort=%d releases=%u remaining=%zu error=%d\n",abort_collection,context.releases,remaining.count,xrtErrorKind(xrtGetError()));
    testRequire(context.releases == 1 && context.finalizers == 1 && !remaining.count,"cycle tails leave no stale discovery node"); ++reclaimed; balance(&before);
}
static xatomic32 running = {0};
static int32 worker(ptr data)
{
    size_t rounds = (size_t)(uintptr_t)data;
    for (size_t round=0; round<rounds; ++round) {
        Lifetime context = {0}; xvalue* object = create(&family,&context);
        testRequire(object,"worker object"); xvalue* copy = xrtValueDeepClone(object); testRequire(copy,"worker clone");
        xrtValueRelease(object); xrtValueRelease(copy); testRequire(context.releases == 1,"worker lifetime ends exactly once");
    }
    (void)xrtAtomic32FetchSub(&running,1,XMEMORY_RELEASE); return 0;
}
static void concurrent(void)
{
    /* Warm thread/runtime services before the measured allocation ledger. */
    xrtAtomic32Store(&running,1,XMEMORY_RELEASE); xthread* warm = xrtThreadCreate(worker,NULL,0);
    testRequire(warm && xrtThreadWaitFor(warm,5000000) == XWAIT_OK,"warm native thread"); xrtThreadDestroy(warm);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); xthread* threads[4];
    xrtAtomic32Store(&running,4,XMEMORY_RELEASE);
    for (unsigned i=0; i<4; ++i) { threads[i]=xrtThreadCreate(worker,(ptr)(uintptr_t)200,0); testRequire(threads[i],"native worker"); }
    unsigned frozen = 0;
    while (xrtAtomic32Load(&running,XMEMORY_ACQUIRE)) {
        xrtownershipscope freeze = {0}; Anchors anchors = {0};
        if (xrtOwnershipFreezeTryBegin(&freeze)) {
            testRequire(xrtValueObjectOwnershipDiscoverV1(&family,discover,&anchors),"concurrent frozen discovery");
            for (size_t i=0; i<anchors.count; ++i) { size_t count=0;
                testRequire(anchors.refs[i].Ops->Count(anchors.refs[i].Data,&count) && count > 0,"no freed/zero-count registry nodes");
            }
            testRequire(xrtOwnershipScopeEnd(&freeze),"concurrent freeze end"); ++frozen;
        }
        xrtThreadYield();
    }
    for (unsigned i=0; i<4; ++i) { testRequire(xrtThreadWaitFor(threads[i],5000000) == XWAIT_OK,"join actual workers"); xrtThreadDestroy(threads[i]); }
    testRequire(!enumerate(&family).count && frozen,"concurrent registry empty after joins"); balance(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"debug allocator");
    for (unsigned i=0; i<100; ++i) { copies(); cycle(false); cycle(true); }
    failed_copies(); failed_creation(); concurrent();
    Anchors stopped = {.stop=true}; Lifetime context = {0}; xvalue* object=create(&family,&context);
    xrtownershipscope freeze = {0}; testRequire(object && xrtOwnershipFreezeTryBegin(&freeze),"stop fixture");
    testRequire(!xrtValueObjectOwnershipDiscoverV1(&family,discover,&stopped) && stopped.count == 1 && !xrtGetError(),"visitor refusal has no fabricated error");
    testRequire(!xrtValueObjectOwnershipDiscoverV1(NULL,discover,&stopped),"invalid policy rejected"); xrtClearError();
    testRequire(xrtOwnershipScopeEnd(&freeze),"stop end"); xrtValueRelease(object);
    printf("Object discovery: %u copied backings, %u detached/abort graphs, 800 concurrent lifetimes, full clone OOM prefix; allocation-free borrowed anchors, exact policy and ledger; %u visits\n",copiedBackings,reclaimed,visits);
    testMemoryDebugDrain("discovery final memory drain"); return 0;
}
