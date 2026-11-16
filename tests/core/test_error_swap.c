#include "../test.h"
#include <xrt/thread.h>
#include <xrt/atomic.h>
static xatomic32 Notifications;
static void swapHandler(const xerror* Error,ptr Data)
{
    (void)Data;xrtAtomic32FetchAdd(&Notifications,1,XMEMORY_RELAXED);xerror* Taken=NULL;
    testRequire(xrtTrySwapErrorTake(NULL,&Taken)&&Taken==Error&&!xrtGetError(),"quiet take inside handler");
    testRequire(xrtTrySwapErrorTake(Taken,&Taken)&&!Taken&&xrtGetError()==Error,"quiet restore inside handler without recursion");
}
static int32 swapWorker(ptr Data)
{
    int32 Number=(int32)(intptr_t)Data;xerror* A=xrtErrorCreate(XERR_STATE,"swap.thread",Number,"thread current");xerror* Spare=xrtErrorCreate(XERR_STATE,"swap.thread",Number,"thread spare");testRequire(A&&Spare,"worker error owners");xerror* Prior=NULL;
    testRequire(xrtTrySwapErrorTake(A,&Prior)&&!Prior,"fresh thread has independent error slot");
    for(unsigned I=0;I<10000;++I){testRequire(xrtTrySwapErrorTake(Spare,&Spare)&&xrtErrorCode(xrtGetError())==Number&&xrtErrorCode(Spare)==Number,"thread-local owning swaps remain isolated");}
    testRequire(xrtTrySwapErrorTake(NULL,&Prior)&&Prior&&Prior!=Spare,"worker takes last slot owner");xrtErrorFree(Prior);xrtErrorFree(Spare);return Number;
}
int main(void)
{
    xrtAtomic32Init(&Notifications,0);xerror* Initial=NULL;testRequire(xrtTrySwapErrorTake(NULL,&Initial),"initial quiet scope");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(65536,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"owning swap allocation accounting scope");
    xerror* A=xrtErrorCreate(XERR_STATE,"swap.owned",1,"first");xerror* B=xrtErrorCreate(XERR_STATE,"swap.owned",2,"second");testRequire(A&&B,"owned errors allocated");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&BI.LiveCount==2,"exact two owned allocations");size_t Peak=BI.PeakBytes;
    xerror* Old=(xerror*)(uintptr_t)1;testRequire(xrtTrySwapErrorTake(A,&Old)&&!Old&&xrtGetError()==A,"slot takes first owner");
    xrtSetErrorHandler(swapHandler,NULL);testRequire(!xrtTrySwapErrorTake(B,NULL)&&xrtGetError()==A,"invalid output takes no input ownership or slot state");testRequire(!xrtTrySwapErrorTake(B,(xerror**)(void*)B)&&xrtGetError()==A,"incoming error alias rejected");
    testRequire(xrtTrySwapErrorTake(B,&Old)&&Old==A&&xrtGetError()==B&&!strcmp(xrtErrorMessage(Old),"first"),"prior owner returned alive without retain/free");xrtErrorFree(Old);testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&BI.LiveCount==1&&BI.PeakBytes==Peak&&!BI.Rejections,"returned owner freed with no hidden reference or new allocation");
    testRequire(xrtTrySwapErrorTake(NULL,&Old)&&Old==B&&!xrtGetError(),"take second owner");xrtErrorFree(Old);testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&!BI.LiveBytes&&!BI.LiveCount,"all transferred owners physically refunded");testRequire(xrtAllocationBudgetLeave(&Scope),"leave owning budget");xrtAllocationBudgetFree(Budget);
    testRequire(!xrtAtomic32Load(&Notifications,XMEMORY_RELAXED),"quiet exchanges never call a user error handler");
    xrtSetErrorInfo(XERR_STATE,"swap.regular",77,"ordinary publication still notifies");testRequire(xrtAtomic32Load(&Notifications,XMEMORY_RELAXED)==1&&xrtErrorCode(xrtGetError())==77,"ordinary setter and reentrant quiet swap retain notification semantics");
    xthread* Threads[4];for(unsigned I=0;I<4;++I){Threads[I]=xrtThreadCreate(swapWorker,(ptr)(intptr_t)(100+I),0);testRequire(Threads[I]!=NULL,"create swap worker");}
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWait(Threads[I])==XWAIT_OK&&xrtThreadExitCode(Threads[I])==(int32)(100+I),"join thread-local swap worker");xrtThreadDestroy(Threads[I]);}
    testRequire(xrtErrorCode(xrtGetError())==77&&xrtAtomic32Load(&Notifications,XMEMORY_RELAXED)==1,"workers preserve parent slot and handler count");xrtSetErrorHandler(NULL,NULL);testRequire(xrtTrySwapErrorTake(Initial,&Old),"restore original context owner");xrtErrorFree(Old);puts("[PASS] quiet owning error swaps, handlers/reentry, budgets/refunds and40000 thread-local exchanges");return 0;
}
