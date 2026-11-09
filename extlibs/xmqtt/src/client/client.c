#include <xmqtt/client.h>
#include <xmqtt/client_auth.h>
#include <xmqtt/client_dial.h>
#include <xmqtt/client_tls.h>
#include <xmqtt/client_tls_resume.h>
#include <xmqtt/client_inbox.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)

/* The host worker is the sole protocol owner. Gate protects admission, public
 * snapshots and the receive queue; no user callback runs while it is held. */
typedef struct xmqttquota {xatomic32 Refs;xmutex Gate;size_t Count,Bytes,MaxCount,MaxBytes;} xmqttquota;
typedef struct xmqttbox {xmqttquota* Quota;size_t Bytes;} xmqttbox;
typedef struct xmqttreadybox {xmqttreadyresult Value;xmqttbox Box;char Id[];} xmqttreadybox;
typedef struct xmqttwritebox {xmqttwriteresult Value;xmqttbox Box;} xmqttwritebox;
typedef struct xmqttsubscriptionbox {xmqttsubscriptionresult Value;xmqttbox Box;} xmqttsubscriptionbox;
typedef struct xmqttrequest xmqttrequest;
typedef struct xmqttsendflow {
    size_t Capacity,Reserved;uint16 Next;
    unsigned char Identifiers[8192];
    xmqttrequest* Buckets[];
} xmqttsendflow;
typedef struct xmqttreceiveflow {
    size_t Capacity,Head,Count;unsigned Offset;
    uint16 Identifiers[];
} xmqttreceiveflow;
enum {XMQTT_RX_ACTIVE=1,XMQTT_RX_REC_QUEUED=4,
    XMQTT_RX_COMP_QUEUED=8,XMQTT_RX_RELEASED=16,XMQTT_RX_UNKNOWN_QUEUED=32};
typedef struct xmqttreceiveack {uint16 Identifier;uint8 Type;bool Unknown;} xmqttreceiveack;
/* Constant-time incoming identifier lookup, independent of outgoing ids. Only
 * the first QoS2/unknown PUBREL allocates it; duplicates allocate no objects. */
typedef struct xmqttreceivetwo {
    size_t Capacity,Head,Count,Inflight;unsigned Offset;bool TwoNext;
    unsigned char State[65536];xmqttreceiveack Acks[];
} xmqttreceivetwo;
typedef struct xmqttstartup {xatomic32 Refs;xmqttclient* Client;size_t Bytes;unsigned char Data[];} xmqttstartup;
typedef struct xmqttalias {size_t Size,Capacity;char Topic[];} xmqttalias;
enum {XMQTT_TX_ALIAS=8,XMQTT_TX_TLS=16};
typedef struct xmqttsendtopic {uint32 Refs;size_t Size;char Data[];} xmqttsendtopic;
typedef struct xmqttsendaliasslot {xmqttsendtopic *Planned,*Actual;} xmqttsendaliasslot;
typedef struct xmqttsendaliases {xmqttsendaliasslot* Pages[256];} xmqttsendaliases;
/* Optional aligned request tail: no larger allocation for ordinary publishes. */
typedef struct xmqttsendaliasrequest {
    xmqttsendtopic* Topic;xmqttsendaliasslot* Slot;size_t FrameBytes,TailOffset;
    uint16 Alias;uint8 PrefixBytes,Mode,RawHeader;unsigned char Prefix[7];
} xmqttsendaliasrequest;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
typedef struct xmqtttlsfence {xmqttrequest *Previous,*Next;uint64 Cut;} xmqtttlsfence;
typedef struct xmqtttlsplan {xtlssession* Session;xtlsstreamconfig Config;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool Capture;
#endif
} xmqtttlsplan;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
typedef struct xmqtttlsresumestate {xmqttclienttlsresumeinfo Info;xtlsresume* Value;bool Notify;} xmqtttlsresumestate;
#endif
typedef struct xmqtttlsstate {
    xmqtttlsplan Plan;xtlsstream* Stream;xmqttrequest *Head,*Tail;size_t Bytes,Calls;bool Open;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    xmqtttlsresumestate Resume;
#endif
} xmqtttlsstate;
#endif
struct xmqttmessage {xatomic32 Refs;xmqttbox Box;xmqttpacket Packet;xmqttmessage* Next;xmqttproperty Props[];};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
struct xmqttclientinbox {
    xmutex Gate;xmqttclientinboxconfig Config;xmqttquota* Quota;
    xmqttclient* Client;xmqttmessage *Head,*Tail;size_t Count,Bytes;
};
#endif
struct xmqttrequest {
    xatomic32 Refs,NativeRefs;
    xmqttclient* Client;xmqttrequest *Next,*Previous,*HashNext;
    xpromise* Promise;xcancel* Cancel;union {xmqttwritebox* Write;xmqttsubscriptionbox* Subscription;} Result;
    size_t Bytes,Offset;double Deadline;bool Retain,Credit,Acknowledged;
    uint8 Qos;uint16 Identifier;uint8 Type,Capabilities;
    union {struct {uint8 Offset;bool Pending,Sent;unsigned char Wire[4];} Release;size_t TopicCount;} State;
    unsigned char Wire[];
};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
typedef struct xmqttauthbox {xmqttauthresult Value;xmqttbox Box;} xmqttauthbox;
typedef struct xmqttauthwire {xatomic32 Refs;xmqttclient* Client;size_t Bytes,Offset;unsigned char Wire[];} xmqttauthwire;
struct xmqttauthticket {xatomic32 Refs;xmqttbox Box;xmqttclient* Client;xmqttmessage* Message;xmqttauthphase Phase;bool Taken,Answered;};
typedef struct xmqttauthstate {
    xmqttauthconfig Config;xstrview Method;size_t Bytes,Rounds,Sent;
    bool ProtocolReady,Verified,Active,Reauth,AwaitServer;int Decision;
    xmqttauthticket* Ticket;xmqttauthwire* Out;xpromise* Promise;xcancel* Cancel;xmqttauthbox* Result;
    double Deadline,StepDeadline;char MethodBytes[];
} xmqttauthstate;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
typedef struct xmqttdialplan {
    xnetresolver* Resolver;xnetaddrlist* Addresses;xstrview Host;uint16 Port;
    xnetdialconfig Config;
} xmqttdialplan;
#endif
struct xmqttclient {
    xatomic32 Refs;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetstream* Stream;
    xmqttclientconfig Config;xmqttclientinfo Info;xmqttquota* Quota;
    xnetpost Post;bool Posted,Started,Stopped,ReadEnded,NativeClosed,ProblemInfo,ReceivePaused,AbortRequested,Draining,DrainClosing;
    xpromise* ReadyPromise;xfuture* ReadyFuture;xcancel* ReadyCancel;xmqttreadybox* ReadyBox;
    xmqttstartup* Startup;size_t SetupOffset;xmqttpacket SetupPacket;
    xmqttproperty *SetupProps,*Props;
    unsigned char* Input;size_t InputBytes,InputCapacity;
    xmqttrequest *SubmitHead,*SubmitTail,*Requests,*RequestTail,*Partial;
    xatomicptr SendFlow;uint32 SendInflight;
    xmqttreceiveflow* ReceiveFlow;xmqttreceivetwo* ReceiveTwo;xmqttalias** ReceiveAliases;xmqttsendaliases* SendAliases;
    xmqttmessage *MessageHead,*MessageTail;
    xmqttnegotiated Peer;xerror* Cause;
    /* Before CONNACK PingDeadline records last accepted control output time;
     * afterwards it is the outstanding PING response deadline. */
    double SetupDeadline,NextPing,PingDeadline,TimerAt,DrainDeadline;
    uint64 Timer;unsigned PingOffset;bool PingOutstanding;
    xmqttrequest* Disconnect;bool DisconnectReserved,ApplicationClose,ApplicationFinal,ApplicationNativeClosing;
    xmutex RetirementLock;xcond RetirementReady;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    xmqttclientinbox* Inbox;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttauthstate* Auth;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetdial* Dial;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqtttlsstate* TLS;
#endif
    xmqttproperty Scratch[];
};
static void xmqttClientWork(xnetworker* Worker,ptr Data);
static bool xmqttClientWakeLocked(xmqttclient* C);
static void xmqttClientWake(xmqttclient* C);
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static void xmqttTlsFail(xmqttclient* C,xtlsresult Result)
{
    const xerror* E=C->TLS->Stream?xrtTlsStreamError(C->TLS->Stream):NULL;
    if(!E && Result==XTLS_ERROR)E=xrtGetError();
    xerrkind K=E?xrtErrorKind(E):XERR_NONE;
    xmqttClientFail(C,K==XERR_MEMORY?XMQTT_STATUS_MEMORY:K==XERR_RANGE || K==XERR_AGAIN?XMQTT_STATUS_LIMIT:K==XERR_TIMEOUT?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_TRANSPORT,E);
}
#endif
static bool xmqttTransportOpen(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return C->TLS->Stream && C->TLS->Open && xrtTlsStreamState(C->TLS->Stream)==XTLS_STREAM_OPEN;
#endif
    return C->Stream && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN;
}
static size_t xmqttTransportAvailable(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return C->TLS->Stream?xrtTlsStreamAvailable(C->TLS->Stream):0;
#endif
    return xrtNetStreamAvailable(C->Stream);
}
static size_t xmqttTransportRead(xmqttclient* C,void* Data,size_t Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){size_t N=0;xtlsresult S=xrtTlsStreamRead(C->TLS->Stream,Data,Size,&N);
        if(S!=XTLS_OK && S!=XTLS_AGAIN && S!=XTLS_CLOSED)xmqttTlsFail(C,S);
        return N;}
#endif
    return xrtNetStreamRead(C->Stream,Data,Size);
}
static size_t xmqttTransportWritable(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return xmqttTransportOpen(C)?SIZE_MAX:0;
#endif
    return xrtNetStreamWritable(C->Stream);
}
static bool xmqttTransportClose(xmqttclient* C,bool Abort)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS && C->TLS->Stream)return Abort?xrtTlsStreamAbort(C->TLS->Stream):xrtTlsStreamClose(C->TLS->Stream);
#endif
    return !C->Stream || (Abort?xrtNetStreamAbort(C->Stream):xrtNetStreamClose(C->Stream));
}
static xnetresult xmqttTransportSend(xmqttclient* C,cbytes Data,size_t* Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(!C->TLS->Bytes || !C->TLS->Calls){*Size=0;xmqttClientWake(C);return XNET_RESULT_AGAIN;}
        size_t N=0,Capacity=*Size;if(Capacity>C->TLS->Bytes)Capacity=C->TLS->Bytes;--C->TLS->Calls;
        xtlsresult S=xrtTlsStreamSend(C->TLS->Stream,Data,Capacity,&N);*Size=N;C->TLS->Bytes-=N;
        if(S!=XTLS_OK && S!=XTLS_AGAIN){xmqttTlsFail(C,S);return N?XNET_RESULT_OK:XNET_RESULT_ERROR;}
        return N?XNET_RESULT_OK:XNET_RESULT_AGAIN;}
#endif
    return xrtNetStreamSend(C->Stream,Data,*Size);
}
static xnetresult xmqttTransportSendRef(xmqttclient* C,cbytes Data,size_t* Size,xnetreleaseproc Release,ptr User)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){xnetresult S=xmqttTransportSend(C,Data,Size);if(S==XNET_RESULT_OK)Release(User,Data,*Size);return S;}
#endif
    return xrtNetStreamSendRef(C->Stream,Data,*Size,Release,User);
}
static bool xmqttClientError(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.mqtt.client",1,Message);return false;}
static void xmqttClientRef(xmqttclient* C)
{(void)xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);}
static void xmqttClientDrop(const void* Data)
{
    xmqttclient* C=(xmqttclient*)Data;uint32 Count=xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE);
    /* Above two, a successful CAS cannot publish the caller-only state. There
     * is no Client access after it. Terminal publication shares the wait lock;
     * Gate may already be held by a failed post, so it cannot be the wait lock. */
    while(Count>2)if(xrtAtomic32CompareExchange(&C->Refs,&Count,Count-1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return;
    if(!xrtMutexLock(&C->RetirementLock))abort();
    uint32 Before=xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_ACQ_REL);
    if(Before<2 || (Before==2 && !xrtCondBroadcast(&C->RetirementReady)))abort();
    if(!xrtMutexUnlock(&C->RetirementLock))abort();
}
/* RefCount is the actual first member of both private producer objects.
 * Their children are deliberately opaque to a foreign ownership collector. */
