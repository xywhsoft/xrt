#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef OWNERSHIP_SNAPSHOT_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct Admission { const void* Denied; unsigned Calls; } Admission;
static bool admit(xrtownershipref ref, ptr data)
{ Admission* a = data; ++a->Calls; return ref.Data != a->Denied; }
static unsigned opaqueCounts, opaqueTraces;
static bool opaqueCount(const void* data, size_t* count)
{ (void)data; ++opaqueCounts; *count=1; return true; }
static bool opaqueTrace(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; ++opaqueTraces; return true; }
static const xrtownershipops opaqueOps={opaqueCount,opaqueTrace};
static void balance(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
	testRequire(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
		after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount, "snapshot exact memory balance");
}
static void physical(unsigned mode)
{
	xvalue *array=xrtValueArray(), *scalar=xrtValueInt(42), *alias=NULL;
	xrtownershipsnapshot* snapshot=NULL; xrtownershipnode node={0};
	Admission admission={0}; size_t scalarNodes=0, arrayNodes=0;
	testRequire(array && scalar && xrtValueArrayAppendNew(array,xrtValueRetain(scalar)) && xrtValueArrayAppendNew(array,xrtValueRetain(scalar)), "two actual array edges");
	if(mode)alias=xrtValueClone(array);
	xrtownershipref anchors[3]={xrtValueOwnership(array),xrtValueOwnership(array),xrtValueOwnership(alias)};
	xrtownershipref retired=xrtValueOwnership(array); xrtownershipscope freeze={0};
	testRequire(xrtOwnershipFreezeTryBegin(&freeze), "freeze source graph");
	testRequire(xrtOwnershipSnapshotCreate(anchors,3,&retired,1,admit,&admission,&snapshot), "create physical snapshot");
	testRequire(xrtOwnershipSnapshotNodeCount(snapshot)==(mode?4u:3u) && admission.Calls==(mode?4u:3u), "unique identities and admission");
	for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){
		testRequire(xrtOwnershipSnapshotNode(snapshot,i,&node), "read record");
		if(node.Reference.Data==scalar){++scalarNodes;testRequire(node.StrongCount==3 && node.InternalCount==2 && node.Reachable, "native scalar alias remains real root");}
		if(node.Reference.Data==array){++arrayNodes;testRequire(node.StrongCount==1 && node.InternalCount==1 && !node.Reachable, "real retiring owner slot subtracted once");}
	}
	testRequire(scalarNodes==1 && arrayNodes==1, "distinct physical shells");
	{ xrtownershipnode saved=node; testRequire(!xrtOwnershipSnapshotNode(snapshot,99,&node) && !memcmp(&saved,&node,sizeof(node)), "invalid read leaves record unchanged"); xrtClearError(); }
	testRequire(xrtMemDebugFailAfter(0) && xrtOwnershipSnapshotNode(snapshot,0,&node) && !xrtMemDebugFailTriggered(), "snapshot read does not allocate or retrace");xrtMemDebugFailClear();
	xrtOwnershipSnapshotDestroy(snapshot);testRequire(xrtOwnershipScopeEnd(&freeze), "unfreeze");
	xrtValueRelease(alias);xrtValueRelease(array);xrtValueRelease(scalar);
}
static void refusal(void)
{
	int opaque=0; xrtownershipref anchor={&opaque,&opaqueOps};
	xrtownershipsnapshot* sentinel=(xrtownershipsnapshot*)&opaque; Admission admission={&opaque,0};
	unsigned counts=opaqueCounts,traces=opaqueTraces;
	testRequire(!xrtOwnershipSnapshotCreate(&anchor,1,NULL,0,admit,&admission,&sentinel) && sentinel==(xrtownershipsnapshot*)&opaque, "denial preserves output");
	testRequire(admission.Calls==1 && counts==opaqueCounts && traces==opaqueTraces, "deny before any adapter callback");xrtClearError();
	testRequire(!xrtOwnershipSnapshotCreate(&anchor,1,NULL,1,NULL,NULL,&sentinel) && sentinel==(xrtownershipsnapshot*)&opaque, "invalid slots atomic");xrtClearError();
}
static unsigned failures(void)
{
	unsigned failures=0;
	for(unsigned round=0;round<50;++round){
		xvalue* value=xrtValueInt(9); testRequire(value!=NULL,"fault source");
		xrtownershipref anchor=xrtValueOwnership(value); xrtownershipscope freeze={0};
		testRequire(xrtOwnershipFreezeTryBegin(&freeze),"fault freeze");
		for(unsigned budget=0;budget<64;++budget){
			xmemdebugsnapshot before; xrtownershipsnapshot* snapshot=NULL; xrtMemDebugSnapshot(&before);
			testRequire(xrtMemDebugFailAfter(budget),"arm fault");
			bool ok=xrtOwnershipSnapshotCreate(&anchor,1,NULL,0,NULL,NULL,&snapshot),failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
			if(!ok){++failures;testRequire(failed && !snapshot && xrtGetError(),"real failure leaves no snapshot");}
			else testRequire(!failed && xrtOwnershipSnapshotNodeCount(snapshot)==1,"complete success");
			xrtOwnershipSnapshotDestroy(snapshot);balance(&before);if(ok)break;testRequire(budget!=63,"finite allocation sweep");
		}
		testRequire(xrtOwnershipScopeEnd(&freeze),"fault unfreeze");xrtValueRelease(value);
	}
	return failures;
}
int main(void)
{
	xmemdebugsnapshot before;unsigned faults;
	testRequire(xrtMemDebugEnable(true),"debug enable");xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
	for(unsigned n=0;n<100;++n){physical(0);physical(1);refusal();}
	faults=failures();balance(&before);testRequire(faults>0,"faults covered");
	printf("Ownership snapshots: 200 physical alias graphs, 100 pre-callback admission refusals, %u real allocation failures; borrowed identities, exact counts, atomic outputs, zero live delta; not collection\n",faults);
	return 0;
}
