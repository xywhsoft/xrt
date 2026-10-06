#include <xrt/acme_store.h>

#if defined(XACME_FEATURE_ACME_STORE)

#include <xrt/charset.h>
#include <xrt/crypto.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <xrt/pem.h>
#include <xrt/time.h>
#include <xrt/x509.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <errno.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif

#define XACME_STORE_GENERATION_PREFIX ".grant-"
#define XACME_STORE_GENERATION_SIZE 23u
#define XACME_STORE_POINTER_LIMIT 64u
#define XACME_STORE_CERT_LIMIT (16u * 1024u * 1024u)
#define XACME_STORE_KEY_LIMIT (1024u * 1024u)
#define XACME_STORE_META_LIMIT 1200u

static void xacmeStoreError(xerrkind Kind, xacmestoreerror Code, cstr s)
{
	xrtSetErrorInfo(Kind, "xrt.acme.store", (int32)Code, s);
}

static bool xacmeStoreFormat(char* sOut, size_t iCapacity, cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store path or metadata exceeds capacity");
		return false;
	}
	return true;
}

/* 存储目录名只接受 ASCII DNS 名与开头的通配符，不接受路径语法。 */
static bool xacmeStoreDomainValid(cstr sDomain)
{
	size_t i, iSize, iLabel = 0u;
	if(sDomain == NULL)
		return false;
	iSize = strlen(sDomain);
	if((iSize == 0u) || (iSize > 253u))
		return false;
	i = ((iSize > 2u) && (sDomain[0] == '*') &&
		(sDomain[1] == '.')) ? 2u : 0u;
	for(; i < iSize; i++)
	{
		unsigned char c = (unsigned char)sDomain[i];
		bool bAlphaNum = ((c >= 'A') && (c <= 'Z')) ||
			((c >= 'a') && (c <= 'z')) ||
			((c >= '0') && (c <= '9'));
		if(c == '.')
		{
			if((iLabel == 0u) || (sDomain[i - 1u] == '-'))
				return false;
			iLabel = 0u;
		}
		else if(bAlphaNum || ((c == '-') && (iLabel != 0u)))
		{
			if(++iLabel > 63u)
				return false;
		}
		else
		{
			return false;
		}
	}
	return (iLabel != 0u) && (sDomain[iSize - 1u] != '-');
}

