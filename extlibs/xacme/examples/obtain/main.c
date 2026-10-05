/* Manual DNS-01 example: public ACME APIs, account/store reuse and explicit TXT confirmation. */
#define XACME_MODULE_ACME_OBTAIN
#if defined(ACME_EXAMPLE_SINGLE)
#include <xacme/features.h>
#define XRT_IMPLEMENTATION
#include <xrt.h>
#define XACME_IMPLEMENTATION
#endif
#include <xacme.h>
#include <stdio.h>
#include <string.h>

typedef struct manualdns { unsigned pending; } manualdns;

static bool confirm_dns(cstr action, xstrview owner, xstrview txt)
{
    char answer[16];
    printf("DNS_%s %.*s %.*s\n", action, (int)owner.Size, owner.Data, (int)txt.Size, txt.Data);
    fflush(stdout);
    if(fgets(answer, sizeof(answer), stdin) == NULL ||
       (strcmp(answer, "yes\n") != 0 && strcmp(answer, "yes\r\n") != 0)) {
        xrtSetErrorInfo(XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_UNCERTAIN,
                       "manual TXT change was not confirmed; verify the printed owner/value");
        return false;
    }
    return true;
}

static bool dns_add(xacmednsprovider* provider, xstrview owner, xstrview txt)
{
    manualdns* dns = provider->pContext;
    dns->pending++;
    return confirm_dns("ADD", owner, txt);
}

static bool dns_remove(xacmednsprovider* provider, xstrview owner, xstrview txt)
{
    manualdns* dns = provider->pContext;
    if(!confirm_dns("REMOVE", owner, txt)) return false;
    if(dns->pending != 0u) dns->pending--;
    return true;
}

static str read_ca(cstr path)
{
    FILE* file = fopen(path, "rb");
    long length;
    str text = NULL;
    if(file == NULL) return NULL;
    if(fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && length <= 65536 &&
       fseek(file, 0, SEEK_SET) == 0) {
        text = xrtMalloc((size_t)length + 1u);
        if(text != NULL) {
            if(fread(text, 1u, (size_t)length, file) != (size_t)length) {
                xrtFree(text); text = NULL;
            } else text[length] = '\0';
        }
    }
    fclose(file);
    return text;
}

int main(int argc, char** argv)
{
    xacmeaccountconfig account;
    xacmeobtainconfig config;
    xacmednsprovider provider = { 0 };
    xacmeissuegrant first = { 0 }, cached = { 0 }, stored = { 0 };
    manualdns dns = { 0 };
    str ca = NULL, account_pem = NULL;
    xstrview domain;
    cstr resolver;
    bool renewed = false, ok = false;
    size_t pending = 0u;
    if(argc != 6) {
        puts("Usage: acme_obtain DIRECTORY CA.pem RESOLVER STORE DOMAIN");
        puts("For each DNS_ADD or DNS_REMOVE, apply that TXT change and enter yes.");
        return argc == 1 ? 0 : 2;
    }
    ca = read_ca(argv[2]);
    if(ca == NULL) { fputs("Cannot read CA PEM\n", stderr); return 1; }
    xrtAcmeAccountConfigInit(&account); account.sDirectoryUrl = argv[1];
    xrtAcmeObtainConfigInit(&config);
    config.pAccount = &account; config.sCaPem = ca;
    config.sStoreRoot = argv[4]; config.iRenewalDays = 30;
    resolver = argv[3]; config.sPropagateResolvers = &resolver; config.iPropagateResolverCount = 1u;
    config.uTimeoutUs = UINT64_C(10000000); config.uIssueTimeoutUs = UINT64_C(120000000);
    config.uPropagateTimeoutMs = 30000u;
    provider.sId = "manual"; provider.pContext = &dns; provider.Add = dns_add; provider.Remove = dns_remove;
    domain = (xstrview){ argv[5], strlen(argv[5]) };
    if(!xrtAcmeObtain(&config, &domain, 1u, &provider, &first, &renewed)) goto Done;
    printf("CERT_READY renewed=%u\n", renewed ? 1u : 0u); fflush(stdout);
    if(!xrtAcmeStoreLoadGrant(argv[4], argv[5], &stored) ||
       strcmp(stored.sFullchainPem, first.sFullchainPem) != 0 ||
       strcmp(stored.sKeyPem, first.sKeyPem) != 0) goto Done;
    account_pem = xrtAcmeStoreLoadAccount(argv[4], argv[1]);
    if(account_pem == NULL) goto Done;
    if(!xrtAcmeObtain(&config, &domain, 1u, &provider, &cached, &renewed) || renewed ||
       strcmp(cached.sFullchainPem, first.sFullchainPem) != 0 ||
       strcmp(cached.sKeyPem, first.sKeyPem) != 0 || dns.pending != 0u) goto Done;
    puts("CACHE_READY account=stored grant=matched pending_dns=0"); fflush(stdout);
    ok = true;
Done:
    if(!ok) fprintf(stderr, "ACME operation failed: kind=%d code=%d message=%s pending_dns=%u\n", xrtErrorKind(xrtGetError()),
        xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()) ? xrtErrorMessage(xrtGetError()) : "", dns.pending);
    xrtAcmeGrantUnit(&first); xrtAcmeGrantUnit(&cached); xrtAcmeGrantUnit(&stored);
    xrtFree(account_pem); xrtFree(ca);
    if(!xrtAcmeCleanupPending(UINT64_C(30000000), &pending)) {
        fprintf(stderr, "ACME cleanup incomplete: pending=%zu\n", pending);
        ok = false;
    }
    return ok ? 0 : 1;
}
