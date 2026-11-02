/* Private structural assertions are kept outside public API unit tests. */
#include "../test.h"
#include "../../src/pending/pending.c"

static size_t rpcPendingWalk(xrpcpending* P,uint32 Slot,bool* Seen,uint32* Previous,unsigned Depth,size_t* Count)
{
	if(!Slot)return 0;
	testRequire(Slot<=P->Config.MaxCalls && Depth<32 && !Seen[Slot-1u],"tree link/cycle/height bound");Seen[Slot-1u]=true;
	xrpcpendingentry* E=&P->Entries[Slot-1u];testRequire(E->Serial && E->KeyBytes && E->KeyBytes<=P->Config.Id.MaxKeyBytes,"tree live key/serial");
	size_t Left=rpcPendingWalk(P,E->Left,Seen,Previous,Depth+1u,Count);
	if(*Previous){xrpcpendingentry* Before=&P->Entries[*Previous-1u];size_t Common=Before->KeyBytes<E->KeyBytes?Before->KeyBytes:E->KeyBytes;
		int Order=memcmp(xrpcPendingKey(P,*Previous),xrpcPendingKey(P,Slot),Common);
		testRequire(Order<0 || (!Order && Before->KeyBytes<E->KeyBytes),"independent in-order key ordering");}
	*Previous=Slot;++*Count;size_t Right=rpcPendingWalk(P,E->Right,Seen,Previous,Depth+1u,Count);
	testRequire((Left>Right?Left-Right:Right-Left)<=1 && E->Height==1u+(Left>Right?Left:Right),"independent AVL height/balance invariant");
	return 1u+(Left>Right?Left:Right);
}
static void rpcPendingStructure(xrpcpending* P)
{
	bool Seen[257]={0},HeapSeen[257]={0};uint32 Previous=0;size_t Count=0;
	rpcPendingWalk(P,P->Root,Seen,&Previous,0,&Count);testRequire(Count==P->Count,"tree live count");
	uint32* Heap=xrpcPendingHeap(P);
	for(size_t Index=0;Index<P->Count;++Index){uint32 Slot=Heap[Index];testRequire(Slot && Slot<=257 && Seen[Slot-1u] && !HeapSeen[Slot-1u],"heap/tree membership");
		HeapSeen[Slot-1u]=true;testRequire(P->Entries[Slot-1u].HeapIndex==Index,"heap reverse index");
		if(Index){xrpcpendingentry* A=&P->Entries[Heap[(Index-1u)/2u]-1u];xrpcpendingentry* B=&P->Entries[Slot-1u];
			testRequire(A->Deadline<B->Deadline || (A->Deadline==B->Deadline && A->Serial<B->Serial),"independent heap tuple order");}}
	for(uint32 Slot=P->Free;Slot;Slot=P->Entries[Slot-1u].NextFree){testRequire(Slot<=257 && !Seen[Slot-1u] && !P->Entries[Slot-1u].Serial,"free/live partition");Seen[Slot-1u]=true;++Count;}
	testRequire(Count==257,"every slot accounted for");
}
int main(void)
{
	xrpcpendingconfig Config;xrpcPendingConfigInit(&Config);Config.MaxCalls=257;xrpcpending* P=NULL;
	testRequire(xrpcPendingCreate(&Config,&P),"structure create");uint32 State=0x142857u;
	for(size_t Trial=0;Trial<50000;++Trial){State=State*1664525u+1013904223u;unsigned Key=(State>>8)%513u;
		char Raw[32];int Bytes=snprintf(Raw,sizeof(Raw),"\"key-%08u\"",Key);xrpcpendingtoken Token;xrpcpendingcall Call;
		if((State&7u)<4){xrpcpendingstatus Status=xrpcPendingInsert(P,(xstrview){Raw,(size_t)Bytes},State>>16,NULL,&Token);testRequire(Status!=XRPC_PENDING_ERROR,"structure insertion");}
		else if((State&7u)<6)testRequire(xrpcPendingTakeId(P,(xstrview){Raw,(size_t)Bytes},&Call)!=XRPC_PENDING_ERROR,"structure remove ID");
		else if((State&7u)==6)testRequire(xrpcPendingTakeFirst(P,&Call)!=XRPC_PENDING_ERROR,"structure drain");
		else testRequire(xrpcPendingExpire(P,State>>16,&Call)!=XRPC_PENDING_ERROR,"structure expiry");
		rpcPendingStructure(P);
	}
	xrpcpendingcall Call;while(xrpcPendingTakeFirst(P,&Call)==XRPC_PENDING_OK){}
	/* Exercise the real exhaustion branch without 2^64 admissions. */
	P->Serial=UINT64_MAX-1u;xrpcpendingtoken Token={0};
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),0,NULL,&Token)==XRPC_PENDING_OK && Token.Serial==UINT64_MAX,"last serial is representable");
	xrpcpendingtoken Before=Token;testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("2"),0,NULL,&Token)==XRPC_PENDING_ERROR &&
		Token.Serial==Before.Serial && Token.Slot==Before.Slot && P->Count==1 && P->Serial==UINT64_MAX && xrtErrorCode(xrtGetError())==XRPC_PENDING_LIMIT,"serial exhaustion atomicity");
	xrtClearError();testRequire(xrpcPendingTakeToken(P,Before,&Call)==XRPC_PENDING_OK,"last serial cancellation");rpcPendingStructure(P);
	printf("[PASS] 50000 private tree/heap/free-partition invariants; serial exhaustion; header=%zu entry=%zu config=%zu token=%zu call=%zu info=%zu owned_257=%zu\n",
		sizeof(xrpcpending),sizeof(xrpcpendingentry),sizeof(xrpcpendingconfig),sizeof(xrpcpendingtoken),sizeof(xrpcpendingcall),sizeof(xrpcpendinginfo),P->OwnedBytes);
	xrpcPendingDestroy(P);return 0;
}