static bool xacmeStoreCertPath(char* sOut, size_t iCapacity,
	cstr sRoot, cstr sDomain, cstr sFile)
{
	char sMapped[256];
	cstr sDiskDomain = sDomain;
	if((sRoot == NULL) || (sRoot[0] == 0) ||
		!xacmeStoreDomainValid(sDomain))
	{
		xacmeStoreError(XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store requires a root and a valid DNS domain");
		return false;
	}
	/* '*' 不能用于 Windows 路径；百分号不会与合法 DNS 标签碰撞。 */
	if(sDomain[0] == '*')
	{
		if(!xacmeStoreFormat(sMapped, sizeof(sMapped),
			"%%2A%s", sDomain + 1u))
			return false;
		sDiskDomain = sMapped;
	}
	if(sFile == NULL)
		return xacmeStoreFormat(sOut, iCapacity,
			"%s/certs/%s", sRoot, sDiskDomain);
	return xacmeStoreFormat(sOut, iCapacity, "%s/certs/%s/%s",
		sRoot, sDiskDomain, sFile);
}

static bool xacmeStoreGenerationNameValid(cstr sName, size_t iSize)
{
	size_t i;
	if((iSize != XACME_STORE_GENERATION_SIZE) ||
		(memcmp(sName, XACME_STORE_GENERATION_PREFIX, 7u) != 0))
		return false;
	for(i = 7u; i < iSize; i++)
	{
		if(!((sName[i] >= '0' && sName[i] <= '9') ||
			(sName[i] >= 'a' && sName[i] <= 'f')))
			return false;
	}
	return true;
}

/* 并发重命名在 Windows 与部分映射文件系统上可短暂报告未找到。 */
static bool xacmeStoreRetryPointerRead(size_t iAttempt)
{
	xerrkind Kind = xrtErrorKind(xrtGetError());
	if(iAttempt < 7u &&
		(Kind == XERR_NOT_FOUND || Kind == XERR_PERMISSION ||
		Kind == XERR_AGAIN))
	{
		xrtClearError();
		xrtSleep(1u);
		return true;
	}
	return false;
}

/* current 缺失时读取直属布局；存在时只接受受限的版本目录名。 */
static bool xacmeStoreResolveAtBase(char* sOut, size_t iCapacity,
	cstr sBase, bool* pbVersioned)
{
	char sPointer[1024];
	size_t iSize = 0u;
	size_t iAttempt;
	bytes pName;
	xfileinfo Info;
	if(!xacmeStoreFormat(sPointer, sizeof(sPointer), "%s/current", sBase))
		return false;
	for(iAttempt = 0u; ; iAttempt++)
	{
		if(!xrtPathStat(sPointer, false, &Info))
		{
			if(xacmeStoreRetryPointerRead(iAttempt))
				continue;
			if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
				return false;
			xrtClearError();
			*pbVersioned = false;
			return xacmeStoreFormat(sOut, iCapacity, "%s", sBase);
		}
		if(Info.Type != XFILE_TYPE_FILE)
		{
			xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
				"acme store current must be a regular file");
			return false;
		}
		pName = xrtFileReadAllLimit(sPointer,
			XACME_STORE_POINTER_LIMIT, &iSize);
		if(pName != NULL)
			break;
		if(!xacmeStoreRetryPointerRead(iAttempt))
			return false;
	}
	if((iSize != XACME_STORE_GENERATION_SIZE + 1u) ||
		(pName[XACME_STORE_GENERATION_SIZE] != '\n') ||
		!xacmeStoreGenerationNameValid((cstr)pName,
			XACME_STORE_GENERATION_SIZE))
	{
		xrtFree(pName);
		xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store current generation is invalid");
		return false;
	}
	*pbVersioned = true;
	if(!xacmeStoreFormat(sOut, iCapacity, "%s/%.*s", sBase,
		(int)XACME_STORE_GENERATION_SIZE, (cstr)pName))
	{
		xrtFree(pName);
		return false;
	}
	xrtFree(pName);
	return true;
}

/* 新的 %2A 目录优先；POSIX 旧版 '*.domain' 目录仍可读取。 */
static bool xacmeStoreResolveBase(char* sOut, size_t iCapacity,
	cstr sRoot, cstr sDomain, bool* pbVersioned)
{
	char sMapped[1024];
	if(!xacmeStoreCertPath(sMapped, sizeof(sMapped),
		sRoot, sDomain, NULL) ||
		!xacmeStoreResolveAtBase(sOut, iCapacity, sMapped, pbVersioned))
		return false;
	#if !defined(_WIN32) && !defined(_WIN64)
		if((sDomain[0] == '*') && !*pbVersioned)
		{
			char sCert[1024];
			char sLegacy[1024];
			xfileinfo Info;
			if(!xacmeStoreFormat(sCert, sizeof(sCert),
				"%s/fullchain.pem", sMapped))
				return false;
			if(xrtPathStat(sCert, false, &Info))
			{
				if(Info.Type != XFILE_TYPE_FILE)
				{
					xacmeStoreError(XERR_PROTOCOL,
						XACME_STORE_ERROR_PARSE,
						"acme store mapped certificate is not a regular file");
					return false;
				}
				return true;
			}
			if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
				return false;
			xrtClearError();
			if(!xacmeStoreFormat(sLegacy, sizeof(sLegacy),
				"%s/certs/%s", sRoot, sDomain))
				return false;
			if(!xrtPathStat(sLegacy, false, &Info))
			{
				if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
					return false;
				xrtClearError();
				return true;
			}
			if(Info.Type != XFILE_TYPE_DIRECTORY)
			{
				xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
					"acme store legacy wildcard path is not a directory");
				return false;
			}
			return xacmeStoreResolveAtBase(sOut, iCapacity,
				sLegacy, pbVersioned);
		}
	#endif
	return true;
}

