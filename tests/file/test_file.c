#include "../test.h"



#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
#else
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif



#if defined(_WIN32) || defined(_WIN64)
	#define TEST_FILE_ATTRIBUTE_HIDDEN 0x02u
#endif



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testFilePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "temporary file path allocation failed");
	return sPath;
}



/* 基础读写必须保留短操作、严格完整操作和 EOF 边界。 */
static void testFileReadWrite(cstr sPath)
{
	static const unsigned char arrData[] = { 0, 1, 2, 3, 0, 254, 255 };
	unsigned char arrRead[8];
	size_t iDone;
	uint64 iPosition;
	xfile File;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE | XFILE_CREATE |
		XFILE_EXCLUSIVE);
	testRequire(File != NULL, "exclusive file creation failed");
	testRequire(
		xrtFileFlags(File) == (XFILE_READ | XFILE_WRITE |
			XFILE_CREATE | XFILE_EXCLUSIVE),
		"opened file flags are incorrect"
	);
	testRequire(xrtFileNative(File) != (intptr_t)-1,
		"native file handle is invalid");
	testRequire(xrtWriteFull(File, arrData, sizeof(arrData), &iDone) &&
		(iDone == sizeof(arrData)), "full binary write failed");
	testRequire(xrtTell(File, &iPosition) && (iPosition == sizeof(arrData)),
		"file position after write is incorrect");
	testRequire(xrtSeek(File, 0, XSEEK_START, &iPosition) && (iPosition == 0u),
		"seek to file start failed");

	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(xrtReadFull(File, arrRead, sizeof(arrData), &iDone) &&
		(iDone == sizeof(arrData)) &&
		(memcmp(arrRead, arrData, sizeof(arrData)) == 0),
		"full binary read failed");
	testRequire(xrtRead(File, &arrRead[7], 1, &iDone) && (iDone == 0u) &&
		(arrRead[7] == 0xA5), "EOF changed the destination buffer");

	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"seek before partial EOF test failed");
	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(!xrtReadFull(File, arrRead, sizeof(arrRead), &iDone) &&
		(iDone == sizeof(arrData)) && (arrRead[7] == 0xA5),
		"premature EOF did not preserve the partial read");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_EOF),
		"premature EOF reported the wrong error");
	xrtClearError();

	testRequire(xrtRead(File, NULL, 0, &iDone) && (iDone == 0u),
		"zero-length read failed");
	testRequire(xrtWrite(File, NULL, 0, &iDone) && (iDone == 0u),
		"zero-length write failed");
	testRequire(xrtClose(File), "file close failed");
}



/* 追加、截断和 64 位定位必须使用操作系统原生语义。 */
static void testFilePositionAndAppend(cstr sPath)
{
	static const unsigned char arrRestore[] = {
		0, 1, 2, 3, 0, 254, 255, 0x5A
	};
	const uint64 iSparsePosition = (UINT64_C(4) << 30) + UINT64_C(17);
	unsigned char iByte = 0x5A;
	uint64 iSize;
	xfile File;

	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE | XFILE_APPEND);
	testRequire(File != NULL, "append open failed");
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"append test seek failed");
	testRequire(xrtWriteFull(File, &iByte, 1, NULL),
		"append write failed");
	testRequire(xrtFileSize(File, &iSize) && (iSize == 8u),
		"append did not write at the file end");
	testRequire(!xrtFileResize(File, 4u),
		"append handle accepted file resize");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"append resize reported the wrong error");
	xrtClearError();
	testRequire(xrtClose(File), "append file close failed");

	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE);
	testRequire(File != NULL, "sparse file reopen failed");
	testRequire(xrtSeek(File, (int64)iSparsePosition, XSEEK_START, NULL),
		"64-bit sparse seek failed");
	testRequire(xrtWriteFull(File, &iByte, 1, NULL),
		"64-bit sparse write failed");
	testRequire(xrtFileSize(File, &iSize) && (iSize == (iSparsePosition + 1u)),
		"64-bit file size was truncated");
	testRequire(xrtFileResize(File, 8u), "file resize failed");
	testRequire(xrtFileSize(File, &iSize) && (iSize == 8u),
		"resized file has the wrong size");
	testRequire(xrtFlush(File), "file flush failed");
	testRequire(xrtClose(File), "sparse file close failed");
	testRequire(xrtFileSetSize(sPath, 3u), "path file resize failed");
	{
		xfileinfo Info;

		testRequire(xrtPathStat(sPath, true, &Info) &&
			((Info.Available & XFILE_INFO_SIZE) != 0u) && (Info.Size == 3u),
			"resized path has the wrong size");
	}
	testRequire(xrtFileSetSize(sPath, 8u), "path file resize restore failed");

	File = xrtOpen(sPath, XFILE_WRITE | XFILE_APPEND | XFILE_TRUNCATE);
	testRequire(File != NULL,
		"append and truncate open failed");
	testRequire(xrtFileSize(File, &iSize) && (iSize == 0u),
		"append and truncate did not clear the file");
	testRequire(xrtWriteFull(File,
		arrRestore, sizeof(arrRestore), NULL),
		"append and truncate restore write failed");
	testRequire(xrtFlush(File),
		"append and truncate flush failed");
	testRequire(xrtClose(File),
		"append and truncate file close failed");
}



