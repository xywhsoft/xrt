static const xrediswatchresult* interopPoolWatch(xredislease* Lease, xbytesview Key, xfuture** Future)
{
    interopRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, Future) == XREDIS_ADMISSION_ACCEPTED, "real nested WATCH admission");
    const xrediswatchresult* Result = interopWait(*Future);
    interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Watch, "real nested WATCH ready"); return Result;
}
static void interopPoolSessions(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
    for (unsigned Mode = 0; Mode < 2; ++Mode) {
        xredisclientinitconfig Init; interopOptions(&Init, Scenario); Init.Mode = Mode ? XREDIS_INIT_RESP2 : XREDIS_INIT_RESP3;
        xredisclient *Client = NULL, *Admin = NULL;
        interopRequire(interopConnect(Engine, Address, &Init, &Client) && interopConnect(Engine, Address, &Init, &Admin), "session members");
        xfuture *Ready = xredisClientReady(Client), *AdminReady = xredisClientReady(Admin);
        interopRequire(((const xredisreadyresult*)interopWait(Ready))->Status == XREDIS_READY_OK &&
            ((const xredisreadyresult*)interopWait(AdminReady))->Status == XREDIS_READY_OK, "session members ready"); interopWatchIdle(Client);
        xredispool* Pool = NULL; interopRequire(xredisPoolCreate(Engine, 1, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "real session pool");
        xbytesview Key = XRT_BYTES_LITERAL("pooled-session\0key"), Set[] = {XRT_BYTES_LITERAL("SET"), Key, XRT_BYTES_LITERAL("initial")}, Get[] = {XRT_BYTES_LITERAL("GET"), Key};
        xredistransactioncommand Commands[] = {{Set, 3}, {Get, 2}}; xfuture *Acquired = NULL, *Operation = NULL;
        xredislease* Lease = interopPoolAcquire(Pool, &Acquired);
        interopRequire(xredisLeaseTransaction(Lease, Commands, 2, NULL, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED, "real pooled EXEC");
        const xredistransactionresult* Result = interopWait(Operation); xredisreplyvalue Root, First, Second;
        interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_EXECUTED &&
            xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 2 && xredisReplyGet(Result->Request.Reply, Root.FirstChild, &First) &&
            xredisReplyGet(Result->Request.Reply, First.Next, &Second) && Second.Data.Size == 7 && !memcmp(Second.Data.Data, "initial", 7), "real pooled transaction array");
        interopPoolReturn(Lease); xrtFutureDestroy(Acquired); xrtFutureDestroy(Operation);
        Lease = interopPoolAcquire(Pool, &Acquired); xfuture* Started = NULL; const xrediswatchresult* Scope = interopPoolWatch(Lease, Key, &Started); xfuture* Read = NULL;
        interopRequire(xredisWatchCall(Scope->Watch, Get, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "real pooled watch read");
        xredisreplyvalue Value = interopValue(Read); interopRequire(Value.Data.Size == 7 && !memcmp(Value.Data.Data, "initial", 7), "real pooled read value");
        Set[2] = (xbytesview)XRT_BYTES_LITERAL("changed"); xfuture* Call = interopCall(Admin, Set, 3); xrtFutureDestroy(Call);
        Set[2] = (xbytesview)XRT_BYTES_LITERAL("must-not-commit"); xredistransactioncommand One = {Set, 3};
        interopRequire(xredisWatchCommit(Scope->Watch, &One, 1, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)interopWait(Operation))->Execution == XREDIS_TRANSACTION_ABORTED, "real nested conflict replayed body");
        interopWatchClosed(Scope->Watch, true, XREDIS_REQUEST_OK); interopPoolReturn(Lease);
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Operation);
        Lease = interopPoolAcquire(Pool, &Acquired); Scope = interopPoolWatch(Lease, Key, &Started); interopPoolCounter(Pool, 0, 0);
        xfuture* ScopeClosed = xredisWatchClosed(Scope->Watch), *Returned = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease);
        const xredisleaseclosedresult* Parent = interopWait(Returned);
        interopRequire(Parent->Reusable && xrtFutureState(ScopeClosed) == XFUTURE_RESOLVED &&
            ((const xrediswatchclosedresult*)interopWait(ScopeClosed))->Reusable, "real lease returned before child barrier");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned);
        /* Reacquire the SAME member and modify its formerly watched key from
         * another connection. A later plain EXEC must not inherit stale WATCH. */
        Set[2] = (xbytesview)XRT_BYTES_LITERAL("after-return"); Call = interopCall(Admin, Set, 3); xrtFutureDestroy(Call);
        Lease = interopPoolAcquire(Pool, &Acquired); Set[2] = (xbytesview)XRT_BYTES_LITERAL("clean-generation");
        interopRequire(xredisLeaseTransaction(Lease, &One, 1, NULL, 5000, &Operation) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)interopWait(Operation))->Execution == XREDIS_TRANSACTION_EXECUTED, "real reused member retained prior WATCH state");
        interopPoolReturn(Lease); xrtFutureDestroy(Acquired); xrtFutureDestroy(Operation);
        Lease = interopPoolAcquire(Pool, &Acquired); Scope = interopPoolWatch(Lease, Key, &Started); interopPoolCounter(Pool, 0, 0);
        interopTransactionAcl(Admin, Scenario, "-unwatch"); ScopeClosed = xredisWatchClosed(Scope->Watch); Returned = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease);
        Parent = interopWait(Returned); interopRequire(!Parent->Reusable && xrtFutureState(ScopeClosed) == XFUTURE_RESOLVED &&
            !((const xrediswatchclosedresult*)interopWait(ScopeClosed))->Reusable, "real nested ACL cleanup reused dirty member");
        interopTransactionAcl(Admin, Scenario, "+unwatch"); xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool);
        interopRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "real session pool close"); xredisPoolRelease(Pool); interopClose(Client); interopClose(Admin);
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned); xrtFutureDestroy(Closed);
        xrtFutureDestroy(Ready); xrtFutureDestroy(AdminReady);
    }
    puts("[PASS] real Redis pooled sessions: RESP2/3 native EXEC; nested read/conflict; scope-before-lease barrier; same-member stale-WATCH rejection; ACL cleanup retirement");
}
