#ifndef XREDIS_EXAMPLE_SENTINEL_SEED_POLICY_H
#define XREDIS_EXAMPLE_SENTINEL_SEED_POLICY_H

#include <xredis/client.h>
#include <stdio.h>
#include <string.h>

static bool appSeedText(const xredisreplyvalue* Value, const char* Text)
{
    size_t Bytes = strlen(Text);
    return (Value->Type == XREDIS_REPLY_SIMPLE_STRING || Value->Type == XREDIS_REPLY_BULK_STRING) &&
        Value->Data.Size == Bytes && !memcmp(Value->Data.Data, Text, Bytes);
}

static bool appSeedListed(const xredisreply* Reply, uint16 Port)
{
    xredisreplyinfo Info = {.Size = sizeof(Info)};
    xredisreplyvalue List;
    if (!Reply || !xredisReplyInfo(Reply, &Info) || Info.PayloadBytes > 65536 ||
        !xredisReplyGet(Reply, Info.Root, &List) || List.Type != XREDIS_REPLY_ARRAY ||
        List.ChildCount > 64) return false;

    char PortText[6];
    snprintf(PortText, sizeof(PortText), "%u", (unsigned)Port);
    for (size_t Index = List.FirstChild; Index != XREDIS_REPLY_NONE;) {
        xredisreplyvalue Row;
        if (!xredisReplyGet(Reply, Index, &Row)) return false;
        Index = Row.Next;
        if ((Row.Type != XREDIS_REPLY_ARRAY && Row.Type != XREDIS_REPLY_MAP) ||
            Row.ChildCount > 128 || Row.ChildCount % 2) continue;
        unsigned Match = 0, Seen = 0;
        for (size_t Pair = Row.FirstChild; Pair != XREDIS_REPLY_NONE;) {
            xredisreplyvalue Key, Value;
            if (!xredisReplyGet(Reply, Pair, &Key) || !xredisReplyGet(Reply, Key.Next, &Value))
                return false;
            Pair = Value.Next;
            unsigned Field = 0;
            const char* Expected = NULL;
            if (appSeedText(&Key, "ip")) { Field = 1; Expected = "127.0.0.1"; }
            else if (appSeedText(&Key, "port")) { Field = 2; Expected = PortText; }
            else if (appSeedText(&Key, "flags")) { Field = 4; Expected = "sentinel"; }
            if (!Field) continue;
            if (Seen & Field) { Match = 0; break; }
            Seen |= Field;
            if (appSeedText(&Value, Expected)) Match |= Field;
        }
        if (Match == 7) return true;
    }
    return false;
}

/* Run on the APP owner and publish accepted addresses to its seed factory.
 * Trust only the configured loopback backup, even when peers advertise others. */
static bool appSeedRead(xredisclient* Client, xbytesview Service, uint16 AllowedPort)
{
    xfuture* Ready = xredisClientReady(Client);
    const xredisreadyresult* Result = Ready && xrtFutureWaitFor(Ready, 2000) == XWAIT_OK ?
        xrtFutureValue(Ready) : NULL;
    bool Connected = Result && Result->Status == XREDIS_READY_OK;
    xrtFutureDestroy(Ready);
    if (!Connected) return false;

    const xbytesview Args[] = {XRT_BYTES_LITERAL("SENTINEL"), XRT_BYTES_LITERAL("SENTINELS"), Service};
    xfuture* Query = NULL;
    if (xredisClientCallAsync(Client, Args, 3, 500, &Query) != XREDIS_ADMISSION_ACCEPTED) return false;
    const xredisrequestresult* Reply = xrtFutureWaitFor(Query, 1000) == XWAIT_OK ?
        xrtFutureValue(Query) : NULL;
    bool Listed = Reply && Reply->Status == XREDIS_REQUEST_OK && appSeedListed(Reply->Reply, AllowedPort);
    xrtFutureDestroy(Query);
    return Listed;
}
#endif