/* directory URL → 16 字符十六进制目录名。 */
static bool xacmeStoreCaDir(
	cstr sDirectoryUrl, char* sOut /* >= 17 */)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t i;
	if(!xrtSha256(sDirectoryUrl, strlen(sDirectoryUrl), Digest))
	{
		return false;
	}
	for(i = 0; i < 8u; i++)
	{
		sprintf(sOut + i * 2u, "%02x", Digest[i]);
	}
	sOut[16] = '\0';
	return true;
}

static bool xacmeStoreWriteAtomicText(cstr sPath, cstr sText)
{
	if(!xrtFileWriteAtomic(
			sPath, (xbytesview){ (const uint8*)sText, strlen(sText) }))
	{
		return false;
	}
	return true;
}

/* POSIX rename 只保证可见性；目录项还需要显式落盘。 */
static bool xacmeStoreSyncDirectory(cstr sPath)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		int iFlags = O_RDONLY;
		int iFd;
		int iResult;
		#ifdef O_DIRECTORY
			iFlags |= O_DIRECTORY;
		#endif
		#ifdef O_CLOEXEC
			iFlags |= O_CLOEXEC;
		#endif
		do {
			iFd = open(sPath, iFlags);
		} while((iFd < 0) && (errno == EINTR));
		if(iFd < 0)
		{
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store could not open directory for sync");
			return false;
		}
		do {
			iResult = fsync(iFd);
		} while((iResult != 0) && (errno == EINTR));
		if(close(iFd) != 0)
			iResult = -1;
		if(iResult != 0)
		{
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store directory sync failed");
			return false;
		}
	#else
		(void)sPath;
	#endif
	return true;
}

/* 首次签发时 root/certs/domain 可能均为新目录，逐级持久化目录项。 */
static bool xacmeStoreSyncAncestors(cstr sDirectory)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		char sPath[1024];
		if(!xacmeStoreFormat(sPath, sizeof(sPath), "%s", sDirectory))
			return false;
		for(;;)
		{
			char* sSlash;
			if(!xacmeStoreSyncDirectory(sPath))
				return false;
			if((strcmp(sPath, ".") == 0) || (strcmp(sPath, "/") == 0))
				break;
			sSlash = strrchr(sPath, '/');
			if(sSlash == NULL)
			{
				strcpy(sPath, ".");
			}
			else if(sSlash == sPath)
			{
				sPath[1] = '\0';
			}
			else
			{
				*sSlash = '\0';
			}
		}
	#else
		(void)sDirectory;
	#endif
	return true;
}

/* 临时文件从创建起就是 POSIX 0600，写完后才发布私钥。 */
static bool xacmeStoreWriteAtomicKey(cstr sPath, cstr sText)
{
	str sDirectory = xrtPathParent(sPath);
	str sTemporary = NULL;
	xfile File;
	bool bOk;
	if(sDirectory == NULL)
	{
		return false;
	}
	File = xrtFileTemp(sDirectory, ".xacme-key-", ".tmp", &sTemporary);
	xrtFree(sDirectory);
	if(File == NULL)
	{
		return false;
	}
	bOk = xrtWriteFull(File, sText, strlen(sText), NULL) && xrtFlush(File);
	if(!xrtClose(File))
	{
		bOk = false;
	}
	if(bOk)
	{
		bOk = xrtPathRename(sTemporary, sPath, true);
	}
	if(!bOk)
	{
		(void)xrtFileDelete(sTemporary);
	}
	xrtFree(sTemporary);
	return bOk;
}

