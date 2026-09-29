#if defined(__linux__)
	#define _GNU_SOURCE
#endif

#include "../test.h"

#include <xrt/acme_store.h>

#include <string.h>

#if defined(__linux__)
	#include <dirent.h>
	#include <errno.h>
	#include <sys/syscall.h>
	#include <unistd.h>

typedef enum storeiofault {
	STORE_IO_NONE = 0,
	STORE_IO_STAGE_TEXT,
	STORE_IO_STAGE_KEY_PARTIAL,
	STORE_IO_POINTER,
	STORE_IO_ACCOUNT_PARTIAL
} storeiofault;

static storeiofault gFault;
static int gMatchedWrites;

static bool testStoreIoTarget(cstr sPath)
{
	if(gFault == STORE_IO_ACCOUNT_PARTIAL)
		return strstr(sPath, "/accounts/") != NULL &&
			strstr(sPath, "/.xacme-key-") != NULL;
	if(strstr(sPath, "/certs/io.example.com/") == NULL)
		return false;
	if(gFault == STORE_IO_STAGE_TEXT)
		return strstr(sPath, "/.grant-") != NULL &&
			strstr(sPath, "/.xrt-write-") != NULL;
	if(gFault == STORE_IO_STAGE_KEY_PARTIAL)
		return strstr(sPath, "/.grant-") != NULL &&
			strstr(sPath, "/.xacme-key-") != NULL;
	if(gFault == STORE_IO_POINTER)
		return strstr(sPath, "/.grant-") == NULL &&
			strstr(sPath, "/.xrt-write-") != NULL;
	return false;
}

/* 文件描述符定位到库的临时文件；其他 write 均调用真实系统调用。 */
ssize_t write(int iFd, const void* pData, size_t iSize)
{
	if(gFault != STORE_IO_NONE)
	{
		char sLink[64];
		char sPath[1024];
		ssize_t iLength;
		int iWritten = snprintf(sLink, sizeof(sLink),
			"/proc/self/fd/%d", iFd);
		if(iWritten > 0 && (size_t)iWritten < sizeof(sLink))
		{
			iLength = readlink(sLink, sPath, sizeof(sPath) - 1u);
			if(iLength > 0)
			{
				sPath[iLength] = '\0';
				if(testStoreIoTarget(sPath))
				{
					gMatchedWrites++;
					if((gFault == STORE_IO_STAGE_KEY_PARTIAL ||
						gFault == STORE_IO_ACCOUNT_PARTIAL) &&
						gMatchedWrites == 1 && iSize > 1u)
						return (ssize_t)syscall(SYS_write, iFd, pData, 1u);
					errno = EIO;
					return -1;
				}
			}
		}
	}
	return (ssize_t)syscall(SYS_write, iFd, pData, iSize);
}

static int testStoreIoCount(cstr sDirectory, cstr sPrefix)
{
	DIR* pDir = opendir(sDirectory);
	struct dirent* pEntry;
	int iCount = 0;
	if(pDir == NULL)
		return -1;
	while((pEntry = readdir(pDir)) != NULL)
	{
		if(strncmp(pEntry->d_name, sPrefix, strlen(sPrefix)) == 0)
			iCount++;
	}
	(void)closedir(pDir);
	return iCount;
}

static int testStoreIoCountAccountTemps(cstr sAccountsDir)
{
	DIR* pDir = opendir(sAccountsDir);
	struct dirent* pEntry;
	int iCount = 0;
	if(pDir == NULL)
		return -1;
	while((pEntry = readdir(pDir)) != NULL)
	{
		char sPath[450];
		int iWritten;
		int iSubCount;
		if(pEntry->d_name[0] == '.')
			continue;
		iWritten = snprintf(sPath, sizeof(sPath), "%s/%s",
			sAccountsDir, pEntry->d_name);
		if(iWritten < 0 || (size_t)iWritten >= sizeof(sPath))
		{
			iCount = -1;
			break;
		}
		iSubCount = testStoreIoCount(sPath, ".xacme-key-");
		if(iSubCount < 0)
		{
			iCount = -1;
			break;
		}
		iCount += iSubCount;
	}
	(void)closedir(pDir);
	return iCount;
}

static void testStoreIoGrant(cstr sRoot, cstr sChain, cstr sKey)
{
	xacmeissuegrant Loaded;
	testRequire(xrtAcmeStoreLoadGrant(sRoot, "io.example.com", &Loaded) &&
		strcmp(Loaded.sFullchainPem, sChain) == 0 &&
		strcmp(Loaded.sKeyPem, sKey) == 0,
		"acme store write fault changed committed grant");
	xrtAcmeGrantUnit(&Loaded);
}

