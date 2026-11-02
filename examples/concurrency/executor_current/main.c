/* Safe synchronous-facade guard with one worker. Run from repository root:
 * python tools/build.py --compiler gcc --suite executor --no-single --cflag=-O2
 * Expect: own-worker wait rejected; queued child still runs once. Every pool
 * is closed, drained and destroyed before its borrowed stack context expires. */
#include <xrt.h>
#include <stdio.h>
typedef struct currentexample {xexecutor* Pool;xatomic32 Parent,Child,Rejected;} currentexample;
static void child(ptr Raw)
{currentexample* C=Raw;xrtAtomic32FetchAdd(&C->Child,1,XMEMORY_RELEASE);}
static void parent(ptr Raw)
{
    currentexample* C=Raw;
    if(xrtExecutorSubmit(C->Pool,child,C,NULL,NULL) && xrtExecutorIsCurrent(C->Pool))xrtAtomic32Store(&C->Rejected,1,XMEMORY_RELEASE);
    /* Return instead of waiting: with one worker, only this return lets the
     * queued child run. NULL/another executor is not this native context. */
    xrtAtomic32Store(&C->Parent,1,XMEMORY_RELEASE);
}
int main(void)
{
    currentexample C={0};xexecutorconfig Config={1,4,0};C.Pool=xrtExecutorCreate(&Config);if(!C.Pool)return 1;
    bool OK=!xrtExecutorIsCurrent(C.Pool) && !xrtExecutorIsCurrent(NULL) && xrtExecutorSubmit(C.Pool,parent,&C,NULL,NULL);double End=xrtTimer()+5;
    while(OK && !xrtAtomic32Load(&C.Parent,XMEMORY_ACQUIRE)){if(xrtTimer()>End){OK=false;break;}xrtThreadYield();}
    if(!xrtExecutorClose(C.Pool) || xrtExecutorWaitFor(C.Pool,5000)!=XWAIT_OK)OK=false;
    OK=OK && xrtAtomic32Load(&C.Rejected,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.Child,XMEMORY_ACQUIRE)==1;
    if(!xrtExecutorDestroy(C.Pool))OK=false;
    if(OK)puts("own-worker wait rejected; queued child ran once");
    return OK?0:2;
}