/* current 只含版本名。Windows 避开 ReplaceFile 的 1176 中间态；
	MoveFileEx 的失败若已尝试发布则保留版本供调用方核对。 */
static bool xacmeStoreWritePointer(cstr sPath, cstr sText,
	bool* pbPublishAttempted)
{
	*pbPublishAttempted = false;
	#if defined(_WIN32) || defined(_WIN64)
		str sDirectory = xrtPathParent(sPath);
		str sTemporary = NULL;
		xfile File;
		bool bOk;
		size_t iAttempt;
		uint64 iDelay = 1u;
		xerror* pRenameError = NULL;
		if(sDirectory == NULL)
			return false;
		File = xrtFileTemp(sDirectory,
			".xacme-current-", ".tmp", &sTemporary);
		xrtFree(sDirectory);
		if(File == NULL)
			return false;
		bOk = xrtWriteFull(File, sText, strlen(sText), NULL) &&
			xrtFlush(File);
		if(!xrtClose(File))
			bOk = false;
		if(bOk)
		{
			*pbPublishAttempted = true;
			/* 读者或文件扫描器的短暂占用可能超过一轮线程调度。
			 * 仅重试原生占用错误，累计等待最多 447 毫秒。 */
			for(iAttempt = 0u; iAttempt < 32u; iAttempt++)
			{
				int64 iSystemCode;
				if(xrtPathRename(sTemporary, sPath, true))
				{
					xrtErrorFree(pRenameError);
					xrtFree(sTemporary);
					return true;
				}
				if(pRenameError == NULL)
					pRenameError = xrtErrorRef(xrtGetError());
				iSystemCode = xrtErrorSystemCode(xrtGetError());
				if(iAttempt == 31u ||
					(xrtErrorKind(xrtGetError()) != XERR_IO &&
						xrtErrorKind(xrtGetError()) != XERR_PERMISSION &&
						xrtErrorKind(xrtGetError()) != XERR_AGAIN) ||
					(iSystemCode != ERROR_ACCESS_DENIED &&
						iSystemCode != ERROR_SHARING_VIOLATION &&
						iSystemCode != ERROR_LOCK_VIOLATION))
					break;
				xrtClearError();
				xrtSleep(iDelay);
				if(iDelay < 16u)
					iDelay *= 2u;
			}
		}
		{
			xerror* pCause = xrtTakeError();
			if(pRenameError != NULL)
			{
				xrtErrorFree(pCause);
				pCause = pRenameError;
			}
			(void)xrtFileDelete(sTemporary);
			xrtClearError();
			if(pCause != NULL)
				xrtSetErrorTake(pCause);
		}
		xrtFree(sTemporary);
		return false;
	#else
		return xacmeStoreWriteAtomicText(sPath, sText);
	#endif
}

static void xacmeStoreReleasePointerLock(xfile File)
{
	xerror* pCause = xrtTakeError();
	(void)xrtFileUnlock(File);
	(void)xrtClose(File);
	xrtClearError();
	if(pCause != NULL)
		xrtSetErrorTake(pCause);
}

static str xacmeStoreReadText(cstr sPath, size_t iLimit,
	bool bSecret, cstr sMissing)
{
	size_t iSize = 0u;
	bytes pBytes = xrtFileReadAllLimit(sPath, iLimit, &iSize);
	str sText;
	if(pBytes == NULL)
	{
		if((sMissing != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND))
			xacmeStoreError(XERR_NOT_FOUND,
				XACME_STORE_ERROR_NOT_FOUND, sMissing);
		return NULL;
	}
	sText = (str)xrtMalloc(iSize + 1u);
	if(sText != NULL)
	{
		memcpy(sText, pBytes, iSize);
		sText[iSize] = '\0';
	}
	if(bSecret)
		xrtSecureZero(pBytes, iSize);
	xrtFree(pBytes);
	return sText;
}

bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem)
{
	char sCa[17];
	char sDirectory[1024];
	char sPath[1024];
	if((sRoot == NULL) || (sRoot[0] == 0) ||
		(sDirectoryUrl == NULL) || (sAccountPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save account requires root, url and pem");
		return false;
	}
	if(strlen(sAccountPem) > XACME_STORE_KEY_LIMIT)
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store account exceeds read limit");
		return false;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return false;
	}
	if(!xacmeStoreFormat(sDirectory, sizeof(sDirectory),
		"%s/accounts/%s", sRoot, sCa))
		return false;
	if(!xrtDirCreateAll(sDirectory))
	{
		return false;
	}
	if(!xacmeStoreSyncAncestors(sDirectory))
		return false;
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/accounts/%s/account.pem", sRoot, sCa))
		return false;
	if(!xacmeStoreWriteAtomicKey(sPath, sAccountPem))
	{
		return false;
	}
	/* 文件已可见；失败时交由调用方重新读取以核对提交状态。 */
	return xacmeStoreSyncDirectory(sDirectory);
}

str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl)
{
	char sCa[17];
	char sPath[1024];
	if((sRoot == NULL) || (sRoot[0] == 0) || (sDirectoryUrl == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load account requires root and url");
		return NULL;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return NULL;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/accounts/%s/account.pem", sRoot, sCa))
		return NULL;
	return xacmeStoreReadText(sPath, XACME_STORE_KEY_LIMIT, true,
		"acme store account not found");
}

bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl)
{
	char sPath[1024];
	char sPointer[1024];
	char sMeta[1200];
	xfileinfo Info;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (sChainPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save cert requires root, domain and chain");
		return false;
	}
	if(strlen(sChainPem) > XACME_STORE_CERT_LIMIT)
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store certificate exceeds read limit");
		return false;
	}
	if(!xacmeStoreCertPath(sPath, sizeof(sPath),
		sRoot, sPrimaryDomain, NULL))
		return false;
	if(!xacmeStoreFormat(sPointer, sizeof(sPointer), "%s/current", sPath))
		return false;
	if(xrtPathStat(sPointer, false, &Info))
	{
		xacmeStoreError(XERR_STATE, XACME_STORE_ERROR_ARGUMENT,
			"acme store standalone cert cannot replace a committed grant");
		return false;
	}
	if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
		return false;
	xrtClearError();
	if(!xrtDirCreateAll(sPath))
	{
		return false;
	}
	if(!xacmeStoreCertPath(sPath, sizeof(sPath),
		sRoot, sPrimaryDomain, "fullchain.pem"))
		return false;
	if(!xacmeStoreWriteAtomicText(sPath, sChainPem))
	{
		return false;
	}
	if(!xacmeStoreFormat(sMeta, sizeof(sMeta), "directory=%s\n",
		(sDirectoryUrl != NULL) ? sDirectoryUrl : "") ||
		!xacmeStoreCertPath(sPath, sizeof(sPath),
			sRoot, sPrimaryDomain, "meta.txt"))
		return false;
	if(!xacmeStoreWriteAtomicText(sPath, sMeta))
	{
		return false;
	}
	return true;
}

str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	char sBase[1024];
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load cert requires root and domain");
		return NULL;
	}
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/fullchain.pem", sBase))
		return NULL;
	(void)bVersioned;
	return xacmeStoreReadText(sPath, XACME_STORE_CERT_LIMIT, false,
		"acme store cert not found");
}