static bool xmqttPrivateCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load((const xatomic32*)Data,XMEMORY_ACQUIRE);return true;}
static bool xmqttPrivateTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xmqttPrivateOps={xmqttPrivateCount,xmqttPrivateTrace};
static bool xmqttClientRetainProducer(const void* Data)
{xmqttClientRef((xmqttclient*)Data);return true;}
static void xmqttClientWake(xmqttclient* C)
{xrtMutexLock(&C->Gate);(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static bool xmqttClientWakeLocked(xmqttclient* C)
{
    if(C->Posted)return true;
    C->Posted=true;xmqttClientRef(C);
    if(xrtNetPost(C->Worker,&C->Post,xmqttClientWork,C))return true;
    C->Posted=false;xmqttClientDrop(C);return false;
}
static bool xmqttClientClosing(xmqttclient* C)
{xrtMutexLock(&C->Gate);bool B=C->Info.State>=XMQTT_CLIENT_CLOSING && !(C->ApplicationClose && !C->ApplicationFinal && !C->AbortRequested && C->Info.CloseStatus==XMQTT_STATUS_OK);xrtMutexUnlock(&C->Gate);return B;}
static void xmqttClientNotify(xmqttclient* C)
{if(C->Config.Notify)C->Config.Notify(C,C->Config.User);}
static double xmqttClientDeadline(double Now,int64 Milliseconds)
{return Milliseconds==XRT_WAIT_FOREVER?INFINITY:nextafter(Now+(double)(Milliseconds/1000)+(double)(Milliseconds%1000)*0.001,INFINITY);}
static int64 xmqttClientRemaining(double At)
{
    double Delta=ceil((At-xrtTimer())*1000.0);
    if(Delta<=1.0)return 1;
    return Delta>=(double)INT64_MAX?INT64_MAX:(int64)Delta;
}
static void xmqttQuotaDrop(xmqttquota* Q)
{
    if(Q && xrtAtomic32FetchSub(&Q->Refs,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&Q->Gate);xrtFree(Q);}
}
static xmqttquota* xmqttQuotaCreate(size_t Count,size_t Bytes)
{
    xmqttquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q)return NULL;
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);return NULL;}
    xrtAtomic32Init(&Q->Refs,1);Q->MaxCount=Count;Q->MaxBytes=Bytes;return Q;
}
static void* xmqttBoxAllocate(xmqttquota* Q,size_t Bytes,xmqttclientstatus* Failure)
{
    xrtMutexLock(&Q->Gate);
    if(Q->Count==Q->MaxCount || Bytes>Q->MaxBytes-Q->Bytes){xrtMutexUnlock(&Q->Gate);*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    ++Q->Count;Q->Bytes+=Bytes;(void)xrtAtomic32FetchAdd(&Q->Refs,1,XMEMORY_RELAXED);xrtMutexUnlock(&Q->Gate);
    void* P=xrtMalloc(Bytes);
    if(P){memset(P,0,Bytes);return P;}
    xrtMutexLock(&Q->Gate);--Q->Count;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xmqttQuotaDrop(Q);*Failure=XMQTT_STATUS_MEMORY;return NULL;
}
static void xmqttBoxFree(void* P,xmqttbox B)
{xrtFree(P);xrtMutexLock(&B.Quota->Gate);--B.Quota->Count;B.Quota->Bytes-=B.Bytes;xrtMutexUnlock(&B.Quota->Gate);xmqttQuotaDrop(B.Quota);}
static xstrview xmqttRebaseText(xstrview V,const unsigned char* Old,const unsigned char* New)
{if(V.Data)V.Data=(const char*)(New+((const unsigned char*)V.Data-Old));return V;}
static xbytesview xmqttRebaseBytes(xbytesview V,const unsigned char* Old,const unsigned char* New)
{if(V.Data)V.Data=New+((const unsigned char*)V.Data-Old);return V;}
static xmqttmessage* xmqttMessageCopy(xmqttclient* C,const xmqttpacket* P,xmqttclientstatus* Failure,const xstrview* Resolved)
{
    size_t Count=P->Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttmessage))/sizeof(xmqttproperty) ||
       P->Wire.Size>SIZE_MAX-sizeof(xmqttmessage)-Count*sizeof(xmqttproperty)){*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    size_t Bytes=sizeof(xmqttmessage)+Count*sizeof(xmqttproperty)+P->Wire.Size;
    if(Resolved){if(Resolved->Size>SIZE_MAX-Bytes){*Failure=XMQTT_STATUS_LIMIT;return NULL;}Bytes+=Resolved->Size;}
    xmqttmessage* M=xmqttBoxAllocate(C->Quota,Bytes,Failure);if(!M)return NULL;
    M->Box=(xmqttbox){C->Quota,Bytes};xrtAtomic32Init(&M->Refs,1);M->Packet=*P;
    unsigned char* Wire=(unsigned char*)(M->Props+Count);memcpy(Wire,P->Wire.Data,P->Wire.Size);
    if(Count)memcpy(M->Props,P->Properties.Data,Count*sizeof(*M->Props));
    M->Packet.Wire=(xbytesview){Wire,P->Wire.Size};M->Packet.Properties.Data=Count?M->Props:NULL;
    for(size_t I=0;I<Count;++I){xmqttproperty* V=M->Props+I;
        if(V->Type==XMQTT_PROPERTY_STRING)V->Value.Text=xmqttRebaseText(V->Value.Text,P->Wire.Data,Wire);
        else if(V->Type==XMQTT_PROPERTY_BINARY)V->Value.Binary=xmqttRebaseBytes(V->Value.Binary,P->Wire.Data,Wire);
        else if(V->Type==XMQTT_PROPERTY_STRING_PAIR){V->Value.Pair.Name=xmqttRebaseText(V->Value.Pair.Name,P->Wire.Data,Wire);V->Value.Pair.Value=xmqttRebaseText(V->Value.Pair.Value,P->Wire.Data,Wire);}
    }
    if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK)M->Packet.Body.Reasons=xmqttRebaseBytes(P->Body.Reasons,P->Wire.Data,Wire);
    if(P->Type==XMQTT_PACKET_PUBLISH){
        if(Resolved){char* Topic=(char*)(Wire+P->Wire.Size);memcpy(Topic,Resolved->Data,Resolved->Size);M->Packet.Body.Publish.Topic=(xstrview){Topic,Resolved->Size};}
        else M->Packet.Body.Publish.Topic=xmqttRebaseText(P->Body.Publish.Topic,P->Wire.Data,Wire);
        M->Packet.Body.Publish.Payload=xmqttRebaseBytes(P->Body.Publish.Payload,P->Wire.Data,Wire);
    }
    return M;
}
XMQTT_API const xmqttpacket* xmqttMessagePacket(const xmqttmessage* M)
{return M?&M->Packet:NULL;}
XMQTT_API bool xmqttMessageRetain(const xmqttmessage* Message)
{
    if(!Message)return xmqttClientError(XERR_ARGUMENT,"message is NULL");
    xmqttmessage* M=(xmqttmessage*)Message;uint32 N=xrtAtomic32Load(&M->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&M->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    return xmqttClientError(XERR_RANGE,"message reference limit");
}
XMQTT_API void xmqttMessageRelease(const xmqttmessage* Message)
{xmqttmessage* M=(xmqttmessage*)Message;if(M && xrtAtomic32FetchSub(&M->Refs,1,XMEMORY_ACQ_REL)==1){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(M->Packet.Type==XMQTT_PACKET_AUTH || M->Packet.Type==XMQTT_PACKET_CONNACK)xrtSecureZero((ptr)M->Packet.Wire.Data,M->Packet.Wire.Size);
#endif
    xmqttBoxFree(M,M->Box);}}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
XMQTT_API bool xmqttClientInboxConfigInit(xmqttclientinboxconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox config output");
    *K=(xmqttclientinboxconfig){sizeof(*K),XMQTT_VERSION_5,512,16u*1024u*1024u,64,8u*1024u*1024u};return true;
}
XMQTT_API bool xmqttClientInboxCreate(const xmqttclientinboxconfig* Config,xmqttclientinbox** Out)
{
    xmqttclientinboxconfig K;
    if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox config");K=*Config;}else xmqttClientInboxConfigInit(&K);
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || K.Size!=sizeof(K) ||
       (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u ||
       !K.MaxObjectBytes || !K.MaxMessages || !K.MaxMessageBytes)return xmqttClientError(XERR_ARGUMENT,"invalid Inbox limits");
    xmqttclientinbox* I=xrtCalloc(1,sizeof(*I));if(!I)return false;
    if(!xrtMutexInit(&I->Gate)){xrtFree(I);return false;}
    I->Config=K;I->Quota=xmqttQuotaCreate(K.MaxObjects,K.MaxObjectBytes);
    if(!I->Quota){xrtMutexUnit(&I->Gate);xrtFree(I);return false;}*Out=I;return true;
}
static bool xmqttInboxPut(xmqttclientinbox* I,xmqttmessage* M)
{
    xrtMutexLock(&I->Gate);bool Full=I->Count==I->Config.MaxMessages || M->Box.Bytes>I->Config.MaxMessageBytes-I->Bytes;
    if(!Full){if(I->Tail)I->Tail->Next=M;else I->Head=M;I->Tail=M;++I->Count;I->Bytes+=M->Box.Bytes;}
    xrtMutexUnlock(&I->Gate);return !Full;
}
XMQTT_API bool xmqttClientInboxTake(xmqttclientinbox* I,xmqttmessage** Out)
{
    if(!I || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox message output");
    xrtMutexLock(&I->Gate);xmqttmessage* M=I->Head;
    if(M){I->Head=M->Next;if(!I->Head)I->Tail=NULL;--I->Count;I->Bytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&I->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInboxInfo(xmqttclientinbox* I,xmqttclientinboxinfo* Out)
{
    if(!I || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox info output");
    xrtMutexLock(&I->Gate);xmqttclientinboxinfo V={sizeof(V),I->Client!=NULL,I->Count,I->Bytes,0,0};xrtMutexUnlock(&I->Gate);
    xrtMutexLock(&I->Quota->Gate);V.Objects=I->Quota->Count;V.ObjectBytes=I->Quota->Bytes;xrtMutexUnlock(&I->Quota->Gate);*Out=V;return true;
}
XMQTT_API bool xmqttClientInboxDestroy(xmqttclientinbox* I)
{
    if(!I)return true;
    xrtMutexLock(&I->Gate);
    if(I->Client){xrtMutexUnlock(&I->Gate);return xmqttClientError(XERR_STATE,"Inbox still attached to Client");}
    xmqttmessage* M=I->Head;xrtMutexUnlock(&I->Gate);
    while(M){xmqttmessage* Next=M->Next;xmqttMessageRelease(M);M=Next;}
    xmqttQuotaDrop(I->Quota);xrtMutexUnit(&I->Gate);xrtFree(I);return true;
}
#endif
static void xmqttReadyFree(ptr Value,ptr Context)
{(void)Context;xmqttreadybox* B=Value;xmqttMessageRelease(B->Value.Connack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttWriteFree(ptr Value,ptr Context)
{(void)Context;xmqttwritebox* B=Value;xmqttMessageRelease(B->Value.Puback);xmqttMessageRelease(B->Value.Pubrec);xmqttMessageRelease(B->Value.Pubcomp);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttSubscriptionFree(ptr Value,ptr Context)
{(void)Context;xmqttsubscriptionbox* B=Value;xmqttMessageRelease(B->Value.Ack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static bool xmqttRequestSubscription(const xmqttrequest* R)
{return R->Type==XMQTT_PACKET_SUBSCRIBE || R->Type==XMQTT_PACKET_UNSUBSCRIBE;}
static void xmqttRequestResultFree(xmqttrequest* R)
{
    if(xmqttRequestSubscription(R)){if(R->Result.Subscription)xmqttSubscriptionFree(R->Result.Subscription,NULL);}
    else if(R->Result.Write)xmqttWriteFree(R->Result.Write,NULL);
}
/* Gate serializes cold allocation and all identifier reservations. Only the
 * worker mutates hash buckets; the immutable table address publishes atomically. */
static xmqttclientadmission xmqttIdentifierReserveLocked(xmqttclient* C,uint16 Requested,uint16* Identifier)
{
    xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
    if(!F){size_t Capacity=2;while(Capacity<C->Config.MaxRequests*2u)Capacity*=2u;
        size_t Bytes=sizeof(*F)+Capacity*sizeof(xmqttrequest*);
        if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes)return XMQTT_ADMISSION_OVERLOADED;
        F=xrtCalloc(1,Bytes);if(!F)return XMQTT_ADMISSION_ERROR;
        F->Capacity=Capacity;C->Info.PrivateBytes+=Bytes;xrtAtomicPtrStore(&C->SendFlow,F,XMEMORY_RELEASE);
    }
    if(F->Reserved==65535u)return XMQTT_ADMISSION_OVERLOADED;
    uint16 Id=Requested;
    if(Id){if(F->Identifiers[Id>>3]&(1u<<(Id&7u)))return XMQTT_ADMISSION_OVERLOADED;}
    else {do{Id=++F->Next;if(!Id)Id=++F->Next;}while(F->Identifiers[Id>>3]&(1u<<(Id&7u)));}
    F->Identifiers[Id>>3]|=(unsigned char)(1u<<(Id&7u));++F->Reserved;*Identifier=Id;return XMQTT_ADMISSION_ACCEPTED;
}
static void xmqttIdentifierReleaseLocked(xmqttclient* C,uint16 Id)
{
    if(Id){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
        F->Identifiers[Id>>3]&=(unsigned char)~(1u<<(Id&7u));--F->Reserved;}
}
static void xmqttRequestMapInsert(xmqttclient* C,xmqttrequest* R)
{
    if(R->Identifier){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);size_t Index=R->Identifier&(F->Capacity-1u);
        R->HashNext=F->Buckets[Index];F->Buckets[Index]=R;}
}
static void xmqttRequestMapRemove(xmqttclient* C,xmqttrequest* R)
{
    if(R->Identifier){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
        xmqttrequest** Link=F->Buckets+(R->Identifier&(F->Capacity-1u));while(*Link && *Link!=R)Link=&(*Link)->HashNext;
        if(*Link)*Link=R->HashNext;}
}
static xmqttrequest* xmqttRequestFind(xmqttclient* C,uint16 Id)
{
    xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);if(!F)return NULL;
    xmqttrequest* R=F->Buckets[Id&(F->Capacity-1u)];while(R && R->Identifier!=Id)R=R->HashNext;return R;
}
static void xmqttStartupDrop(xmqttstartup* W)
{
    if(!W || xrtAtomic32FetchSub(&W->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=W->Client;size_t Bytes=sizeof(*W)+W->Bytes;xrtSecureZero(W->Data,W->Bytes);xrtFree(W);
    xrtMutexLock(&C->Gate);C->Info.StartupBytes=0;C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
}
static void xmqttStartupRelease(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;(void)Size;xmqttStartupDrop(Data);}
static void xmqttStartupRetire(xmqttclient* C)
{
    xmqttstartup* W=C->Startup;C->Startup=NULL;memset(&C->SetupPacket,0,sizeof(C->SetupPacket));
    if(C->SetupProps)memset(C->SetupProps,0,(size_t)(C->Props-C->SetupProps)*sizeof(*C->SetupProps));
    xmqttStartupDrop(W);
}
static void xmqttReadyFinish(xmqttclient* C,xmqttclientstatus Status)
{
    if(!C->ReadyPromise)return;
    xmqttreadybox* B=C->ReadyBox;xpromise* P=C->ReadyPromise;C->ReadyBox=NULL;C->ReadyPromise=NULL;
    B->Value.Status=Status;if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
    if(!xrtPromiseResolveOwned(P,B,xmqttReadyFree,NULL))xmqttReadyFree(B,NULL);
    xrtPromiseDestroy(P);xmqttStartupRetire(C);xmqttClientNotify(C);
}
static size_t xmqttSendAliasPadding(size_t Bytes)
{return (0u-Bytes)&(_Alignof(xmqttsendaliasrequest)-1u);}
static xmqttsendaliasrequest* xmqttSendAliasRequest(const xmqttrequest* R)
{return (xmqttsendaliasrequest*)(R->Wire+R->Bytes+xmqttSendAliasPadding(R->Bytes));}
static size_t xmqttRequestExtra(size_t Bytes,bool Alias,bool TLS)
{
    size_t N=Alias?xmqttSendAliasPadding(Bytes)+sizeof(xmqttsendaliasrequest):0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(TLS)N+=((0u-Bytes-N)&(_Alignof(xmqtttlsfence)-1u))+sizeof(xmqtttlsfence);
#else
    (void)TLS;
#endif
    return N;
}
static bool xmqttClientUsesTls(const xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    return C->TLS!=NULL;
#else
    (void)C;return false;
#endif
}
static size_t xmqttRequestWireBytes(const xmqttrequest* R)
{if(R->Capabilities&XMQTT_TX_ALIAS){size_t Bytes=xmqttSendAliasRequest(R)->FrameBytes;if(Bytes)return Bytes;}return R->Bytes;}
static void xmqttSendTopicDropLocked(xmqttclient* C,xmqttsendtopic* T)
{if(T && !--T->Refs){C->Info.PrivateBytes-=sizeof(*T)+T->Size;xrtFree(T);}}
static void* xmqttSendAliasAllocateLocked(xmqttclient* C,size_t Bytes,xmqttclientadmission* Admission)
{
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){*Admission=XMQTT_ADMISSION_OVERLOADED;return NULL;}
    void* P=xrtCalloc(1,Bytes);if(!P){*Admission=XMQTT_ADMISSION_ERROR;return NULL;}C->Info.PrivateBytes+=Bytes;return P;
}
/* Planned state linearizes only with successful public admission. Actual state
 * is worker-owned and describes accepted TCP packet order, not broker ACKs. */
static xmqttclientadmission xmqttSendAliasSnapshotLocked(xmqttclient* C,xmqttrequest* R)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttclientadmission Admission=XMQTT_ADMISSION_ACCEPTED;
    size_t H=1;while(R->Wire[H++]&128u){}A->RawHeader=(uint8)H;
    size_t Topic=((size_t)R->Wire[H]<<8)|R->Wire[H+1];A->TailOffset=H+2+Topic;
    size_t Page=A->Alias>>8,Index=A->Alias&255u;
    if(!Topic && (!C->SendAliases || !C->SendAliases->Pages[Page] || !C->SendAliases->Pages[Page][Index].Planned)){
        xmqttClientError(XERR_STATE,"empty outgoing alias has no admitted topic");return XMQTT_ADMISSION_ERROR;}
    if(!C->SendAliases){C->SendAliases=xmqttSendAliasAllocateLocked(C,sizeof(*C->SendAliases),&Admission);if(!C->SendAliases)return Admission;}
    if(!C->SendAliases->Pages[Page]){C->SendAliases->Pages[Page]=xmqttSendAliasAllocateLocked(C,256*sizeof(xmqttsendaliasslot),&Admission);if(!C->SendAliases->Pages[Page])return Admission;}
    A->Slot=C->SendAliases->Pages[Page]+Index;xmqttsendtopic* T=A->Slot->Planned;
    if(Topic && (!T || Topic!=T->Size || memcmp(T->Data,R->Wire+H+2,Topic))){
        T=xmqttSendAliasAllocateLocked(C,sizeof(*T)+Topic,&Admission);if(!T)return Admission;T->Refs=1;T->Size=Topic;memcpy(T->Data,R->Wire+H+2,Topic);
    }else ++T->Refs;
    A->Topic=T;return XMQTT_ADMISSION_ACCEPTED;
}
static void xmqttSendAliasCommitLocked(xmqttclient* C,xmqttrequest* R)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsendtopic* Old=A->Slot->Planned;
    ++A->Topic->Refs;A->Slot->Planned=A->Topic;xmqttSendTopicDropLocked(C,Old);
}
static void xmqttRequestRef(xmqttrequest* R)
{(void)xrtAtomic32FetchAdd(&R->Refs,1,XMEMORY_RELAXED);}
static void xmqttRequestDrop(const void* Data)
{
    xmqttrequest* R=(xmqttrequest*)Data;if(xrtAtomic32FetchSub(&R->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=R->Client;size_t Bytes=R->Bytes;uint16 Id=R->Identifier;xrtCancelDestroy(R->Cancel);
    xrtMutexLock(&C->Gate);
    if(R->Capabilities&XMQTT_TX_ALIAS){xmqttSendTopicDropLocked(C,xmqttSendAliasRequest(R)->Topic);C->Info.PrivateBytes-=xmqttSendAliasPadding(Bytes)+sizeof(xmqttsendaliasrequest);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(R->Capabilities&XMQTT_TX_TLS)C->Info.PrivateBytes-=xmqttRequestExtra(Bytes,(R->Capabilities&XMQTT_TX_ALIAS)!=0,true)-xmqttRequestExtra(Bytes,(R->Capabilities&XMQTT_TX_ALIAS)!=0,false);
#endif
    xmqttIdentifierReleaseLocked(C,Id);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);xrtFree(R);xmqttClientDrop(C);
}
static bool xmqttRequestRetainProducer(const void* Data)
{xmqttRequestRef((xmqttrequest*)Data);return true;}
static void xmqttRequestCancel(const void* Data)
{xmqttClientWake(((xmqttrequest*)Data)->Client);}
static void xmqttReadyCancel(const void* Data)
{xmqttclient* C=(xmqttclient*)Data;xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);xmqttClientWake(C);}
static const xfutureproducerownershipv1 xmqttRequestProducer={sizeof(xfutureproducerownershipv1),xmqttRequestDrop};
static const xfutureproducercancellationv1 xmqttRequestCancellation={sizeof(xfutureproducercancellationv1),xmqttRequestRetainProducer,xmqttRequestCancel};
static const xfutureproducerownershipv1 xmqttReadyProducer={sizeof(xfutureproducerownershipv1),xmqttClientDrop};
static const xfutureproducercancellationv1 xmqttReadyCancellation={sizeof(xfutureproducercancellationv1),xmqttClientRetainProducer,xmqttReadyCancel};
static void xmqttRequestNativeDrop(ptr Data,cbytes Bytes,size_t Size)
{
    (void)Bytes;(void)Size;xmqttrequest* R=Data;
    (void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_ACQ_REL);xmqttClientWake(R->Client);xmqttRequestDrop(R);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static xmqtttlsfence* xmqttTlsFence(const xmqttrequest* R)
{
    size_t At=R->Bytes+xmqttRequestExtra(R->Bytes,(R->Capabilities&XMQTT_TX_ALIAS)!=0,true)-sizeof(xmqtttlsfence);
    return (xmqtttlsfence*)(R->Wire+At);
}
static void xmqttTlsUnlink(xmqttclient* C,xmqttrequest* R)
{
    xmqtttlsfence* F=xmqttTlsFence(R);xmqtttlsstate* T=C->TLS;
    if(F->Previous)xmqttTlsFence(F->Previous)->Next=F->Next;else T->Head=F->Next;
    if(F->Next)xmqttTlsFence(F->Next)->Previous=F->Previous;else T->Tail=F->Previous;
    F->Previous=F->Next=NULL;
}
static void xmqttTlsAppend(xmqttclient* C,xmqttrequest* R,uint64 Cut)
{
    xmqtttlsstate* T=C->TLS;xmqtttlsfence* F=xmqttTlsFence(R);F->Cut=Cut;F->Previous=T->Tail;F->Next=NULL;
    if(T->Tail)xmqttTlsFence(T->Tail)->Next=R;else T->Head=R;T->Tail=R;
}
static void xmqttTlsReleaseFence(xmqttclient* C,xmqttrequest* R)
{
    xmqttTlsUnlink(C,R);(void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_RELEASE);xmqttRequestDrop(R);
}
static void xmqttTlsProgress(xmqttclient* C)
{
    xmqtttlsstate* T=C->TLS;
    if(!T || !T->Head || !T->Stream || !T->Open || C->ReadEnded || C->NativeClosed || C->ApplicationNativeClosing || C->DrainClosing || xmqttClientClosing(C))return;
    xtlswriteprogress W={sizeof(W),0,0};
    if(!xrtTlsStreamWriteProgress(T->Stream,&W)){xmqttTlsFail(C,XTLS_ERROR);return;}
    while(T->Head && xmqttTlsFence(T->Head)->Cut<=W.Completed)xmqttTlsReleaseFence(C,T->Head);
}
#endif
/* Raw TCP keeps its existing per-segment leases. TLS copies plaintext into its
 * authenticated record buffers and holds one actual request credit until the
 * saved ciphertext prefix retires, independently of early protocol ACKs. */
static xnetresult xmqttRequestSend(xmqttclient* C,xmqttrequest* R,const xnetref* Refs,size_t Count,size_t* Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(!C->TLS->Bytes || !C->TLS->Calls){*Size=0;xmqttClientWake(C);return XNET_RESULT_AGAIN;}
        bool Added=!xrtAtomic32Load(&R->NativeRefs,XMEMORY_RELAXED);
        if(Added){xmqttRequestRef(R);xrtAtomic32Store(&R->NativeRefs,1,XMEMORY_RELAXED);xmqttTlsAppend(C,R,UINT64_MAX);}
        xnetspan Spans[3];size_t Left=C->TLS->Bytes,SpanCount=0;
        for(size_t I=0;I<Count && Left;++I){size_t N=Refs[I].Size;if(N>Left)N=Left;Spans[SpanCount++]=(xnetspan){Refs[I].Data,N};Left-=N;}
        size_t N=0;--C->TLS->Calls;xtlsresult S=xrtTlsStreamSendVec(C->TLS->Stream,Spans,SpanCount,&N);*Size=N;C->TLS->Bytes-=N;
        if(N && C->TLS->Stream){xtlswriteprogress W={sizeof(W),0,0};
            if(xrtTlsStreamWriteProgress(C->TLS->Stream,&W)){xmqttTlsUnlink(C,R);xmqttTlsAppend(C,R,W.Produced);}
            else xmqttTlsFail(C,XTLS_ERROR);
        }else if(!N && Added && xrtAtomic32Load(&R->NativeRefs,XMEMORY_RELAXED))xmqttTlsReleaseFence(C,R);
        if(S!=XTLS_OK && S!=XTLS_AGAIN){xmqttTlsFail(C,S);return N?XNET_RESULT_OK:XNET_RESULT_ERROR;}
        return N?XNET_RESULT_OK:XNET_RESULT_AGAIN;
    }
#endif
    for(size_t I=0;I<Count;++I)xmqttRequestRef(R);
    (void)xrtAtomic32FetchAdd(&R->NativeRefs,(uint32)Count,XMEMORY_RELAXED);
    xnetresult S=Count==1?xrtNetStreamSendRef(C->Stream,Refs[0].Data,Refs[0].Size,xmqttRequestNativeDrop,R):xrtNetStreamSendRefs(C->Stream,Refs,Count);
    if(S!=XNET_RESULT_OK){(void)xrtAtomic32FetchSub(&R->NativeRefs,(uint32)Count,XMEMORY_RELAXED);for(size_t I=0;I<Count;++I)xmqttRequestDrop(R);}
    (void)Size;return S;
}
static void xmqttRequestFinish(xmqttclient* C,xmqttrequest** Link,xmqttclientstatus Status)
{
    xmqttrequest* R=*Link;*Link=R->Next;xpromise* P=R->Promise;
    if(C->Partial==R)C->Partial=NULL;
    if(R->Next)R->Next->Previous=R->Previous;else C->RequestTail=R->Previous;
    xmqttRequestMapRemove(C,R);if(R->Credit)--C->SendInflight;
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    if(xmqttRequestSubscription(R)){
        xmqttsubscriptionbox* B=R->Result.Subscription;
        B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Identifier=R->Identifier;
        if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
        if(!xrtPromiseResolveOwned(P,B,xmqttSubscriptionFree,NULL))xmqttSubscriptionFree(B,NULL);
    }else {
        xmqttwritebox* B=R->Result.Write;
        B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Delivery=R->Offset?XMQTT_MAY_HAVE_REACHED_PEER:XMQTT_NOT_SENT;
        B->Value.Identifier=R->Identifier;
        if(R->Acknowledged)B->Value.Delivery=B->Value.Reason<0x80?XMQTT_PEER_ACCEPTED:XMQTT_PEER_REJECTED;
        if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
        if(!xrtPromiseResolveOwned(P,B,xmqttWriteFree,NULL))xmqttWriteFree(B,NULL);
    }
    xrtPromiseDestroy(P);xmqttRequestDrop(R);xmqttClientNotify(C);
}
static void xmqttClientFailReason(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause,uint8 Reason)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XMQTT_CLIENT_CLOSING || (C->ApplicationClose && C->Info.State==XMQTT_CLIENT_CLOSING && C->Info.CloseStatus==XMQTT_STATUS_OK)){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=Status;
        if(Status==XMQTT_STATUS_PROTOCOL)++C->Info.ProtocolErrors;
        if(Cause)C->Cause=xrtErrorRef(Cause);
        if(!C->ApplicationClose && C->Config.Version==XMQTT_VERSION_5 && C->Info.Ready)C->Info.LocalReason=Reason;
    }
    xrtMutexUnlock(&C->Gate);
}
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause)
{xmqttClientFailReason(C,Status,Cause,Status==XMQTT_STATUS_PROTOCOL?0x82:0);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* A retained cancellation dispatch may outlive its exchange. Only wake the
 * owner; it samples the CURRENT exchange's token, never a stale generation. */
static void xmqttAuthCancel(const void* Data)
{xmqttClientWake((xmqttclient*)Data);}
static const xfutureproducercancellationv1 xmqttAuthCancellation={sizeof(xfutureproducercancellationv1),xmqttClientRetainProducer,xmqttAuthCancel};
static xmqttclientadmission xmqttAuthDecisionTimeout(xmqttclient* C)
{
    xmqttClientError(XERR_TIMEOUT,"authentication decision deadline expired");
    xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,xrtGetError());xmqttClientWake(C);return XMQTT_ADMISSION_ERROR;
}
static bool xmqttAuthMethod(const xmqttauthstate* A,const xmqttpacket* P)
{
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_METHOD){
        xstrview V=P->Properties.Data[I].Value.Text;
        return V.Size==A->Method.Size && (!V.Size || !memcmp(V.Data,A->Method.Data,V.Size));
    }
    return false;
}
XMQTT_API bool xmqttAuthConfigInit(xmqttauthconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication config output");
    *K=(xmqttauthconfig){sizeof(*K),32,1024u*1024u,5000};return true;
}
static void xmqttAuthResultFree(ptr Value,ptr Context)
{
    (void)Context;xmqttauthbox* B=Value;xmqttMessageRelease(B->Value.Final);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);
}
static void xmqttAuthWireDrop(xmqttauthwire* W)
{
    if(!W || xrtAtomic32FetchSub(&W->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=W->Client;size_t Bytes=sizeof(*W)+W->Bytes;xrtSecureZero(W->Wire,W->Bytes);xrtFree(W);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
}
static void xmqttAuthWireRelease(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;(void)Size;xmqttauthwire* W=Data;xmqttClientWake(W->Client);xmqttAuthWireDrop(W);}
static xmqttauthwire* xmqttAuthWireAllocate(xmqttclient* C,const xmqttpacket* P,size_t Bytes,xmqttclientadmission* Admission)
{
    xrtMutexLock(&C->Gate);
    if(Bytes>SIZE_MAX-sizeof(xmqttauthwire) || sizeof(xmqttauthwire)+Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        xrtMutexUnlock(&C->Gate);*Admission=XMQTT_ADMISSION_OVERLOADED;return NULL;}
    C->Info.PrivateBytes+=sizeof(xmqttauthwire)+Bytes;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttauthwire* W=xrtMalloc(sizeof(*W)+Bytes);
    if(!W){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=sizeof(xmqttauthwire)+Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return NULL;}
    memset(W,0,sizeof(*W));W->Client=C;W->Bytes=Bytes;xrtAtomic32Init(&W->Refs,1);
    if(!xmqttPacketWrite5(W->Wire,Bytes,P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)){xmqttAuthWireDrop(W);return NULL;}
    return W;
}
XMQTT_API void xmqttAuthRelease(xmqttauthticket* T)
{
    if(T && xrtAtomic32FetchSub(&T->Refs,1,XMEMORY_ACQ_REL)==1){xmqttclient* C=T->Client;xmqttMessageRelease(T->Message);xmqttBoxFree(T,T->Box);xmqttClientDrop(C);}
}
XMQTT_API xmqttauthphase xmqttAuthPhase(const xmqttauthticket* T)
{return T?T->Phase:XMQTT_AUTH_CHALLENGE;}
XMQTT_API const xmqttmessage* xmqttAuthMessage(const xmqttauthticket* T)
{return T?T->Message:NULL;}
XMQTT_API bool xmqttClientTakeAuth(xmqttclient* C,xmqttauthticket** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication ticket output");
    xrtMutexLock(&C->Gate);xmqttauthticket* T=C->Auth?C->Auth->Ticket:NULL;
    if(T && !T->Taken && C->Info.State<XMQTT_CLIENT_CLOSING){T->Taken=true;(void)xrtAtomic32FetchAdd(&T->Refs,1,XMEMORY_RELAXED);}else T=NULL;
    xrtMutexUnlock(&C->Gate);if(!T)return false;*Out=T;return true;
}
/* Worker-owned receipt; the mailbox and decision publication share Gate. */
static bool xmqttAuthOffer(xmqttclient* C,xmqttmessage* M,xmqttauthphase Phase)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttauthticket* T=xmqttBoxAllocate(C->Quota,sizeof(*T),&Failure);
    if(!T){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    T->Box=(xmqttbox){C->Quota,sizeof(*T)};T->Client=C;T->Message=M;T->Phase=Phase;xrtAtomic32Init(&T->Refs,1);
    xmqttClientRef(C);(void)xmqttMessageRetain(M);
    xrtMutexLock(&C->Gate);C->Auth->Ticket=T;C->Auth->AwaitServer=false;C->Auth->Decision=0;
    C->Auth->StepDeadline=xmqttClientDeadline(xrtTimer(),C->Auth->Config.ResponseTimeout);xrtMutexUnlock(&C->Gate);
    xmqttClientNotify(C);return true;
}
static bool xmqttAuthCharge(xmqttclient* C,size_t Bytes,bool Challenge)
{
    xrtMutexLock(&C->Gate);xmqttauthstate* A=C->Auth;
    bool Fits=Bytes<=A->Config.MaxExchangeBytes-A->Bytes && (!Challenge || A->Rounds<A->Config.MaxRounds);
    if(Fits){A->Bytes+=Bytes;if(Challenge)++A->Rounds;}xrtMutexUnlock(&C->Gate);
    if(!Fits)xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);
    return Fits;
}
static bool xmqttAuthFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);bool Expected=A && A->Active && A->AwaitServer && !A->Ticket && (!A->Out || A->Out->Offset==A->Out->Bytes) && (A->ProtocolReady || (C->Startup && C->SetupOffset==C->Startup->Bytes));
    bool Reauth=A && A->Reauth;xrtMutexUnlock(&C->Gate);
    if(!Expected || !xmqttAuthMethod(A,P) || (P->Reason!=0x18 && !(Reauth && P->Reason==0))){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!C->ProblemInfo)for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_REASON_STRING || P->Properties.Data[I].Id==XMQTT_PROPERTY_USER){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!xmqttAuthCharge(C,P->Wire.Size,P->Reason==0x18))return false;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,NULL);
    if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    if(!P->Reason){A->Result->Value.Final=M;return xmqttAuthOffer(C,M,XMQTT_AUTH_VERIFY_REAUTH);}
    bool OK=xmqttAuthOffer(C,M,XMQTT_AUTH_CHALLENGE);xmqttMessageRelease(M);return OK;
}
static bool xmqttAuthConnack(xmqttclient* C,xmqttmessage* M)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);bool Expected=A->Active && !A->Reauth && A->AwaitServer && !A->Ticket;xrtMutexUnlock(&C->Gate);
    if(!Expected || !xmqttAuthMethod(A,&M->Packet)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!xmqttAuthCharge(C,M->Packet.Wire.Size,false))return false;
    xrtMutexLock(&C->Gate);A->ProtocolReady=true;xrtMutexUnlock(&C->Gate);
    /* Negotiated views already belong to Ready's owned CONNACK/id snapshot.
     * Local proof verification needs no retained CONNECT credentials. */
    xmqttStartupRetire(C);return xmqttAuthOffer(C,M,XMQTT_AUTH_VERIFY_CONNECT);
}
static void xmqttAuthFinish(xmqttclient* C,xmqttclientstatus Status)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);xpromise* P=A->Promise;xmqttauthbox* B=A->Result;xcancel* Cancel=A->Cancel;
    A->Promise=NULL;A->Result=NULL;A->Cancel=NULL;A->Active=false;A->Reauth=false;A->StepDeadline=INFINITY;
    if(B){B->Value.Status=Status;B->Value.AcceptedBytes=A->Sent;if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);}
    xrtMutexUnlock(&C->Gate);
    xrtCancelDestroy(Cancel);if(P){if(!xrtPromiseResolveOwned(P,B,xmqttAuthResultFree,NULL))xmqttAuthResultFree(B,NULL);xrtPromiseDestroy(P);xmqttClientNotify(C);}
}
static void xmqttAuthWork(xmqttclient* C)
{
    xmqttauthstate* A=C->Auth;if(!A)return;
    xrtMutexLock(&C->Gate);xmqttauthticket* T=A->Ticket;int Decision=A->Decision;bool Closing=C->Info.State>=XMQTT_CLIENT_CLOSING;
    if(T && T->Answered){A->Ticket=NULL;A->StepDeadline=INFINITY;}else T=NULL;
    bool Cancel=A->Cancel && xrtCancelRequested(A->Cancel);double At=fmin(A->Deadline,A->StepDeadline);bool Active=A->Active;bool Expired=Active && xrtTimer()>=At;
    /* Reauth stays reserved until Finish atomically detaches its producer/result.
     * Otherwise a new admission can overwrite the still-pending old exchange. */
    if(T && Decision>0 && !Closing && !Cancel && !Expired && T->Phase==XMQTT_AUTH_VERIFY_CONNECT){A->Active=false;A->Verified=true;C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;}
    xrtMutexUnlock(&C->Gate);
    if(!Closing && Cancel)xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    if(!Closing && Expired)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(T){if(Decision<0)xmqttClientFailReason(C,XMQTT_STATUS_AUTHENTICATION,NULL,0x80);
        else if(Decision>0 && !Closing && !Cancel && !Expired){if(T->Phase==XMQTT_AUTH_VERIFY_CONNECT)xmqttReadyFinish(C,XMQTT_STATUS_OK);else xmqttAuthFinish(C,XMQTT_STATUS_OK);}
        xmqttAuthRelease(T);}
}
static bool xmqttAuthSend(xmqttclient* C,size_t* Budget)
{
    xmqttauthstate* A=C->Auth;if(!A)return true;
    xrtMutexLock(&C->Gate);xmqttauthwire* W=A->Out;xrtMutexUnlock(&C->Gate);
    while(W && W->Offset<W->Bytes && *Budget){size_t N=W->Bytes-W->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xmqttTransportSendRef(C,W->Wire+W->Offset,&N,xmqttAuthWireRelease,W);
        if(S!=XNET_RESULT_OK){xmqttAuthWireDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        W->Offset+=N;*Budget-=N;xrtMutexLock(&C->Gate);A->Sent+=N;xrtMutexUnlock(&C->Gate);
        double SentAt=xrtTimer();if(!A->ProtocolReady)C->PingDeadline=SentAt;
        if(C->Peer.KeepAlive)C->NextPing=SentAt+C->Peer.KeepAlive;
    }
    if(W && W->Offset<W->Bytes){xmqttClientWake(C);return false;}
    if(W){xrtMutexLock(&C->Gate);A->Out=NULL;A->AwaitServer=true;xrtMutexUnlock(&C->Gate);xmqttAuthWireDrop(W);}return true;
}
/* Stop releases only the client's mailbox/output owners. Caller/native duties
 * still pin Client and remain visible to physical WaitFor. */
static bool xmqttAuthPartial(xmqttclient* C)
{
    if(!C->Auth)return false;
    xrtMutexLock(&C->Gate);xmqttauthwire* W=C->Auth?C->Auth->Out:NULL;bool Partial=W && W->Offset && W->Offset<W->Bytes;xrtMutexUnlock(&C->Gate);return Partial;
}
static double xmqttAuthDeadline(xmqttclient* C)
{
    if(!C->Auth)return INFINITY;
    xrtMutexLock(&C->Gate);xmqttauthstate* A=C->Auth;double At=A && A->Active?fmin(A->Deadline,A->StepDeadline):INFINITY;xrtMutexUnlock(&C->Gate);return At;
}
static bool xmqttAuthStop(xmqttclient* C)
{
    xmqttauthstate* A=C->Auth;if(!A)return false;
    xrtMutexLock(&C->Gate);xmqttauthticket* T=A->Ticket;xmqttauthwire* W=A->Out;
    A->Ticket=NULL;A->Out=NULL;bool Partial=W && W->Offset && W->Offset<W->Bytes;xrtMutexUnlock(&C->Gate);
    xmqttAuthFinish(C,C->Info.CloseStatus);xmqttAuthRelease(T);xmqttAuthWireDrop(W);return Partial;
}
XMQTT_API xmqttclientadmission xmqttAuthComplete(xmqttauthticket* T,bool Accept)
{
    if(!T){xmqttClientError(XERR_ARGUMENT,"authentication ticket is NULL");return XMQTT_ADMISSION_ERROR;}
    xmqttclient* C=T->Client;xmqttauthstate* A=C->Auth;xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(A->Ticket!=T || !T->Taken || T->Answered || (Accept && T->Phase==XMQTT_AUTH_CHALLENGE)){
        xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"authentication ticket cannot accept this decision");return XMQTT_ADMISSION_ERROR;}
    if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline)){xrtMutexUnlock(&C->Gate);return xmqttAuthDecisionTimeout(C);}
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_ERROR;}
    T->Answered=true;A->Decision=Accept?1:-1;A->StepDeadline=INFINITY;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_ACCEPTED;
}
XMQTT_API xmqttclientadmission xmqttAuthRespond(xmqttauthticket* T,const xmqttpacket* P)
{
    if(!T || !P || !xrtMemRangeValid(P,sizeof(*P)) || P->Type!=XMQTT_PACKET_AUTH || P->Reason!=0x18){xmqttClientError(XERR_ARGUMENT,"expected authentication continuation");return XMQTT_ADMISSION_ERROR;}
    xmqttclient* C=T->Client;xmqttauthstate* A=C->Auth;size_t Bytes=0;
    if(!xmqttPacketSize5(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes))return XMQTT_ADMISSION_ERROR;
    if(!xmqttAuthMethod(A,P)){xmqttClientError(XERR_ARGUMENT,"authentication method changed");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(A->Ticket!=T || !T->Taken || T->Answered || T->Phase!=XMQTT_AUTH_CHALLENGE){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"authentication ticket is stale");return XMQTT_ADMISSION_ERROR;}
    if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline)){xrtMutexUnlock(&C->Gate);return xmqttAuthDecisionTimeout(C);}
    if(A->Out || Bytes>A->Config.MaxExchangeBytes-A->Bytes){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    xrtMutexUnlock(&C->Gate);
    xmqttclientadmission S=XMQTT_ADMISSION_ERROR;xmqttauthwire* W=xmqttAuthWireAllocate(C,P,Bytes,&S);if(!W)return S;
    bool Late=false;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING)S=XMQTT_ADMISSION_CLOSED;
    else if(A->Ticket!=T || !T->Taken || T->Answered || T->Phase!=XMQTT_AUTH_CHALLENGE){xmqttClientError(XERR_STATE,"authentication ticket is stale");}
    else if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline))Late=true;
    else if(Bytes>A->Config.MaxExchangeBytes-A->Bytes || A->Out)S=XMQTT_ADMISSION_OVERLOADED;
    else if(A->ProtocolReady && Bytes>C->Peer.SendPacketBytes){xmqttClientError(XERR_RANGE,"AUTH exceeds peer packet limit");}
    else if(xmqttClientWakeLocked(C)){A->Out=W;A->Bytes+=Bytes;T->Answered=true;A->Decision=0;A->StepDeadline=INFINITY;S=XMQTT_ADMISSION_ACCEPTED;}
    xrtMutexUnlock(&C->Gate);if(S!=XMQTT_ADMISSION_ACCEPTED)xmqttAuthWireDrop(W);
    return Late?xmqttAuthDecisionTimeout(C):S;
}
XMQTT_API xmqttclientadmission xmqttClientReauthenticateAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    double Deadline=Timeout?xmqttClientDeadline(xrtTimer(),Timeout):-INFINITY;
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<0 || P->Type!=XMQTT_PACKET_AUTH || P->Reason!=0x19){xmqttClientError(XERR_ARGUMENT,"invalid reauthentication request");return XMQTT_ADMISSION_ERROR;}
    xmqttauthstate* A=C->Auth;if(!A){xmqttClientError(XERR_STATE,"connection has no enhanced authentication");return XMQTT_ADMISSION_ERROR;}
    size_t Bytes=0;if(!xmqttPacketSize5(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes))return XMQTT_ADMISSION_ERROR;
    if(!xmqttAuthMethod(A,P)){xmqttClientError(XERR_ARGUMENT,"reauthentication method changed");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(!A->Verified){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"initial authentication is not verified");return XMQTT_ADMISSION_ERROR;}
    if(A->Active || A->Out || C->DisconnectReserved || Bytes>A->Config.MaxExchangeBytes){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    xrtMutexUnlock(&C->Gate);
    xmqttclientadmission S=XMQTT_ADMISSION_ERROR;xmqttauthwire* W=xmqttAuthWireAllocate(C,P,Bytes,&S);if(!W)return S;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttauthbox* B=xmqttBoxAllocate(C->Quota,sizeof(*B),&Failure);xfuture* F=NULL;xpromise* Promise=NULL;xcancel* Cancel=NULL;
    if(!B){if(Failure==XMQTT_STATUS_LIMIT)S=XMQTT_ADMISSION_OVERLOADED;goto Done;}
    B->Box=(xmqttbox){C->Quota,sizeof(*B)};Promise=xrtPromiseCreate(&F,NULL);if(!Promise)goto Done;
    xmqttClientRef(C);if(!xrtPromiseProducerBindTakeV2(Promise,(xrtownershipref){C,&xmqttPrivateOps},&xmqttReadyProducer,&xmqttAuthCancellation)){xmqttClientDrop(C);goto Done;}
    Cancel=xrtPromiseCancelToken(Promise);if(!Cancel)goto Done;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING)S=XMQTT_ADMISSION_CLOSED;
    else if(!A->Verified){xmqttClientError(XERR_STATE,"initial authentication is not verified");}
    else if(A->Active || A->Out || C->DisconnectReserved || Bytes>A->Config.MaxExchangeBytes)S=XMQTT_ADMISSION_OVERLOADED;
    else if(Bytes>C->Peer.SendPacketBytes){xmqttClientError(XERR_RANGE,"AUTH exceeds peer packet limit");}
    else if(xmqttClientWakeLocked(C)){A->Active=A->Reauth=true;A->AwaitServer=false;A->Out=W;A->Promise=Promise;A->Cancel=Cancel;A->Result=B;
        A->Rounds=A->Sent=0;A->Bytes=Bytes;A->Deadline=Deadline;A->StepDeadline=INFINITY;S=XMQTT_ADMISSION_ACCEPTED;}
    xrtMutexUnlock(&C->Gate);
