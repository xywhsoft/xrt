#include <xmqtt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned assignedBrokerRound;
static char assignedBrokerId[512];static size_t assignedBrokerBytes;
static bool assignedBrokerCreate(const xmqttsessionconfig* Given,xmqttsession** Out)
{xmqttsessionconfig K=*Given;K.ClientId=(xstrview){0};return xmqttSessionCreateUnassigned(&K,sizeof(assignedBrokerId),Out);}
static void assignedBrokerReady(xmqttsession* S,const xmqttreadyresult* R)
{
    xstrview V;bool OK=xmqttSessionClientId(S,&V) && V.Size && V.Size<=sizeof(assignedBrokerId) && R->Negotiated.ClientId.Size==V.Size && !memcmp(V.Data,R->Negotiated.ClientId.Data,V.Size) && R->Negotiated.ServerAssignedId==!assignedBrokerRound;
    if(!assignedBrokerRound && OK){assignedBrokerBytes=V.Size;memcpy(assignedBrokerId,V.Data,V.Size);}
    else if(OK)OK=V.Size==assignedBrokerBytes && !memcmp(V.Data,assignedBrokerId,V.Size);
    if(!OK){fputs("[FAIL] independent broker assigned identity continuity\n",stderr);exit(1);}++assignedBrokerRound;
}
#define xmqttSessionCreate assignedBrokerCreate
#define XMQTT_SESSION_BROKER_ASSIGNED_ID
#define XMQTT_SESSION_BROKER_ASSIGNED_READY assignedBrokerReady
