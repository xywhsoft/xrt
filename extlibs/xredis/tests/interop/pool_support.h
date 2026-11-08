/* Disposable independent Redis oracle. No Redis implementation source. */
static void interopPoolCounter(xredispool* Pool, uint64 Requests, uint64 Bytes)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info); interopRequire(xredisPoolInfo(Pool, &Info), "real pool snapshot");
        if (Info.Requests == Requests && Info.CommandBytes == Bytes) return;
        interopRequire(xrtTimer() < End, "real pool physical counters did not settle"); xrtThreadYield();
    }
}
static xredislease* interopPoolAcquire(xredispool* Pool, xfuture** Future)
{
    interopRequire(xredisPoolAcquire(Pool, 5000, Future) == XREDIS_ADMISSION_ACCEPTED, "real pool acquire");
    const xredisleaseresult* Result = interopWait(*Future); interopRequire(Result->Status == XREDIS_REQUEST_OK && Result->Lease, "real pool granted lease"); return Result->Lease;
}
static void interopPoolReturn(xredislease* Lease)
{
    xfuture* Closed = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease); const xredisleaseclosedresult* Result = interopWait(Closed);
    interopRequire(Result->Reason == XREDIS_REQUEST_OK && Result->Reusable, "real pool reusable return"); xrtFutureDestroy(Closed);
}
static bool interopPoolBlockedFlag(xbytesview Text)
{
    const char Token[] = " flags=";
    for (size_t Index = 0; Index + sizeof(Token) - 1u <= Text.Size; ++Index) {
        if (memcmp(Text.Data + Index, Token, sizeof(Token) - 1u)) continue;
        for (size_t Flag = Index + sizeof(Token) - 1u; Flag < Text.Size && Text.Data[Flag] != ' ' && Text.Data[Flag] != '\n'; ++Flag)
            if (Text.Data[Flag] == 'b') return true;
    }
    return false;
}
static void interopPoolServerBlocked(xredisclient* Admin, int64 Id)
{
    char Number[32]; int Size = snprintf(Number, sizeof(Number), "%lld", (long long)Id);
    xbytesview Args[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("LIST"), XRT_BYTES_LITERAL("ID"), {(cbytes)Number, (size_t)Size}};
    double End = xrtTimer() + 10;
    for (;;) {
        xfuture* Future = interopCall(Admin, Args, 4); xredisreplyvalue Value = interopValue(Future);
        bool Blocked = interopPoolBlockedFlag(Value.Data); xrtFutureDestroy(Future);
        if (Blocked) return;
        interopRequire(xrtTimer() < End, "Redis did not report the dedicated connection blocked"); xrtThreadYield();
    }
}
static void interopPools(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
    for (unsigned Mode = 0; Mode < 2; ++Mode) {
        xredisclientinitconfig Init; interopOptions(&Init, Scenario); Init.Mode = Mode ? XREDIS_INIT_RESP2 : XREDIS_INIT_RESP3;
        xredisclient *Clients[2] = {0}, *Admin = NULL; xfuture* Ready[3];
        for (size_t Index = 0; Index < 2; ++Index) {
            interopRequire(interopConnect(Engine, Address, &Init, &Clients[Index]), "real pool member connection"); Ready[Index] = xredisClientReady(Clients[Index]);
            interopRequire(((const xredisreadyresult*)interopWait(Ready[Index]))->Status == XREDIS_READY_OK, "real pool member ready");
        }
        interopRequire(interopConnect(Engine, Address, &Init, &Admin), "real pool independent admin"); Ready[2] = xredisClientReady(Admin);
        interopRequire(((const xredisreadyresult*)interopWait(Ready[2]))->Status == XREDIS_READY_OK, "real pool admin ready");
        xbytesview IdArgs[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; xfuture* Call = interopCall(Clients[0], IdArgs, 2);
        xredisreplyvalue Value = interopValue(Call); interopRequire(Value.Type == XREDIS_REPLY_INTEGER && Value.Integer > 0, "real member id"); int64 Id = Value.Integer; xrtFutureDestroy(Call);
        xbytesview Key = XRT_BYTES_LITERAL("pool\0blocking"), Delete[] = {XRT_BYTES_LITERAL("DEL"), Key}; Call = interopCall(Admin, Delete, 2); xrtFutureDestroy(Call);
        interopWatchIdle(Clients[0]); interopWatchIdle(Clients[1]); xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.MaxRequests = 1; Config.WorkItems = 1;
        xredispool* Pool = NULL; interopRequire(xredisPoolCreate(Engine, 0, Clients, 2, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "real two-member pool");
        xfuture *A = NULL, *B = NULL; xredislease *LA = interopPoolAcquire(Pool, &A), *LB = interopPoolAcquire(Pool, &B);
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("pool\0binary")};
        interopRequire(xredisLeaseCall(LA, Echo, 2, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED, "real binary leased call");
        Value = interopValue(Call); interopRequire(Value.Data.Size == Echo[1].Size && !memcmp(Value.Data.Data, Echo[1].Data, Echo[1].Size), "real pool binary result");
        xrtFutureDestroy(Call); interopPoolCounter(Pool, 0, 0);
        xfuture* Cancelled = NULL; interopRequire(xredisPoolAcquireAsync(Pool, 5000, &Cancelled) == XREDIS_ADMISSION_ACCEPTED && xrtFutureCancel(Cancelled), "real queued cancel");
        const xredisleaseresult* CancelledResult = interopWait(Cancelled); interopRequire(CancelledResult->Status == XREDIS_REQUEST_CANCELLED && !CancelledResult->Lease, "real queued cancellation outcome"); xrtFutureDestroy(Cancelled);
        xbytesview Block[] = {XRT_BYTES_LITERAL("BLPOP"), Key, XRT_BYTES_LITERAL("0")}; size_t Wire = 0; xredisCommandSize(Block, 3, &Wire); xfuture* Slow = NULL;
        interopRequire(xredisLeaseCallAsync(LA, Block, 3, 10000, &Slow) == XREDIS_ADMISSION_ACCEPTED, "real blocking lease command");
        interopPoolServerBlocked(Admin, Id); interopPoolCounter(Pool, 1, Wire);
        xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Rejected = (xfuture*)(uintptr_t)1;
        interopRequire(xredisLeaseCallAsync(LB, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1, "real aggregate request cap");
        interopRequire(xrtFutureCancel(Slow), "real sent blocking cancellation"); const xredisrequestresult* SlowResult = interopWait(Slow);
        interopRequire(SlowResult->Status == XREDIS_REQUEST_CANCELLED && SlowResult->Delivery == XREDIS_MAY_HAVE_EXECUTED, "real cancelled blocking delivery");
        interopPoolCounter(Pool, 1, Wire);
        interopRequire(xredisLeaseCallAsync(LB, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1,
            "real cancellation released a physical reservation");
        xbytesview Push[] = {XRT_BYTES_LITERAL("LPUSH"), Key, XRT_BYTES_LITERAL("one")}; Call = interopCall(Admin, Push, 3); xrtFutureDestroy(Call);
        interopPoolCounter(Pool, 0, 0); xrtFutureDestroy(Slow);
        interopRequire(xredisLeaseCall(LB, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)interopWait(Call))->Status == XREDIS_REQUEST_OK,
            "real reclaimed quota native call"); xrtFutureDestroy(Call); interopPoolCounter(Pool, 0, 0);
        interopRequire(xredisLeaseCallAsync(LA, Block, 3, 10000, &Slow) == XREDIS_ADMISSION_ACCEPTED, "real graceful blocking command");
        interopPoolServerBlocked(Admin, Id); xfuture* Returned = xredisLeaseClosed(LA); xredisLeaseReturn(LA); xfuture* Waiting = NULL;
        interopRequire(xredisPoolAcquireAsync(Pool, 5000, &Waiting) == XREDIS_ADMISSION_ACCEPTED && xrtFutureState(Waiting) == XFUTURE_PENDING && xrtFutureState(Returned) == XFUTURE_PENDING,
            "real busy connection leased before drain");
        Call = interopCall(Admin, Push, 3); xrtFutureDestroy(Call); SlowResult = interopWait(Slow);
        interopRequire(SlowResult->Status == XREDIS_REQUEST_OK && ((const xredisleaseclosedresult*)interopWait(Returned))->Reusable, "real graceful FIFO drain");
        xredislease* Next = ((const xredisleaseresult*)interopWait(Waiting))->Lease; interopRequire(Next != NULL, "real queued handoff");
        interopPoolReturn(Next); interopPoolReturn(LB);
        interopRequire(xredisLeaseCallAsync(LA, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1, "real old generation reused");
        xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool);
        interopRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "real pool physical close");
        xredisPoolRelease(Pool); interopClose(Clients[0]); interopClose(Clients[1]); interopClose(Admin);
        xrtFutureDestroy(A); xrtFutureDestroy(B); xrtFutureDestroy(Slow); xrtFutureDestroy(Returned); xrtFutureDestroy(Waiting); xrtFutureDestroy(Closed);
        for (size_t Index = 0; Index < 3; ++Index) xrtFutureDestroy(Ready[Index]);
    }
    puts("[PASS] real Redis pool: RESP2/3 authenticated TLS members; binary calls; blocked BLPOP witness; physical aggregate quota; cancel/drain/FIFO handoff; old-generation rejection");
}
