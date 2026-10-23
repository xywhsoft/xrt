#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PATH_BUILD)

#define XRT_X509_PATH_LOCAL_FRAMES 16u



typedef struct xrt_x509_path_frame {
	size_t NextIssuer;
	bool AnchorsTried;
} xrt_x509_path_frame;



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
static xrt_x509_decision __xrtX509PathSearch(
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
	xrt_x509_decision Best = __xrtX509CheckValid();
	xrt_x509_decision Decision;
	bool bHasDecision = false;
	size_t iDepth = 0;
	bool bTooDeep = false;

	if ( !__xrtX509PathBuildArguments(
		pTarget, pSource, pConfig, ppPath, iCapacity, pResult
	) ) {
		__xrtErrorSetInvalidArgument();
		return __xrtX509CheckError();
	}
	if ( iCapacity > XRT_X509_PATH_LOCAL_FRAMES ) {
		pFrames = (xrt_x509_path_frame*)xrtMalloc(
			iCapacity * sizeof(xrt_x509_path_frame)
		);
		if ( pFrames == NULL ) {
			return __xrtX509CheckError();
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
					Decision = __xrtX509CheckError();
					goto Done;
				}
				if ( Match != X509_VALUE ) {
					continue;
				}
				Decision = __xrtX509PathEvaluate(
					ppPath, iDepth + 1u, pAnchor, pConfig
				);
				if ( Decision.Result == X509_CHECK_VALID ) {
					xx509pathresult Result;

					Result.Count = iDepth + 1u;
					Result.Anchor = pAnchor;
					*pResult = Result;
					goto Done;
				}
				if ( Decision.Result == X509_CHECK_ERROR ) {
					goto Done;
				}
				if ( !bHasDecision ) {
					Best = Decision;
					bHasDecision = true;
				}
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
				Decision = __xrtX509CheckError();
				goto Done;
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

	if ( bTooDeep ) {
		__xrtErrorSetRange();
		Decision = __xrtX509CheckError();
	} else if ( bHasDecision ) {
		Decision = Best;
	} else {
		Decision = __xrtX509Decision(X509_CHECK_REJECTED, X509_ERROR_PATH_BUILD,
			"no issuer chain reaches a trust anchor");
	}
Done:
	if ( pFrames != Local ) {
		xrtFree(pFrames);
	}
	return Decision;
}

XRT_API xx509check xrtX509PathBuildCheck(
	const xx509cert* pTarget, const xx509pathsource* pSource,
	const xx509pathconfig* pConfig, const xx509cert** ppPath,
	size_t iCapacity, xx509pathresult* pResult, xx509pathdecision* pDecision
)
{
	xrt_x509_decision Decision;
	if ( pDecision == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_CHECK_ERROR;
	}
	Decision = __xrtX509PathSearch(pTarget, pSource, pConfig, ppPath, iCapacity, pResult);
	if ( Decision.Result != X509_CHECK_ERROR ) {
		pDecision->Reason = Decision.Reason;
		pDecision->Depth = Decision.Depth;
		pDecision->Message = Decision.Message;
	}
	return Decision.Result;
}

XRT_API bool xrtX509PathBuild(
	const xx509cert* pTarget, const xx509pathsource* pSource,
	const xx509pathconfig* pConfig, const xx509cert** ppPath,
	size_t iCapacity, xx509pathresult* pResult
)
{
	xrt_x509_decision Decision = __xrtX509PathSearch(
		pTarget, pSource, pConfig, ppPath, iCapacity, pResult);
	xerror* Cause;
	if ( Decision.Result == X509_CHECK_VALID ) {
		return true;
	}
	if ( Decision.Result == X509_CHECK_ERROR ) {
		return false;
	}
	if ( Decision.Reason == X509_ERROR_PATH_BUILD ) {
		return __xrtX509PathBuildError(XERR_NOT_FOUND, Decision.Message, NULL);
	}
	if ( !__xrtX509DecisionRaise(&Decision, "x509-path") ) {
		return false;
	}
	Cause = xrtTakeError();
	return __xrtX509PathBuildError(XERR_NOT_FOUND,
		"no candidate certification path passed validation", Cause);
}

#endif
