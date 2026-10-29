typedef struct interop_pool_factory {
    xnetaddr Address;
    xredisclientinitconfig Init;
    xatomic32 Calls, Dropped;
} interop_pool_factory;
static bool interopPoolFactory(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client)
{
    (void)Slot; interop_pool_factory* Factory = Data; xrtAtomic32FetchAdd(&Factory->Calls, 1, XMEMORY_RELEASE);
    if (Attempt == 1) return false; /* Initial transient factory failure, no client transferred. */
    xredisclientinitconfig Init = Factory->Init; Init.Timeout = Timeout;
    return interopConnect(Engine, &Factory->Address, &Init, Client);
}
static void interopPoolFactoryDrop(ptr Data)
{ xrtAtomic32FetchAdd(&((interop_pool_factory*)Data)->Dropped, 1, XMEMORY_RELEASE); }
static int64 interopPoolClientId(xredislease* Lease)
{
    xbytesview Args[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; xfuture* Call = NULL;
    interopRequire(xredisLeaseCall(Lease, Args, 2, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED, "automatic member id admission");
    xredisreplyvalue Value = interopValue(Call); interopRequire(Value.Type == XREDIS_REPLY_INTEGER && Value.Integer > 0, "automatic member id");
    int64 Id = Value.Integer; xrtFutureDestroy(Call); return Id;
}
static void interopPoolConnect(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
    for (unsigned Mode = 0; Mode < 2; ++Mode) {
        interop_pool_factory Factory = {0}; Factory.Address = *Address; interopOptions(&Factory.Init, Scenario);
        Factory.Init.Mode = Mode ? XREDIS_INIT_RESP2 : XREDIS_INIT_RESP3;
        xredisclient* Admin = NULL; interopRequire(interopConnect(Engine, Address, &Factory.Init, &Admin), "automatic admin");
        xfuture* AdminReady = xredisClientReady(Admin); interopRequire(((const xredisreadyresult*)interopWait(AdminReady))->Status == XREDIS_READY_OK, "automatic admin readiness");
        xredispoolconnectconfig Config; xredisPoolConnectConfigInit(&Config); Config.Connect = interopPoolFactory; Config.Drop = interopPoolFactoryDrop;
        Config.Data = &Factory; Config.Protocol = Mode ? XREDIS_RESP2 : XREDIS_RESP3; Config.RetryMin = Config.RetryMax = 5;
        xredispool* Pool = NULL; interopRequire(xredisPoolConnect(Engine, 1, 1, NULL, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "real automatic pool");
        xfuture* Acquired = NULL; xredislease* Lease = interopPoolAcquire(Pool, &Acquired); int64 OldId = interopPoolClientId(Lease);
        xbytesview Key = XRT_BYTES_LITERAL("automatic-pool\0key"), Set[] = {XRT_BYTES_LITERAL("SET"), Key, XRT_BYTES_LITERAL("repair\0value")}, Get[] = {XRT_BYTES_LITERAL("GET"), Key};
        xredistransactioncommand Commands[] = {{Set, 3}, {Get, 2}}; xfuture* Operation = NULL;
        interopRequire(xredisLeaseTransaction(Lease, Commands, 2, NULL, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)interopWait(Operation))->Execution == XREDIS_TRANSACTION_EXECUTED, "automatic native transaction"); xrtFutureDestroy(Operation);
        char Number[40]; int Length = snprintf(Number, sizeof(Number), "%lld", (long long)OldId);
        xbytesview Kill[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("KILL"), XRT_BYTES_LITERAL("ID"), {(cbytes)Number, (size_t)Length}};
        xfuture* Returned = xredisLeaseClosed(Lease); xfuture* Killed = interopCall(Admin, Kill, 4);
        interopRequire(interopValue(Killed).Integer == 1 && !((const xredisleaseclosedresult*)interopWait(Returned))->Reusable, "real server kill retired lease");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Killed); xrtFutureDestroy(Returned);
        Lease = interopPoolAcquire(Pool, &Acquired); interopRequire(interopPoolClientId(Lease) != OldId, "server-killed generation reused");
        interopRequire(xredisLeaseCall(Lease, Get, 2, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED, "automatic replaced GET");
        xredisreplyvalue Value = interopValue(Operation); interopRequire(Value.Data.Size == Set[2].Size && !memcmp(Value.Data.Data, Set[2].Data, Value.Data.Size), "automatic setup/profile and binary data");
        xrtFutureDestroy(Operation);
        /* Logical GET completion does not retire its native request tails.
         * This one-member fixture must await physical idle before nesting. */
        interopPoolCounter(Pool, 0, 0);
        xfuture* Started = NULL; const xrediswatchresult* Scope = interopPoolWatch(Lease, Key, &Started);
        xfuture* ChildClosed = xredisWatchClosed(Scope->Watch); Returned = xredisLeaseClosed(Lease); xredisLeaseAbort(Lease);
        interopWait(Returned); interopRequire(xrtFutureState(ChildClosed) == XFUTURE_RESOLVED, "automatic replacement preceded nested physical close");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ChildClosed); xrtFutureDestroy(Returned);
        Lease = interopPoolAcquire(Pool, &Acquired);
        interopRequire(xredisLeaseTransaction(Lease, NULL, 0, NULL, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)interopWait(Operation))->Execution == XREDIS_TRANSACTION_EXECUTED, "replacement inherited dirty WATCH");
        interopPoolReturn(Lease); xrtFutureDestroy(Acquired); xrtFutureDestroy(Operation);
        xredispoolconnectinfo Info = {0}; Info.Size = sizeof(Info);
        interopRequire(xredisPoolConnectInfo(Pool, &Info) && Info.Connected == 3 && Info.Attempts == 4 && Info.Failures == 3, "real automatic generation accounting");
        xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); interopRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK &&
            xrtAtomic32Load(&Factory.Dropped, XMEMORY_ACQUIRE) == 1, "real automatic context retirement");
        xredisPoolRelease(Pool); xrtFutureDestroy(Closed); xbytesview Delete[] = {XRT_BYTES_LITERAL("DEL"), Key}; Operation = interopCall(Admin, Delete, 2);
        xrtFutureDestroy(Operation); interopClose(Admin); xrtFutureDestroy(AdminReady);
    }
    puts("[PASS] real Redis automatic pools: RESP2/3 authenticated TLS setup, transient retry, server CLIENT KILL/refill, native transactions, nested abort and clean successor");
}
