#include "../test.h"



/* 从任意地址和长度构造测试字节视图。 */
static xbytesview testBufferBytes(const void* pData, size_t iSize)
{
	return (xbytesview){ (cbytes)pData, iSize };
}



/* 验证缓冲内容与预期字节完全一致。 */
static void testBufferContent(
	const xbuffer* pBuffer,
	const void* pExpected,
	size_t iSize,
	cstr sMessage
)
{
	xbytesview View = xrtBufferView(pBuffer);

	testRequire(View.Size == iSize, sMessage);
	testRequire(
		(iSize == 0) || (memcmp(View.Data, pExpected, iSize) == 0),
		sMessage
	);
}



/* 验证生命周期、容量和未初始化空间的底层路径。 */
static void testBufferLifecycle(void)
{
	xbuffer tBuffer;
	xbuffer* pCreated;
	bytes pSpace;
	size_t iCapacity;

	testRequire(xrtBufferInit(&tBuffer), "buffer init failed");
	testRequire(
		(tBuffer.Data == NULL) && (tBuffer.Size == 0) &&
		(tBuffer.Capacity == 0),
		"buffer initial state mismatch"
	);
	testRequire(xrtBufferReserve(&tBuffer, 4), "buffer reserve failed");
	testRequire(
		(tBuffer.Capacity >= 4) && (tBuffer.Capacity < 256),
		"small buffer growth is excessive"
	);
	iCapacity = tBuffer.Capacity;
	pSpace = xrtBufferAdd(&tBuffer, 3);
	testRequire(pSpace != NULL, "buffer add failed");
	memcpy(pSpace, "abc", 3);
	pSpace = xrtBufferInsertSpace(&tBuffer, 1, 2);
	testRequire(pSpace != NULL, "buffer insert space failed");
	memcpy(pSpace, "XY", 2);
	testBufferContent(&tBuffer, "aXYbc", 5, "buffer space content mismatch");

	xrtBufferClear(&tBuffer);
	testRequire(
		(tBuffer.Size == 0) && (tBuffer.Capacity >= iCapacity),
		"buffer clear released capacity"
	);
	testRequire(xrtBufferTrim(&tBuffer), "empty buffer trim failed");
	testRequire(
		(tBuffer.Data == NULL) && (tBuffer.Capacity == 0),
		"empty buffer trim did not release storage"
	);
	xrtBufferUnit(&tBuffer);

	pCreated = xrtBufferCreate();
	testRequire(pCreated != NULL, "buffer create failed");
	testRequire(
		xrtBufferAppendByte(pCreated, UINT8_C(0x5a)),
		"created buffer append byte failed"
	);
	testRequire(
		(pCreated->Size == 1) && (pCreated->Data[0] == UINT8_C(0x5a)),
		"created buffer byte mismatch"
	);
	xrtBufferDestroy(pCreated);
}



/* 验证旧版已有的中间插入、自引用插入和后缀保留合同。 */
static void testBufferCopyOperations(void)
{
	xbuffer tBuffer;
	xbytesview Alias;
	bytes pInactive;

	testRequire(xrtBufferInit(&tBuffer), "copy buffer init failed");
	testRequire(
		xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("ad")),
		"buffer initial append failed"
	);
	testRequire(
		xrtBufferInsert(&tBuffer, 1, XRT_BYTES_LITERAL("bc")),
		"buffer middle insert failed"
	);
	testBufferContent(&tBuffer, "abcd", 4, "middle insert lost suffix");

	Alias = testBufferBytes(tBuffer.Data + 2, 2);
	testRequire(
		xrtBufferInsert(&tBuffer, 0, Alias),
		"buffer self insert failed"
	);
	testBufferContent(&tBuffer, "cdabcd", 6, "buffer self insert mismatch");
	Alias = testBufferBytes(tBuffer.Data + 1, 4);
	testRequire(
		xrtBufferAppend(&tBuffer, Alias),
		"buffer self append failed"
	);
	testBufferContent(
		&tBuffer,
		"cdabcddabc",
		10,
		"buffer self append mismatch"
	);

	testRequire(
		xrtBufferAssign(&tBuffer, testBufferBytes(tBuffer.Data + 2, 5)),
		"buffer alias assign failed"
	);
	testBufferContent(&tBuffer, "abcdd", 5, "buffer alias assign mismatch");
	testRequire(
		xrtBufferRemove(&tBuffer, 1, 2),
		"buffer remove failed"
	);
	testBufferContent(&tBuffer, "add", 3, "buffer remove mismatch");

	/* 保留容量不属于有效内容，不能作为复制来源。 */
	testRequire(
		xrtBufferReserve(&tBuffer, tBuffer.Size + 16u),
		"buffer inactive source reserve failed"
	);
	pInactive = tBuffer.Data + tBuffer.Size;
	xrtClearError();
	testRequire(
		!xrtBufferAppend(&tBuffer, testBufferBytes(pInactive, 1)),
		"inactive buffer source should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"inactive buffer source error mismatch"
	);
	testBufferContent(&tBuffer, "add", 3, "invalid source changed buffer");
	xrtBufferUnit(&tBuffer);
}



