#include <xredis/pool_shared.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned srCase;
static void srRequire(bool B, cstr T) {if (!B) {fprintf(stderr, "[FAIL] real shared pool case=%u: %s\n", srCase, T); exit(1);}}
static const xredisrequestresult* srWait(xfuture* F)
{srRequire(F && xrtFutureWaitFor(F, 15000) == XWAIT_OK && xrtFutureValue(F), "native result"); return xrtFutureValue(F);}
static xredisreplyvalue srValueAt(xfuture* F, int Line)
{const xredisrequestresult* R = srWait(F); xredisreplyvalue V; xredisreplyinfo I = {0}; I.Size = sizeof(I);
 if (R->Status != XREDIS_REQUEST_OK) fprintf(stderr, "line=%d status=%d delivery=%d system=%d cause=%s\n", Line, (int)R->Status, (int)R->Delivery, xrtErrorSystemCode(R->Cause), xrtErrorMessage(R->Cause));
 srRequire(R->Status == XREDIS_REQUEST_OK && xredisReplyInfo(R->Reply, &I) && xredisReplyGet(R->Reply, I.Root, &V), "native reply root"); return V;}
#define srValue(F) srValueAt(F, __LINE__)
static bool srContains(xbytesview V, cstr Text)
{size_t N = strlen(Text); for (size_t I = 0; I + N <= V.Size; ++I) if (!memcmp(V.Data + I, Text, N)) return true; return false;}
int main(int Argc, char** Argv)
{
    if (Argc != 3 || strlen(Argv[2]) != 40) return 2;
    unsigned long Port = strtoul(Argv[1], NULL, 10); if (!Port || Port > 65535) return 2;
    xnetengineconfig C; xrtNetEngineConfigInit(&C); C.Workers = 2; xnetengine* E = xrtNetEngineCreate(&C);
    srRequire(E && xrtNetEngineStart(E), "engine start"); xnetresolver* Resolver = xrtNetResolverCreate(NULL); srRequire(Resolver != NULL, "resolver");
    const xredisinitmode Modes[] = {XREDIS_INIT_RESP2, XREDIS_INIT_RESP3, XREDIS_INIT_AUTO}; xfuture* Retained[18] = {0}; size_t Saved = 0;
    for (unsigned Factory = 0; Factory < 3; ++Factory) for (unsigned Mode = 0; Mode < 3; ++Mode) for (unsigned ACL = 0; ACL < 2; ++ACL) {
        srCase = Factory * 6 + Mode * 2 + ACL;
        xredisclient* Clients[2] = {0}; int64 Ids[2];
        for (unsigned Member = 0; Member < 2; ++Member) {
            xredisclientinitconfig I; xredisClientInitConfigInit(&I); I.Mode = Modes[Mode]; I.Timeout = 10000;
            if (ACL) {I.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME; I.Username = (xbytesview)XRT_BYTES_LITERAL("shared-fixture");}
            xredisclientdialconfig D; xredisClientDialConfigInit(&D); D.Family = XNET_FAMILY_IPV4;
            xnetaddr Address; srRequire(xrtNetAddrParse(&Address, "127.0.0.1", (uint16)Port), "numeric owned target"); bool OK;
            if (Factory == 1) OK = xredisClientDial(E, Resolver, XRT_STR_LITERAL("localhost"), (uint16)Port, Member, NULL, &I, &D, &Clients[Member]);
            else if (Factory == 2) {xnetaddrlist* L = xrtNetAddrListCreate(&Address, 1); srRequire(L != NULL, "prepared addresses");
                OK = xredisClientDialAddresses(E, L, Member, NULL, &I, &D, &Clients[Member]); xrtNetAddrListDestroy(L);}
            else OK = xredisClientConnectInit(E, &Address, Member, NULL, &I, &Clients[Member]);
            srRequire(OK, "managed factory"); xfuture* Ready = xredisClientReady(Clients[Member]);
            srRequire(xrtFutureWaitFor(Ready, 15000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "authenticated ready"); xrtFutureDestroy(Ready);
            xbytesview Info[] = {XRT_BYTES_LITERAL("INFO"), XRT_BYTES_LITERAL("SERVER")}; xfuture* F = NULL;
            srRequire(xredisClientCall(Clients[Member], Info, 2, 10000, &F) == XREDIS_ADMISSION_ACCEPTED && srContains(srValue(F).Data, Argv[2]), "actual owned run ID"); xrtFutureDestroy(F);
            xbytesview ID[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; F = NULL;
            srRequire(xredisClientCall(Clients[Member], ID, 2, 10000, &F) == XREDIS_ADMISSION_ACCEPTED, "original client ID"); Ids[Member] = srValue(F).Integer; xrtFutureDestroy(F);
            double Due = xrtTimer() + 10; xredisclientinfo CI = {0}; CI.Size = sizeof(CI);
            do {srRequire(xredisClientInfo(Clients[Member], &CI) && xrtTimer() < Due, "pre-pool physical idle"); if (CI.Requests) xrtThreadYield();} while (CI.Requests);
        }
        srRequire(Ids[0] != Ids[1], "distinct physical members"); xredispool* Pool = NULL;
        srRequire(xredisSharedPoolCreate(E, 1, Clients, 2, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "fixed shared pool");
        for (unsigned Member = 0; Member < 2; ++Member) {xbytesview ID[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; xfuture* F = NULL;
            srRequire(xredisSharedPoolCall(Pool, ID, 2, 10000, &F) == XREDIS_ADMISSION_ACCEPTED && srValue(F).Integer == Ids[Member], "native round robin same connections"); xrtFutureDestroy(F);}
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("shared\0value")}; xfuture* Calls[64] = {0};
        for (unsigned N = 0; N < 64; ++N) srRequire(xredisSharedPoolCallAsync(Pool, Echo, 2, 10000, &Calls[N]) == XREDIS_ADMISSION_ACCEPTED, "native pipeline");
        for (unsigned N = 0; N < 64; ++N) {xredisreplyvalue V = srValue(Calls[N]); srRequire(V.Data.Size == 12 && !memcmp(V.Data.Data, "shared\0value", 12), "binary pipeline correlation"); if (N) xrtFutureDestroy(Calls[N]);}
        xfuture* Closed = xredisClientClosed(Clients[0]); xredisClientClose(Clients[0]); srRequire(xrtFutureWaitFor(Closed, 15000) == XWAIT_OK, "member physical close"); xrtFutureDestroy(Closed);
        double Due = xrtTimer() + 10; xredispoolinfo PI = {0}; PI.Size = sizeof(PI);
        do {srRequire(xredisPoolInfo(Pool, &PI) && xrtTimer() < Due, "pool failed to retire member"); if (PI.Live != 1) xrtThreadYield();} while (PI.Live != 1);
        xfuture* F = NULL; srRequire(xredisSharedPoolCall(Pool, Echo, 2, 10000, &F) == XREDIS_ADMISSION_ACCEPTED && srValue(F).Data.Size == 12, "surviving member call"); xrtFutureDestroy(F);
        Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); srRequire(xrtFutureWaitFor(Closed, 15000) == XWAIT_OK && xredisPoolInfo(Pool, &PI) && !PI.Requests && !PI.CommandBytes && !PI.Pending && !PI.Leased && PI.Retired == 2, "physical pool retirement");
        xrtFutureDestroy(Closed); xredisPoolRelease(Pool);
        for (unsigned Member = 0; Member < 2; ++Member) {xredisclientinfo CI = {0}; CI.Size = sizeof(CI); srRequire(xredisClientInfo(Clients[Member], &CI) && !CI.Requests && !CI.CommandBytes && CI.Accepted == CI.Completed, "native tail accounting"); xredisClientRelease(Clients[Member]);}
        Retained[Saved++] = Calls[0];
    }
    srRequire(xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E), "engine ownership retired");
    for (size_t I = 0; I < Saved; ++I) {srRequire(srValue(Retained[I]).Data.Size == 12, "retained replies survive engine"); xrtFutureDestroy(Retained[I]);}
    puts("[PASS] real shared pool: 18 numeric/DNS/prepared RESP2/3/AUTO ACL cases, two members, pipeline, retirement and retained replies"); return 0;
}
