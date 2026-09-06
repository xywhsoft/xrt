/*
 * 范例：containers/buffer_tour —— 连续字节缓冲编辑/接管族补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【容量】    xrtBufferReserve / Resize / Trim / Clear
 *   【编辑】    xrtBufferAdd（尾部扩展返回直写指针）/
 *              InsertSpace / Insert / Remove / AppendByte / Assign
 *   【接管族】  xrtBufferSetTake（接管内存替换内容）/
 *              CreateTake（创建并接管来源槽）/ From（复制创建）
 * 模块宏：XRT_MODULE_BUFFER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/buffer_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   buffer: reserve/resize/trim size=2
 *   buffer: edit add+insert+remove+append assign=9
 *   buffer: take set-take=6 create-take=3 from=2
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define BV(x) ((xbytesview) { (cbytes)(x), sizeof(x) - 1u })

int main(void)
{
	xbuffer* pBuffer = NULL;
	xbuffer* pTaken = NULL;
	xbuffer* pFrom = NULL;
	bytes pData = NULL;
	bytes pSlot = NULL;
	bytes pWrite = NULL;
	int iResult = 1;

	/* ---- 容量族：Reserve / Resize / Trim ---- */
	pBuffer = xrtBufferCreate();
	if ( (pBuffer == NULL) ||
		!xrtBufferReserve(pBuffer, 16u) ||
		!xrtBufferResize(pBuffer, 4u) ||
		(xrtBufferView(pBuffer).Size != 4u) ||
		!xrtBufferTrim(pBuffer) ||
		(xrtBufferView(pBuffer).Size != 4u) ) {
		goto Cleanup;
	}
	/* Resize 扩张填零 + 收缩截断。 */
	if ( !xrtBufferResize(pBuffer, 2u) ||
		(xrtBufferView(pBuffer).Size != 2u) ) {
		goto Cleanup;
	}
	printf("buffer: reserve/resize/trim size=%zu\n",
		xrtBufferView(pBuffer).Size);

	/* ---- 编辑族 ---- */
	/* Assign：整体替换内容。 */
	if ( !xrtBufferAssign(pBuffer, BV("abcdef")) ||
		(xrtBufferView(pBuffer).Size != 6u) ||
		(memcmp(xrtBufferView(pBuffer).Data, "abcdef",
			6u) != 0) ) {
		goto Cleanup;
	}
	/* Insert：中段插入 "XY" → abXYcdef。 */
	if ( !xrtBufferInsert(pBuffer, 2u, BV("XY")) ||
		(xrtBufferView(pBuffer).Size != 8u) ||
		(memcmp(xrtBufferView(pBuffer).Data, "abXYcdef",
			8u) != 0) ) {
		goto Cleanup;
	}
	/* Remove：删 "XY" → abcdef。 */
	if ( !xrtBufferRemove(pBuffer, 2u, 2u) ||
		(xrtBufferView(pBuffer).Size != 6u) ||
		(memcmp(xrtBufferView(pBuffer).Data, "abcdef",
			6u) != 0) ) {
		goto Cleanup;
	}
	/* Add：尾部扩展 2 字节直写 "gh"。 */
	pWrite = xrtBufferAdd(pBuffer, 2u);
	if ( (pWrite == NULL) ||
		(xrtBufferView(pBuffer).Size != 8u) ) {
		goto Cleanup;
	}
	memcpy(pWrite, "gh", 2u);
	/* AppendByte：单字节追加。 */
	if ( !xrtBufferAppendByte(pBuffer, '!') ||
		(xrtBufferView(pBuffer).Size != 9u) ||
		(memcmp(xrtBufferView(pBuffer).Data, "abcdefgh!",
			9u) != 0) ) {
		goto Cleanup;
	}
	/* InsertSpace：中段腾 3 个未初始化槽（不零填充），
	 * 返回首地址由调用方写入 → 写 "ZZZ" → abZZZcdefgh!。 */
	{
		bytes pSpace = xrtBufferInsertSpace(pBuffer, 2u, 3u);

		if ( (pSpace == NULL) ||
			(xrtBufferView(pBuffer).Size != 12u) ) {
			goto Cleanup;
		}
		memcpy(pSpace, "ZZZ", 3u);
		if ( memcmp(xrtBufferView(pBuffer).Data, "abZZZcdefgh!",
				12u) != 0 ) {
			goto Cleanup;
		}
	}
	printf("buffer: edit add+insert+remove+append assign=9\n");

	/* ---- Clear：清空有效内容，容量保留。 ---- */
	xrtBufferClear(pBuffer);
	if ( xrtBufferView(pBuffer).Size != 0u ) {
		goto Cleanup;
	}

	/* ---- 接管族 ---- */
	/* SetTake：接管 xrtMalloc 内存替换缓冲内容。 */
	pData = (bytes)xrtMalloc(6);
	if ( pData == NULL ) {
		goto Cleanup;
	}
	memcpy(pData, "takeit", 6u);
	pSlot = pData;
	if ( !xrtBufferSetTake(pBuffer, &pSlot, 6u, 6u) ||
		(pSlot != NULL) ||  /* 来源槽被清空 */
		(xrtBufferView(pBuffer).Size != 6u) ||
		(memcmp(xrtBufferView(pBuffer).Data, "takeit",
			6u) != 0) ) {
		goto Cleanup;
	}
	pData = NULL;
	/* CreateTake：创建新缓冲并接管。 */
	pData = (bytes)xrtMalloc(3);
	if ( pData == NULL ) {
		goto Cleanup;
	}
	memcpy(pData, "own", 3u);
	pSlot = pData;
	pTaken = xrtBufferCreateTake(&pSlot, 3u, 3u);
	if ( (pTaken == NULL) ||
		(pSlot != NULL) ||
		(xrtBufferView(pTaken).Size != 3u) ||
		(memcmp(xrtBufferView(pTaken).Data, "own",
			3u) != 0) ) {
		goto Cleanup;
	}
	pData = NULL;
	/* From：复制创建（来源不受影响）。 */
	pFrom = xrtBufferFrom(BV("cp"));
	if ( (pFrom == NULL) ||
		(xrtBufferView(pFrom).Size != 2u) ||
		(memcmp(xrtBufferView(pFrom).Data, "cp", 2u) != 0) ) {
		goto Cleanup;
	}
	printf("buffer: take set-take=6 create-take=3 from=2\n");
	iResult = 0;

Cleanup:
	xrtFree(pData);
	xrtBufferDestroy(pFrom);
	xrtBufferDestroy(pTaken);
	xrtBufferDestroy(pBuffer);
	return iResult;
}
