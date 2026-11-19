#include "../support/store_session_peer.h"
#include <xrt/time.h>
#include <stdlib.h>
/* Actual public Client+Engine; the Python peer has no xmqtt code imports. */
static const void* sscOutcome(xfuture* F)
{testRequire(F&&xrtFutureWaitFor(F,10000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"store Client bounded outcome");return xrtFutureValue(F);}
int main(int Argc,char** Argv)
{
    if(Argc!=4)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);unsigned Level=(unsigned)strtoul(Argv[2],NULL,10),Fragment=(unsigned)strtoul(Argv[3],NULL,10);if(!Port||Port>65535||(Level!=5&&Level!=311)||Fragment>1)return 2;
    xmqttversion V=Level==5?XMQTT_VERSION_5:XMQTT_VERSION_311;xmqttstorestate* State=ssRecover(V,true,60);xmqttsessionconfig SK=ssSessionConfig(V);xmqttstoresessionconfig SC=ssConfig(xrtTimer(),4000);xmqttsession* S=NULL;ssStatus(xmqttStoreSessionRestore(State,&SK,&SC,&S),XMQTT_SESSION_DONE,"committed history imports for real Client");
    xnetengineconfig EK;xrtNetEngineConfigInit(&EK);EK.Workers=2;xnetengine* E=xrtNetEngineCreate(&EK);testRequire(E&&xrtNetEngineStart(E),"native Engine started");
    xmqttclientconfig CK;xmqttClientConfigInit(&CK);CK.Version=V;if(Fragment){CK.ReadBytes=7;CK.WriteBytes=7;CK.WriteChunkBytes=3;CK.WorkPackets=1;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=SK.ClientId;xmqttproperty P={0};P.Id=XMQTT_PROPERTY_SESSION_EXPIRY;P.Type=XMQTT_PROPERTY_UINT32;P.Value.Number=60;if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&P,1};
    xnetaddr A;testRequire(xrtNetAddrLoopback(&A,XNET_FAMILY_IPV4,(uint16)Port),"literal peer address");xmqttclient* C=NULL;testRequire(xmqttClientConnectSession(E,&A,1,&CK,S,&Q,&C),"actual imported Session attached");xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=sscOutcome(Ready);testRequire(R->Status==XMQTT_STATUS_OK&&R->Negotiated.SessionPresent,"new CONNACK authorizes replay");
    char Line[16];puts("WAIT_PEER");fflush(stdout);testRequire(fgets(Line,sizeof(Line),stdin)&&!strcmp(Line,"done\n"),"independent peer completed all wire obligations");
    double End=xrtTimer()+10;xmqttsessioninfo SI;do{SI=ssInfo(S);testRequire(xrtTimer()<End,"native replay/ACK physical drain");if(SI.ActiveOutgoing||SI.Objects||SI.Incoming)xrtSleep(1);}while(SI.ActiveOutgoing||SI.Objects||SI.Incoming);
    xmqttmessage* Message=NULL;testRequire(!xmqttClientTakeMessage(C,&Message)&&stInfo(State).Inbox==2,"old receive dedup emits no second local message and does not ACK durable Inbox");
    xmqttstoreview View={0};testRequire(xmqttStoreStateView(State,XMQTT_STORE_INBOX,0,&View)==XMQTT_STORE_DONE&&View.Wire.Size,"source durable payload still readable");
    xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Bye)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)sscOutcome(Bye))->Status==XMQTT_STATUS_OK,"actual graceful close after full recovery");testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,10000)==XWAIT_OK&&xmqttClientDestroy(C),"actual Client physical retirement before owner destruction");
    testRequire(xrtNetEngineStop(E)&&xrtNetEngineDestroy(E)&&xmqttSessionDestroy(S),"all native/ordinary owners retired");testRequire(stInfo(State).Inbox==2,"native protocol completion cannot complete durable application delivery");xmqttStoreStateDestroy(State);xrtFutureDestroy(Bye);xrtFutureDestroy(Ready);puts("[PASS] actual Client+Engine committed-state import and independent wire replay/dedup");return 0;
}