Done:
    if(S==XMQTT_ADMISSION_ACCEPTED){*Out=F;return S;}
    xrtCancelDestroy(Cancel);xrtPromiseDestroy(Promise);xrtFutureDestroy(F);if(B)xmqttAuthResultFree(B,NULL);xmqttAuthWireDrop(W);return S;
}
XMQTT_API xmqttclientadmission xmqttClientReauthenticate(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously authenticate from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientReauthenticateAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
#endif

static void xmqttClientOpen(xnetstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttClientRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)S;(void)B;xmqttClientWake(Data);}
static void xmqttClientEnd(xnetstream* S,ptr Data)
{(void)S;xmqttclient* C=Data;C->ReadEnded=true;xmqttClientWake(C);}
static void xmqttClientLow(xnetstream* S,size_t Bytes,ptr Data)
{(void)S;(void)Bytes;xmqttClientWake(Data);}
static void xmqttClientDrain(xnetstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttClientClosed(xnetstream* S,xnetresult Result,const xerror* Cause,ptr Data)
{(void)S;xmqttclient* C=Data;C->NativeClosed=true;
    xrtMutexLock(&C->Gate);bool Expected=C->ApplicationClose && C->ApplicationNativeClosing && Result==XNET_RESULT_OK;xrtMutexUnlock(&C->Gate);
    if(!Expected)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,Cause);
    xmqttClientWake(C);
}
static const xnetstreameventsownershipv1 xmqttClientEvents={sizeof(xnetstreameventsownershipv1),
    {xmqttClientOpen,xmqttClientRead,xmqttClientEnd,NULL,xmqttClientLow,xmqttClientDrain,xmqttClientClosed},xmqttClientDrop,&xmqttPrivateOps};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
