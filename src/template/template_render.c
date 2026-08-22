#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_CORE)



/* 判断路径键是否与一个无零结尾依赖的 ASCII 常量相同。 */
static bool __xrtTemplateKeyEqual(xstrview Key, cstr sText)
{
	size_t iSize = strlen(sText);

	return (Key.Size == iSize) &&
		(memcmp(Key.Data, sText, iSize) == 0);
}



/* 消耗有限渲染步数，防止恶意模板无限放大解释成本。 */
bool __xrtTemplateStep(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	if ( pRender->Steps >= pRender->Config->MaxSteps ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render",
			"template render step limit exceeded",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	pRender->Steps++;
	return true;
}



/* 调用 writer 并隔离回调内部留下的错误状态。 */
bool __xrtTemplateEmit(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	xstrview Text
)
{
	xerror* pPrevious;
	xerror* pCallback;
	bool bResult;

	if ( Text.Size > (pRender->Config->MaxOutputBytes -
		pRender->Written) ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render",
			"template output byte limit exceeded",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	pPrevious = xrtTakeError();
	bResult = pRender->Write(pRender->UserData, Text);
	pCallback = xrtTakeError();
	if ( bResult ) {
		xrtErrorFree(pCallback);
		if ( pPrevious != NULL ) {
			__xrtErrorSetOwned(pPrevious);
		}
		pRender->Written += Text.Size;
		return true;
	}
	xrtErrorFree(pPrevious);
	if ( pCallback != NULL ) {
		__xrtErrorSetOwned(pCallback);
	} else {
		__xrtTemplateError(
			XERR_IO,
			XTEMPLATE_ERROR_WRITE,
			"write",
			"template writer rejected output",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
	}
	return false;
}



/* 读取已经编译的路径段并返回它借用的键。 */
static xstrview __xrtTemplatePathKey(
	const xtemplate* pTemplate,
	const xrt_template_path* pPath
)
{
	return (xstrview){
		pTemplate->Source + pPath->Offset,
		pPath->Size
	};
}



/* 把数组正负索引解析为现有元素位置，缺失不污染错误状态。 */
static bool __xrtTemplateArrayIndex(
	const xvalue* pArray,
	int64 iIndex,
	size_t* pResolved
)
{
	size_t iCount = xrtValueCount(pArray);

	if ( iIndex >= 0 ) {
		if ( ((uint64)iIndex > (uint64)SIZE_MAX) ||
			 ((size_t)iIndex >= iCount) ) {
			return false;
		}
		*pResolved = (size_t)iIndex;
		return true;
	}
	{
		uint64 iOffset = (uint64)(-(iIndex + 1)) + 1u;

		if ( iOffset > iCount ) {
			return false;
		}
		*pResolved = iCount - (size_t)iOffset;
		return true;
	}
}



/* 从借用值创建不分配的路径求值结果。 */
static void __xrtTemplateResolvedValue(
	const xvalue* pValue,
	xrt_template_eval* pResult
)
{
	memset(pResult, 0, sizeof(*pResult));
	if ( pValue == NULL ) {
		pResult->Type = XVALUE_NULL;
		return;
	}
	pResult->Type = xrtValueType(pValue);
	pResult->Value = pValue;
}



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

/* 从循环栈解析 loop.index、key、value、first、last 和 depth。 */
static bool __xrtTemplateResolveLoop(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iPathStart,
	uint32 iPathCount,
	xrt_template_eval* pValue,
	bool* pFound
)
{
	const xrt_template_path* pField;
	xstrview Field;

	if ( (pRender->Loop == NULL) || (iPathCount < 2u) ) {
		return true;
	}
	pField = (const xrt_template_path*)xrtArrayConstGet(
		&pRender->Template->Paths,
		(size_t)iPathStart + 1u
	);
	if ( (pField == NULL) || (pField->Type != XRT_TEMPLATE_PATH_KEY) ||
		 !__xrtTemplateStep(pRender, pNode) ) {
		return false;
	}
	Field = __xrtTemplatePathKey(pRender->Template, pField);
	memset(pValue, 0, sizeof(*pValue));
	if ( __xrtTemplateKeyEqual(Field, "value") ) {
		if ( pRender->Loop->IntegerValue ) {
			pValue->Type = XVALUE_INT;
			pValue->Data.Integer = pRender->Loop->Integer;
		} else {
			__xrtTemplateResolvedValue(pRender->Loop->Value, pValue);
		}
	} else if ( __xrtTemplateKeyEqual(Field, "index") ) {
		pValue->Type = XVALUE_INT;
		pValue->Data.Integer = (int64)pRender->Loop->Index;
	} else if ( __xrtTemplateKeyEqual(Field, "number") ) {
		pValue->Type = XVALUE_INT;
		pValue->Data.Integer = (int64)(pRender->Loop->Index + 1u);
	} else if ( __xrtTemplateKeyEqual(Field, "depth") ) {
		pValue->Type = XVALUE_INT;
		pValue->Data.Integer = (int64)pRender->Loop->Depth;
	} else if ( __xrtTemplateKeyEqual(Field, "first") ) {
		pValue->Type = XVALUE_BOOL;
		pValue->Data.Bool = pRender->Loop->Index == 0u;
	} else if ( __xrtTemplateKeyEqual(Field, "last") ) {
		pValue->Type = XVALUE_BOOL;
		pValue->Data.Bool = (pRender->Loop->Index + 1u) ==
			pRender->Loop->Count;
	} else if ( __xrtTemplateKeyEqual(Field, "key") ) {
		if ( pRender->Loop->Key.Type == XVALUE_KEY_STRING ) {
			pValue->Type = XVALUE_STRING;
			pValue->Data.String = pRender->Loop->Key.String;
		} else if ( pRender->Loop->Key.Type == XVALUE_KEY_INT ) {
			pValue->Type = XVALUE_INT;
			pValue->Data.Integer = pRender->Loop->Key.Integer;
		} else if ( pRender->Loop->Key.Type == XVALUE_KEY_INDEX ) {
			pValue->Type = XVALUE_INT;
			pValue->Data.Integer = (int64)pRender->Loop->Key.Index;
		} else {
			return true;
		}
	} else {
		return true;
	}
	if ( iPathCount > 2u ) {
		const xvalue* pResolved = pValue->Value;

		if ( pResolved == NULL ) {
			__xrtTemplateError(
				XERR_TYPE,
				XTEMPLATE_ERROR_TYPE,
				"resolve",
				"template loop scalar cannot have child path segments",
				pRender->Template,
				pField->Offset,
				pField->Size
			);
			return false;
		}
		for ( uint32 i = 2u; i < iPathCount; i++ ) {
			const xrt_template_path* pPath =
				(const xrt_template_path*)xrtArrayConstGet(
					&pRender->Template->Paths,
					(size_t)iPathStart + i
				);

			if ( (pPath == NULL) || !__xrtTemplateStep(pRender, pNode) ) {
				return false;
			}
			if ( pPath->Type == XRT_TEMPLATE_PATH_KEY ) {
				if ( xrtValueType(pResolved) != XVALUE_OBJECT ) {
					goto child_type_error;
				}
				pResolved = xrtValueObjectGet(
					pResolved,
					__xrtTemplatePathKey(pRender->Template, pPath)
				);
			} else {
				size_t iIndex;

				if ( xrtValueType(pResolved) != XVALUE_ARRAY ) {
					goto child_type_error;
				}
				if ( !__xrtTemplateArrayIndex(
						pResolved,
						pPath->Index,
						&iIndex
					) ) {
					return true;
				}
				pResolved = xrtValueArrayGet(pResolved, iIndex);
			}
			if ( pResolved == NULL ) {
				return true;
			}
		}
		__xrtTemplateResolvedValue(pResolved, pValue);
	}
	*pFound = true;
	return true;

child_type_error:
	__xrtTemplateError(
		XERR_TYPE,
		XTEMPLATE_ERROR_TYPE,
		"resolve",
		"template loop value path has an incompatible container",
		pRender->Template,
		pField->Offset,
		pField->Size
	);
	return false;
}

#endif



/* 从当前、根、全局和循环作用域解析一条预编译值路径。 */
bool __xrtTemplateResolveCompiled(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iPathStart,
	uint32 iPathCount,
	xrt_template_eval* pValue,
	bool* pFound
)
{
	const xvalue* pCurrent = pRender->Current;
	const xvalue* pResolved = NULL;
	size_t i;

	memset(pValue, 0, sizeof(*pValue));
	pValue->Type = XVALUE_NULL;
	*pFound = false;
	for ( i = 0; i < iPathCount; i++ ) {
		const xrt_template_path* pPath =
			(const xrt_template_path*)xrtArrayConstGet(
				&pRender->Template->Paths,
				(size_t)iPathStart + i
			);

		if ( (pPath == NULL) || !__xrtTemplateStep(pRender, pNode) ) {
			return false;
		}
		if ( i == 0 ) {
			xstrview Key = __xrtTemplatePathKey(
				pRender->Template,
				pPath
			);

			if ( __xrtTemplateKeyEqual(Key, "this") ) {
				pResolved = pCurrent;
			} else if ( __xrtTemplateKeyEqual(Key, "root") ) {
				pResolved = pRender->Config->Root;
			} else if ( __xrtTemplateKeyEqual(Key, "global") ) {
				pResolved = pRender->Config->Global;
			}
			#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
				else if ( __xrtTemplateKeyEqual(Key, "loop") ) {
					return __xrtTemplateResolveLoop(
						pRender,
						pNode,
						iPathStart,
						iPathCount,
						pValue,
						pFound
					);
				}
			#endif
			else if ( (pCurrent != NULL) &&
				 (xrtValueType(pCurrent) == XVALUE_OBJECT) ) {
				pResolved = xrtValueObjectGet(pCurrent, Key);
			} else {
				return true;
			}
			if ( pResolved == NULL ) {
				return true;
			}
			continue;
		}
		if ( xrtValueType(pResolved) == XVALUE_NULL ) {
			return true;
		}
		if ( pPath->Type == XRT_TEMPLATE_PATH_KEY ) {
			if ( xrtValueType(pResolved) != XVALUE_OBJECT ) {
				__xrtTemplateError(
					XERR_TYPE,
					XTEMPLATE_ERROR_TYPE,
					"resolve",
					"template path key requires an object",
					pRender->Template,
					pPath->Offset,
					pPath->Size
				);
				return false;
			}
			pResolved = xrtValueObjectGet(
				pResolved,
				__xrtTemplatePathKey(pRender->Template, pPath)
			);
		} else {
			size_t iIndex;

			if ( xrtValueType(pResolved) != XVALUE_ARRAY ) {
				__xrtTemplateError(
					XERR_TYPE,
					XTEMPLATE_ERROR_TYPE,
					"resolve",
					"template path index requires an array",
					pRender->Template,
					pPath->Offset,
					pPath->Size
				);
				return false;
			}
			if ( !__xrtTemplateArrayIndex(
				pResolved,
				pPath->Index,
				&iIndex
			) ) {
				return true;
			}
			pResolved = xrtValueArrayGet(pResolved, iIndex);
		}
		if ( pResolved == NULL ) {
			return true;
		}
	}
	__xrtTemplateResolvedValue(pResolved, pValue);
	*pFound = true;
	return true;
}



/* 在分配前检查已知长度是否仍位于输出预算内。 */
static bool __xrtTemplateOutputFits(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	size_t iSize
)
{
	if ( iSize <= (pRender->Config->MaxOutputBytes -
		pRender->Written) ) {
		return true;
	}
	__xrtTemplateError(
		XERR_RANGE,
		XTEMPLATE_ERROR_LIMIT,
		"render",
		"template output byte limit exceeded",
		pRender->Template,
		pNode->SourceOffset,
		pNode->SourceSize
	);
	return false;
}



/* 格式化并写出整数或浮点数，常见结果完全使用栈缓冲。 */
static bool __xrtTemplateWriteNumber(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	const xrt_template_eval* pValue,
	xstrview Format
)
{
	char arrBuffer[128];
	char* sOutput = arrBuffer;
	size_t iSize;
	bool bResult;

	if ( pValue->Type == XVALUE_INT ) {
		int64 iValue;

		if ( pValue->Value != NULL ) {
			if ( !xrtValueGetInt(pValue->Value, &iValue) ) {
				goto format_error;
			}
		} else {
			iValue = pValue->Data.Integer;
		}
		if ( !xrtIntFormatTo(
			iValue, Format, NULL, 0, &iSize
		) ) {
			goto format_error;
		}
		if ( !__xrtTemplateOutputFits(pRender, pNode, iSize) ) {
			return false;
		}
		if ( iSize >= sizeof(arrBuffer) ) {
			sOutput = (char*)xrtMalloc(iSize + 1u);
			if ( sOutput == NULL ) {
				return false;
			}
		}
		bResult = xrtIntFormatTo(
			iValue,
			Format,
			sOutput,
			iSize + 1u,
			&iSize
		);
	} else {
		double fValue;

		if ( pValue->Value != NULL ) {
			if ( !xrtValueGetFloat(pValue->Value, &fValue) ) {
				goto format_error;
			}
		} else {
			fValue = pValue->Data.Float;
		}
		if ( !xrtNumFormatTo(
			fValue, Format, NULL, 0, &iSize
		) ) {
			goto format_error;
		}
		if ( !__xrtTemplateOutputFits(pRender, pNode, iSize) ) {
			return false;
		}
		if ( iSize >= sizeof(arrBuffer) ) {
			sOutput = (char*)xrtMalloc(iSize + 1u);
			if ( sOutput == NULL ) {
				return false;
			}
		}
		bResult = xrtNumFormatTo(
			fValue,
			Format,
			sOutput,
			iSize + 1u,
			&iSize
		);
	}
	if ( !bResult ) {
		if ( sOutput != arrBuffer ) {
			xrtFree(sOutput);
		}
		goto format_error;
	}
	bResult = __xrtTemplateEmit(
		pRender,
		pNode,
		(xstrview){ sOutput, iSize }
	);
	if ( sOutput != arrBuffer ) {
		xrtFree(sOutput);
	}
	return bResult;

format_error:
	__xrtTemplateWrapCurrent(
		XTEMPLATE_ERROR_FORMAT,
		"format-number",
		"failed to format a template number",
		pRender->Template,
		pNode->SourceOffset,
		pNode->SourceSize
	);
	return false;
}



/* 格式化并写出 UTC 时间，默认使用 RFC 3339 稳定文本。 */
static bool __xrtTemplateWriteTime(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	const xrt_template_eval* pValue,
	xstrview Format
)
{
	char arrBuffer[128];
	char* sOutput = arrBuffer;
	bool bDefault = Format.Size == 0;
	bool bResult;
	size_t iSize;
	xtime Time;

	if ( pValue->Value != NULL ) {
		if ( !xrtValueGetTime(pValue->Value, &Time) ) {
			goto format_error;
		}
	} else {
		Time = pValue->Data.Time;
	}
	iSize = bDefault
		? xrtTimeWriteRFC3339(NULL, 0, Time, 0)
		: xrtTimeWrite(NULL, 0, Time, 0, Format);
	if ( iSize == XRT_NPOS ) {
		goto format_error;
	}
	if ( !__xrtTemplateOutputFits(pRender, pNode, iSize) ) {
		return false;
	}
	if ( iSize >= sizeof(arrBuffer) ) {
		sOutput = (char*)xrtMalloc(iSize + 1u);
		if ( sOutput == NULL ) {
			return false;
		}
	}
	if ( bDefault ) {
		bResult = xrtTimeWriteRFC3339(
			sOutput,
			iSize + 1u,
			Time,
			0
		) != XRT_NPOS;
	} else {
		bResult = xrtTimeWrite(
			sOutput,
			iSize + 1u,
			Time,
			0,
			Format
		) != XRT_NPOS;
	}
	if ( !bResult ) {
		if ( sOutput != arrBuffer ) {
			xrtFree(sOutput);
		}
		goto format_error;
	}
	bResult = __xrtTemplateEmit(
		pRender,
		pNode,
		(xstrview){ sOutput, iSize }
	);
	if ( sOutput != arrBuffer ) {
		xrtFree(sOutput);
	}
	return bResult;

format_error:
	__xrtTemplateWrapCurrent(
		XTEMPLATE_ERROR_FORMAT,
		"format-time",
		"failed to format a template time",
		pRender->Template,
		pNode->SourceOffset,
		pNode->SourceSize
	);
	return false;
}



/* 按文本输出规则写出一个动态标量。 */
static bool __xrtTemplateWriteText(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	const xrt_template_eval* pValue
)
{
	xstrview Text;
	xbytesview Data;

	switch ( pValue->Type ) {
		case XVALUE_NULL:
			return true;
		case XVALUE_BOOL:
		{
			bool bValue;

			if ( pValue->Value != NULL ) {
				if ( !xrtValueGetBool(pValue->Value, &bValue) ) {
					break;
				}
			} else {
				bValue = pValue->Data.Bool;
			}
			return __xrtTemplateEmit(
				pRender,
				pNode,
				bValue ? XRT_STR_LITERAL("true") :
					XRT_STR_LITERAL("false")
			);
		}
		case XVALUE_INT:
		case XVALUE_FLOAT:
			return __xrtTemplateWriteNumber(
				pRender,
				pNode,
				pValue,
				(xstrview){ NULL, 0 }
			);
		case XVALUE_STRING:
			if ( pValue->Value == NULL ) {
				return __xrtTemplateEmit(
					pRender,
					pNode,
					pValue->Data.String
				);
			}
			if ( xrtValueGetString(pValue->Value, &Text) ) {
				return __xrtTemplateEmit(pRender, pNode, Text);
			}
			break;
		case XVALUE_BYTES:
			if ( pValue->Value == NULL ) {
				return __xrtTemplateEmit(
					pRender,
					pNode,
					pValue->Data.String
				);
			}
			if ( xrtValueGetBytes(pValue->Value, &Data) ) {
				return __xrtTemplateEmit(
					pRender,
					pNode,
					(xstrview){ (cstr)Data.Data, Data.Size }
				);
			}
			break;
		case XVALUE_TIME:
			return __xrtTemplateWriteTime(
				pRender,
				pNode,
				pValue,
				(xstrview){ NULL, 0 }
			);
		default:
			__xrtTemplateError(
				XERR_TYPE,
				XTEMPLATE_ERROR_TYPE,
				"render-text",
				"template text output requires a scalar value",
				pRender->Template,
				pNode->SourceOffset,
				pNode->SourceSize
			);
			return false;
	}
	__xrtTemplateWrapCurrent(
		XTEMPLATE_ERROR_TYPE,
		"render-text",
		"failed to read a template scalar value",
		pRender->Template,
		pNode->SourceOffset,
		pNode->SourceSize
	);
	return false;
}



/* 渲染一个已经解析并确认存在的输出节点。 */
static bool __xrtTemplateRenderOutput(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xrt_template_eval Value;
	bool bFound;
	xstrview Format = {
		pRender->Template->Text.Data != NULL
			? pRender->Template->Text.Data +
				pNode->Data.Output.FormatOffset
			: NULL,
		pNode->Data.Output.FormatSize
	};

	if ( !__xrtTemplateResolveCompiled(
		pRender,
		pNode,
		pNode->Data.Output.PathStart,
		pNode->Data.Output.PathCount,
		&Value,
		&bFound
	) ) {
		return false;
	}
	if ( !bFound ) {
		if ( (pRender->Config->Flags & XTEMPLATE_STRICT_UNDEFINED) == 0 ) {
			return true;
		}
		__xrtTemplateError(
			XERR_NOT_FOUND,
			XTEMPLATE_ERROR_UNDEFINED,
			"resolve",
			"template value path is undefined",
			pRender->Template,
			pNode->Data.Output.ExpressionOffset,
			pNode->Data.Output.ExpressionSize
		);
		return false;
	}
	if ( pNode->Output == XTEMPLATE_OUTPUT_TEXT ) {
		return __xrtTemplateWriteText(pRender, pNode, &Value);
	}
	if ( pNode->Output == XTEMPLATE_OUTPUT_NUMBER ) {
		if ( (Value.Type != XVALUE_INT) &&
			 (Value.Type != XVALUE_FLOAT) ) {
			__xrtTemplateError(
				XERR_TYPE,
				XTEMPLATE_ERROR_TYPE,
				"render-number",
				"template number output requires an integer or float",
				pRender->Template,
				pNode->SourceOffset,
				pNode->SourceSize
			);
			return false;
		}
		return __xrtTemplateWriteNumber(
			pRender,
			pNode,
			&Value,
			Format
		);
	}
	if ( Value.Type != XVALUE_TIME ) {
		__xrtTemplateError(
			XERR_TYPE,
			XTEMPLATE_ERROR_TYPE,
			"render-time",
			"template time output requires a time value",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	return __xrtTemplateWriteTime(pRender, pNode, &Value, Format);
}



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

xrt_template_flow __xrtTemplateRenderSpan(
	xrt_template_render* pRender,
	uint32 iStart,
	uint32 iEnd
);



/* 渲染内联条件选中的一段预编译文本。 */
static bool __xrtTemplateRenderInlineIf(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xrt_template_eval Value;
	bool bCondition;
	uint32 iOffset;
	uint32 iSize;

	if ( !__xrtTemplateEvalExpression(
		pRender,
		pNode,
		pNode->Data.InlineIf.Expression,
		&Value
	) || !__xrtTemplateEvalTruthy(&Value, &bCondition) ) {
		return false;
	}
	iOffset = bCondition ? pNode->Data.InlineIf.TrueOffset :
		pNode->Data.InlineIf.FalseOffset;
	iSize = bCondition ? pNode->Data.InlineIf.TrueSize :
		pNode->Data.InlineIf.FalseSize;
	return __xrtTemplateEmit(
		pRender,
		pNode,
		(xstrview){
			pRender->Template->Text.Data != NULL
				? pRender->Template->Text.Data + iOffset : NULL,
			iSize
		}
	);
}



/* 渲染第一个条件成立的 if 分支。 */
static xrt_template_flow __xrtTemplateRenderIf(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	uint32 iBranch = pNode->Data.If.BranchStart;

	for ( uint32 i = 0; i < pNode->Data.If.BranchCount; i++ ) {
		const xrt_template_branch* pBranch =
			(const xrt_template_branch*)xrtArrayConstGet(
				&pRender->Template->Branches,
				iBranch
			);
		bool bCondition = true;

		if ( pBranch == NULL ) {
			__xrtTemplateError(
				XERR_STATE,
				XTEMPLATE_ERROR_TYPE,
				"render-if",
				"template contains an invalid compiled branch",
				pRender->Template,
				pNode->SourceOffset,
				pNode->SourceSize
			);
			return XRT_TEMPLATE_FLOW_ERROR;
		}
		if ( pBranch->Expression != XRT_TEMPLATE_INDEX_NONE ) {
			xrt_template_eval Value;

			if ( !__xrtTemplateEvalExpression(
				pRender,
				pNode,
				pBranch->Expression,
				&Value
			) || !__xrtTemplateEvalTruthy(&Value, &bCondition) ) {
				return XRT_TEMPLATE_FLOW_ERROR;
			}
		}
		if ( bCondition ) {
			return __xrtTemplateRenderSpan(
				pRender,
				pBranch->BodyStart,
				pBranch->BodyEnd
			);
		}
		iBranch = pBranch->Next;
	}
	return XRT_TEMPLATE_FLOW_OK;
}



/* 计算闭区间循环次数并在执行前拒绝超出迭代预算的范围。 */
static bool __xrtTemplateRangeCount(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	int64 iStart,
	int64 iEnd,
	int64 iStep,
	size_t* pCount
)
{
	uint64 iDistance;
	uint64 iStride;
	uint64 iQuotient;
	size_t iRemaining = pRender->Config->MaxLoopIterations -
		pRender->LoopIterations;

	*pCount = 0;
	if ( ((iStep > 0) && (iStart > iEnd)) ||
		 ((iStep < 0) && (iStart < iEnd)) ) {
		return true;
	}
	iDistance = iStep > 0
		? (uint64)iEnd - (uint64)iStart
		: (uint64)iStart - (uint64)iEnd;
	iStride = iStep > 0
		? (uint64)iStep : (uint64)(-(iStep + 1)) + 1u;
	iQuotient = iDistance / iStride;
	if ( (iRemaining == 0) ||
		 (iQuotient >= (uint64)iRemaining) ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render-for",
			"template loop iteration limit exceeded",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	*pCount = (size_t)iQuotient + 1u;
	return true;
}



/* 求值并读取 for 整数参数，把底层类型错误包装到模板错误域。 */
static bool __xrtTemplateRenderForInteger(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iExpression,
	int64* pValue
)
{
	xrt_template_eval Value;

	if ( !__xrtTemplateEvalExpression(
		pRender,
		pNode,
		iExpression,
		&Value
	) ) {
		return false;
	}
	if ( !__xrtTemplateEvalInteger(&Value, pValue) ) {
		__xrtTemplateWrapCurrent(
			XTEMPLATE_ERROR_TYPE,
			"render-for",
			"template for bounds and step must be integers",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	return true;
}



/* 渲染一个有限整数闭区间，当前整数通过 loop.value 暴露。 */
static xrt_template_flow __xrtTemplateRenderFor(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xrt_template_loop Loop;
	const xrt_template_loop* pParent = pRender->Loop;
	int64 iStart;
	int64 iEnd;
	int64 iStep;
	int64 iValue;
	size_t iCount;

	if ( !__xrtTemplateRenderForInteger(
		pRender,
		pNode,
		pNode->Data.For.StartExpression,
		&iStart
	) || !__xrtTemplateRenderForInteger(
		pRender,
		pNode,
		pNode->Data.For.EndExpression,
		&iEnd
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( pNode->Data.For.StepExpression == XRT_TEMPLATE_INDEX_NONE ) {
		iStep = iStart <= iEnd ? 1 : -1;
	} else if ( !__xrtTemplateRenderForInteger(
		pRender,
		pNode,
		pNode->Data.For.StepExpression,
		&iStep
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( iStep == 0 ) {
		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_TYPE,
			"render-for",
			"template for step cannot be zero",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( !__xrtTemplateRangeCount(
		pRender, pNode, iStart, iEnd, iStep, &iCount
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	memset(&Loop, 0, sizeof(Loop));
	Loop.Parent = pParent;
	Loop.Key.Type = XVALUE_KEY_INDEX;
	Loop.Count = iCount;
	Loop.Depth = pParent != NULL ? pParent->Depth + 1u : 1u;
	Loop.IntegerValue = true;
	pRender->Loop = &Loop;
	iValue = iStart;
	for ( size_t i = 0; i < iCount; i++ ) {
		xrt_template_flow Flow;

		Loop.Index = i;
		Loop.Key.Index = i;
		Loop.Integer = iValue;
		pRender->LoopIterations++;
		Flow = __xrtTemplateRenderSpan(
			pRender,
			pNode->Data.For.BodyStart,
			pNode->Data.For.BodyEnd
		);
		if ( Flow == XRT_TEMPLATE_FLOW_ERROR ) {
			pRender->Loop = pParent;
			return Flow;
		}
		if ( Flow == XRT_TEMPLATE_FLOW_BREAK ) {
			break;
		}
		if ( (i + 1u) < iCount ) {
			iValue += iStep;
		}
	}
	pRender->Loop = pParent;
	return XRT_TEMPLATE_FLOW_OK;
}



/* 判断值类型是否具有统一稳定迭代协议。 */
static bool __xrtTemplateIterable(xvaluetype Type)
{
	return (Type == XVALUE_ARRAY) || (Type == XVALUE_INT_MAP) ||
		(Type == XVALUE_SET) || (Type == XVALUE_OBJECT);
}



/* 渲染容器稳定快照，每轮只更新借用的当前值和栈上循环元数据。 */
static xrt_template_flow __xrtTemplateRenderForeach(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xrt_template_eval Value;
	xrt_template_loop Loop;
	xvalueiter Iterator;
	xvaluekey Key;
	const xrt_template_loop* pParentLoop = pRender->Loop;
	const xvalue* pParentCurrent = pRender->Current;
	xvalue* pItem;
	xrt_template_flow Result = XRT_TEMPLATE_FLOW_OK;

	if ( !__xrtTemplateEvalExpression(
		pRender,
		pNode,
		pNode->Data.Foreach.Expression,
		&Value
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( (Value.Value == NULL) || !__xrtTemplateIterable(Value.Type) ) {
		__xrtTemplateError(
			XERR_TYPE,
			XTEMPLATE_ERROR_TYPE,
			"render-foreach",
			"template foreach requires an array, map, set or object",
			pRender->Template,
			pNode->Data.Foreach.ExpressionOffset,
			pNode->Data.Foreach.ExpressionSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( xrtValueCount(Value.Value) >
		 (pRender->Config->MaxLoopIterations -
		 pRender->LoopIterations) ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render-foreach",
			"template loop iteration limit exceeded",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(Value.Value, &Iterator) ) {
		__xrtTemplateWrapCurrent(
			XTEMPLATE_ERROR_ITERATE,
			"render-foreach",
			"failed to create a stable template iteration snapshot",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	memset(&Loop, 0, sizeof(Loop));
	Loop.Parent = pParentLoop;
	Loop.Count = xrtValueCount(Value.Value);
	Loop.Depth = pParentLoop != NULL ? pParentLoop->Depth + 1u : 1u;
	pRender->Loop = &Loop;
	while ( (pItem = xrtValueIterNext(&Iterator, &Key)) != NULL ) {
		xrt_template_flow Flow;

		Loop.Value = pItem;
		Loop.Key = Key;
		pRender->Current = pItem;
		pRender->LoopIterations++;
		Flow = __xrtTemplateRenderSpan(
			pRender,
			pNode->Data.Foreach.BodyStart,
			pNode->Data.Foreach.BodyEnd
		);
		if ( Flow == XRT_TEMPLATE_FLOW_ERROR ) {
			Result = Flow;
			break;
		}
		if ( Flow == XRT_TEMPLATE_FLOW_BREAK ) {
			break;
		}
		Loop.Index++;
	}
	pRender->Current = pParentCurrent;
	pRender->Loop = pParentLoop;
	xrtValueIterEnd(&Iterator);
	return Result;
}



/* 执行一个扁平节点范围，并把循环控制流传递给最近的循环。 */
xrt_template_flow __xrtTemplateRenderSpan(
	xrt_template_render* pRender,
	uint32 iStart,
	uint32 iEnd
)
{
	xrt_template_flow Result = XRT_TEMPLATE_FLOW_OK;
	uint32 i = iStart;

	if ( pRender->Depth >= pRender->Config->MaxDepth ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render",
			"template render depth limit exceeded",
			pRender->Template,
			0,
			0
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	pRender->Depth++;
	while ( i < iEnd ) {
		const xrt_template_node* pNode =
			(const xrt_template_node*)xrtArrayConstGet(
				&pRender->Template->Nodes,
				i
			);

		if ( (pNode == NULL) || !__xrtTemplateStep(pRender, pNode) ) {
			Result = XRT_TEMPLATE_FLOW_ERROR;
			break;
		}
		if ( pNode->Type == XTEMPLATE_NODE_TEXT ) {
			if ( !__xrtTemplateEmit(
				pRender,
				pNode,
				(xstrview){
					pRender->Template->Source + pNode->Data.Text.Offset,
					pNode->Data.Text.Size
				}
			) ) {
				Result = XRT_TEMPLATE_FLOW_ERROR;
				break;
			}
			i++;
			continue;
		}
		if ( pNode->Type == XTEMPLATE_NODE_OUTPUT ) {
			if ( !__xrtTemplateRenderOutput(pRender, pNode) ) {
				Result = XRT_TEMPLATE_FLOW_ERROR;
				break;
			}
			i++;
			continue;
		}
		if ( pNode->Type == XTEMPLATE_NODE_INLINE_IF ) {
			if ( !__xrtTemplateRenderInlineIf(pRender, pNode) ) {
				Result = XRT_TEMPLATE_FLOW_ERROR;
				break;
			}
			i++;
			continue;
		}
		if ( pNode->Type == XTEMPLATE_NODE_IF ) {
			Result = __xrtTemplateRenderIf(pRender, pNode);
			i = pNode->Data.If.Next;
		} else if ( pNode->Type == XTEMPLATE_NODE_FOR ) {
			Result = __xrtTemplateRenderFor(pRender, pNode);
			i = pNode->Data.For.Next;
		} else if ( pNode->Type == XTEMPLATE_NODE_FOREACH ) {
			Result = __xrtTemplateRenderForeach(pRender, pNode);
			i = pNode->Data.Foreach.Next;
		} else if ( pNode->Type == XTEMPLATE_NODE_BREAK ) {
			Result = XRT_TEMPLATE_FLOW_BREAK;
		} else if ( pNode->Type == XTEMPLATE_NODE_CONTINUE ) {
			Result = XRT_TEMPLATE_FLOW_CONTINUE;
		#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		} else if ( pNode->Type == XTEMPLATE_NODE_DEFINE ) {
			i = pNode->Data.Define.Next;
			continue;
		} else if ( pNode->Type == XTEMPLATE_NODE_INCLUDE ) {
			Result = __xrtTemplateRenderInclude(pRender, pNode);
			i++;
		} else if ( pNode->Type == XTEMPLATE_NODE_RAW ) {
			if ( !__xrtTemplateEmit(
				pRender,
				pNode,
				(xstrview){
					pRender->Template->Source +
						pNode->Data.Raw.Offset,
					pNode->Data.Raw.Size
				}
			) ) {
				Result = XRT_TEMPLATE_FLOW_ERROR;
			}
			i++;
		#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		} else if ( pNode->Type == XTEMPLATE_NODE_EXTENSION ) {
			Result = __xrtTemplateRenderExtension(pRender, pNode);
			i = pNode->Data.Extension.Next;
		#endif
		#endif
		} else {
			__xrtTemplateError(
				XERR_STATE,
				XTEMPLATE_ERROR_TYPE,
				"render",
				"template contains an unknown compiled node",
				pRender->Template,
				pNode->SourceOffset,
				pNode->SourceSize
			);
			Result = XRT_TEMPLATE_FLOW_ERROR;
		}
		if ( Result != XRT_TEMPLATE_FLOW_OK ) {
			break;
		}
	}
	pRender->Depth--;
	return Result;
}

#endif



/* 按顺序执行不可变模板中的节点。 */
bool __xrtTemplateRender(xrt_template_render* pRender)
{
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
			xrt_template_frame Frame;
		#endif
		xrt_template_flow Flow;

		#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
			Frame.Parent = NULL;
			Frame.Template = pRender->Template;
			Frame.Definition = XRT_TEMPLATE_INDEX_NONE;
			pRender->Frame = &Frame;
		#endif
		Flow = __xrtTemplateRenderSpan(
			pRender,
			0,
			(uint32)pRender->Template->Nodes.Count
		);
		#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
			pRender->Frame = NULL;
		#endif

		if ( (Flow == XRT_TEMPLATE_FLOW_BREAK) ||
			 (Flow == XRT_TEMPLATE_FLOW_CONTINUE) ) {
			__xrtTemplateError(
				XERR_STATE,
				XTEMPLATE_ERROR_SYNTAX,
				"render",
				"template break or continue has no enclosing loop",
				pRender->Template,
				0,
				0
			);
			return false;
		}
		return Flow == XRT_TEMPLATE_FLOW_OK;
	#else
		for ( size_t i = 0; i < pRender->Template->Nodes.Count; i++ ) {
			const xrt_template_node* pNode =
				(const xrt_template_node*)xrtArrayConstGet(
					&pRender->Template->Nodes,
					i
				);

			if ( (pNode == NULL) || !__xrtTemplateStep(pRender, pNode) ) {
				return false;
			}
			if ( pNode->Type == XTEMPLATE_NODE_TEXT ) {
				if ( !__xrtTemplateEmit(
					pRender,
					pNode,
					(xstrview){
						pRender->Template->Source +
							pNode->Data.Text.Offset,
						pNode->Data.Text.Size
					}
				) ) {
					return false;
				}
			} else if ( !__xrtTemplateRenderOutput(pRender, pNode) ) {
				return false;
			}
		}
		return true;
	#endif
}

#endif
