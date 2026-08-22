#include "../test.h"



/* 文件映射必须支持只读、共享写、私有写、空区间和边界检查。 */
int main(void)
{
	static const char sPath[] = "xrt-file-map.tmp";
	xfile File;
	xfilemap Map;
	char arrText[6];

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_EXCLUSIVE);
	testRequire(File != NULL, "file map fixture open failed");
	testRequire(xrtWriteFull(File, "abcdef", 6u, NULL),
		"file map fixture write failed");

	Map = xrtFileMap(File, 1u, 3u, XFILE_MAP_READ);
	testRequire((Map != NULL) && (xrtFileMapSize(Map) == 3u) &&
		(memcmp(xrtFileMapData(Map), "bcd", 3u) == 0),
		"read-only file mapping is incorrect");
	testRequire(xrtFileUnmap(Map), "read-only file unmap failed");

	Map = xrtFileMap(File, 1u, 3u,
		XFILE_MAP_READ | XFILE_MAP_WRITE);
	testRequire(Map != NULL, "shared-write file mapping failed");
	memcpy(xrtFileMapData(Map), "XYZ", 3u);
	testRequire(xrtFileMapFlush(Map, 0u, 0u),
		"shared-write file mapping flush failed");
	testRequire(xrtFileUnmap(Map), "shared-write file unmap failed");
	testRequire(xrtReadAtFull(File, 0u,
		arrText, sizeof(arrText), NULL) &&
		(memcmp(arrText, "aXYZef", 6u) == 0),
		"shared-write mapping did not update the file");

	Map = xrtFileMap(File, 0u, 3u,
		XFILE_MAP_READ | XFILE_MAP_COPY);
	testRequire(Map != NULL, "private file mapping failed");
	memcpy(xrtFileMapData(Map), "123", 3u);
	testRequire(!xrtFileMapFlush(Map, 0u, 0u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"private mapping accepted a shared flush");
	xrtClearError();
	testRequire(xrtFileUnmap(Map), "private file unmap failed");
	testRequire(xrtReadAtFull(File, 0u,
		arrText, sizeof(arrText), NULL) &&
		(memcmp(arrText, "aXYZef", 6u) == 0),
		"private mapping modified the file");

	Map = xrtFileMap(File, 6u, 0u,
		XFILE_MAP_READ | XFILE_MAP_WRITE);
	testRequire((Map != NULL) &&
		(xrtFileMapData(Map) == NULL) &&
		(xrtFileMapSize(Map) == 0u),
		"empty file mapping contract is incorrect");
	testRequire(xrtFileMapFlush(Map, 0u, 0u),
		"empty shared-write mapping flush failed");
	testRequire(xrtFileUnmap(Map), "empty file unmap failed");

	testRequire(xrtFileMap(File, 7u, 0u,
		XFILE_MAP_READ) == NULL,
		"file mapping accepted an offset beyond EOF");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"out-of-range file mapping reported the wrong error");
	xrtClearError();
	testRequire(xrtFileMap(File, 0u, 1u,
		XFILE_MAP_WRITE) == NULL,
		"file mapping accepted write without read mode");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid file mapping flags reported the wrong error");
	xrtClearError();

	testRequire(xrtClose(File), "file map fixture close failed");
	File = xrtOpen(sPath,
		XFILE_READ | XFILE_WRITE | XFILE_APPEND);
	testRequire(File != NULL,
		"append mapping fixture open failed");
	testRequire(xrtFileMap(File, 0u, 1u,
		XFILE_MAP_READ | XFILE_MAP_WRITE) == NULL,
		"append handle accepted a shared-write mapping");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"append mapping reported the wrong error");
	xrtClearError();
	Map = xrtFileMap(File, 0u, 1u, XFILE_MAP_READ);
	testRequire(Map != NULL,
		"append handle lost read-only mapping support");
	testRequire(xrtFileUnmap(Map),
		"append read-only mapping unmap failed");
	Map = xrtFileMap(File, 0u, 1u,
		XFILE_MAP_READ | XFILE_MAP_COPY);
	testRequire(Map != NULL,
		"append handle lost private mapping support");
	*(char*)xrtFileMapData(Map) = 'Q';
	testRequire(xrtFileUnmap(Map),
		"append private mapping unmap failed");
	testRequire(xrtReadAtFull(File, 0u, arrText, 1u, NULL) &&
		(arrText[0] == 'a'),
		"append private mapping modified the file");
	testRequire(xrtClose(File),
		"append mapping fixture close failed");
	testRequire(xrtFileDelete(sPath), "file map fixture delete failed");
	return 0;
}
