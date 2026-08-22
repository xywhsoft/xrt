#include "../test.h"



#define TEST_TLS_RESUME_OOM_SERVER_NAME_SIZE 1024u



typedef struct test_tls_resume_alloc {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_tls_resume_alloc;



/* 在指定调用失败，并记录恢复对象单块分配的尺寸。 */
static ptr testTlsResumeAlloc(ptr pContext, size_t iSize)
{
	test_tls_resume_alloc* pState = (test_tls_resume_alloc*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配保持故障点和存活块统计一致。 */
static ptr testTlsResumeRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_resume_alloc* pState = (test_tls_resume_alloc*)pContext;
	bool bNew = pMemory == NULL;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && bNew ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放底层存储并核对分配器存活块计数。 */
static void testTlsResumeFree(ptr pContext, ptr pMemory)
{
	test_tls_resume_alloc* pState = (test_tls_resume_alloc*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "TLS resume allocator underflow");
	pState->Live--;
	free(pMemory);
}



/* 初始化只需要一次非池化直接分配的有效恢复配置。 */
static void testTlsResumeOomConfig(
	xtlsresumeconfig* pConfig,
	uint8* pTicket,
	uint8* pSecret,
	cstr sServerName
)
{
	xrtTlsResumeConfigInit(pConfig);
	pConfig->Cipher = XTLS_AES_128_GCM_SHA256;
	pConfig->Ticket = (xbytesview) { pTicket, 8u };
	pConfig->Secret = (xbytesview) { pSecret, 32u };
	pConfig->ServerName = (xstrview) {
		sServerName,
		TEST_TLS_RESUME_OOM_SERVER_NAME_SIZE
	};
	pConfig->Lifetime = 60u;
}



/* 分配失败必须原子返回，成功释放不得增加底层存活块。 */
int main(void)
{
	static test_tls_resume_alloc State = { 0, SIZE_MAX, 0 };
	xallocator Allocator;
	uint8 Ticket[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint8 Secret[32];
	char ServerName[TEST_TLS_RESUME_OOM_SERVER_NAME_SIZE];
	xtlsresumeconfig Config;
	xtlsresume* pResume;
	size_t iHeapBaseline;

	memset(Secret, 0xa5, sizeof(Secret));
	memset(ServerName, 'a', sizeof(ServerName));
	Allocator.Context = &State;
	Allocator.Alloc = testTlsResumeAlloc;
	Allocator.Realloc = testTlsResumeRealloc;
	Allocator.Free = testTlsResumeFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS resume OOM allocator install failed");
	testTlsResumeOomConfig(
		&Config,
		Ticket,
		Secret,
		ServerName
	);

	State.FailAt = State.Calls + 1u;
	pResume = xrtTlsResumeCreate(&Config);
	testRequire((pResume == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"TLS resume unexpectedly survived OOM");
	xrtClearError();

	State.FailAt = SIZE_MAX;
	pResume = xrtTlsResumeCreate(&Config);
	testRequire(pResume != NULL,
		"TLS resume recovery after OOM failed");
	xrtTlsResumeRelease(pResume);
	testMemoryDebugDrain(
		"TLS resume baseline memory debug drain failed");
	iHeapBaseline = State.Live;

	for ( size_t i = 0; i < 64u; i++ ) {
		pResume = xrtTlsResumeCreate(&Config);
		testRequire(pResume != NULL,
			"repeated TLS resume creation failed");
		xrtTlsResumeRelease(pResume);
	}
	testMemoryDebugDrain(
		"TLS resume final memory debug drain failed");
	if ( State.Live != iHeapBaseline ) {
		fprintf(
			stderr,
			"[DIAG] TLS resume baseline=%zu live=%zu calls=%zu\n",
			iHeapBaseline,
			State.Live,
			State.Calls
		);
	}
	testRequire(State.Live == iHeapBaseline,
		"repeated TLS resume release grew backing memory");
	return 0;
}
