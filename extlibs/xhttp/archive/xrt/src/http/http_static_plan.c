#include "../internal/xrt_http_static.h"



#if defined(XRT_FEATURE_HTTP_STATIC_PLAN)

/* Range 扫描区分缺失、可处理和应被忽略三种结果。 */
typedef enum xrt_http_static_range_state {
	XRT_HTTP_STATIC_RANGE_NONE = 0,
	XRT_HTTP_STATIC_RANGE_READY,
	XRT_HTTP_STATIC_RANGE_IGNORE
} xrt_http_static_range_state;



/* 初始化静态响应计划的有界默认策略。 */
XRT_API void xrtHttpStaticPlanConfigInit(
	xhttpstaticplanconfig* pConfig
)
{
	xhttpstaticplanconfig Config = { 16u, 0 };

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 解析调用方配置并验证范围数组大小计算不会溢出。 */
static bool __xrtHttpStaticPlanConfigResolve(
	const xhttpstaticplanconfig* pInput,
	size_t iRangeCapacity,
	xhttpstaticplanconfig* pConfig,
	size_t* pRangeBytes
)
{
	*pConfig = (xhttpstaticplanconfig){ 16u, 0 };
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(iRangeCapacity >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(iRangeCapacity < pConfig->MaxRanges) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pRangeBytes = iRangeCapacity * sizeof(xhttpbyterange);
	return true;
}



/* 判断一个输出区间是否覆盖任一借用请求输入。 */
static bool __xrtHttpStaticPlanOutputAliases(
	const void* pOutput,
	size_t iOutputSize,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrentInput,
	const xhttprepresentation* pCurrent,
	const xhttpstaticplanconfig* pConfig
)
{
	xhttpfield Field;
	size_t iFieldBytes = iFieldCount * sizeof(xhttpfield);
	size_t i;

	if ( __xrtRangesOverlap(
			pOutput, iOutputSize, Method.Data, Method.Size
		) || __xrtRangesOverlap(
			pOutput, iOutputSize, pFields, iFieldBytes
		) || __xrtRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrentInput,
			sizeof(*pCurrentInput)
		) || ((pConfig != NULL) && __xrtRangesOverlap(
			pOutput, iOutputSize, pConfig, sizeof(*pConfig)
		)) || (pCurrent->HasETag && __xrtRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrent->ETag.Opaque.Data,
			pCurrent->ETag.Opaque.Size
		)) ) {
		return true;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtRangesOverlap(
				pOutput,
				iOutputSize,
				Field.Name.Data,
				Field.Name.Size
			) || __xrtRangesOverlap(
				pOutput,
				iOutputSize,
				Field.Value.Data,
				Field.Value.Size
			) ) {
			return true;
		}
	}
	return false;
}



/* 验证两个输出与全部输入相互独立。 */
static bool __xrtHttpStaticPlanAliasesValid(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrentInput,
	const xhttprepresentation* pCurrent,
	xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
)
{
	if ( __xrtHttpStaticPlanOutputAliases(
			pPlan,
			sizeof(*pPlan),
			Method,
			pFields,
			iFieldCount,
			pCurrentInput,
			pCurrent,
			pConfig
		) || __xrtHttpStaticPlanOutputAliases(
			pRanges,
			iRangeBytes,
			Method,
			pFields,
			iFieldCount,
			pCurrentInput,
			pCurrent,
			pConfig
		) || __xrtRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			pRanges,
			iRangeBytes
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 查找唯一 Range 字段并完整验证它的单位、语法和工作上限。 */
static xrt_http_static_range_state __xrtHttpStaticRangeRead(
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t iMaximum,
	xstrview* pSet
)
{
	xhttpfield Field;
	xhttpfield Range;
	xhttprangespec Spec;
	xhttpnext Next;
	xstrview Unit;
	xstrview Set;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t i;
	bool bRange = false;

	*pSet = (xstrview){ NULL, 0 };
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Range")
		) ) {
			continue;
		}
		if ( bRange ) {
			return XRT_HTTP_STATIC_RANGE_IGNORE;
		}
		Range = Field;
		bRange = true;
	}
	if ( !bRange ) {
		return XRT_HTTP_STATIC_RANGE_NONE;
	}
	if ( (iMaximum == 0) ||
		!__xrtHttpRangeParseValue(
			Range.Value, &Unit, &Set
		) || !xrtHttpTokenEqual(
			Unit, XRT_STR_LITERAL("bytes")
		) ) {
		return XRT_HTTP_STATIC_RANGE_IGNORE;
	}
	do {
		Next = __xrtHttpByteRangeNextValue(
			Set, &iOffset, &Spec
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XRT_HTTP_STATIC_RANGE_IGNORE;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			iCount++;
			if ( iCount > iMaximum ) {
				return XRT_HTTP_STATIC_RANGE_IGNORE;
			}
		}
	} while ( Next == XHTTP_NEXT_ITEM );
	if ( iCount == 0 ) {
		return XRT_HTTP_STATIC_RANGE_IGNORE;
	}
	*pSet = Set;
	return XRT_HTTP_STATIC_RANGE_READY;
}



