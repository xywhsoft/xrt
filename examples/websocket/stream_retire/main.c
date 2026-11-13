#include <xrt/websocket_stream.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct wsRetireExample {xnetengine* Engine;xatomicptr Client,Server;xatomic32 Entered,Permit,Returned,Refs,Drops;} wsRetireExample;
static void exampleRequire(bool OK,cstr S){if(!OK){fprintf(stderr,"[FAIL] %s\n",S);exit(1);}}
static void exampleAwait(xatomic32* A){double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){exampleRequire(xrtTimer()<End,"barrier timeout");xrtSleep(1);}}
static bool exampleCount(const void* D,size_t* N){*N=xrtAtomic32Load(&((const wsRetireExample*)D)->Refs,XMEMORY_ACQUIRE);return true;}
static bool exampleTrace(const void* D,xrtownershipvisitor V,ptr U){(void)D;(void)V;(void)U;return false;}
static void exampleDrop(const void* D){wsRetireExample* C=(wsRetireExample*)D;xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static const xrtownershipops ExampleOps={exampleCount,exampleTrace};
static void exampleClosed(xwsstream* S,const xwsstreamclose* X,ptr D){(void)X;wsRetireExample* C=D;if(xrtWsStreamRole(S)!=XWS_ROLE_CLIENT)return;exampleRequire(xrtWsStreamPollRetired(S)==XWAIT_TIMEOUT,"own Close callback still live");xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);xrtAtomic32Store(&C->Returned,1,XMEMORY_RELEASE);}
static const xwsstreamownershipv1 ExamplePolicy={sizeof(xwsstreamownershipv1),{.Close=exampleClosed},exampleDrop,&ExampleOps,NULL};
static xwsstream* exampleAttach(xnetstream* S,wsRetireExample* C,xwsrole Role){xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=Role;xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);xwsstream* W=xrtWsStreamAttachOwnedV1(S,NULL,&K,C,&ExamplePolicy);exampleRequire(W!=NULL,"owned WS exampleAttach");return W;}
static bool exampleAccept(xnetlistener* L,xnetstream* S,ptr D){(void)L;wsRetireExample* C=D;xrtAtomicPtrStore(&C->Server,exampleAttach(S,C,XWS_ROLE_SERVER),XMEMORY_RELEASE);return true;}
static void exampleOpened(xnetstream* S,ptr D){wsRetireExample* C=D;xrtAtomicPtrStore(&C->Client,exampleAttach(S,C,XWS_ROLE_CLIENT),XMEMORY_RELEASE);}
int main(void){wsRetireExample C={0};xrtAtomicPtrInit(&C.Client,NULL);xrtAtomicPtrInit(&C.Server,NULL);xrtAtomic32Init(&C.Entered,0);xrtAtomic32Init(&C.Permit,0);xrtAtomic32Init(&C.Returned,0);xrtAtomic32Init(&C.Refs,1);xrtAtomic32Init(&C.Drops,0);
 xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;C.Engine=xrtNetEngineCreate(&E);exampleRequire(C.Engine&&xrtNetEngineStart(C.Engine),"Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);exampleRequire(xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0),"loopback");xnetlistenerevents LE={exampleAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(C.Engine,&L,&LE,NULL,&C);xnetaddr A;exampleRequire(Listener&&xrtNetListenerLocal(Listener,&A),"listener");xnetstreamevents CE={.Open=exampleOpened};exampleRequire(xrtNetStreamConnect(C.Engine,&A,1,NULL,&CE,&C)!=NULL,"client TCP");
 double End=xrtTimer()+5;xwsstream *Client,*Server;while(!(Client=xrtAtomicPtrLoad(&C.Client,XMEMORY_ACQUIRE))||!(Server=xrtAtomicPtrLoad(&C.Server,XMEMORY_ACQUIRE))){exampleRequire(xrtTimer()<End,"WS ready");xrtSleep(1);}xnetstream* TCP=xrtWsStreamTcpRef(Client);exampleRequire(TCP&&xrtWsStreamAbort(Client),"client abort");exampleAwait(&C.Entered);
 exampleRequire(xrtWsStreamState(Client)==XWS_STREAM_CLOSED && xrtWsStreamWaitRetired(Client,20)==XWAIT_TIMEOUT,"extra native TCP alias delays physical retirement");
 xrtNetStreamDestroy(TCP);exampleRequire(xrtWsStreamWaitRetired(Client,5000)==XWAIT_OK,"client physical retirement");exampleAwait(&C.Returned);if(xrtWsStreamState(Server)!=XWS_STREAM_CLOSED)exampleRequire(xrtWsStreamAbort(Server)||xrtWsStreamState(Server)==XWS_STREAM_CLOSED,"server abort");xrtClearError();exampleRequire(xrtWsStreamWaitRetired(Server,5000)==XWAIT_OK && !xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE),"server retirement preserves owned Data");xrtWsStreamDestroy(Client);xrtWsStreamDestroy(Server);exampleRequire(xrtNetListenerClose(Listener),"listener close");xrtNetListenerDestroy(Listener);
 xnetretireresult R;End=xrtTimer()+5;while((R=xrtNetEngineTryDestroy(C.Engine))==XNET_RETIRE_BUSY){exampleRequire(xrtTimer()<End,"Engine physical tail");xrtSleep(1);}exampleRequire(R==XNET_RETIRE_READY&&xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==2,"all callback credits after host join");puts("[PASS] physical WS retirement and exact Data release after native alias handoff");return 0;}
