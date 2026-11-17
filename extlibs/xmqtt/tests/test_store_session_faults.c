#include "support/store_session_peer.h"
#include <math.h>
static void sfLimits(xmqttversion V)
{
    xmqttstorestate* State=ssRecover(V,false,60);
    for(unsigned Mode=0;Mode<20;++Mode){xmqttsessionconfig K=ssSessionConfig(V);xmqttstoresessionconfig C=ssConfig(10,4000);xmqttsessionstatus Want=XMQTT_SESSION_ERROR;bool Assigned=false;
        switch(Mode){case 0:K.Size=0;break;case 1:C.Size=0;break;case 2:C.MonotonicTime=NAN;break;case 3:C.MonotonicTime=-1;break;case 4:C.WallMilliseconds=INT64_MAX+UINT64_C(1);break;
        case 5:C.Generation=8;Want=XMQTT_SESSION_STATE_ERROR;break;case 6:C.Binding=XRT_BYTES_LITERAL("wrong");Want=XMQTT_SESSION_STATE_ERROR;break;case 7:K.ClientId=XRT_STR_LITERAL("wrong");Want=XMQTT_SESSION_STATE_ERROR;break;
        case 8:K.Version=V==XMQTT_VERSION_5?XMQTT_VERSION_311:XMQTT_VERSION_5;Want=XMQTT_SESSION_STATE_ERROR;break;case 9:C.WallMilliseconds=2999;Want=XMQTT_SESSION_STATE_ERROR;break;
        case 10:K.MaxOutgoing=2;Want=XMQTT_SESSION_LIMIT;break;case 11:K.MaxIncoming=1;Want=XMQTT_SESSION_LIMIT;break;case 12:K.MaxBytes=0;Want=XMQTT_SESSION_LIMIT;break;
        case 13:K.Codec.MaxPacketBytes=2;Want=XMQTT_SESSION_LIMIT;break;case 14:K.Codec.MaxPayloadBytes=2;Want=XMQTT_SESSION_LIMIT;break;
        case 15:if(V==XMQTT_VERSION_5)C.MaxScratchBytes=0;else K.Codec.MaxPayloadBytes=0;Want=XMQTT_SESSION_LIMIT;break;
        case 16:if(V==XMQTT_VERSION_5)K.Codec.MaxProperties=3;else K.Codec.MaxPacketBytes=2;Want=XMQTT_SESSION_LIMIT;break;
        case 17:K.ClientId=(xstrview){0};break;case 18:Assigned=true;break;default:Assigned=true;K.ClientId=(xstrview){0};K.Version=XMQTT_VERSION_311;break;}
        xmqttsession* S=(xmqttsession*)(uintptr_t)1;xrtSetErrorInfo(XERR_AGAIN,"import.caller",1,"preserved on limit");const xerror* Old=xrtGetError();
        ssStatus(Assigned?xmqttStoreSessionRestoreAssigned(State,&K,&C,&S):xmqttStoreSessionRestore(State,&K,&C,&S),Want,"invalid input/identity/clock/resource caps");testRequire(S==(xmqttsession*)(uintptr_t)1,"failure/refusal never publishes Session");if(Want==XMQTT_SESSION_LIMIT)testRequire(xrtGetError()==Old,"normal refusal restores exact caller diagnostic");xrtClearError();
        testRequire(stInfo(State).Outgoing==3&&stInfo(State).Inbox==2,"failed import never mutates source obligations");}
    xmqttsessionconfig K=ssSessionConfig(V);xmqttstoresessionconfig C=ssConfig(10,4000);xmqttsession* S=(xmqttsession*)(uintptr_t)1;
    ssStatus(xmqttStoreSessionRestore(State,&K,&C,(xmqttsession**)&K),XMQTT_SESSION_ERROR,"output/config overlap rejected");xrtClearError();
    xmqttstoreview W=stView(State,XMQTT_STORE_OUTGOING,0);ssStatus(xmqttStoreSessionRestore(State,&K,&C,(xmqttsession**)(ptr)W.Wire.Data),XMQTT_SESSION_ERROR,"output cannot overwrite owned source wire");xrtClearError();
    ssStatus(xmqttStoreSessionRestore(State,&K,&C,(xmqttsession**)(ptr)stInfo(State).ClientId.Data),XMQTT_SESSION_ERROR,"output cannot overwrite bound source identity");xrtClearError();
    ssStatus(xmqttStoreSessionRestore(NULL,&K,&C,&S),XMQTT_SESSION_ERROR,"null source rejected");testRequire(S==(xmqttsession*)(uintptr_t)1,"invalid source output unchanged");xrtClearError();xmqttStoreStateDestroy(State);
}
static void sfEmpty(void)
{
    jpdrive Drive={0};jpfile F=jpFile(&Drive);xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);xmqttjournal* J=jpCreate(F,&JC);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(J,&C,&State),XMQTT_STORE_DONE,"empty journal is a valid unbound recovered view");xmqttsessionconfig K=ssSessionConfig(XMQTT_VERSION_5);xmqttstoresessionconfig IC=ssConfig(10,C.WallMilliseconds);xmqttsession* S=(xmqttsession*)(uintptr_t)1;
    ssStatus(xmqttStoreSessionRestore(State,&K,&IC,&S),XMQTT_SESSION_STATE_ERROR,"unbound view cannot invent durable identity");testRequire(S==(xmqttsession*)(uintptr_t)1,"unbound output unchanged");xrtClearError();xmqttStoreStateDestroy(State);testRequire(xmqttJournalDestroy(J),"empty journal retired");jpFileClose(F);
}
int main(void)
{sfLimits(XMQTT_VERSION_5);sfLimits(XMQTT_VERSION_311);sfEmpty();puts("[PASS]40 argument/identity/clock/count/byte/codec/scratch refusals,source aliases and unbound identity");return 0;}
