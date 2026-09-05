/*
 * 范例：file/io_tour —— 句柄 IO 与元数据全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   【打开/读写】 xrtFileOpen（选项结构版）/ xrtRead / xrtWrite
 *                xrtReadAt / xrtWriteAt / xrtWriteAtFull / xrtFlush
 *   【元数据】   xrtFileSize / xrtFileStat / xrtFileFlags / xrtFileNative
 *                xrtFileResize / xrtFileSetSize / xrtFileTouch
 *                xrtFileReadAllLimit（带上限整读）
 *   【区间锁】   xrtFileLockRange / xrtFileUnlockRange
 *   【映射】     xrtFileMapFlush（共享写映射提交）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/file/io_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   pos-io: rd=4 rd-at=4 size=8 flags-readable=1
 *   resized=4 setsize=2 stat-size=2
 *   limit=ok lock=1 map-flush=1
 *
 * 单次语义：Write 后游标在文件尾——读回前必须 Seek 回 0
 *   （Read 成功读 0 字节即 EOF）；Write 允许短写；
 *   At 族不动共享游标。LockRange 解锁必须是完全相同的
 *   字节区间。MapFlush 只对共享写映射有意义。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const char sPath[] = "xrt-io-tour.tmp";
	xfileoptions Options;
	xfileinfo Info;
	xfile File;
	xfilemap Map;
	char Buffer[16];
	size_t iDone = 0;
	uint64 iSize = 0;
	uint32 iFlags = 0;
	int iResult = 1;

	(void)xrtFileDelete(sPath);
	xrtClearError();

	/* FileOpen：选项结构版（与 xrtOpen 的标志版等价）。 */
	(void)xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtFileOpen(sPath, &Options);
	if ( File == NULL ) {
		return 1;
	}

	/* 单次读写：Write 可能短写；ReadAt 绝对偏移不动游标。 */
	if ( !xrtWrite(File, "abcd", 4u, &iDone) || (iDone != 4u) ) {
		goto cleanup;
	}
	if ( !xrtFlush(File) ) {
		goto cleanup;
	}
	if ( !xrtSeek(File, 0u, XSEEK_START, NULL) ) {
		goto cleanup;
	}
	iDone = 0;
	if ( !xrtRead(File, Buffer, sizeof(Buffer), &iDone) || (iDone != 4u) ) {
		goto cleanup;
	}
	iDone = 0;
	if ( !xrtReadAt(File, 0u, Buffer, 4u, &iDone) || (iDone != 4u) ) {
		goto cleanup;
	}
	/* WriteAt / WriteAtFull：绝对偏移写入，Full 保证写满。 */
	if ( !xrtWriteAtFull(File, 4u, "efgh", 4u, NULL) ) {
		goto cleanup;
	}
	(void)xrtWriteAt(File, 0u, "AB", 2u, NULL);
	/* 恢复被 WriteAt 覆盖的前缀后自检。 */
	(void)xrtWriteAtFull(File, 0u, "abcd", 4u, NULL);

	/* 元数据族：Size / Stat / Flags / Native。 */
	if ( !xrtFileSize(File, &iSize) || (iSize != 8u) ) {
		goto cleanup;
	}
	if ( !xrtFileStat(File, &Info) || (Info.Size != 8u) ) {
		goto cleanup;
	}
	iFlags = xrtFileFlags(File);
	printf("pos-io: rd=%zu rd-at=%zu size=%llu flags-readable=%d\n",
		(size_t)4u, (size_t)4u, (unsigned long long)iSize,
		(iFlags & XFILE_READ) ? 1 : 0);
	{
		intptr_t Native = xrtFileNative(File);

		printf("native=%d\n", Native != 0 ? 1 : 0);
	}

	/* Resize（打开对象）/ SetSize（路径版）。 */
	if ( !xrtFileResize(File, 4u) || !xrtFileSize(File, &iSize) ||
		(iSize != 4u) ) {
		goto cleanup;
	}
	printf("resized=%llu", (unsigned long long)iSize);
	if ( !xrtFileSetSize(sPath, 2u) ) {
		goto cleanup;
	}
	(void)xrtFileStat(File, &Info);
	printf(" setsize=%llu stat-size=%llu\n",
		(unsigned long long)Info.Size, (unsigned long long)Info.Size);

	/* ReadAllLimit：带上限整读（防超大文件撑爆内存）。 */
	{
		bytes pAll = xrtFileReadAllLimit(sPath, 16u, &iDone);

		printf("limit=%s", (pAll != NULL && iDone == 2u) ? "ok" : "fail");
		xrtFree(pAll);
	}

	/* 区间锁：Lock/Unlock 必须是完全相同的字节区间。 */
	if ( !xrtFileLockRange(File, XFILE_LOCK_SHARED, 0u, 4u, true) ) {
		goto cleanup;
	}
	if ( !xrtFileUnlockRange(File, 0u, 4u) ) {
		goto cleanup;
	}
	printf(" lock=1");

	/* 共享写映射 + MapFlush 提交。 */
	{
		xfileoptions MapOpen;

		(void)xrtFileOptionsInit(&MapOpen);
		MapOpen.Flags = XFILE_READ | XFILE_WRITE;
		(void)xrtClose(File);
		File = xrtFileOpen(sPath, &MapOpen);
		if ( File == NULL ) {
			return 2;
		}
		Map = xrtFileMap(File, 0u, 0u, XFILE_MAP_READ | XFILE_MAP_WRITE);
		if ( Map == NULL ) {
			goto cleanup;
		}
		printf(" map-flush=%d\n", xrtFileMapFlush(Map, 0u, 0u) ? 1 : 0);
		(void)xrtFileUnmap(Map);
	}

	/* Touch：已存在文件 = 刷新时间戳。 */
	(void)xrtFileTouch(sPath);
	iResult = 0;

cleanup:
	if ( File != NULL ) {
		(void)xrtClose(File);
	}
	(void)xrtFileDelete(sPath);
	return iResult;
}
