#include "../test.h"
#include "../test_thread.h"

/* Fail the backing allocator after quota reservation, rather than rejecting
 * at the quota check. This also exercises the non-pooled allocation path. */
static bool failBacking;
static ptr budgetAlloc(ptr context, size_t size)
{
	(void)context;
	return failBacking ? NULL : malloc(size);
}
static ptr budgetRealloc(ptr context, ptr memory, size_t size)
{
	(void)context;
	return failBacking ? NULL : realloc(memory, size);
}
static void budgetFree(ptr context, ptr memory)
{
	(void)context;
	free(memory);
}

static unsigned checks;
#define CK(x) do{++checks;testRequire((x),#x);}while(0)
typedef struct Worker {xallocationbudget* Budget; xallocationbudget* Other; xallocationscope* Foreign; void* Block; unsigned Success,Rejected;} Worker;
static int worker(ptr user)
{
	Worker* w=(Worker*)user;xallocationscope scope={0};
	if(w->Foreign&&xrtAllocationBudgetLeave(w->Foreign))return 1;
	if(!xrtAllocationBudgetEnter(w->Budget,&scope))return 2;
	if(w->Block){
		unsigned char* p=(unsigned char*)xrtRealloc(w->Block,32);if(!p||p[0]!=0x5A||p[31]!=0x5A)return 3;
		xallocationbudgetinfo info={.Size=sizeof(info)};
		if(!xrtAllocationBudgetInfo(w->Budget,&info)||info.LiveBytes)return 4;
		if(!xrtAllocationBudgetInfo(w->Other,&info)||info.LiveBytes!=32)return 5;
		xrtFree(p);w->Block=NULL;
	}else for(unsigned i=0;i<2000;++i){
		void* p=xrtCalloc(1,32768);xallocationbudgetinfo info={.Size=sizeof(info)};
		if(!xrtAllocationBudgetInfo(w->Budget,&info)||info.LiveBytes>info.Limit||info.PeakBytes>info.Limit)return 6;
		if(p){++w->Success;testThreadYield();xrtFree(p);}else ++w->Rejected;
	}
	return xrtAllocationBudgetLeave(&scope)?0:7;
}
int main(void)
{
	xallocator allocator = {NULL, budgetAlloc, budgetRealloc, budgetFree};
	testRequire(xrtSetAllocator(&allocator), "budget allocator installation failed");
	xallocationbudget *a=NULL,*b=NULL;xallocationbudgetinfo info={.Size=sizeof(info)};xallocationscope outer={0},inner={0};
	CK(!xrtAllocationBudgetCreate(0,&a)&&!a);CK(!xrtAllocationBudgetCreate(1,NULL));
	CK(xrtAllocationBudgetCreate(64,&a)&&xrtAllocationBudgetCreate(16,&b));
	CK(xrtAllocationBudgetInfo(a,&info)&&info.Limit==64&&!info.LiveBytes&&!info.LiveCount);
	CK(xrtAllocationBudgetEnter(a,&outer));CK(!xrtAllocationBudgetEnter(a,&outer));
	unsigned char* p=(unsigned char*)xrtMalloc(48);CK(p&&((uintptr_t)p&15)==0);memset(p,0x5A,48);
	CK(xrtAllocationBudgetInfo(a,&info)&&info.LiveBytes==48&&info.LiveCount==1);
	CK(!xrtAllocationBudgetSetLimit(a,47));CK(!xrtMalloc(17));
	CK(!xrtRealloc(p,32)&&p[0]==0x5A&&p[47]==0x5A);
	CK(xrtAllocationBudgetInfo(a,&info)&&info.LiveBytes==48&&info.Rejections==2&&info.PeakBytes<=64);
	CK(outer.Rejections==2&&xrtAllocationBudgetAddRef(a));xrtAllocationBudgetFree(a);
	CK(xrtAllocationBudgetEnter(b,&inner));CK(!xrtAllocationBudgetLeave(&outer));
	void* zero=xrtMalloc(0);CK(zero&&xrtAllocationBudgetInfo(b,&info)&&info.LiveBytes==1);
	CK(!xrtCalloc(SIZE_MAX,2)&&xrtAllocationBudgetInfo(b,&info)&&info.LiveBytes==1);
	CK(xrtAllocationBudgetSetLimit(a,128));unsigned char* moved=(unsigned char*)xrtRealloc(p,64);
	CK(moved&&moved[0]==0x5A&&moved[47]==0x5A);p=moved;
	CK(xrtAllocationBudgetInfo(a,&info)&&info.LiveBytes==64&&info.PeakBytes==112);
	CK(xrtAllocationBudgetInfo(b,&info)&&info.LiveBytes==1);
	CK(xrtAllocationBudgetLeave(&inner));CK(!xrtAllocationBudgetLeave(&inner));xrtFree(zero);
	Worker w={.Budget=b,.Other=a,.Foreign=&inner,.Block=p};testthread thread={.Proc=worker,.Data=&w};
	/* A block and the entered scope independently retain the original quota. */
	CK(xrtAllocationBudgetLeave(&outer));xrtAllocationBudgetFree(a);a=NULL;
	CK(xrtAllocationBudgetEnter(b,&inner));
	testThreadsStart(&thread,1);testThreadsJoin(&thread,1);CK(!thread.Result&&!w.Block);
	CK(xrtAllocationBudgetInfo(b,&info)&&!info.LiveBytes&&!info.LiveCount);
	CK(xrtAllocationBudgetLeave(&inner));
	CK(xrtAllocationBudgetEnter(b,&inner));xrtAllocationBudgetFree(b);b=NULL;
	p=(unsigned char*)xrtMalloc(16);CK(p);CK(xrtAllocationBudgetLeave(&inner));xrtFree(p);
	CK(xrtAllocationBudgetCreate(65536,&a));Worker workers[4]={0};testthread threads[4]={0};
	for(unsigned i=0;i<4;++i){workers[i].Budget=a;threads[i].Proc=worker;threads[i].Data=&workers[i];}
	testThreadsStart(threads,4);testThreadsJoin(threads,4);unsigned successes=0,rejected=0;
	for(unsigned i=0;i<4;++i){CK(!threads[i].Result);successes+=workers[i].Success;rejected+=workers[i].Rejected;}
	CK(successes>0&&successes+rejected==8000);
	CK(xrtAllocationBudgetInfo(a,&info)&&!info.LiveBytes&&!info.LiveCount&&info.PeakBytes<=65536&&info.PeakBytes>=32768);
	xrtAllocationBudgetFree(a);

	/* Backing OOM must return the reservation and keep realloc's source. */
	failBacking = true;
	CK(!xrtAllocationBudgetCreate(4096, &a) && !a);
	failBacking = false;
	CK(xrtAllocationBudgetCreate(16384, &a));
	CK(xrtAllocationBudgetEnter(a, &outer));
	failBacking = true;
	CK(!xrtMalloc(4096));
	failBacking = false;
	CK(xrtAllocationBudgetInfo(a, &info) && !info.LiveBytes && !info.LiveCount);
	p = (unsigned char*)xrtCalloc(1, 4096);
	CK(p && p[0] == 0 && p[4095] == 0);
	memset(p, 0xA5, 4096);
	failBacking = true;
	CK(!xrtRealloc(p, 8192));
	failBacking = false;
	CK(p[0] == 0xA5 && p[4095] == 0xA5);
	CK(xrtAllocationBudgetInfo(a, &info) && info.LiveBytes == 4096 && info.LiveCount == 1);
	CK(xrtRealloc(p, 0) == NULL);
	CK(xrtAllocationBudgetInfo(a, &info) && !info.LiveBytes && !info.LiveCount);
	CK(xrtAllocationBudgetLeave(&outer));
	xrtAllocationBudgetFree(a);

	/* An uncharged block adopts the active quota when it is reallocated. */
	p = (unsigned char*)xrtMalloc(32);
	CK(p);
	memset(p, 0x3C, 32);
	CK(xrtAllocationBudgetCreate(128, &a));
	CK(xrtAllocationBudgetEnter(a, &outer));
	moved = (unsigned char*)xrtRealloc(p, 48);
	CK(moved && moved[0] == 0x3C && moved[31] == 0x3C);
	CK(xrtAllocationBudgetInfo(a, &info) && info.LiveBytes == 48 && info.LiveCount == 1);
	CK(xrtAllocationBudgetLeave(&outer));
	xrtFree(moved);
	CK(xrtAllocationBudgetInfo(a, &info) && !info.LiveBytes && !info.LiveCount);
	xrtAllocationBudgetFree(a);
	xrtClearError();
	testMemoryDebugDrain("allocation budget leaked tracked memory");
	printf("{\"checks\":%u,\"failures\":0,\"shared_thread_quota\":true,\"realloc_reserves_destination\":true,\"cross_thread_free_and_sticky_realloc\":true,\"owner_release_before_blocks\":true,\"nested_scope_lifo\":true,\"zero_allocation_charged\":true,\"thread_successes\":%u,\"thread_rejections\":%u}\n",checks,successes,rejected);return 0;
}
