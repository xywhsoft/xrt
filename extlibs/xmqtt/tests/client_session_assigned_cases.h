typedef struct csAssignedPeer {
    xstrview Expected;char Name[8];bool Auth;unsigned Failure;
} csAssignedPeer;
static xstrview csAssignedId(xmqttsession* S)
{xstrview V;testRequire(xmqttSessionClientId(S,&V),"Client assigned identity");return V;}
static void csAssignedConnack(mqttResumePeer* P)
{
    csAssignedPeer* T=P->FrameData;xmqttpacket A;xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);A.SessionPresent=P->Present;A.Reason=T->Failure==5?0x87:0;
    xmqttproperty Props[2]={{0}};size_t Count=0;
    if(!T->Expected.Size && T->Failure!=4 && T->Failure!=5){Props[Count].Id=XMQTT_PROPERTY_ASSIGNED_CLIENT_ID;Props[Count].Type=XMQTT_PROPERTY_STRING;Props[Count++].Value.Text=(xstrview){T->Name,sizeof(T->Name)};}
    if(T->Auth){Props[Count].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[Count].Type=XMQTT_PROPERTY_STRING;Props[Count++].Value.Text=XRT_STR_LITERAL("m");}
    A.Properties=(xmqttpropertylist){Props,Count};mqttResumePacket(P,&A);
}
static void csAssignedFrame(mqttResumePeer* P,xnetstream* S,xmqttframe F)
{
    if(F.Header.Type==XMQTT_PACKET_CONNECT){csAssignedPeer* T=P->FrameData;xmqttproperty Props[4];xmqttcodecbuffers B={Props,4,NULL,0};xmqttpacket Q;
        testRequire(xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL)==XMQTT_CODEC_DONE,"assigned CONNECT wire");
        testRequire(Q.Body.Connect.ClientId.Size==T->Expected.Size && (!T->Expected.Size || !memcmp(Q.Body.Connect.ClientId.Data,T->Expected.Data,T->Expected.Size)),"empty then exact assigned wire identity");
        xrtAtomic32FetchAdd(&P->Connects,1,XMEMORY_RELEASE);if(!(P->Mode&MQTT_RESUME_HOLD_READY))csAssignedConnack(P);
    }else mqttResumeFrame(P,S,F);
}
static xmqttclient* csAssignedConnect(mqttResumePeer* P,xmqttsession* S,xmqttclientinbox* I,bool Tiny,xfuture** Ready)
{
    csAssignedPeer* T=P->FrameData;(void)T;xmqttclientconfig K;xmqttClientConfigInit(&K);if(Tiny){K.ReadBytes=K.WriteBytes=K.WriteChunkBytes=1;K.WorkPackets=1;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=csAssignedId(S);xmqttproperty Props[2]={{0}};Props[0]=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);Q.Properties=(xmqttpropertylist){Props,1};
    xmqttclient* C=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(T->Auth){Props[1].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[1].Type=XMQTT_PROPERTY_STRING;Props[1].Value.Text=XRT_STR_LITERAL("m");Q.Properties.Count=2;
        testRequire(xmqttClientConnectSessionAuth(P->Engine,&P->Address,1,&K,NULL,S,I,&Q,&C),"unassigned authenticated constructor");}else
#endif
    {testRequire(xmqttClientConnectSession(P->Engine,&P->Address,1,&K,S,I,&Q,&C),"unassigned constructor");}
    *Ready=xmqttClientReady(C);return C;
}
static void csAssignedProof(xmqttclient* C,xfuture* F,xmqttsession* S,bool Auth,bool Accept)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth){double End=xrtTimer()+8;while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){
                testRequire(xmqttAuthPhase(T)==XMQTT_AUTH_VERIFY_CONNECT && !csInfo(S).Connected && !csAssignedId(S).Size,"assigned identity quarantined until final local proof");
                testRequire(xmqttAuthComplete(T,Accept)==XMQTT_ADMISSION_ACCEPTED,"assigned local proof decision");xmqttAuthRelease(T);
            }else{testRequire(xrtTimer()<End,"assigned proof deadline");xrtSleep(1);}}}
#else
    (void)C;(void)F;(void)S;(void)Auth;(void)Accept;
