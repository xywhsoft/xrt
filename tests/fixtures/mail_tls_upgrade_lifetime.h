#ifndef XRT_TEST_MAIL_TLS_UPGRADE_LIFETIME_H
#define XRT_TEST_MAIL_TLS_UPGRADE_LIFETIME_H

#include <xrt/detail/wait.h>

/* Include before the product's test_tls.h. The real rejection is published,
 * but the Worker is held inside the callback that still borrows Upgrade. */
static xatomic32 TestMailUpgradePublished;
static xatomic32 TestMailUpgradeRelease;
static xatomic32 TestMailUpgradeWaited;
static xatomic32 TestMailUpgradeReturned;
static double TestMailUpgradeGuard;

static inline xtlsstream* testMailTlsUpgrade(
    xnetstream** ppTcp, const xtlsserverconfig* pServer, double iDeadline);

static bool testMailUpgradeReject(xpromise* pPromise, const xerror* pError)
{
    bool Result = xrtPromiseReject(pPromise, pError);
    xrtAtomic32Store(&TestMailUpgradePublished, 1u, XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&TestMailUpgradeRelease, XMEMORY_ACQUIRE)) {
        testRequire(!__xrtWaitExpired(TestMailUpgradeGuard), "TLS rejection gate stalled");
        xrtThreadYield();
    }
    return Result;
}

static xwaitresult testMailUpgradeWait(xfuture* pFuture, double Deadline)
{
    xwaitresult Result = __xrtFutureWaitUntil(pFuture, Deadline);
    xrtAtomic32Store(&TestMailUpgradeWaited, 1u, XMEMORY_RELEASE);
    return Result;
}

typedef struct testmailupgradelifetime {
    xnetstream* Tcp;
    xtlsserverconfig Server;
} testmailupgradelifetime;

static int32 testMailUpgradeCaller(ptr pData)
{
    testmailupgradelifetime* pTest = (testmailupgradelifetime*)pData;
    xtlsstream* pTls = testMailTlsUpgrade(
        &pTest->Tcp, &pTest->Server, TestMailUpgradeGuard);
    testRequire(pTls == NULL, "invalid TLS server unexpectedly attached");
    xrtAtomic32Store(&TestMailUpgradeReturned, 1u, XMEMORY_RELEASE);
    return 0;
}

static int testMailUpgradeLifetime(void)
{
    xnetengineconfig Engine;
    xnetlistenconfig Listen;
    xnetaddr Address;
    testmailupgradelifetime Test;
    xnetengine* pEngine;
    xnetlistener* pListener;
    xnetstream* pClient;
    xthread* pThread;
    double Quiet;
    xmemdebugsnapshot Memory;
    testRequire(xrtMemDebugEnable(true), "TLS lifetime memory debug enable failed");
    xrtNetEngineConfigInit(&Engine);
    Engine.Workers = 1u;
    Engine.Backend = XNET_PORT_SELECT;
    pEngine = xrtNetEngineCreate(&Engine);
    testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "TLS lifetime engine start failed");
    xrtNetListenConfigInit(&Listen);
    testRequire(xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0),
        "TLS lifetime address failed");
    pListener = xrtNetListen(pEngine, &Listen, NULL, NULL, NULL);
    testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &Address),
        "TLS lifetime listener failed");
    pClient = xrtNetStreamConnect(pEngine, &Address, 0, NULL, NULL, NULL);
    testRequire(pClient != NULL, "TLS lifetime connect failed");
    TestMailUpgradeGuard = __xrtWaitAfter(INT64_C(5000));
    Test.Tcp = __xrtNetListenerAcceptWait(pListener, TestMailUpgradeGuard, NULL);
    testRequire(Test.Tcp != NULL, "TLS lifetime accept failed");
    /* The actual TLS constructor rejects this missing context/identity.
     * No fake stream, fabricated future state or allocator shortcut is used. */
    xrtTlsServerConfigInit(&Test.Server);
    xrtAtomic32Init(&TestMailUpgradePublished, 0u);
    xrtAtomic32Init(&TestMailUpgradeRelease, 0u);
    xrtAtomic32Init(&TestMailUpgradeWaited, 0u);
    xrtAtomic32Init(&TestMailUpgradeReturned, 0u);
    pThread = xrtThreadCreate(testMailUpgradeCaller, &Test, 0);
    testRequire(pThread != NULL, "TLS lifetime caller failed");
    while(!xrtAtomic32Load(&TestMailUpgradePublished, XMEMORY_ACQUIRE) ||
          !xrtAtomic32Load(&TestMailUpgradeWaited, XMEMORY_ACQUIRE)) {
        testRequire(!__xrtWaitExpired(TestMailUpgradeGuard), "TLS lifetime publication stalled");
        xrtThreadYield();
    }
    Quiet = __xrtWaitAfter(INT64_C(250));
    while(!xrtAtomic32Load(&TestMailUpgradeReturned, XMEMORY_ACQUIRE) &&
          !__xrtWaitExpired(Quiet)) xrtThreadYield();
    testRequire(!xrtAtomic32Load(&TestMailUpgradeReturned, XMEMORY_ACQUIRE),
        "TLS upgrade returned while Worker still borrows its stack job");
    xrtAtomic32Store(&TestMailUpgradeRelease, 1u, XMEMORY_RELEASE);
    testRequire(__xrtThreadWaitUntil(pThread, TestMailUpgradeGuard) == XWAIT_OK &&
        xrtThreadExitCode(pThread) == 0, "TLS lifetime caller did not retire");
    xrtThreadDestroy(pThread);
    (void)xrtNetStreamAbort(Test.Tcp);
    xrtNetStreamDestroy(Test.Tcp);
    (void)xrtNetStreamAbort(pClient);
    xrtNetStreamDestroy(pClient);
    testRequire(xrtNetListenerClose(pListener), "TLS lifetime listener close failed");
    xrtNetListenerDestroy(pListener);
    for(;;) {
        xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
        if(Result == XNET_RETIRE_READY) break;
        testRequire(Result != XNET_RETIRE_ERROR && !__xrtWaitExpired(TestMailUpgradeGuard),
            "TLS lifetime engine did not retire");
        xrtThreadYield();
    }
    xrtClearError();
    xrtMemDebugSnapshot(&Memory);
    testRequire(Memory.LiveCount == 0u && Memory.LiveBytes == 0u &&
        Memory.InvalidFreeCount == 0u && Memory.DoubleFreeCount == 0u,
        "TLS lifetime fixture retained allocations or corrupted ownership");
    printf("[PASS] TLS upgrade failure waits for Worker retirement\n");
    return 0;
}
#endif
