#include <xmqtt/client_store.h>
#include <xrt/memory.h>
#include "../common/auth_peer.h"

static bool demoWall(ptr User,uint64* Milliseconds)
{
    *Milliseconds=*(const uint64*)User;
    return true;
}

static bool verify(xmqttclient* C,xmqttauthphase Phase)
{
    for(unsigned I=0;I<3;++I){
        xmqttauthticket* Ticket=mqttAuthTicket(C);
        if(xmqttAuthPhase(Ticket)==XMQTT_AUTH_CHALLENGE){
            mqttAuthAnswer(Ticket,false);
            continue;
        }
        const unsigned char Expected[]={'p',0,255,'f'};
        const xmqttpacket* Message=xmqttMessagePacket(xmqttAuthMessage(Ticket));
        bool Matched=false;
        for(size_t J=0;J<Message->Properties.Count;++J){
            const xmqttproperty* P=Message->Properties.Data+J;
            if(P->Id==XMQTT_PROPERTY_AUTH_DATA){
                xbytesview Data=P->Value.Binary;
                Matched=Data.Size==sizeof(Expected)&&!memcmp(Data.Data,Expected,sizeof(Expected));
            }
        }
        bool Accepted=xmqttAuthPhase(Ticket)==Phase&&Matched;
        bool OK=xmqttAuthComplete(Ticket,Accepted)==XMQTT_ADMISSION_ACCEPTED;
        xmqttAuthRelease(Ticket);
        return OK&&Accepted;
    }
    return false;
}

int main(void)
{
    str Path=NULL;
    xfile File=xrtFileTemp(NULL,"xmqtt-auth-demo-",NULL,&Path);
    if(!File)return 1;
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Generation=1;
    JC.Binding=XRT_BYTES_LITERAL("local-demo/broker/principal");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Binding=JC.Binding;
    SC.Generation=1;
    SC.ClientId=XRT_STR_LITERAL("auth-client");
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttsession* Session=NULL;
    xmqttstorewrite* Writer=NULL;
    xmqttclient* Client=NULL;
    xtaskpool* Pool=NULL;
    xfuture *Ready=NULL,*Ending=NULL;
    mqttAuthPeer Peer;
    bool Running=false,OK=false;

    if(xmqttJournalCreate(File,&JC,&Journal)!=XMQTT_JOURNAL_DONE||
       xmqttStoreStateRecover(Journal,&SC,&State)!=XMQTT_STORE_DONE)goto Cleanup;
    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    Pool=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    if(!Pool||!xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer))goto Cleanup;
    Journal=NULL;
    State=NULL;
    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.ClientId=SC.ClientId;
    if(!xmqttSessionCreate(&SK,&Session))goto Cleanup;

    mqttAuthStart(&Peer,0);
    Running=true;
    /* Deterministic clock and assertion tokens for this one local connection. */
    uint64 Wall=1000;
    xmqttclientstoreconfig Store={sizeof(Store),Writer,demoWall,&Wall};
    xmqttproperty Props[2];
    xmqttpacket Connect=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);
    if(!xmqttClientConnectStoreAuth(Peer.Engine,&Peer.Address,1,NULL,NULL,Session,&Store,&Connect,&Client))goto Cleanup;
    Ready=xmqttClientReady(Client);
    if(!verify(Client,XMQTT_AUTH_VERIFY_CONNECT)||xrtFutureWaitFor(Ready,10000)!=XWAIT_OK||
       xrtFutureState(Ready)!=XFUTURE_RESOLVED||
       ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status!=XMQTT_STATUS_OK)goto Cleanup;

    xmqttstorewriteinfo Info;
    if(!xmqttStoreWriteInfo(Writer,&Info)||Info.State.Transactions!=1)goto Cleanup;
    if(xmqttClientDisconnect(Client,NULL,10000,&Ending)!=XMQTT_ADMISSION_ACCEPTED||
       xrtFutureState(Ending)!=XFUTURE_RESOLVED)goto Cleanup;
    const xmqttwriteresult* Closed=xrtFutureValue(Ending);
    xmqttclientstoreoutcome Outcome;
    if(Closed->Status!=XMQTT_STATUS_OK||!xmqttClientStoreOutcome(Closed,&Outcome)||
       Outcome.End.Effect!=XMQTT_JOURNAL_COMMITTED||Outcome.End.Sequence!=2)goto Cleanup;
    OK=true;

Cleanup:
    xrtFutureDestroy(Ending);
    xrtFutureDestroy(Ready);
    if(Client&&!xmqttClientDestroy(Client))return 1;
    if(Running)mqttAuthStop(&Peer);
    if(Session&&!xmqttSessionDestroy(Session))return 1;
    if(Writer&&!xmqttStoreWriteDestroy(Writer))return 1;
    if(Pool&&(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,10000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool)))return 1;
    xmqttStoreStateDestroy(State);
    if(Journal&&!xmqttJournalDestroy(Journal))return 1;
    if(!xrtClose(File)||!xrtFileDelete(Path))OK=false;
    xrtFree(Path);
    if(OK)puts("Final proof accepted, durable OPEN committed, and graceful END committed.");
    return OK?0:1;
}
