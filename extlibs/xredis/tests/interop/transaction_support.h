/* Included after the common independent Redis fixture client helpers. */
static const xredistransactionresult* interopTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, xfuture** Future)
{
    interopRequire(xredisClientTransactionAsync(Client, Commands, Count, NULL, 5000, Future) == XREDIS_ADMISSION_ACCEPTED, "real transaction admission");
    return (const xredistransactionresult*)interopWait(*Future);
}
static void interopTransactionAcl(xredisclient* Client, int Scenario, cstr Rule)
{
    xbytesview Arguments[] = {XRT_BYTES_LITERAL("ACL"), XRT_BYTES_LITERAL("SETUSER"),
        Scenario == 2 ? (xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME) : (xbytesview)XRT_BYTES_LITERAL("default"),
        {(cbytes)Rule, strlen(Rule)}};
    xfuture* Future = interopCall(Client, Arguments, 4); xrtFutureDestroy(Future);
}
static void interopTransactions(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
    for (unsigned Mode = 0; Mode < 2; ++Mode) {
        xredisclientinitconfig Init; interopOptions(&Init, Scenario); Init.Mode = Mode ? XREDIS_INIT_RESP2 : XREDIS_INIT_RESP3;
        xredisclient* Client = NULL; interopRequire(interopConnect(Engine, Address, &Init, &Client), "transaction managed connection");
        xfuture* Ready = xredisClientReady(Client); interopRequire(((const xredisreadyresult*)interopWait(Ready))->Status == XREDIS_READY_OK, "transaction peer readiness");
        xbytesview Delete[] = {XRT_BYTES_LITERAL("DEL"), XRT_BYTES_LITERAL("transaction\0binary"), XRT_BYTES_LITERAL("tx-queue-abort"),
            XRT_BYTES_LITERAL("tx-runtime-error"), XRT_BYTES_LITERAL("tx-after-error"), XRT_BYTES_LITERAL("tx-acl-multi"), XRT_BYTES_LITERAL("tx-acl-exec")};
        xfuture* Call = interopCall(Client, Delete, 7); xrtFutureDestroy(Call);
        unsigned char Payload[] = {'v', 0, '\r', '\n', 0xff};
        xbytesview Set[] = {XRT_BYTES_LITERAL("SET"), Delete[1], {Payload, sizeof(Payload)}};
        xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Delete[1]};
        xredistransactioncommand Commands[] = {{Set, 3}, {Get, 2}}; xfuture* Future = NULL;
        const xredistransactionresult* Result = interopTransaction(Client, Commands, 2, &Future); xredisreplyvalue Root, First, Second;
        memset(Payload, 0x5a, sizeof(Payload));
        interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_EXECUTED &&
            Result->Request.Delivery == XREDIS_REPLIED && Result->Queued == 2 && !Result->QueueErrors &&
            xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 2 &&
            xredisReplyGet(Result->Request.Reply, Root.FirstChild, &First) && xredisReplyGet(Result->Request.Reply, First.Next, &Second) &&
            Second.Data.Size == 5 && !memcmp(Second.Data.Data, "v\0\r\n\xff", 5), "real binary EXEC array"); xrtFutureDestroy(Future);
        xbytesview QueueSet[] = {XRT_BYTES_LITERAL("SET"), Delete[2], XRT_BYTES_LITERAL("must-not-commit")};
        xbytesview Invalid = XRT_BYTES_LITERAL("__XREDIS_INVALID_TRANSACTION_COMMAND__");
        xredistransactioncommand Rejected[] = {{QueueSet, 3}, {&Invalid, 1}};
        Result = interopTransaction(Client, Rejected, 2, &Future);
        interopRequire(Result->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Result->Execution == XREDIS_TRANSACTION_NOT_EXECUTED &&
            Result->QueueError && Result->QueueErrors == 1 && Result->Queued == 1 && !Result->Request.Reply, "real queue rejection/discard"); xrtFutureDestroy(Future);
        Get[1] = Delete[2]; Call = interopCall(Client, Get, 2); Root = interopValue(Call);
        interopRequire(Root.Type == XREDIS_REPLY_NULL || (Root.Flags & XREDIS_REPLY_NULL_VALUE), "queue error committed sibling SET"); xrtFutureDestroy(Call);
        xbytesview RuntimeSet[] = {XRT_BYTES_LITERAL("SET"), Delete[3], XRT_BYTES_LITERAL("a-string")};
        xbytesview WrongType[] = {XRT_BYTES_LITERAL("LPUSH"), Delete[3], XRT_BYTES_LITERAL("wrong-type")};
        xbytesview Increment[] = {XRT_BYTES_LITERAL("INCR"), Delete[4]};
        xredistransactioncommand Runtime[] = {{RuntimeSet, 3}, {WrongType, 3}, {Increment, 2}};
        Result = interopTransaction(Client, Runtime, 3, &Future);
        interopRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_EXECUTED &&
            xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 3 && xredisReplyGet(Result->Request.Reply, Root.FirstChild, &First) &&
            xredisReplyGet(Result->Request.Reply, First.Next, &Second) && Second.Type == XREDIS_REPLY_SIMPLE_ERROR &&
            xredisReplyGet(Result->Request.Reply, Second.Next, &First) && First.Type == XREDIS_REPLY_INTEGER && First.Integer == 1,
            "runtime error lost independent EXEC children or invented rollback"); xrtFutureDestroy(Future);
        QueueSet[1] = Delete[5]; xredistransactioncommand One = {QueueSet, 3};
        interopTransactionAcl(Client, Scenario, "-multi"); Result = interopTransaction(Client, &One, 1, &Future);
        interopRequire(Result->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Result->Execution == XREDIS_TRANSACTION_NOT_EXECUTED &&
            Result->Request.Reply && !Result->Queued, "MULTI ACL rejection executed transaction body"); xrtFutureDestroy(Future);
        interopTransactionAcl(Client, Scenario, "+multi"); Get[1] = Delete[5]; Call = interopCall(Client, Get, 2); Root = interopValue(Call);
        interopRequire(Root.Type == XREDIS_REPLY_NULL || (Root.Flags & XREDIS_REPLY_NULL_VALUE), "pipelined business escaped failed MULTI"); xrtFutureDestroy(Call);
        xredisclient* Subject = NULL; interopRequire(interopConnect(Engine, Address, &Init, &Subject), "EXEC ACL subject connection");
        xfuture* SubjectReady = xredisClientReady(Subject); interopRequire(((const xredisreadyresult*)interopWait(SubjectReady))->Status == XREDIS_READY_OK, "EXEC ACL subject readiness");
        QueueSet[1] = Delete[6]; interopTransactionAcl(Client, Scenario, "-exec"); Result = interopTransaction(Subject, &One, 1, &Future);
        interopRequire(Result->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Result->Request.Reply &&
            Result->Execution == XREDIS_TRANSACTION_ABORTED, "real EXECABORT did not establish aborted execution");
        xbytesview Ping = XRT_BYTES_LITERAL("PING"); Call = interopCall(Subject, &Ping, 1); xrtFutureDestroy(Call);
        interopClose(Subject); xrtFutureDestroy(SubjectReady); xrtFutureDestroy(Future);
        interopTransactionAcl(Client, Scenario, "+exec"); Get[1] = Delete[6]; Call = interopCall(Client, Get, 2); Root = interopValue(Call);
        interopRequire(Root.Type == XREDIS_REPLY_NULL || (Root.Flags & XREDIS_REPLY_NULL_VALUE), "EXEC ACL rejection committed queued body"); xrtFutureDestroy(Call);
        interopClose(Client); xrtFutureDestroy(Ready);
    }
    puts("[PASS] real Redis transactions: RESP2/3 binary EXEC; queue rejection; runtime error without rollback; MULTI/EXEC ACL denial; safe retirement");
}
