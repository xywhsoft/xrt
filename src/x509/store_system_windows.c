#include "../internal/xrt_x509.h"

#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	#include <winapi/wincrypt.h>
#elif defined(_WIN32) || defined(_WIN64)
	#include <wincrypt.h>
#endif



#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && \
	(defined(_WIN32) || defined(_WIN64))

typedef HCERTSTORE (WINAPI *xrt_cert_open_store)(
	LPCSTR pProvider,
	DWORD iEncoding,
	ULONG_PTR iProvider,
	DWORD iFlags,
	const void* pParameter
);

typedef PCCERT_CONTEXT (WINAPI *xrt_cert_enum)(
	HCERTSTORE Store,
	PCCERT_CONTEXT pPrevious
);

typedef BOOL (WINAPI *xrt_cert_free)(PCCERT_CONTEXT pCertificate);

typedef BOOL (WINAPI *xrt_cert_close)(HCERTSTORE Store, DWORD iFlags);



typedef struct xrt_x509_windows_api {
	HMODULE Library;
	xrt_cert_open_store Open;
	xrt_cert_enum Next;
	xrt_cert_free Free;
	xrt_cert_close Close;
} xrt_x509_windows_api;



/* 从 crypt32.dll 解析信任库枚举所需的最小 API 集合。 */
static bool __xrtX509StoreWindowsApi(xrt_x509_windows_api* pApi)
{
	FARPROC pOpen;
	FARPROC pNext;
	FARPROC pFree;
	FARPROC pClose;
	DWORD iCode;

	memset(pApi, 0, sizeof(*pApi));
	pApi->Library = LoadLibraryA("crypt32.dll");
	if ( pApi->Library == NULL ) {
		iCode = GetLastError();
		__xrtX509StoreSystemError(
			XERR_IO, (int32)iCode, "crypt32.dll loading failed", NULL
		);
		return false;
	}
	pOpen = GetProcAddress(pApi->Library, "CertOpenStore");
	pNext = GetProcAddress(pApi->Library, "CertEnumCertificatesInStore");
	pFree = GetProcAddress(pApi->Library, "CertFreeCertificateContext");
	pClose = GetProcAddress(pApi->Library, "CertCloseStore");
	if ( (pOpen == NULL) || (pNext == NULL) ||
		(pFree == NULL) || (pClose == NULL) ) {
		iCode = GetLastError();
		FreeLibrary(pApi->Library);
		memset(pApi, 0, sizeof(*pApi));
		__xrtX509StoreSystemError(
			XERR_UNSUPPORTED, (int32)iCode,
			"required Windows certificate APIs are unavailable", NULL
		);
		return false;
	}
	memcpy(&pApi->Open, &pOpen, sizeof(pApi->Open));
	memcpy(&pApi->Next, &pNext, sizeof(pApi->Next));
	memcpy(&pApi->Free, &pFree, sizeof(pApi->Free));
	memcpy(&pApi->Close, &pClose, sizeof(pApi->Close));
	return true;
}



/* 关闭一个已打开的 Windows 证书逻辑库。 */
static bool __xrtX509StoreWindowsClose(
	const xrt_x509_windows_api* pApi,
	HCERTSTORE Store
)
{
	if ( pApi->Close(Store, 0) ) {
		return true;
	}
	__xrtX509StoreSystemError(
		XERR_IO, (int32)GetLastError(),
		"Windows ROOT store closing failed", NULL
	);
	return false;
}



/* 枚举一个 Windows ROOT 逻辑库，并直接导入原始 DER。 */
static bool __xrtX509StoreWindowsLocation(
	xx509store* pStore,
	const xrt_x509_windows_api* pApi,
	DWORD iLocation,
	bool* pFound
)
{
	HCERTSTORE Store;
	PCCERT_CONTEXT pCertificate = NULL;
	DWORD iCode;

	Store = pApi->Open(
		CERT_STORE_PROV_SYSTEM_A, 0, 0,
		CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | iLocation,
		"ROOT"
	);
	if ( Store == NULL ) {
		__xrtX509StoreSystemError(
			XERR_IO, (int32)GetLastError(),
			"Windows ROOT store opening failed", NULL
		);
		return false;
	}
	SetLastError(ERROR_SUCCESS);
	while ( (pCertificate = pApi->Next(Store, pCertificate)) != NULL ) {
		xx509result Result;

		if ( (pCertificate->pbCertEncoded == NULL) ||
			(pCertificate->cbCertEncoded == 0) ) {
			pApi->Free(pCertificate);
			(void)pApi->Close(Store, 0);
			__xrtX509StoreSystemError(
				XERR_PROTOCOL, 0,
				"Windows ROOT store contains an empty certificate", NULL
			);
			return false;
		}
		Result = xrtX509StoreAdd(
			pStore, pCertificate->pbCertEncoded,
			(size_t)pCertificate->cbCertEncoded
		);
		if ( Result == X509_ERROR ) {
			/* OS 信任库可能包含不完全符合 RFC 5280 的存量根证书
			   （如 NameConstraints 未标记 critical）。系统导入只跳过
			   被严格策略拒绝的单张证书；OOM 等资源错误必须照常失败，
			   保持 OOM 注入下的失败原子性。 */
			if ( xrtErrorKind(xrtGetError()) == XERR_MEMORY ) {
				pApi->Free(pCertificate);
				(void)pApi->Close(Store, 0);
				__xrtX509StoreSystemFailure(
					"Windows ROOT certificate import failed"
				);
				return false;
			}
			xrtClearError();
			continue;
		}
		*pFound = true;
	}
	iCode = GetLastError();
	if ( (iCode != (DWORD)CRYPT_E_NOT_FOUND) &&
		(iCode != ERROR_NO_MORE_FILES) ) {
		(void)pApi->Close(Store, 0);
		__xrtX509StoreSystemError(
			XERR_IO, (int32)iCode,
			"Windows ROOT store enumeration failed", NULL
		);
		return false;
	}
	return __xrtX509StoreWindowsClose(pApi, Store);
}



/* 导入当前用户和本机两个 Windows ROOT 逻辑库。 */
bool __xrtX509StoreSystemLoad(xx509store* pStore)
{
	static const DWORD Locations[] = {
		CERT_SYSTEM_STORE_CURRENT_USER,
		CERT_SYSTEM_STORE_LOCAL_MACHINE
	};
	xrt_x509_windows_api Api;
	bool bFound = false;
	bool bSuccess = true;

	if ( !__xrtX509StoreWindowsApi(&Api) ) {
		return false;
	}
	for ( size_t i = 0; i < sizeof(Locations) / sizeof(Locations[0]); i++ ) {
		if ( !__xrtX509StoreWindowsLocation(
			pStore, &Api, Locations[i], &bFound
		) ) {
			bSuccess = false;
			break;
		}
	}
	FreeLibrary(Api.Library);
	if ( !bSuccess ) {
		return false;
	}
	if ( !bFound ) {
		__xrtX509StoreSystemError(
			XERR_NOT_FOUND, 0,
			"Windows ROOT stores contain no certificates", NULL
		);
		return false;
	}
	return true;
}

#endif
