#include "../test.h"

#include <xrt/acme_store.h>
#include <xrt/file.h>
#include <xrt/pem.h>
#include <xrt/x509.h>
#include <xrt/time.h>

#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/types.h>
	#include <sys/stat.h>
#endif

/*
	临时目录做存取往返；NeedRenew 用真实 LE staging 证书（若在
	XACME_STORE_CERT 指定路径），否则手工生成短期证书场景只验证
	"缺文件→需要续"路径。
*/

#define STORE_ROOT testOutRoot()

static bool testStoreCurrentFilePath(char* sOut, size_t iCapacity,
	cstr sDomain, cstr sFile)
{
	char sPointer[400];
	char sGeneration[40];
	FILE* pFile;
	int n = snprintf(sPointer, sizeof(sPointer),
		"%s/certs/%s/current", STORE_ROOT, sDomain);
	if(n < 0 || (size_t)n >= sizeof(sPointer))
		return false;
	pFile = fopen(sPointer, "rb");
	if(pFile == NULL)
		return false;
	if(fgets(sGeneration, sizeof(sGeneration), pFile) == NULL)
	{
		fclose(pFile);
		return false;
	}
	if(fclose(pFile) != 0)
		return false;
	sGeneration[strcspn(sGeneration, "\r\n")] = '\0';
	if(strlen(sGeneration) != 23u ||
		strncmp(sGeneration, ".grant-", 7u) != 0)
		return false;
	n = snprintf(sOut, iCapacity, "%s/certs/%s/%s/%s",
		STORE_ROOT, sDomain, sGeneration, sFile);
	return n >= 0 && (size_t)n < iCapacity;
}