static xmqttclientstatus xmqttDialFailure(xnetresult Result,const xerror* Cause)
{
    if(Result==XNET_RESULT_TIMEOUT)return XMQTT_STATUS_TIMEOUT;
    if(Result==XNET_RESULT_CANCELLED)return XMQTT_STATUS_CANCELLED;
    xerrkind Kind=Cause?xrtErrorKind(Cause):XERR_NONE;
    return Kind==XERR_MEMORY?XMQTT_STATUS_MEMORY:(Kind==XERR_RANGE || Kind==XERR_AGAIN)?XMQTT_STATUS_LIMIT:XMQTT_STATUS_TRANSPORT;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
static void xmqttTlsResumeIncrement(uint64* Value)
{if(*Value!=UINT64_MAX)++*Value;}
static bool xmqttTlsResumeBytes(xtlsresume* R,size_t* Bytes)
{
    xtlsresumeinfo I;if(!xrtTlsResumeInfo(R,&I))return false;
    const size_t Sizes[]={I.Ticket.Size,I.Secret.Size,I.ServerName.Size,I.Protocol.Size,I.PeerIdentity.Size};size_t N=0;
    for(size_t At=0;At<sizeof(Sizes)/sizeof(*Sizes);++At){if(Sizes[At]>SIZE_MAX-N)return false;N+=Sizes[At];}
    size_t Count=xrtTlsResumePeerCertificateCountV1(R);
    for(size_t At=0;At<Count;++At){const xx509cert* Cert=xrtTlsResumePeerCertificateV1(R,At);if(!Cert || Cert->Raw.Size>SIZE_MAX-N)return false;N+=Cert->Raw.Size;}
    *Bytes=N;return true;
}
/* Native already owns bounded immutable credentials. Move one reference; no
 * new node, credential copy, Work item or cross-thread native Session call. */
static void xmqttTlsTicket(xtlsstream* S,ptr Data)
{
    xmqttclient* C=Data;xmqtttlsresumestate* R=&C->TLS->Resume;xtlssession* Session=xrtTlsStreamSession(S);
    for(size_t At=0;At<XTLS_CLIENT_RESUME_LIMIT_MAX;++At){xtlsresume* Next=xrtTlsClientTakeResume(Session);if(!Next)break;
        size_t Bytes=0;bool Valid=xmqttTlsResumeBytes(Next,&Bytes);xtlsresume* Drop=Next;
        xrtMutexLock(&C->Gate);xmqttTlsResumeIncrement(&R->Info.Captured);
        if(Valid && R->Info.Enabled && Bytes<=C->Config.MaxPrivateBytes-C->Info.PrivateBytes+R->Info.CachedBytes){
            Drop=R->Value;if(Drop)xmqttTlsResumeIncrement(&R->Info.Evicted);
            C->Info.PrivateBytes-=R->Info.CachedBytes;C->Info.PrivateBytes+=Bytes;R->Value=Next;R->Info.CachedBytes=Bytes;
        }else xmqttTlsResumeIncrement(&R->Info.Rejected);
        R->Notify=true;xrtMutexUnlock(&C->Gate);xrtTlsResumeRelease(Drop);
    }
    xmqttClientWake(C);
}
static bool xmqttTlsResumeOutput(xmqttclient* C,ptr Out,size_t Bytes)
{
    if(!C || !Out || !xrtMemRangeValid(Out,Bytes) || !C->TLS ||
       xrtMemRangesOverlap(Out,Bytes,C,sizeof(*C)) || xrtMemRangesOverlap(Out,Bytes,C->TLS,sizeof(*C->TLS)))
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS resume client/output or owner overlap");
    size_t Scratch=(size_t)(C->Props-C->Scratch)+C->Config.Codec.MaxProperties;
    if(xrtMemRangesOverlap(Out,Bytes,C->Scratch,Scratch*sizeof(*C->Scratch)))return xmqttClientError(XERR_ARGUMENT,"TLS resume output overlaps client scratch");
    return true;
}
XMQTT_API bool xmqttClientTlsResumeInfo(xmqttclient* C,xmqttclienttlsresumeinfo* Out)
{
    if(!xmqttTlsResumeOutput(C,Out,sizeof(*Out)))return false;
    xrtMutexLock(&C->Gate);xmqttclienttlsresumeinfo I=C->TLS->Resume.Info;xrtMutexUnlock(&C->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientTlsTakeResume(xmqttclient* C,xtlsresume** Out)
{
    if(!xmqttTlsResumeOutput(C,Out,sizeof(*Out)))return false;
    xrtMutexLock(&C->Gate);xmqtttlsresumestate* R=&C->TLS->Resume;xtlsresume* Value=R->Value;
    if(Value){R->Value=NULL;C->Info.PrivateBytes-=R->Info.CachedBytes;R->Info.CachedBytes=0;}
    xrtMutexUnlock(&C->Gate);if(!Value)return false;*Out=Value;return true;
}
#endif
static void xmqttTlsWritten(xtlsstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttTlsOpen(xtlsstream* S,ptr Data)
{
    xmqttclient* C=Data;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool Resumed=xrtTlsClientResumed(xrtTlsStreamSession(S));
    xrtMutexLock(&C->Gate);C->TLS->Resume.Info.Established=true;C->TLS->Resume.Info.Resumed=Resumed;xrtMutexUnlock(&C->Gate);
#endif
    if(!xrtTlsStreamSetWriteProgress(S,xmqttTlsWritten))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());
    else C->TLS->Open=true;
    xmqttClientWake(C);
}
static void xmqttTlsRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{(void)S;(void)B;xmqttClientWake(Data);}
static void xmqttTlsEnd(xtlsstream* S,ptr Data)
{(void)S;xmqttclient* C=Data;C->ReadEnded=true;xmqttClientWake(C);}
static void xmqttTlsClosed(xtlsstream* S,xnetresult Result,const xerror* Cause,ptr Data)
{
    xmqttclient* C=Data;C->NativeClosed=true;C->TLS->Open=false;
    while(C->TLS->Head)xmqttTlsReleaseFence(C,C->TLS->Head);
    xrtMutexLock(&C->Gate);bool Expected=C->ApplicationClose && C->ApplicationNativeClosing && Result==XNET_RESULT_OK;xrtMutexUnlock(&C->Gate);
    if(!Expected)xmqttClientFail(C,xmqttDialFailure(Result,Cause),Cause);
    C->TLS->Stream=NULL;xmqttClientWake(C);
    /* Break the actual Client -> TLS -> Client cycle. Native/runtime references
     * retain TLS and its owned Client credit through final Session destruction.
     * The separate real TCP caller reference remains for WaitRetired. */
    xrtTlsStreamDestroy(S);
}
static const xtlsstreamownershipv1 xmqttTlsEvents={sizeof(xtlsstreamownershipv1),
    {xmqttTlsOpen,xmqttTlsRead,xmqttTlsEnd,xmqttTlsWritten,xmqttTlsWritten,xmqttTlsClosed,
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    xmqttTlsTicket
#else
    NULL
#endif
    },xmqttClientDrop,&xmqttPrivateOps};
static bool xmqttTlsAttach(xmqttclient* C,xnetstream* S)
{
    if(!xrtNetStreamRef(S))return false; /* Client's independent retirement handle. */
    xmqttClientRef(C);
    if(xrtTlsStreamAttachOwnedV1(S,C->TLS->Plan.Session,&C->TLS->Plan.Config,C,&xmqttTlsEvents,&C->TLS->Stream)){
        C->TLS->Plan.Session=NULL;return true;
    }
    xmqttClientDrop(C);xrtNetStreamDestroy(S);return false;
}
#endif
static void xmqttClientDialDone(xnetdial* D,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{
    xmqttclient* C=Data;
    /* Pair with factory publication before accessing its immutable setup. The
     * owning Worker serializes all subsequent transport/state operations. */
    xrtMutexLock(&C->Gate);C->Dial=NULL;C->Stream=Stream;xrtMutexUnlock(&C->Gate);
    if(Stream){
        if(xmqttClientClosing(C))(void)xrtNetStreamAbort(Stream);
        else {
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
            if(C->TLS){if(!xmqttTlsAttach(C,Stream)){xmqttClientFail(C,xmqttDialFailure(XNET_RESULT_ERROR,xrtGetError()),xrtGetError());(void)xrtNetStreamAbort(Stream);}}
            else
#endif
            {xmqttClientRef(C);
            if(!xrtNetStreamSetEventsOwnedV1(Stream,C,&xmqttClientEvents)){
                xmqttClientDrop(C);const xerror* Error=xrtNetStreamError(Stream);if(!Error)Error=xrtGetError();
                xmqttClientFail(C,xmqttDialFailure(XNET_RESULT_ERROR,Error),Error);(void)xrtNetStreamAbort(Stream);}
            else {xnetstreamstats I;
                if(!xrtNetStreamStats(Stream,&I))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());
                else if(I.ReadEnded){C->ReadEnded=true;xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);}}
            }
        }
    }else {C->NativeClosed=true;xmqttClientFail(C,xmqttDialFailure(Result,Cause),Cause);}
    /* A queued client turn also holds C through the physical Done/Drop tail.
     * Initially callback-less winners retain buffered input; that turn reads it. */
    xmqttClientWake(C);
    /* Consume the Client's caller reference after detaching its field. Core's
     * active Done invocation owns D through its callback tail. Successful
     * connections need no completed controller, candidates or old failures. */
    xrtNetDialDestroy(D);
}
static const xnetdialownershipv1 xmqttClientDialEvents={sizeof(xnetdialownershipv1),xmqttClientDialDone,xmqttClientDrop,&xmqttPrivateOps};
#endif
static void xmqttClientTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xmqttclient* C=Data;
    if(C->Timer==Id){C->Timer=0;C->TimerAt=INFINITY;if(Result==XNET_RESULT_OK)xmqttClientWake(C);}
    xmqttClientDrop(C);
}
static void xmqttClientSchedule(xmqttclient* C,double At)
{
    if(C->Timer && C->TimerAt<=At)return;
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    C->TimerAt=At;if(!isfinite(At))return;
    xmqttClientRef(C);C->Timer=xrtNetEngineAfter(C->Engine,xrtNetWorkerIndex(C->Worker),xmqttClientRemaining(At),xmqttClientTimer,C);
    if(!C->Timer){xmqttClientDrop(C);xmqttClientFail(C,XMQTT_STATUS_LIMIT,xrtGetError());xmqttClientWake(C);}
}
static bool xmqttClientGrow(xmqttclient* C,size_t Capacity)
{
    if(Capacity<=C->InputCapacity)return true;
    xrtMutexLock(&C->Gate);
    if(Capacity>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Capacity;xrtMutexUnlock(&C->Gate);
    unsigned char* P=xrtMalloc(Capacity);
    if(!P){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Capacity;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    if(C->InputBytes)memcpy(P,C->Input,C->InputBytes);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth)xrtSecureZero(C->Input,C->InputCapacity);
#endif
    xrtFree(C->Input);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=C->InputCapacity;xrtMutexUnlock(&C->Gate);
    C->Input=P;C->InputCapacity=Capacity;return true;
}
static void* xmqttAliasAllocate(xmqttclient* C,size_t Bytes,bool Zero)
{
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return NULL;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    void* Data=Zero?xrtCalloc(1,Bytes):xrtMalloc(Bytes);
    if(!Data){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());}
    return Data;
}
/* Called once per accepted PUBLISH, including active QoS2 duplicates. Only the
 * owner accesses this table. Messages never retain an entry or its Client. */
static bool xmqttReceiveAlias(xmqttclient* C,const xmqttpacket* P,xstrview* Resolved)
{
    uint16 Id=0;
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){Id=(uint16)P->Properties.Data[I].Value.Number;break;}
    if(!Id)return true; /* The codec rejects zero/duplicate alias properties. */
    if(Id>C->Peer.ReceiveAliasMaximum){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x94);return false;}
    xstrview Topic=P->Body.Publish.Topic;
    if(!Topic.Size){
        xmqttalias* A=C->ReceiveAliases?C->ReceiveAliases[Id-1]:NULL;
        if(!A){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x82);return false;}
        *Resolved=(xstrview){A->Topic,A->Size};return true;
    }
    if(!C->ReceiveAliases){C->ReceiveAliases=xmqttAliasAllocate(C,(size_t)C->Peer.ReceiveAliasMaximum*sizeof(*C->ReceiveAliases),true);if(!C->ReceiveAliases)return false;}
    xmqttalias* Old=C->ReceiveAliases[Id-1];
    if(Old && Topic.Size<=Old->Capacity){memcpy(Old->Topic,Topic.Data,Topic.Size);Old->Size=Topic.Size;return true;}
    xmqttalias* A=xmqttAliasAllocate(C,sizeof(*A)+Topic.Size,false);if(!A)return false;
    A->Size=A->Capacity=Topic.Size;memcpy(A->Topic,Topic.Data,Topic.Size);C->ReceiveAliases[Id-1]=A;
    if(Old){size_t Bytes=sizeof(*Old)+Old->Capacity;xrtFree(Old);xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);}
    return true;
}
static bool xmqttReceivePrepare(xmqttclient* C)
{
    if(C->ReceiveFlow)return true;
    size_t Capacity=C->Config.MaxMessages;
    if(Capacity>C->Peer.ReceiveMaximum)Capacity=C->Peer.ReceiveMaximum;
    size_t Bytes=sizeof(xmqttreceiveflow)+Capacity*sizeof(uint16);
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    xmqttreceiveflow* F=xrtCalloc(1,Bytes);
    if(!F){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    F->Capacity=Capacity;C->ReceiveFlow=F;return true;
}
static bool xmqttReceiveTwoPrepare(xmqttclient* C)
{
    if(C->ReceiveTwo)return true;
    size_t Capacity=C->Config.MaxMessages;
    if(Capacity>C->Peer.ReceiveMaximum)Capacity=C->Peer.ReceiveMaximum;
    Capacity*=2;size_t Bytes=sizeof(xmqttreceivetwo)+Capacity*sizeof(xmqttreceiveack);
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    xmqttreceivetwo* F=xrtCalloc(1,Bytes);
    if(!F){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    F->Capacity=Capacity;C->ReceiveTwo=F;return true;
}
static void xmqttReceiveTwoQueue(xmqttclient* C,uint16 Id,uint8 Type,bool Unknown)
{
    xmqttreceivetwo* F=C->ReceiveTwo;
    unsigned Flag=Type==0x50?XMQTT_RX_REC_QUEUED:Unknown?XMQTT_RX_UNKNOWN_QUEUED:XMQTT_RX_COMP_QUEUED;
    if(F->State[Id]&Flag)return;
    F->Acks[(F->Head+F->Count)%F->Capacity]=(xmqttreceiveack){Id,Type,Unknown};++F->Count;
    F->State[Id]|=(unsigned char)Flag;
}
/* Preflight before consuming the buffered frame. Queue exhaustion can always
 * be relieved by writing ACKs, whereas active ids retain receive credit. */
static bool xmqttReceiveRoom(xmqttclient* C,const xmqttpacket* P)
{
    if(!C->Info.Ready)return true;
    unsigned Qos=P->Type==XMQTT_PACKET_PUBLISH?(P->Flags>>1)&3u:0;
    if(!Qos && P->Type!=XMQTT_PACKET_PUBREL)return true;
    unsigned State=C->ReceiveTwo?C->ReceiveTwo->State[P->Identifier]:0;
    if(Qos){
        if((State&XMQTT_RX_ACTIVE) && (Qos!=2 || (State&XMQTT_RX_RELEASED))){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        bool Duplicate=Qos==2 && (State&XMQTT_RX_ACTIVE);
        if(!Duplicate){
            if(Qos==2 && C->ReceiveFlow){xmqttreceiveflow* F=C->ReceiveFlow;
                for(size_t I=0;I<F->Count;++I)if(F->Identifiers[(F->Head+I)%F->Capacity]==P->Identifier){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}}
            size_t Inflight=(C->ReceiveFlow?C->ReceiveFlow->Count:0)+(C->ReceiveTwo?C->ReceiveTwo->Inflight:0);
            if(Inflight>=C->Peer.ReceiveMaximum){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x93);return false;}
        }
        if(Qos==1){
            if(C->ReceiveFlow && C->ReceiveFlow->Count==C->ReceiveFlow->Capacity){C->ReceivePaused=true;return false;}
            return true;
        }
    }
    if(!xmqttReceiveTwoPrepare(C))return false;
    unsigned Flag=Qos==2?XMQTT_RX_REC_QUEUED:State&XMQTT_RX_ACTIVE?XMQTT_RX_COMP_QUEUED:XMQTT_RX_UNKNOWN_QUEUED;
    if(!(State&Flag) && C->ReceiveTwo->Count==C->ReceiveTwo->Capacity){C->ReceivePaused=true;return false;}
    return true;
}
static bool xmqttClientProtocolReady(const xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && C->Auth->ProtocolReady)return true;
#endif
    return C->Info.Ready;
}
static bool xmqttClientFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(P->Type==XMQTT_PACKET_AUTH)return xmqttAuthFrame(C,P);
#endif
    if(!xmqttClientProtocolReady(C)){
        if(P->Type!=XMQTT_PACKET_CONNACK || !C->Startup || C->SetupOffset!=C->Startup->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,NULL);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=C->Config.Version;K.Codec=C->Config.Codec;
        xmqttnegotiated N;xmqttnegotiationissue Issue;
        xmqttnegotiationstatus S=xmqttNegotiate(&C->SetupPacket,&M->Packet,&K,&N,&Issue);
        if(S<0){xmqttMessageRelease(M);xmqttClientFail(C,S==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_STATUS_SESSION:S==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;}
        if(N.Accepted && N.SessionPresent){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}
        xmqttreadybox* B=C->ReadyBox;B->Value.Connack=M;B->Value.Reason=P->Reason;
        if(N.ClientId.Size && !N.ServerAssignedId)N.ClientId=(xstrview){B->Id,N.ClientId.Size};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(C->Auth && N.HasAuthMethod)for(size_t I=0;I<M->Packet.Properties.Count;++I)
            if(M->Packet.Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_METHOD)N.AuthMethod=M->Packet.Properties.Data[I].Value.Text;
#endif
        B->Value.Negotiated=N;C->Peer=N;
        if(S==XMQTT_NEGOTIATION_REJECTED){xmqttClientFail(C,XMQTT_STATUS_REJECTED,NULL);return false;}
        /* Keep Alive is based on control output, not CONNACK arrival. A server
         * cannot send PINGRESP before its successful CONNACK, so a pre-CONNACK
         * ping starts its response budget only now. */
        C->NextPing=N.KeepAlive?C->PingDeadline+N.KeepAlive:INFINITY;
        C->PingDeadline=C->PingOutstanding?xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout):INFINITY;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(C->Auth)return xmqttAuthConnack(C,M);
#endif
        xrtMutexLock(&C->Gate);C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;xrtMutexUnlock(&C->Gate);
        xmqttReadyFinish(C,XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PINGRESP){C->PingOutstanding=false;C->PingDeadline=INFINITY;xrtMutexLock(&C->Gate);++C->Info.Pongs;xrtMutexUnlock(&C->Gate);return true;}
    if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK){
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        uint8 Expected=P->Type==XMQTT_PACKET_SUBACK?XMQTT_PACKET_SUBSCRIBE:XMQTT_PACKET_UNSUBSCRIBE;
        if(!R || R->Type!=Expected || R->Offset!=xmqttRequestWireBytes(R) || (!C->ProblemInfo && P->Properties.Count)){
            xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        size_t Count=R->State.TopicCount,Rejected=0;
        if(!(C->Config.Version==XMQTT_VERSION_311 && Expected==XMQTT_PACKET_UNSUBSCRIBE) && P->Body.Reasons.Size!=Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        /* Traverse our already validated immutable encoding, without another
         * allocation or retained caller descriptors. Skip filter bytes, compare
         * each successful grant with its original requested QoS. */
        size_t At=1;while(R->Wire[At++]&128u){}At+=2;
        if(C->Config.Version==XMQTT_VERSION_5){size_t Properties=0,Shift=0;unsigned char Byte;
            do{Byte=R->Wire[At++];Properties|=(size_t)(Byte&127u)<<Shift;Shift+=7;}while(Byte&128u);At+=Properties;}
        for(size_t I=0;I<P->Body.Reasons.Size;++I){unsigned char Reason=((const unsigned char*)P->Body.Reasons.Data)[I];
            if(Reason>=0x80)++Rejected;
            if(Expected==XMQTT_PACKET_SUBSCRIBE){size_t N=((size_t)R->Wire[At]<<8)|R->Wire[At+1];At+=N+2;
                if(Reason<0x80 && Reason>(R->Wire[At]&3u)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}++At;}
        }
        xmqttsubscriptionresult* V=&R->Result.Subscription->Value;
        V->Acknowledged=true;V->Granted=Count-Rejected;V->Rejected=Rejected;
        V->Ack=xmqttMessageCopy(C,P,&Failure,NULL);
        if(!V->Ack){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,Rejected?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBACK){
        /* The codec already limits PUBACK properties to Reason String/User. */
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        if(!R || R->Type!=XMQTT_PACKET_PUBLISH || R->Qos!=1 || !R->Credit || R->Offset!=xmqttRequestWireBytes(R)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        R->Acknowledged=true;R->Result.Write->Value.Reason=P->Reason;
        R->Result.Write->Value.Puback=xmqttMessageCopy(C,P,&Failure,NULL);
        if(!R->Result.Write->Value.Puback){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,P->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBREC || P->Type==XMQTT_PACKET_PUBCOMP){
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        /* A late/repeated PUBCOMP must not complete a new flow or mint quota.
         * Its old generation is not encoded on the wire. Ignore until an actual
         * QoS2 flow has sent a complete PUBREL on this connection. */
        if(P->Type==XMQTT_PACKET_PUBCOMP && (!R || (R->Qos==2 && !R->State.Release.Sent)))return true;
        if(!R || R->Type!=XMQTT_PACKET_PUBLISH || R->Qos!=2 || !R->Credit || R->Offset!=xmqttRequestWireBytes(R)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttwriteresult* V=&R->Result.Write->Value;
        if(P->Type==XMQTT_PACKET_PUBREC){
            if(R->Acknowledged)return true;
            if(!V->PubrecReceived || P->Reason>=0x80){
                V->PubrecReceived=true;V->PubrecReason=P->Reason;V->Reason=P->Reason;
                if(P->Reason>=0x80)R->Acknowledged=true;
                xmqttMessageRelease(V->Pubrec);V->Pubrec=NULL;
                xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,NULL);
                if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
                V->Pubrec=M;
            }
            if(P->Reason<0x80){
                /* Coalesce duplicates while one immutable PUBREL is unsent or
                 * partial; once a complete one was sent, schedule it again. */
                if(R->State.Release.Offset==4)R->State.Release.Offset=0;
                R->State.Release.Pending=true;return true;
            }
        }else {
            if(V->PubcompReceived)return true;
            V->PubcompReceived=true;V->Reason=P->Reason;R->Acknowledged=true;
            V->Pubcomp=xmqttMessageCopy(C,P,&Failure,NULL);
            if(!V->Pubcomp){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        }
        if(R->Qos==2 && R->State.Release.Offset && R->State.Release.Offset<4)return true;
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,V->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBREL){
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttreceivetwo* F=C->ReceiveTwo;unsigned State=F->State[P->Identifier];
        bool Unknown=!(State&XMQTT_RX_ACTIVE);
        if(!Unknown)F->State[P->Identifier]|=XMQTT_RX_RELEASED;
        xmqttReceiveTwoQueue(C,P->Identifier,0x70,Unknown);return true;
    }
    if(P->Type==XMQTT_PACKET_DISCONNECT){xrtMutexLock(&C->Gate);C->Info.ServerReason=P->Reason;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_SERVER_DISCONNECT,NULL);return false;}
    if(P->Type!=XMQTT_PACKET_PUBLISH){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    xstrview Resolved={0};if(!xmqttReceiveAlias(C,P,&Resolved))return false;
    unsigned Qos=(P->Flags>>1)&3u;
    if(Qos==2 && (C->ReceiveTwo->State[P->Identifier]&XMQTT_RX_ACTIVE)){xmqttReceiveTwoQueue(C,P->Identifier,0x50,false);return true;}
    if(Qos==1 && !xmqttReceivePrepare(C))return false;
    xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,Resolved.Size?&Resolved:NULL);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    xrtMutexLock(&C->Gate);
    bool Full;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){Full=!xmqttInboxPut(C->Inbox,M);if(!Full)++C->Info.Received;}else
#endif
    {Full=C->Info.Messages==C->Config.MaxMessages || M->Box.Bytes>C->Config.MaxMessageBytes-C->Info.MessageBytes;
    if(!Full){if(C->MessageTail)C->MessageTail->Next=M;else C->MessageHead=M;C->MessageTail=M;++C->Info.Messages;C->Info.MessageBytes+=M->Box.Bytes;++C->Info.Received;}}
    xrtMutexUnlock(&C->Gate);
    if(Full){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    if(Qos==1){xmqttreceiveflow* F=C->ReceiveFlow;F->Identifiers[(F->Head+F->Count)%F->Capacity]=P->Identifier;++F->Count;}
    else if(Qos==2){C->ReceiveTwo->State[P->Identifier]|=XMQTT_RX_ACTIVE;++C->ReceiveTwo->Inflight;xmqttReceiveTwoQueue(C,P->Identifier,0x50,false);}
    xmqttClientNotify(C);return true;
}
static void xmqttClientReadPump(xmqttclient* C)
{
    size_t Read=0,Packets=0;
    while(!xmqttClientClosing(C) && Packets<C->Config.WorkPackets){
        size_t Needed=5;xmqttfixedheader H;xmqttwireconfig K={sizeof(K),C->Config.Version,XMQTT_PACKET_MAX_BYTES};
        xmqttwirestatus Header=xmqttHeaderRead((xbytesview){C->Input,C->InputBytes},&K,&H);
        if(Header==XMQTT_WIRE_ERROR){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,xrtGetError(),0x81);return;}
        if(Header==XMQTT_WIRE_DONE){
            if(H.PacketBytes>C->Peer.ReceivePacketBytes){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x95);return;}
            if(H.PacketBytes>C->Config.Codec.MaxPacketBytes){xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return;}
            Needed=H.PacketBytes;
            if(C->InputBytes>=Needed){xmqttpacket P;xmqttcodecissue Issue;xmqttcodecbuffers B={C->Props,C->Config.Codec.MaxProperties,NULL,0};
                xmqttcodecstatus S=C->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue):xmqttPacketRead311((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue);
                if(S!=XMQTT_CODEC_DONE){xmqttClientFailReason(C,S==XMQTT_CODEC_LOCAL_ERROR?XMQTT_STATUS_LIMIT:XMQTT_STATUS_PROTOCOL,xrtGetError(),S==XMQTT_CODEC_LOCAL_ERROR?0:Issue.Error==XMQTT_CODEC_TOPIC_ALIAS?0x94:Issue.Error==XMQTT_CODEC_TOPIC_ALIAS_REPEAT?0x82:0x81);return;}
                if(!xmqttReceiveRoom(C,&P))return;
                if(!xmqttClientFrame(C,&P))return;
                C->InputBytes-=Needed;if(C->InputBytes)memmove(C->Input,C->Input+Needed,C->InputBytes);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
                if(C->Auth && (P.Type==XMQTT_PACKET_AUTH || P.Type==XMQTT_PACKET_CONNACK))xrtSecureZero(C->Input+C->InputBytes,Needed);
#endif
                ++Packets;continue;
            }
        }
        if(Needed>C->InputCapacity && !xmqttClientGrow(C,Needed))return;
        if(Read==C->Config.ReadBytes || !xmqttTransportAvailable(C))break;
        size_t Count=C->InputCapacity-C->InputBytes;
        if(Count>C->Config.ReadBytes-Read)Count=C->Config.ReadBytes-Read;
        size_t N=xmqttTransportRead(C,C->Input+C->InputBytes,Count);if(!N)break;
        Read+=N;C->InputBytes+=N;
    }
    if(Packets==C->Config.WorkPackets || (Read==C->Config.ReadBytes && xmqttTransportAvailable(C)))xmqttClientWake(C);
}
static bool xmqttClientSendStartup(xmqttclient* C,size_t* Budget)
{
    xmqttstartup* W=C->Startup;
    while(W && C->SetupOffset<W->Bytes && *Budget){
        size_t N=W->Bytes-C->SetupOffset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xmqttTransportSendRef(C,W->Data+C->SetupOffset,&N,xmqttStartupRelease,W);
        if(S!=XNET_RESULT_OK){xmqttStartupDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        C->SetupOffset+=N;*Budget-=N;
        if(C->SetupOffset==W->Bytes){C->PingDeadline=xrtTimer();C->NextPing=C->Peer.KeepAlive?C->PingDeadline+C->Peer.KeepAlive:INFINITY;}
    }
    if(W && C->SetupOffset<W->Bytes && !*Budget)xmqttClientWake(C);
    return W && C->SetupOffset==W->Bytes;
}
static bool xmqttClientSendPing(xmqttclient* C,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<2){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    static const unsigned char Ping[]={0xc0,0};
    while(C->PingOffset<2 && *Budget){
        size_t N=2-C->PingOffset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ping+C->PingOffset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(!C->PingOutstanding){C->PingOutstanding=true;
            if(xmqttClientProtocolReady(C))C->PingDeadline=xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout);}
        C->PingOffset+=(unsigned)N;*Budget-=N;
    }
    if(C->PingOffset!=2){xmqttClientWake(C);return false;}
    C->PingOffset=0;double SentAt=xrtTimer();C->NextPing=C->Peer.KeepAlive?SentAt+C->Peer.KeepAlive:INFINITY;
    if(!xmqttClientProtocolReady(C))C->PingDeadline=SentAt;
    xrtMutexLock(&C->Gate);++C->Info.Pings;xrtMutexUnlock(&C->Gate);return true;
}
static bool xmqttClientSendAcks(xmqttclient* C,size_t* Budget,bool One)
{
    xmqttreceiveflow* F=C->ReceiveFlow;
    while(F && F->Count && *Budget){uint16 Id=F->Identifiers[F->Head];
        const unsigned char Ack[]={0x40,2,(unsigned char)(Id>>8),(unsigned char)Id};
        if(C->Peer.SendPacketBytes<sizeof(Ack)){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
        size_t N=sizeof(Ack)-F->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ack+F->Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        F->Offset+=(unsigned)N;*Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        if(F->Offset==sizeof(Ack)){F->Offset=0;F->Head=(F->Head+1)%F->Capacity;--F->Count;
            if(C->ReceivePaused){C->ReceivePaused=false;xmqttClientWake(C);}if(One)return true;}
    }
    if(F && F->Count){xmqttClientWake(C);return false;}return true;
}
static bool xmqttClientSendTwoAcks(xmqttclient* C,size_t* Budget,bool PartialOnly)
{
    xmqttreceivetwo* F=C->ReceiveTwo;size_t Packets=0;
    while(F && F->Count && *Budget && Packets<C->Config.WorkPackets){xmqttreceiveack A=F->Acks[F->Head];
        unsigned char Ack[]={A.Type,2,(unsigned char)(A.Identifier>>8),(unsigned char)A.Identifier,0x92};
        size_t Bytes=A.Unknown && C->Config.Version==XMQTT_VERSION_5?5:4;Ack[1]=(unsigned char)(Bytes-2);
        if(C->Peer.SendPacketBytes<Bytes){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
        size_t N=Bytes-F->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ack+F->Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        F->Offset+=(unsigned)N;*Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        if(F->Offset==Bytes){unsigned char* State=F->State+A.Identifier;
            if(A.Type==0x50)*State&=(unsigned char)~XMQTT_RX_REC_QUEUED;
            else if(A.Unknown)*State&=(unsigned char)~XMQTT_RX_UNKNOWN_QUEUED;
            else {*State&=(unsigned char)~(XMQTT_RX_ACTIVE|XMQTT_RX_RELEASED|XMQTT_RX_COMP_QUEUED);--F->Inflight;}
            F->Offset=0;F->Head=(F->Head+1)%F->Capacity;--F->Count;++Packets;
            if(C->ReceivePaused){C->ReceivePaused=false;xmqttClientWake(C);}
            if(PartialOnly)return true;
        }
    }
    if(F && F->Count){xmqttClientWake(C);return false;}return true;
}
/* A partial frame always wins. At boundaries, alternate ready ACK classes so
 * a replenished QoS1 ring cannot starve QoS2 under small write budgets. */
static bool xmqttClientSendReceiveAcks(xmqttclient* C,size_t* Budget)
{
    xmqttreceivetwo* F=C->ReceiveTwo;xmqttreceiveflow* One=C->ReceiveFlow;size_t Packets=0;
    while((F->Count || (One && One->Count)) && *Budget && Packets<C->Config.WorkPackets){
        bool Two=F->Offset || (!(One && One->Offset) && F->Count && (!(One && One->Count) || F->TwoNext));
        if(!(Two?xmqttClientSendTwoAcks(C,Budget,true):xmqttClientSendAcks(C,Budget,true)))return false;
        F->TwoNext=!Two;++Packets;
    }
    if(F->Count || (One && One->Count)){xmqttClientWake(C);return false;}return true;
}
static xmqttclientstatus xmqttSendAliasPrepare(xmqttclient* C,xmqttrequest* R)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsendtopic* Actual=A->Slot->Actual;
    if(A->Alias>C->Peer.SendAliasMaximum)return XMQTT_STATUS_RESTRICTED;
    bool Compact=Actual && (Actual==A->Topic || (Actual->Size==A->Topic->Size && !memcmp(Actual->Data,A->Topic->Data,Actual->Size)));
    size_t Topic=Compact?0:A->Topic->Size,Tail=R->Bytes-A->TailOffset,Body=2+Topic+Tail;
    if(Body>XMQTT_VARIABLE_MAX)return XMQTT_STATUS_RESTRICTED;
    A->Prefix[0]=R->Wire[0];size_t H=1;uint32 N=(uint32)Body;
    do{unsigned char Byte=(unsigned char)(N&127u);N>>=7;A->Prefix[H++]=(unsigned char)(Byte|(N?128u:0u));}while(N);
    A->Prefix[H++]=(unsigned char)(Topic>>8);A->Prefix[H++]=(unsigned char)Topic;A->PrefixBytes=(uint8)H;A->FrameBytes=H+Topic+Tail;
    if(A->FrameBytes>C->Peer.SendPacketBytes)return XMQTT_STATUS_RESTRICTED;
    if(A->FrameBytes>C->Config.Codec.MaxPacketBytes)return XMQTT_STATUS_LIMIT;
    bool RawEmpty=!R->Wire[A->RawHeader] && !R->Wire[A->RawHeader+1];
    A->Mode=Compact?(RawEmpty?1:2):(RawEmpty?3:1);return XMQTT_STATUS_OK;
}
static size_t xmqttSendAliasRefs(xmqttrequest* R,size_t Bytes,xnetref Refs[3])
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);size_t At=R->Offset,Count=0;
    const unsigned char* Data[3]={A->Prefix,(const unsigned char*)A->Topic->Data,R->Wire+A->TailOffset};
    size_t Sizes[3]={A->PrefixBytes,A->Mode==3?A->Topic->Size:0,R->Bytes-A->TailOffset};
    for(unsigned I=0;I<3 && Bytes;++I){if(At>=Sizes[I]){At-=Sizes[I];continue;}size_t N=Sizes[I]-At;if(N>Bytes)N=Bytes;
        Refs[Count++]=(xnetref){Data[I]+At,N,xmqttRequestNativeDrop,R};Bytes-=N;At=0;}
    return Count;
}
static bool xmqttClientSendAliasRequest(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    size_t Bytes=xmqttRequestWireBytes(R);
    while(R->Offset<Bytes && *Budget){size_t N=Bytes-R->Offset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        xnetref Refs[3];size_t Count=1;
        if(xmqttSendAliasRequest(R)->Mode!=1)Count=xmqttSendAliasRefs(R,N,Refs);
        else Refs[0]=(xnetref){R->Wire+R->Offset,N,xmqttRequestNativeDrop,R};
        xnetresult S=xmqttRequestSend(C,R,Refs,Count,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
        R->Offset+=N;*Budget-=N;C->Partial=R->Offset==Bytes?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->Offset!=Bytes){xmqttClientWake(C);return false;}
    {xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);if(A->Slot->Actual!=A->Topic){
        xrtMutexLock(&C->Gate);xmqttsendtopic* Old=A->Slot->Actual;++A->Topic->Refs;A->Slot->Actual=A->Topic;xmqttSendTopicDropLocked(C,Old);xrtMutexUnlock(&C->Gate);}}
    return true;
}
static bool xmqttClientSendRequest(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    if(R->Capabilities&XMQTT_TX_ALIAS)return xmqttClientSendAliasRequest(C,R,Budget);
    while(R->Offset<R->Bytes && *Budget){size_t N=R->Bytes-R->Offset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        xnetref Ref={R->Wire+R->Offset,N,xmqttRequestNativeDrop,R};xnetresult S=xmqttRequestSend(C,R,&Ref,1,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
        R->Offset+=N;*Budget-=N;C->Partial=R->Offset==R->Bytes?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->Offset!=R->Bytes){xmqttClientWake(C);return false;}return true;
}
/* The inline four-byte frame is immutable while every native lease owns R.
 * Identifier retirement therefore includes repeated PUBREL write tails. */
static bool xmqttClientSendPubrel(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<4){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    while(R->State.Release.Offset<4 && *Budget){size_t N=4-R->State.Release.Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetref Ref={R->State.Release.Wire+R->State.Release.Offset,N,xmqttRequestNativeDrop,R};xnetresult S=xmqttRequestSend(C,R,&Ref,1,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        R->State.Release.Offset+=(uint8)N;*Budget-=N;C->Partial=R->State.Release.Offset==4?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->State.Release.Offset!=4){xmqttClientWake(C);return false;}
    R->State.Release.Pending=false;R->State.Release.Sent=true;return true;
}
static void xmqttClientSendPump(xmqttclient* C)
{
    size_t Budget=C->Config.WriteBytes;
    if(!xmqttClientProtocolReady(C)){
        if(C->Startup && C->SetupOffset==C->Startup->Bytes){
            if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
            if(!xmqttAuthSend(C,&Budget))return;
#endif
            if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
        }
        return;
    }
    /* Finish the one partial packet before inserting any other packet, even
     * when a formerly blocked earlier request has just gained send credit. */
    if(C->Partial){xmqttrequest* R=C->Partial;
        if(!(R->Qos==2 && R->Result.Write->Value.PubrecReceived?xmqttClientSendPubrel(C,R,&Budget):xmqttClientSendRequest(C,R,&Budget)))return;
    }
    if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    /* An AUTH can be partial too. Complete an existing receive ACK before
     * starting AUTH; finish AUTH before starting any new receive ACK. */
    if(C->Auth){
        if(C->ReceiveTwo && C->ReceiveTwo->Offset && !xmqttClientSendTwoAcks(C,&Budget,true))return;
        if(C->ReceiveFlow && C->ReceiveFlow->Offset && !xmqttClientSendAcks(C,&Budget,true))return;
        if(!xmqttAuthSend(C,&Budget))return;
    }
#endif
    if(!(C->ReceiveTwo?xmqttClientSendReceiveAcks(C,&Budget):xmqttClientSendAcks(C,&Budget,false)))return;
    if(!C->Info.Ready){if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);return;}
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){
        xmqttrequest* R=*Link;
        if(R->Qos==2 && R->Acknowledged){xmqttRequestFinish(C,Link,R->Result.Write->Value.Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);continue;}
        if(R->Qos==2 && R->State.Release.Pending){if(!xmqttClientSendPubrel(C,R,&Budget))return;if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;continue;}
        if(R->Offset==xmqttRequestWireBytes(R)){Link=&R->Next;continue;}
        if((!(R->Capabilities&XMQTT_TX_ALIAS) && R->Bytes>C->Peer.SendPacketBytes) || R->Qos>C->Peer.MaximumQos || (R->Retain && !C->Peer.RetainAvailable) ||
           ((R->Capabilities&1u) && !C->Peer.WildcardAvailable) || ((R->Capabilities&2u) && !C->Peer.SharedAvailable) ||
           ((R->Capabilities&4u) && !C->Peer.SubscriptionIdsAvailable)){xmqttRequestFinish(C,Link,XMQTT_STATUS_RESTRICTED);continue;}
        if(R->Qos && !R->Credit && C->SendInflight>=C->Peer.SendReceiveMaximum){Link=&R->Next;continue;}
        if((R->Capabilities&XMQTT_TX_ALIAS) && !R->Offset){xmqttclientstatus S=xmqttSendAliasPrepare(C,R);
            if(S!=XMQTT_STATUS_OK){xmqttRequestFinish(C,Link,S);continue;}}
        if(!xmqttClientSendRequest(C,R,&Budget))return;
        if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;
    }
    if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
}
static void xmqttClientCollect(xmqttclient* C)
{
    xrtMutexLock(&C->Gate);xmqttrequest* Head=C->SubmitHead;C->SubmitHead=C->SubmitTail=NULL;xrtMutexUnlock(&C->Gate);
    if(Head){if(C->RequestTail)C->RequestTail->Next=Head;else C->Requests=Head;
        while(Head){Head->Previous=C->RequestTail;C->RequestTail=Head;xmqttRequestMapInsert(C,Head);Head=Head->Next;}}
}
static void xmqttClientDrainStop(xmqttclient* C,bool Abort)
{
    C->Draining=false;
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;C->TimerAt=INFINITY;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    if(Abort)(void)xmqttTransportClose(C,true);
}
static void xmqttClientDrainError(xmqttclient* C)
{
    xrtMutexLock(&C->Gate);bool Abort=C->AbortRequested;size_t Offset=C->Info.DisconnectBytes;uint8 Reason=C->Info.LocalReason;xrtMutexUnlock(&C->Gate);
    if(C->NativeClosed){xmqttClientDrainStop(C,false);return;}
    if(Abort || C->ReadEnded || xrtTimer()>=C->DrainDeadline){
        if(!Abort && !C->ReadEnded){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);}
        xmqttClientDrainStop(C,true);return;
    }
    if(C->DrainClosing)return;
    const unsigned char Wire[]={0xe0,1,Reason};size_t Budget=C->Config.WriteBytes;
    while(Offset<sizeof(Wire) && Budget){size_t N=sizeof(Wire)-Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>Budget)N=Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return;
        xnetresult S=xmqttTransportSend(C,Wire+Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientDrainStop(C,true);return;}
        Offset+=N;Budget-=N;xrtMutexLock(&C->Gate);C->Info.DisconnectBytes=Offset;xrtMutexUnlock(&C->Gate);
    }
    if(Offset==sizeof(Wire)){C->DrainClosing=true;if(!xmqttTransportClose(C,false))xmqttClientDrainStop(C,true);}
    else xmqttClientWake(C);
}
/* The accepted application request owns its wire and producer lease. Drain
 * prior protocol obligations before making DISCONNECT the final frame. */
