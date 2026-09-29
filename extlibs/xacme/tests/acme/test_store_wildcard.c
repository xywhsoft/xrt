#include "../test.h"

#include <xrt/acme_store.h>
#include <xrt/file.h>
#include <xrt/random.h>

#include <string.h>

int main(void)
{
	char sRoot[340];
	char sMapped[420];
	char aDomains[4][256];
	size_t iCount = 0u;
	size_t i;
	int iMatches = 0;
	xacmeissuegrant Grant = {
		(str)"wildcard chain\n", (str)"wildcard key\n"
	};
	xacmeissuegrant Loaded;
	uint64 iNonce;
	testRequire(xrtSecureRandom(&iNonce, sizeof(iNonce)),
		"acme store wildcard fixture random failed");
	snprintf(sRoot, sizeof(sRoot), "%s/store_wildcard-%016llx",
		testOutRoot(), (unsigned long long)iNonce);
	snprintf(sMapped, sizeof(sMapped),
		"%s/certs/%%2A.wildcard.example.com/current", sRoot);
	testRequire(xrtAcmeStoreSaveGrant(sRoot,
		"*.wildcard.example.com", &Grant, NULL),
		"acme store wildcard grant save failed");
	testRequire(xrtFileExists(sMapped),
		"acme store wildcard directory is not portable");
	testRequire(xrtAcmeStoreLoadGrant(sRoot,
		"*.wildcard.example.com", &Loaded) &&
		strcmp(Loaded.sFullchainPem, Grant.sFullchainPem) == 0 &&
		strcmp(Loaded.sKeyPem, Grant.sKeyPem) == 0,
		"acme store wildcard grant roundtrip failed");
	xrtAcmeGrantUnit(&Loaded);
	testRequire(xrtAcmeStoreListDomains(sRoot,
		aDomains, 4u, &iCount),
		"acme store wildcard domain list failed");
	for(i = 0u; i < iCount; i++)
		if(strcmp(aDomains[i], "*.wildcard.example.com") == 0)
			iMatches++;
	testRequire(iMatches == 1,
		"acme store wildcard domain list missing or duplicated name");
	#if !defined(_WIN32) && !defined(_WIN64)
		{
			char sLegacy[420];
			char sGeneration[450];
			char sFile[500];
			xacmeissuegrant Legacy;
			xacmeissuegrant Migrated = {
				(str)"migrated chain\n", (str)"migrated key\n"
			};
			snprintf(sLegacy, sizeof(sLegacy),
				"%s/certs/*.legacy.example.com", sRoot);
			snprintf(sGeneration, sizeof(sGeneration),
				"%s/.grant-0123456789abcdef", sLegacy);
			testRequire(xrtDirCreateAll(sGeneration),
				"acme store legacy wildcard fixture directory failed");
			snprintf(sFile, sizeof(sFile),
				"%s/fullchain.pem", sGeneration);
			testRequire(xrtFileWriteAtomic(sFile,
				(xbytesview){ (const uint8*)"legacy chain\n", 13u }),
				"acme store legacy wildcard chain fixture failed");
			snprintf(sFile, sizeof(sFile), "%s/key.pem", sGeneration);
			testRequire(xrtFileWriteAtomic(sFile,
				(xbytesview){ (const uint8*)"legacy key\n", 11u }),
				"acme store legacy wildcard key fixture failed");
			snprintf(sFile, sizeof(sFile), "%s/current", sLegacy);
			testRequire(xrtFileWriteAtomic(sFile,
				(xbytesview){
					(const uint8*)".grant-0123456789abcdef\n", 24u }),
				"acme store legacy wildcard pointer fixture failed");
			testRequire(xrtAcmeStoreLoadGrant(sRoot,
				"*.legacy.example.com", &Legacy) &&
				strcmp(Legacy.sFullchainPem, "legacy chain\n") == 0 &&
				strcmp(Legacy.sKeyPem, "legacy key\n") == 0,
				"acme store legacy wildcard fallback failed");
			xrtAcmeGrantUnit(&Legacy);
			testRequire(xrtAcmeStoreSaveGrant(sRoot,
				"*.legacy.example.com", &Migrated, NULL),
				"acme store wildcard migration save failed");
			testRequire(xrtAcmeStoreLoadGrant(sRoot,
				"*.legacy.example.com", &Legacy) &&
				strcmp(Legacy.sFullchainPem, Migrated.sFullchainPem) == 0 &&
				strcmp(Legacy.sKeyPem, Migrated.sKeyPem) == 0,
				"acme store mapped wildcard did not supersede legacy");
			xrtAcmeGrantUnit(&Legacy);
			testRequire(xrtAcmeStoreListDomains(sRoot,
				aDomains, 4u, &iCount),
				"acme store migrated wildcard list failed");
			iMatches = 0;
			for(i = 0u; i < iCount; i++)
				if(strcmp(aDomains[i], "*.legacy.example.com") == 0)
					iMatches++;
			testRequire(iMatches == 1,
				"acme store wildcard migration duplicated domain listing");
		}
	#endif
	printf("[PASS] acme store wildcard portable path\n");
	return 0;
}