/* 绝对偏移读写不得移动共享游标，并必须拒绝追加写和范围溢出。 */
static void testFilePositionedIO(cstr sPath)
{
	static const unsigned char arrBefore[] = {
		0, 1, 2, 3, 0, 254, 255, 0x5A
	};
	static const unsigned char arrExpected[] = {
		0, 1, 2, 3, 0, 9, 8, 0x5A
	};
	static const unsigned char arrPatch[] = { 9, 8 };
	unsigned char arrRead[9];
	size_t iDone;
	uint64 iPosition;
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE);

	testRequire(File != NULL, "positioned IO open failed");
	#if !defined(_WIN32) && !defined(_WIN64)
		testRequire((fcntl((int)xrtFileNative(File), F_GETFD) &
			FD_CLOEXEC) != 0, "opened file descriptor is inheritable");
	#endif
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL) &&
		xrtWriteFull(File, arrBefore, sizeof(arrBefore), NULL),
		"positioned IO fixture reset failed");
	testRequire(xrtSeek(File, 3, XSEEK_START, NULL),
		"positioned IO cursor setup failed");
	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(xrtReadAt(File, 1u, arrRead, 3u, &iDone),
		"single positioned read operation failed");
	testRequire(iDone == 3u, "single positioned read size is incorrect");
	testRequire(memcmp(arrRead, arrExpected + 1u, 3u) == 0,
		"single positioned read content is incorrect");
	testRequire(xrtTell(File, &iPosition) && (iPosition == 3u),
		"positioned read changed the shared cursor");
	testRequire(xrtWriteAt(File, 5u, arrPatch, sizeof(arrPatch), &iDone) &&
		(iDone == sizeof(arrPatch)), "single positioned write failed");
	testRequire(xrtTell(File, &iPosition) && (iPosition == 3u),
		"positioned write changed the shared cursor");
	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(xrtReadAtFull(File, 0u, arrRead,
		sizeof(arrExpected), &iDone) && (iDone == sizeof(arrExpected)) &&
		(memcmp(arrRead, arrExpected, sizeof(arrExpected)) == 0),
		"full positioned read returned incorrect data");
	testRequire(xrtTell(File, &iPosition) && (iPosition == 3u),
		"full positioned read changed the shared cursor");

	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(!xrtReadAtFull(File, 7u, arrRead, 2u, &iDone) &&
		(iDone == 1u) && (arrRead[0] == 0x5A) && (arrRead[1] == 0xA5),
		"positioned EOF did not preserve the partial read");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_EOF),
		"positioned EOF reported the wrong error");
	xrtClearError();
	testRequire(!xrtReadAt(File, (uint64)INT64_MAX,
		arrRead, 2u, &iDone) && (iDone == 0u),
		"positioned read accepted an overflowing range");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"positioned range failure reported the wrong error");
	xrtClearError();
	testRequire(xrtReadAt(File, UINT64_MAX, NULL, 0u, &iDone) &&
		(iDone == 0u), "zero-length positioned read rejected its offset");
	testRequire(xrtClose(File), "positioned IO file close failed");

	File = xrtOpen(sPath, XFILE_WRITE | XFILE_APPEND);
	testRequire(File != NULL, "append handle for positioned write failed");
	testRequire(!xrtWriteAt(File, 0u, arrPatch,
		sizeof(arrPatch), &iDone) && (iDone == 0u),
		"positioned write accepted an append handle");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"append positioned write reported the wrong error");
	xrtClearError();
	testRequire(xrtClose(File), "append positioned file close failed");
}