static void xmqttClientStopApplication(xmqttclient* C)
{
    xmqttrequest* R=C->Disconnect;if(!R)return;
    if(!C->Stopped){C->Stopped=true;xmqttClientNotify(C);}
    if(xrtCancelRequested(R->Cancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    if(xrtTimer()>=R->Deadline){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);}
    xrtMutexLock(&C->Gate);xmqttclientstatus Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    if(Status!=XMQTT_STATUS_OK)goto Failed;
    xmqttClientSchedule(C,R->Deadline);
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    if(Status!=XMQTT_STATUS_OK)goto Failed;
    if(!C->ApplicationNativeClosing){
        xrtMutexLock(&C->Gate);C->Info.DrainStarted=true;xrtMutexUnlock(&C->Gate);
        size_t Budget=C->Config.WriteBytes;bool Sent=xmqttClientSendRequest(C,R,&Budget);
        xrtMutexLock(&C->Gate);C->Info.DisconnectBytes=R->Offset;Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
        if(Status!=XMQTT_STATUS_OK)goto Failed;
        if(!Sent)return;
        C->ApplicationNativeClosing=true;
        if(!xmqttTransportClose(C,false)){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());goto Failed;}
    }
    if(!C->NativeClosed || xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE))return;
    Status=XMQTT_STATUS_OK;
    goto Finish;
