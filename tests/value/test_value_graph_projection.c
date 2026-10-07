#include "../test.h"

#define SCHEMA_ID UINT64_C(0x91c105ed)
static xstrview view(const char* text) { return (xstrview){text,strlen(text)}; }
typedef struct projectionstate {
    xvalue* Root;
    xvaluegraphnextv1* Parent;
    int Mode;
    int Calls;
} projectionstate;

static void requireError(xerrkind kind)
{
    testRequire(xrtGetError() && xrtErrorKind(xrtGetError())==kind,"projection error contract");
    xrtClearError();
}
static void guarded(xvalue* value)
{
    xerror* prior=xrtTakeError();
    testRequire(!xrtValueRetain(value),"projection source was not guarded");
    requireError(XERR_STATE); xrtSetErrorTake(prior);
}
static void failingDrop(ptr raw,ptr user)
{ (void)raw; (void)user; xrtSetErrorKind(XERR_TYPE); }
static const xvaluehandleops failing={NULL,failingDrop,NULL,NULL};
static void noopDrop(ptr raw,ptr user) { (void)raw; (void)user; }
static const xvaluehandleops cycleOps={NULL,noopDrop,NULL,NULL};
static int projectCycle(const xvaluehandleops* ops,ptr raw,ptr handleUser,uint64 identity,
    xvaluegraphnextv1* next,xvalue** output,ptr user)
{
    (void)handleUser; (void)identity; (void)user;
    if (ops!=&cycleOps) return 0;
    testRequire(!xrtValueGraphObjectGetV1(next,view("value")),"non-object capability accepted");
    requireError(XERR_TYPE);
    *output=xrtValueGraphNextV1(next,raw); return *output ? 1 : -1;
}

/* A schema selects an edge BEFORE recursion: renamed public fields are
 * visited, missing fields omitted, and private/excluded cycles ignored. */
