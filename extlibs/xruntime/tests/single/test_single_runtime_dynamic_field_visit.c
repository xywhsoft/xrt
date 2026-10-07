#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct VisitContext { xrtdynamicfields* fields; size_t count; int mode; } VisitContext;
static bool visit(xstrview name,const xvalue* value,ptr user)
{
    VisitContext* context=user;
    assert(name.Size==3 && !memcmp(name.Data,"a\0b",3));
    int64 number=0; assert(xrtValueGetInt(value,&number) && number==42); ++context->count;
    if (context->mode==1) {
        assert(!xrtDynamicFieldsRemove(context->fields,name));
        assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE); return false;
    }
    if (context->mode==2) return false;
    if (context->mode==3) { xrtSetErrorKind(XERR_IO); return true; }
    if (context->mode==4) {
        assert(!xrtDynamicFieldsVisitV1(context->fields,visit,context));
        assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE); return false;
    }
    if (context->mode==5) {
        assert(!xrtDynamicFieldsGet(context->fields,name));
        assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE); return false;
    }
    if (context->mode==6) {
        xvalue* copy=xrtValueInt(number);
        if (!copy) return false;
        xrtValueRelease(copy);
    }
    return true;
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount && after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static size_t dropped;
static void dropFailure(ptr raw,ptr user)
{ assert(raw==(ptr)(uintptr_t)1 && !user); ++dropped; xrtSetErrorKind(XERR_TYPE); }
static const xvaluehandleops failureOps={NULL,dropFailure,NULL,NULL};
static bool retireCaller(xstrview key,const xvalue* value,ptr user)
{
    VisitContext* context=user; assert(key.Size==6 && xrtValueType(value)==XVALUE_HANDLE);
    xrtDynamicFieldsUnref(context->fields); /* visitor still owns its independent pin */
    if (context->mode) { xrtSetErrorKind(XERR_IO); return false; }
    return true;
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline);
    xrtdynamicfields* fields=xrtDynamicFieldsCreate(); assert(fields);
    assert(xrtDynamicFieldsSetRefNew(fields,XRT_STR_LITERAL("a\0b"),xrtValueInt(42)));
    VisitContext context={fields,0,0};
    xrtSetErrorKind(XERR_IO); const xerror* prior=xrtGetError(); assert(prior);
    assert(xrtDynamicFieldsVisitV1(fields,visit,&context) && context.count==1 && xrtGetError()==prior); xrtClearError();
    for (int mode=1;mode<=5;++mode) {
        context.mode=mode; assert(!xrtDynamicFieldsVisitV1(fields,visit,&context));
        error(mode==3 ? XERR_IO : XERR_STATE);
        const xvalue* original=xrtDynamicFieldsGet(fields,XRT_STR_LITERAL("a\0b")); int64 number;
        assert(original && xrtValueGetInt(original,&number) && number==42);
    }
    assert(!xrtDynamicFieldsVisitV1(fields,NULL,NULL)); error(XERR_ARGUMENT);
    assert(!xrtDynamicFieldsVisitV1(NULL,visit,&context)); error(XERR_ARGUMENT);
    context.mode=0; assert(xrtMemDebugFailAfter(0));
    assert(xrtDynamicFieldsVisitV1(fields,visit,&context) && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    context.mode=6; xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); size_t point;
    for (point=0;point<256;++point) {
        assert(xrtMemDebugFailAfter(point)); bool ready=xrtDynamicFieldsVisitV1(fields,visit,&context);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) { assert(!ready); if (xrtGetError()) error(XERR_MEMORY); }
        else assert(ready);
        balance(&before); if (!hit) break;
    }
    assert(point<256); xrtDynamicFieldsUnref(fields); balance(&baseline);
    for (int failure=0;failure<2;++failure) {
        fields=xrtDynamicFieldsCreate(); ptr raw=(ptr)(uintptr_t)1;
        xvalue* handle=xrtValueHandleTake(&raw,&failureOps,NULL); assert(fields && handle && !raw);
        assert(xrtDynamicFieldsSetRefNew(fields,XRT_STR_LITERAL("handle"),handle));
        context.fields=fields; context.mode=failure;
        assert(!xrtDynamicFieldsVisitV1(fields,retireCaller,&context));
        error(failure ? XERR_IO : XERR_TYPE); assert(dropped==(size_t)failure+1); balance(&baseline);
    }
    printf("dynamic-field visit: protected keys/edges; %zu complete fault positions balanced\n",point); return 0;
}
