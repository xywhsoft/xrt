#ifndef XMQTT_TEST_TLS_RESUME_PEER_H
#define XMQTT_TEST_TLS_RESUME_PEER_H
#include "tls_peer.h"
typedef struct mqttTlsResumePeer {mqttTlsPeer Peer;xtlsresume* Ticket;xatomic32 Configured,Issued,Reset,Lookups,ServerResumed;unsigned Batch;} mqttTlsResumePeer;
static inline const xtlsresume* mqttTlsResumeLookup(ptr Data,const xtlsserverresumerequest* Request)
{
    mqttTlsResumePeer* R=Data;testRequire(Request && Request->Ticket.Size,"native resume lookup");
    xrtAtomic32FetchAdd(&R->Lookups,1,XMEMORY_RELEASE);return R->Ticket;
}
static inline void mqttTlsResumeConfigure(xnetworker* W,ptr Data)
{
    (void)W;mqttTlsResumePeer* R=Data;R->Peer.Config.Resume=mqttTlsResumeLookup;R->Peer.Config.ResumeContext=R;
    xrtAtomic32Store(&R->Configured,1,XMEMORY_RELEASE);
}
static inline void mqttTlsResumeStart(mqttTlsResumePeer* R,xmqttversion Version)
{
    memset(R,0,sizeof(*R));mqttTlsStart(&R->Peer,Version,0);
    testRequire(xrtNetEnginePost(R->Peer.Engine,0,mqttTlsResumeConfigure,R),"configure native server on its Worker");mqttTlsWait(&R->Configured,1,"native resume configuration");
}
static inline void mqttTlsResumeIssueTask(xnetworker* W,ptr Data)
{
    (void)W;mqttTlsResumePeer* R=Data;xtlsstream* S=xrtAtomicPtrLoad(&R->Peer.Server,XMEMORY_ACQUIRE);
    testRequire(S && xrtTlsStreamState(S)==XTLS_STREAM_OPEN,"authenticated issuing server");
    xrtAtomic32Store(&R->ServerResumed,xrtTlsServerResumed(xrtTlsStreamSession(S)),XMEMORY_RELEASE);
    unsigned Batch=R->Batch;
    for(unsigned I=0;I<Batch;++I){xtlsresume* Next=NULL;testRequire(xrtTlsServerTicketNew(xrtTlsStreamSession(S),&Next)==XTLS_OK && Next,"bounded native ticket issue");xrtTlsResumeRelease(R->Ticket);R->Ticket=Next;
        testRequire(xrtTlsStreamFlush(S)==XTLS_OK,"native ticket output");xrtAtomic32FetchAdd(&R->Issued,1,XMEMORY_RELEASE);}
}
static inline void mqttTlsResumeIssue(mqttTlsResumePeer* R,unsigned Count)
{
    uint32 Before=xrtAtomic32Load(&R->Issued,XMEMORY_ACQUIRE);R->Batch=Count;
    testRequire(xrtNetEnginePost(R->Peer.Engine,0,mqttTlsResumeIssueTask,R),"issue ticket command");mqttTlsWait(&R->Issued,Before+Count,"ticket command completion");
}
static inline void mqttTlsResumeResetTask(xnetworker* W,ptr Data)
{
    (void)W;mqttTlsResumePeer* R=Data;xtlsstream* S=xrtAtomicPtrLoad(&R->Peer.Server,XMEMORY_ACQUIRE);
    testRequire(S && xrtTlsStreamState(S)==XTLS_STREAM_CLOSED,"previous authenticated channel closed");xrtTlsStreamDestroy(S);xrtAtomicPtrStore(&R->Peer.Server,NULL,XMEMORY_RELEASE);
    R->Peer.Used=0;xrtAtomic32Store(&R->Peer.Closed,0,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&R->Reset,1,XMEMORY_RELEASE);
}
static inline void mqttTlsResumeReset(mqttTlsResumePeer* R)
{
    mqttTlsWait(&R->Peer.Closed,1,"previous server authenticated close");uint32 Before=xrtAtomic32Load(&R->Reset,XMEMORY_ACQUIRE);
    testRequire(xrtNetEnginePost(R->Peer.Engine,0,mqttTlsResumeResetTask,R),"reset peer on actual owner");mqttTlsWait(&R->Reset,Before+1,"peer reset completion");
}
static inline void mqttTlsResumeStop(mqttTlsResumePeer* R)
{mqttTlsStop(&R->Peer);xrtTlsResumeRelease(R->Ticket);R->Ticket=NULL;}
#endif