Failed:
    /* Disconnect/Close may publish after this worker's initial collection but
     * before its stop decision. All earlier accepted submissions must settle
     * before the disconnect request and the application-close state retire. */
    xmqttClientCollect(C);
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    (void)xmqttTransportClose(C,true);
    {size_t Count=0;while(C->Requests && Count++<C->Config.WorkPackets)xmqttRequestFinish(C,&C->Requests,Status);}
    if(C->Requests){xmqttClientWake(C);return;}
Finish:
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;C->TimerAt=INFINITY;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;C->Info.State=XMQTT_CLIENT_CLOSED;xrtMutexUnlock(&C->Gate);
    xmqttRequestFinish(C,&C->Disconnect,Status);xmqttClientNotify(C);
}
static void xmqttClientStop(xmqttclient* C)
{
    if(C->ApplicationClose){xmqttClientStopApplication(C);return;}
    if(!C->Stopped){C->Stopped=true;
        bool AuthPartial=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        AuthPartial=xmqttAuthStop(C);
#endif
        if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
        xmqttReadyFinish(C,C->Info.CloseStatus);xmqttStartupRetire(C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
        if(C->TLS && C->TLS->Plan.Session){xtlssession* S=C->TLS->Plan.Session;C->TLS->Plan.Session=NULL;xrtTlsSessionDestroy(S);}
#endif
        xrtMutexLock(&C->Gate);bool Diagnostic=C->Info.LocalReason && !C->AbortRequested;xrtMutexUnlock(&C->Gate);
        /* Never append a diagnostic inside a partial MQTT packet. Already
         * fully accepted output frames may drain ahead of the final packet. */
        C->Draining=Diagnostic && !AuthPartial && !C->Partial && !C->PingOffset && !(C->ReceiveFlow && C->ReceiveFlow->Offset) && !(C->ReceiveTwo && C->ReceiveTwo->Offset) &&
            !C->ReadEnded && !C->NativeClosed &&
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
            C->Stream &&
#endif
            C->Peer.SendPacketBytes>=3 && xmqttTransportOpen(C);
        if(C->Draining){C->DrainDeadline=xmqttClientDeadline(xrtTimer(),C->Config.CloseTimeout);xmqttClientSchedule(C,C->DrainDeadline);
            if(!C->Timer)C->Draining=false;else {xrtMutexLock(&C->Gate);C->Info.DrainStarted=true;xrtMutexUnlock(&C->Gate);}}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        if(C->Dial && !C->Stream)(void)xrtNetDialCancel(C->Dial);
#endif
        if(!C->Draining
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
            && C->Stream
#endif
        )(void)xmqttTransportClose(C,true);
        xmqttClientNotify(C); /* CLOSING is observable; reentrant Close may abort. */
    }
    if(C->Draining)xmqttClientDrainError(C);
    size_t Count=0;
    while(C->Requests && Count++<C->Config.WorkPackets)xmqttRequestFinish(C,&C->Requests,C->Info.CloseStatus);
    if(C->Requests){xmqttClientWake(C);return;}
    if(C->Draining)return;
    xrtMutexLock(&C->Gate);bool Changed=C->Info.State!=XMQTT_CLIENT_CLOSED;C->Info.State=XMQTT_CLIENT_CLOSED;xrtMutexUnlock(&C->Gate);
    if(Changed)xmqttClientNotify(C);
}
static void xmqttClientWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xmqttclient* C=Data;
    xrtMutexLock(&C->Gate);C->Posted=false;bool Started=C->Started;xmqttrequest* Disconnect=C->Disconnect;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool ResumeNotify=Started && C->TLS && C->TLS->Resume.Notify;if(ResumeNotify)C->TLS->Resume.Notify=false;
#endif
    xrtMutexUnlock(&C->Gate);
    if(!Started){xmqttClientDrop(C);return;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){C->TLS->Bytes=C->Config.WriteBytes;C->TLS->Calls=C->Config.WorkPackets;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    if(ResumeNotify)xmqttClientNotify(C);
#endif
    xmqttClientCollect(C);
    if(Disconnect){
        if(xrtCancelRequested(Disconnect->Cancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
        if(xrtTimer()>=Disconnect->Deadline){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);}
        if(!C->Stopped){C->Stopped=true;xmqttClientNotify(C);}
    }
    if(C->ReadyPromise && xrtCancelRequested(C->ReadyCancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    double Now=xrtTimer();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqttTlsProgress(C);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttAuthWork(C);
#endif
    if(!C->Info.Ready && Now>=C->SetupDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(!C->ApplicationFinal && C->PingOutstanding && xmqttClientProtocolReady(C) && Now>=C->PingDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){xmqttrequest* R=*Link;
        xmqttclientstatus S=xrtCancelRequested(R->Cancel)?XMQTT_STATUS_CANCELLED:Now>=R->Deadline?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK;
        if(S!=XMQTT_STATUS_OK){bool Partial=R->Offset!=0;xmqttRequestFinish(C,Link,S);if(Partial)xmqttClientFail(C,S,NULL);continue;}
        if(R->Type==XMQTT_PACKET_PUBLISH && !R->Qos && R->Offset==xmqttRequestWireBytes(R) && !xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE)){
            if(!xmqttTransportOpen(C) || C->ReadEnded){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);break;}
            xmqttRequestFinish(C,Link,XMQTT_STATUS_OK);continue;
        }
        Link=&R->Next;
    }
    if(!xmqttClientClosing(C)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
       && C->Stream
#endif
       && xmqttTransportOpen(C)){
        /* Startup must be queued before an immediate/coalesced CONNACK. */
        if(!C->Info.Ready){size_t Budget=C->Config.WriteBytes;(void)xmqttClientSendStartup(C,&Budget);}
        if(!xmqttClientClosing(C))xmqttClientReadPump(C);
        if(!xmqttClientClosing(C) && !C->ReadEnded)xmqttClientSendPump(C);
        if(xmqttClientProtocolReady(C) && !C->PingOutstanding && xrtTimer()>=C->NextPing){
            if((C->ReceiveFlow && C->ReceiveFlow->Offset) || (C->ReceiveTwo && C->ReceiveTwo->Offset))xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
            if(xmqttAuthPartial(C))xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
#endif
            for(xmqttrequest* R=C->Requests;R;R=R->Next)if((R->Offset && R->Offset<xmqttRequestWireBytes(R)) || (R->Qos==2 && R->State.Release.Offset && R->State.Release.Offset<4)){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);break;}}
    }
    if((C->ReadEnded || C->NativeClosed) && !(Disconnect && C->ApplicationNativeClosing))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);
    if(Disconnect && !xmqttClientClosing(C) && !C->Requests && !C->Partial && !C->PingOffset && !(C->ReceiveFlow && C->ReceiveFlow->Count) && !(C->ReceiveTwo && (C->ReceiveTwo->Count || C->ReceiveTwo->Inflight))){
        xrtMutexLock(&C->Gate);if(!C->SubmitHead)C->ApplicationFinal=true;xrtMutexUnlock(&C->Gate);
    }
    if(xmqttClientClosing(C))xmqttClientStop(C);
    else {double At=C->Info.Ready?C->NextPing:C->SetupDeadline;
        if(!C->Info.Ready && C->NextPing<At)At=C->NextPing;
        if(xmqttClientProtocolReady(C) && C->PingOutstanding && C->PingDeadline<At)At=C->PingDeadline;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        {double AuthAt=xmqttAuthDeadline(C);if(AuthAt<At)At=AuthAt;}
#endif
        for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Deadline<At)At=R->Deadline;
        if(Disconnect && Disconnect->Deadline<At)At=Disconnect->Deadline;
        xmqttClientSchedule(C,At);
    }
    xmqttClientDrop(C);
}

XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid client config output");
    *K=(xmqttclientconfig){0};K->Size=sizeof(*K);K->Version=XMQTT_VERSION_5;xrtNetStreamConfigInit(&K->Stream);xmqttCodecConfigInit(&K->Codec);
    K->MaxRequests=128;K->MaxOutgoingBytes=4u*1024u*1024u;K->MaxObjects=512;K->MaxObjectBytes=16u*1024u*1024u;K->MaxMessages=64;K->MaxMessageBytes=8u*1024u*1024u;
    K->MaxPrivateBytes=4u*1024u*1024u;K->MaxSetupBytes=64u*1024u;K->InitialInputBytes=1024;K->ReadBytes=K->WriteBytes=64u*1024u;K->WriteChunkBytes=16u*1024u;K->WorkPackets=64;K->SetupTimeout=K->PingTimeout=K->CloseTimeout=5000;return true;
}
static void xmqttClientFree(xmqttclient* C)
{
    xmqttmessage* M=C->MessageHead;while(M){xmqttmessage* Next=M->Next;xmqttMessageRelease(M);M=Next;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xrtNetDialDestroy(C->Dial);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(C->TLS->Stream || C->TLS->Head)abort();xrtTlsSessionDestroy(C->TLS->Plan.Session);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
        xrtTlsResumeRelease(C->TLS->Resume.Value);
#endif
        xrtFree(C->TLS);}
#endif
    xrtNetStreamDestroy(C->Stream);xrtFutureDestroy(C->ReadyFuture);xrtCancelDestroy(C->ReadyCancel);xrtErrorFree(C->Cause);
    if(C->SendAliases){for(size_t Page=0;Page<256;++Page){xmqttsendaliasslot* Slots=C->SendAliases->Pages[Page];if(!Slots)continue;
        for(size_t I=0;I<256;++I){xmqttSendTopicDropLocked(C,Slots[I].Planned);xmqttSendTopicDropLocked(C,Slots[I].Actual);}xrtFree(Slots);}xrtFree(C->SendAliases);}
    if(C->ReceiveAliases){for(size_t I=0;I<C->Peer.ReceiveAliasMaximum;++I)xrtFree(C->ReceiveAliases[I]);xrtFree(C->ReceiveAliases);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth){xrtSecureZero(C->Input,C->InputCapacity);xrtSecureZero(C->Auth->MethodBytes,C->Auth->Method.Size);xrtFree(C->Auth);}
#endif
    xrtFree(C->Input);xrtFree(C->ReceiveFlow);xrtFree(C->ReceiveTwo);xrtFree(xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_RELAXED));xmqttQuotaDrop(C->Quota);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){xmqttclientinbox* I=C->Inbox;xrtMutexLock(&I->Gate);if(I->Client!=C)abort();I->Client=NULL;xrtMutexUnlock(&I->Gate);}
#endif
    (void)xrtNetEngineUnpin(C->Engine);xrtCondUnit(&C->RetirementReady);xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);
}
static bool xmqttClientConnectImpl(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out,const void* AuthConfig,double Now,const void* DialPlan,const void* TlsPlan,void* InboxPlan)
{
    bool Planned=false;size_t HostBytes=0,TlsBytes=0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    const xmqtttlsplan* TLS=TlsPlan;if(TLS)TlsBytes=sizeof(xmqtttlsstate);
#else
    (void)TlsPlan;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    const xmqttdialplan* D=DialPlan;char* HostCopy=NULL;Planned=D!=NULL;
    if(D && !D->Addresses)HostBytes=D->Host.Size+1;
#else
    (void)DialPlan;
#endif
    xmqttclientconfig K;if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid client config");K=*Config;}else xmqttClientConfigInit(&K);
    if(!Engine || (!Remote && !Planned) || !Out || !Connect || !xrtMemRangeValid(Out,sizeof(*Out)) || !xrtMemRangeValid(Connect,sizeof(*Connect)) ||
       K.Size!=sizeof(K) || (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT ||
       !K.MaxRequests || K.MaxRequests>65536 || !K.MaxOutgoingBytes || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u || !K.MaxObjectBytes ||
       !K.MaxMessages || !K.MaxMessageBytes || !K.MaxSetupBytes || K.InitialInputBytes<5 || K.InitialInputBytes>K.Codec.MaxPacketBytes ||
       !K.ReadBytes || !K.WriteBytes || !K.WriteChunkBytes || !K.WorkPackets || K.SetupTimeout<0 || K.PingTimeout<=0 || K.CloseTimeout<=0)
        return xmqttClientError(XERR_ARGUMENT,"invalid MQTT client configuration");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    xmqttclientinbox* Inbox=InboxPlan;
    if(Inbox && (K.Version!=Inbox->Config.Version || K.MaxObjects!=Inbox->Config.MaxObjects || K.MaxObjectBytes!=Inbox->Config.MaxObjectBytes ||
       K.MaxMessages!=Inbox->Config.MaxMessages || K.MaxMessageBytes!=Inbox->Config.MaxMessageBytes))return xmqttClientError(XERR_ARGUMENT,"Client and Inbox limits differ");
#else
    (void)InboxPlan;
#endif
    if(!xrtNetStreamConfigValid(&K.Stream))return false;
    size_t WireBytes=0;bool Valid=K.Version==XMQTT_VERSION_5?xmqttPacketSize5(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes):xmqttPacketSize311(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes);
    if(!Valid)return false;
    uint32 ReceiveBytes=XMQTT_PACKET_MAX_BYTES;bool ProblemInfo=true,HasMethod=false;xstrview Method={0};
    for(size_t I=0;I<Connect->Properties.Count;++I){const xmqttproperty* P=Connect->Properties.Data+I;
        if(P->Id==XMQTT_PROPERTY_AUTH_METHOD){HasMethod=true;Method=P->Value.Text;if(!AuthConfig)return xmqttClientError(XERR_STATE,"enhanced authentication requires xmqttClientConnectAuth");}
        if(P->Id==XMQTT_PROPERTY_REQUEST_PROBLEM_INFO)ProblemInfo=P->Value.Number!=0;
        if(P->Id==XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE){ReceiveBytes=P->Value.Number;if(ReceiveBytes>K.Codec.MaxPacketBytes)return xmqttClientError(XERR_ARGUMENT,"advertised receive packet size exceeds local codec limit");}
    }
    size_t AuthBytes=0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(AuthConfig){if(K.Version!=XMQTT_VERSION_5 || !HasMethod)return xmqttClientError(XERR_ARGUMENT,"enhanced authentication requires MQTT5 CONNECT method");AuthBytes=sizeof(xmqttauthstate)+Method.Size;}
#else
    (void)HasMethod;(void)Method;
#endif
    size_t SetupCount=Connect->Properties.Count+Connect->Body.Connect.WillProperties.Count;
    if(SetupCount>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty) || K.Codec.MaxProperties>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty)-SetupCount || WireBytes>K.MaxSetupBytes || WireBytes>SIZE_MAX-sizeof(xmqttstartup))return xmqttClientError(XERR_RANGE,"MQTT client setup capacity");
    size_t ClientBytes=sizeof(xmqttclient)+(SetupCount+K.Codec.MaxProperties)*sizeof(xmqttproperty),StartupBytes=sizeof(xmqttstartup)+WireBytes;
    if(ClientBytes>K.MaxPrivateBytes || K.InitialInputBytes>K.MaxPrivateBytes-ClientBytes || StartupBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes || AuthBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes || HostBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes-AuthBytes || TlsBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes-AuthBytes-HostBytes)return xmqttClientError(XERR_RANGE,"MQTT private setup budget");
    if(!xrtNetEnginePin(Engine))return false;
    xmqttclient* C=xrtMalloc(ClientBytes);if(!C){xrtNetEngineUnpin(Engine);return false;}memset(C,0,ClientBytes);
    if(!xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtMutexInit(&C->RetirementLock)){xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtCondInit(&C->RetirementReady)){xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    C->Engine=Engine;C->Config=K;C->ProblemInfo=ProblemInfo;C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));xrtAtomic32Init(&C->Refs,1);
    xrtAtomicPtrInit(&C->SendFlow,NULL);
    C->Info.Size=sizeof(C->Info);C->Info.State=XMQTT_CLIENT_CONNECTING;C->Info.PrivateBytes=ClientBytes+K.InitialInputBytes+StartupBytes+AuthBytes+HostBytes+TlsBytes;C->Info.StartupBytes=WireBytes;
    C->SetupProps=C->Scratch;C->Props=C->Scratch+SetupCount;C->Peer.ReceivePacketBytes=ReceiveBytes;
    C->Peer.SendPacketBytes=XMQTT_PACKET_MAX_BYTES;C->Peer.KeepAlive=Connect->Body.Connect.KeepAlive;
    C->SetupDeadline=xmqttClientDeadline(Now,K.SetupTimeout);C->NextPing=C->PingDeadline=C->TimerAt=INFINITY;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(TLS){C->TLS=xrtCalloc(1,sizeof(*C->TLS));if(!C->TLS)goto Fail;C->TLS->Plan=*TLS;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
        C->TLS->Resume.Info.Size=sizeof(C->TLS->Resume.Info);C->TLS->Resume.Info.Enabled=TLS->Capture;
#endif
    }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(AuthConfig){C->Auth=xrtCalloc(1,AuthBytes);if(!C->Auth)goto Fail;C->Auth->Config=*(const xmqttauthconfig*)AuthConfig;
        C->Auth->Method=(xstrview){C->Auth->MethodBytes,Method.Size};if(Method.Size)memcpy(C->Auth->MethodBytes,Method.Data,Method.Size);
        C->Auth->Active=C->Auth->AwaitServer=true;C->Auth->Deadline=C->SetupDeadline;C->Auth->StepDeadline=INFINITY;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(Inbox){xrtMutexLock(&Inbox->Gate);
        if(Inbox->Client){xrtMutexUnlock(&Inbox->Gate);xmqttClientError(XERR_STATE,"Inbox already attached");goto Fail;}
        Inbox->Client=C;C->Inbox=Inbox;C->Quota=Inbox->Quota;(void)xrtAtomic32FetchAdd(&C->Quota->Refs,1,XMEMORY_RELAXED);xrtMutexUnlock(&Inbox->Gate);
    }else
#endif
    {C->Quota=xmqttQuotaCreate(K.MaxObjects,K.MaxObjectBytes);if(!C->Quota)goto Fail;}
    C->Input=xrtMalloc(K.InitialInputBytes);if(!C->Input)goto Fail;C->InputCapacity=K.InitialInputBytes;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    C->ReadyBox=xmqttBoxAllocate(C->Quota,sizeof(xmqttreadybox)+Connect->Body.Connect.ClientId.Size,&Failure);if(!C->ReadyBox){if(Failure==XMQTT_STATUS_LIMIT)xmqttClientError(XERR_RANGE,"ready result budget");goto Fail;}
    C->ReadyBox->Box=(xmqttbox){C->Quota,sizeof(xmqttreadybox)+Connect->Body.Connect.ClientId.Size};
    if(Connect->Body.Connect.ClientId.Size)memcpy(C->ReadyBox->Id,Connect->Body.Connect.ClientId.Data,Connect->Body.Connect.ClientId.Size);
    C->Startup=xrtMalloc(StartupBytes);if(!C->Startup)goto Fail;C->Startup->Client=C;C->Startup->Bytes=WireBytes;xrtAtomic32Init(&C->Startup->Refs,1);xmqttClientRef(C);
    if(!(K.Version==XMQTT_VERSION_5?xmqttPacketWrite5(C->Startup->Data,WireBytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,NULL):xmqttPacketWrite311(C->Startup->Data,WireBytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,NULL)))goto Fail;
    xmqttcodecbuffers B={C->SetupProps,SetupCount,NULL,0};
    if((K.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){C->Startup->Data,WireBytes},XMQTT_CLIENT_TO_SERVER,&K.Codec,&B,&C->SetupPacket,NULL):xmqttPacketRead311((xbytesview){C->Startup->Data,WireBytes},XMQTT_CLIENT_TO_SERVER,&K.Codec,&B,&C->SetupPacket,NULL))!=XMQTT_CODEC_DONE)goto Fail;
    C->ReadyPromise=xrtPromiseCreate(&C->ReadyFuture,NULL);if(!C->ReadyPromise)goto Fail;
    xmqttClientRef(C);if(!xrtPromiseProducerBindTakeV2(C->ReadyPromise,(xrtownershipref){C,&xmqttPrivateOps},&xmqttReadyProducer,&xmqttReadyCancellation)){xmqttClientDrop(C);goto Fail;}
    C->ReadyCancel=xrtPromiseCancelToken(C->ReadyPromise);if(!C->ReadyCancel || !xrtNetPostInit(&C->Post))goto Fail;
    /* Serialize startup publication against immediate native callbacks. */
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(HostBytes){HostCopy=xrtMalloc(HostBytes);if(!HostCopy)goto Fail;memcpy(HostCopy,D->Host.Data,D->Host.Size);HostCopy[D->Host.Size]=0;}
#endif
    xrtMutexLock(&C->Gate);xmqttClientRef(C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(D){xnetdialconfig Policy=D->Config;Policy.Stream=K.Stream;Policy.Affinity=Affinity;
        C->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(Engine,D->Addresses,&Policy,NULL,NULL,C,&xmqttClientDialEvents):
            xrtNetDialOwnedV1(Engine,D->Resolver,HostCopy,D->Port,&Policy,NULL,NULL,C,&xmqttClientDialEvents);
        if(!C->Dial){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}
        xrtFree(HostCopy);HostCopy=NULL;C->Info.PrivateBytes-=HostBytes;
    }else
#endif
    {C->Stream=xrtNetStreamConnectOwnedV1(Engine,Remote,Affinity,&K.Stream,C,&xmqttClientEvents);
        if(!C->Stream){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}}
    C->Started=true;(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);*Out=C;return true;
Fail:
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xrtFree(HostCopy);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)C->TLS->Plan.Session=NULL; /* failure transfers no plan ownership */
#endif
    xrtPromiseDestroy(C->ReadyPromise);C->ReadyPromise=NULL;if(C->ReadyBox)xmqttReadyFree(C->ReadyBox,NULL);xmqttStartupRetire(C);xmqttClientFree(C);return false;
}
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out)
{double Now=xrtTimer();return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,NULL,Now,NULL,NULL,NULL);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
XMQTT_API bool xmqttClientConnectInbox(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Out)
{double Now=xrtTimer();if(!Inbox)return xmqttClientError(XERR_ARGUMENT,"Inbox is NULL");return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,NULL,Now,NULL,NULL,Inbox);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static bool xmqttAuthConfigCopy(const xmqttauthconfig* Auth,xmqttauthconfig* K)
{
    if(Auth){if(!xrtMemRangeValid(Auth,sizeof(*Auth)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication config");*K=*Auth;}else xmqttAuthConfigInit(K);
    if(K->Size!=sizeof(*K) || !K->MaxRounds || K->MaxRounds>65536 || !K->MaxExchangeBytes || K->ResponseTimeout<=0)return xmqttClientError(XERR_ARGUMENT,"invalid authentication limits");
    return true;
}
XMQTT_API bool xmqttClientConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Out)
{
    double Now=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,&K,Now,NULL,NULL,NULL);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
XMQTT_API bool xmqttClientDialConfigInit(xmqttclientdialconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid Dial config output");
    *K=(xmqttclientdialconfig){sizeof(*K),XNET_FAMILY_UNSPEC,30000,250,8,1024};return true;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
XMQTT_API bool xmqttClientTlsConfigInit(xmqttclienttlsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS config output");
    memset(K,0,sizeof(*K));K->Size=sizeof(*K);xrtTlsClientConfigInit(&K->Session);xrtTlsStreamConfigInit(&K->Stream);
    xmqttClientDialConfigInit(&K->Dial);K->MaxMetadataBytes=65536;K->ServerNameFromHost=true;return true;
}
static bool xmqttTlsPlanCreate(const xmqttclienttlsconfig* K,xstrview Host,xmqttclient** Out,xmqtttlsplan* Plan)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || xrtMemRangesOverlap(K,sizeof(*K),Out,sizeof(*Out)) ||
       !K->MaxMetadataBytes || K->MaxMetadataBytes>1024u*1024u || K->Stream.HandshakeTimeout<0 || K->Stream.CloseTimeout<0 ||
       !K->Stream.AsyncBytesLimit || !K->Stream.AsyncCountLimit || !K->Stream.AsyncBatch || K->Session.ProtocolCount>256)
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS config, metadata bounds or output overlap");
    xtlsclientconfig S=K->Session;
    if(K->ServerNameFromHost && !S.ServerName.Size && Host.Size){
        bool Numeric=memchr(Host.Data,':',Host.Size)!=NULL;
        if(!Numeric && Host.Size<32){char Text[32];memcpy(Text,Host.Data,Host.Size);Text[Host.Size]=0;xnetaddr A;
            xerror* Saved=xrtTakeError();Numeric=xrtNetAddrParse(&A,Text,1);xrtSetErrorTake(Saved);}
        if(Numeric){if(!S.VerifyName.Size)S.VerifyName=Host;}else S.ServerName=Host;
    }
    if(!S.ServerName.Size && !S.VerifyName.Size)return xmqttClientError(XERR_ARGUMENT,"TLS peer identity is required");
    size_t Bytes=S.ProtocolCount*sizeof(xstrview);
    if(!xrtMemRangeValid(S.Protocols,Bytes) || xrtMemRangesOverlap(S.Protocols,Bytes,Out,sizeof(*Out)) || Bytes>K->MaxMetadataBytes)
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS ALPN descriptors");
    for(size_t I=0;I<S.ProtocolCount+2;++I){xstrview V=I==0?S.ServerName:I==1?S.VerifyName:S.Protocols[I-2];
        if(V.Size>K->MaxMetadataBytes-Bytes || !xrtMemRangeValid(V.Data,V.Size) || xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out)) ||
           (I<2 && V.Size && memchr(V.Data,0,V.Size)))return xmqttClientError(XERR_ARGUMENT,"invalid or excessive TLS metadata");
        Bytes+=V.Size;
    }
    if(K->ClientAuth && (!xrtMemRangeValid(K->ClientAuth,sizeof(*K->ClientAuth)) || xrtMemRangesOverlap(K->ClientAuth,sizeof(*K->ClientAuth),Out,sizeof(*Out))))
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS client authentication descriptor");
    Plan->Session=K->ClientAuth?xrtTlsClientCreateAuthV1(&S,K->ClientAuth,NULL):xrtTlsClientCreate(&S,NULL);
    if(!Plan->Session)return false;
    Plan->Config=K->Stream;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    Plan->Capture=K->Session.ResumeLimit!=0 && !K->ClientAuth;