/* 元数据和存在性 Helper 必须正确区分文件、目录和失败输出。 */
static void testFileMetadata(cstr sPath)
{
	xfileinfo Info;
	xfileinfo Saved;
	str sMissing = testFilePath("xrt-file-base-missing.tmp");

	testRequire(xrtPathStat(sPath, true, &Info), "path metadata query failed");
	testRequire((Info.Type == XFILE_TYPE_FILE) &&
		((Info.Available & XFILE_INFO_SIZE) != 0u) && (Info.Size == 8u),
		"file metadata is incorrect");
	testRequire(xrtPathExists(sPath) && xrtFileExists(sPath) &&
		!xrtDirExists(sPath), "file predicates are incorrect");

	(void)xrtFileDelete(sMissing);
	xrtClearError();
	memset(&Info, 0xA5, sizeof(Info));
	Saved = Info;
	testRequire(!xrtPathStat(sMissing, true, &Info),
		"missing path metadata unexpectedly succeeded");
	testRequire(memcmp(&Info, &Saved, sizeof(Info)) == 0,
		"failed metadata query modified its output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"missing path reported the wrong error kind");
	xrtClearError();
	testRequire(!xrtPathExists(sMissing) && !xrtFileExists(sMissing),
		"missing path predicate is incorrect");
	xrtFree(sMissing);
}



/* 元数据修改必须支持时间、平台属性或权限以及 Touch 语义。 */
static void testFileMetadataMutation(cstr sPath)
{
	const xtime Accessed = (xtime)946684800000000LL;
	const xtime Modified = (xtime)946684801234567LL;
	xfileinfo Info;
	str sTouched = testFilePath("xrt-file-base-touched.tmp");

	testRequire(xrtPathSetTimes(sPath, true, &Accessed, &Modified),
		"path timestamp update failed");
	testRequire(xrtPathStat(sPath, true, &Info) &&
		(Info.Accessed == Accessed) && (Info.Modified == Modified),
		"updated path timestamps are incorrect");
	#if defined(_WIN32) || defined(_WIN64)
		{
			const uint64 iEpochTicks = UINT64_C(116444736000000000);
			uint32 iOriginal = Info.Attributes;
			uint32 iHidden = iOriginal | TEST_FILE_ATTRIBUTE_HIDDEN;
			FILETIME BeforeEpoch;
			xfile File;

			testRequire(xrtPathSetAttributes(sPath, iHidden),
				"Windows attribute update failed");
			testRequire(xrtPathStat(sPath, false, &Info) &&
				((Info.Attributes & TEST_FILE_ATTRIBUTE_HIDDEN) != 0u),
				"Windows hidden attribute was not applied");
			testRequire(xrtPathSetAttributes(sPath, iOriginal),
				"Windows attribute restore failed");
			testRequire(!xrtPathSetMode(sPath, true, 0600u) &&
				(xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
				"Windows mode update did not report unsupported");
			xrtClearError();

			File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE);
			testRequire(File != NULL,
				"Windows timestamp boundary open failed");
			BeforeEpoch.dwLowDateTime = (DWORD)(iEpochTicks - 1u);
			BeforeEpoch.dwHighDateTime =
				(DWORD)((iEpochTicks - 1u) >> 32);
			testRequire(SetFileTime((HANDLE)xrtFileNative(File), NULL,
				NULL, &BeforeEpoch) != 0,
				"Windows timestamp boundary setup failed");
			testRequire(xrtFileStat(File, &Info) && (Info.Modified == -1),
				"Windows sub-microsecond time did not round down");
			testRequire(xrtClose(File),
				"Windows timestamp boundary file close failed");
		}
	#else
		testRequire(xrtPathSetMode(sPath, true, 0600u),
			"POSIX mode update failed");
		testRequire(xrtPathStat(sPath, true, &Info) &&
			((Info.Mode & 0777u) == 0600u), "POSIX mode was not applied");
		testRequire(!xrtPathSetAttributes(sPath, 0u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
			"POSIX attributes update did not report unsupported");
		xrtClearError();
	#endif

	(void)xrtFileDelete(sTouched);
	xrtClearError();
	testRequire(xrtFileTouch(sTouched) && xrtFileExists(sTouched),
		"touch did not create a missing file");
	testRequire(xrtFileTouch(sPath), "touch did not update an existing file");
	testRequire(xrtPathStat(sPath, true, &Info) &&
		(Info.Modified > Modified), "touch did not advance the modified time");
	testRequire(xrtFileDelete(sTouched), "touched file cleanup failed");
	xrtFree(sTouched);
}



/* 改名必须分别压实禁止替换和原子替换两种契约。 */
static void testFileRename(cstr sSource)
{
	str sTarget = testFilePath("xrt-file-base-target.tmp");
	str sOccupied = testFilePath("xrt-file-base-occupied.tmp");
	xfile File;

	(void)xrtFileDelete(sTarget);
	(void)xrtFileDelete(sOccupied);
	xrtClearError();
	testRequire(xrtPathRename(sSource, sTarget, false),
		"non-replacing rename failed");
	testRequire(!xrtPathExists(sSource) && xrtFileExists(sTarget),
		"rename did not move the source name");

	File = xrtOpen(sOccupied, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	testRequire(File != NULL, "occupied rename target creation failed");
	testRequire(xrtClose(File), "occupied rename target close failed");
	testRequire(!xrtPathRename(sTarget, sOccupied, false),
		"non-replacing rename overwrote an existing target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"rename collision reported the wrong error kind");
	xrtClearError();
	testRequire(xrtPathRename(sTarget, sOccupied, true),
		"replacing rename failed");
	testRequire(!xrtPathExists(sTarget) && xrtFileExists(sOccupied),
		"replacing rename left the old name behind");
	testRequire(xrtFileDelete(sOccupied), "renamed file cleanup failed");
	xrtFree(sTarget);
	xrtFree(sOccupied);
}



/* 打开参数检查必须拒绝含糊或无意义的组合。 */
static void testFileArguments(cstr sPath)
{
	size_t iDone = 9;

	testRequire(xrtOpen(sPath, XFILE_READ | XFILE_TRUNCATE) == NULL,
		"read-only truncate was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid open flags reported the wrong error");
	xrtClearError();
	testRequire(!xrtRead(NULL, NULL, 0, &iDone) && (iDone == 0u),
		"null file read was accepted");
	testRequire(xrtFileFlags(NULL) == 0u,
		"null file flags query was accepted");
	testRequire(!xrtClose(NULL), "null file close was accepted");
	xrtClearError();
}



/* 文件基础层回归入口。 */
int main(void)
{
	str sPath = testFilePath("xrt-file-base-\xE8\xBE\xB9\xE7\x95\x8C.tmp");

	testFileReadWrite(sPath);
	testFilePositionAndAppend(sPath);
	testFilePositionedIO(sPath);
	testFileMetadata(sPath);
	testFileMetadataMutation(sPath);
	testFileArguments(sPath);
	testFileRename(sPath);
	xrtFree(sPath);
	return 0;
}
