#define XRT_MODULE_LOGGER_ASYNC
#define XRT_MODULE_LOGGER_RING
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>

static unsigned Drops, Traces;
static bool trace(const void* Data,xrtownershipvisitor Visit,void* Context)
{ (void)Data;(void)Visit;(void)Context;++Traces;return true; }
static bool worker(const void* Data){(void)Data;return true;}
static const xlogsinkownershipv1 Policy={sizeof(Policy),trace,worker};
static const xlogsinkownershipv1 Other={sizeof(Other),trace,NULL};
static void drop(void* Data){(void)Data;++Drops;}
static xlogresult write_record(const xlogrecord* Record,void* Data)
{(void)Record;(void)Data;return XLOG_RESULT_WRITTEN;}
static xrtownershipref Nodes[12]; static size_t NodeCount;
static bool collect(xrtownershipref Ref,void* Unused)
{ (void)Unused;assert(Ref.Data&&Ref.Ops);assert(NodeCount<12);Nodes[NodeCount++]=Ref;return true; }
static const xrtownershipadapterv1* adapter(xrtownershipref Ref)
{
 const xlogsinkownershipv1* Allowed[]={&Policy,xrtLogAsyncSinkOwnershipV1Get(),xrtLogRingSinkOwnershipV1Get()};
 const xrtownershipadapterv1* A=xrtLogOwnershipAdapterV1(Ref);
 if(!A)A=xrtLogSinkOwnershipAdapterV1(Ref,Allowed,3);
 if(!A)A=xrtLogAsyncOwnershipAdapterV1(Ref);
 if(!A)A=xrtLogRingOwnershipAdapterV1(Ref);
 return A;
}
static void closure(xrtownershipref Root)
{
 NodeCount=0;collect(Root,NULL);
 for(size_t I=0;I<NodeCount;++I){assert(adapter(Nodes[I]));assert(Nodes[I].Ops->Trace(Nodes[I].Data,collect,NULL));}
}
static xlogsink* sink(void)
{
 xlogsinkconfig C={0};C.Level=XLOG_TRACE;C.Write=write_record;C.Drop=drop;
 xlogsink* S=xrtLogSinkCreate(&C);assert(S);return S;
}
int main(void)
{
 xlogger* L=xrtLogCreate(XRT_STR_LITERAL("owned"),XLOG_TRACE);xlogsink* S=sink();
 assert(L&&S&&!xrtLogSinkOwnership(S).Ops&&!xrtLogSinkWorkerSafe(S));
 assert(xrtLogSinkBindOwnershipV1(S,&Policy));assert(xrtLogSinkWorkerSafe(S));
 assert(!xrtLogSinkBindOwnershipV1(S,&Other));xrtClearError();
 xrtownershipref SR=xrtLogSinkOwnership(S);const xlogsinkownershipv1* Wrong[]={&Other};
 assert(!xrtLogSinkOwnershipAdapterV1(SR,Wrong,1)&&!Traces);
 assert(xrtLogAttach(L,S));xrtLogSinkFree(S);
 xrtownershipref LR=xrtLogOwnership(L);xrtownershipscope Freeze={0};
 assert(xrtOwnershipFreezeTryBegin(&Freeze));closure(LR);assert(NodeCount==3&&Traces==1);
 size_t N;assert(Nodes[0].Ops->Count(Nodes[0].Data,&N)&&N==1);
 assert(Nodes[1].Ops->Count(Nodes[1].Data,&N)&&N==1);
 assert(Nodes[2].Ops->Count(Nodes[2].Data,&N)&&N==1);
 xrtownershipresult Result={0};assert(xrtOwnershipInspect(NULL,0,&LR,1,&Result));
 assert(Result.ExternalRootCount==0&&Result.NodeCount==3);
 const xrtownershipadapterv1* A[12];unsigned Token=0;
 for(size_t I=0;I<NodeCount;++I){A[I]=adapter(Nodes[I]);assert(A[I]->Hold(Nodes[I].Data));}
 for(size_t I=0;I<NodeCount;++I)assert(A[I]->Claim(Nodes[I].Data,&Token));
 assert(!xrtLogRef(L));xrtClearError();
 for(size_t I=0;I<NodeCount;++I)A[I]->Clear(Nodes[I].Data,&Token);
 assert(xrtOwnershipScopeEnd(&Freeze));assert(!Drops);
 for(size_t I=0;I<NodeCount;++I){assert(A[I]->Finish(Nodes[I].Data,&Token));assert(A[I]->Finish(Nodes[I].Data,&Token));}
 assert(Drops==1);
 for(size_t I=0;I<NodeCount;++I)A[I]->Drop(Nodes[I].Data);
 xrtLogFree(L);assert(Drops==1);
 /* The real worker state is a separate owner of its target. Nesting an async
  * wrapper remains native and safe; no invented roots or flattened counts. */
 S=sink();assert(xrtLogSinkBindOwnershipV1(S,&Policy));
 xlogsink* Async=xrtLogAsync(S,NULL);assert(Async&&xrtLogSinkWorkerSafe(Async));
 assert(xrtLogAsyncStop(Async));
 assert(xrtOwnershipFreezeTryBegin(&Freeze));closure(xrtLogSinkOwnership(Async));
 assert(NodeCount==3);assert(Nodes[1].Ops->Count(Nodes[1].Data,&N)&&N==1);
 assert(Nodes[2].Ops->Count(Nodes[2].Data,&N)&&N==2);assert(xrtOwnershipScopeEnd(&Freeze));
 xrtLogSinkFree(Async);xrtLogSinkFree(S);assert(Drops==2);
 S=sink();assert(xrtLogSinkBindOwnershipV1(S,&Policy));
 xlogsink* Ring=xrtLogRing(S,NULL);assert(Ring&&xrtLogSinkWorkerSafe(Ring));assert(xrtLogRingStop(Ring));
 assert(xrtOwnershipFreezeTryBegin(&Freeze));closure(xrtLogSinkOwnership(Ring));assert(NodeCount==3);
 assert(xrtOwnershipScopeEnd(&Freeze));xrtLogSinkFree(Ring);xrtLogSinkFree(S);assert(Drops==3);
 assert(!xrtGetError());return 0;
}
