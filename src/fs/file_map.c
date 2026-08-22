#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../internal/xrt_file.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/mman.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_FILE_MAP)

/* 映射对象同时保存系统基址和调用方可见的未对齐子区间。 */
struct xfilemap_impl {
	ptr Base;
	ptr Data;
	size_t MappedSize;
	size_t Size;
	size_t Delta;
	bool SharedWrite;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Mapping;
	#endif
};



/* 检查映射模式和文件访问权限。 */
static bool __xrtFileMapFlags(xfile File, uint32 iFlags)
{
	const uint32 iKnown = XFILE_MAP_READ |
		XFILE_MAP_WRITE | XFILE_MAP_COPY;
	uint32 iFileFlags;

	if ( (File == NULL) || ((iFlags & XFILE_MAP_READ) == 0u) ||
		 ((iFlags & ~iKnown) != 0u) ||
		 (((iFlags & XFILE_MAP_WRITE) != 0u) &&
		  ((iFlags & XFILE_MAP_COPY) != 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iFileFlags = xrtFileFlags(File);
	if ( ((iFileFlags & XFILE_READ) == 0u) ||
		 (((iFlags & XFILE_MAP_WRITE) != 0u) &&
		  (((iFileFlags & XFILE_WRITE) == 0u) ||
		   ((iFileFlags & XFILE_APPEND) != 0u))) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 根据当前文件大小展开映射范围，避免访问文件末端之外的页面。 */
static bool __xrtFileMapRange(xfile File, uint64 iOffset,
	size_t iInputSize, size_t* pSize)
{
	uint64 iFileSize;
	uint64 iSize;

	if ( (iOffset > (uint64)INT64_MAX) ||
		 !xrtFileSize(File, &iFileSize) ) {
		if ( iOffset > (uint64)INT64_MAX ) {
			__xrtFileError(XERR_RANGE, XFILE_ERROR_MAP, "map-range",
				"the mapping offset is outside the supported range");
		}
		return false;
	}
	if ( iOffset > iFileSize ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_MAP, "map-range",
			"the mapping offset is beyond the file");
		return false;
	}
	iSize = (iInputSize == 0u) ?
		(iFileSize - iOffset) : (uint64)iInputSize;
	if ( (iSize > (iFileSize - iOffset)) ||
		 (iSize > (uint64)SIZE_MAX) ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_MAP, "map-range",
			"the mapping range is beyond the file or address space");
		return false;
	}
	*pSize = (size_t)iSize;
	return true;
}



/* 创建空映射对象，使空文件不需要失败哨兵。 */
static xfilemap __xrtFileMapEmpty(bool bSharedWrite)
{
	xfilemap Map = (xfilemap)xrtMalloc(sizeof(*Map));

	if ( Map != NULL ) {
		memset(Map, 0, sizeof(*Map));
		Map->SharedWrite = bSharedWrite;
	}
	return Map;
}



/* 映射一个文件区间。 */
XRT_API xfilemap xrtFileMap(xfile File, uint64 iOffset,
	size_t iInputSize, uint32 iFlags)
{
	size_t iSize;
	xfilemap Map;
	intptr_t hFile;

	if ( !__xrtFileMapFlags(File, iFlags) ||
		 !__xrtFileMapRange(File, iOffset, iInputSize, &iSize) ) {
		return NULL;
	}
	if ( iSize == 0u ) {
		return __xrtFileMapEmpty(
			(iFlags & XFILE_MAP_WRITE) != 0u);
	}
	hFile = xrtFileNative(File);
	if ( hFile == (intptr_t)-1 ) {
		return NULL;
	}
	Map = (xfilemap)xrtMalloc(sizeof(*Map));
	if ( Map == NULL ) {
		return NULL;
	}
	memset(Map, 0, sizeof(*Map));
	Map->Size = iSize;
	Map->SharedWrite = (iFlags & XFILE_MAP_WRITE) != 0u;
	#if defined(_WIN32) || defined(_WIN64)
		{
			SYSTEM_INFO Info;
			uint64 iAligned;
			DWORD iProtect;
			DWORD iAccess;

			GetSystemInfo(&Info);
			iAligned = iOffset -
				(iOffset % (uint64)Info.dwAllocationGranularity);
			Map->Delta = (size_t)(iOffset - iAligned);
			if ( iSize > (SIZE_MAX - Map->Delta) ) {
				xrtFree(Map);
				__xrtErrorSetSizeOverflow();
				return NULL;
			}
			Map->MappedSize = Map->Delta + iSize;
			iProtect = ((iFlags & XFILE_MAP_WRITE) != 0u) ?
				PAGE_READWRITE :
				(((iFlags & XFILE_MAP_COPY) != 0u) ?
				 PAGE_WRITECOPY : PAGE_READONLY);
			iAccess = ((iFlags & XFILE_MAP_WRITE) != 0u) ?
				FILE_MAP_WRITE :
				(((iFlags & XFILE_MAP_COPY) != 0u) ?
				 FILE_MAP_COPY : FILE_MAP_READ);
			Map->Mapping = CreateFileMappingW((HANDLE)hFile,
				NULL, iProtect, 0u, 0u, NULL);
			if ( Map->Mapping == NULL ) {
				int iCode = (int)GetLastError();

				xrtFree(Map);
				__xrtFileSetError(XFILE_ERROR_MAP, "map-create",
					"failed to create the file mapping", iCode);
				return NULL;
			}
			Map->Base = MapViewOfFile(Map->Mapping, iAccess,
				(DWORD)(iAligned >> 32), (DWORD)iAligned,
				Map->MappedSize);
			if ( Map->Base == NULL ) {
				int iCode = (int)GetLastError();

				(void)CloseHandle(Map->Mapping);
				xrtFree(Map);
				__xrtFileSetError(XFILE_ERROR_MAP, "map-view",
					"failed to map the file view", iCode);
				return NULL;
			}
		}
	#else
		{
			long iPageSize = sysconf(_SC_PAGE_SIZE);
			uint64 iAligned;
			int iProtect = PROT_READ;
			int iMapFlags = ((iFlags & XFILE_MAP_WRITE) != 0u) ?
				MAP_SHARED : MAP_PRIVATE;

			if ( iPageSize <= 0 ) {
				int iCode = errno;

				xrtFree(Map);
				__xrtFileSetError(XFILE_ERROR_MAP, "map-page-size",
					"failed to query the mapping page size", iCode);
				return NULL;
			}
			iAligned = iOffset - (iOffset % (uint64)iPageSize);
			Map->Delta = (size_t)(iOffset - iAligned);
			if ( iSize > (SIZE_MAX - Map->Delta) ) {
				xrtFree(Map);
				__xrtErrorSetSizeOverflow();
				return NULL;
			}
			Map->MappedSize = Map->Delta + iSize;
			if ( (iFlags &
				(XFILE_MAP_WRITE | XFILE_MAP_COPY)) != 0u ) {
				iProtect |= PROT_WRITE;
			}
			Map->Base = mmap(NULL, Map->MappedSize,
				iProtect, iMapFlags, (int)hFile, (off_t)iAligned);
			if ( Map->Base == MAP_FAILED ) {
				int iCode = errno;

				xrtFree(Map);
				__xrtFileSetError(XFILE_ERROR_MAP, "map-view",
					"failed to map the file view", iCode);
				return NULL;
			}
		}
	#endif
	Map->Data = (bytes)Map->Base + Map->Delta;
	return Map;
}



/* 返回调用方可访问的映射地址。 */
XRT_API ptr xrtFileMapData(xfilemap Map)
{
	if ( Map == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return Map->Data;
}



/* 返回调用方可访问的映射大小。 */
XRT_API size_t xrtFileMapSize(xfilemap Map)
{
	if ( Map == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0u;
	}
	return Map->Size;
}



/* 检查并展开相对于可见映射的刷新区间。 */
static bool __xrtFileMapFlushRange(xfilemap Map,
	size_t iOffset, size_t iInputSize, size_t* pSize)
{
	if ( Map == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !Map->SharedWrite ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( iOffset > Map->Size ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_MAP, "map-flush",
			"the flush offset is outside the mapping");
		return false;
	}
	*pSize = (iInputSize == 0u) ?
		(Map->Size - iOffset) : iInputSize;
	if ( *pSize > (Map->Size - iOffset) ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_MAP, "map-flush",
			"the flush range is outside the mapping");
		return false;
	}
	return true;
}



/* 把共享写映射区间提交给操作系统。 */
XRT_API bool xrtFileMapFlush(xfilemap Map,
	size_t iOffset, size_t iInputSize)
{
	size_t iSize;

	if ( !__xrtFileMapFlushRange(Map,
		iOffset, iInputSize, &iSize) ) {
		return false;
	}
	if ( iSize == 0u ) {
		return true;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !FlushViewOfFile((bytes)Map->Data + iOffset, iSize) ) {
			int iCode = (int)GetLastError();

			__xrtFileSetError(XFILE_ERROR_MAP, "map-flush",
				"failed to flush the mapped file range", iCode);
			return false;
		}
	#else
		{
			long iPageSize = sysconf(_SC_PAGE_SIZE);
			size_t iStart = Map->Delta + iOffset;
			size_t iAligned;
			size_t iLength;

			if ( iPageSize <= 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_MAP, "map-page-size",
					"failed to query the mapping page size", iCode);
				return false;
			}
			iAligned = iStart - (iStart % (size_t)iPageSize);
			iLength = (iStart - iAligned) + iSize;
			if ( msync((bytes)Map->Base + iAligned,
				iLength, MS_SYNC) != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_MAP, "map-flush",
					"failed to flush the mapped file range", iCode);
				return false;
			}
		}
	#endif
	return true;
}



/* 解除映射并销毁对象。 */
XRT_API bool xrtFileUnmap(xfilemap Map)
{
	bool bResult = true;
	int iCode = 0;

	if ( Map == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Map->Base != NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( !UnmapViewOfFile(Map->Base) ) {
				bResult = false;
				iCode = (int)GetLastError();
			}
			(void)CloseHandle(Map->Mapping);
		#else
			if ( munmap(Map->Base, Map->MappedSize) != 0 ) {
				bResult = false;
				iCode = errno;
			}
		#endif
	}
	xrtFree(Map);
	if ( !bResult ) {
		__xrtFileSetError(XFILE_ERROR_MAP, "unmap",
			"failed to unmap the file view", iCode);
	}
	return bResult;
}

#endif
