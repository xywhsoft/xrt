#include <xrt.h>
#include <stdio.h>
/* The application owns one Engine and keeps the Stream owner alive through
 * both posts. Poll observes retirement; it neither closes nor consumes it. */
typedef struct pollExample {xnetpost Post;xnetstream* Stream;xatomic32 Done;xwaitresult Result;} pollExample;
static void query(xnetworker* Worker,ptr Data)
{(void)Worker;pollExample* P=Data;P->Result=xrtNetStreamPollRetired(P->Stream);xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);}
static bool posted(xnetengine* E,pollExample* P,unsigned Affinity)
{
    xrtAtomic32Init(&P->Done,0);if(!xrtNetPostInit(&P->Post)||!xrtNetPost(xrtNetEngineWorker(E,Affinity),&P->Post,query,P)){return false;}
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&P->Done,XMEMORY_ACQUIRE)){if(xrtTimer()>=End){return false;}xrtThreadYield();}return true;
}
int main(void)
{
    xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* E=xrtNetEngineCreate(&K);if(!E||!xrtNetEngineStart(E)){return 1;}
    xnetlistenconfig L;xrtNetListenConfigInit(&L);if(!xrtNetAddrParse(&L.Address,"127.0.0.1",0)){return 2;}xnetlistener* Listen=xrtNetListen(E,&L,NULL,NULL,NULL);xnetaddr A;
    if(!Listen||!xrtNetListenerLocal(Listen,&A)){return 3;}xnetstream* S=xrtNetStreamConnect(E,&A,0,NULL,NULL,NULL);if(!S){return 4;}
    double End=xrtTimer()+5;while(xrtNetStreamState(S)!=XNET_STREAM_OPEN){if(xrtTimer()>=End){return 5;}xrtThreadYield();}
    xnetstream* Peer=NULL;while(!(Peer=xrtNetListenerAccept(Listen))){if(xrtTimer()>=End){return 6;}xrtThreadYield();}
    if(!xrtNetStreamAbort(S)||xrtNetStreamWaitRetired(S,5000)!=XWAIT_OK){return 7;}
    xnetstream* Alias=xrtNetStreamRef(S);pollExample Pending={0},Ready={0};Pending.Stream=Ready.Stream=S;
    if(!Alias||!posted(E,&Pending,0)||Pending.Result!=XWAIT_TIMEOUT){return 8;}
    xrtNetStreamDestroy(Alias);if(!posted(E,&Ready,1)||Ready.Result!=XWAIT_OK){return 9;}
    if(!xrtNetStreamAbort(Peer)||xrtNetStreamWaitRetired(Peer,5000)!=XWAIT_OK){return 10;}xrtNetStreamDestroy(Peer);xrtNetStreamDestroy(S);
    if(!xrtNetListenerClose(Listen)){return 11;}xrtNetListenerDestroy(Listen);End=xrtTimer()+5;
    while(!xrtNetEngineStop(E)){if(xrtTimer()>=End){return 12;}xrtClearError();xrtThreadYield();}
    if(!xrtNetEngineDestroy(E)){return 13;}puts("retired poll: real alias pending -> same-engine worker ready; all owners joined");{return 0;}
}
