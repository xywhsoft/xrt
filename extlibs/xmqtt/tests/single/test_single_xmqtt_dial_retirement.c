#define XMQTT_MODULE_XMQTT_CLIENT_DIAL
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test.h"
#include "../../examples/common/dial_peer.h"
typedef struct dialObservation {xmqttclient* Client;xnetpost Post;xatomic32 Done;bool Held;} dialObservation;
static void dialObserve(xnetworker* W,ptr Data)
{
    (void)W;dialObservation* O=Data;xrtMutexLock(&O->Client->Gate);O->Held=O->Client->Dial!=NULL;xrtMutexUnlock(&O->Client->Gate);xrtAtomic32Store(&O->Done,1,XMEMORY_RELEASE);
}
static bool dialHeld(xmqttclient* C,dialObservation* O)
{
    O->Client=C;xrtAtomic32Store(&O->Done,0,XMEMORY_RELEASE);testRequire(xrtNetPost(C->Worker,&O->Post,dialObserve,O),"controller observation");mqttExampleWait(&O->Done);return O->Held;
}
int main(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){dialObservation O={0};xrtAtomic32Init(&O.Done,0);testRequire(xrtNetPostInit(&O.Post),"observation post");
        /* O outlives all callbacks through the fixture Engine join. */
        mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,Mode==2?MQTT_DIAL_BLOCK:0);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("retirement");xmqttclient* C=NULL;
        if(Mode==0){xnetaddr A[]={P.RefusedAddress,P.Peer.Address};xnetaddrlist* L=xrtNetAddrListCreate(A,2);testRequire(L&&xmqttClientDialAddresses(P.Peer.Engine,L,1,NULL,NULL,&Q,&C),"prepared fallback");xrtNetAddrListDestroy(L);}
        else testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Q,&C),"hostname Dial");
        xfuture* Ready=xmqttClientReady(C);
        if(Mode==2){mqttExampleWait(&P.Entered);testRequire(dialHeld(C,&O),"pending controller stays owned and cancellable");testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,5000)==XWAIT_OK,"cancelled subscriber physical retirement");testRequire(!dialHeld(C,&O),"cancelled controller reference detached");}
        else {testRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK&&((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"ready before live-state check");testRequire(!dialHeld(C,&O),"completed controller not retained on live connection");
            xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("retired/dial");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("live");xfuture* Sent=NULL;testRequire(xmqttClientPublish(C,&Q,5000,&Sent)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)xrtFutureValue(Sent))->Status==XMQTT_STATUS_OK,"live traffic after controller retirement");xrtFutureDestroy(Sent);}
        testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,5000)==XWAIT_OK&&xmqttClientDestroy(C),"Client physical close");mqttDialStop(&P);testRequire(((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==(Mode==2?XMQTT_STATUS_CLOSED:XMQTT_STATUS_OK),"retained typed result");xrtFutureDestroy(Ready);
    }
    puts("[PASS] MQTT Dial live controller retirement, pending cancellation and retained results");return 0;
}
