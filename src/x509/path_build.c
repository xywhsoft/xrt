#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PATH_BUILD)

#define XRT_X509_PATH_LOCAL_FRAMES 16u



typedef struct xrt_x509_path_frame {
	size_t NextIssuer;
	bool AnchorsTried;
} xrt_x509_path_frame;



/* 保存第一项候选失败，避免回溯过程泄漏临时错误对象。 */
static void __xrtX509PathBuildCapture(xerror** ppError)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		return;
	}
	if ( *ppError == NULL ) {
		*ppError = pError;
	} else {
		xrtErrorFree(pError);
	}
}



/* 设置建链失败并接管后释放可选原因引用。 */
static bool __xrtX509PathBuildError(
	xerrkind Kind,
	cstr sMessage,
	xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_PATH_BUILD, "x509-path-build",
		sMessage, SIZE_MAX, pCause
	);
	xrtErrorFree(pCause);
	return false;
}



/* 验证建链输入中的借用数组和对象视图。 */
static bool __xrtX509PathBuildArguments(
	const xx509cert* pTarget,
	const xx509pathsource* pSource,
	const xx509pathconfig* pConfig,
	const xx509cert** ppPath,
	size_t iCapacity,
	xx509pathresult* pResult
)
{
	if ( (pTarget == NULL) || (pSource == NULL) ||
		!__xrtX509PathConfigValid(pConfig) || (ppPath == NULL) ||
		(iCapacity == 0) || (pResult == NULL) ||
		(pTarget->Raw.Data == NULL) || (pTarget->Raw.Size == 0) ||
		((pSource->Issuers == NULL) && (pSource->IssuerCount != 0)) ||
		((const void*)pSource->Issuers == (const void*)ppPath) ||
		(pSource->Anchors == NULL) || (pSource->AnchorCount == 0) ||
		(iCapacity > (SIZE_MAX / sizeof(xrt_x509_path_frame))) ) {
		return false;
	}
	for ( size_t i = 0; i < pSource->IssuerCount; i++ ) {
		const xx509cert* pIssuer = pSource->Issuers[i];

		if ( (pIssuer == NULL) || (pIssuer->Raw.Data == NULL) ||
			(pIssuer->Raw.Size == 0) ) {
			return false;
		}
	}
	for ( size_t i = 0; i < pSource->AnchorCount; i++ ) {
		const xx509anchor* pAnchor = &pSource->Anchors[i];

		if ( (pAnchor->Name.Data == NULL) || (pAnchor->Name.Size == 0) ||
			((pAnchor->Certificate.Data == NULL) &&
			 (pAnchor->Certificate.Size != 0)) ) {
			return false;
		}
	}
	return true;
}



/* 判断候选证书是否已经位于当前回溯路径。 */
static bool __xrtX509PathBuildUsed(
	const xx509cert* const* ppPath,
	size_t iCount,
	const xx509cert* pCandidate
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtX509PathDuplicate(ppPath[i], pCandidate) ) {
			return true;
		}
	}
	return false;
}



/* 在候选发行者和信任锚中确定性回溯构建有效路径。 */
XRT_API bool xrtX509PathBuild(
	const xx509cert* pTarget,
	const xx509pathsource* pSource,
	const xx509pathconfig* pConfig,
	const xx509cert** ppPath,
	size_t iCapacity,
	xx509pathresult* pResult
)
{
	xrt_x509_path_frame Local[XRT_X509_PATH_LOCAL_FRAMES];
	xrt_x509_path_frame* pFrames = Local;
	xerror* pBestError = NULL;
	size_t iDepth = 0;
	bool bTooDeep = false;

	if ( !__xrtX509PathBuildArguments(
		pTarget, pSource, pConfig, ppPath, iCapacity, pResult
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity > XRT_X509_PATH_LOCAL_FRAMES ) {
		pFrames = (xrt_x509_path_frame*)xrtMalloc(
			iCapacity * sizeof(xrt_x509_path_frame)
		);
		if ( pFrames == NULL ) {
			xerror* pCause = xrtTakeError();

			return __xrtX509PathBuildError(
				XERR_MEMORY, "path search frame allocation failed", pCause
			);
		}
	}
	ppPath[0] = pTarget;
	pFrames[0].NextIssuer = 0;
	pFrames[0].AnchorsTried = false;

	while ( true ) {
		xrt_x509_path_frame* pFrame = &pFrames[iDepth];
		const xx509cert* pCurrent = ppPath[iDepth];
		bool bDescended = false;

		/* 优先尝试直接终止到锚，稳定选择最短有效路径。 */
		if ( !pFrame->AnchorsTried ) {
			pFrame->AnchorsTried = true;
			for ( size_t i = 0; i < pSource->AnchorCount; i++ ) {
				const xx509anchor* pAnchor = &pSource->Anchors[i];
				xx509result Match = xrtX509NameEqual(
					pCurrent->Issuer, pAnchor->Name
				);

				if ( Match == X509_ERROR ) {
					__xrtX509PathBuildCapture(&pBestError);
					continue;
				}
				if ( Match != X509_VALUE ) {
					continue;
				}
				if ( xrtX509PathValidate(
					ppPath, iDepth + 1u, pAnchor, pConfig
				) ) {
					xx509pathresult Result;

					Result.Count = iDepth + 1u;
					Result.Anchor = pAnchor;
					*pResult = Result;
					xrtErrorFree(pBestError);
					if ( pFrames != Local ) {
						xrtFree(pFrames);
					}
					return true;
				}
				__xrtX509PathBuildCapture(&pBestError);
			}
		}

		/* 每层按调用方顺序尝试尚未访问的候选发行者。 */
		while ( pFrame->NextIssuer < pSource->IssuerCount ) {
			const xx509cert* pCandidate =
				pSource->Issuers[pFrame->NextIssuer++];
			xx509result Match;

			if ( __xrtX509PathBuildUsed(
				ppPath, iDepth + 1u, pCandidate
			) ) {
				continue;
			}
			Match = xrtX509IssuerMatch(pCurrent, pCandidate);
			if ( Match == X509_ERROR ) {
				__xrtX509PathBuildCapture(&pBestError);
				continue;
			}
			if ( Match != X509_VALUE ) {
				continue;
			}
			if ( iDepth + 1u >= iCapacity ) {
				bTooDeep = true;
				continue;
			}
			iDepth++;
			ppPath[iDepth] = pCandidate;
			pFrames[iDepth].NextIssuer = 0;
			pFrames[iDepth].AnchorsTried = false;
			bDescended = true;
			break;
		}
		if ( bDescended ) {
			continue;
		}
		if ( iDepth == 0 ) {
			break;
		}
		iDepth--;
	}

	if ( pFrames != Local ) {
		xrtFree(pFrames);
	}
	if ( bTooDeep ) {
		return __xrtX509PathBuildError(
			XERR_RANGE, "path output capacity is too small", pBestError
		);
	}
	if ( pBestError != NULL ) {
		return __xrtX509PathBuildError(
			XERR_NOT_FOUND, "no candidate certification path passed validation",
			pBestError
		);
	}
	return __xrtX509PathBuildError(
		XERR_NOT_FOUND, "no issuer chain reaches a trust anchor", NULL
	);
}

#endif
