/* Borrowed backing discovery is collector support, not a GC/unload action. */
#define XRT_MODULE_VALUE_CONTAINER
#include <xrt.h>
#include <stdio.h>

static unsigned released;
static bool empty_trace(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; return true; }
static void release_context(ptr data) { (void)data; ++released; }
static void finalize(xvalue* object, ptr data) { (void)object; (void)data; }
static bool checked(xvalue* object, ptr data) { finalize(object,data); return true; }
static const xvalueobjectownershipv1 policy = {
    sizeof(policy),finalize,empty_trace,release_context,empty_trace,release_context,checked
};
typedef struct Observed { size_t backings, references; const void* marker; } Observed;
static bool observe(xrtownershipref ref, const void* lifetime, ptr data)
{
    Observed* result=data; size_t refs=0;
    if (lifetime != result->marker) return true;
    /* Exact admission still precedes Count. The visitor adds NO references. */
    if (!xrtValueObjectOwnershipAdapterV1(ref,&policy) || !ref.Ops->Count(ref.Data,&refs)) return false;
    ++result->backings; result->references += refs; return true;
}
int main(void)
{
    unsigned marker=42; xvalue* object=xrtValueObject(); xrtownershipscope freeze={0};
    if (!object || !xrtValueObjectLifetimeBindOwned(object,&marker,empty_trace,release_context) ||
        !xrtValueObjectConstructionPrepare(object) || !xrtValueObjectOwnershipBindV1(object,&policy) ||
        !xrtValueObjectConstructionCommit(object)) { xrtValueRelease(object); return 1; }
    Observed result={.marker=&marker};
    if (!xrtOwnershipFreezeTryBegin(&freeze)) { xrtValueRelease(object); return 1; }
    bool ok=xrtValueObjectOwnershipDiscoverV1(&policy,observe,&result);
    if (!xrtOwnershipScopeEnd(&freeze)) return 1;
    printf("borrowed backings: %zu; live owning refs: %zu\n",result.backings,result.references);
    xrtValueRelease(object);
    printf("context releases: %u\n",released);
    return ok && result.backings == 1 && result.references == 1 && released == 1 ? 0 : 1;
}
