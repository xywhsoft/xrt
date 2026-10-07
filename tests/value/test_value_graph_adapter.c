#include "../test.h"

typedef struct graphadapterstate {
    xvalue* Root;
    xvalue* Handle;
    xvalue* Child;
    int Mode;
    int Calls;
    int Clones;
    int Drops;
    int TransientClones;
    int TransientDrops;
} graphadapterstate;
static bool cloneOpaque(ptr raw,ptr* copy,ptr user)
{ graphadapterstate* state=user; ++state->Clones; *copy=raw; return true; }
static void dropOpaque(ptr raw,ptr user)
{ (void)raw; ++((graphadapterstate*)user)->Drops; }
static void dropFailure(ptr raw,ptr user)
{ (void)raw; (void)user; xrtSetErrorKind(XERR_TYPE); }
static const xvaluehandleops opaque={cloneOpaque,dropOpaque,NULL,NULL};
static const xvaluehandleops failing={NULL,dropFailure,NULL,NULL};
static bool cloneTransient(ptr raw,ptr* copy,ptr user)
{ ++((graphadapterstate*)user)->TransientClones; *copy=raw; return true; }
static void dropTransient(ptr raw,ptr user)
{ (void)raw; ++((graphadapterstate*)user)->TransientDrops; }
static const xvaluehandleops transient={cloneTransient,dropTransient,NULL,NULL};
static void guarded(xvalue* value)
{
    testRequire(xrtValueRetain(value)==NULL && xrtErrorKind(xrtGetError())==XERR_STATE,
        "adapter did not guard source shell"); xrtClearError();
}
static int adapt(const xvaluehandleops* ops,ptr raw,ptr handleUser,uint64 identity,
    xvaluegraphnextv1* next,xvalue** target,ptr user)
{
    graphadapterstate* state=user; (void)raw; (void)identity;
    if (ops!=&opaque || handleUser!=state) return 0;
    ++state->Calls; guarded(state->Root); guarded(state->Handle);
    if (state->Mode==6) {
        *target=xrtValueArray();
        if (!*target) return -1;
        for (uintptr_t at=1;at<=70;++at) {
            ptr data=(ptr)at;
            xvalue* source=xrtValueHandleTake(&data,&transient,state);
            xvalue* copy=source ? xrtValueGraphNextV1(next,source) : NULL;
            xrtValueRelease(source);
            if (!copy) return -1;
            data=NULL;
            testRequire(xrtValueGetHandle(copy,&data,NULL,NULL) && data==(ptr)at,
                "transient source address reused an earlier memo identity");
            if (!xrtValueArrayAppendTake(*target,&copy)) { xrtValueRelease(copy); return -1; }
        }
        return 1;
    }
    if (state->Mode==3) {
        ptr pointer=state; *target=xrtValueHandleTake(&pointer,&failing,NULL);
        testRequire(*target!=NULL,"failure output fixture"); xrtSetErrorKind(XERR_IO); return -1;
    }
    if (state->Mode==4) return 1; /* success without an owned result */
    if (state->Mode==5) return 7; /* invalid status */
    xvalue* child=xrtValueGraphNextV1(next,state->Child);
    if (!child) return -1;
    guarded(state->Root); guarded(state->Handle);
    if (state->Mode==1 || state->Mode==2) {
        /* Discarded results remain memo-owned even when we decline. The
         * sibling occurrence of Child later reuses that completed copy. */
        xrtValueRelease(child);
        if (state->Mode==1) return 0;
        *target=xrtValueInt(7); return *target ? 1 : -1;
    }
    *target=xrtValueArray();
    if (!*target) { xrtValueRelease(child); return -1; }
    bool ok=xrtValueArrayAppend(*target,child) && xrtValueArrayAppend(*target,child);
    xrtValueRelease(child); return ok ? 1 : -1;
}
static void requireError(xerrkind kind)
{
    testRequire(xrtGetError()!=NULL && xrtErrorKind(xrtGetError())==kind,
        "graph adapter lost its first failure"); xrtClearError();
}
int main(void)
{
    graphadapterstate state={0};
    state.Child=xrtValueArray(); state.Root=xrtValueArray();
    testRequire(state.Child && state.Root && xrtValueArrayAppendNew(state.Child,xrtValueInt(1)),"fixture");
    ptr pointer=&state; state.Handle=xrtValueHandleTake(&pointer,&opaque,&state);
    testRequire(state.Handle && !pointer && xrtValueArrayAppend(state.Root,state.Handle) &&
        xrtValueArrayAppend(state.Root,state.Child),"root fixture");
    xvaluegraphcopyv1 config={.Size=sizeof(config),.CopyHandle=adapt,.UserData=&state};
    xvalue* result=xrtValueGraphCopyV1(state.Root,&config); testRequire(result!=NULL,"mixed copy");
    xvalue* output=xrtValueArrayAt(result,0),*child=xrtValueArrayAt(result,1);
    testRequire(child!=state.Child && xrtValueArrayAt(output,0)==child && xrtValueArrayAt(output,1)==child,
        "continuation did not share the outer graph memo"); xrtValueRelease(result);
    for (state.Mode=1;state.Mode<=2;++state.Mode) {
        result=xrtValueGraphCopyV1(state.Root,&config);
        testRequire(result && xrtValueArrayAt(result,1)!=state.Child,"discarded memo result was lost");
        xrtValueRelease(result);
    }
    state.Mode=3; testRequire(!xrtValueGraphCopyV1(state.Root,&config),"failed adapter accepted"); requireError(XERR_IO);
    state.Mode=4; testRequire(!xrtValueGraphCopyV1(state.Root,&config),"empty successful output accepted"); requireError(XERR_STATE);
    state.Mode=5; testRequire(!xrtValueGraphCopyV1(state.Root,&config),"invalid adapter status accepted"); requireError(XERR_STATE);
    config.Flags=2; testRequire(!xrtValueGraphCopyV1(state.Root,&config),"unknown graph flags accepted"); requireError(XERR_ARGUMENT);
    config.Flags=0; --config.Size; testRequire(!xrtValueGraphCopyV1(state.Root,&config),"wrong config size accepted"); requireError(XERR_ARGUMENT);
    config.Size=sizeof(config); config.CopyHandle=NULL; config.Flags=XVALUE_GRAPH_COPY_DATA_V1;
    int clones=state.Clones; result=xrtValueGraphCopyV1(state.Root,&config);
    testRequire(result && xrtValueArrayAt(result,0)==state.Handle && state.Clones==clones,
        "data snapshot cloned an opaque resource"); xrtValueRelease(result);
    result=xrtValueGraphCopyV1(state.Handle,&config);
    testRequire(result==state.Handle && state.Clones==clones,"opaque root data snapshot changed the handle");
    xrtValueRelease(result);
    config.CopyHandle=adapt; config.Flags=0; state.Mode=0;
    state.Mode=6; result=xrtValueGraphCopyV1(state.Root,&config);
    testRequire(result && xrtValueCount(xrtValueArrayAt(result,0))==70,"transient source graph");
    xrtValueRelease(result);
    testRequire(state.TransientClones==70 && state.TransientDrops==140,
        "transient source/target memo pins unbalanced");
    state.Mode=0;
    xvalue* saved=state.Child; state.Child=state.Handle;
    testRequire(!xrtValueGraphCopyV1(state.Root,&config),"adapter continuation accepted its active source");
    requireError(XERR_VALUE); state.Child=saved;
    xrtValueRelease(state.Root); xrtValueRelease(state.Handle); xrtValueRelease(state.Child);
    testRequire(state.Drops==state.Clones+1 && !xrtGetError(),"adapter resources unbalanced");
    puts("[PASS] value graph adapter and shared memo"); return 0;
}
