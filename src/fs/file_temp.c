#include "../internal/xrt_file_temp.h"



#if defined(XRT_FEATURE_FILE_TEMP)

/* 检查临时对象名前后缀是否为安全的单个名称片段。 */
static bool __xrtTempNamePart(cstr sPart, bool bSuffix)
{
	const unsigned char* pText = (const unsigned char*)sPart;
	const unsigned char* pLast = NULL;

	if ( sPart == NULL ) {
		return true;
	}
	while ( *pText != 0u ) {
		if ( (*pText == (unsigned char)'/') ||
			 (*pText == (unsigned char)'\\') ) {
			return false;
		}
		#if defined(_WIN32) || defined(_WIN64)
			if ( (*pText < 32u) ||
				 (*pText == (unsigned char)'<') ||
				 (*pText == (unsigned char)'>') ||
				 (*pText == (unsigned char)':') ||
				 (*pText == (unsigned char)'"') ||
				 (*pText == (unsigned char)'|') ||
				 (*pText == (unsigned char)'?') ||
				 (*pText == (unsigned char)'*') ) {
				return false;
			}
		#endif
		pLast = pText++;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( bSuffix && (pLast != NULL) &&
			 ((*pLast == (unsigned char)'.') ||
			  (*pLast == (unsigned char)' ')) ) {
			return false;
		}
	#else
		(void)bSuffix;
		(void)pLast;
	#endif
	return true;
}



/* 初始化临时名称生成器并解析默认值。 */
bool __xrtTempNameInit(__xrttempname* pName,
	cstr sDirectory, cstr sPrefix, cstr sSuffix,
	cstr sDefaultPrefix, cstr sDefaultSuffix)
{
	if ( pName != NULL ) {
		memset(pName, 0, sizeof(*pName));
	}
	if ( (pName == NULL) || (sDefaultPrefix == NULL) ||
		 (sDefaultSuffix == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pName->Prefix = (sPrefix != NULL) ? sPrefix : sDefaultPrefix;
	pName->Suffix = (sSuffix != NULL) ? sSuffix : sDefaultSuffix;
	if ( !__xrtTempNamePart(pName->Prefix, false) ||
		 !__xrtTempNamePart(pName->Suffix, true) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sDirectory == NULL ) {
		pName->OwnedDirectory = xrtPathTemp();
		if ( pName->OwnedDirectory == NULL ) {
			return false;
		}
		pName->Directory = pName->OwnedDirectory;
	} else {
		pName->Directory = sDirectory;
	}
	return true;
}



/* 生成带 64 位安全随机部分的临时对象路径。 */
str __xrtTempNameNext(const __xrttempname* pName)
{
	static const char sHex[] = "0123456789abcdef";
	size_t iPrefix;
	size_t iSuffix;
	size_t iSize;
	uint64 iRandom;
	str sName;
	str sPath;
	size_t i;

	if ( pName == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iPrefix = strlen(pName->Prefix);
	iSuffix = strlen(pName->Suffix);
	if ( (iPrefix > (SIZE_MAX - 17u)) ||
		 (iSuffix > (SIZE_MAX - iPrefix - 17u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !xrtSecureRandom(&iRandom, sizeof(iRandom)) ) {
		return NULL;
	}
	iSize = iPrefix + 16u + iSuffix;
	sName = (str)xrtMalloc(iSize + 1u);
	if ( sName == NULL ) {
		return NULL;
	}
	memcpy(sName, pName->Prefix, iPrefix);
	for ( i = 0; i < 16u; i++ ) {
		sName[iPrefix + i] =
			sHex[(iRandom >> ((15u - i) * 4u)) & 0x0Fu];
	}
	memcpy(sName + iPrefix + 16u, pName->Suffix, iSuffix);
	sName[iSize] = '\0';
	sPath = xrtPathJoin(pName->Directory, sName);
	xrtFree(sName);
	return sPath;
}



/* 释放临时名称生成器拥有的系统目录。 */
void __xrtTempNameFree(__xrttempname* pName)
{
	if ( pName == NULL ) {
		return;
	}
	xrtFree(pName->OwnedDirectory);
	memset(pName, 0, sizeof(*pName));
}



/* 按指定创建模式在目录中排他创建临时文件。 */
xfile __xrtFileTempCreate(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, uint32 iMode, str* pPath)
{
	__xrttempname Name;
	uint32 i;

	if ( pPath != NULL ) {
		*pPath = NULL;
	}
	if ( (pPath == NULL) || ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtTempNameInit(&Name, sDirectory, sPrefix, sSuffix,
		".xrt-", ".tmp") ) {
		return NULL;
	}
	for ( i = 0; i < XRT_TEMP_ATTEMPTS; i++ ) {
		str sPath = __xrtTempNameNext(&Name);
		xfileoptions Options;
		xfile File;

		if ( sPath == NULL ) {
			__xrtTempNameFree(&Name);
			return NULL;
		}
		xrtFileOptionsInit(&Options);
		Options.Flags = XFILE_READ | XFILE_WRITE |
			XFILE_CREATE | XFILE_EXCLUSIVE;
		Options.Mode = iMode;
		File = xrtFileOpen(sPath, &Options);
		if ( File != NULL ) {
			__xrtTempNameFree(&Name);
			*pPath = sPath;
			return File;
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
	__xrtFileError(XERR_AGAIN, XFILE_ERROR_TEMP, "temp",
		"could not create a unique temporary file");
	return NULL;
}



/* 在指定目录排他创建调用方拥有的临时文件。 */
XRT_API xfile xrtFileTemp(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, str* pPath)
{
	if ( (sDirectory != NULL) && (sDirectory[0] == '\0') ) {
		if ( pPath != NULL ) {
			*pPath = NULL;
		}
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtFileTempCreate(sDirectory,
		sPrefix, sSuffix, 0600u, pPath);
}

#endif