str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	char sBase[1024];
	size_t iSize = 0u;
	bytes pBytes;
	str sCa = NULL;
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		return NULL;
	}
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath), "%s/meta.txt", sBase))
		return NULL;
	(void)bVersioned;
	pBytes = xrtFileReadAllLimit(sPath, XACME_STORE_META_LIMIT, &iSize);
	if((pBytes == NULL) || (iSize <= 11u) ||
		(memcmp(pBytes, "directory=", 10u) != 0))
	{
		xrtFree(pBytes);
		return NULL;
	}
	/* 去掉末尾换行。 */
	while((iSize > 0u) &&
		((pBytes[iSize - 1u] == '\n') || (pBytes[iSize - 1u] == '\r')))
	{
		iSize--;
	}
	sCa = (str)xrtMalloc(iSize - 10u + 1u);
	if(sCa != NULL)
	{
		memcpy(sCa, pBytes + 10u, iSize - 10u);
		sCa[iSize - 10u] = '\0';
	}
	xrtFree(pBytes);
	return sCa;
}

static void xacmeStoreDiscardGeneration(cstr sGeneration)
{
	static const cstr sFiles[] = {
		"fullchain.pem", "key.pem", "meta.txt"
	};
	xerror* pCause = xrtTakeError();
	char sPath[1024];
	size_t i;
	for(i = 0u; i < sizeof(sFiles) / sizeof(sFiles[0]); i++)
	{
		if(xacmeStoreFormat(sPath, sizeof(sPath), "%s/%s",
			sGeneration, sFiles[i]))
			(void)xrtFileDelete(sPath);
	}
	(void)xrtDirRemove(sGeneration);
	xrtClearError();
	if(pCause != NULL)
		xrtSetErrorTake(pCause);
}

bool xrtAcmeStoreSaveGrant(
	cstr sRoot, cstr sPrimaryDomain, const xacmeissuegrant* pGrant,
	cstr sDirectoryUrl)
{
	char sBase[1024];
	char sProbe[1024];
	char sPath[1024];
	char sPointer[1024];
	char sLockPath[1024];
	char sPointerText[XACME_STORE_GENERATION_SIZE + 2u];
	char sMeta[1200];
	str sGeneration;
	xfile Lock;
	const char* sName;
	bool bPublishAttempted;
	size_t iSize;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pGrant == NULL) ||
		(pGrant->sFullchainPem == NULL) || (pGrant->sKeyPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save grant requires root, domain, chain and key");
		return false;
	}
	if((strlen(pGrant->sFullchainPem) > XACME_STORE_CERT_LIMIT) ||
		(strlen(pGrant->sKeyPem) > XACME_STORE_KEY_LIMIT))
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store grant exceeds read limits");
		return false;
	}
	if(!xacmeStoreCertPath(sBase, sizeof(sBase),
		sRoot, sPrimaryDomain, NULL) ||
		!xacmeStoreFormat(sProbe, sizeof(sProbe),
			"%s/.grant-0000000000000000/fullchain.pem", sBase) ||
		!xacmeStoreFormat(sPointer, sizeof(sPointer),
			"%s/current", sBase) ||
		!xacmeStoreFormat(sLockPath, sizeof(sLockPath),
			"%s/current.lock", sBase) ||
		!xacmeStoreFormat(sMeta, sizeof(sMeta), "directory=%s\n",
			(sDirectoryUrl != NULL) ? sDirectoryUrl : ""))
		return false;
	if(!xrtDirCreateAll(sBase))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant directory creation failed");
		return false;
	}
	sGeneration = xrtDirTemp(sBase, XACME_STORE_GENERATION_PREFIX, "");
	if(sGeneration == NULL)
		return false;
	iSize = strlen(sGeneration);
	sName = (iSize > XACME_STORE_GENERATION_SIZE) ?
		sGeneration + iSize - XACME_STORE_GENERATION_SIZE : NULL;
	if((sName == NULL) ||
		(sName[-1] != '/' && sName[-1] != '\\') ||
		!xacmeStoreGenerationNameValid(sName,
			XACME_STORE_GENERATION_SIZE))
	{
		xacmeStoreError(XERR_INTERNAL, XACME_STORE_ERROR_PARSE,
			"acme store temporary generation name is invalid");
		goto Fail;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/fullchain.pem", sGeneration) ||
		!xacmeStoreWriteAtomicText(sPath, pGrant->sFullchainPem) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/key.pem", sGeneration) ||
		!xacmeStoreWriteAtomicKey(sPath, pGrant->sKeyPem) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/meta.txt", sGeneration) ||
		!xacmeStoreWriteAtomicText(sPath, sMeta))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant staging failed");
		goto Fail;
	}
	if(!xacmeStoreSyncDirectory(sGeneration) ||
		!xacmeStoreSyncAncestors(sBase))
		goto Fail;
	/* 同域的跨进程写者串行发布；读者始终无锁读取固定版本。 */
	Lock = xrtOpen(sLockPath, XFILE_READ | XFILE_WRITE | XFILE_CREATE);
	if(Lock == NULL)
		goto Fail;
	if(!xrtFileLock(Lock, XFILE_LOCK_EXCLUSIVE, true))
	{
		xerror* pCause = xrtTakeError();
		(void)xrtClose(Lock);
		xrtClearError();
		if(pCause != NULL)
			xrtSetErrorTake(pCause);
		goto Fail;
	}
	memcpy(sPointerText, sName, XACME_STORE_GENERATION_SIZE);
	sPointerText[XACME_STORE_GENERATION_SIZE] = '\n';
	sPointerText[XACME_STORE_GENERATION_SIZE + 1u] = '\0';
	if(!xacmeStoreWritePointer(sPointer, sPointerText,
		&bPublishAttempted))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant commit failed");
		xacmeStoreReleasePointerLock(Lock);
		/* Windows 重命名失败的可见性可能不确定，不能删除它可能引用的版本。 */
		if(bPublishAttempted)
		{
			xrtFree(sGeneration);
			return false;
		}
		goto Fail;
	}
	if(!xrtFileUnlock(Lock))
	{
		xerror* pCause = xrtTakeError();
		(void)xrtClose(Lock);
		xrtClearError();
		if(pCause != NULL)
			xrtSetErrorTake(pCause);
		xrtFree(sGeneration);
		return false;
	}
	if(!xrtClose(Lock))
	{
		xrtFree(sGeneration);
		return false;
	}
	/* 指针已可见；即使目录 fsync 失败也不得删除它所指向的版本。 */
	if(!xacmeStoreSyncDirectory(sBase))
	{
		xrtFree(sGeneration);
		return false;
	}
	xrtFree(sGeneration);
	return true;
