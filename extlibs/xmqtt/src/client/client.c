#include <xmqtt/client.h>
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
typedef struct xmqttstartup {xatomic32 Refs;xmqttclient* Client;size_t Bytes;unsigned char Data[];} xmqttstartup;
struct xmqttmessage {xatomic32 Refs;xmqttbox Box;xmqttpacket Packet;xmqttmessage* Next;xmqttproperty Props[];};
struct xmqttrequest {
    xatomic32 Refs,NativeRefs;
    xmqttclient* Client;xmqttrequest *Next,*Previous,*HashNext;
    xpromise* Promise;xcancel* Cancel;xmqttwritebox* Result;
    size_t Bytes,Offset;double Deadline;bool Retain,Credit,Acknowledged;
    uint8 Qos;uint16 Identifier;
    unsigned char Wire[];
};
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
    xmqttreceiveflow* ReceiveFlow;
    xmqttmessage *MessageHead,*MessageTail;
    xmqttnegotiated Peer;xerror* Cause;
    double SetupDeadline,NextPing,PingDeadline,TimerAt,DrainDeadline;
    uint64 Timer;unsigned PingOffset;bool PingOutstanding;
    xmutex RetirementLock;xcond RetirementReady;
    xmqttproperty Scratch[];
};
static void xmqttClientWork(xnetworker* Worker,ptr Data);
static bool xmqttClientWakeLocked(xmqttclient* C);
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause);
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
{xrtMutexLock(&C->Gate);bool B=C->Info.State>=XMQTT_CLIENT_CLOSING;xrtMutexUnlock(&C->Gate);return B;}
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
static xmqttmessage* xmqttMessageCopy(xmqttclient* C,const xmqttpacket* P,xmqttclientstatus* Failure)
{
    size_t Count=P->Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttmessage))/sizeof(xmqttproperty) ||
       P->Wire.Size>SIZE_MAX-sizeof(xmqttmessage)-Count*sizeof(xmqttproperty)){*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    size_t Bytes=sizeof(xmqttmessage)+Count*sizeof(xmqttproperty)+P->Wire.Size;
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
    if(P->Type==XMQTT_PACKET_PUBLISH){M->Packet.Body.Publish.Topic=xmqttRebaseText(P->Body.Publish.Topic,P->Wire.Data,Wire);M->Packet.Body.Publish.Payload=xmqttRebaseBytes(P->Body.Publish.Payload,P->Wire.Data,Wire);}
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
{xmqttmessage* M=(xmqttmessage*)Message;if(M && xrtAtomic32FetchSub(&M->Refs,1,XMEMORY_ACQ_REL)==1)xmqttBoxFree(M,M->Box);}
static void xmqttReadyFree(ptr Value,ptr Context)
{(void)Context;xmqttreadybox* B=Value;xmqttMessageRelease(B->Value.Connack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttWriteFree(ptr Value,ptr Context)
{(void)Context;xmqttwritebox* B=Value;xmqttMessageRelease(B->Value.Puback);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
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
static void xmqttRequestRef(xmqttrequest* R)
{(void)xrtAtomic32FetchAdd(&R->Refs,1,XMEMORY_RELAXED);}
static void xmqttRequestDrop(const void* Data)
{
    xmqttrequest* R=(xmqttrequest*)Data;if(xrtAtomic32FetchSub(&R->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=R->Client;size_t Bytes=R->Bytes;uint16 Id=R->Identifier;xrtCancelDestroy(R->Cancel);xrtFree(R);
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Id);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
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
static void xmqttRequestFinish(xmqttclient* C,xmqttrequest** Link,xmqttclientstatus Status)
{
    xmqttrequest* R=*Link;*Link=R->Next;xmqttwritebox* B=R->Result;xpromise* P=R->Promise;
    if(C->Partial==R)C->Partial=NULL;
    if(R->Next)R->Next->Previous=R->Previous;else C->RequestTail=R->Previous;
    xmqttRequestMapRemove(C,R);if(R->Credit)--C->SendInflight;
    B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Delivery=R->Offset?XMQTT_MAY_HAVE_REACHED_PEER:XMQTT_NOT_SENT;
    B->Value.Identifier=R->Identifier;
    if(R->Acknowledged)B->Value.Delivery=B->Value.Reason<0x80?XMQTT_PEER_ACCEPTED:XMQTT_PEER_REJECTED;
    if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    if(!xrtPromiseResolveOwned(P,B,xmqttWriteFree,NULL))xmqttWriteFree(B,NULL);
    xrtPromiseDestroy(P);xmqttRequestDrop(R);xmqttClientNotify(C);
}
static void xmqttClientFailReason(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause,uint8 Reason)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XMQTT_CLIENT_CLOSING){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=Status;
        if(Status==XMQTT_STATUS_PROTOCOL)++C->Info.ProtocolErrors;
        if(Cause)C->Cause=xrtErrorRef(Cause);
        if(C->Config.Version==XMQTT_VERSION_5 && C->Info.Ready)C->Info.LocalReason=Reason;
    }
    xrtMutexUnlock(&C->Gate);
}
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause)
{xmqttClientFailReason(C,Status,Cause,Status==XMQTT_STATUS_PROTOCOL?0x82:0);}
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
{(void)S;(void)Result;xmqttclient* C=Data;C->NativeClosed=true;xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,Cause);xmqttClientWake(C);}
static const xnetstreameventsownershipv1 xmqttClientEvents={sizeof(xnetstreameventsownershipv1),
    {xmqttClientOpen,xmqttClientRead,xmqttClientEnd,NULL,xmqttClientLow,xmqttClientDrain,xmqttClientClosed},xmqttClientDrop,&xmqttPrivateOps};
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
    xrtFree(C->Input);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=C->InputCapacity;xrtMutexUnlock(&C->Gate);
    C->Input=P;C->InputCapacity=Capacity;return true;
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
static bool xmqttClientFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!C->Info.Ready){
        if(P->Type!=XMQTT_PACKET_CONNACK || !C->Startup || C->SetupOffset!=C->Startup->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttmessage* M=xmqttMessageCopy(C,P,&Failure);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=C->Config.Version;K.Codec=C->Config.Codec;
        xmqttnegotiated N;xmqttnegotiationissue Issue;
        xmqttnegotiationstatus S=xmqttNegotiate(&C->SetupPacket,&M->Packet,&K,&N,&Issue);
        if(S<0){xmqttMessageRelease(M);xmqttClientFail(C,S==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_STATUS_SESSION:S==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;}
        if(N.Accepted && N.SessionPresent){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}
        xmqttreadybox* B=C->ReadyBox;B->Value.Connack=M;B->Value.Reason=P->Reason;
        if(N.ClientId.Size && !N.ServerAssignedId)N.ClientId=(xstrview){B->Id,N.ClientId.Size};
        B->Value.Negotiated=N;C->Peer=N;
        if(S==XMQTT_NEGOTIATION_REJECTED){xmqttClientFail(C,XMQTT_STATUS_REJECTED,NULL);return false;}
        xrtMutexLock(&C->Gate);C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;xrtMutexUnlock(&C->Gate);
        C->NextPing=N.KeepAlive?xrtTimer()+N.KeepAlive:INFINITY;xmqttReadyFinish(C,XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PINGRESP){C->PingOutstanding=false;C->PingDeadline=INFINITY;xrtMutexLock(&C->Gate);++C->Info.Pongs;xrtMutexUnlock(&C->Gate);return true;}
    if(P->Type==XMQTT_PACKET_PUBACK){
        /* The codec already limits PUBACK properties to Reason String/User. */
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        if(!R || R->Qos!=1 || !R->Credit || R->Offset!=R->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        R->Acknowledged=true;R->Result->Value.Reason=P->Reason;
        R->Result->Value.Puback=xmqttMessageCopy(C,P,&Failure);
        if(!R->Result->Value.Puback){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,P->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_DISCONNECT){xrtMutexLock(&C->Gate);C->Info.ServerReason=P->Reason;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_SERVER_DISCONNECT,NULL);return false;}
    if(P->Type!=XMQTT_PACKET_PUBLISH){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x94);return false;}
    unsigned Qos=(P->Flags>>1)&3u;
    if(Qos>1){xmqttClientFail(C,XMQTT_STATUS_UNSUPPORTED,NULL);return false;}
    if(Qos && !xmqttReceivePrepare(C))return false;
    xmqttmessage* M=xmqttMessageCopy(C,P,&Failure);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    xrtMutexLock(&C->Gate);
    bool Full=C->Info.Messages==C->Config.MaxMessages || M->Box.Bytes>C->Config.MaxMessageBytes-C->Info.MessageBytes;
    if(!Full){if(C->MessageTail)C->MessageTail->Next=M;else C->MessageHead=M;C->MessageTail=M;++C->Info.Messages;C->Info.MessageBytes+=M->Box.Bytes;++C->Info.Received;}
    xrtMutexUnlock(&C->Gate);
    if(Full){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    if(Qos){xmqttreceiveflow* F=C->ReceiveFlow;F->Identifiers[(F->Head+F->Count)%F->Capacity]=P->Identifier;++F->Count;}
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
                if(S!=XMQTT_CODEC_DONE){xmqttClientFailReason(C,S==XMQTT_CODEC_LOCAL_ERROR?XMQTT_STATUS_LIMIT:XMQTT_STATUS_PROTOCOL,xrtGetError(),S==XMQTT_CODEC_LOCAL_ERROR?0:0x81);return;}
                if(P.Type==XMQTT_PACKET_PUBLISH && ((P.Flags>>1)&3u)==1 && C->ReceiveFlow){
                    if(C->ReceiveFlow->Count>=C->Peer.ReceiveMaximum){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x93);return;}
                    /* A local ACK bound is backpressure, not permission to drop
                     * the already buffered frame or acknowledge it early. */
                    if(C->ReceiveFlow->Count==C->ReceiveFlow->Capacity){C->ReceivePaused=true;return;}
                }
                if(!xmqttClientFrame(C,&P))return;
                C->InputBytes-=Needed;if(C->InputBytes)memmove(C->Input,C->Input+Needed,C->InputBytes);++Packets;continue;
            }
        }
        if(Needed>C->InputCapacity && !xmqttClientGrow(C,Needed))return;
        if(Read==C->Config.ReadBytes || !xrtNetStreamAvailable(C->Stream))break;
        size_t Count=C->InputCapacity-C->InputBytes;
        if(Count>C->Config.ReadBytes-Read)Count=C->Config.ReadBytes-Read;
        size_t N=xrtNetStreamRead(C->Stream,C->Input+C->InputBytes,Count);if(!N)break;
        Read+=N;C->InputBytes+=N;
    }
    if(Packets==C->Config.WorkPackets || (Read==C->Config.ReadBytes && xrtNetStreamAvailable(C->Stream)))xmqttClientWake(C);
}
static bool xmqttClientSendStartup(xmqttclient* C,size_t* Budget)
{
    xmqttstartup* W=C->Startup;
    while(W && C->SetupOffset<W->Bytes && *Budget){
        size_t N=W->Bytes-C->SetupOffset,Available=xrtNetStreamWritable(C->Stream);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xrtNetStreamSendRef(C->Stream,W->Data+C->SetupOffset,N,xmqttStartupRelease,W);
        if(S!=XNET_RESULT_OK){xmqttStartupDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        C->SetupOffset+=N;*Budget-=N;
    }
    if(W && C->SetupOffset<W->Bytes && !*Budget)xmqttClientWake(C);
    return W && C->SetupOffset==W->Bytes;
}
static bool xmqttClientSendPing(xmqttclient* C,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<2){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    static const unsigned char Ping[]={0xc0,0};
    while(C->PingOffset<2 && *Budget){
        size_t N=2-C->PingOffset,Available=xrtNetStreamWritable(C->Stream);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(!N)return false;
        xnetresult S=xrtNetStreamSend(C->Stream,Ping+C->PingOffset,N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(!C->PingOffset){C->PingOutstanding=true;C->PingDeadline=xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout);}
        C->PingOffset+=(unsigned)N;*Budget-=N;
    }
    if(C->PingOffset!=2){xmqttClientWake(C);return false;}
    C->PingOffset=0;C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    xrtMutexLock(&C->Gate);++C->Info.Pings;xrtMutexUnlock(&C->Gate);return true;
}
static bool xmqttClientSendAcks(xmqttclient* C,size_t* Budget)
{
    xmqttreceiveflow* F=C->ReceiveFlow;
    while(F && F->Count && *Budget){uint16 Id=F->Identifiers[F->Head];
        const unsigned char Ack[]={0x40,2,(unsigned char)(Id>>8),(unsigned char)Id};
        if(C->Peer.SendPacketBytes<sizeof(Ack)){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
        size_t N=sizeof(Ack)-F->Offset,Available=xrtNetStreamWritable(C->Stream);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetresult S=xrtNetStreamSend(C->Stream,Ack+F->Offset,N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        F->Offset+=(unsigned)N;*Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        if(F->Offset==sizeof(Ack)){F->Offset=0;F->Head=(F->Head+1)%F->Capacity;--F->Count;
            if(C->ReceivePaused){C->ReceivePaused=false;xmqttClientWake(C);}}
    }
    if(F && F->Count){xmqttClientWake(C);return false;}return true;
}
static bool xmqttClientSendRequest(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    while(R->Offset<R->Bytes && *Budget){size_t N=R->Bytes-R->Offset,Available=xrtNetStreamWritable(C->Stream);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        xmqttRequestRef(R);(void)xrtAtomic32FetchAdd(&R->NativeRefs,1,XMEMORY_RELAXED);
        xnetresult S=xrtNetStreamSendRef(C->Stream,R->Wire+R->Offset,N,xmqttRequestNativeDrop,R);
        if(S!=XNET_RESULT_OK){(void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_RELAXED);xmqttRequestDrop(R);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
        R->Offset+=N;*Budget-=N;C->Partial=R->Offset==R->Bytes?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->Offset!=R->Bytes){xmqttClientWake(C);return false;}return true;
}
static void xmqttClientSendPump(xmqttclient* C)
{
    size_t Budget=C->Config.WriteBytes;
    if(!C->Info.Ready)return;
    /* Finish the one partial packet before inserting any other packet, even
     * when a formerly blocked earlier request has just gained send credit. */
    if(C->Partial && !xmqttClientSendRequest(C,C->Partial,&Budget))return;
    if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
    if(!xmqttClientSendAcks(C,&Budget))return;
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){
        xmqttrequest* R=*Link;
        if(R->Offset==R->Bytes){Link=&R->Next;continue;}
        if(R->Bytes>C->Peer.SendPacketBytes || R->Qos>C->Peer.MaximumQos || (R->Retain && !C->Peer.RetainAvailable)){xmqttRequestFinish(C,Link,XMQTT_STATUS_RESTRICTED);continue;}
        if(R->Qos && !R->Credit && C->SendInflight>=C->Peer.SendReceiveMaximum){Link=&R->Next;continue;}
        if(!xmqttClientSendRequest(C,R,&Budget))return;
        if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;
    }
    if(!C->PingOutstanding && xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
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
    if(Abort)(void)xrtNetStreamAbort(C->Stream);
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
    while(Offset<sizeof(Wire) && Budget){size_t N=sizeof(Wire)-Offset,Available=xrtNetStreamWritable(C->Stream);
        if(N>Available)N=Available;
        if(N>Budget)N=Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return;
        xnetresult S=xrtNetStreamSend(C->Stream,Wire+Offset,N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientDrainStop(C,true);return;}
        Offset+=N;Budget-=N;xrtMutexLock(&C->Gate);C->Info.DisconnectBytes=Offset;xrtMutexUnlock(&C->Gate);
    }
    if(Offset==sizeof(Wire)){C->DrainClosing=true;if(!xrtNetStreamClose(C->Stream))xmqttClientDrainStop(C,true);}
    else xmqttClientWake(C);
}
static void xmqttClientStop(xmqttclient* C)
{
    if(!C->Stopped){C->Stopped=true;
        if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
        xmqttReadyFinish(C,C->Info.CloseStatus);xmqttStartupRetire(C);
        xrtMutexLock(&C->Gate);bool Diagnostic=C->Info.LocalReason && !C->AbortRequested;xrtMutexUnlock(&C->Gate);
        /* Never append a diagnostic inside a partial MQTT packet. Already
         * fully accepted output frames may drain ahead of the final packet. */
        C->Draining=Diagnostic && !C->Partial && !C->PingOffset && !(C->ReceiveFlow && C->ReceiveFlow->Offset) &&
            !C->ReadEnded && !C->NativeClosed && C->Peer.SendPacketBytes>=3 && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN;
        if(C->Draining){C->DrainDeadline=xmqttClientDeadline(xrtTimer(),C->Config.CloseTimeout);xmqttClientSchedule(C,C->DrainDeadline);
            if(!C->Timer)C->Draining=false;else {xrtMutexLock(&C->Gate);C->Info.DrainStarted=true;xrtMutexUnlock(&C->Gate);}}
        if(!C->Draining)(void)xrtNetStreamAbort(C->Stream);
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
    xrtMutexLock(&C->Gate);C->Posted=false;bool Started=C->Started;xrtMutexUnlock(&C->Gate);
    if(!Started){xmqttClientDrop(C);return;}
    xmqttClientCollect(C);
    if(C->ReadyPromise && xrtCancelRequested(C->ReadyCancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    double Now=xrtTimer();
    if(!C->Info.Ready && Now>=C->SetupDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(C->PingOutstanding && Now>=C->PingDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){xmqttrequest* R=*Link;
        xmqttclientstatus S=xrtCancelRequested(R->Cancel)?XMQTT_STATUS_CANCELLED:Now>=R->Deadline?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK;
        if(S!=XMQTT_STATUS_OK){bool Partial=R->Offset!=0;xmqttRequestFinish(C,Link,S);if(Partial)xmqttClientFail(C,S,NULL);continue;}
        if(!R->Qos && R->Offset==R->Bytes && !xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE)){
            if(xrtNetStreamState(C->Stream)!=XNET_STREAM_OPEN || C->ReadEnded){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);break;}
            xmqttRequestFinish(C,Link,XMQTT_STATUS_OK);continue;
        }
        Link=&R->Next;
    }
    if(!xmqttClientClosing(C) && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN){
        /* Startup must be queued before an immediate/coalesced CONNACK. */
        if(!C->Info.Ready){size_t Budget=C->Config.WriteBytes;(void)xmqttClientSendStartup(C,&Budget);}
        if(!xmqttClientClosing(C))xmqttClientReadPump(C);
        if(!xmqttClientClosing(C) && !C->ReadEnded)xmqttClientSendPump(C);
        if(C->Info.Ready && !C->PingOutstanding && xrtTimer()>=C->NextPing)
            for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Offset && R->Offset<R->Bytes){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);break;}
    }
    if(C->ReadEnded || C->NativeClosed)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);
    if(xmqttClientClosing(C))xmqttClientStop(C);
    else {double At=C->Info.Ready?(C->PingOutstanding?C->PingDeadline:C->NextPing):C->SetupDeadline;
        for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Deadline<At)At=R->Deadline;
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
    xrtNetStreamDestroy(C->Stream);xrtFutureDestroy(C->ReadyFuture);xrtCancelDestroy(C->ReadyCancel);xrtErrorFree(C->Cause);
    xrtFree(C->Input);xrtFree(C->ReceiveFlow);xrtFree(xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_RELAXED));xmqttQuotaDrop(C->Quota);(void)xrtNetEngineUnpin(C->Engine);xrtCondUnit(&C->RetirementReady);xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);
}
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out)
{
    double Now=xrtTimer();xmqttclientconfig K;if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid client config");K=*Config;}else xmqttClientConfigInit(&K);
    if(!Engine || !Remote || !Out || !Connect || !xrtMemRangeValid(Out,sizeof(*Out)) || !xrtMemRangeValid(Connect,sizeof(*Connect)) ||
       K.Size!=sizeof(K) || (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT ||
       !K.MaxRequests || K.MaxRequests>65536 || !K.MaxOutgoingBytes || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u || !K.MaxObjectBytes ||
       !K.MaxMessages || !K.MaxMessageBytes || !K.MaxSetupBytes || K.InitialInputBytes<5 || K.InitialInputBytes>K.Codec.MaxPacketBytes ||
       !K.ReadBytes || !K.WriteBytes || !K.WriteChunkBytes || !K.WorkPackets || K.SetupTimeout<0 || K.PingTimeout<=0 || K.CloseTimeout<=0)
        return xmqttClientError(XERR_ARGUMENT,"invalid MQTT client configuration");
    if(!xrtNetStreamConfigValid(&K.Stream))return false;
    size_t WireBytes=0;bool Valid=K.Version==XMQTT_VERSION_5?xmqttPacketSize5(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes):xmqttPacketSize311(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes);
    if(!Valid)return false;
    uint32 ReceiveBytes=XMQTT_PACKET_MAX_BYTES;bool ProblemInfo=true;
    for(size_t I=0;I<Connect->Properties.Count;++I){const xmqttproperty* P=Connect->Properties.Data+I;
        if(P->Id==XMQTT_PROPERTY_AUTH_METHOD || (P->Id==XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM && P->Value.Number))return xmqttClientError(XERR_STATE,"enhanced authentication/receive aliases not implemented");
        if(P->Id==XMQTT_PROPERTY_REQUEST_PROBLEM_INFO)ProblemInfo=P->Value.Number!=0;
        if(P->Id==XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE){ReceiveBytes=P->Value.Number;if(ReceiveBytes>K.Codec.MaxPacketBytes)return xmqttClientError(XERR_ARGUMENT,"advertised receive packet size exceeds local codec limit");}
    }
    size_t SetupCount=Connect->Properties.Count+Connect->Body.Connect.WillProperties.Count;
    if(SetupCount>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty) || K.Codec.MaxProperties>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty)-SetupCount || WireBytes>K.MaxSetupBytes || WireBytes>SIZE_MAX-sizeof(xmqttstartup))return xmqttClientError(XERR_RANGE,"MQTT client setup capacity");
    size_t ClientBytes=sizeof(xmqttclient)+(SetupCount+K.Codec.MaxProperties)*sizeof(xmqttproperty),StartupBytes=sizeof(xmqttstartup)+WireBytes;
    if(ClientBytes>K.MaxPrivateBytes || K.InitialInputBytes>K.MaxPrivateBytes-ClientBytes || StartupBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes)return xmqttClientError(XERR_RANGE,"MQTT private setup budget");
    if(!xrtNetEnginePin(Engine))return false;
    xmqttclient* C=xrtMalloc(ClientBytes);if(!C){xrtNetEngineUnpin(Engine);return false;}memset(C,0,ClientBytes);
    if(!xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtMutexInit(&C->RetirementLock)){xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtCondInit(&C->RetirementReady)){xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    C->Engine=Engine;C->Config=K;C->ProblemInfo=ProblemInfo;C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));xrtAtomic32Init(&C->Refs,1);
    xrtAtomicPtrInit(&C->SendFlow,NULL);
    C->Info.Size=sizeof(C->Info);C->Info.State=XMQTT_CLIENT_CONNECTING;C->Info.PrivateBytes=ClientBytes+K.InitialInputBytes+StartupBytes;C->Info.StartupBytes=WireBytes;
    C->SetupProps=C->Scratch;C->Props=C->Scratch+SetupCount;C->Peer.ReceivePacketBytes=ReceiveBytes;
    C->SetupDeadline=xmqttClientDeadline(Now,K.SetupTimeout);C->NextPing=C->PingDeadline=C->TimerAt=INFINITY;
    C->Quota=xrtMalloc(sizeof(*C->Quota));if(!C->Quota)goto Fail;memset(C->Quota,0,sizeof(*C->Quota));
    if(!xrtMutexInit(&C->Quota->Gate)){xrtFree(C->Quota);C->Quota=NULL;goto Fail;}
    xrtAtomic32Init(&C->Quota->Refs,1);C->Quota->MaxCount=K.MaxObjects;C->Quota->MaxBytes=K.MaxObjectBytes;
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
    xrtMutexLock(&C->Gate);xmqttClientRef(C);C->Stream=xrtNetStreamConnectOwnedV1(Engine,Remote,Affinity,&K.Stream,C,&xmqttClientEvents);
    if(!C->Stream){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}
    C->Started=true;(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);*Out=C;return true;
