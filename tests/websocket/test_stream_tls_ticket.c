#include "../test.h"
static xnetportbackend TicketBackend;
#define WS_RETIRE_BACKEND TicketBackend
#define main originalTlsTicketRetirementMain
#include "test_stream_retire.c"
#undef main
typedef struct ticketTest {
    wsRetireContext Base;xtlsresume *ServerTicket,*Value;
    xatomic32 Issued,Captured,Calls,Done,Entered,Permit,Payload;
    unsigned Mode,Action,Batch;bool Rejected,DroppedCaller;double Deadline;
} ticketTest;
static void ticketObserve(xwsstream* S,ptr Data)
{
    ticketTest* T=Data;testRequire(xrtNetWorkerIsCurrent(xrtWsStreamWorker(S)),"native ticket observer on actual WS owner");
    xtlssession* Native=xrtTlsStreamSession(xrtWsStreamTls(S));xtlsresume* Next;unsigned Count=0;
    while((Next=xrtTlsClientTakeResume(Native))!=NULL){xtlsresumeinfo I;testRequire(xrtTlsResumeInfo(Next,&I)&&I.Ticket.Size&&I.Secret.Size,"actual immutable authenticated TLS credential");xrtTlsResumeRelease(T->Value);T->Value=Next;++Count;}
    testRequire(Count>0,"ticket signal corresponds to native published credentials");xrtAtomic32FetchAdd(&T->Captured,Count,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&T->Calls,1,XMEMORY_RELEASE);
    if(T->Mode==1)testRequire(xrtWsStreamSetTlsTicket(S,NULL),"observer removes itself without altering WS/TLS events");
    if(T->Mode==2){testRequire(xrtWsStreamAbort(S),"observer aborts actual native transport");T->DroppedCaller=true;xrtWsStreamDestroy(S);}
    if(T->Mode==3){xrtAtomic32Store(&T->Entered,1,XMEMORY_RELEASE);wrAwait(&T->Permit,1,"release held native TLS ticket callback");}
}
static void ticketIssue(xnetworker* W,ptr Data)
{
    (void)W;ticketTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Server,XMEMORY_ACQUIRE);xtlsstream* Native=xrtWsStreamTls(S);
    testRequire(!xrtWsStreamSetTlsTicket(S,ticketObserve)&&!S->TlsTicket,"server WSS cannot register client ticket observer");xrtClearError();
    const unsigned Batch=T->Batch;
    for(unsigned N=0;N<Batch;++N){xtlsresume* Next=NULL;testRequire(Native&&xrtTlsServerTicketNew(xrtTlsStreamSession(Native),&Next)==XTLS_OK&&Next,"native server actually issues post-WS TLS1.3 ticket");xrtTlsResumeRelease(T->ServerTicket);T->ServerTicket=Next;testRequire(xrtTlsStreamFlush(Native)==XTLS_OK,"flush TLS ticket without any WS application bytes");xrtAtomic32FetchAdd(&T->Issued,1,XMEMORY_RELEASE);}
}
static void ticketCommand(xnetworker* W,ptr Data)
{
    (void)W;ticketTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Client,XMEMORY_ACQUIRE);
    if(T->Action==0){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"one-byte real allocation budget");
        for(unsigned I=0;I<10000;++I)testRequire(xrtWsStreamSetTlsTicket(S,ticketObserve)&&xrtWsStreamSetTlsTicket(S,NULL),"allocation-free optional registration/removal");
        testRequire(xrtWsStreamSetTlsTicket(S,ticketObserve)&&xrtAllocationBudgetLeave(&Scope),"finish allocation-free registration");xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B,&Info)&&!Info.PeakBytes&&!Info.Rejections&&!Info.LiveCount,"registration makes no allocation or callback work item");xrtAllocationBudgetFree(B);
        xtlsstream* Native=xrtWsStreamTls(S);ptr Saved=xrtTlsStreamData(Native);(void)Saved;
        xrtSetErrorInfo(XERR_STATE,"tls.ticket.ambient",1,"successful setter preserves error");const xerror* Error=xrtGetError();testRequire(xrtWsStreamSetTlsTicket(S,ticketObserve)&&xrtGetError()==Error,"success preserves ambient error");xrtClearError();
    }else if(T->Action==1)testRequire(xrtWsStreamSetTlsTicket(S,NULL),"remove opaque observer");
    else if(T->Action==2){T->Rejected=!xrtWsStreamSetTlsTicket(S,ticketObserve);xrtClearError();}
    else if(T->Action==3)testRequire(xrtWsStreamSetTlsTicket(S,ticketObserve),"restore admitted caller's callback");
    else if(T->Action==4){xtlsresume* Value=xrtTlsClientTakeResume(xrtTlsStreamSession(xrtWsStreamTls(S)));if(!Value){testRequire(xrtTimer()<T->Deadline&&xrtNetEnginePost(T->Base.Engine,1,ticketCommand,T),"bounded test-only inspection of a deliberately unobserved queue");return;}xrtTlsResumeRelease(Value);}
    else if(T->Action==5){xtlsstream* Native=xrtWsStreamTls(S);const xtlsstreamevents* E=&xrtWsStreamTlsEventsOwnershipV1Get()->Events;
        if(!T->Base.Owned){xtlsstreamevents Altered=*E;Altered.Ticket=NULL;testRequire(xrtTlsStreamSetEvents(Native,&Altered,S),"fixture replaces unowned native binding");testRequire(!xrtWsStreamSetTlsTicket(S,NULL)&&S->TlsTicket==ticketObserve,"replaced binding leaves observer unchanged");xrtClearError();testRequire(xrtTlsStreamSetEvents(Native,E,S),"restore actual WS transport binding");}
    }
    else if(T->Action==6){testRequire(!xrtWsStreamSetTlsTicket(S,ticketObserve)&&!S->TlsTicket,"non-WSS or terminal owner rejects without mutation");xrtClearError();}
    xrtAtomic32FetchAdd(&T->Done,1,XMEMORY_RELEASE);
}
static void ticketRunCommand(ticketTest* T,unsigned Action)
{uint32 Before=xrtAtomic32Load(&T->Done,XMEMORY_ACQUIRE);T->Action=Action;T->Deadline=xrtTimer()+10;testRequire(xrtNetEnginePost(T->Base.Engine,1,ticketCommand,T),"post owner command");wrAwait(&T->Done,Before+1,"owner command completed");}
static void ticketSend(ticketTest* T,unsigned Batch)
{uint32 Before=xrtAtomic32Load(&T->Issued,XMEMORY_ACQUIRE);T->Batch=Batch;testRequire(xrtNetEnginePost(T->Base.Engine,0,ticketIssue,T),"issue on actual server owner");wrAwait(&T->Issued,Before+Batch,"real ticket issued");}
static void ticketOpaque(ticketTest* T,bool Observe,bool Claim)
{
    if(!T->Base.Owned){return;}xwsstream* S=xrtAtomicPtrLoad(&T->Base.Client,XMEMORY_ACQUIRE);xrtownershipref Ref=xrtWsStreamOwnership(S);double End=xrtTimer()+10;unsigned Token=0;
    for(;;){xrtownershipscope Freeze={0};bool Idle=false,OK=false;if(xrtOwnershipFreezeTryBegin(&Freeze)){
        Idle=!xrtWsStreamOwnershipQuiescenceV1Get()->Busy(S);
        if(Idle){size_t Count=0;OK=Ref.Ops->Count(Ref.Data,&Count);testRequire(OK!=Observe,"opaque ticket callback blocks otherwise idle ownership snapshot");
            if(Claim){const xwsstreamownershipv1* Policies[]={&WrPolicy};const xrtownershippreparationv1* Prepare=NULL;const xrtownershipadapterv1* A=xrtWsStreamOwnershipAdapterV1(Ref,Policies,1,&Prepare);testRequire(A&&A->Claim(Ref.Data,&Token),"actual owned adapter claims after callback removal");testRequire(xrtOwnershipScopeEnd(&Freeze),"release freeze before owner attempt");ticketRunCommand(T,2);testRequire(T->Rejected&&!S->TlsTicket,"actual claim rejects new observer");
                while(!xrtOwnershipFreezeTryBegin(&Freeze)){testRequire(xrtTimer()<End,"restore claim freeze");xrtSleep(1);}A->Restore(Ref.Data,&Token);}
        }testRequire(xrtOwnershipScopeEnd(&Freeze),"leave actual ownership snapshot freeze");}
        if(Idle){break;}testRequire(xrtTimer()<End,"idle real Engine/WS ownership boundary");xrtSleep(1);}
}
static void ticketCase(bool Owned,unsigned Mode)
{
    ticketTest T={0};T.Mode=Mode;wrStart(&T.Base,true,Owned);xrtAtomic32Store(&T.Base.Permit,1,XMEMORY_RELEASE);xwsstream* C=xrtAtomicPtrLoad(&T.Base.Client,XMEMORY_ACQUIRE);xwsstream* S=xrtAtomicPtrLoad(&T.Base.Server,XMEMORY_ACQUIRE);
    testRequire(!xrtWsStreamSetTlsTicket(C,ticketObserve),"wrong Worker rejects registration");xrtClearError();
    if(!Mode){ticketSend(&T,1);ticketRunCommand(&T,4);testRequire(!xrtAtomic32Load(&T.Calls,XMEMORY_ACQUIRE),"native queue works before observing");}
    ticketRunCommand(&T,0);ticketRunCommand(&T,5);ticketOpaque(&T,true,false);
    if(!Mode){ticketRunCommand(&T,1);ticketOpaque(&T,false,true);ticketRunCommand(&T,3);ticketSend(&T,3);wrAwait(&T.Captured,3,"all real ticket-only records observed");}
    xwsstream* Alias=Mode==2?xrtWsStreamRef(C):NULL;xtlsstream* NativeAlias=Mode==3?xrtTlsStreamRef(xrtAtomicPtrLoad(&T.Base.TLSClient,XMEMORY_ACQUIRE)):NULL;
    if(Mode){ticketSend(&T,1);wrAwait(&T.Captured,1,"post-WS signal without application input");}
    if(Mode==1){uint32 Calls=xrtAtomic32Load(&T.Calls,XMEMORY_ACQUIRE);ticketSend(&T,1);ticketRunCommand(&T,4);testRequire(xrtAtomic32Load(&T.Calls,XMEMORY_ACQUIRE)==Calls,"self-removal stops notification but preserves native queue");}
    if(Mode==3){wrAwait(&T.Entered,1,"native ticket callback held");testRequire(xrtWsStreamAbort(C)&&xrtWsStreamWaitRetired(C,20)==XWAIT_TIMEOUT,"held ticket callback prevents physical retirement");xrtAtomic32Store(&T.Permit,1,XMEMORY_RELEASE);wrAwait(&T.Base.Returned,1,"actual Close callback completed");testRequire(xrtWsStreamWaitRetired(C,20)==XWAIT_TIMEOUT,"real TLS alias still prevents retirement");xrtTlsStreamDestroy(NativeAlias);}
    else if(Mode!=2)testRequire(xrtWsStreamAbort(C),"abort client after ticket observation");
    testRequire(xrtWsStreamWaitRetired(C,5000)==XWAIT_OK&&!C->TlsTicket,"physical client completion clears callback before context release");
    ticketRunCommand(&T,6);
    if(xrtWsStreamState(S)!=XWS_STREAM_CLOSED){(void)xrtWsStreamAbort(S);}testRequire(xrtWsStreamWaitRetired(S,5000)==XWAIT_OK,"physical native server completion");
    if(Alias)xrtWsStreamDestroy(Alias);else xrtWsStreamDestroy(C);xrtWsStreamDestroy(S);xrtNetListenerClose(T.Base.Listener);wrAwait(&T.Base.ListenerClosed,1,"listener closed");xrtNetListenerDestroy(T.Base.Listener);testRequire(xrtNetEngineDestroy(T.Base.Engine),"Engine joins every callback and native owner");
    testRequire(xrtAtomic32Load(&T.Base.Refs,XMEMORY_ACQUIRE)==1,"all owned WS/TLS Data credits returned after actual joins");xtlsresumeinfo I;testRequire(T.Value&&xrtTlsResumeInfo(T.Value,&I)&&I.Ticket.Size&&I.Secret.Size,"credential survives all native/WS/Engine owners");xrtTlsResumeRelease(T.Value);xrtTlsResumeRelease(T.ServerTicket);xrtTlsVerifierRelease(T.Base.Verifier);xrtTlsIdentityRelease(T.Base.Identity);xrtTlsContextRelease(T.Base.TLSContext);printf("[PASS] WSS TLS ticket owned%u mode%u\n",Owned,Mode);
}
static void ticketPlain(bool Owned)
{
    ticketTest T={0};wrStart(&T.Base,false,Owned);xrtAtomic32Store(&T.Base.Permit,1,XMEMORY_RELEASE);ticketRunCommand(&T,6);
    xwsstream* C=xrtAtomicPtrLoad(&T.Base.Client,XMEMORY_ACQUIRE);xwsstream* S=xrtAtomicPtrLoad(&T.Base.Server,XMEMORY_ACQUIRE);testRequire(xrtWsStreamAbort(C),"plain rejection still leaves healthy native owner");if(xrtWsStreamState(S)!=XWS_STREAM_CLOSED){(void)xrtWsStreamAbort(S);}
    testRequire(xrtWsStreamWaitRetired(C,5000)==XWAIT_OK&&xrtWsStreamWaitRetired(S,5000)==XWAIT_OK,"plain physical retirement");xrtWsStreamDestroy(C);xrtWsStreamDestroy(S);xrtNetListenerClose(T.Base.Listener);wrAwait(&T.Base.ListenerClosed,1,"plain listener closed");xrtNetListenerDestroy(T.Base.Listener);testRequire(xrtNetEngineDestroy(T.Base.Engine)&&xrtAtomic32Load(&T.Base.Refs,XMEMORY_ACQUIRE)==1,"plain native callback owners joined");
}
int main(int Argc,char** Argv)
{
    TicketBackend=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;setbuf(stdout,NULL);testRequire(!xrtWsStreamSetTlsTicket(NULL,NULL),"null owner rejected");xrtClearError();
    for(unsigned O=0;O<2;++O){ticketPlain(O!=0);for(unsigned M=0;M<4;++M)ticketCase(O!=0,M);}
    return 0;
}
