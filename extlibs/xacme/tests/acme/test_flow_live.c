#include "../test.h"

#include <xrt/acme_client.h>
#include <xrt/acme_store.h>

#include <stdlib.h>
#include <string.h>

typedef struct live_dns_trace {
	xacmednsprovider* provider;
	unsigned pending;
	unsigned added;
	unsigned removed;
} live_dns_trace;

static bool liveDnsAdd(xacmednsprovider* provider, xstrview owner, xstrview txt)
{
	live_dns_trace* trace = provider->pContext;
	if(!trace->provider->Add(trace->provider, owner, txt)) return false;
	trace->pending++;
	trace->added++;
	return true;
}

static bool liveDnsRemove(xacmednsprovider* provider, xstrview owner, xstrview txt)
{
	live_dns_trace* trace = provider->pContext;
	if(!trace->provider->Remove(trace->provider, owner, txt)) return false;
	if(trace->pending != 0u) trace->pending--;
	trace->removed++;
	return true;
}

static bool liveDnsPropagate(xacmednsprovider* provider, xstrview owner, xstrview txt)
{
	live_dns_trace* trace = provider->pContext;
	return trace->provider->Propagate(trace->provider, owner, txt);
}

/* Real DNS-01 execution is opt-in. Credentials and the authorized domain are
 * supplied at runtime; ordinary test runs never contact a CA or DNS provider. */
int main(void)
{
	const char* live = getenv("XACME_LIVE");
	const char* key = getenv("XACME_ALI_KEY");
	const char* secret = getenv("XACME_ALI_SECRET");
	const char* domain = getenv("XACME_LIVE_DOMAIN");
	const char* root = getenv("XACME_LIVE_STORE");
	const char* directory = getenv("XACME_LIVE_DIRECTORY");
	xacmednaliconfig ali_config;
	xacmednsprovider ali = { 0 };
	xacmednsprovider observed = { 0 };
	live_dns_trace trace = { 0 };
	xacmeaccountconfig account;
	xacmeclientconfig config;
	struct xacmeclient* client = NULL;
	xacmeissuegrant grant = { 0 }, stored = { 0 }, cached = { 0 };
	xstrview names[1];
	char* account_pem = NULL;
	char* fresh_account_pem = NULL;
	const char* stage = "configuration";
	bool renewed = false, ok = false;
	size_t pending = 0u;

	if(live == NULL || strcmp(live, "1") != 0)
	{
		puts("[SKIP] acme flow live (requires XACME_LIVE=1)");
		return 0;
	}
	if(key == NULL || key[0] == '\0' || secret == NULL ||
		secret[0] == '\0' || domain == NULL || domain[0] == '\0' ||
		root == NULL || root[0] == '\0')
	{
		fputs("[FAIL] live ACME requires ALI_KEY, ALI_SECRET, "
			"LIVE_DOMAIN and LIVE_STORE runtime configuration\n", stderr);
		return 2;
	}
	if(directory == NULL || directory[0] == '\0')
		directory = XACME_DIRECTORY_LE_STAGING;
	names[0] = xrtStrView(domain);

	stage = "DNS provider construction";
	xrtAcmeDnsAliConfigInit(&ali_config);
	ali_config.sAccessKeyId = key;
	ali_config.sAccessKeySecret = secret;
	if(!xrtAcmeDnsAli(&ali_config, NULL, &ali)) goto Done;
	trace.provider = &ali;
	observed = ali;
	observed.pContext = &trace;
	observed.Add = liveDnsAdd;
	observed.Remove = liveDnsRemove;
	observed.Propagate = ali.Propagate != NULL ? liveDnsPropagate : NULL;

	xrtAcmeAccountConfigInit(&account);
	account.sDirectoryUrl = directory;
	account.sContactEmail = getenv("XACME_LIVE_CONTACT");
	account_pem = xrtAcmeStoreLoadAccount(root, directory);
	account.sAccountKeyPem = account_pem;
	xrtAcmeClientConfigInit(&config);
	config.pAccount = &account;
	config.uTimeoutUs = UINT64_C(30000000);
	config.uIssueTimeoutUs = UINT64_C(300000000);
	config.uPropagateTimeoutMs = 120000u;
	stage = "CA client construction";
	client = xrtAcmeClientCreate(&config);
	if(client == NULL) goto Done;
	if(account_pem == NULL)
	{
		stage = "account persistence";
		fresh_account_pem = xrtAcmeClientAccountPem(client);
		if(fresh_account_pem == NULL ||
			!xrtAcmeStoreSaveAccount(root, directory, fresh_account_pem)) goto Done;
	}
	stage = "DNS-01 issuance";
	if(!xrtAcmeClientIssueStored(client, names, 1u, &observed, root,
		30, &grant, &renewed)) goto Done;
	stage = "owned TXT cleanup verification";
	if(trace.pending != 0u || trace.added == 0u || trace.removed != trace.added)
		goto Done;
	stage = "stored grant verification";
	if(!xrtAcmeStoreLoadGrant(root, domain, &stored) ||
		strcmp(stored.sFullchainPem, grant.sFullchainPem) != 0 ||
		strcmp(stored.sKeyPem, grant.sKeyPem) != 0) goto Done;
	stage = "cached grant verification";
	if(!xrtAcmeClientIssueStored(client, names, 1u, &observed, root,
		30, &cached, &renewed) || renewed ||
		strcmp(cached.sFullchainPem, grant.sFullchainPem) != 0 ||
		strcmp(cached.sKeyPem, grant.sKeyPem) != 0 || trace.pending != 0u ||
		trace.added != trace.removed) goto Done;
	ok = true;

Done:
	/* Do not print arbitrary provider/CA messages, credentials, account IDs or
	 * private keys. The flow removes only its own exact TXT values. */
	if(!ok) fprintf(stderr, "[FAIL] %s: kind=%d code=%d\n", stage,
		(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
	xrtAcmeGrantUnit(&grant);
	xrtAcmeGrantUnit(&stored);
	xrtAcmeGrantUnit(&cached);
	xrtFree(account_pem);
	xrtFree(fresh_account_pem);
	if(client != NULL)
	{
		if(xrtAcmeClientCleanup(client)) xrtAcmeClientDestroy(client);
		else { fputs("[FAIL] CA client cleanup incomplete\n", stderr); ok = false; }
	}
	xrtAcmeDnsAliProviderUnit(&ali);
	if(ali.pContext != NULL)
	{
		fputs("[FAIL] DNS provider cleanup incomplete\n", stderr);
		ok = false;
	}
	if(!xrtAcmeCleanupPending(UINT64_C(30000000), &pending) || pending != 0u)
	{
		fputs("[FAIL] pending ACME cleanup incomplete\n", stderr);
		ok = false;
	}
	testRequire(ok, "live ACME acceptance failed");
	puts("[PASS] acme live DNS-01, account/store and cached grant");
	return 0;
}
