/* Independent disposable Redis oracle, included after common helpers. */
static void interopWatchIdle(xredisclient* Client)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredisclientinfo Info = {0}; Info.Size = sizeof(Info); interopRequire(xredisClientInfo(Client, &Info), "watch native snapshot");
        if (!Info.Requests) return;
        interopRequire(xrtTimer() < End, "watch native reservations did not retire"); xrtThreadYield();
    }
}
static xrediswatch* interopWatchBegin(xredisclient* Client, xbytesview Key, const xrediswatchconfig* Config, xfuture** Started)
{
    interopWatchIdle(Client);
    interopRequire(xredisClientWatch(Client, &Key, 1, Config, Started) == XREDIS_ADMISSION_ACCEPTED, "real WATCH admission");
    const xrediswatchresult* Result = interopWait(*Started);
    interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Watch, "real WATCH acknowledgement"); return Result->Watch;
}
static void interopWatchClosed(xrediswatch* Watch, bool Reusable, xredisrequeststatus Reason)
{
    xfuture* Closed = xredisWatchClosed(Watch); const xrediswatchclosedresult* Result = interopWait(Closed);
    interopRequire(Result->Reusable == Reusable && Result->Reason == Reason, "real WATCH physical disposition"); xrtFutureDestroy(Closed);
}
static void interopWatches(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
    for (unsigned Mode = 0; Mode < 2; ++Mode) {
        xredisclientinitconfig Init; interopOptions(&Init, Scenario); Init.Mode = Mode ? XREDIS_INIT_RESP2 : XREDIS_INIT_RESP3;
        xredisclient *Client = NULL, *Other = NULL;
        interopRequire(interopConnect(Engine, Address, &Init, &Client) && interopConnect(Engine, Address, &Init, &Other), "real WATCH clients");
        xfuture *Ready = xredisClientReady(Client), *OtherReady = xredisClientReady(Other);
        interopRequire(((const xredisreadyresult*)interopWait(Ready))->Status == XREDIS_READY_OK &&
            ((const xredisreadyresult*)interopWait(OtherReady))->Status == XREDIS_READY_OK, "real WATCH clients ready");
        xbytesview Key = XRT_BYTES_LITERAL("watch\0binary"), Set[] = {XRT_BYTES_LITERAL("SET"), Key, XRT_BYTES_LITERAL("initial")};
        xfuture* Call = interopCall(Other, Set, 3); xrtFutureDestroy(Call);
        xfuture* Started = NULL; xrediswatch* Watch = interopWatchBegin(Client, Key, NULL, &Started);
        xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Key}; xfuture* Read = NULL;
        interopRequire(xredisWatchCall(Watch, Get, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "real scoped GET");
        xredisreplyvalue Value = interopValue(Read); interopRequire(Value.Data.Size == 7 && !memcmp(Value.Data.Data, "initial", 7), "real scoped read value");
        Set[2] = (xbytesview)XRT_BYTES_LITERAL("changed"); Call = interopCall(Other, Set, 3); xrtFutureDestroy(Call);
        Set[2] = (xbytesview)XRT_BYTES_LITERAL("must-not-commit"); xredistransactioncommand Command = {Set, 3}; xfuture* Commit = NULL;
        interopRequire(xredisWatchCommit(Watch, &Command, 1, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "real conflicting commit");
        const xredistransactionresult* Result = interopWait(Commit);
        interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_ABORTED && Result->Queued == 1,
            "real WATCH conflict did not return null EXEC/ABORTED");
        interopWatchClosed(Watch, true, XREDIS_REQUEST_OK);
        Call = interopCall(Other, Get, 2); Value = interopValue(Call);
        interopRequire(Value.Data.Size == 7 && !memcmp(Value.Data.Data, "changed", 7), "conflicting WATCH executed/retried body"); xrtFutureDestroy(Call);
        xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Commit);
        Watch = interopWatchBegin(Client, Key, NULL, &Started);
        unsigned char Binary[] = {'v', 0, '\r', '\n', 0xff}; Set[2] = (xbytesview){Binary, sizeof(Binary)};
        xredistransactioncommand Commands[] = {{Set, 3}, {Get, 2}};
        interopRequire(xredisWatchCommit(Watch, Commands, 2, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "real successful watch commit");
        Result = interopWait(Commit); xredisreplyvalue Root, First, Second;
        interopRequire(Result->Execution == XREDIS_TRANSACTION_EXECUTED && xredisReplyGet(Result->Request.Reply, 0, &Root) &&
            Root.ChildCount == 2 && xredisReplyGet(Result->Request.Reply, Root.FirstChild, &First) &&
            xredisReplyGet(Result->Request.Reply, First.Next, &Second) && Second.Data.Size == sizeof(Binary) && !memcmp(Second.Data.Data, Binary, sizeof(Binary)),
            "real WATCH binary EXEC array");
        interopWatchClosed(Watch, true, XREDIS_REQUEST_OK); xrtFutureDestroy(Started); xrtFutureDestroy(Commit);
        Watch = interopWatchBegin(Client, Key, NULL, &Started); interopWatchIdle(Client); xredisWatchAbort(Watch);
        interopWatchClosed(Watch, true, XREDIS_REQUEST_CANCELLED); xrtFutureDestroy(Started);
        xrediswatchconfig Config; xredisWatchConfigInit(&Config); Config.Timeout = 500;
        Watch = interopWatchBegin(Client, Key, &Config, &Started); interopWatchClosed(Watch, true, XREDIS_REQUEST_TIMEOUT); xrtFutureDestroy(Started);
        /* A real ACL rejection cannot certify UNWATCH, even if no changes have
         * yet occurred. The dedicated connection must physically close. */
        Watch = interopWatchBegin(Client, Key, NULL, &Started); interopWatchIdle(Client);
        interopTransactionAcl(Other, Scenario, "-unwatch"); xredisWatchAbort(Watch);
        interopWatchClosed(Watch, false, XREDIS_REQUEST_CANCELLED); interopTransactionAcl(Other, Scenario, "+unwatch");
        interopClose(Client); interopClose(Other); xrtFutureDestroy(Started); xrtFutureDestroy(Ready); xrtFutureDestroy(OtherReady);
    }
    puts("[PASS] real Redis WATCH: RESP2/3 conflict abort without replay; binary scoped GET/EXEC; idle abort/deadline reuse; UNWATCH ACL rejection retires connection");
}