/* 只有唯一且匹配的 If-Range 才允许继续处理 Range。 */
static bool __xrtHttpStaticIfRangeAllows(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent
)
{
	xhttpfield Field;
	xstrview Value = { NULL, 0 };
	size_t i;
	bool bIfRange = false;

	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("If-Range")
		) ) {
			continue;
		}
		if ( bIfRange ) {
			return false;
		}
		Value = Field.Value;
		bIfRange = true;
	}
	return !bIfRange || xrtHttpIfRangeMatch(Value, pCurrent);
}



/* 原子发布一个已经完整构建的静态响应计划。 */
static void __xrtHttpStaticPlanPublish(
	xhttpstaticplan* pOutput,
	const xhttpstaticplan* pPlan
)
{
	memcpy(pOutput, pPlan, sizeof(*pPlan));
}



/* 评估静态表示的条件请求、方法和字节范围。 */
XRT_API bool xrtHttpStaticPlanBuild(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iRangeCapacity,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
)
{
	xhttpstaticplanconfig Config;
	xhttpstaticplan Plan;
	xhttprepresentation Current;
	xhttpprecondition Condition;
	xrt_http_static_range_state RangeState;
	xhttprangeresult RangeResult;
	xstrview Set;
	size_t iRangeBytes;
	size_t iResolved = 0;
	bool bGet;
	bool bHead;

	if ( !__xrtRangeValid(pPlan, sizeof(Plan)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStaticPlanConfigResolve(
		pConfig,
		iRangeCapacity,
		&Config,
		&iRangeBytes
	) ) {
		return false;
	}
	if ( !__xrtRangeValid(pRanges, iRangeBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpPreconditionInputRead(
		Method,
		pFields,
		iFieldCount,
		pCurrent,
		&Current
	) ) {
		return false;
	}
	if ( !Current.Exists ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStaticPlanAliasesValid(
		Method,
		pFields,
		iFieldCount,
		pCurrent,
		&Current,
		pRanges,
		iRangeBytes,
		pConfig,
		pPlan
	) ) {
		return false;
	}

	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.CompleteLength = iLength;
	Plan.SelectedLength = iLength;
	bGet = __xrtHttpViewEqual(
		Method, XRT_STR_LITERAL("GET")
	);
	bHead = __xrtHttpViewEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);
	if ( !bGet && !bHead ) {
		Plan.Status = XHTTP_STATUS_METHOD_NOT_ALLOWED;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	Plan.SendBody = bGet;
	Plan.AcceptRanges = Config.MaxRanges != 0;

	Condition = xrtHttpPreconditionsEvaluate(
		Method,
		pFields,
		iFieldCount,
		&Current
	);
	if ( Condition == XHTTP_PRECONDITION_ERROR ) {
		return false;
	}
	if ( Condition == XHTTP_PRECONDITION_NOT_MODIFIED ) {
		Plan.Status = XHTTP_STATUS_NOT_MODIFIED;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	if ( Condition == XHTTP_PRECONDITION_FAILED ) {
		Plan.Status = XHTTP_STATUS_PRECONDITION_FAILED;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}

	/* Range 只用于 GET；HEAD 按完整表示生成元数据但不发送正文。 */
	if ( !bGet ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	RangeState = __xrtHttpStaticRangeRead(
		pFields,
		iFieldCount,
		Config.MaxRanges,
		&Set
	);
	if ( (RangeState != XRT_HTTP_STATIC_RANGE_READY) ||
		!__xrtHttpStaticIfRangeAllows(
			pFields, iFieldCount, &Current
		) ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	RangeResult = xrtHttpByteRangesResolve(
		Set,
		iLength,
		pRanges,
		iRangeCapacity,
		Config.MergeGap,
		&iResolved,
		&Plan.SelectedLength
	);
	if ( RangeResult == XHTTP_RANGE_ERROR ) {
		return false;
	}
	if ( RangeResult == XHTTP_RANGE_EMPTY ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	if ( RangeResult == XHTTP_RANGE_UNSATISFIED ) {
		Plan.Status = XHTTP_STATUS_RANGE_NOT_SATISFIABLE;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	Plan.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Plan.RangeCount = iResolved;
	__xrtHttpStaticPlanPublish(pPlan, &Plan);
	return true;
}

#endif
