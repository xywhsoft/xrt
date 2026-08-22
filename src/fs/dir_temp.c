#include "../internal/xrt_dir.h"
#include "../internal/xrt_file_temp.h"



#if defined(XRT_FEATURE_DIR_TEMP)

/* 排他创建临时目录并返回调用方拥有的路径。 */
XRT_API str xrtDirTemp(cstr sDirectory, cstr sPrefix, cstr sSuffix)
{
	__xrttempname Name;
	uint32 i;

	if ( (sDirectory != NULL) && (sDirectory[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtTempNameInit(&Name, sDirectory, sPrefix, sSuffix,
		".xrt-dir-", "") ) {
		return NULL;
	}
	for ( i = 0; i < XRT_TEMP_ATTEMPTS; i++ ) {
		str sPath = __xrtTempNameNext(&Name);

		if ( sPath == NULL ) {
			__xrtTempNameFree(&Name);
			return NULL;
		}
		if ( xrtDirCreateMode(sPath, 0700u) ) {
			__xrtTempNameFree(&Name);
			return sPath;
		}
		if ( (xrtGetError() == NULL) ||
			 (xrtErrorKind(xrtGetError()) != XERR_EXISTS) ) {
			xrtFree(sPath);
			__xrtTempNameFree(&Name);
			return NULL;
		}
		xrtFree(sPath);
		xrtClearError();
	}
	__xrtTempNameFree(&Name);
	__xrtDirError(XERR_AGAIN, XDIR_ERROR_TEMP, "temp",
		"could not create a unique temporary directory");
	return NULL;
}

#endif