#endif
    return true;
}
#endif
static bool xmqttClientDialCreate(xnetengine* Engine,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const void* Auth,const xmqttpacket* Connect,xmqttclient** Out,double Entered,const void* TLS)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Dial && (!xrtMemRangeValid(Dial,sizeof(*Dial)) || xrtMemRangesOverlap(Dial,sizeof(*Dial),Out,sizeof(*Out)))))
        return xmqttClientError(XERR_ARGUMENT,"invalid Dial config/output");
    xmqttclientdialconfig K;if(Dial)K=*Dial;else xmqttClientDialConfigInit(&K);
    if(K.Size!=sizeof(K) || (K.Family!=XNET_FAMILY_UNSPEC && K.Family!=XNET_FAMILY_IPV4 && K.Family!=XNET_FAMILY_IPV6) ||
       K.Timeout<XRT_WAIT_FOREVER || K.FallbackDelay<0 || !K.MaxAttempts || K.MaxAttempts>XMQTT_DIAL_ADDRESSES_MAX || !K.MaxHostBytes || K.MaxHostBytes>XMQTT_DIAL_HOST_MAX)
        return xmqttClientError(XERR_ARGUMENT,"invalid Dial family, timeout or limits");
    if(Addresses){size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;
        if(!Count || Count>XMQTT_DIAL_ADDRESSES_MAX)return xmqttClientError(XERR_ARGUMENT,"invalid Dial endpoint count");
        for(size_t I=0;I<Count;++I){const xnetaddr* A=xrtNetAddrListGet(Addresses,I);if(!A)return false;
            if(xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"Dial output overlaps endpoint storage");
            if(K.Family!=XNET_FAMILY_UNSPEC && A->Family!=K.Family)continue;
            if(!A->Port)return xmqttClientError(XERR_ARGUMENT,"zero Dial endpoint port");
            ++Eligible;}
        if(!Eligible)return xmqttClientError(XERR_ARGUMENT,"no eligible Dial endpoint");
    }else if(!Resolver || !Port || !Host.Size || Host.Size>K.MaxHostBytes || !xrtMemRangeValid(Host.Data,Host.Size) ||
        xrtMemRangesOverlap(Host.Data,Host.Size,Out,sizeof(*Out)) || memchr(Host.Data,0,Host.Size))return xmqttClientError(XERR_ARGUMENT,"invalid Dial host, Resolver or port");
    xmqttdialplan P={0};P.Resolver=Resolver;P.Addresses=Addresses;P.Host=Host;P.Port=Port;xrtNetDialConfigInit(&P.Config);
    P.Config.Family=K.Family;P.Config.Timeout=K.Timeout;P.Config.FallbackDelay=K.FallbackDelay;P.Config.MaxAttempts=K.MaxAttempts;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqtttlsplan T={0};if(TLS && !xmqttTlsPlanCreate(TLS,Host,Out,&T))return false;
    bool OK=xmqttClientConnectImpl(Engine,NULL,Affinity,Config,Connect,Out,Auth,Entered,&P,TLS?&T:NULL,NULL);
    if(!OK)xrtTlsSessionDestroy(T.Session);
    return OK;
#else
    return xmqttClientConnectImpl(Engine,NULL,Affinity,Config,Connect,Out,Auth,Entered,&P,TLS,NULL);
#endif
}
XMQTT_API bool xmqttClientDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttpacket* Q,xmqttclient** O)
{return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,NULL,Q,O,xrtTimer(),NULL);}
XMQTT_API bool xmqttClientDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();if(!L)return xmqttClientError(XERR_ARGUMENT,"Dial addresses are NULL");
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,NULL,Q,O,Entered,NULL);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientDialAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,&K,Q,O,Entered,NULL);
}
XMQTT_API bool xmqttClientDialAddressesAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();xmqttauthconfig K;if(!L)return xmqttClientError(XERR_ARGUMENT,"Dial addresses are NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,&K,Q,O,Entered,NULL);
}
#endif
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static bool xmqttClientTlsFactory(xnetengine* E,xnetresolver* Resolver,xnetaddrlist* L,const xnetaddr* Address,xstrview H,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* K,const xmqttclienttlsconfig* TLS,const void* Auth,const xmqttpacket* Q,xmqttclient** Out,double Entered)
{
    if(!TLS || !xrtMemRangeValid(TLS,sizeof(*TLS)) || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) ||
       xrtMemRangesOverlap(TLS,sizeof(*TLS),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS config/output");
    xnetaddrlist* Temporary=NULL;
    if(Address){if(!xrtMemRangeValid(Address,sizeof(*Address)) || xrtMemRangesOverlap(Address,sizeof(*Address),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS numeric endpoint/output");
        Temporary=xrtNetAddrListCreate(Address,1);if(!Temporary)return false;L=Temporary;}
    bool OK=xmqttClientDialCreate(E,Resolver,L,H,Port,Affinity,K,&TLS->Dial,Auth,Q,Out,Entered,TLS);
    xrtNetAddrListDestroy(Temporary);return OK;
}
XMQTT_API bool xmqttClientTlsConnect(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");
    return xmqttClientTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,NULL,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{return xmqttClientTlsFactory(E,R,NULL,NULL,H,P,A,C,T,NULL,Q,O,xrtTimer());}
XMQTT_API bool xmqttClientTlsDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");
    return xmqttClientTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,NULL,Q,O,At);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientTlsConnectAuth(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,&K,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDialAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,R,NULL,NULL,H,P,A,C,T,&K,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDialAddressesAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,&K,Q,O,At);
}
#endif
#endif
XMQTT_API xfuture* xmqttClientReady(xmqttclient* C)
{if(!C){xmqttClientError(XERR_ARGUMENT,"client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
static xmqttclientadmission xmqttClientSubmitAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out,uint8 Type)
{
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<XRT_WAIT_FOREVER ||
       P->Type!=Type || (Type==XMQTT_PACKET_PUBLISH && (((P->Flags>>1)&3u)>2 || (P->Flags&8u)))){xmqttClientError(XERR_ARGUMENT,"invalid MQTT request arguments");return XMQTT_ADMISSION_ERROR;}
    bool Subscription=Type!=XMQTT_PACKET_PUBLISH;uint8 Qos=Subscription?0:(uint8)((P->Flags>>1)&3u);bool NeedId=Subscription || Qos;uint16 Identifier=0;xmqttpacket Adapted;const xmqttpacket* Packet=P;
    if(NeedId){Adapted=*P;if(!Adapted.Identifier)Adapted.Identifier=1;Packet=&Adapted;} /* sizing only */
    bool Alias=false;uint16 AliasId=0;
    if(!Subscription && C->Config.Version==XMQTT_VERSION_5 && P->Properties.Count<=C->Config.Codec.MaxProperties && P->Properties.Count<=SIZE_MAX/sizeof(xmqttproperty) &&
       (!P->Properties.Count || P->Properties.Data) && xrtMemRangeValid(P->Properties.Data,P->Properties.Count*sizeof(xmqttproperty))){
        for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){Alias=true;AliasId=(uint16)P->Properties.Data[I].Value.Number;}}
    xmqttcodecconfig Encoding;const xmqttcodecconfig* Codec=&C->Config.Codec;
    if(Alias){Encoding=*Codec;Encoding.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES;Codec=&Encoding;}
    size_t Bytes;bool Valid=C->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(Packet,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes):xmqttPacketSize311(Packet,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes);
    if(!Valid){if(!Subscription && !P->Body.Publish.Topic.Size)xmqttClientError(XERR_ARGUMENT,"empty publish topic requires a valid outgoing alias");return XMQTT_ADMISSION_ERROR;}
    size_t Extra=xmqttRequestExtra(Bytes,Alias,xmqttClientUsesTls(C));
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest) || Extra>SIZE_MAX-sizeof(xmqttrequest)-Bytes || Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    if(NeedId){xmqttclientadmission Admission=xmqttIdentifierReserveLocked(C,P->Identifier,&Identifier);
        if(Admission!=XMQTT_ADMISSION_ACCEPTED){if(Admission==XMQTT_ADMISSION_OVERLOADED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
        Adapted.Identifier=Identifier;
    }
    if(Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xmqttIdentifierReleaseLocked(C,Identifier);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    ++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;C->Info.PrivateBytes+=Extra;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Bytes+Extra);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Type=Type;R->Retain=!Subscription && (P->Flags&1u)!=0;R->Qos=Qos;R->Identifier=Identifier;R->State.Release.Wire[0]=0x62;R->State.Release.Wire[1]=2;R->State.Release.Wire[2]=(unsigned char)(Identifier>>8);R->State.Release.Wire[3]=(unsigned char)Identifier;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    if(xmqttClientUsesTls(C))R->Capabilities|=XMQTT_TX_TLS;
    if(Alias){R->Capabilities|=XMQTT_TX_ALIAS;xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);memset(A,0,sizeof(*A));A->Alias=AliasId;}
    if(Subscription){
        R->Result.Subscription=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Subscription),&Failure);
        if(!R->Result.Subscription){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
        R->Result.Subscription->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Subscription)};
        R->State.TopicCount=P->Body.Topics.Count;R->Result.Subscription->Value.Requested=R->State.TopicCount;
        if(Type==XMQTT_PACKET_SUBSCRIBE){
            for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_SUBSCRIPTION_ID)R->Capabilities|=4u;
            for(size_t I=0;I<P->Body.Topics.Count;++I){xstrview Filter=P->Body.Topics.Data[I].Filter;
                if(Filter.Size>=7 && !memcmp(Filter.Data,"$share/",7))R->Capabilities|=2u;
                for(size_t J=0;J<Filter.Size;++J)if(Filter.Data[J]=='+' || Filter.Data[J]=='#'){R->Capabilities|=1u;break;}
            }
        }
    }else {
        R->Result.Write=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Write),&Failure);if(!R->Result.Write){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
        R->Result.Write->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Write)};
    }
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);goto Closed;}
    if(Alias){xmqttclientadmission A=xmqttSendAliasSnapshotLocked(C,R);if(A!=XMQTT_ADMISSION_ACCEPTED){xrtMutexUnlock(&C->Gate);if(A==XMQTT_ADMISSION_OVERLOADED)goto Overload;goto Failed;}}
    /* Publish no accepted request unless its worker wake is committed. The
     * post cannot run inline and cannot observe the queue until Gate unlocks. */
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    if(Alias)xmqttSendAliasCommitLocked(C,R);
    if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Out=F;return XMQTT_ADMISSION_ACCEPTED;
Closed:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_CLOSED;
Overload:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_OVERLOADED;
Failed:
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_ERROR;
UndoReservation:
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Identifier);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;C->Info.PrivateBytes-=Extra;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_PUBLISH);}
XMQTT_API xmqttclientadmission xmqttClientSubscribeAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_SUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientUnsubscribeAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_UNSUBSCRIBE);}
static xmqttclientadmission xmqttClientSubscriptionSync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out,uint8 Type)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously change subscriptions from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientSubmitAsync(C,P,Timeout,Out,Type);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API xmqttclientadmission xmqttClientSubscribe(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubscriptionSync(C,P,Timeout,Out,XMQTT_PACKET_SUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientUnsubscribe(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubscriptionSync(C,P,Timeout,Out,XMQTT_PACKET_UNSUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously publish from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientPublishAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API xmqttclientadmission xmqttClientDisconnectAsync(xmqttclient* C,const xmqttpacket* Packet,int64 Timeout,xfuture** Out)
{
    double Deadline=Timeout?xmqttClientDeadline(xrtTimer(),Timeout):-INFINITY;xmqttpacket Default;
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<0 || (Packet && !xrtMemRangeValid(Packet,sizeof(*Packet)))){xmqttClientError(XERR_ARGUMENT,"invalid graceful disconnect arguments");return XMQTT_ADMISSION_ERROR;}
    if(!Packet){xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Default);Packet=&Default;}
    if(Packet->Type!=XMQTT_PACKET_DISCONNECT){xmqttClientError(XERR_ARGUMENT,"expected DISCONNECT packet");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(!C->Info.Ready){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"graceful disconnect requires accepted CONNACK");return XMQTT_ADMISSION_ERROR;}
    xmqttnegotiated Peer=C->Peer;xrtMutexUnlock(&C->Gate);size_t Bytes=0;xmqttnegotiationissue Issue;
    if(xmqttNegotiatedPacketCheck(&Peer,Packet,&C->Config.Codec,&Bytes,&Issue)!=XMQTT_NEGOTIATION_DONE){xmqttClientError(XERR_ARGUMENT,"DISCONNECT violates packet or negotiated limits");return XMQTT_ADMISSION_ERROR;}
    size_t Extra=xmqttRequestExtra(Bytes,false,xmqttClientUsesTls(C));
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && C->Auth->Active){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
#endif
    if(C->DisconnectReserved || C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest) || Extra>SIZE_MAX-sizeof(xmqttrequest)-Bytes || Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    C->DisconnectReserved=true;++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;C->Info.PrivateBytes+=Extra;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Bytes+Extra);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttclientadmission Admission=XMQTT_ADMISSION_ERROR;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Type=XMQTT_PACKET_DISCONNECT;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    if(xmqttClientUsesTls(C))R->Capabilities|=XMQTT_TX_TLS;
    R->Result.Write=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Write),&Failure);
    if(!R->Result.Write){if(Failure==XMQTT_STATUS_LIMIT)Admission=XMQTT_ADMISSION_OVERLOADED;goto Failed;}
    R->Result.Write->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Write)};R->Result.Write->Value.Reason=Packet->Reason;
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);Admission=XMQTT_ADMISSION_CLOSED;goto Failed;}
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    C->DisconnectReserved=false;C->Disconnect=R;C->ApplicationClose=true;C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=XMQTT_STATUS_OK;C->Info.LocalReason=Packet->Reason;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Out=F;return XMQTT_ADMISSION_ACCEPTED;
Failed:
    xrtMutexLock(&C->Gate);C->DisconnectReserved=false;if(Admission!=XMQTT_ADMISSION_ERROR)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result.Write)xmqttWriteFree(R->Result.Write,NULL);xmqttRequestDrop(R);return Admission;
UndoReservation:
    xrtMutexLock(&C->Gate);C->DisconnectReserved=false;--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;C->Info.PrivateBytes-=Extra;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientDisconnect(xmqttclient* C,const xmqttpacket* Packet,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously disconnect from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientDisconnectAsync(C,Packet,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* C,xmqttmessage** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid message output");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox)return xmqttClientInboxTake(C->Inbox,Out);
#endif
    xrtMutexLock(&C->Gate);xmqttmessage* M=C->MessageHead;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && !C->Auth->Verified)M=NULL;
#endif
    if(M){C->MessageHead=M->Next;if(!C->MessageHead)C->MessageTail=NULL;--C->Info.Messages;C->Info.MessageBytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&C->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInfo(xmqttclient* C,xmqttclientinfo* Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid client info output");
    xrtMutexLock(&C->Gate);xmqttclientinfo I=C->Info;xrtMutexUnlock(&C->Gate);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){xrtMutexLock(&C->Inbox->Gate);I.Messages=C->Inbox->Count;I.MessageBytes=C->Inbox->Bytes;xrtMutexUnlock(&C->Inbox->Gate);}
#endif
    xrtMutexLock(&C->Quota->Gate);I.Objects=C->Quota->Count;I.ObjectBytes=C->Quota->Bytes;xrtMutexUnlock(&C->Quota->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientClose(xmqttclient* C)
{
    if(!C)return xmqttClientError(XERR_ARGUMENT,"client is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.State==XMQTT_CLIENT_CLOSED){xrtMutexUnlock(&C->Gate);return true;}
    if(C->ApplicationClose && C->Info.CloseStatus==XMQTT_STATUS_OK)C->Info.CloseStatus=XMQTT_STATUS_CLOSED;
    C->AbortRequested=true;
    if(C->Info.State<XMQTT_CLIENT_CLOSING){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=XMQTT_STATUS_CLOSED;}
    bool OK=xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);return OK;
}
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* C,int64 Timeout)
{
    if(!C || Timeout<XRT_WAIT_FOREVER){xmqttClientError(XERR_ARGUMENT,"invalid client wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot wait from an Engine worker");return XWAIT_ERROR;}
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!xrtMutexLock(&C->RetirementLock))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_OK;
    while(xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE)!=1){
        if(!Timeout || xrtTimer()>=Deadline){Result=XWAIT_TIMEOUT;break;}
        Result=xrtCondWaitFor(&C->RetirementReady,&C->RetirementLock,xmqttClientRemaining(Deadline));
        if(Result!=XWAIT_OK && Result!=XWAIT_TIMEOUT)break;
    }
    bool Retired=xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE)==1;
    if(!xrtMutexUnlock(&C->RetirementLock))return XWAIT_ERROR;
    if(Result==XWAIT_ERROR)return Result;
    if(!Retired)return XWAIT_TIMEOUT;
    /* Never acquire Gate under RetirementLock: failed admission may drop its
     * last asynchronous reference while Gate is held. Caller owner stays live. */
    xrtMutexLock(&C->Gate);bool Closed=C->Info.State==XMQTT_CLIENT_CLOSED && !C->Info.Requests && !C->Info.StartupBytes;xrtMutexUnlock(&C->Gate);
    if(!Closed){xmqttClientError(XERR_STATE,"client retired before logical close");return XWAIT_ERROR;}
    /* Logical Client publication can precede the actual native callback tail.
     * Carry only the original budget, rounded to the public millisecond unit. */
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(!C->Stream)return XWAIT_OK;
#endif
    return xrtNetStreamWaitRetired(C->Stream,(!Timeout || xrtTimer()>=Deadline)?0:xmqttClientRemaining(Deadline));
}
XMQTT_API bool xmqttClientDestroy(xmqttclient* C)
{
    if(!C)return true;
    if(xrtNetEngineCurrent(C->Engine))return xmqttClientError(XERR_STATE,"cannot destroy from an Engine worker");
    if(!xmqttClientClose(C) || xmqttClientWaitFor(C,XRT_WAIT_FOREVER)!=XWAIT_OK)return false;
    xmqttClientFree(C);return true;
}
#endif
