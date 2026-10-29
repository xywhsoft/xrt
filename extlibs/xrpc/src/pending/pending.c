#include <xrpc/pending.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_PENDING)
typedef struct xrpcpendingentry {
	size_t KeyBytes;
	uint64 Serial, Deadline;
	ptr User;
	uint32 Left, Right, Height, HeapIndex, NextFree;
} xrpcpendingentry;
struct xrpcpending {
	xrpcpendingconfig Config;
	size_t Count, OwnedBytes;
	uint64 Serial;
	uint32 Root, Free;
	xrpcpendingentry Entries[];
};
static xrpcpendingstatus xrpcPendingReport(xerrkind Kind, xrpcpendingerror Code, cstr Message)
{ xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message); return XRPC_PENDING_ERROR; }
static xrpcpendingentry* xrpcPendingEntry(xrpcpending* P,uint32 Slot) { return &P->Entries[Slot-1u]; }
static uint32* xrpcPendingHeap(xrpcpending* P) { return (uint32*)(P->Entries+P->Config.MaxCalls); }
static unsigned char* xrpcPendingScratch(xrpcpending* P) { return (unsigned char*)(xrpcPendingHeap(P)+P->Config.MaxCalls); }
static unsigned char* xrpcPendingKey(xrpcpending* P,uint32 Slot) { return xrpcPendingScratch(P)+(size_t)Slot*P->Config.Id.MaxKeyBytes; }
static int xrpcPendingCompare(xrpcpending* P,const unsigned char* Key,size_t Bytes,uint32 Slot)
{
	size_t Other=xrpcPendingEntry(P,Slot)->KeyBytes,Common=Bytes<Other?Bytes:Other;
	int Order=memcmp(Key,xrpcPendingKey(P,Slot),Common);
	return Order?Order:(Bytes>Other)-(Bytes<Other);
}
static uint32 xrpcPendingHeight(xrpcpending* P,uint32 Slot) { return Slot?xrpcPendingEntry(P,Slot)->Height:0; }
static void xrpcPendingHeightSet(xrpcpending* P,uint32 Slot)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Slot);uint32 L=xrpcPendingHeight(P,E->Left),R=xrpcPendingHeight(P,E->Right);
	E->Height=1u+(L>R?L:R);
}
static uint32 xrpcPendingRotate(xrpcpending* P,uint32 Root,bool Left)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);uint32 Next=Left?E->Right:E->Left;
	xrpcpendingentry* N=xrpcPendingEntry(P,Next);
	if(Left){E->Right=N->Left;N->Left=Root;}else{E->Left=N->Right;N->Right=Root;}
	xrpcPendingHeightSet(P,Root);xrpcPendingHeightSet(P,Next);return Next;
}
static uint32 xrpcPendingBalance(xrpcpending* P,uint32 Root)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);xrpcPendingHeightSet(P,Root);
	int Difference=(int)xrpcPendingHeight(P,E->Left)-(int)xrpcPendingHeight(P,E->Right);
	if(Difference>1){
		xrpcpendingentry* L=xrpcPendingEntry(P,E->Left);
		if(xrpcPendingHeight(P,L->Right)>xrpcPendingHeight(P,L->Left))E->Left=xrpcPendingRotate(P,E->Left,true);
		return xrpcPendingRotate(P,Root,false);
	}
	if(Difference < -1){
		xrpcpendingentry* R=xrpcPendingEntry(P,E->Right);
		if(xrpcPendingHeight(P,R->Left)>xrpcPendingHeight(P,R->Right))E->Right=xrpcPendingRotate(P,E->Right,false);
		return xrpcPendingRotate(P,Root,true);
	}
	return Root;
}
static uint32 xrpcPendingTreeInsert(xrpcpending* P,uint32 Root,uint32 Slot)
{
	if(!Root)return Slot;
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(xrpcPendingCompare(P,xrpcPendingKey(P,Slot),xrpcPendingEntry(P,Slot)->KeyBytes,Root)<0)
		E->Left=xrpcPendingTreeInsert(P,E->Left,Slot);
	else E->Right=xrpcPendingTreeInsert(P,E->Right,Slot);
	return xrpcPendingBalance(P,Root);
}
static uint32 xrpcPendingExtractMin(xrpcpending* P,uint32 Root,uint32* Min)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(!E->Left){*Min=Root;return E->Right;}
	E->Left=xrpcPendingExtractMin(P,E->Left,Min);return xrpcPendingBalance(P,Root);
}
/* Structural transplantation keeps each live token bound to its original slot. */
static uint32 xrpcPendingTreeRemove(xrpcpending* P,uint32 Root,uint32 Slot)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(Root==Slot){
		if(!E->Left)return E->Right;
		if(!E->Right)return E->Left;
		uint32 Next=0,Right=xrpcPendingExtractMin(P,E->Right,&Next);
		xrpcpendingentry* N=xrpcPendingEntry(P,Next);N->Left=E->Left;N->Right=Right;
		return xrpcPendingBalance(P,Next);
	}
	if(xrpcPendingCompare(P,xrpcPendingKey(P,Slot),xrpcPendingEntry(P,Slot)->KeyBytes,Root)<0)
		E->Left=xrpcPendingTreeRemove(P,E->Left,Slot);
	else E->Right=xrpcPendingTreeRemove(P,E->Right,Slot);
	return xrpcPendingBalance(P,Root);
}
static uint32 xrpcPendingFind(xrpcpending* P,size_t Bytes)
{
	uint32 Slot=P->Root;
	while(Slot){int Order=xrpcPendingCompare(P,xrpcPendingScratch(P),Bytes,Slot);
		if(!Order)return Slot;
		Slot=Order<0?xrpcPendingEntry(P,Slot)->Left:xrpcPendingEntry(P,Slot)->Right;}
	return 0;
}
static bool xrpcPendingBefore(xrpcpending* P,uint32 A,uint32 B)
{
	xrpcpendingentry* X=xrpcPendingEntry(P,A);xrpcpendingentry* Y=xrpcPendingEntry(P,B);
	return X->Deadline!=Y->Deadline?X->Deadline<Y->Deadline:X->Serial<Y->Serial;
}
static void xrpcPendingHeapSet(xrpcpending* P,size_t Index,uint32 Slot)
{ xrpcPendingHeap(P)[Index]=Slot;xrpcPendingEntry(P,Slot)->HeapIndex=(uint32)Index; }
static void xrpcPendingHeapUp(xrpcpending* P,size_t Index,uint32 Slot)
{
	uint32* Heap=xrpcPendingHeap(P);
	while(Index){size_t Parent=(Index-1u)/2u;if(!xrpcPendingBefore(P,Slot,Heap[Parent]))break;
		xrpcPendingHeapSet(P,Index,Heap[Parent]);Index=Parent;}
	xrpcPendingHeapSet(P,Index,Slot);
}
static void xrpcPendingHeapDown(xrpcpending* P,size_t Index,uint32 Slot)
{
	uint32* Heap=xrpcPendingHeap(P);
	while(Index<P->Count/2u){size_t Child=Index*2u+1u;
		if(Child+1u<P->Count && xrpcPendingBefore(P,Heap[Child+1u],Heap[Child]))++Child;
		if(!xrpcPendingBefore(P,Heap[Child],Slot))break;
		xrpcPendingHeapSet(P,Index,Heap[Child]);Index=Child;}
	/* Odd counts have one final parent at (Count-2)/2, covered by Count/2. */
	xrpcPendingHeapSet(P,Index,Slot);
}
static bool xrpcPendingOutput(xrpcpending* P,ptr Output,size_t Bytes)
{
	if(!P || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(P,P->OwnedBytes,Output,Bytes)){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending output/table overlap");return false;}
	return true;
}
static bool xrpcPendingInput(xrpcpending* P,xstrview Id,ptr Output,size_t OutputBytes,size_t* Bytes)
{
	if(!xrpcPendingOutput(P,Output,OutputBytes))return false;
	if(!xrtMemRangeValid(Id.Data,Id.Size) || xrtMemRangesOverlap(P,P->OwnedBytes,Id.Data,Id.Size) ||
		xrtMemRangesOverlap(Output,OutputBytes,Id.Data,Id.Size)){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending ID/output/table overlap");return false;}
	return xrpcIdKeyWrite(xrpcPendingScratch(P),P->Config.Id.MaxKeyBytes,Id,&P->Config.Id,Bytes);
}
XRPC_API bool xrpcPendingConfigInit(xrpcpendingconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config))){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending config output");return false;}
	xrpcpendingconfig C={sizeof(C),1024,2u*1024u*1024u,{0}};
	xrpcIdConfigInit(&C.Id);C.Id.MaxStringBytes=256;C.Id.MaxKeyBytes=512;*Config=C;return true;
}
XRPC_API bool xrpcPendingCreate(const xrpcpendingconfig* Config,xrpcpending** Result)
{
	if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config &&
		(!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result))))){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending constructor span");return false;}
	xrpcpendingconfig C;if(Config)C=*Config;else xrpcPendingConfigInit(&C);
	if(C.Size!=sizeof(C) || !C.MaxCalls || C.MaxCalls>UINT32_MAX || !C.MaxOwnedBytes ||
		C.Id.Size!=sizeof(C.Id) || !C.Id.MaxInputBytes || !C.Id.MaxStringBytes || !C.Id.MaxKeyBytes){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending configuration");return false;}
	size_t Bytes=sizeof(xrpcpending);
	if(C.MaxCalls>(SIZE_MAX-Bytes)/(sizeof(xrpcpendingentry)+sizeof(uint32)))goto Limit;
	Bytes+=C.MaxCalls*(sizeof(xrpcpendingentry)+sizeof(uint32));
	if(C.MaxCalls==SIZE_MAX || C.Id.MaxKeyBytes>(SIZE_MAX-Bytes)/(C.MaxCalls+1u))goto Limit;
	Bytes+=(C.MaxCalls+1u)*C.Id.MaxKeyBytes;
	if(Bytes>C.MaxOwnedBytes)goto Limit;
	xrpcpending* P=xrtMalloc(Bytes);
	if(!P){xrpcPendingReport(XERR_MEMORY,XRPC_PENDING_MEMORY,"RPC pending allocation failed");return false;}
	P->Config=C;P->Count=0;P->OwnedBytes=Bytes;P->Serial=0;P->Root=0;P->Free=1;
	for(size_t Index=0;Index<C.MaxCalls;++Index){P->Entries[Index]=(xrpcpendingentry){0};
		P->Entries[Index].NextFree=Index+1u<C.MaxCalls?(uint32)(Index+2u):0;}
	*Result=P;return true;
Limit:
	xrpcPendingReport(XERR_RANGE,XRPC_PENDING_LIMIT,"RPC pending owned-byte limit/size overflow");return false;
}
XRPC_API void xrpcPendingDestroy(xrpcpending* Pending) { if(Pending)xrtFree(Pending); }
XRPC_API bool xrpcPendingInfo(const xrpcpending* Pending,xrpcpendinginfo* Info)
{
	xrpcpending* P=(xrpcpending*)Pending;
	if(!xrpcPendingOutput(P,Info,sizeof(*Info)))return false;
	if(Info->Size!=sizeof(*Info)){xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending info size");return false;}
	*Info=(xrpcpendinginfo){sizeof(*Info),P->Config.MaxCalls,P->Count,P->OwnedBytes,
		P->Count?xrpcPendingEntry(P,xrpcPendingHeap(P)[0])->Deadline:UINT64_MAX};return true;
}
XRPC_API xrpcpendingstatus xrpcPendingInsert(xrpcpending* P,xstrview Id,uint64 Deadline,ptr User,xrpcpendingtoken* Token)
{
	size_t Bytes=0;if(!xrpcPendingInput(P,Id,Token,sizeof(*Token),&Bytes))return XRPC_PENDING_ERROR;
	if(xrpcPendingFind(P,Bytes))return XRPC_PENDING_DUPLICATE;
	if(!P->Free)return XRPC_PENDING_FULL;
	if(P->Serial==UINT64_MAX)return xrpcPendingReport(XERR_RANGE,XRPC_PENDING_LIMIT,"RPC pending serial exhausted");
	uint32 Slot=P->Free;xrpcpendingentry* E=xrpcPendingEntry(P,Slot);P->Free=E->NextFree;
	*E=(xrpcpendingentry){Bytes,++P->Serial,Deadline,User,0,0,1,0,0};
	memcpy(xrpcPendingKey(P,Slot),xrpcPendingScratch(P),Bytes);
	P->Root=xrpcPendingTreeInsert(P,P->Root,Slot);++P->Count;xrpcPendingHeapUp(P,P->Count-1u,Slot);
	*Token=(xrpcpendingtoken){E->Serial,(size_t)Slot-1u};return XRPC_PENDING_OK;
}
static xrpcpendingstatus xrpcPendingTake(xrpcpending* P,uint32 Slot,xrpcpendingcall* Call)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Slot);
	xrpcpendingcall Value={{E->Serial,(size_t)Slot-1u},E->Deadline,E->User};
	P->Root=xrpcPendingTreeRemove(P,P->Root,Slot);
	size_t Index=E->HeapIndex;uint32 Last=xrpcPendingHeap(P)[--P->Count];
	if(Index<P->Count){
		if(Index && xrpcPendingBefore(P,Last,xrpcPendingHeap(P)[(Index-1u)/2u]))xrpcPendingHeapUp(P,Index,Last);
		else xrpcPendingHeapDown(P,Index,Last);
	}
	*E=(xrpcpendingentry){0};E->NextFree=P->Free;P->Free=Slot;*Call=Value;return XRPC_PENDING_OK;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeId(xrpcpending* P,xstrview Id,xrpcpendingcall* Call)
{
	size_t Bytes=0;if(!xrpcPendingInput(P,Id,Call,sizeof(*Call),&Bytes))return XRPC_PENDING_ERROR;
	uint32 Slot=xrpcPendingFind(P,Bytes);return Slot?xrpcPendingTake(P,Slot,Call):XRPC_PENDING_MISSING;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeToken(xrpcpending* P,xrpcpendingtoken Token,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	if(!Token.Serial || Token.Slot>=P->Config.MaxCalls || P->Entries[Token.Slot].Serial!=Token.Serial)return XRPC_PENDING_MISSING;
	return xrpcPendingTake(P,(uint32)(Token.Slot+1u),Call);
}
XRPC_API xrpcpendingstatus xrpcPendingExpire(xrpcpending* P,uint64 Now,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	if(!P->Count)return XRPC_PENDING_MISSING;
	uint32 Slot=xrpcPendingHeap(P)[0];uint64 Deadline=xrpcPendingEntry(P,Slot)->Deadline;
	return Deadline!=UINT64_MAX && Deadline<=Now?xrpcPendingTake(P,Slot,Call):XRPC_PENDING_MISSING;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeFirst(xrpcpending* P,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	return P->Count?xrpcPendingTake(P,xrpcPendingHeap(P)[0],Call):XRPC_PENDING_MISSING;
}
#endif
