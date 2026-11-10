/* Assignment is immutable Session identity, not a new transport or credential. */
static xstrview assignedId(xmqttsession* S)
{xstrview Id={(const char*)1,99};testRequire(xmqttSessionClientId(S,&Id),"assigned identity snapshot");return Id;}
static void assignedEmpty(xmqttsession* S)
{xstrview Id=assignedId(S);testRequire(!Id.Data && !Id.Size,"canonical unbound identity");}
static xmqttproperty assignedProperty(xstrview Id)
{xmqttproperty P={0};P.Id=XMQTT_PROPERTY_ASSIGNED_CLIENT_ID;P.Type=XMQTT_PROPERTY_STRING;P.Value.Text=Id;return P;}
typedef struct assignedReader {xmqttsession* Session;xatomic32* Start;} assignedReader;
static int32 assignedReadThread(ptr Data)
{
    assignedReader* R=Data;while(!xrtAtomic32Load(R->Start,XMEMORY_ACQUIRE))xrtThreadYield();
    for(unsigned I=0;I<10000;++I){xstrview V;if(!xmqttSessionClientId(R->Session,&V))return 1;
        if(V.Size){if(V.Size!=32)return 2;for(size_t J=0;J<V.Size;++J)if(V.Data[J]!='z')return 3;}else if(V.Data)return 4;}
    return 0;
}
static void testSessionAssigned(void)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.MaxOutgoing=K.MaxIncoming=0;
    xmqttsession* S=(xmqttsession*)(uintptr_t)1;
    testRequire(!xmqttSessionCreate(&K,&S) && S==(xmqttsession*)(uintptr_t)1,"ordinary factory still requires identity");
    testRequire(!xmqttSessionCreateUnassigned(&K,0,&S) && !xmqttSessionCreateUnassigned(&K,65536,&S) && S==(xmqttsession*)(uintptr_t)1,"bounded assignment capacity");
    K.Version=XMQTT_VERSION_311;testRequire(!xmqttSessionCreateUnassigned(&K,32,&S),"no invisible 311 assigned identity");K.Version=XMQTT_VERSION_5;
    K.ClientId=XRT_STR_LITERAL("fixed");testRequire(!xmqttSessionCreateUnassigned(&K,32,&S),"unassigned factory rejects existing identity");K.ClientId=(xstrview){0};
    testRequire(!xmqttSessionCreateUnassigned(&K,32,(xmqttsession**)&K),"factory output aliases config");
    testRequire(xmqttSessionCreateUnassigned(&K,32,&S),"bounded identity create");assignedEmpty(S);xmqttsessioninfo Before=sessionInfo(S);size_t Base=Before.BaseBytes;
    testRequire(Before.LiveBytes==Base && xmqttSessionDestroy(S),"reserved identity bytes charged as base");
    K.MaxBytes=Base-1;S=(xmqttsession*)(uintptr_t)1;testRequire(!xmqttSessionCreateUnassigned(&K,32,&S) && S==(xmqttsession*)(uintptr_t)1,"reserved identity exact budget minus one");K.MaxBytes=Base;
    testRequire(xmqttSessionCreateUnassigned(&K,32,&S),"reserved identity exact budget");
    xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);xmqttproperty E=sessionNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);C.Properties=(xmqttpropertylist){&E,1};
    char Name[33];memset(Name,'z',sizeof(Name));xmqttproperty P=assignedProperty((xstrview){Name,32});A.Properties=(xmqttpropertylist){&P,1};xmqttnegotiated N,Poison;memset(&Poison,0xa5,sizeof(Poison));
    for(unsigned Failure=0;Failure<8;++Failure){
        N=Poison;Before=sessionInfo(S);A.Reason=0;A.SessionPresent=false;A.Properties=(xmqttpropertylist){&P,1};P.Value.Text=(xstrview){Name,32};double Now=1;
        xmqttsessionstatus Expected=XMQTT_SESSION_PROTOCOL_ERROR;
        if(Failure==0)A.Properties=(xmqttpropertylist){0};
        if(Failure==1)P.Value.Text=(xstrview){0};
        if(Failure==2)P.Value.Text=(xstrview){Name,33},Expected=XMQTT_SESSION_RESTRICTED;
        if(Failure==3)A.SessionPresent=true;
        if(Failure==4)Now=NAN,Expected=XMQTT_SESSION_STATE_ERROR;
        if(Failure==5)C.Body.Connect.ClientId=XRT_STR_LITERAL("unexpected"),A.Properties=(xmqttpropertylist){0},Expected=XMQTT_SESSION_STATE_ERROR;
        if(Failure==6)P.Value.Text=XRT_STR_LITERAL("bad\0id"),Expected=XMQTT_SESSION_ERROR;
        if(Failure==7)A.Reason=0x87,Expected=XMQTT_SESSION_REJECTED;
        sessionStatus(xmqttSessionOpen(S,&C,&A,Now,&N),Expected,"assignment failure classification");
        if(Expected!=XMQTT_SESSION_REJECTED)testRequire(!memcmp(&N,&Poison,sizeof(N)),"failed assignment leaves negotiated output unchanged");
        xmqttsessioninfo After=sessionInfo(S);testRequire(sessionSameInfo(&Before,&After),"failed assignment changes no session state");assignedEmpty(S);C.Body.Connect.ClientId=(xstrview){0};
    }
    A.Reason=0;A.SessionPresent=false;P.Value.Text=(xstrview){Name,32};A.Properties=(xmqttpropertylist){&P,1};
    /* The single reserved allocation supports binding and repeated reads under
     * a one-byte allocation budget, with the ambient error identity preserved. */
    xatomic32 Start;xrtAtomic32Init(&Start,0);assignedReader Reader={S,&Start};xthread* Threads[3];for(unsigned I=0;I<3;++I){Threads[I]=xrtThreadCreate(assignedReadThread,&Reader,0);testRequire(Threads[I]!=NULL,"assignment concurrent reader");}xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);
    xrtSetErrorInfo(XERR_STATE,"test",1,"sentinel");const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"assignment one-byte budget");
    sessionStatus(xmqttSessionOpen(S,&C,&A,1,&N),XMQTT_SESSION_DONE,"assignment no-allocation bind");
    xstrview Id=assignedId(S);testRequire(Id.Size==32 && Id.Data!=Name && !memcmp(Id.Data,Name,32),"accepted id copied once into reserved storage");memset(Name,'x',sizeof(Name));
    for(unsigned J=0;J<10000;++J){xstrview V=assignedId(S);testRequire(V.Data==Id.Data && V.Size==32 && V.Data[0]=='z',"immutable stable borrowed identity");}
    testRequire(xrtGetError()==Error && xrtAllocationBudgetLeave(&Scope),"binding/getter preserve ambient diagnostics");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.Rejections,"binding/getter do not allocate");xrtAllocationBudgetFree(B);xrtClearError();
    for(unsigned I=0;I<3;++I){testRequire(xrtThreadWaitFor(Threads[I],8000)==XWAIT_OK && xrtThreadExitCode(Threads[I])==0,"identity readers see only empty or complete immutable id");xrtThreadDestroy(Threads[I]);}
    testRequire(!xmqttSessionClientId(S,(xstrview*)Id.Data),"identity output cannot overwrite reserved bytes");
    sessionEnd(S,2);C.Body.Connect.ClientId=Id;A.Properties=(xmqttpropertylist){0};A.SessionPresent=true;
    sessionStatus(xmqttSessionOpen(S,&C,&A,3,&N),XMQTT_SESSION_DONE,"assigned identity resumes server session");sessionEnd(S,4);
    C.Body.Connect.ClientId=(xstrview){0};A.SessionPresent=false;P.Value.Text=XRT_STR_LITERAL("second");A.Properties=(xmqttpropertylist){&P,1};Before=sessionInfo(S);N=Poison;
    sessionStatus(xmqttSessionOpen(S,&C,&A,5,&N),XMQTT_SESSION_STATE_ERROR,"bound identity cannot be reassigned");xmqttsessioninfo After=sessionInfo(S);testRequire(sessionSameInfo(&Before,&After) && !memcmp(&N,&Poison,sizeof(N)),"reassignment rollback");
    testRequire(xmqttSessionReset(S)==XMQTT_SESSION_DONE && assignedId(S).Data==Id.Data && assignedId(S).Data[0]=='z',"explicit reset preserves bound identity");
    C.Body.Connect.ClientId=Id;A.Properties=(xmqttpropertylist){0};sessionStatus(xmqttSessionOpen(S,&C,&A,6,&N),XMQTT_SESSION_DONE,"identity after reset");sessionEnd(S,7);
    sessionStatus(xmqttSessionExpire(S,100),XMQTT_SESSION_DONE,"assigned session expires");testRequire(assignedId(S).Data==Id.Data && assignedId(S).Size==32,"expiry preserves identity");testRequire(xmqttSessionDestroy(S),"assigned identity destroyed");
    K.MaxBytes=1024*1024;testRequire(xmqttSessionCreateUnassigned(&K,65535,&S),"protocol maximum reserved identity");char* Big=malloc(65535);testRequire(Big!=NULL,"maximum caller id");memset(Big,'M',65535);P.Value.Text=(xstrview){Big,65535};C.Body.Connect.ClientId=(xstrview){0};A.Properties=(xmqttpropertylist){&P,1};sessionStatus(xmqttSessionOpen(S,&C,&A,1,&N),XMQTT_SESSION_DONE,"protocol maximum assignment");memset(Big,0,65535);free(Big);testRequire(assignedId(S).Size==65535 && assignedId(S).Data[65534]=='M',"maximum snapshot lifetime");sessionEnd(S,2);testRequire(xmqttSessionDestroy(S),"maximum assignment cleanup");
    unsigned Failed=0,Passed=0;
    for(size_t Limit=1;Limit<Base+2048;Limit+=97){
        testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"assigned factory quota prefix");S=(xmqttsession*)(uintptr_t)1;
        if(xmqttSessionCreateUnassigned(&K,32,&S)){++Passed;assignedEmpty(S);testRequire(xmqttSessionDestroy(S),"assigned factory successful prefix cleanup");}else{++Failed;testRequire(S==(xmqttsession*)(uintptr_t)1,"assigned failed factory output unchanged");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"assigned factory quota leave");BI=(xallocationbudgetinfo){0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes,"assigned factory no allocation leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Passed,"assigned factory tests both failed and successful allocations");
    testRequire(!xmqttSessionClientId(NULL,&Id),"null identity owner");
}