Fail:
	xacmeStoreDiscardGeneration(sGeneration);
	xrtFree(sGeneration);
	return false;
}

bool xrtAcmeStoreLoadGrant(
	cstr sRoot, cstr sPrimaryDomain, xacmeissuegrant* pOut)
{
	char sPath[1024];
	char sBase[1024];
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pOut == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load grant requires root, domain and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/fullchain.pem", sBase))
		return false;
	(void)bVersioned;
	pOut->sFullchainPem = xacmeStoreReadText(sPath,
		XACME_STORE_CERT_LIMIT, false, "acme store cert not found");
	if(pOut->sFullchainPem == NULL)
	{
		return false;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath), "%s/key.pem", sBase))
	{
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	pOut->sKeyPem = xacmeStoreReadText(sPath,
		XACME_STORE_KEY_LIMIT, true, "acme store key not found");
	if(pOut->sKeyPem == NULL)
	{
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	return true;
}

bool xrtAcmeStoreListDomains(
	cstr sRoot, char (*sOutDomains)[256],
	size_t iCapacity, size_t* pOutCount)
{
	char sPath[1024];
	xdir Dir;
	if((sRoot == NULL) || (sOutDomains == NULL) || (pOutCount == NULL) ||
		(iCapacity == 0u))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store list requires root, output and capacity");
		return false;
	}
	*pOutCount = 0u;
	if((sRoot[0] == 0) ||
		!xacmeStoreFormat(sPath, sizeof(sPath), "%s/certs", sRoot))
		return false;
	Dir = xrtDirOpen(sPath, XDIR_STAT);
	if(!Dir)
	{
		if(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND)
			xacmeStoreError(XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
				"acme store certs dir not found");
		return false;
	}
	for(;;)
	{
		xdirentry Entry;
		xdirnext eNext = xrtDirNext(Dir, &Entry);
		char sDomain[256];
		size_t iDomainSize;
		size_t i;
		if(eNext == XDIR_NEXT_END)
		{
			break;
		}
		if(eNext != XDIR_NEXT_ITEM)
		{
			xerror* pCause = xrtTakeError();
			(void)xrtDirClose(Dir);
			xrtClearError();
			if(pCause != NULL) xrtSetErrorTake(pCause);
			return false;
		}
		if((Entry.Name.Size == 0u) || (Entry.Name.Size >= 256u) ||
			(Entry.Info.Type != XFILE_TYPE_DIRECTORY))
		{
			continue;
		}
		if((Entry.Name.Size >= 4u) &&
			(memcmp(Entry.Name.Data, "%2A.", 4u) == 0))
		{
			iDomainSize = Entry.Name.Size - 2u;
			sDomain[0] = '*';
			memcpy(sDomain + 1u,
				Entry.Name.Data + 3u, Entry.Name.Size - 3u);
		}
		else
		{
			iDomainSize = Entry.Name.Size;
			memcpy(sDomain, Entry.Name.Data, iDomainSize);
		}
		sDomain[iDomainSize] = '\0';
		if((strlen(sDomain) != iDomainSize) ||
			!xacmeStoreDomainValid(sDomain))
			continue;
		for(i = 0u; i < *pOutCount; i++)
			if(strcmp(sOutDomains[i], sDomain) == 0)
				break;
		if(i < *pOutCount)
			continue;
		if(*pOutCount >= iCapacity)
		{
			xrtDirClose(Dir);
			xacmeStoreError(
				XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
				"acme store list capacity exhausted");
			return false;
		}
		memcpy(sOutDomains[*pOutCount], sDomain, iDomainSize + 1u);
		(*pOutCount)++;
	}
	xrtDirClose(Dir);
	return true;
}

bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed)
{
	str sChain;
	xpemcursor Pem;
	xpemblock Block;
	size_t iDerSize = 0u;
	bytes pDer;
	xx509cert Cert;
	xtime Deadline;
	bool bNeed = true;
	bool bOk = false;

	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pbNeed == NULL) ||
		(iRenewalDays < 0))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store need renew requires root, domain and days");
		return false;
	}
	*pbNeed = true;

	sChain = xrtAcmeStoreLoadCert(sRoot, sPrimaryDomain);
	if(sChain == NULL)
	{
		if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
		{
			/* 读故障不是"缺证书"：如实失败，避免误触重签。 */
			return false;
		}
		return true; /* 缺证书即需要签发。 */
	}
	if(!xrtPemInit(&Pem, sChain, strlen(sChain))) goto Done;
	xpemresult Next = xrtPemRead(&Pem, &Block);
	if(Next == XPEM_ERROR) goto Done;
	if(Next != XPEM_BLOCK)
	{
		xacmeStoreError(
			XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store chain has no pem block");
		goto Done;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if(pDer == NULL)
	{
		goto Done;
	}
	if(!xrtX509Parse(pDer, iDerSize, &Cert))
	{
		xrtFree(pDer);
		goto Done;
	}
	xrtFree(pDer);
	if(!xrtTimeAdd(
			xrtNow(), (int64)iRenewalDays, XTIME_UNIT_DAY, &Deadline))
	{
		goto Done;
	}
	/* notAfter 早于续签线 → 需要续。 */
	bNeed = (Cert.NotAfter < Deadline);
	bOk = true;

Done:
	xrtFree(sChain);
	if(bOk)
	{
		*pbNeed = bNeed;
	}
	return bOk;
}

#endif
