/* Real managed snapshot references, not a pin counter on a stack iterator. */
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct Node {xrtownershipref Ref;const xrtownershipadapterv1* Adapter;} Node;
typedef struct Tree {Node Nodes[16];size_t Count;} Tree;
static unsigned graphs,pins,clears,budgets,failures;
static void freeze(xrtownershipscope* scope)
{testRequire(xrtOwnershipFreezeTryBegin(scope),"exclusive cursor inspection");}
static void end(xrtownershipscope* scope)
{testRequire(xrtOwnershipScopeEnd(scope),"end cursor scope");}
static bool visit(xrtownershipref ref,ptr data)
{
	Tree* tree=data;if(!ref.Data)return true;
	for(size_t i=0;i<tree->Count;++i)if(tree->Nodes[i].Ref.Data==ref.Data&&tree->Nodes[i].Ref.Ops==ref.Ops)return true;
	testRequire(tree->Count<16,"bounded actual graph");
	const xrtownershipadapterv1* adapter=xrtValueCursorOwnershipAdapterV1(ref);
	if(!adapter)adapter=xrtValueOwnershipAdapterV1(ref);
	testRequire(adapter!=NULL,"every physical child independently admitted");
	tree->Nodes[tree->Count++]=(Node){ref,adapter};return true;
}
static void retire(xvaluecursor* cursor)
{
	Tree tree={0};xrtownershipscope scope={0};freeze(&scope);
	testRequire(visit(xrtValueCursorOwnership(cursor),&tree),"real root");
	for(size_t i=0;i<tree.Count;++i)testRequire(tree.Nodes[i].Ref.Ops->Trace(tree.Nodes[i].Ref.Data,visit,&tree),"native owning slots");
	testRequire(tree.Count==4,"cursor, backing, two distinct values");
	testRequire(xrtMemDebugFailAfter(0),"pin path must not allocate");
	for(size_t i=0;i<tree.Count;++i){testRequire(tree.Nodes[i].Adapter->Hold(tree.Nodes[i].Ref.Data),"actual strong pin");++pins;}
	testRequire(!xrtMemDebugFailTriggered(),"no allocations in Hold");xrtMemDebugFailClear();end(&scope);
	/* The graph is now kept alive by these REAL pins alone. No host owner is
	 * subtracted from the later lifecycle steps. */
	xrtValueCursorRelease(cursor);freeze(&scope);
	for(size_t i=0;i<tree.Count;++i)testRequire(tree.Nodes[i].Adapter->Claim(tree.Nodes[i].Ref.Data,&tree),"claim native graph");
	testRequire(xrtMemDebugFailAfter(0),"clear path must not allocate");
	for(size_t i=0;i<tree.Count;++i)tree.Nodes[i].Adapter->Clear(tree.Nodes[i].Ref.Data,&tree);
	testRequire(!xrtMemDebugFailTriggered(),"no allocations in Clear");xrtMemDebugFailClear();end(&scope);
	for(size_t i=0;i<tree.Count;++i)if(tree.Nodes[i].Adapter->Finish){
		testRequire(tree.Nodes[i].Adapter->Finish(tree.Nodes[i].Ref.Data,&tree),"finish outside freeze");
		testRequire(tree.Nodes[i].Adapter->Finish(tree.Nodes[i].Ref.Data,&tree),"repeat Finish has no extra effects");}
	for(size_t i=tree.Count;i>0;--i)tree.Nodes[i-1].Adapter->Drop(tree.Nodes[i-1].Ref.Data);
	++clears;
}
static bool insert(xvalue* container,unsigned kind,unsigned index,xvalue* item)
{
	if(kind==0)return xrtValueArrayAppendNew(container,item);
	if(kind==1)return xrtValueIntMapSetNew(container,100+index,item);
	if(kind==2)return xrtValueSetAddNew(container,item);
	return xrtValueObjectSetNew(container,index==0?XRT_STR_LITERAL("first"):index==1?XRT_STR_LITERAL("second"):XRT_STR_LITERAL("third"),item);
}
static void scenario(unsigned kind,bool reverse)
{
	xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
	xvalue* source=kind==0?xrtValueArray():kind==1?xrtValueIntMap():kind==2?xrtValueSet():xrtValueObject();
	xvalue* first=xrtValueInt(41);xvalue* second=xrtValueInt(42);
	testRequire(source&&first&&second&&insert(source,kind,0,first)&&insert(source,kind,1,second),"two actual owning elements");
	for(unsigned fault=0;fault<8;++fault){
		testRequire(xrtMemDebugFailAfter(fault),"arm creation budget");
		xvaluecursor* probe=reverse?xrtValueCursorRCreate(source):xrtValueCursorCreate(source);
		bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
		if(triggered){testRequire(!probe&&xrtErrorKind(xrtGetError())==XERR_MEMORY,"atomic cursor creation failure");++failures;}
		else testRequire(probe!=NULL,"successful creation budget");
		xrtValueCursorRelease(probe);xrtClearError();
	}
	xvaluecursor* cursor=reverse?xrtValueCursorRCreate(source):xrtValueCursorCreate(source);
	testRequire(cursor!=NULL&&insert(source,kind,2,xrtValueInt(43)),"COW source mutation");
	xrtValueRelease(source);
	xrtownershipref ref=xrtValueCursorOwnership(cursor);xrtownershipresult graph={0};xrtownershipscope scope={0};freeze(&scope);
	testRequire(xrtOwnershipInspect(&ref,1,&ref,1,&graph)&&graph.NodeCount==4&&graph.EdgeCount==4&&graph.ExternalRootCount==0,"complete physical snapshot graph");++graphs;
	const xrtownershipadapterv1* adapter=xrtValueCursorOwnershipAdapterV1(ref);size_t count=0;int firstToken,secondToken;
	testRequire(adapter&&ref.Ops->Count(ref.Data,&count)&&count==1,"one actual initial owner");
	xvalueiter legacy={0};
	testRequire(!xrtValueCursorOwnershipAdapterV1(xrtValueIterOwnership(&legacy))&&
		!xrtValueCursorOwnershipAdapterV1((xrtownershipref){NULL,ref.Ops}),"legacy and null identity rejected before data use");
	testRequire(!adapter->Claim(ref.Data,NULL)&&adapter->Claim(ref.Data,&firstToken)&&
		adapter->Claim(ref.Data,&firstToken)&&!adapter->Claim(ref.Data,&secondToken),"exact claim token");
	adapter->Restore(ref.Data,&firstToken);end(&scope);
	testRequire(xrtValueCursorRetain(cursor)==cursor,"second actual owner");freeze(&scope);
	testRequire(ref.Ops->Count(ref.Data,&count)&&count==2&&adapter->Hold(ref.Data)&&
		ref.Ops->Count(ref.Data,&count)&&count==3,"Hold contributes an actual reference");end(&scope);
	adapter->Drop(ref.Data);xrtValueCursorRelease(cursor);
	xvaluekey key={0};xvalue* item=NULL;
	xrtSetErrorKind(XERR_STATE);
	testRequire(xrtValueCursorAdvance(cursor,&key,&item)==XVALUE_ITER_ITEM&&item==(reverse?second:first)&&
		xrtErrorKind(xrtGetError())==XERR_STATE,"first borrowed item and preserved ambient error");xrtClearError();
	if(kind==0)testRequire(key.Type==XVALUE_KEY_INDEX&&key.Index==(reverse?1u:0u),"snapshot array key");
	if(kind==1)testRequire(key.Type==XVALUE_KEY_INT&&key.Integer==(reverse?101:100),"snapshot integer key");
	if(kind==2)testRequire(key.Type==XVALUE_KEY_NONE,"set has no fabricated key");
	if(kind==3)testRequire(key.Type==XVALUE_KEY_STRING&&key.String.Size==(reverse?6u:5u)&&
		!memcmp(key.String.Data,reverse?"second":"first",key.String.Size),"borrowed snapshot key bytes");
	testRequire(xrtValueCursorAdvance(cursor,&key,&item)==XVALUE_ITER_ITEM&&item==(reverse?first:second),"second snapshot item");
	testRequire(xrtValueCursorAdvance(cursor,&key,&item)==XVALUE_ITER_END&&!item&&!xrtGetError(),"no post-snapshot source item");
	retire(cursor);xrtClearError();xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
		after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
		before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
		before.UseAfterFreeCount==after.UseAfterFreeCount,"per-transaction physical memory balance");
}
int main(void)
{
	testRequire(xrtMemDebugEnable(true),"enable allocator checks");xrtSetErrorKind(XERR_MEMORY);xrtClearError();
	for(unsigned round=0;round<50;++round)for(unsigned kind=0;kind<4;++kind)for(unsigned reverse=0;reverse<2;++reverse)scenario(kind,reverse!=0);
	testRequire(graphs==400&&pins==1600&&clears==400&&budgets==3200&&failures==400,"all managed cursor populations executed");
	printf("Managed Value cursors: 400 complete snapshot graphs, 1600 real pins, 400 claim/clear/repeated-finish lifecycles, 3200 OOM budgets and 400 actual failures; legacy ABI, COW isolation, borrowed views and exact memory balance\n");return 0;
}