static int project(uint64 identity,xvaluegraphnextv1* next,xvalue** target,ptr user)
{
    projectionstate* state=user;
    if (identity!=SCHEMA_ID) return 0;
    ++state->Calls;
    guarded(state->Root);
    if (state->Mode==1) return 1;
    if (state->Mode==2) return 7;
    if (state->Mode==3) {
        ptr data=state; *target=xrtValueHandleTake(&data,&failing,NULL);
        testRequire(*target!=NULL,"failed projection output fixture");
        xrtSetErrorKind(XERR_IO); return -1;
    }
    if (state->Mode==4) { *target=xrtValueInt(99); return 0; }
    if (state->Parent) {
        xerror* prior=xrtTakeError();
        testRequire(!xrtValueGraphObjectGetV1(state->Parent,view("value")),
            "suspended outer capability was accepted"); requireError(XERR_STATE);
        xrtSetErrorTake(prior);
    }
    const xerror* before=xrtGetError();
    testRequire(!xrtValueGraphObjectGetV1(next,view("missing")) && xrtGetError()==before,
        "missing field changed the incoming error");
    xvalue* borrowed=xrtValueGraphObjectGetV1(next,view("value"));
    if (!borrowed) return -1;
    guarded(state->Root);
    xvaluegraphnextv1* parent=state->Parent; state->Parent=next;
    xvalue* copy=xrtValueGraphNextV1(next,borrowed); state->Parent=parent;
    if (!copy) return -1;
    guarded(state->Root);
    *target=xrtValueObject();
    bool ok=*target && xrtValueObjectSetTake(*target,view("renamed"),&copy);
    xrtValueRelease(copy);
    return ok ? 1 : -1;
}
static xvalue* objectWith(xvalue* child)
{
    xvalue* result=xrtValueObject();
    testRequire(result && xrtValueTypeIdBind(result,SCHEMA_ID) &&
        xrtValueObjectSet(result,view("value"),child),"schema object fixture");
    return result;
}
static xmemdebugsnapshot live(void)
{ xmemdebugsnapshot snapshot; xrtMemDebugSnapshot(&snapshot); return snapshot; }
static void balanced(xmemdebugsnapshot baseline)
{
    xmemdebugsnapshot current=live();
    testRequire(current.LiveCount==baseline.LiveCount && current.LiveBytes==baseline.LiveBytes,
        "projection source/target memo pins leaked");
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"projection logical allocation tracking");
    xmemdebugsnapshot initial=live();
    projectionstate state={0};
    xvaluegraphcopyv1 config={.Size=sizeof(config),.Flags=XVALUE_GRAPH_COPY_DATA_V1,
        .CopyHandle=projectCycle,.UserData=&state,.CopyObject=project};
    testRequire(!xrtValueGraphObjectGetV1(NULL,view("value")),"NULL capability accepted");
    requireError(XERR_STATE);
    xvalue* shared=xrtValueArray();
    testRequire(shared && xrtValueArrayAppendNew(shared,xrtValueString((xstrview){"a\0b",3})),"shared fixture");
    xvalue* a=objectWith(shared),*b=objectWith(a);
    ptr cycleData=a; xvalue* cycle=xrtValueHandleTake(&cycleData,&cycleOps,NULL);
    testRequire(cycle && xrtValueObjectSet(a,view("excluded"),cycle),"excluded cycle fixture");
    state.Root=xrtValueArray();
    testRequire(state.Root && xrtValueArrayAppend(state.Root,b) && xrtValueArrayAppend(state.Root,a) &&
        xrtValueArrayAppend(state.Root,shared),"mixed root fixture");
    xvalue* mixed=state.Root;
    xvalue* result=xrtValueGraphCopyV1(state.Root,&config);
    testRequire(result && state.Calls==2,"schema projection");
    xvalue* pa=xrtValueArrayAt(result,1),*pb=xrtValueArrayAt(result,0),*ps=xrtValueArrayAt(result,2);
    testRequire(xrtValueObjectGet(pb,view("renamed"))==pa &&
        xrtValueObjectGet(pa,view("renamed"))==ps && ps!=shared && xrtValueTypeId(pa)==0 &&
        !xrtValueObjectGet(pa,view("excluded")) && !xrtValueObjectGet(pa,view("value")),
        "projection lost memo aliases, selected fields or data-only identity");
    xrtValueRelease(result);
    xrtSetErrorKind(XERR_IO); const xerror* prior=xrtGetError();
    result=xrtValueGraphCopyV1(state.Root,&config);
    testRequire(result && xrtGetError()==prior,"projection changed the incoming error");
    xrtValueRelease(result); xrtClearError();
    xmemdebugsnapshot baseline=live();
    for (state.Mode=1;state.Mode<=4;++state.Mode) {
        testRequire(!xrtValueGraphCopyV1(state.Root,&config),"invalid object projection accepted");
        requireError(state.Mode==3 ? XERR_IO : XERR_STATE); balanced(baseline);
    }
    state.Mode=0;
    testRequire(xrtValueObjectSet(a,view("value"),cycle),"included cycle fixture");
    testRequire(!xrtValueGraphCopyV1(state.Root,&config),"included cycle accepted"); requireError(XERR_VALUE);
    testRequire(xrtValueObjectSet(a,view("value"),shared),"restore included edge");
    /* Resource adapters can reveal edges deeper than ordinary container
     * admission allows. Keep those borrowed nodes alive explicitly. */
    xvalue* chain[300]; xvalue* deep=shared;
    for (size_t at=0;at<300;++at) {
        ptr raw=deep; xvalue* edge=xrtValueHandleTake(&raw,&cycleOps,NULL);
        testRequire(edge!=NULL,"depth edge fixture");
        chain[at]=objectWith(edge); xrtValueRelease(edge); deep=chain[at];
    }
    state.Root=deep;
    testRequire(!xrtValueGraphCopyV1(deep,&config),"schema depth boundary accepted"); requireError(XERR_VALUE);
    for (size_t at=300;at!=0;--at) xrtValueRelease(chain[at-1]);
    /* A complete overflow-memo sweep, not a fixed number of sampled faults. */
    state.Root=xrtValueArray();
    testRequire(state.Root!=NULL,"wide fixture");
    for (size_t at=0;at<70;++at) {
        xvalue* item=objectWith(shared);
        testRequire(xrtValueArrayAppendTake(state.Root,&item),"wide schema insert");
    }
    baseline=live(); size_t failures=0,firstNoHit=0;
    for (size_t point=0;point<4097;++point) {
        testRequire(xrtMemDebugFailAfter(point),"arm projection logical allocation failure");
        result=xrtValueGraphCopyV1(state.Root,&config);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) {
            ++failures; testRequire(!result,"allocation failure published a partial projection");
            requireError(XERR_MEMORY);
        } else { testRequire(result!=NULL,"fault-free projection failed"); firstNoHit=point; }
        xrtValueRelease(result); balanced(baseline);
        if (!hit) break;
    }
    testRequire(failures && firstNoHit==failures,"incomplete projection OOM prefix");
    xrtValueRelease(state.Root);
    xrtValueRelease(mixed);
    testRequire(xrtValueObjectRemove(a,view("excluded")),"remove excluded cycle");
    xrtValueRelease(cycle);
    xrtValueRelease(b); xrtValueRelease(a); xrtValueRelease(shared);
    xrtClearError(); balanced(initial); testMemoryDebugDrain("projection retained logical allocations");
    printf("[PASS] schema projection; %zu complete allocation failure positions\n",failures);
    return 0;
}