/* 验证覆盖、稀疏扩展、零填充和自引用写入。 */
static void testBufferWrite(void)
{
	xbuffer tBuffer;
	xbytesview Alias;
	const unsigned char pSparse[] = {
		'a', 'X', 'c', 0, 0, 'z'
	};
	const unsigned char pAliased[] = {
		'a', 'X', 'c', 0, 0, 'z', 0, 0, 'a', 'X', 'c'
	};
	size_t iSize;
	size_t iCapacity;
	bytes pData;

	testRequire(xrtBufferInit(&tBuffer), "write buffer init failed");
	testRequire(
		xrtBufferAssign(&tBuffer, XRT_BYTES_LITERAL("abc")),
		"write buffer assign failed"
	);
	testRequire(
		xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z")),
		"sparse buffer write failed"
	);
	testRequire(
		xrtBufferWrite(&tBuffer, 1, XRT_BYTES_LITERAL("X")),
		"overwrite buffer write failed"
	);
	testBufferContent(&tBuffer, pSparse, sizeof(pSparse), "sparse write mismatch");

	Alias = testBufferBytes(tBuffer.Data, 3);
	testRequire(
		xrtBufferWrite(&tBuffer, 8, Alias),
		"growing alias write failed"
	);
	testBufferContent(
		&tBuffer,
		pAliased,
		sizeof(pAliased),
		"growing alias write mismatch"
	);

	pData = tBuffer.Data;
	iSize = tBuffer.Size;
	iCapacity = tBuffer.Capacity;
	xrtClearError();
	testRequire(
		!xrtBufferWrite(
			&tBuffer,
			SIZE_MAX,
			testBufferBytes("xx", 2)
		),
		"overflowing buffer write should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"overflowing buffer write error mismatch"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity),
		"overflowing buffer write changed state"
	);
	testRequire(
		xrtBufferWrite(&tBuffer, SIZE_MAX, testBufferBytes(NULL, 0)),
		"empty buffer write should be a no-op"
	);
	xrtBufferUnit(&tBuffer);
}



/* 验证复制构造和显式所有权转移不会产生额外数据副本。 */
static void testBufferOwnership(void)
{
	xbuffer tBuffer;
	xbuffer* pCopy;
	xbuffer* pTaken;
	bytes pOwned;
	bytes pResult;
	size_t iSize;
	size_t iCapacity;

	testRequire(xrtBufferInit(&tBuffer), "ownership buffer init failed");
	testRequire(
		xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("old")),
		"ownership buffer setup failed"
	);
	pOwned = (bytes)xrtMalloc(8);
	testRequire(pOwned != NULL, "owned buffer allocation failed");
	memcpy(pOwned, "xyz", 3);
	pResult = pOwned;
	testRequire(
		xrtBufferSetTake(&tBuffer, &pOwned, 3, 8),
		"buffer set take failed"
	);
	testRequire(pOwned == NULL, "buffer set take did not clear source slot");
	testRequire(tBuffer.Data == pResult, "buffer set take copied data");
	testBufferContent(&tBuffer, "xyz", 3, "buffer set take content mismatch");

	pResult = xrtBufferTake(&tBuffer, &iSize, &iCapacity);
	testRequire(
		(pResult != NULL) && (iSize == 3) && (iCapacity == 8),
		"buffer take metadata mismatch"
	);
	testRequire(
		(tBuffer.Data == NULL) && (tBuffer.Size == 0) &&
		(tBuffer.Capacity == 0),
		"buffer take did not reset state"
	);
	xrtFree(pResult);

	pCopy = xrtBufferFrom(XRT_BYTES_LITERAL("copy"));
	testRequire(pCopy != NULL, "buffer copy construction failed");
	testBufferContent(pCopy, "copy", 4, "buffer copy construction mismatch");
	xrtBufferDestroy(pCopy);

	pOwned = (bytes)xrtMalloc(4);
	testRequire(pOwned != NULL, "create take allocation failed");
	memcpy(pOwned, "take", 4);
	pResult = pOwned;
	pTaken = xrtBufferCreateTake(&pOwned, 4, 4);
	testRequire(pTaken != NULL, "buffer create take failed");
	testRequire(
		(pOwned == NULL) && (pTaken->Data == pResult),
		"buffer create take ownership mismatch"
	);
	xrtBufferDestroy(pTaken);
	xrtBufferUnit(&tBuffer);
}