int main(void)
{
	const char* sPem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIGgAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBIGFMIGCAgEBBCDJr6nYRbp1Fmtc\n"
		"IVdnsdaTTlDD2zbomxJ7imIrEg9nIaAKBggqhkjOPQMBaFEA0IABGD+1LolWp0x\n"
		"yWXrdMY1bWjASbiSO2H6bOZYi5jg8p+2eQP+EAi4vJmkGunpVii8ZPLxsgwtfp9R\n"
		"d6PClNRGIpk=\n"
		"-----END PRIVATE KEY-----\n";
	str sLoaded;
	bool bNeed = false;

	/* 账户存取往返 + 多 CA 隔离。 */
	testRequire(
		xrtAcmeStoreSaveAccount(
			STORE_ROOT, "https://acme-staging-v02.api.letsencrypt.org/directory", sPem),
		"acme store save account failed"
	);
	sLoaded = xrtAcmeStoreLoadAccount(
		STORE_ROOT, "https://acme-staging-v02.api.letsencrypt.org/directory");
	testRequire(
		(sLoaded != NULL) && (strcmp(sLoaded, sPem) == 0),
		"acme store account roundtrip mismatch"
	);
	xrtFree(sLoaded);
	testRequire(
		xrtAcmeStoreLoadAccount(
			STORE_ROOT, "https://acme.zerossl.com/v2/DV90") == NULL,
		"acme store account isolation mismatch"
	);

	/* 证书存取 + 溯源。 */
	testRequire(bNeed == false, "acme store fresh cert should not renew");
	testRequire(
		xrtAcmeStoreNeedRenew(STORE_ROOT, "test.xxrpa.com", 90, &bNeed),
		"acme store need renew 90 failed"
	);
	testRequire(bNeed == true, "acme store near-expiry should renew");
	testRequire(
		xrtAcmeStoreNeedRenew(
			STORE_ROOT, "missing.example.com", 30, &bNeed) && bNeed,
		"acme store missing cert should renew"
	);

	/* 签发产物整体存取（key.pem + fullchain.pem）。 */
	{
		xacmeissuegrant Grant;
		xacmeissuegrant Loaded;
		Grant.sFullchainPem = (str)sPem; /* 借用内容，Save 只读 */
		Grant.sKeyPem = (str)xrtMalloc(strlen(sPem) + 1u);
		testRequire(Grant.sKeyPem != NULL, "acme store grant key alloc failed");
		memcpy(Grant.sKeyPem, sPem, strlen(sPem) + 1u);
		#if !defined(_WIN32) && !defined(_WIN64)
			mode_t iOldUmask = umask(0022);
		#endif
		testRequire(
			xrtAcmeStoreSaveGrant(
				STORE_ROOT, "grant.example.com", &Grant,
				"https://acme-staging-v02.api.letsencrypt.org/directory"),
			"acme store save grant failed"
		);
		#if !defined(_WIN32) && !defined(_WIN64)
			{
				char sKeyPath[400];
				struct stat Info;
				(void)umask(iOldUmask);
				testRequire(testStoreCurrentFilePath(sKeyPath,
					sizeof(sKeyPath), "grant.example.com", "key.pem"),
					"acme store current key path missing");
				testRequire((stat(sKeyPath, &Info) == 0) &&
					((Info.st_mode & 0777) == 0600),
					"acme store private key must be 0600 under umask 022");
			}
		#endif
		xrtFree(Grant.sKeyPem);
		Grant.sKeyPem = NULL;
		Grant.sFullchainPem = NULL;
		testRequire(
			xrtAcmeStoreLoadGrant(STORE_ROOT, "grant.example.com", &Loaded) &&
				(Loaded.sFullchainPem != NULL) &&
				(Loaded.sKeyPem != NULL) &&
				(strcmp(Loaded.sFullchainPem, sPem) == 0) &&
				(strcmp(Loaded.sKeyPem, sPem) == 0),
			"acme store grant roundtrip mismatch"
		);
		xrtAcmeGrantUnit(&Loaded);
		/* 新一代只在三份产物齐备后提交；旧版仍可供并发读者读取。 */
		{
			char sOldKey[400];
			char sNewKey[400];
			const char* sNextChain = "next fullchain\n";
			const char* sNextKey = "next private key\n";
			testRequire(testStoreCurrentFilePath(sOldKey,
				sizeof(sOldKey), "grant.example.com", "key.pem"),
				"acme store old generation path missing");
			Grant.sFullchainPem = (str)sNextChain;
			Grant.sKeyPem = (str)sNextKey;
			testRequire(xrtAcmeStoreSaveGrant(STORE_ROOT,
				"grant.example.com", &Grant, "https://next.example/directory"),
				"acme store grant rotation failed");
			testRequire(testStoreCurrentFilePath(sNewKey,
				sizeof(sNewKey), "grant.example.com", "key.pem") &&
				strcmp(sOldKey, sNewKey) != 0 && xrtFileExists(sOldKey),
				"acme store rotation must retain old generation");
			testRequire(xrtAcmeStoreLoadGrant(
				STORE_ROOT, "grant.example.com", &Loaded) &&
				strcmp(Loaded.sFullchainPem, sNextChain) == 0 &&
				strcmp(Loaded.sKeyPem, sNextKey) == 0,
				"acme store rotated grant mixed generations");
			xrtAcmeGrantUnit(&Loaded);
			{
				str sCa = xrtAcmeStoreLoadCertCa(
					STORE_ROOT, "grant.example.com");
				str sChain = xrtAcmeStoreLoadCert(
					STORE_ROOT, "grant.example.com");
				testRequire(sCa != NULL && sChain != NULL &&
					strcmp(sCa, "https://next.example/directory") == 0 &&
					strcmp(sChain, sNextChain) == 0,
					"acme store rotated metadata or chain mismatch");
				xrtFree(sCa);
				xrtFree(sChain);
			}
			{
				const size_t iHugeSize = 1024u * 1024u + 1u;
				str sHugeKey = (str)xrtMalloc(iHugeSize + 1u);
				testRequire(sHugeKey != NULL,
					"acme store oversized key fixture alloc failed");
				memset(sHugeKey, 'x', iHugeSize);
				sHugeKey[iHugeSize] = '\0';
				Grant.sKeyPem = sHugeKey;
				xrtClearError();
				testRequire(!xrtAcmeStoreSaveGrant(STORE_ROOT,
					"grant.example.com", &Grant, NULL) &&
					xrtErrorKind(xrtGetError()) == XERR_RANGE,
					"acme store must reject unreadable oversized key");
				Grant.sKeyPem = (str)sNextKey;
				xrtFree(sHugeKey);
			}
			xrtClearError();
			testRequire(!xrtAcmeStoreSaveCert(STORE_ROOT,
				"grant.example.com", sPem, NULL) &&
				xrtErrorKind(xrtGetError()) == XERR_STATE,
				"acme store standalone cert must not replace a grant");
			xrtClearError();
			{
				char sPointer[400];
				size_t iPointerSize = 0u;
				bytes pPointer;
				snprintf(sPointer, sizeof(sPointer),
					"%s/certs/grant.example.com/current", STORE_ROOT);
				pPointer = xrtFileReadAllLimit(sPointer, 64u, &iPointerSize);
				testRequire(pPointer != NULL,
					"acme store current fixture read failed");
				testRequire(xrtFileWriteAtomic(sPointer,
					(xbytesview){ (const uint8*)"../escape\n", 10u }),
					"acme store corrupt current fixture failed");
				xrtClearError();
				testRequire(!xrtAcmeStoreLoadGrant(
					STORE_ROOT, "grant.example.com", &Loaded) &&
					Loaded.sFullchainPem == NULL && Loaded.sKeyPem == NULL &&
					xrtErrorKind(xrtGetError()) == XERR_PROTOCOL,
					"acme store must reject traversal in current pointer");
				testRequire(xrtFileWriteAtomic(sPointer,
					(xbytesview){ pPointer, iPointerSize }),
					"acme store current fixture restore failed");
				xrtFree(pPointer);
				xrtClearError();
			}
		}
		/* key 缺失 → NOT_FOUND（删 key.pem 后链仍在）。 */
		{
			char sPath[400];
			testRequire(testStoreCurrentFilePath(sPath, sizeof(sPath),
				"grant.example.com", "key.pem"),
				"acme store current key path missing");
			testRequire(remove(sPath) == 0, "acme store key remove failed");
			xrtClearError();
			testRequire(
				!xrtAcmeStoreLoadGrant(
					STORE_ROOT, "grant.example.com", &Loaded) &&
					(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
				"acme store grant missing key mismatch"
			);
		}
	}
	/* 无 current 指针的旧布局仍能读取，迁移无需重签证书。 */
	{
		char sLegacyKey[400];
		xacmeissuegrant Legacy;
		testRequire(xrtAcmeStoreSaveCert(STORE_ROOT,
			"legacy.example.com", sPem, "https://legacy.example/dir"),
			"acme store legacy cert save failed");
		snprintf(sLegacyKey, sizeof(sLegacyKey),
			"%s/certs/legacy.example.com/key.pem", STORE_ROOT);
		testRequire(xrtFileWriteAtomic(sLegacyKey,
			(xbytesview){ (const uint8*)sPem, strlen(sPem) }),
			"acme store legacy key fixture failed");
		testRequire(xrtAcmeStoreLoadGrant(STORE_ROOT,
			"legacy.example.com", &Legacy) &&
			strcmp(Legacy.sFullchainPem, sPem) == 0 &&
			strcmp(Legacy.sKeyPem, sPem) == 0,
			"acme store legacy grant fallback failed");
		xrtAcmeGrantUnit(&Legacy);
	}

	/* 域名枚举：grant.example.com 已登记。 */
	{
		char sDomains[8][256];
		char sJunk[400];
		size_t iCount = 0u;
		size_t i;
		bool bHas = false;
		snprintf(sJunk, sizeof(sJunk), "%s/certs/.invalid", STORE_ROOT);
		testRequire(xrtDirCreateAll(sJunk),
			"acme store junk directory fixture failed");
		snprintf(sJunk, sizeof(sJunk), "%s/certs/file.example", STORE_ROOT);
		{
			FILE* pJunk = fopen(sJunk, "wb");
			testRequire(pJunk != NULL && fclose(pJunk) == 0,
				"acme store junk file fixture failed");
		}
		testRequire(
			xrtAcmeStoreListDomains(STORE_ROOT, sDomains, 8u, &iCount),
			"acme store list failed"
		);
		for(i = 0; i < iCount; i++)
		{
			if(strcmp(sDomains[i], "grant.example.com") == 0)
			{
				bHas = true;
			}
			testRequire(strcmp(sDomains[i], ".invalid") != 0 &&
				strcmp(sDomains[i], "file.example") != 0,
				"acme store list must skip non-domain or non-directory entries");
		}
		testRequire(bHas, "acme store list missing grant domain");
		xrtClearError();
		testRequire(
			!xrtAcmeStoreListDomains(STORE_ROOT, sDomains, 1u, &iCount) ||
				(iCount <= 1u),
			"acme store list capacity mismatch"
		);
	}

	/* 错误语义。 */
	xrtClearError();
	testRequire(
		!xrtAcmeStoreSaveCert(STORE_ROOT, "../../outside", sPem, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme store rejects traversal in domain"
	);
	xrtClearError();
	testRequire(
		xrtAcmeStoreLoadCert(STORE_ROOT, "a/b.example") == NULL &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme store rejects path separators in domain"
	);
	{
		char sLongRoot[1100];
		memset(sLongRoot, 'a', sizeof(sLongRoot) - 1u);
		sLongRoot[sizeof(sLongRoot) - 1u] = 0;
		xrtClearError();
		testRequire(
			!xrtAcmeStoreSaveCert(sLongRoot, "valid.example", sPem, NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"acme store rejects truncated path"
		);
	}
	xrtClearError();
	testRequire(
		!xrtAcmeStoreSaveAccount(NULL, NULL, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme store null argument mismatch"
	);

	printf("[PASS] acme store roundtrip renew\n");
	return 0;
}
