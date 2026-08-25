#include "../internal/xrt_x509.h"

#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && defined(__APPLE__)
	#include <CoreFoundation/CoreFoundation.h>
	#include <Security/Security.h>
	#include <dlfcn.h>
#endif



#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && defined(__APPLE__)

typedef OSStatus (*xrt_sec_copy_anchors)(CFArrayRef* pAnchors);
typedef CFDataRef (*xrt_sec_copy_data)(SecCertificateRef Certificate);
typedef CFIndex (*xrt_cf_array_count)(CFArrayRef Array);
typedef const void* (*xrt_cf_array_value)(CFArrayRef Array, CFIndex iIndex);
typedef CFIndex (*xrt_cf_data_length)(CFDataRef Data);
typedef const UInt8* (*xrt_cf_data_bytes)(CFDataRef Data);
typedef void (*xrt_cf_release)(CFTypeRef Object);



typedef struct xrt_x509_macos_api {
	void* Security;
	void* CoreFoundation;
	xrt_sec_copy_anchors CopyAnchors;
	xrt_sec_copy_data CopyData;
	xrt_cf_array_count ArrayCount;
	xrt_cf_array_value ArrayValue;
	xrt_cf_data_length DataLength;
	xrt_cf_data_bytes DataBytes;
	xrt_cf_release Release;
} xrt_x509_macos_api;



/* 以函数指针大小安全的方式解析一个 framework 符号。 */
static bool __xrtX509StoreMacSymbol(
	void* pLibrary,
	cstr sName,
	void* pFunction,
	size_t iSize
)
{
	void* pSymbol = dlsym(pLibrary, sName);

	if ( pSymbol == NULL ) {
		return false;
	}
	memcpy(pFunction, &pSymbol, iSize);
	return true;
}



/* 动态解析 macOS 系统锚枚举和 CoreFoundation 只读访问函数。 */
static bool __xrtX509StoreMacApi(xrt_x509_macos_api* pApi)
{
	memset(pApi, 0, sizeof(*pApi));
	pApi->Security = dlopen(
		"/System/Library/Frameworks/Security.framework/Security",
		RTLD_LAZY | RTLD_LOCAL
	);
	pApi->CoreFoundation = dlopen(
		"/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
		RTLD_LAZY | RTLD_LOCAL
	);
	if ( (pApi->Security == NULL) || (pApi->CoreFoundation == NULL) ) {
		if ( pApi->Security != NULL ) {
			dlclose(pApi->Security);
		}
		if ( pApi->CoreFoundation != NULL ) {
			dlclose(pApi->CoreFoundation);
		}
		__xrtX509StoreSystemError(
			XERR_IO, 0, "macOS security frameworks loading failed", NULL
		);
		return false;
	}
	if ( !__xrtX509StoreMacSymbol(
		pApi->Security, "SecTrustCopyAnchorCertificates",
		&pApi->CopyAnchors, sizeof(pApi->CopyAnchors)
	) || !__xrtX509StoreMacSymbol(
		pApi->Security, "SecCertificateCopyData",
		&pApi->CopyData, sizeof(pApi->CopyData)
	) || !__xrtX509StoreMacSymbol(
		pApi->CoreFoundation, "CFArrayGetCount",
		&pApi->ArrayCount, sizeof(pApi->ArrayCount)
	) || !__xrtX509StoreMacSymbol(
		pApi->CoreFoundation, "CFArrayGetValueAtIndex",
		&pApi->ArrayValue, sizeof(pApi->ArrayValue)
	) || !__xrtX509StoreMacSymbol(
		pApi->CoreFoundation, "CFDataGetLength",
		&pApi->DataLength, sizeof(pApi->DataLength)
	) || !__xrtX509StoreMacSymbol(
		pApi->CoreFoundation, "CFDataGetBytePtr",
		&pApi->DataBytes, sizeof(pApi->DataBytes)
	) || !__xrtX509StoreMacSymbol(
		pApi->CoreFoundation, "CFRelease",
		&pApi->Release, sizeof(pApi->Release)
	) ) {
		dlclose(pApi->Security);
		dlclose(pApi->CoreFoundation);
		memset(pApi, 0, sizeof(*pApi));
		__xrtX509StoreSystemError(
			XERR_UNSUPPORTED, 0,
			"required macOS trust APIs are unavailable", NULL
		);
		return false;
	}
	return true;
}