/* 验证范围、状态、视图和所有权槽错误都保持原状态。 */
static void testBufferInvalidInputs(void)
{
	xbuffer tBuffer;
	bytes pData;
	size_t iSize;
	size_t iCapacity;
	size_t* pInside;

	testRequire(xrtBufferInit(&tBuffer), "invalid buffer init failed");
	testRequire(
		xrtBufferAssign(&tBuffer, XRT_BYTES_LITERAL("abcdef")),
		"invalid buffer setup failed"
	);

	xrtClearError();
	testRequire(
		!xrtBufferAppend(&tBuffer, testBufferBytes(NULL, 1)),
		"invalid non-empty null view should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid view error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtBufferInsert(&tBuffer, tBuffer.Size + 1u, XRT_BYTES_LITERAL("x")) == false,
		"out-of-range insert should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"insert range error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtBufferRemove(&tBuffer, 1, tBuffer.Size),
		"oversized remove should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"remove range error mismatch"
	);
	xrtClearError();
	testRequire(xrtBufferAdd(&tBuffer, 0) == NULL, "zero add should fail");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero add error mismatch"
	);

	/* 元数据输出不能覆盖即将交给调用方的字节。 */
	pInside = (size_t*)tBuffer.Data;
	pData = tBuffer.Data;
	iSize = tBuffer.Size;
	iCapacity = tBuffer.Capacity;
	xrtClearError();
	testRequire(
		xrtBufferTake(&tBuffer, pInside, NULL) == NULL,
		"aliased take output should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"aliased take output error mismatch"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity),
		"aliased take output changed buffer"
	);
	xrtClearError();
	testRequire(
		xrtBufferTake(&tBuffer, &tBuffer.Size, NULL) == NULL,
		"buffer field take output should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"buffer field take output error mismatch"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity),
		"buffer field take output changed buffer"
	);

	/* 当前持有的内存不能再次作为外部所有权接管，避免同一块内存被释放两次。 */
	pData = tBuffer.Data;
	xrtClearError();
	testRequire(
		!xrtBufferSetTake(
			&tBuffer,
			&pData,
			tBuffer.Size,
			tBuffer.Capacity
		),
		"buffer should reject taking its current allocation"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"current allocation take error mismatch"
	);
	testRequire(
		(pData == tBuffer.Data) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity),
		"current allocation take changed ownership"
	);

	/* 公开结构损坏时必须在进入地址运算前拒绝。 */
	tBuffer.Size = tBuffer.Capacity + 1u;
	xrtClearError();
	testRequire(
		!xrtBufferReserve(&tBuffer, tBuffer.Capacity),
		"invalid buffer state should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"invalid buffer state error mismatch"
	);
	tBuffer.Size = iSize;
	xrtBufferUnit(&tBuffer);
}



/* 运行连续字节缓冲完整合同测试。 */
int main(void)
{
	testBufferLifecycle();
	testBufferCopyOperations();
	testBufferWrite();
	testBufferOwnership();
	testBufferInvalidInputs();
	printf("[PASS] buffer\n");
	return 0;
}