static void testStoreIoAccount(cstr sRoot, cstr sExpected)
{
	str sLoaded = xrtAcmeStoreLoadAccount(
		sRoot, "https://ca.example/directory");
	testRequire(sLoaded != NULL && strcmp(sLoaded, sExpected) == 0,
		"acme store write fault changed committed account");
	xrtFree(sLoaded);
}
#endif

int main(void)
{
	#if defined(__linux__)
		char sRoot[340];
		char sGrantDir[400];
		char sAccountDir[400];
		xacmeissuegrant Old = {
			(str)"old chain\n", (str)"old key\n"
		};
		xacmeissuegrant New = {
			(str)"new chain\n", (str)"new private key\n"
		};
		const storeiofault aFaults[] = {
			STORE_IO_STAGE_TEXT,
			STORE_IO_STAGE_KEY_PARTIAL,
			STORE_IO_POINTER
		};
		int iGenerations;
		size_t i;
		snprintf(sRoot, sizeof(sRoot), "%s/store_io", testOutRoot());
		snprintf(sGrantDir, sizeof(sGrantDir),
			"%s/certs/io.example.com", sRoot);
		testRequire(xrtAcmeStoreSaveGrant(sRoot,
			"io.example.com", &Old, NULL),
			"acme store write fault baseline grant failed");
		iGenerations = testStoreIoCount(sGrantDir, ".grant-");
		testRequire(iGenerations > 0,
			"acme store write fault baseline generation missing");
		for(i = 0u; i < sizeof(aFaults) / sizeof(aFaults[0]); i++)
		{
			bool bSaved;
			gMatchedWrites = 0;
			gFault = aFaults[i];
			xrtClearError();
			bSaved = xrtAcmeStoreSaveGrant(sRoot,
				"io.example.com", &New, NULL);
			gFault = STORE_IO_NONE;
			testRequire(!bSaved && gMatchedWrites >= 1 &&
				xrtErrorKind(xrtGetError()) == XERR_IO,
				"acme store ignored file write failure");
			testStoreIoGrant(sRoot, Old.sFullchainPem, Old.sKeyPem);
			testRequire(testStoreIoCount(sGrantDir, ".grant-") ==
				iGenerations,
				"acme store left uncommitted generation after write failure");
		}
		testRequire(xrtAcmeStoreSaveAccount(sRoot,
			"https://ca.example/directory", "old account\n"),
			"acme store write fault baseline account failed");
		snprintf(sAccountDir, sizeof(sAccountDir), "%s/accounts", sRoot);
		gMatchedWrites = 0;
		gFault = STORE_IO_ACCOUNT_PARTIAL;
		xrtClearError();
		{
			bool bSaved = xrtAcmeStoreSaveAccount(sRoot,
				"https://ca.example/directory", "new account key\n");
			gFault = STORE_IO_NONE;
			testRequire(!bSaved && gMatchedWrites >= 2 &&
				xrtErrorKind(xrtGetError()) == XERR_IO,
				"acme store ignored partial account write failure");
		}
		testStoreIoAccount(sRoot, "old account\n");
		testRequire(testStoreIoCountAccountTemps(sAccountDir) == 0,
			"acme store left partial private-key temp file");
		printf("[PASS] acme store file write fault paths\n");
	#else
		char sRoot[340];
		xacmeissuegrant Grant = {
			(str)"windows chain\n", (str)"windows key\n"
		};
		xacmeissuegrant Loaded;
		snprintf(sRoot, sizeof(sRoot), "%s/store_io", testOutRoot());
		testRequire(xrtAcmeStoreSaveGrant(sRoot,
			"io.example.com", &Grant, NULL),
			"acme store write test baseline grant failed");
		testRequire(xrtAcmeStoreLoadGrant(sRoot,
			"io.example.com", &Loaded) &&
			strcmp(Loaded.sFullchainPem, Grant.sFullchainPem) == 0 &&
			strcmp(Loaded.sKeyPem, Grant.sKeyPem) == 0,
			"acme store write test baseline grant mismatch");
		xrtAcmeGrantUnit(&Loaded);
		printf("[PASS] acme store write baseline (fault injection Linux-only)\n");
	#endif
	return 0;
}