#endif
}
static void csAssigned(bool Tiny,bool Auth,unsigned Failure)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);xmqttsession* S=NULL;testRequire(xmqttSessionCreateUnassigned(&K,Failure==1?4:8,&S),"assigned Client Session factory");xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);xmqttclientinbox* I=NULL;testRequire(xmqttClientInboxCreate(&IK,&I),"assigned Inbox");
    csAssignedPeer T={0};memcpy(T.Name,"bound-id",8);T.Auth=Auth;T.Failure=Failure;mqttResumePeer P;mqttResumeStartHook(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY|MQTT_RESUME_HOLD_ACK,false,NULL,csAssignedFrame,&T);
    xfuture *Ready,*Next,*F=NULL;xmqttclient* C=csAssignedConnect(&P,S,I,Tiny,&Ready);mqttResumeWait(&P.Connects,1);testRequire(!csAssignedId(S).Data && !csInfo(S).Connected,"no premature identity binding");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=77;Q.Body.Publish.Topic=XRT_STR_LITERAL("assigned/recover");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED && csInfo(S).Objects==1 && !csInfo(S).HasState,"pre-CONNACK snapshot retained without bound identity");
    if(Failure!=3){csAssignedConnack(&P);csAssignedProof(C,Ready,S,Auth,Failure!=2);}else testRequire(xmqttClientClose(C),"close before assignment");
    const xmqttreadyresult* R=csResult(Ready);
    if(Failure){xmqttclientstatus Expected=Failure==1?XMQTT_STATUS_LIMIT:Failure==2?XMQTT_STATUS_AUTHENTICATION:Failure==3?XMQTT_STATUS_CLOSED:Failure==4?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_REJECTED;
        testRequire(R->Status==Expected && !csAssignedId(S).Size && !csInfo(S).HasState,"failed assignment keeps passive identity empty");csClose(C);mqttResumeStop(&P);testRequire(!csInfo(S).Objects,"unassigned pending payload physically refunded");xrtFutureDestroy(F);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"failed assignment cleanup");return;}
    testRequire(R->Status==XMQTT_STATUS_OK && R->Negotiated.ServerAssignedId && R->Negotiated.ClientId.Size==8,"first Ready reports assigned identity");mqttResumeWait(&P.Publishes,1);memset(T.Name,'z',8);xstrview Id=csAssignedId(S);testRequire(Id.Size==8 && !memcmp(Id.Data,"bound-id",8),"passive identity independent of original input");csClose(C);mqttResumeStop(&P);
    const xmqttwriteresult* Old=csResult(F);testRequire(Old->Status==XMQTT_STATUS_CLOSED && csInfo(S).ActiveOutgoing==1,"assigned session obligation survives Engine retirement");
    T.Expected=Id;mqttResumeStartHook(&P,XMQTT_VERSION_5,0,true,NULL,csAssignedFrame,&T);C=csAssignedConnect(&P,S,I,Tiny,&Next);
    /* Reconnect proof must preserve, rather than clear, an already bound id. */
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth){double End=xrtTimer()+8;while(!xrtFutureDone(Next)){xmqttauthticket* A=NULL;if(xmqttClientTakeAuth(C,&A)){testRequire(csAssignedId(S).Data==Id.Data && xmqttAuthPhase(A)==XMQTT_AUTH_VERIFY_CONNECT && !csInfo(S).Connected,"reconnect proof retains prior bound identity");testRequire(xmqttAuthComplete(A,true)==XMQTT_ADMISSION_ACCEPTED,"assigned resume proof");xmqttAuthRelease(A);}else{testRequire(xrtTimer()<End,"assigned resume proof deadline");xrtSleep(1);}}}
#endif
    const xmqttreadyresult* N=csResult(Next);testRequire(N->Status==XMQTT_STATUS_OK && N->Negotiated.SessionPresent && !N->Negotiated.ServerAssignedId,"assigned id resumes same server state");csDone(S,C);mqttResumeWait(&P.Publishes,1);testRequire(P.Log[0].Id==77 && (P.Log[0].Flags&8) && P.Log[0].Payload==3 && P.Log[0].First=='x',"assigned canonical replay id/DUP/payload");csClose(C);mqttResumeStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"assigned recovery passive cleanup");
    testRequire(R==xrtFutureValue(Ready) && R->Negotiated.ClientId.Size==8 && !memcmp(R->Negotiated.ClientId.Data,"bound-id",8) && Old==xrtFutureValue(F),"assigned Ready and old outcomes outlive both Engines/Session");xrtFutureDestroy(Ready);xrtFutureDestroy(Next);xrtFutureDestroy(F);
}
static void csAssignedTests(void)
{
    for(unsigned Tiny=0;Tiny<2;++Tiny){csAssigned(Tiny!=0,false,0);for(unsigned Failure=1;Failure<6;++Failure)if(Failure!=2)csAssigned(Tiny!=0,false,Failure);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        csAssigned(Tiny!=0,true,0);csAssigned(Tiny!=0,true,1);csAssigned(Tiny!=0,true,2);
#endif
    }
}
