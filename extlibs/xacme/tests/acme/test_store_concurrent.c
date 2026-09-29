#include "../test.h"
#include "../../../../tests/test_thread.h"

#include <xrt/acme_store.h>

#include <stdatomic.h>
#include <string.h>

typedef struct storewriter {
	cstr sRoot;
	cstr sChain;
	cstr sKey;
	atomic_int* pStart;
	atomic_int* pFinished;
	int iSuccess;
	int iFailure;
	int iLastError;
	int iLastSystemCode;
	char sLastOperation[48];
} storewriter;

static int testStoreWriter(ptr pData)
{
	storewriter* pWriter = (storewriter*)pData;
	xacmeissuegrant Grant;
	int i;
	Grant.sFullchainPem = (str)pWriter->sChain;
	Grant.sKeyPem = (str)pWriter->sKey;
	while(atomic_load(pWriter->pStart) == 0)
		testThreadYield();
	for(i = 0; i < 12; i++)
	{
		if(xrtAcmeStoreSaveGrant(pWriter->sRoot,
			"concurrent.example.com", &Grant, NULL))
			pWriter->iSuccess++;
		else
		{
			pWriter->iFailure++;
			pWriter->iLastError = (int)xrtErrorKind(xrtGetError());
			pWriter->iLastSystemCode =
				(int)xrtErrorSystemCode(xrtGetError());
			snprintf(pWriter->sLastOperation,
				sizeof(pWriter->sLastOperation), "%s",
				xrtErrorOperation(xrtGetError()) != NULL ?
					xrtErrorOperation(xrtGetError()) : "");
		}
	}
	(void)atomic_fetch_add(pWriter->pFinished, 1);
	return 0;
}

static bool testStorePairValid(const xacmeissuegrant* pGrant)
{
	return (strcmp(pGrant->sFullchainPem, "initial chain\n") == 0 &&
		strcmp(pGrant->sKeyPem, "initial key\n") == 0) ||
		(strcmp(pGrant->sFullchainPem, "A chain\n") == 0 &&
		strcmp(pGrant->sKeyPem, "A key\n") == 0) ||
		(strcmp(pGrant->sFullchainPem, "B chain\n") == 0 &&
		strcmp(pGrant->sKeyPem, "B key\n") == 0);
}

int main(void)
{
	char sRoot[340];
	xacmeissuegrant Initial = {
		(str)"initial chain\n", (str)"initial key\n"
	};
	atomic_int iStart = 0;
	atomic_int iFinished = 0;
	storewriter aWriters[2] = {
		{ sRoot, "A chain\n", "A key\n", &iStart, &iFinished,
			0, 0, 0, 0, { 0 } },
		{ sRoot, "B chain\n", "B key\n", &iStart, &iFinished,
			0, 0, 0, 0, { 0 } }
	};
	testthread aThreads[2] = { { 0 }, { 0 } };
	int iReads = 0;
	int iReadFailures = 0;
	int iMixed = 0;
	int iLastReadError = 0;
	int iLastReadSystemCode = 0;
	char sLastReadOperation[48] = { 0 };
	snprintf(sRoot, sizeof(sRoot), "%s/store_concurrent", testOutRoot());
	aThreads[0].Proc = testStoreWriter;
	aThreads[0].Data = &aWriters[0];
	aThreads[1].Proc = testStoreWriter;
	aThreads[1].Data = &aWriters[1];
	testRequire(xrtAcmeStoreSaveGrant(sRoot,
		"concurrent.example.com", &Initial, NULL),
		"acme store concurrent baseline save failed");
	testThreadsStart(aThreads, 2u);
	atomic_store(&iStart, 1);
	while((atomic_load(&iFinished) < 2 || iReads < 50) &&
		iReads < 5000)
	{
		xacmeissuegrant Loaded;
		if(xrtAcmeStoreLoadGrant(sRoot,
			"concurrent.example.com", &Loaded))
		{
			if(!testStorePairValid(&Loaded))
				iMixed++;
			xrtAcmeGrantUnit(&Loaded);
		}
		else
		{
			iReadFailures++;
			iLastReadError = (int)xrtErrorKind(xrtGetError());
			iLastReadSystemCode =
				(int)xrtErrorSystemCode(xrtGetError());
			snprintf(sLastReadOperation, sizeof(sLastReadOperation), "%s",
				xrtErrorOperation(xrtGetError()) != NULL ?
					xrtErrorOperation(xrtGetError()) : "");
		}
		iReads++;
		testThreadYield();
	}
	testThreadsJoin(aThreads, 2u);
	if(iReadFailures != 0 || iMixed != 0 ||
		aWriters[0].iFailure != 0 || aWriters[1].iFailure != 0)
		printf("[diag] reads=%d read_failures=%d last=%d/%d/%s mixed=%d "
			"writer_a=%d/%d/%d/%d/%s writer_b=%d/%d/%d/%d/%s\n",
			iReads, iReadFailures, iLastReadError,
			iLastReadSystemCode, sLastReadOperation, iMixed,
			aWriters[0].iSuccess, aWriters[0].iFailure,
			aWriters[0].iLastError, aWriters[0].iLastSystemCode,
			aWriters[0].sLastOperation,
			aWriters[1].iSuccess, aWriters[1].iFailure,
			aWriters[1].iLastError, aWriters[1].iLastSystemCode,
			aWriters[1].sLastOperation);
	testRequire(iReads >= 50 && iReadFailures == 0 && iMixed == 0,
		"acme store reader saw missing or mixed concurrent grant");
	testRequire(aThreads[0].Result == 0 && aThreads[1].Result == 0 &&
		aWriters[0].iSuccess > 0 && aWriters[1].iSuccess > 0 &&
		aWriters[0].iFailure == 0 && aWriters[1].iFailure == 0,
		"acme store concurrent writers failed without injected faults");
	printf("[PASS] acme store concurrent rotation reads=%d\n", iReads);
	return 0;
}
