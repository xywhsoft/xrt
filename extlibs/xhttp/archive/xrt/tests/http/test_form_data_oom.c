#include "../test.h"



/* 可调失败点分配器扫描 FormData 的拥有型协议路径。 */
typedef struct test_form_data_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_form_data_allocator;



/* 在目标分配序号失败，其余请求交给 C 运行库。 */
static ptr testFormDataAlloc(ptr pContext, size_t iSize)
{
	test_form_data_allocator* pState =
		(test_form_data_allocator*)pContext;
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



/* 重分配失败时保持原块有效并维持存活计数。 */
static ptr testFormDataRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_form_data_allocator* pState =
		(test_form_data_allocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放成功分配的底层内存并维护存活计数。 */
static void testFormDataFree(ptr pContext, ptr pMemory)
{
	test_form_data_allocator* pState =
		(test_form_data_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"FormData OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个故障点下验证修改原子性、封包、读取、解析和释放。 */
static bool testFormDataOomAttempt(void)
{
	xformdataconfig Config;
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xformdata* pForm = NULL;
	xformdata* pClone = NULL;
	xformdata* pParsed = NULL;
	xhttpbody* pEncoded = NULL;
	xhttpbodyreader* pReader = NULL;
	xhttpbodychunk Chunk;
	xformdatapart Part;
	xformdatapart Current;
	xhttpbodystatus Status;
	uint8* pWire = NULL;
	uint64 iLength;
	size_t iOffset = 0;
	size_t i;
	bool bComplete = false;

	xrtFormDataConfigInit(&Config);
	Config.InitialParts = 0;
	Config.MaxParts = 128u;
	pForm = xrtFormDataCreate(&Config);
	if ( pForm == NULL ) {
		goto done;
	}
	for ( i = 0; i < 40u; i++ ) {
		char Name[48];
		char Value[128];
		int iName = snprintf(
			Name, sizeof(Name), "field-%u", (unsigned)i
		);
		int iValue = snprintf(
			Value,
			sizeof(Value),
			"value-%u-abcdefghijklmnopqrstuvwxyz-0123456789",
			(unsigned)i
		);
		size_t iCount = xrtFormDataCount(pForm);
		size_t iMetadata = xrtFormDataMetadata(pForm);

		if ( !xrtFormDataAppendBytes(
			pForm,
			(xstrview){ Name, (size_t)iName },
			(xbytesview){ (cbytes)Value, (size_t)iValue },
			NULL,
			(xstrview){ NULL, 0 }
		) ) {
			testRequire((xrtFormDataCount(pForm) == iCount) &&
				(xrtFormDataMetadata(pForm) == iMetadata),
				"FormData append OOM changed visible state");
			goto done;
		}
	}
	if ( !xrtFormDataAt(pForm, 20u, &Part) ) {
		goto done;
	}
	{
		size_t iCount = xrtFormDataCount(pForm);
		size_t iMetadata = xrtFormDataMetadata(pForm);

		if ( !xrtFormDataSetBody(
			pForm,
			Part.Name,
			Part.Body,
			NULL,
			(xstrview){ NULL, 0 }
		) ) {
			testRequire((xrtFormDataCount(pForm) == iCount) &&
				(xrtFormDataMetadata(pForm) == iMetadata) &&
				xrtFormDataAt(pForm, 20u, &Current) &&
				(Current.Body == Part.Body),
				"FormData alias Set OOM changed visible state");
			goto done;
		}
	}
	pClone = xrtFormDataClone(pForm);
	if ( pClone == NULL ) {
		goto done;
	}
	if ( !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("oom-boundary"), &Boundary
	) ) {
		goto done;
	}
	pEncoded = xrtFormDataBody(pClone, &Boundary);
	if ( pEncoded == NULL ) {
		goto done;
	}
	iLength = xrtHttpBodyLength(pEncoded);
	if ( (iLength == XHTTP_BODY_UNKNOWN) ||
		(iLength > (uint64)SIZE_MAX) ) {
		goto done;
	}
	pWire = (uint8*)xrtMalloc((size_t)iLength);
	if ( pWire == NULL ) {
		goto done;
	}
	pReader = xrtHttpBodyOpen(pEncoded);
	if ( pReader == NULL ) {
		goto done;
	}
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 256u, &Chunk);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		if ( Status != XHTTP_BODY_DATA ) {
			goto done;
		}
		if ( Chunk.Size > ((size_t)iLength - iOffset) ) {
			xrtHttpBodyChunkRelease(&Chunk);
			goto done;
		}
		memcpy(pWire + iOffset, Chunk.Data, Chunk.Size);
		iOffset += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	if ( iOffset != (size_t)iLength ) {
		goto done;
	}
	pParsed = xrtFormDataParse(
		(xbytesview){ pWire, iOffset },
		&Boundary,
		&Config,
		NULL,
		&Error
	);
	if ( (pParsed == NULL) ||
		(xrtFormDataCount(pParsed) != 40u) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFormDataDestroy(pParsed);
	xrtHttpBodyReaderDestroy(pReader);
	xrtFree(pWire);
	xrtHttpBodyDestroy(pEncoded);
	xrtFormDataDestroy(pClone);
	xrtFormDataDestroy(pForm);
	xrtClearError();
	return bComplete;
}



/* 扫描实际分配序号并要求所有失败路径回到同一存活基线。 */
int main(void)
{
	test_form_data_allocator State = { 0, 0, 0 };
	xallocator Allocator = {
		&State,
		testFormDataAlloc,
		testFormDataRealloc,
		testFormDataFree
	};
	size_t iBaseline;
	size_t iMaxCalls;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"FormData OOM allocator install failed");
	testRequire(testFormDataOomAttempt(),
		"FormData OOM warm-up failed");
	iMaxCalls = State.Calls + 1u;
	testMemoryDebugDrain(
		"FormData OOM memory debug warm-up reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= iMaxCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testFormDataOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"FormData OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"FormData OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"FormData OOM sweep missed failure or success paths");
	printf("[PASS] FormData OOM\n");
	return 0;
}
