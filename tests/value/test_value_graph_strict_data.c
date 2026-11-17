#ifdef XRT_STRICT_DATA_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct strictstate { int clones, drops, calls; xvalue* child; } strictstate;
static bool cloneOpaque(ptr raw, ptr* copy, ptr user)
{ ++((strictstate*)user)->clones; *copy=raw; return true; }
static void dropOpaque(ptr raw, ptr user)
{ (void)raw; ++((strictstate*)user)->drops; }
static const xvaluehandleops opaque={cloneOpaque,dropOpaque,NULL,NULL};
static int project(const xvaluehandleops* ops,ptr raw,ptr handleUser,uint64 identity,
    xvaluegraphnextv1* next,xvalue** output,ptr user)
{
    strictstate* state=user; (void)raw; (void)identity;
    if(ops!=&opaque || handleUser!=state)return 0;
    ++state->calls;
    *output=xrtValueGraphNextV1(next,state->child);
    return *output ? 1 : -1;
}
static void error(xerrkind kind)
{ testRequire(xrtErrorKind(xrtGetError())==kind,"strict data error kind"); xrtClearError(); }
#if defined(XRT_FEATURE_MEMORY_DEBUG)
static xmemdebugsnapshot live(void)
{ xmemdebugsnapshot result; xrtMemDebugSnapshot(&result); return result; }
static void balanced(xmemdebugsnapshot before)
{ xmemdebugsnapshot after=live(); testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,"strict data leaked partial graph"); }
#endif
int main(void)
{
    strictstate state={0}; ptr raw=&state;
    xvalue* handle=xrtValueHandleTake(&raw,&opaque,&state);
    xvalue* pointer=xrtValuePointer(&state);
    xvalue* root=xrtValueArray();
    xvalue* child=xrtValueObject();
    testRequire(handle && pointer && root && child,"strict data fixture");
    testRequire(xrtValueObjectSetNew(child,XRT_STR_LITERAL("n"),xrtValueInt(7)) &&
        xrtValueArrayAppend(root,child) && xrtValueArrayAppend(root,child),"strict data shared input");
    xvaluegraphcopyv1 config={.Size=sizeof(config),
        .Flags=XVALUE_GRAPH_COPY_DATA_V1|XVALUE_GRAPH_COPY_STRICT_DATA_V1};
    xvalue* copy=xrtValueGraphCopyV1(root,&config);
    testRequire(copy && copy!=root && xrtValueArrayGet(copy,0)!=child &&
        xrtValueArrayGet(copy,0)==xrtValueArrayGet(copy,1),"strict data isolation and DAG sharing");
    testRequire(xrtValueObjectSetNew(child,XRT_STR_LITERAL("n"),xrtValueInt(9)),"mutate input");
    int64 number=0;
    testRequire(xrtValueGetInt(xrtValueObjectGet(xrtValueArrayGet(copy,0),XRT_STR_LITERAL("n")),&number) && number==7,"snapshot changed with source");
    xrtValueRelease(copy);
    testRequire(!xrtValueGraphCopyV1(pointer,&config),"strict pointer accepted"); error(XERR_UNSUPPORTED);
    testRequire(!xrtValueGraphCopyV1(handle,&config),"strict opaque handle accepted"); error(XERR_UNSUPPORTED);
    testRequire(!state.clones && !state.drops,"strict refusal invoked user lifecycle");
    testRequire(xrtValueArrayAppend(root,handle),"append resource fixture");
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    xmemdebugsnapshot before=live();
#endif
    for(int i=0;i<100;++i) {
        testRequire(!xrtValueGraphCopyV1(root,&config),"nested opaque handle accepted"); error(XERR_UNSUPPORTED);
#if defined(XRT_FEATURE_MEMORY_DEBUG)
        balanced(before);
#endif
    }
    config.CopyHandle=project; config.UserData=&state; state.child=child;
    copy=xrtValueGraphCopyV1(root,&config);
    testRequire(copy && state.calls==1 && xrtValueArrayGet(copy,2)==xrtValueArrayGet(copy,0),"trusted adapter lost shared memo");
    xrtValueRelease(copy);
    state.child=pointer;
    testRequire(!xrtValueGraphCopyV1(root,&config),"adapter Next bypassed strict pointers"); error(XERR_UNSUPPORTED);
    state.child=child;
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    size_t failures=0;
    for(size_t point=0;point<4097;++point) {
        testRequire(xrtMemDebugFailAfter(point),"arm strict data OOM");
        copy=xrtValueGraphCopyV1(root,&config);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) { ++failures; testRequire(!copy,"strict OOM published partial result"); error(XERR_MEMORY); }
        else testRequire(copy && xrtValueArrayGet(copy,0)==xrtValueArrayGet(copy,2),"strict OOM retry output");
        xrtValueRelease(copy); balanced(before);
        if(!hit)break;
    }
    testRequire(failures && failures<4097,"strict OOM sweep incomplete");
    printf("[PASS] strict data complete OOM prefix: %zu\n",failures);
#endif
    config.Flags=XVALUE_GRAPH_COPY_STRICT_DATA_V1;
    testRequire(!xrtValueGraphCopyV1(root,&config),"strict flag without DATA accepted"); error(XERR_ARGUMENT);
    config.Flags=4;
    testRequire(!xrtValueGraphCopyV1(root,&config),"unknown flag accepted"); error(XERR_ARGUMENT);
    config.CopyHandle=NULL; config.Flags=XVALUE_GRAPH_COPY_DATA_V1;
    copy=xrtValueGraphCopyV1(root,&config);
    testRequire(copy && xrtValueArrayGet(copy,2)==handle && !state.clones,"legacy DATA handle fallback changed");
    xrtValueRelease(copy);
    copy=xrtValueGraphCopyV1(pointer,&config);
    testRequire(copy==pointer,"legacy DATA pointer fallback changed"); xrtValueRelease(copy);
    xrtValueRelease(root); xrtValueRelease(child); xrtValueRelease(pointer); xrtValueRelease(handle);
    testRequire(state.drops==1 && !state.clones,"source ownership changed");
    printf("[PASS] strict data projections and legacy compatibility\n");
    return 0;
}