/* 关闭动态加载的 macOS framework 句柄。 */
static void __xrtX509StoreMacClose(xrt_x509_macos_api* pApi)
{
	dlclose(pApi->Security);
	dlclose(pApi->CoreFoundation);
	memset(pApi, 0, sizeof(*pApi));
}



/* 从 macOS 默认锚集合复制每张证书的 DER 到 XRT 信任库。 */
bool __xrtX509StoreSystemLoad(xx509store* pStore)
{
	xrt_x509_macos_api Api;
	CFArrayRef Anchors = NULL;
	CFIndex iCount;
	OSStatus Status;

	if ( !__xrtX509StoreMacApi(&Api) ) {
		return false;
	}
	Status = Api.CopyAnchors(&Anchors);
	if ( (Status != errSecSuccess) || (Anchors == NULL) ) {
		__xrtX509StoreMacClose(&Api);
		__xrtX509StoreSystemError(
			XERR_IO, (int32)Status,
			"macOS system anchor query failed", NULL
		);
		return false;
	}
	iCount = Api.ArrayCount(Anchors);
	if ( iCount <= 0 ) {
		Api.Release(Anchors);
		__xrtX509StoreMacClose(&Api);
		__xrtX509StoreSystemError(
			XERR_NOT_FOUND, 0,
			"macOS system anchor set is empty", NULL
		);
		return false;
	}
	for ( CFIndex i = 0; i < iCount; i++ ) {
		SecCertificateRef Certificate = (SecCertificateRef)
			Api.ArrayValue(Anchors, i);
		CFDataRef Data;
		const UInt8* pBytes;
		CFIndex iSize;
		xx509result Result;

		if ( Certificate == NULL ) {
			Api.Release(Anchors);
			__xrtX509StoreMacClose(&Api);
			__xrtX509StoreSystemError(
				XERR_PROTOCOL, 0,
				"macOS anchor array contains an empty item", NULL
			);
			return false;
		}
		Data = Api.CopyData(Certificate);
		if ( Data == NULL ) {
			Api.Release(Anchors);
			__xrtX509StoreMacClose(&Api);
			__xrtX509StoreSystemError(
				XERR_IO, 0,
				"macOS anchor DER export failed", NULL
			);
			return false;
		}
		iSize = Api.DataLength(Data);
		if ( (iSize <= 0) || ((uint64)iSize > (uint64)SIZE_MAX) ) {
			Api.Release(Data);
			Api.Release(Anchors);
			__xrtX509StoreMacClose(&Api);
			__xrtX509StoreSystemError(
				XERR_RANGE, 0,
				"macOS anchor DER length is invalid", NULL
			);
			return false;
		}
		pBytes = Api.DataBytes(Data);
		if ( pBytes == NULL ) {
			Api.Release(Data);
			Api.Release(Anchors);
			__xrtX509StoreMacClose(&Api);
			__xrtX509StoreSystemError(
				XERR_PROTOCOL, 0,
				"macOS anchor DER has no byte storage", NULL
			);
			return false;
		}
		Result = xrtX509StoreAdd(
			pStore, pBytes, (size_t)iSize
		);
		Api.Release(Data);
		if ( Result == X509_ERROR ) {
			/* 与 Windows 系统导入一致：跳过被严格策略拒绝的存量锚；
			   OOM 等资源错误照常失败。 */
			if ( xrtErrorKind(xrtGetError()) == XERR_MEMORY ) {
				Api.Release(Anchors);
				__xrtX509StoreMacClose(&Api);
				__xrtX509StoreSystemFailure(
					"macOS system anchor import failed"
				);
				return false;
			}
			xrtClearError();
			continue;
		}
	}
	Api.Release(Anchors);
	__xrtX509StoreMacClose(&Api);
	return true;
}

#endif