Fail:
    xrtPromiseDestroy(C->ReadyPromise);C->ReadyPromise=NULL;if(C->ReadyBox)xmqttReadyFree(C->ReadyBox,NULL);xmqttStartupRetire(C);xmqttClientFree(C);return false;
}
XMQTT_API xfuture* xmqttClientReady(xmqttclient* C)
{if(!C){xmqttClientError(XERR_ARGUMENT,"client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<XRT_WAIT_FOREVER ||
       P->Type!=XMQTT_PACKET_PUBLISH || ((P->Flags>>1)&3u)>1 || (P->Flags&8u) || !P->Body.Publish.Topic.Size){xmqttClientError(XERR_ARGUMENT,"new QoS-0/1 publish arguments");return XMQTT_ADMISSION_ERROR;}
    uint8 Qos=(uint8)((P->Flags>>1)&3u);uint16 Identifier=0;xmqttpacket Adapted;const xmqttpacket* Packet=P;
    if(Qos){Adapted=*P;if(!Adapted.Identifier)Adapted.Identifier=1;Packet=&Adapted;} /* sizing only */
    size_t Bytes;bool Valid=C->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes):xmqttPacketSize311(Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes);
    if(!Valid)return XMQTT_ADMISSION_ERROR;
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){xmqttClientError(XERR_ARGUMENT,"outgoing alias not implemented");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest)){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    if(Qos){xmqttclientadmission Admission=xmqttIdentifierReserveLocked(C,P->Identifier,&Identifier);
        if(Admission!=XMQTT_ADMISSION_ACCEPTED){if(Admission==XMQTT_ADMISSION_OVERLOADED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
        Adapted.Identifier=Identifier;
    }
    ++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Bytes);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Retain=(P->Flags&1u)!=0;R->Qos=Qos;R->Identifier=Identifier;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    R->Result=xmqttBoxAllocate(C->Quota,sizeof(*R->Result),&Failure);if(!R->Result){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
    R->Result->Box=(xmqttbox){C->Quota,sizeof(*R->Result)};
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);goto Closed;}
    /* Publish no accepted request unless its worker wake is committed. The
     * post cannot run inline and cannot observe the queue until Gate unlocks. */
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Out=F;return XMQTT_ADMISSION_ACCEPTED;
Closed:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result)xmqttWriteFree(R->Result,NULL);xmqttRequestDrop(R);return XMQTT_ADMISSION_CLOSED;
Overload:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);xmqttRequestDrop(R);return XMQTT_ADMISSION_OVERLOADED;
Failed:
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result)xmqttWriteFree(R->Result,NULL);xmqttRequestDrop(R);return XMQTT_ADMISSION_ERROR;
UndoReservation:
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Identifier);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously publish from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientPublishAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* C,xmqttmessage** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid message output");
    xrtMutexLock(&C->Gate);xmqttmessage* M=C->MessageHead;
    if(M){C->MessageHead=M->Next;if(!C->MessageHead)C->MessageTail=NULL;--C->Info.Messages;C->Info.MessageBytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&C->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInfo(xmqttclient* C,xmqttclientinfo* Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid client info output");
    xrtMutexLock(&C->Gate);xmqttclientinfo I=C->Info;xrtMutexUnlock(&C->Gate);
    xrtMutexLock(&C->Quota->Gate);I.Objects=C->Quota->Count;I.ObjectBytes=C->Quota->Bytes;xrtMutexUnlock(&C->Quota->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientClose(xmqttclient* C)
{
    if(!C)return xmqttClientError(XERR_ARGUMENT,"client is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.State==XMQTT_CLIENT_CLOSED){xrtMutexUnlock(&C->Gate);return true;}
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
        if(xrtTimer()>=Deadline){Result=XWAIT_TIMEOUT;break;}
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
    return xrtNetStreamWaitRetired(C->Stream,xrtTimer()>=Deadline?0:xmqttClientRemaining(Deadline));
}
XMQTT_API bool xmqttClientDestroy(xmqttclient* C)
{
    if(!C)return true;
    if(xrtNetEngineCurrent(C->Engine))return xmqttClientError(XERR_STATE,"cannot destroy from an Engine worker");
    if(!xmqttClientClose(C) || xmqttClientWaitFor(C,XRT_WAIT_FOREVER)!=XWAIT_OK)return false;
    xmqttClientFree(C);return true;
}
#endif
