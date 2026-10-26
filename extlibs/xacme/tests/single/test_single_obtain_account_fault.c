/* Actual Obtain orchestration and Core errors/allocator. Store and client
 * boundaries are controlled; this does not simulate a CA or client shutdown. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XACME_MODULE_ACME_OBTAIN
#include <xacme/features.h>
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "../../../../single/xrt.h"
#include "../../src/internal/xacme_flow.h"
#include <xrt/acme_obtain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum account_stage { NONE, LOAD, CREATE, EXPORT, SAVE, ISSUE } account_stage;
typedef struct account_case {
    cstr name;
    account_stage failure;
    xerrkind kind;
    bool stored, cleanup_error;
} account_case;
static const account_case Cases[] = {
    {"stored", NONE, XERR_NONE, true, false},
    {"missing", NONE, XERR_NONE, false, false},
    {"load-memory", LOAD, XERR_MEMORY, false, false},
    {"load-io", LOAD, XERR_IO, false, false},
    {"load-permission", LOAD, XERR_PERMISSION, false, false},
    {"load-state", LOAD, XERR_STATE, false, false},
    {"load-without-error", LOAD, XERR_NONE, false, false},
    {"create-memory", CREATE, XERR_MEMORY, false, false},
    {"create-without-error", CREATE, XERR_NONE, false, false},
    {"export-memory", EXPORT, XERR_MEMORY, false, false},
    {"export-without-error", EXPORT, XERR_NONE, false, false},
    {"save-memory", SAVE, XERR_MEMORY, false, false},
    {"save-io", SAVE, XERR_IO, false, false},
    {"save-without-error", SAVE, XERR_NONE, false, false},
    {"issue-memory", ISSUE, XERR_MEMORY, true, false},
    {"issue-io", ISSUE, XERR_IO, true, false},
    {"issue-without-error", ISSUE, XERR_NONE, true, false},
    {"export-cleanup-error", EXPORT, XERR_MEMORY, false, true},
    {"save-cleanup-error", SAVE, XERR_IO, false, true},
    {"issue-cleanup-error", ISSUE, XERR_IO, true, true},
};
static const account_case* Current;
static unsigned Loads, Creates, Exports, Saves, Issues, Discards;
static xerror* Cause;
static xacmeclient Client;
static xacmeaccountconfig CallerAccount;
static xacmeobtainconfig CallerConfig;
static xstrview Domain;
static xacmednsprovider Dns;
static bool HandledErrorSeen;
static struct { ptr address; size_t size; } Secrets[4];
static unsigned SecretCount, Retired, Uncleared;

static void require(bool result, cstr message)
{
    if (!result) {
        fprintf(stderr, "[FAIL] obtain %s: %s\n", Current->name, message);
        exit(1);
    }
}
static str secret(cstr text)
{
    str value = xrtStrDup(text);
    require(value != NULL && SecretCount < 4u, "secret fixture allocation");
    Secrets[SecretCount].address = value;
    Secrets[SecretCount++].size = strlen(value);
    return value;
}
static void observe_free(ptr value)
{
    for (unsigned i = 0; i < SecretCount; ++i) {
        if (value != NULL && value == Secrets[i].address) {
            uint8 nonzero = 0;
            for (size_t n = 0; n < Secrets[i].size; ++n) nonzero |= ((const uint8*)value)[n];
            if (nonzero) Uncleared++;
            Secrets[i].address = NULL;
            Retired++;
        }
    }
    xrtFree(value);
}
#undef xrtFree
#define xrtFree observe_free
#include "../../src/acme/xacme_core.c"
#include "../../src/acme/xacme_obtain.c"
#undef xrtFree

static void fail(void)
{
    require(Cause == NULL, "one primary failure");
    if (Current->kind == XERR_MEMORY) {
        require(xrtMemDebugFailAfter(0), "allocation fault armed");
        require(xrtMalloc(1) == NULL && xrtMemDebugFailTriggered(), "real allocator failure");
        xrtMemDebugFailClear();
    } else if (Current->kind != XERR_NONE) {
        xrtSetErrorInfo(Current->kind, "test.account.backend", 391,
            "account operation failed before publication");
    } else xrtClearError();
    Cause = xrtErrorRef(xrtGetError());
}
str xrtAcmeStoreLoadAccount(cstr root, cstr directory)
{
    Loads++;
    require(root == CallerConfig.sStoreRoot && directory == CallerAccount.sDirectoryUrl,
        "account load uses original store/CA");
    if (Current->failure == LOAD) { fail(); return NULL; }
    if (Current->stored) return secret("stored-account-private-key");
    xrtSetErrorInfo(XERR_NOT_FOUND, "xrt.acme.store", XACME_STORE_ERROR_NOT_FOUND,
        "account file absent");
    return NULL;
}
void xrtAcmeClientConfigInit(xacmeclientconfig* config) { memset(config, 0, sizeof(*config)); }
struct xacmeclient* xrtAcmeClientCreate(const xacmeclientconfig* config)
{
    Creates++;
    HandledErrorSeen = xrtGetError() != NULL;
    require(config->pAccount != &CallerAccount &&
        config->pAccount->sDirectoryUrl == CallerAccount.sDirectoryUrl &&
        config->pAccount->sContactEmail == CallerAccount.sContactEmail,
        "account configuration copied without changing caller");
    if (Current->stored) {
        require(config->pAccount->sAccountKeyPem == Secrets[0].address &&
            config->pAccount->Eab.sKid == NULL && config->pAccount->Eab.sHmac == NULL,
            "stored key replaces borrowed key and disables repeat EAB binding");
    } else require(config->pAccount->sAccountKeyPem == CallerAccount.sAccountKeyPem &&
        config->pAccount->Eab.sKid == CallerAccount.Eab.sKid &&
        config->pAccount->Eab.sHmac == CallerAccount.Eab.sHmac,
        "fresh account keeps explicit key/EAB views");
    require(config->sCaPem == CallerConfig.sCaPem &&
        config->pBorrowedEngine == CallerConfig.pBorrowedEngine &&
        config->uTimeoutMs == CallerConfig.uTimeoutMs &&
        config->sPropagateResolvers == CallerConfig.sPropagateResolvers &&
        config->iPropagateResolverCount == CallerConfig.iPropagateResolverCount &&
        config->uPropagateTimeoutMs == CallerConfig.uPropagateTimeoutMs &&
        config->uIssueTimeoutMs == CallerConfig.uIssueTimeoutMs &&
        config->sCertKeyPem == CallerConfig.sCertKeyPem, "client options forwarded");
    if (Current->failure == CREATE) { fail(); return NULL; }
    memset(&Client, 0, sizeof(Client)); Client.Http.uTimeoutMs = config->uTimeoutMs;
    return &Client;
}
str xrtAcmeClientAccountPem(const struct xacmeclient* client)
{
    require(client == &Client, "export correct client"); Exports++;
    if (Current->failure == EXPORT) { fail(); return NULL; }
    return secret("fresh-account-private-key");
}
bool xrtAcmeStoreSaveAccount(cstr root, cstr directory, cstr pem)
{
    Saves++;
    require(root == CallerConfig.sStoreRoot && directory == CallerAccount.sDirectoryUrl &&
        strcmp(pem, "fresh-account-private-key") == 0, "fresh account persisted before issue");
    if (Current->failure == SAVE) { fail(); return false; }
    return true;
}
bool xrtAcmeClientIssueStored(struct xacmeclient* client, const xstrview* domains,
    size_t count, const xacmednsprovider* dns, cstr root, int days,
    xacmeissuegrant* grant, bool* renewed)
{
    Issues++;
    require(client == &Client && domains == &Domain && count == 1u && dns == &Dns &&
        root == CallerConfig.sStoreRoot && days == (CallerConfig.iRenewalDays ?
        CallerConfig.iRenewalDays : 30), "issue arguments/default renewal forwarded");
    grant->sFullchainPem = xrtStrDup("certificate-chain");
    grant->sKeyPem = secret("certificate-private-key");
    require(grant->sFullchainPem != NULL, "partial grant allocation");
    *renewed = true;
    if (Current->failure == ISSUE) { fail(); return false; }
    return true;
}
void xacmeClientDiscard(xacmeclient* client)
{
    Discards++;
    require(client == &Client && client->Http.uTimeoutMs >= INT64_C(30000),
        "temporary client receives independent retirement budget");
    if (Current->cleanup_error)
        xrtSetErrorInfo(XERR_STATE, "test.account.cleanup", 392, "secondary cleanup failure");
}
static void run(const account_case* test)
{
    xacmeaccountconfig original;
    xacmeissuegrant grant;
    bool renewed = true, result;
    static const cstr resolvers[] = {"127.0.0.1"};
    Current = test; Loads = Creates = Exports = Saves = Issues = Discards = 0;
    SecretCount = Retired = Uncleared = 0; HandledErrorSeen = false;
    Cause = NULL; memset(Secrets, 0, sizeof(Secrets));
    xrtAcmeAccountConfigInit(&CallerAccount);
    CallerAccount.sDirectoryUrl = "https://ca.example/directory";
    CallerAccount.sAccountKeyPem = "borrowed-account-key";
    CallerAccount.Eab.sKid = "borrowed-eab-id";
    CallerAccount.Eab.sHmac = "borrowed-eab-key";
    CallerAccount.sContactEmail = "mailto:admin@example.test";
    original = CallerAccount;
    xrtAcmeObtainConfigInit(&CallerConfig);
    CallerConfig.pAccount = &CallerAccount; CallerConfig.sStoreRoot = "controlled-store";
    CallerConfig.sCaPem = "borrowed-ca"; CallerConfig.uTimeoutMs = 1234;
    CallerConfig.sPropagateResolvers = resolvers; CallerConfig.iPropagateResolverCount = 1u;
    CallerConfig.uPropagateTimeoutMs = 4321; CallerConfig.uIssueTimeoutMs = 9876;
    CallerConfig.sCertKeyPem = "borrowed-cert-key";
    CallerConfig.iRenewalDays = test->stored ? 14 : 0;
    Domain = XRT_STR_LITERAL("example.test"); memset(&Dns, 0, sizeof(Dns));
    memset(&grant, 0xa5, sizeof(grant));
    xrtSetErrorInfo(XERR_IO, "test.caller", 393, "old unrelated failure");
    result = xrtAcmeObtain(&CallerConfig, &Domain, 1u, &Dns, &grant, &renewed);
    printf("[obtain] %s success=%d kind=%d create=%u export=%u save=%u issue=%u discard=%u uncleared=%u\n",
        test->name, result, (int)xrtErrorKind(xrtGetError()), Creates, Exports,
        Saves, Issues, Discards, Uncleared);
    require(result == (test->failure == NONE), "operation success/failure");
    require(Loads == 1u && Creates == (test->failure != LOAD), "storage error cannot create an account");
    require(Exports == ((!test->stored && test->failure != LOAD && test->failure != CREATE) ? 1u : 0u),
        "export only after client creation and missing account");
    require(Saves == ((!test->stored && test->failure != LOAD && test->failure != CREATE &&
        test->failure != EXPORT) ? 1u : 0u), "export failure cannot persist or issue");
    require(Issues == ((test->failure == NONE || test->failure == ISSUE) ? 1u : 0u),
        "account persistence failure prevents issue");
    require(Discards == ((test->failure != LOAD && test->failure != CREATE) ? 1u : 0u),
        "every created temporary client retired exactly once");
    require(!HandledErrorSeen, "handled missing/stale diagnostic cleared before client creation");
    require(memcmp(&CallerAccount, &original, sizeof(original)) == 0, "borrowed account configuration unchanged");
    if (result) {
        require(grant.sFullchainPem != NULL && grant.sKeyPem != NULL && renewed,
            "successful grant transferred to caller");
        require(xrtGetError() == NULL, "successful cache/bootstrap leaves no stale not-found error");
        xrtAcmeGrantUnit(&grant);
    } else {
        require(grant.sFullchainPem == NULL && grant.sKeyPem == NULL && !renewed,
            "failed output and renewal indicator reset after partial issue");
        if (test->kind == XERR_NONE)
            require(xrtErrorKind(xrtGetError()) == XERR_INTERNAL, "diagnostic-free backend failure has explicit fallback");
        else require(xrtGetError() == Cause && xrtErrorKind(Cause) == test->kind,
            "primary backend/allocator diagnostic survives context and cleanup");
    }
    require(Retired == SecretCount && Uncleared == 0u,
        "all owned account/failed grant private strings cleared before real free");
    xrtErrorFree(Cause); Cause = NULL; xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    require(memory.LiveCount == 0 && memory.LiveBytes == 0 && memory.InvalidFreeCount == 0 &&
        memory.DoubleFreeCount == 0, "no logical allocation leak or invalid free");
    printf("[PASS] obtain %s\n", test->name);
}
int main(int argc, char** argv)
{
    bool selected = argc == 1;
    for (size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); ++i) {
        if (argc == 1 || strcmp(argv[1], Cases[i].name) == 0) { selected = true; run(&Cases[i]); }
    }
    return selected ? 0 : 2;
}
