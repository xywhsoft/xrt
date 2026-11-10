/* Public packaged adversarial probe: every factory failure has the SAME
 * ambient MEMORY diagnostic. Only its explicit status may select policy. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct policyInput {xmqttclientstatus Status;unsigned Calls;} policyInput;
static void policyCheck(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] reconnect typed policy: %s\n",Message);exit(1);}}
static xmqttclientstatus policyFactory(xnetengine* E,uint64 A,const xmqttclientconfig* C,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** Out)
{
    (void)E;(void)A;(void)C;(void)S;(void)I;(void)Q;(void)Out;policyInput* P=Data;++P->Calls;xrtSetErrorKind(XERR_MEMORY);return P->Status;
}
int main(void)
{
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);policyCheck(Engine && xrtNetEngineStart(Engine),"Engine");xtaskpoolconfig T={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&T);policyCheck(Pool!=NULL,"borrowed Pool");
    for(unsigned N=0;N<17;++N){
        policyInput P={(xmqttclientstatus)(N<15?N:N==15?99:XMQTT_STATUS_TRANSPORT),0};xmqttreconnectconfig K;xmqttReconnectConfigInit(&K);K.Factory=policyFactory;K.FactoryData=&P;K.RetryStatuses=N==16?(UINT32_C(1)<<XMQTT_STATUS_TRANSPORT):0;K.InitialDelay=1;K.MaxDelay=2;K.JitterPermille=0;K.MaxAttempts=3;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("typed-factory");xmqttreconnect* R=NULL;policyCheck(xmqttReconnectCreate(Engine,Pool,1,&K,&Q,&R) && xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"bounded controller outcome");
        xmqttreconnectinfo I;policyCheck(xmqttReconnectInfo(R,&I),"typed info");xmqttclientstatus Expected=(!N || N==15)?XMQTT_STATUS_INITIALIZATION:P.Status;
        policyCheck(I.LastStatus==Expected && I.Attempts==(N==16?3u:1u) && P.Calls==I.Attempts,"explicit status overrides the identical ambient MEMORY diagnostic");
        policyCheck(I.State==(Expected==XMQTT_STATUS_CLOSED?XMQTT_RECONNECT_STOPPED:XMQTT_RECONNECT_FAILED),"normal closure versus failure terminal state");policyCheck(xmqttReconnectDestroy(R),"unique owner retired");
    }
    policyCheck(xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(Pool) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"all actual borrowed owners joined");puts("[PASS] 17 typed factory policy cases with identical ambient MEMORY, invalid/empty success and exact retry counts");return 0;
}
