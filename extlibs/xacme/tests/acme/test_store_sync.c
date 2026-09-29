#if defined(__linux__)
	#define _GNU_SOURCE
#endif

#include "../test.h"

#include <xrt/acme_store.h>

#include <string.h>

#if defined(__linux__)
	#include <errno.h>
	#include <sys/stat.h>
	#include <sys/syscall.h>
	#include <unistd.h>

static int gDirectorySyncCalls;
static int gFailDirectorySyncAt;

/* 只截断目录 fsync；普通文件刷新仍走真实系统调用。 */
int fsync(int iFd)
{
	struct stat Info;
	if(fstat(iFd, &Info) == 0 && S_ISDIR(Info.st_mode))
	{
		gDirectorySyncCalls++;
		if(gDirectorySyncCalls == gFailDirectorySyncAt)
		{
			errno = EIO;
			return -1;
		}
	}
	return (int)syscall(SYS_fsync, iFd);
}
#endif

static void testStoredKey(cstr sRoot, cstr sExpected)
{
	xacmeissuegrant Loaded;
	testRequire(xrtAcmeStoreLoadGrant(sRoot, "sync.example.com", &Loaded) &&
		strcmp(Loaded.sKeyPem, sExpected) == 0,
		"acme store sync failure changed the wrong grant");
	xrtAcmeGrantUnit(&Loaded);
}

static void testStoredAccount(cstr sRoot, cstr sExpected)
{
	str sLoaded = xrtAcmeStoreLoadAccount(
		sRoot, "https://ca.example/directory");
	testRequire(sLoaded != NULL && strcmp(sLoaded, sExpected) == 0,
		"acme store sync failure changed the wrong account");
	xrtFree(sLoaded);
}

int main(void)
{
	char sRoot[340];
	xacmeissuegrant Old = { (str)"old chain\n", (str)"old key\n" };
	snprintf(sRoot, sizeof(sRoot), "%s/store_sync", testOutRoot());
	testRequire(xrtAcmeStoreSaveGrant(sRoot, "sync.example.com", &Old,
		"https://ca.example/directory"),
		"acme store sync baseline save failed");
	testStoredKey(sRoot, Old.sKeyPem);

	#if defined(__linux__)
		{
			xacmeissuegrant New = {
				(str)"new chain\n", (str)"new key\n"
			};
			int iSyncsPerCommit = gDirectorySyncCalls;
			testRequire(iSyncsPerCommit >= 3,
				"acme store did not sync generation and parent directories");
			gDirectorySyncCalls = 0;
			gFailDirectorySyncAt = 1;
			xrtClearError();
			testRequire(!xrtAcmeStoreSaveGrant(sRoot,
				"sync.example.com", &New, NULL) &&
				xrtErrorKind(xrtGetError()) == XERR_IO &&
				gDirectorySyncCalls == 1,
				"acme store ignored precommit directory sync failure");
			gFailDirectorySyncAt = 0;
			testStoredKey(sRoot, Old.sKeyPem);

			gDirectorySyncCalls = 0;
			gFailDirectorySyncAt = iSyncsPerCommit;
			xrtClearError();
			testRequire(!xrtAcmeStoreSaveGrant(sRoot,
				"sync.example.com", &New, NULL) &&
				xrtErrorKind(xrtGetError()) == XERR_IO &&
				gDirectorySyncCalls == iSyncsPerCommit,
				"acme store ignored postcommit directory sync failure");
			gFailDirectorySyncAt = 0;
			testStoredKey(sRoot, New.sKeyPem);
		}
		gDirectorySyncCalls = 0;
	#endif
	testRequire(xrtAcmeStoreSaveAccount(sRoot,
		"https://ca.example/directory", "old account\n"),
		"acme store sync baseline account save failed");
	testStoredAccount(sRoot, "old account\n");
	#if defined(__linux__)
		{
			int iSyncsPerCommit = gDirectorySyncCalls;
			testRequire(iSyncsPerCommit >= 2,
				"acme store account directory was not synced");
			gDirectorySyncCalls = 0;
			gFailDirectorySyncAt = 1;
			xrtClearError();
			testRequire(!xrtAcmeStoreSaveAccount(sRoot,
				"https://ca.example/directory", "new account\n") &&
				xrtErrorKind(xrtGetError()) == XERR_IO,
				"acme store ignored precommit account directory sync failure");
			gFailDirectorySyncAt = 0;
			testStoredAccount(sRoot, "old account\n");

			gDirectorySyncCalls = 0;
			gFailDirectorySyncAt = iSyncsPerCommit;
			xrtClearError();
			testRequire(!xrtAcmeStoreSaveAccount(sRoot,
				"https://ca.example/directory", "new account\n") &&
				xrtErrorKind(xrtGetError()) == XERR_IO &&
				gDirectorySyncCalls == iSyncsPerCommit,
				"acme store ignored postcommit account directory sync failure");
			gFailDirectorySyncAt = 0;
			testStoredAccount(sRoot, "new account\n");
		}
	#endif

	printf("[PASS] acme store directory sync fault paths\n");
	return 0;
}
