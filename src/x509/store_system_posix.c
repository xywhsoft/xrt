#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && \
	!defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)

/* 判断一个名称是否符合 OpenSSL 证书目录的八位哈希加序号格式。 */
static bool __xrtX509StoreSystemHashName(xstrview Name)
{
	size_t i;

	if ( Name.Size < 10u ) {
		return false;
	}
	for ( i = 0; i < 8u; i++ ) {
		uint8 iValue = (uint8)Name.Data[i];

		if ( !((iValue >= '0') && (iValue <= '9')) &&
			!((iValue >= 'a') && (iValue <= 'f')) &&
			!((iValue >= 'A') && (iValue <= 'F')) ) {
			return false;
		}
	}
	if ( Name.Data[8] != '.' ) {
		return false;
	}
	for ( i = 9u; i < Name.Size; i++ ) {
		if ( (Name.Data[i] < '0') || (Name.Data[i] > '9') ) {
			return false;
		}
	}
	return true;
}



/* 在错误清理路径关闭目录，同时保留原始错误对象。 */
static void __xrtX509StoreSystemDirCleanup(xdir Dir)
{
	xerror* pError = xrtTakeError();

	(void)xrtDirClose(Dir);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 导入一个 OpenSSL 风格哈希目录中的所有证书。 */
static bool __xrtX509StoreSystemDirectory(
	xx509store* pStore,
	cstr sPath,
	bool* pFound
)
{
	xdir Dir = xrtDirOpen(sPath, XDIR_STAT | XDIR_FOLLOW_LINKS);
	xdirentry Entry;
	xdirnext Next;

	if ( Dir == NULL ) {
		return false;
	}
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		str sCertificate;

		if ( (Entry.Info.Type != XFILE_TYPE_FILE) ||
			!__xrtX509StoreSystemHashName(Entry.Name) ) {
			continue;
		}
		sCertificate = xrtDirEntryPath(Dir, &Entry);
		if ( sCertificate == NULL ) {
			__xrtX509StoreSystemDirCleanup(Dir);
			return false;
		}
		if ( !xrtX509StoreAddFile(pStore, sCertificate, NULL) ) {
			xrtFree(sCertificate);
			__xrtX509StoreSystemDirCleanup(Dir);
			return false;
		}
		xrtFree(sCertificate);
		*pFound = true;
	}
	if ( Next == XDIR_NEXT_ERROR ) {
		__xrtX509StoreSystemDirCleanup(Dir);
		return false;
	}
	return xrtDirClose(Dir);
}



/* 导入 SSL_CERT_DIR 使用冒号分隔的显式目录列表。 */
static bool __xrtX509StoreSystemDirectoryList(
	xx509store* pStore,
	cstr sList,
	bool* pFound
)
{
	size_t iSize = strlen(sList);
	size_t iStart = 0;

	while ( iStart <= iSize ) {
		size_t iEnd = iStart;

		while ( (iEnd < iSize) && (sList[iEnd] != ':') ) {
			iEnd++;
		}
		if ( iEnd != iStart ) {
			str sPath = xrtStrDupN(sList + iStart, iEnd - iStart);

			if ( sPath == NULL ) {
				return false;
			}
			if ( !xrtDirExists(sPath) ) {
				xrtFree(sPath);
				__xrtX509StoreSystemError(
					XERR_NOT_FOUND, 0,
					"SSL_CERT_DIR contains a missing directory", NULL
				);
				return false;
			}
			if ( !__xrtX509StoreSystemDirectory(pStore, sPath, pFound) ) {
				xrtFree(sPath);
				return false;
			}
			xrtFree(sPath);
		}
		if ( iEnd == iSize ) {
			break;
		}
		iStart = iEnd + 1u;
	}
	return true;
}



/* 按 Unix 环境覆盖和主流系统默认位置装载信任锚。 */
bool __xrtX509StoreSystemLoad(xx509store* pStore)
{
	static const char* Files[] = {
		"/etc/ssl/certs/ca-certificates.crt",
		"/etc/pki/tls/certs/ca-bundle.crt",
		"/etc/ssl/ca-bundle.pem",
		"/etc/pki/tls/cacert.pem",
		"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
		"/etc/ssl/cert.pem"
	};
	static const char* Directories[] = {
		"/etc/ssl/certs",
		"/etc/pki/tls/certs",
		"/system/etc/security/cacerts",
		"/data/misc/keychain/certs-added"
	};
	cstr sFile = getenv("SSL_CERT_FILE");
	cstr sDirectory = getenv("SSL_CERT_DIR");
	bool bFound = false;

	if ( (sFile != NULL) && (sFile[0] != 0) ) {
		if ( !xrtX509StoreAddFile(pStore, sFile, NULL) ) {
			__xrtX509StoreSystemFailure(
				"SSL_CERT_FILE trust store import failed"
			);
			return false;
		}
		bFound = true;
	} else {
		for ( size_t i = 0; i < sizeof(Files) / sizeof(Files[0]); i++ ) {
			if ( !xrtFileExists(Files[i]) ) {
				continue;
			}
			if ( !xrtX509StoreAddFile(pStore, Files[i], NULL) ) {
				__xrtX509StoreSystemFailure(
					"Unix system certificate bundle import failed"
				);
				return false;
			}
			bFound = true;
			break;
		}
	}
	if ( (sDirectory != NULL) && (sDirectory[0] != 0) ) {
		if ( !__xrtX509StoreSystemDirectoryList(
			pStore, sDirectory, &bFound
		) ) {
			if ( xrtErrorFind(
				xrtGetError(), "xrt.x509", X509_ERROR_TRUST_STORE_SYSTEM
			) == NULL ) {
				__xrtX509StoreSystemFailure(
					"SSL_CERT_DIR trust store import failed"
				);
			}
			return false;
		}
	} else {
		for ( size_t i = 0;
			i < sizeof(Directories) / sizeof(Directories[0]); i++ ) {
			if ( !xrtDirExists(Directories[i]) ) {
				continue;
			}
			if ( !__xrtX509StoreSystemDirectory(
				pStore, Directories[i], &bFound
			) ) {
				__xrtX509StoreSystemFailure(
					"Unix system certificate directory import failed"
				);
				return false;
			}
		}
	}
	if ( !bFound ) {
		__xrtX509StoreSystemError(
			XERR_NOT_FOUND, 0,
			"no Unix system trust source was found", NULL
		);
		return false;
	}
	return true;
}

#endif
