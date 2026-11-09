#define XMQTT_MODULE_XMQTT_CLIENT_AUTH
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static bool authCompletionUnlock(xmutex* Lock);
#define xrtMutexUnlock authCompletionUnlock
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtMutexUnlock
#include "../../examples/common/auth_peer.h"
static xmqttclient* authCompletionTarget;
static xatomic32 authCompletionArmed,authCompletionEntered,authCompletionRelease,authCompletionHold;
/* Test-only deterministic scheduling point after the owner dequeues final proof,
 * before it detaches the old result/producer. Capture under the real mutex. */
static bool authCompletionUnlock(xmutex* Lock)
{
    bool Stop=false;
    if(xrtAtomic32Load(&authCompletionArmed,XMEMORY_ACQUIRE) && Lock==&authCompletionTarget->Gate){
        xmqttauthstate* A=authCompletionTarget->Auth;uint32 One=1;
        if(!A->Ticket && A->Promise && A->Reauth)Stop=xrtAtomic32CompareExchange(&authCompletionArmed,&One,0,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE);
    }
    bool OK=xrtMutexUnlock(Lock);
    if(Stop){xrtAtomic32Store(&authCompletionEntered,1,XMEMORY_RELEASE);double End=xrtTimer()+5;
        while(!xrtAtomic32Load(&authCompletionRelease,XMEMORY_ACQUIRE)){mqttAuthCheck(xrtTimer()<End,"completion owner barrier");xrtThreadYield();}}
    return OK;
}
static void authCompletionNotify(xmqttclient* C,ptr User)
{
    (void)User;if(xrtAtomic32Load(&authCompletionHold,XMEMORY_ACQUIRE))return;
    xmqttauthticket* T=NULL;while(xmqttClientTakeAuth(C,&T))mqttAuthAnswer(T,false);
}
int main(void)
{
    xrtAtomic32Init(&authCompletionArmed,0);xrtAtomic32Init(&authCompletionEntered,0);xrtAtomic32Init(&authCompletionRelease,0);xrtAtomic32Init(&authCompletionHold,0);
    mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_DIRECT);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=authCompletionNotify;
    xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);xmqttclient* C=NULL;
    mqttAuthCheck(xmqttClientConnectAuth(P.Engine,&P.Address,1,&K,NULL,&Q,&C),"completion factory");xfuture* Ready=xmqttClientReady(C);
    mqttAuthCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"initial verified readiness");
    xrtAtomic32Store(&authCompletionHold,1,XMEMORY_RELEASE);Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* Old=NULL;
    mqttAuthCheck(xmqttClientReauthenticateAsync(C,&Q,5000,&Old)==XMQTT_ADMISSION_ACCEPTED,"old reauth admission");xmqttauthticket* T=mqttAuthTicket(C);
    authCompletionTarget=C;xrtAtomic32Store(&authCompletionArmed,1,XMEMORY_RELEASE);mqttAuthCheck(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"accept old final proof");xmqttAuthRelease(T);
    mqttAuthWait(&authCompletionEntered,1);xfuture* Next=(xfuture*)(uintptr_t)1;xmqttclientadmission Admission=xmqttClientReauthenticateAsync(C,&Q,5000,&Next);
    if(Admission!=XMQTT_ADMISSION_OVERLOADED || Next!=(xfuture*)(uintptr_t)1){fprintf(stderr,"[FAIL] completion reservation lost: admission=%d\n",Admission);xrtAtomic32Store(&authCompletionRelease,1,XMEMORY_RELEASE);return 1;}
    xrtAtomic32Store(&authCompletionRelease,1,XMEMORY_RELEASE);
    mqttAuthCheck(xrtFutureWaitFor(Old,5000)==XWAIT_OK && ((const xmqttauthresult*)xrtFutureValue(Old))->Status==XMQTT_STATUS_OK && ((const xmqttauthresult*)xrtFutureValue(Old))->Final,"old producer/result cannot be replaced before completion");
    mqttAuthCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"completion producer physically retired");mqttAuthStop(&P);xrtFutureDestroy(Old);xrtFutureDestroy(Ready);
    puts("[PASS] MQTT auth completion keeps its reservation until producer/result detachment");return 0;
}
