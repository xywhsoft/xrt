/* Included after the real TCP fixture, in modular and single-header tests. */
typedef struct mqttPollObservation {
    xmqttclient* Client;xnetpost Posts[8];unsigned Used;xatomic32 Done;
    xwaitresult Expected;
} mqttPollObservation;
static void mqttPollWorker(xnetworker* Worker,ptr Data)
{
    (void)Worker;mqttPollObservation* O=Data;
    testRequire(xmqttClientPollRetired(O->Client)==O->Expected,"worker physical retirement query");
    testRequire(xmqttClientWaitFor(O->Client,0)==XWAIT_ERROR && !xmqttClientDestroy(O->Client),"legacy worker Wait/Destroy remain forbidden");xrtClearError();
    xrtAtomic32Store(&O->Done,1,XMEMORY_RELEASE);
}
static void mqttPollOnWorker(mqttPollObservation* O,xnetengine* E,unsigned Affinity,xwaitresult Expected)
{
    testRequire(O->Used<8,"query observation capacity");xnetpost* Post=&O->Posts[O->Used++];O->Expected=Expected;xrtAtomic32Store(&O->Done,0,XMEMORY_RELEASE);
    testRequire(xrtNetPostInit(Post) && xrtNetPost(xrtNetEngineWorker(E,Affinity),Post,mqttPollWorker,O),"query observation post");
    mqttTestWait(&O->Done,1);
}
typedef struct mqttPollAmbient {const xerror* Error;unsigned Calls;} mqttPollAmbient;
static void mqttPollError(const xerror* Error,ptr Data)
{mqttPollAmbient* A=Data;if(Error==A->Error)++A->Calls;}
static void mqttPollBudget(xmqttclient* C,xwaitresult Expected)
{
    xrtSetErrorInfo(XERR_STATE,"mqtt.poll.ambient",19,"preserve without redelivery");mqttPollAmbient A={xrtGetError(),0};xrtSetErrorHandler(mqttPollError,&A);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"query allocation budget");
    for(unsigned I=0;I<10000;++I)testRequire(xmqttClientPollRetired(C)==Expected && xrtGetError()==A.Error,"query preserves exact ambient error");
    testRequire(xrtAllocationBudgetLeave(&Scope),"query budget leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);
    testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount && !Info.Rejections && !A.Calls,"query neither allocates nor redelivers errors");
    xrtAllocationBudgetFree(B);xrtSetErrorHandler(NULL,NULL);xrtClearError();
}
static void testMqttRetirementPoll(void)
{
    testRequire(xmqttClientPollRetired(NULL)==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"null retirement query");xrtClearError();
    for(unsigned V=4;V<=5;++V){
        mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        mqttPollObservation O={0};O.Client=C;xrtAtomic32Init(&O.Done,0);
        mqttPollBudget(C,XWAIT_TIMEOUT);mqttPollOnWorker(&O,P.Engine,0,XWAIT_TIMEOUT);mqttPollOnWorker(&O,P.Engine,1,XWAIT_TIMEOUT);
        testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK,"query fixture physically retired");
        mqttPollOnWorker(&O,P.Engine,0,XWAIT_OK);mqttPollOnWorker(&O,P.Engine,1,XWAIT_OK);mqttPollBudget(C,XWAIT_OK);
#if defined(XMQTT_IMPLEMENTATION)
        /* Freeze actual existing predicates, never fabricate ready refcounts.
         * A posted query must return before its mutex owner releases the lock. */
        xmutex* Locks[]={&C->RetirementLock,&C->Gate,&C->Stream->RetirementLock};
        for(unsigned I=0;I<3;++I){testRequire(xrtMutexLock(Locks[I]),"hold real predicate mutex");
            testRequire(xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"self-held predicate is pending");mqttPollOnWorker(&O,P.Engine,1,XWAIT_TIMEOUT);
            testRequire(xrtMutexUnlock(Locks[I]) && xmqttClientPollRetired(C)==XWAIT_OK,"query ready after predicate release");}
        xnetstream* Alias=xrtNetStreamRef(C->Stream);testRequire(Alias && xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"native alias keeps Client physically pending");xrtNetStreamDestroy(Alias);
        for(unsigned I=0;I<2;++I){xrt_mutex_impl* M=__xrtMutexImpl(Locks[I]);uint32 Magic=M->Magic;
            xrtSetErrorKind(XERR_STATE);const xerror* Old=xrtGetError();M->Magic=0;xwaitresult R=xmqttClientPollRetired(C);M->Magic=Magic;
            testRequire(R==XWAIT_ERROR && xrtGetError()==Old,"real synchronization failure with identical static diagnostic");xrtClearError();}
#endif
        testRequire(xmqttClientPollRetired(C)==XWAIT_OK && xmqttClientDestroy(C),"query preserves unique owner for normal destruction");mqttTestStop(&P);
        testRequire(((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"result outlives queried Client and Engine");xrtFutureDestroy(Ready);
    }
    puts("[PASS] MQTT nonwaiting retirement: workers, notification/native tails, predicate contention, ambient errors and allocation budgets");
}
