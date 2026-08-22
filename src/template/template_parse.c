#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_CORE)



/* 判断字节是否属于模板语法允许忽略的 ASCII 空白。 */
bool __xrtTemplateSpace(char iByte)
{
	return (iByte == ' ') || (iByte == '\t') ||
		(iByte == '\r') || (iByte == '\n');
}



/* 判断字节是否可以作为路径标识符的首字节。 */
static bool __xrtTemplateNameStart(char iByte)
{
	return ((iByte >= 'a') && (iByte <= 'z')) ||
		((iByte >= 'A') && (iByte <= 'Z')) || (iByte == '_');
}



/* 判断字节是否可以作为路径标识符的后续字节。 */
static bool __xrtTemplateNameByte(char iByte)
{
	return __xrtTemplateNameStart(iByte) ||
		((iByte >= '0') && (iByte <= '9'));
}



/* 判断源串指定位置是否匹配完整标记。 */
static bool __xrtTemplateMarkerAt(
	const xtemplate* pTemplate,
	size_t iPosition,
	xstrview Marker
)
{
	if ( iPosition > pTemplate->SourceSize ) {
		return false;
	}
	if ( Marker.Size > (pTemplate->SourceSize - iPosition) ) {
		return false;
	}
	return memcmp(pTemplate->Source + iPosition,
		Marker.Data, Marker.Size) == 0;
}



/* 去除视图两侧的 ASCII 空白但保持源偏移可追踪。 */
static void __xrtTemplateTrim(size_t* pStart, size_t* pEnd, cstr sSource)
{
	while ( (*pStart < *pEnd) &&
		__xrtTemplateSpace(sSource[*pStart]) ) {
		(*pStart)++;
	}
	while ( (*pEnd > *pStart) &&
		__xrtTemplateSpace(sSource[*pEnd - 1u]) ) {
		(*pEnd)--;
	}
}



/* 在当前编译预算内追加一个节点。 */
static bool __xrtTemplatePushNode(
	xrt_template_parser* pParser,
	const xrt_template_node* pNode
)
{
	if ( pParser->Template->Nodes.Count >= pParser->Config->MaxNodes ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"compile",
			"template node limit exceeded",
			pParser->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	return xrtArrayPush(&pParser->Template->Nodes, pNode);
}



/* 把一段原样源文本追加为借用源串的文本节点。 */
static bool __xrtTemplatePushText(
	xrt_template_parser* pParser,
	size_t iOffset,
	size_t iSize
)
{
	xrt_template_node Node;

	if ( iSize == 0 ) {
		return true;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_TEXT;
	Node.SourceOffset = (uint32)iOffset;
	Node.SourceSize = (uint32)iSize;
	Node.Data.Text.Offset = (uint32)iOffset;
	Node.Data.Text.Size = (uint32)iSize;
	return __xrtTemplatePushNode(pParser, &Node);
}



/* 严格解析带可选负号的十进制数组索引。 */
static bool __xrtTemplateParseIndex(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	int64* pIndex
)
{
	uint64 iLimit;
	uint64 iValue = 0;
	bool bNegative = false;
	size_t i = iStart;

	if ( (i < iEnd) && ((pParser->Template->Source[i] == '-') ||
		 (pParser->Template->Source[i] == '+')) ) {
		bNegative = pParser->Template->Source[i] == '-';
		i++;
	}
	if ( i == iEnd ) {
		return false;
	}
	iLimit = bNegative ? (UINT64_C(1) << 63u) : (uint64)INT64_MAX;
	for ( ; i < iEnd; i++ ) {
		uint64 iDigit;

		if ( (pParser->Template->Source[i] < '0') ||
			 (pParser->Template->Source[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(pParser->Template->Source[i] - '0');
		if ( iValue > ((iLimit - iDigit) / 10u) ) {
			return false;
		}
		iValue = (iValue * 10u) + iDigit;
	}
	if ( bNegative ) {
		*pIndex = iValue == (UINT64_C(1) << 63u)
			? INT64_MIN : -(int64)iValue;
	} else {
		*pIndex = (int64)iValue;
	}
	return true;
}



/* 在全局和单路径预算内追加已经解析的路径段。 */
static bool __xrtTemplatePushPath(
	xrt_template_parser* pParser,
	const xrt_template_path* pPath,
	size_t iErrorOffset,
	size_t iErrorSize,
	size_t iPathCount
)
{
	if ( (iPathCount >= pParser->Config->MaxPathDepth) ||
		 (pParser->Template->Paths.Count >=
		  pParser->Config->MaxPathSegments) ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"compile-path",
			"template path segment limit exceeded",
			pParser->Template,
			iErrorOffset,
			iErrorSize
		);
		return false;
	}
	return xrtArrayPush(&pParser->Template->Paths, pPath);
}



/* 编译对象键和数组索引组成的路径，渲染时不再重新解析。 */
bool __xrtTemplateCompilePath(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pPathStart,
	uint32* pPathCount
)
{
	xrt_template_path Path;
	size_t i = iStart;
	size_t iCount = 0;
	size_t iPartStart;

	*pPathStart = (uint32)pParser->Template->Paths.Count;
	if ( (i == iEnd) ||
		 !__xrtTemplateNameStart(pParser->Template->Source[i]) ) {
		goto syntax_error;
	}
	for ( ;; ) {
		iPartStart = i;
		i++;
		while ( (i < iEnd) &&
			__xrtTemplateNameByte(pParser->Template->Source[i]) ) {
			i++;
		}
		memset(&Path, 0, sizeof(Path));
		Path.Type = XRT_TEMPLATE_PATH_KEY;
		Path.Offset = (uint32)iPartStart;
		Path.Size = (uint32)(i - iPartStart);
		if ( !__xrtTemplatePushPath(
			pParser,
			&Path,
			iStart,
			iEnd - iStart,
			iCount
		) ) {
			return false;
		}
		iCount++;
		while ( (i < iEnd) &&
			 (pParser->Template->Source[i] == '[') ) {
			size_t iIndexStart;
			int64 iIndex;

			i++;
			iIndexStart = i;
			while ( (i < iEnd) &&
				 (pParser->Template->Source[i] != ']') ) {
				i++;
			}
			if ( (i == iEnd) ||
				 !__xrtTemplateParseIndex(
					pParser,
					iIndexStart,
					i,
					&iIndex
				 ) ) {
				goto syntax_error;
			}
			i++;
			memset(&Path, 0, sizeof(Path));
			Path.Type = XRT_TEMPLATE_PATH_INDEX;
			Path.Offset = (uint32)iIndexStart;
			Path.Size = (uint32)((i - 1u) - iIndexStart);
			Path.Index = iIndex;
			if ( !__xrtTemplatePushPath(
				pParser,
				&Path,
				iStart,
				iEnd - iStart,
				iCount
			) ) {
				return false;
			}
			iCount++;
		}
		if ( i == iEnd ) {
			break;
		}
		if ( pParser->Template->Source[i] != '.' ) {
			goto syntax_error;
		}
		i++;
		if ( (i == iEnd) ||
			 !__xrtTemplateNameStart(pParser->Template->Source[i]) ) {
			goto syntax_error;
		}
	}
	*pPathCount = (uint32)iCount;
	return true;

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-path",
		"invalid template value path",
		pParser->Template,
		iStart,
		iEnd - iStart
	);
	return false;
}



/* 查找标签关闭标记，反斜杠保护的字节不会参与匹配。 */
static bool __xrtTemplateFindClose(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t* pClose
)
{
	size_t i = iStart;

	while ( i < pParser->Template->SourceSize ) {
		if ( pParser->Template->Source[i] == '\\' ) {
			if ( (i + 1u) >= pParser->Template->SourceSize ) {
				break;
			}
			i += 2u;
			continue;
		}
		if ( __xrtTemplateMarkerAt(
			pParser->Template,
			i,
			pParser->Config->Close
		) ) {
			*pClose = i;
			return true;
		}
		i++;
	}
	return false;
}



/* 把格式文本解转义后追加到模板私有文本池。 */
bool __xrtTemplateTextUnescape(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pOffset,
	uint32* pSize
)
{
	size_t iOldSize = pParser->Template->Text.Size;
	size_t i = iStart;

	while ( i < iEnd ) {
		if ( (pParser->Template->Source[i] == '\\') &&
			 ((i + 1u) < iEnd) ) {
			i++;
		}
		if ( !xrtStrBufAppendByte(
			&pParser->Template->Text,
			pParser->Template->Source[i]
		) ) {
			return false;
		}
		i++;
	}
	*pOffset = (uint32)iOldSize;
	*pSize = (uint32)(pParser->Template->Text.Size - iOldSize);
	return true;
}



/* 编译一个核心输出标签。 */
static bool __xrtTemplateCompileOutput(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iBodyStart,
	size_t iBodyEnd,
	size_t iTagEnd
)
{
	xrt_template_node Node;
	size_t iExprStart;
	size_t iExprEnd;
	size_t iFormatStart;
	size_t i;
	char iPrefix;
	bool bHasFormat = false;

	__xrtTemplateTrim(&iBodyStart, &iBodyEnd,
		pParser->Template->Source);
	iFormatStart = iBodyEnd;
	if ( iBodyStart == iBodyEnd ) {
		goto syntax_error;
	}
	iPrefix = pParser->Template->Source[iBodyStart++];
	if ( (iPrefix != '$') && (iPrefix != '%') && (iPrefix != '&') ) {
		goto syntax_error;
	}
	iExprStart = iBodyStart;
	iExprEnd = iBodyEnd;
	for ( i = iBodyStart; i < iBodyEnd; i++ ) {
		if ( pParser->Template->Source[i] == '\\' ) {
			i++;
			continue;
		}
		if ( pParser->Template->Source[i] == ':' ) {
			iExprEnd = i;
			iFormatStart = i + 1u;
			bHasFormat = true;
			break;
		}
	}
	__xrtTemplateTrim(&iExprStart, &iExprEnd,
		pParser->Template->Source);
	__xrtTemplateTrim(&iFormatStart, &iBodyEnd,
		pParser->Template->Source);
	if ( (iExprStart == iExprEnd) ||
		 ((iPrefix == '$') && bHasFormat) ) {
		goto syntax_error;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_OUTPUT;
	Node.Output = iPrefix == '$' ? XTEMPLATE_OUTPUT_TEXT :
		(iPrefix == '%' ? XTEMPLATE_OUTPUT_NUMBER : XTEMPLATE_OUTPUT_TIME);
	Node.SourceOffset = (uint32)iTagStart;
	Node.SourceSize = (uint32)(iTagEnd - iTagStart);
	Node.Data.Output.ExpressionOffset = (uint32)iExprStart;
	Node.Data.Output.ExpressionSize = (uint32)(iExprEnd - iExprStart);
	if ( !__xrtTemplateCompilePath(
		pParser,
		iExprStart,
		iExprEnd,
		&Node.Data.Output.PathStart,
		&Node.Data.Output.PathCount
	) ) {
		return false;
	}
	if ( !__xrtTemplateTextUnescape(
		pParser,
		iFormatStart,
		iBodyEnd,
		&Node.Data.Output.FormatOffset,
		&Node.Data.Output.FormatSize
	) ) {
		return false;
	}
	return __xrtTemplatePushNode(pParser, &Node);

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-output",
		"invalid template output tag",
		pParser->Template,
		iTagStart,
		iTagEnd - iTagStart
	);
	return false;
}



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

/* 控制块递归解析返回遇到的边界标签及其参数区间。 */
typedef enum xrt_template_stop_type {
	XRT_TEMPLATE_STOP_NONE = 0,
	XRT_TEMPLATE_STOP_ELSEIF,
	XRT_TEMPLATE_STOP_ELSE,
	XRT_TEMPLATE_STOP_END
} xrt_template_stop_type;



/* 边界标签位置用于父控制节点继续编译并报告错误。 */
typedef struct xrt_template_stop {
	xrt_template_stop_type Type;
	size_t SourceStart;
	size_t SourceEnd;
	size_t ArgumentStart;
	size_t ArgumentEnd;
} xrt_template_stop;



/* 判断源区间是否与固定 ASCII 名称完全相同。 */
static bool __xrtTemplateNameEqual(
	const xtemplate* pTemplate,
	size_t iStart,
	size_t iEnd,
	cstr sName
)
{
	size_t iSize = strlen(sName);

	return ((iEnd - iStart) == iSize) &&
		(memcmp(pTemplate->Source + iStart, sName, iSize) == 0);
}



/* 返回下一个顶层冒号参数，位置超过结尾表示全部完成。 */
static bool __xrtTemplateNextArgument(
	xrt_template_parser* pParser,
	size_t* pPosition,
	size_t iEnd,
	size_t* pStart,
	size_t* pEnd
)
{
	size_t iPartStart = *pPosition;
	size_t iDepth = 0;
	char iQuote = 0;

	for ( size_t i = iPartStart; i <= iEnd; i++ ) {
		char iByte = i < iEnd ? pParser->Template->Source[i] : ':';

		if ( (i < iEnd) && (iByte == '\\') ) {
			i++;
			continue;
		}
		if ( iQuote != 0 ) {
			if ( iByte == iQuote ) {
				iQuote = 0;
			}
			continue;
		}
		if ( (iByte == '\'') || (iByte == '"') ) {
			iQuote = iByte;
			continue;
		}
		if ( iByte == '(' ) {
			iDepth++;
			continue;
		}
		if ( iByte == ')' ) {
			if ( iDepth == 0 ) {
				goto syntax_error;
			}
			iDepth--;
			continue;
		}
		if ( (iByte != ':') || (iDepth != 0) ) {
			continue;
		}
		*pStart = iPartStart;
		*pEnd = i;
		__xrtTemplateTrim(
			pStart,
			pEnd,
			pParser->Template->Source
		);
		*pPosition = i + 1u;
		return true;
	}

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-control",
		"invalid template control argument list",
		pParser->Template,
		iPartStart,
		iEnd - iPartStart
	);
	return false;
}



/* 按引号、转义和括号层级切分固定容量的顶层冒号参数。 */
static bool __xrtTemplateSplitArguments(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	size_t* pStarts,
	size_t* pEnds,
	size_t iCapacity,
	size_t* pCount
)
{
	size_t iPosition = iStart;
	size_t iCount = 0;

	while ( iPosition <= iEnd ) {
		if ( iCount == iCapacity ) {
			__xrtTemplateError(
				XERR_VALUE,
				XTEMPLATE_ERROR_SYNTAX,
				"compile-control",
				"invalid template control argument list",
				pParser->Template,
				iStart,
				iEnd - iStart
			);
			return false;
		}
		if ( !__xrtTemplateNextArgument(
			pParser,
			&iPosition,
			iEnd,
			&pStarts[iCount],
			&pEnds[iCount]
		) ) {
			return false;
		}
		iCount++;
	}
	*pCount = iCount;
	return true;
}



/* 在统一节点预算内追加控制节点并返回索引。 */
static bool __xrtTemplatePushControlNode(
	xrt_template_parser* pParser,
	const xrt_template_node* pNode,
	uint32* pIndex
)
{
	*pIndex = (uint32)pParser->Template->Nodes.Count;
	return __xrtTemplatePushNode(pParser, pNode);
}



#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)

/* 编译非空引号字符串，并把解转义名称保存到模板私有文本池。 */
static bool __xrtTemplateCompileStaticName(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pOffset,
	uint32* pSize
)
{
	char iQuote;
	size_t i;

	if ( (iEnd - iStart) < 2u ) {
		goto syntax_error;
	}
	iQuote = pParser->Template->Source[iStart];
	if ( ((iQuote != '\'') && (iQuote != '"')) ||
		 (pParser->Template->Source[iEnd - 1u] != iQuote) ) {
		goto syntax_error;
	}
	for ( i = iStart + 1u; i < (iEnd - 1u); i++ ) {
		if ( pParser->Template->Source[i] == '\\' ) {
			if ( (i + 1u) >= (iEnd - 1u) ) {
				goto syntax_error;
			}
			i++;
			continue;
		}
		if ( pParser->Template->Source[i] == iQuote ) {
			goto syntax_error;
		}
	}
	if ( iStart + 1u == iEnd - 1u ) {
		goto syntax_error;
	}
	return __xrtTemplateTextUnescape(
		pParser,
		iStart + 1u,
		iEnd - 1u,
		pOffset,
		pSize
	);

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-define",
		"template definition name must be one non-empty quoted string",
		pParser->Template,
		iStart,
		iEnd - iStart
	);
	return false;
}



#endif



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 判断参数左侧是否是可作为命名参数的完整标识符。 */
static bool __xrtTemplateArgumentName(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd
)
{
	if ( (iStart == iEnd) ||
		 !__xrtTemplateNameStart(pParser->Template->Source[iStart]) ) {
		return false;
	}
	for ( size_t i = iStart + 1u; i < iEnd; i++ ) {
		if ( !__xrtTemplateNameByte(pParser->Template->Source[i]) ) {
			return false;
		}
	}
	return true;
}



/* 从参数中识别顶层 name=expression 形式，比较表达式可用 == 消除歧义。 */
static void __xrtTemplateNamedArgument(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	size_t* pNameStart,
	size_t* pNameEnd,
	size_t* pExpressionStart
)
{
	size_t iDepth = 0;
	char iQuote = 0;

	*pNameStart = iEnd;
	*pNameEnd = iEnd;
	*pExpressionStart = iStart;
	for ( size_t i = iStart; i < iEnd; i++ ) {
		char iByte = pParser->Template->Source[i];

		if ( iByte == '\\' ) {
			i++;
			continue;
		}
		if ( iQuote != 0 ) {
			if ( iByte == iQuote ) {
				iQuote = 0;
			}
			continue;
		}
		if ( (iByte == '\'') || (iByte == '"') ) {
			iQuote = iByte;
			continue;
		}
		if ( iByte == '(' ) {
			iDepth++;
			continue;
		}
		if ( iByte == ')' ) {
			if ( iDepth != 0 ) {
				iDepth--;
			}
			continue;
		}
		if ( (iByte != '=') || (iDepth != 0) ||
			 ((i + 1u) < iEnd &&
			  (pParser->Template->Source[i + 1u] == '=')) ||
			 (i > iStart &&
			  ((pParser->Template->Source[i - 1u] == '!') ||
			   (pParser->Template->Source[i - 1u] == '>') ||
			   (pParser->Template->Source[i - 1u] == '<') ||
			   (pParser->Template->Source[i - 1u] == '~'))) ) {
			continue;
		}
		*pNameStart = iStart;
		*pNameEnd = i;
		__xrtTemplateTrim(
			pNameStart,
			pNameEnd,
			pParser->Template->Source
		);
		if ( !__xrtTemplateArgumentName(
			pParser,
			*pNameStart,
			*pNameEnd
		) ) {
			*pNameStart = iEnd;
			*pNameEnd = iEnd;
			return;
		}
		*pExpressionStart = i + 1u;
		__xrtTemplateTrim(
			pExpressionStart,
			&iEnd,
			pParser->Template->Source
		);
		return;
	}
}



/* 判断当前调用已经保存的命名参数是否与新名称重复。 */
static bool __xrtTemplateArgumentDuplicated(
	xrt_template_parser* pParser,
	uint32 iStart,
	uint32 iCount,
	size_t iNameStart,
	size_t iNameEnd
)
{
	size_t iSize = iNameEnd - iNameStart;

	for ( uint32 i = 0; i < iCount; i++ ) {
		const xrt_template_argument* pArgument =
			(const xrt_template_argument*)xrtArrayConstGet(
				&pParser->Template->Arguments,
				(size_t)iStart + i
			);

		if ( (pArgument != NULL) &&
			 (pArgument->NameSize == iSize) &&
			 (memcmp(
				pParser->Template->Source + pArgument->NameOffset,
				pParser->Template->Source + iNameStart,
				iSize
			 ) == 0) ) {
			return true;
		}
	}
	return false;
}



/* 编译扩展调用参数并执行总量、单调用数量和重复名称检查。 */
static bool __xrtTemplateCompileExtensionArguments(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	bool bHasArguments,
	uint32* pStart,
	uint32* pCount
)
{
	size_t iPosition = iStart;
	uint32 iCount = 0;

	*pStart = (uint32)pParser->Template->Arguments.Count;
	*pCount = 0;
	if ( !bHasArguments ) {
		return true;
	}
	while ( iPosition <= iEnd ) {
		xrt_template_argument Argument;
		size_t iArgumentStart;
		size_t iArgumentEnd;
		size_t iNameStart;
		size_t iNameEnd;
		size_t iExpressionStart;

		if ( (iCount >= pParser->Config->MaxCallArguments) ||
			 (pParser->Template->Arguments.Count >=
			  pParser->Config->MaxArguments) ) {
			__xrtTemplateError(
				XERR_RANGE,
				XTEMPLATE_ERROR_LIMIT,
				"compile-extension",
				"template extension argument limit exceeded",
				pParser->Template,
				iStart,
				iEnd - iStart
			);
			return false;
		}
		if ( !__xrtTemplateNextArgument(
			pParser,
			&iPosition,
			iEnd,
			&iArgumentStart,
			&iArgumentEnd
		) ) {
			return false;
		}
		if ( iArgumentStart == iArgumentEnd ) {
			goto syntax_error;
		}
		__xrtTemplateNamedArgument(
			pParser,
			iArgumentStart,
			iArgumentEnd,
			&iNameStart,
			&iNameEnd,
			&iExpressionStart
		);
		if ( iExpressionStart == iArgumentEnd ) {
			goto syntax_error;
		}
		if ( (iNameStart != iNameEnd) &&
			 __xrtTemplateArgumentDuplicated(
				pParser,
				*pStart,
				iCount,
				iNameStart,
				iNameEnd
			 ) ) {
			__xrtTemplateError(
				XERR_EXISTS,
				XTEMPLATE_ERROR_SYNTAX,
				"compile-extension",
				"template extension named argument is duplicated",
				pParser->Template,
				iNameStart,
				iNameEnd - iNameStart
			);
			return false;
		}
		memset(&Argument, 0, sizeof(Argument));
		Argument.NameOffset = (uint32)iNameStart;
		Argument.NameSize = (uint32)(iNameEnd - iNameStart);
		Argument.SourceOffset = (uint32)iExpressionStart;
		Argument.SourceSize =
			(uint32)(iArgumentEnd - iExpressionStart);
		if ( !__xrtTemplateCompileExpression(
			pParser,
			iExpressionStart,
			iArgumentEnd,
			&Argument.Expression
		) || !xrtArrayPush(
			&pParser->Template->Arguments,
			&Argument
		) ) {
			return false;
		}
		iCount++;
	}
	*pCount = iCount;
	return true;

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-extension",
		"template extension contains an empty argument",
		pParser->Template,
		iStart,
		iEnd - iStart
	);
	return false;
}



#endif



/* 在统一分支预算内追加一个条件分支。 */
static bool __xrtTemplatePushBranch(
	xrt_template_parser* pParser,
	xrt_template_branch* pBranch,
	uint32* pFirst,
	uint32* pPrevious,
	size_t iErrorStart,
	size_t iErrorEnd
)
{
	uint32 iIndex;

	if ( pParser->Template->Branches.Count >=
		 pParser->Config->MaxNodes ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"compile-if",
			"template branch limit exceeded",
			pParser->Template,
			iErrorStart,
			iErrorEnd - iErrorStart
		);
		return false;
	}
	iIndex = (uint32)pParser->Template->Branches.Count;
	pBranch->Next = XRT_TEMPLATE_INDEX_NONE;
	if ( !xrtArrayPush(&pParser->Template->Branches, pBranch) ) {
		return false;
	}
	if ( *pPrevious == XRT_TEMPLATE_INDEX_NONE ) {
		*pFirst = iIndex;
	} else {
		xrt_template_branch* pPreviousBranch =
			(xrt_template_branch*)xrtArrayGet(
				&pParser->Template->Branches,
				*pPrevious
			);

		if ( pPreviousBranch == NULL ) {
			return false;
		}
		pPreviousBranch->Next = iIndex;
	}
	*pPrevious = iIndex;
	return true;
}



/* 编译内联条件文本，两个结果只在选中时写出。 */
static bool __xrtTemplateCompileInlineIf(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iBodyStart,
	size_t iBodyEnd,
	size_t iTagEnd
)
{
	xrt_template_node Node;
	size_t arrStart[3];
	size_t arrEnd[3];
	size_t iCount;

	if ( !__xrtTemplateSplitArguments(
		pParser,
		iBodyStart + 1u,
		iBodyEnd,
		arrStart,
		arrEnd,
		3u,
		&iCount
	) ) {
		return false;
	}
	if ( (iCount != 3u) || (arrStart[0] == arrEnd[0]) ) {
		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-inline-if",
			"inline template condition requires expression, true text and false text",
			pParser->Template,
			iTagStart,
			iTagEnd - iTagStart
		);
		return false;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_INLINE_IF;
	Node.SourceOffset = (uint32)iTagStart;
	Node.SourceSize = (uint32)(iTagEnd - iTagStart);
	Node.Data.InlineIf.ExpressionOffset = (uint32)arrStart[0];
	Node.Data.InlineIf.ExpressionSize =
		(uint32)(arrEnd[0] - arrStart[0]);
	if ( !__xrtTemplateCompileExpression(
		pParser,
		arrStart[0],
		arrEnd[0],
		&Node.Data.InlineIf.Expression
	) || !__xrtTemplateTextUnescape(
		pParser,
		arrStart[1],
		arrEnd[1],
		&Node.Data.InlineIf.TrueOffset,
		&Node.Data.InlineIf.TrueSize
	) || !__xrtTemplateTextUnescape(
		pParser,
		arrStart[2],
		arrEnd[2],
		&Node.Data.InlineIf.FalseOffset,
		&Node.Data.InlineIf.FalseSize
	) ) {
		return false;
	}
	return __xrtTemplatePushNode(pParser, &Node);
}



static bool __xrtTemplateParseControlNodes(
	xrt_template_parser* pParser,
	bool bAllowStop,
	xrt_template_stop* pStop
);



#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)

/* 编译一个模板内定义，定义本身不输出且名称在模板内唯一。 */
static bool __xrtTemplateCompileDefine(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iArgumentStart,
	size_t iArgumentEnd
)
{
	xrt_template_node Node;
	xrt_template_stop Stop = { 0 };
	xrt_template_node* pNode;
	xstrview Name;
	uint32 iNode;

	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_DEFINE;
	Node.SourceOffset = (uint32)iTagStart;
	if ( !__xrtTemplateCompileStaticName(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		&Node.Data.Define.NameOffset,
		&Node.Data.Define.NameSize
	) || !__xrtTemplatePushControlNode(pParser, &Node, &iNode) ) {
		return false;
	}
	Node.Data.Define.BodyStart =
		(uint32)pParser->Template->Nodes.Count;
	if ( !__xrtTemplateParseControlNodes(pParser, true, &Stop) ) {
		return false;
	}
	if ( Stop.Type != XRT_TEMPLATE_STOP_END ) {
		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-define",
			"template definition requires one matching end tag",
			pParser->Template,
			iTagStart,
			Stop.SourceEnd - iTagStart
		);
		return false;
	}
	pNode = (xrt_template_node*)xrtArrayGet(
		&pParser->Template->Nodes,
		iNode
	);
	if ( pNode == NULL ) {
		return false;
	}
	pNode->SourceSize = (uint32)(Stop.SourceEnd - iTagStart);
	pNode->Data.Define.BodyStart = Node.Data.Define.BodyStart;
	pNode->Data.Define.BodyEnd =
		(uint32)pParser->Template->Nodes.Count;
	pNode->Data.Define.Next =
		(uint32)pParser->Template->Nodes.Count;
	Name = (xstrview){
		pParser->Template->Text.Data + pNode->Data.Define.NameOffset,
		pNode->Data.Define.NameSize
	};
	return __xrtTemplateDefinitionAdd(
		pParser->Template,
		Name,
		iNode
	);
}



/* 编译动态 include 名称表达式，解析与所有权留给渲染配置。 */
static bool __xrtTemplateCompileInclude(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iArgumentStart,
	size_t iArgumentEnd,
	size_t iTagEnd
)
{
	xrt_template_node Node;

	if ( iArgumentStart == iArgumentEnd ) {
		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-include",
			"template include requires one name expression",
			pParser->Template,
			iTagStart,
			iTagEnd - iTagStart
		);
		return false;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_INCLUDE;
	Node.SourceOffset = (uint32)iTagStart;
	Node.SourceSize = (uint32)(iTagEnd - iTagStart);
	Node.Data.Include.ExpressionOffset = (uint32)iArgumentStart;
	Node.Data.Include.ExpressionSize =
		(uint32)(iArgumentEnd - iArgumentStart);
	if ( !__xrtTemplateCompileExpression(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		&Node.Data.Include.Expression
	) ) {
		return false;
	}
	return __xrtTemplatePushNode(pParser, &Node);
}



/* 查找原样块的精确 end 标签，中间内容完全不参与模板解析。 */
static bool __xrtTemplateFindRawBody(
	xrt_template_parser* pParser,
	size_t iTagStart,
	uint32* pOffset,
	uint32* pSize,
	size_t* pSourceEnd
)
{
	size_t iRawStart = pParser->Position;
	size_t i = pParser->Position;

	while ( i < pParser->Template->SourceSize ) {
		size_t iBodyStart;
		size_t iBodyEnd;
		size_t iClose;
		size_t iEnd;

		while ( (i < pParser->Template->SourceSize) &&
			 !__xrtTemplateMarkerAt(
				pParser->Template,
				i,
				pParser->Config->Open
			 ) ) {
			i++;
		}
		if ( i == pParser->Template->SourceSize ) {
			break;
		}
		if ( __xrtTemplateMarkerAt(
			pParser->Template,
			i + pParser->Config->Open.Size,
			pParser->Config->Open
		) ) {
			i += pParser->Config->Open.Size * 2u;
			continue;
		}
		iBodyStart = i + pParser->Config->Open.Size;
		if ( !__xrtTemplateFindClose(pParser, iBodyStart, &iClose) ) {
			break;
		}
		iBodyEnd = iClose;
		iEnd = iClose + pParser->Config->Close.Size;
		__xrtTemplateTrim(
			&iBodyStart,
			&iBodyEnd,
			pParser->Template->Source
		);
		if ( __xrtTemplateNameEqual(
			pParser->Template,
			iBodyStart,
			iBodyEnd,
			"#end"
		) ) {
			*pOffset = (uint32)iRawStart;
			*pSize = (uint32)(i - iRawStart);
			*pSourceEnd = iEnd;
			pParser->Position = iEnd;
			return true;
		}
		i = iEnd;
	}
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-raw",
		"template raw block has no end tag",
		pParser->Template,
		iTagStart,
		pParser->Template->SourceSize - iTagStart
	);
	return false;
}



/* 编译内建 raw 节点并保存原样主体。 */
static bool __xrtTemplateCompileRaw(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iTagEnd
)
{
	xrt_template_node Node;
	size_t iSourceEnd;

	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_RAW;
	Node.SourceOffset = (uint32)iTagStart;
	if ( !__xrtTemplateFindRawBody(
		pParser,
		iTagStart,
		&Node.Data.Raw.Offset,
		&Node.Data.Raw.Size,
		&iSourceEnd
	) ) {
		return false;
	}
	Node.SourceSize = (uint32)(iSourceEnd - iTagStart);
	(void)iTagEnd;
	return __xrtTemplatePushNode(pParser, &Node);
}



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 编译一个已经解析名称和注册定义的扩展调用节点。 */
static bool __xrtTemplateCompileExtension(
	xrt_template_parser* pParser,
	const xrt_template_extension_def* pDefinition,
	size_t iTagStart,
	size_t iNameStart,
	size_t iNameEnd,
	size_t iArgumentStart,
	size_t iArgumentEnd,
	bool bHasArguments,
	size_t iTagEnd
)
{
	xrt_template_node Node;
	xrt_template_stop Stop = { 0 };
	xrt_template_node* pCompiled;
	uint32 iNode;
	size_t iSourceEnd = iTagEnd;
	bool bParsed;

	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_EXTENSION;
	Node.SourceOffset = (uint32)iTagStart;
	Node.SourceSize = (uint32)(iTagEnd - iTagStart);
	Node.Data.Extension.Definition = pDefinition;
	Node.Data.Extension.NameOffset = (uint32)iNameStart;
	Node.Data.Extension.NameSize = (uint32)(iNameEnd - iNameStart);
	if ( !__xrtTemplateCompileExtensionArguments(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		bHasArguments,
		&Node.Data.Extension.ArgumentStart,
		&Node.Data.Extension.ArgumentCount
	) ) {
		return false;
	}
	if ( (Node.Data.Extension.ArgumentCount <
		  pDefinition->MinArguments) ||
		 (Node.Data.Extension.ArgumentCount >
		  pDefinition->MaxArguments) ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-extension",
			"template extension argument count is outside its registered range",
			pParser->Template,
			iTagStart,
			iTagEnd - iTagStart
		);
		return false;
	}
	if ( !__xrtTemplatePushControlNode(pParser, &Node, &iNode) ) {
		return false;
	}
	if ( (pDefinition->Type == XTEMPLATE_EXTENSION_FUNCTION) ||
		 (pDefinition->Type == XTEMPLATE_EXTENSION_STATEMENT) ) {
		pCompiled = (xrt_template_node*)xrtArrayGet(
			&pParser->Template->Nodes,
			iNode
		);
		if ( pCompiled == NULL ) {
			return false;
		}
		pCompiled->Data.Extension.Next =
			(uint32)pParser->Template->Nodes.Count;
		return true;
	}
	if ( pDefinition->Type == XTEMPLATE_EXTENSION_RAW_BLOCK ) {
		if ( !__xrtTemplateFindRawBody(
			pParser,
			iTagStart,
			&Node.Data.Extension.RawOffset,
			&Node.Data.Extension.RawSize,
			&iSourceEnd
		) ) {
			return false;
		}
	} else {
		if ( pParser->Depth >= pParser->Config->MaxBlockDepth ) {
			__xrtTemplateError(
				XERR_RANGE,
				XTEMPLATE_ERROR_LIMIT,
				"compile-extension",
				"template block depth limit exceeded",
				pParser->Template,
				iTagStart,
				iTagEnd - iTagStart
			);
			return false;
		}
		Node.Data.Extension.BodyStart =
			(uint32)pParser->Template->Nodes.Count;
		pParser->Depth++;
		bParsed = __xrtTemplateParseControlNodes(
			pParser,
			true,
			&Stop
		);
		pParser->Depth--;
		if ( !bParsed ) {
			return false;
		}
		if ( Stop.Type != XRT_TEMPLATE_STOP_END ) {
			__xrtTemplateError(
				XERR_VALUE,
				XTEMPLATE_ERROR_SYNTAX,
				"compile-extension",
				"template extension block requires one matching end tag",
				pParser->Template,
				iTagStart,
				Stop.SourceEnd - iTagStart
			);
			return false;
		}
		iSourceEnd = Stop.SourceEnd;
		Node.Data.Extension.BodyEnd =
			(uint32)pParser->Template->Nodes.Count;
	}
	pCompiled = (xrt_template_node*)xrtArrayGet(
		&pParser->Template->Nodes,
		iNode
	);
	if ( pCompiled == NULL ) {
		return false;
	}
	pCompiled->SourceSize = (uint32)(iSourceEnd - iTagStart);
	pCompiled->Data.Extension.BodyStart =
		Node.Data.Extension.BodyStart;
	pCompiled->Data.Extension.BodyEnd = Node.Data.Extension.BodyEnd;
	pCompiled->Data.Extension.RawOffset = Node.Data.Extension.RawOffset;
	pCompiled->Data.Extension.RawSize = Node.Data.Extension.RawSize;
	pCompiled->Data.Extension.Next =
		(uint32)pParser->Template->Nodes.Count;
	return true;
}



/* 解析并绑定 {@name:arguments} 函数调用。 */
static bool __xrtTemplateCompileFunction(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iBodyStart,
	size_t iBodyEnd,
	size_t iTagEnd
)
{
	const xrt_template_extension_def* pDefinition;
	size_t iNameStart = iBodyStart + 1u;
	size_t iNameEnd = iNameStart;
	size_t iArgumentStart;
	bool bHasArguments;

	while ( (iNameEnd < iBodyEnd) &&
		 (pParser->Template->Source[iNameEnd] != ':') ) {
		iNameEnd++;
	}
	bHasArguments = iNameEnd < iBodyEnd;
	iArgumentStart = bHasArguments ? iNameEnd + 1u : iBodyEnd;
	__xrtTemplateTrim(
		&iNameStart,
		&iNameEnd,
		pParser->Template->Source
	);
	__xrtTemplateTrim(
		&iArgumentStart,
		&iBodyEnd,
		pParser->Template->Source
	);
	if ( !__xrtTemplateArgumentName(
		pParser,
		iNameStart,
		iNameEnd
	) ) {
		goto syntax_error;
	}
	pDefinition = __xrtTemplateExtensionFind(
		pParser->Template->Registry,
		XTEMPLATE_EXTENSION_FUNCTION,
		(xstrview){
			pParser->Template->Source + iNameStart,
			iNameEnd - iNameStart
		}
	);
	if ( pDefinition == NULL ) {
		__xrtTemplateError(
			XERR_NOT_FOUND,
			XTEMPLATE_ERROR_UNDEFINED,
			"compile-function",
			"template function is not registered",
			pParser->Template,
			iNameStart,
			iNameEnd - iNameStart
		);
		return false;
	}
	return __xrtTemplateCompileExtension(
		pParser,
		pDefinition,
		iTagStart,
		iNameStart,
		iNameEnd,
		iArgumentStart,
		iBodyEnd,
		bHasArguments,
		iTagEnd
	);

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-function",
		"invalid template function tag",
		pParser->Template,
		iTagStart,
		iTagEnd - iTagStart
	);
	return false;
}



#endif



#endif



/* 编译 if、elseif 和 else 分支及其扁平子节点范围。 */
static bool __xrtTemplateCompileIf(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iArgumentStart,
	size_t iArgumentEnd
)
{
	xrt_template_node Node;
	xrt_template_stop Stop;
	xrt_template_branch Branch;
	uint32 iNode;
	uint32 iExpression;
	uint32 iFirstBranch = XRT_TEMPLATE_INDEX_NONE;
	uint32 iPreviousBranch = XRT_TEMPLATE_INDEX_NONE;
	uint32 iBranchCount = 0;
	size_t iBranchStart;

	if ( iArgumentStart == iArgumentEnd ) {
		goto empty_condition;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_IF;
	Node.SourceOffset = (uint32)iTagStart;
	Node.Data.If.BranchStart = XRT_TEMPLATE_INDEX_NONE;
	if ( !__xrtTemplateCompileExpression(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		&iExpression
	) || !__xrtTemplatePushControlNode(pParser, &Node, &iNode) ) {
		return false;
	}
	for ( ;; ) {
		iBranchStart = pParser->Template->Nodes.Count;
		memset(&Stop, 0, sizeof(Stop));
		if ( !__xrtTemplateParseControlNodes(pParser, true, &Stop) ) {
			return false;
		}
		memset(&Branch, 0, sizeof(Branch));
		Branch.Expression = iExpression;
		Branch.ExpressionOffset = (uint32)iArgumentStart;
		Branch.ExpressionSize =
			(uint32)(iArgumentEnd - iArgumentStart);
		Branch.BodyStart = (uint32)iBranchStart;
		Branch.BodyEnd = (uint32)pParser->Template->Nodes.Count;
		if ( !__xrtTemplatePushBranch(
			pParser,
			&Branch,
			&iFirstBranch,
			&iPreviousBranch,
			iTagStart,
			Stop.SourceEnd
		) ) {
			return false;
		}
		iBranchCount++;
		if ( Stop.Type == XRT_TEMPLATE_STOP_END ) {
			break;
		}
		if ( Stop.Type == XRT_TEMPLATE_STOP_ELSEIF ) {
			iArgumentStart = Stop.ArgumentStart;
			iArgumentEnd = Stop.ArgumentEnd;
			if ( iArgumentStart == iArgumentEnd ) {
				goto empty_condition;
			}
			if ( !__xrtTemplateCompileExpression(
					pParser,
					iArgumentStart,
					iArgumentEnd,
					&iExpression
				) ) {
				return false;
			}
			continue;
		}
		if ( Stop.Type == XRT_TEMPLATE_STOP_ELSE ) {
			if ( Stop.ArgumentStart != Stop.ArgumentEnd ) {
				goto branch_error;
			}
			iBranchStart = pParser->Template->Nodes.Count;
			memset(&Stop, 0, sizeof(Stop));
			if ( !__xrtTemplateParseControlNodes(
				pParser,
				true,
				&Stop
			) ) {
				return false;
			}
			if ( Stop.Type != XRT_TEMPLATE_STOP_END ) {
				goto branch_error;
			}
			memset(&Branch, 0, sizeof(Branch));
			Branch.Expression = XRT_TEMPLATE_INDEX_NONE;
			Branch.BodyStart = (uint32)iBranchStart;
			Branch.BodyEnd =
				(uint32)pParser->Template->Nodes.Count;
			if ( !__xrtTemplatePushBranch(
				pParser,
				&Branch,
				&iFirstBranch,
				&iPreviousBranch,
				iTagStart,
				Stop.SourceEnd
			) ) {
				return false;
			}
			iBranchCount++;
			break;
		}
		goto branch_error;
	}
	{
		xrt_template_node* pNode =
			(xrt_template_node*)xrtArrayGet(
				&pParser->Template->Nodes,
				iNode
			);

		if ( pNode == NULL ) {
			return false;
		}
		pNode->SourceSize =
			(uint32)(Stop.SourceEnd - iTagStart);
		pNode->Data.If.BranchStart = iFirstBranch;
		pNode->Data.If.BranchCount = iBranchCount;
		pNode->Data.If.Next =
			(uint32)pParser->Template->Nodes.Count;
	}
	return true;

empty_condition:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-if",
		"template if branch requires a condition",
		pParser->Template,
		iTagStart,
		iArgumentEnd - iTagStart
	);
	return false;

branch_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-if",
		"invalid template if branch structure",
		pParser->Template,
		Stop.SourceStart,
		Stop.SourceEnd - Stop.SourceStart
	);
	return false;
}



/* 编译有界整数范围循环。 */
static bool __xrtTemplateCompileFor(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iArgumentStart,
	size_t iArgumentEnd
)
{
	xrt_template_node Node;
	xrt_template_stop Stop = { 0 };
	size_t arrStart[3];
	size_t arrEnd[3];
	size_t iCount;
	uint32 iNode;

	if ( !__xrtTemplateSplitArguments(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		arrStart,
		arrEnd,
		3u,
		&iCount
	) ) {
		return false;
	}
	if ( (iCount != 2u) && (iCount != 3u) ) {
		goto syntax_error;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_FOR;
	Node.SourceOffset = (uint32)iTagStart;
	Node.Data.For.StepExpression = XRT_TEMPLATE_INDEX_NONE;
	if ( !__xrtTemplateCompileExpression(
		pParser,
		arrStart[0],
		arrEnd[0],
		&Node.Data.For.StartExpression
	) || !__xrtTemplateCompileExpression(
		pParser,
		arrStart[1],
		arrEnd[1],
		&Node.Data.For.EndExpression
	) || ((iCount == 3u) && !__xrtTemplateCompileExpression(
		pParser,
		arrStart[2],
		arrEnd[2],
		&Node.Data.For.StepExpression
	)) || !__xrtTemplatePushControlNode(pParser, &Node, &iNode) ) {
		return false;
	}
	Node.Data.For.BodyStart =
		(uint32)pParser->Template->Nodes.Count;
	memset(&Stop, 0, sizeof(Stop));
	if ( !__xrtTemplateParseControlNodes(pParser, true, &Stop) ) {
		return false;
	}
	if ( Stop.Type != XRT_TEMPLATE_STOP_END ) {
		goto syntax_error;
	}
	{
		xrt_template_node* pNode =
			(xrt_template_node*)xrtArrayGet(
				&pParser->Template->Nodes,
				iNode
			);

		if ( pNode == NULL ) {
			return false;
		}
		pNode->Data.For.BodyStart = Node.Data.For.BodyStart;
		pNode->Data.For.BodyEnd =
			(uint32)pParser->Template->Nodes.Count;
		pNode->Data.For.Next =
			(uint32)pParser->Template->Nodes.Count;
		pNode->SourceSize =
			(uint32)(Stop.SourceEnd - iTagStart);
	}
	return true;

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-for",
		"template for requires start, end and optional step expressions followed by end",
		pParser->Template,
		iTagStart,
		Stop.SourceEnd > iTagStart
			? Stop.SourceEnd - iTagStart
			: iArgumentEnd - iTagStart
	);
	return false;
}



/* 编译容器遍历循环。 */
static bool __xrtTemplateCompileForeach(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iArgumentStart,
	size_t iArgumentEnd
)
{
	xrt_template_node Node;
	xrt_template_stop Stop = { 0 };
	uint32 iNode;

	if ( iArgumentStart == iArgumentEnd ) {
		goto syntax_error;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = XTEMPLATE_NODE_FOREACH;
	Node.SourceOffset = (uint32)iTagStart;
	Node.Data.Foreach.ExpressionOffset = (uint32)iArgumentStart;
	Node.Data.Foreach.ExpressionSize =
		(uint32)(iArgumentEnd - iArgumentStart);
	if ( !__xrtTemplateCompileExpression(
		pParser,
		iArgumentStart,
		iArgumentEnd,
		&Node.Data.Foreach.Expression
	) || !__xrtTemplatePushControlNode(pParser, &Node, &iNode) ) {
		return false;
	}
	Node.Data.Foreach.BodyStart =
		(uint32)pParser->Template->Nodes.Count;
	memset(&Stop, 0, sizeof(Stop));
	if ( !__xrtTemplateParseControlNodes(pParser, true, &Stop) ) {
		return false;
	}
	if ( Stop.Type != XRT_TEMPLATE_STOP_END ) {
		goto syntax_error;
	}
	{
		xrt_template_node* pNode =
			(xrt_template_node*)xrtArrayGet(
				&pParser->Template->Nodes,
				iNode
			);

		if ( pNode == NULL ) {
			return false;
		}
		pNode->Data.Foreach.BodyStart =
			Node.Data.Foreach.BodyStart;
		pNode->Data.Foreach.BodyEnd =
			(uint32)pParser->Template->Nodes.Count;
		pNode->Data.Foreach.Next =
			(uint32)pParser->Template->Nodes.Count;
		pNode->SourceSize =
			(uint32)(Stop.SourceEnd - iTagStart);
	}
	return true;

syntax_error:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-foreach",
		"template foreach requires one expression followed by end",
		pParser->Template,
		iTagStart,
		Stop.SourceEnd > iTagStart
			? Stop.SourceEnd - iTagStart
			: iArgumentEnd - iTagStart
	);
	return false;
}



/* 识别并编译一个控制标签，边界标签交回父块处理。 */
static bool __xrtTemplateCompileControlTag(
	xrt_template_parser* pParser,
	size_t iTagStart,
	size_t iBodyStart,
	size_t iBodyEnd,
	size_t iTagEnd,
	bool bAllowStop,
	xrt_template_stop* pStop
)
{
	xrt_template_node Node;
	size_t iNameStart;
	size_t iNameEnd;
	size_t iArgumentStart;
	size_t iArgumentEnd;
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		bool bHasArguments;
	#endif

	__xrtTemplateTrim(
		&iBodyStart,
		&iBodyEnd,
		pParser->Template->Source
	);
	if ( (iBodyStart == iBodyEnd) ||
		 (pParser->Template->Source[iBodyStart] != '#') ) {
		goto unknown;
	}
	iNameStart = ++iBodyStart;
	iNameEnd = iNameStart;
	while ( (iNameEnd < iBodyEnd) &&
			 (pParser->Template->Source[iNameEnd] != ':') ) {
		iNameEnd++;
	}
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		bHasArguments = iNameEnd < iBodyEnd;
	#endif
	iArgumentStart = iNameEnd < iBodyEnd ? iNameEnd + 1u : iBodyEnd;
	iArgumentEnd = iBodyEnd;
	__xrtTemplateTrim(
		&iNameStart,
		&iNameEnd,
		pParser->Template->Source
	);
	__xrtTemplateTrim(
		&iArgumentStart,
		&iArgumentEnd,
		pParser->Template->Source
	);
	if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "elseif"
	) || __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "else"
	) || __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "end"
	) ) {
		if ( !bAllowStop ) {
			goto unexpected_stop;
		}
		if ( !__xrtTemplateNameEqual(
			pParser->Template, iNameStart, iNameEnd, "elseif"
		) && (iArgumentStart != iArgumentEnd) ) {
			goto unknown;
		}
		pStop->Type = __xrtTemplateNameEqual(
			pParser->Template, iNameStart, iNameEnd, "elseif"
		) ? XRT_TEMPLATE_STOP_ELSEIF :
			(__xrtTemplateNameEqual(
				pParser->Template, iNameStart, iNameEnd, "else"
			) ? XRT_TEMPLATE_STOP_ELSE : XRT_TEMPLATE_STOP_END);
		pStop->SourceStart = iTagStart;
		pStop->SourceEnd = iTagEnd;
		pStop->ArgumentStart = iArgumentStart;
		pStop->ArgumentEnd = iArgumentEnd;
		return true;
	}
	if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "if"
	) ) {
		bool bResult;

		if ( pParser->Depth >= pParser->Config->MaxBlockDepth ) {
			goto depth_error;
		}
		pParser->Depth++;
		bResult = __xrtTemplateCompileIf(
			pParser,
			iTagStart,
			iArgumentStart,
			iArgumentEnd
		);
		pParser->Depth--;
		return bResult;
	}
	if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "for"
	) ) {
		bool bResult;

		if ( pParser->Depth >= pParser->Config->MaxBlockDepth ) {
			goto depth_error;
		}
		pParser->Depth++;
		bResult = __xrtTemplateCompileFor(
			pParser,
			iTagStart,
			iArgumentStart,
			iArgumentEnd
		);
		pParser->Depth--;
		return bResult;
	}
	if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "foreach"
	) ) {
		bool bResult;

		if ( pParser->Depth >= pParser->Config->MaxBlockDepth ) {
			goto depth_error;
		}
		pParser->Depth++;
		bResult = __xrtTemplateCompileForeach(
			pParser,
			iTagStart,
			iArgumentStart,
			iArgumentEnd
		);
		pParser->Depth--;
		return bResult;
	}
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		if ( __xrtTemplateNameEqual(
			pParser->Template, iNameStart, iNameEnd, "define"
		) ) {
			bool bResult;

			if ( pParser->Depth >= pParser->Config->MaxBlockDepth ) {
				goto depth_error;
			}
			pParser->Depth++;
			bResult = __xrtTemplateCompileDefine(
				pParser,
				iTagStart,
				iArgumentStart,
				iArgumentEnd
			);
			pParser->Depth--;
			return bResult;
		}
		if ( __xrtTemplateNameEqual(
			pParser->Template, iNameStart, iNameEnd, "include"
		) ) {
			return __xrtTemplateCompileInclude(
				pParser,
				iTagStart,
				iArgumentStart,
				iArgumentEnd,
				iTagEnd
			);
		}
		if ( __xrtTemplateNameEqual(
			pParser->Template, iNameStart, iNameEnd, "raw"
		) ) {
			if ( iArgumentStart != iArgumentEnd ) {
				goto unknown;
			}
			return __xrtTemplateCompileRaw(
				pParser,
				iTagStart,
				iTagEnd
			);
		}
	#endif
	memset(&Node, 0, sizeof(Node));
	if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "break"
	) ) {
		Node.Type = XTEMPLATE_NODE_BREAK;
	} else if ( __xrtTemplateNameEqual(
		pParser->Template, iNameStart, iNameEnd, "continue"
	) ) {
		Node.Type = XTEMPLATE_NODE_CONTINUE;
	} else {
		#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
			const xrt_template_extension_def* pDefinition =
				__xrtTemplateExtensionFind(
					pParser->Template->Registry,
					XTEMPLATE_EXTENSION_STATEMENT,
					(xstrview){
						pParser->Template->Source + iNameStart,
						iNameEnd - iNameStart
					}
				);

			if ( pDefinition == NULL ) {
				__xrtTemplateError(
					XERR_NOT_FOUND,
					XTEMPLATE_ERROR_UNDEFINED,
					"compile-statement",
					"template statement extension is not registered",
					pParser->Template,
					iTagStart,
					iTagEnd - iTagStart
				);
				return false;
			}
			return __xrtTemplateCompileExtension(
				pParser,
				pDefinition,
				iTagStart,
				iNameStart,
				iNameEnd,
				iArgumentStart,
				iArgumentEnd,
				bHasArguments,
				iTagEnd
			);
		#else
			goto unknown;
		#endif
	}
	if ( iArgumentStart != iArgumentEnd ) {
		goto unknown;
	}
	Node.SourceOffset = (uint32)iTagStart;
	Node.SourceSize = (uint32)(iTagEnd - iTagStart);
	return __xrtTemplatePushNode(pParser, &Node);

depth_error:
	__xrtTemplateError(
		XERR_RANGE,
		XTEMPLATE_ERROR_LIMIT,
		"compile-control",
		"template block depth limit exceeded",
		pParser->Template,
		iTagStart,
		iTagEnd - iTagStart
	);
	return false;

unexpected_stop:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-control",
		"template branch or end appears outside a block",
		pParser->Template,
		iTagStart,
		iTagEnd - iTagStart
	);
	return false;

unknown:
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-control",
		"unknown or malformed template control tag",
		pParser->Template,
		iTagStart,
		iTagEnd - iTagStart
	);
	return false;
}



/* 递归编译节点，遇到父块边界时停止并返回边界信息。 */
static bool __xrtTemplateParseControlNodes(
	xrt_template_parser* pParser,
	bool bAllowStop,
	xrt_template_stop* pStop
)
{
	size_t iTextStart = pParser->Position;

	if ( pStop != NULL ) {
		memset(pStop, 0, sizeof(*pStop));
	}
	while ( pParser->Position < pParser->Template->SourceSize ) {
		size_t iOpen = pParser->Position;
		size_t iBodyStart;
		size_t iBodyEnd;
		size_t iClose;
		size_t iTagEnd;

		while ( (iOpen < pParser->Template->SourceSize) &&
			 !__xrtTemplateMarkerAt(
				pParser->Template,
				iOpen,
				pParser->Config->Open
			 ) ) {
			iOpen++;
		}
		if ( iOpen == pParser->Template->SourceSize ) {
			break;
		}
		if ( __xrtTemplateMarkerAt(
			pParser->Template,
			iOpen + pParser->Config->Open.Size,
			pParser->Config->Open
		) ) {
			if ( !__xrtTemplatePushText(
				pParser,
				iTextStart,
				iOpen - iTextStart
			) || !__xrtTemplatePushText(
				pParser,
				iOpen,
				pParser->Config->Open.Size
			) ) {
				return false;
			}
			pParser->Position = iOpen +
				(pParser->Config->Open.Size * 2u);
			iTextStart = pParser->Position;
			continue;
		}
		if ( !__xrtTemplatePushText(
			pParser,
			iTextStart,
			iOpen - iTextStart
		) ) {
			return false;
		}
		iBodyStart = iOpen + pParser->Config->Open.Size;
		if ( !__xrtTemplateFindClose(pParser, iBodyStart, &iClose) ) {
			__xrtTemplateError(
				XERR_VALUE,
				XTEMPLATE_ERROR_SYNTAX,
				"compile",
				"template tag has no closing marker",
				pParser->Template,
				iOpen,
				pParser->Template->SourceSize - iOpen
			);
			return false;
		}
		iBodyEnd = iClose;
		iTagEnd = iClose + pParser->Config->Close.Size;
		pParser->Position = iTagEnd;
		{
			size_t iTrimStart = iBodyStart;
			size_t iTrimEnd = iBodyEnd;

			__xrtTemplateTrim(
				&iTrimStart,
				&iTrimEnd,
				pParser->Template->Source
			);
			if ( iTrimStart == iTrimEnd ) {
				__xrtTemplateError(
					XERR_VALUE,
					XTEMPLATE_ERROR_SYNTAX,
					"compile",
					"template tag is empty",
					pParser->Template,
					iOpen,
					iTagEnd - iOpen
				);
				return false;
			}
			if ( pParser->Template->Source[iTrimStart] == '#' ) {
				if ( !__xrtTemplateCompileControlTag(
					pParser,
					iOpen,
					iTrimStart,
					iTrimEnd,
					iTagEnd,
					bAllowStop,
					pStop
				) ) {
					return false;
				}
				if ( (pStop != NULL) &&
					 (pStop->Type != XRT_TEMPLATE_STOP_NONE) ) {
					return true;
				}
			#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
			} else if ( pParser->Template->Source[iTrimStart] == '@' ) {
				if ( !__xrtTemplateCompileFunction(
					pParser,
					iOpen,
					iTrimStart,
					iTrimEnd,
					iTagEnd
				) ) {
					return false;
				}
			#endif
			} else if ( pParser->Template->Source[iTrimStart] == '?' ) {
				if ( !__xrtTemplateCompileInlineIf(
					pParser,
					iOpen,
					iTrimStart,
					iTrimEnd,
					iTagEnd
				) ) {
					return false;
				}
			} else if ( !__xrtTemplateCompileOutput(
				pParser,
				iOpen,
				iTrimStart,
				iTrimEnd,
				iTagEnd
			) ) {
				return false;
			}
		}
		iTextStart = pParser->Position;
	}
	if ( !__xrtTemplatePushText(
		pParser,
		iTextStart,
		pParser->Template->SourceSize - iTextStart
	) ) {
		return false;
	}
	if ( bAllowStop ) {
		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-control",
			"template block has no end tag",
			pParser->Template,
			iTextStart,
			pParser->Template->SourceSize - iTextStart
		);
		return false;
	}
	return true;
}

#endif



/* 把模板源代码一次扫描编译为文本与输出节点。 */
bool __xrtTemplateParse(
	xtemplate* pTemplate,
	const xtemplateconfig* pConfig
)
{
	xrt_template_parser Parser;

	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		memset(&Parser, 0, sizeof(Parser));
		Parser.Template = pTemplate;
		Parser.Config = pConfig;
		return __xrtTemplateParseControlNodes(
			&Parser,
			false,
			NULL
		);
	#else
	size_t iTextStart = 0;

	memset(&Parser, 0, sizeof(Parser));
	Parser.Template = pTemplate;
	Parser.Config = pConfig;
	while ( Parser.Position < pTemplate->SourceSize ) {
		size_t iOpen = Parser.Position;
		size_t iBodyStart;
		size_t iClose;
		size_t iTagEnd;

		while ( (iOpen < pTemplate->SourceSize) &&
			 !__xrtTemplateMarkerAt(pTemplate, iOpen, pConfig->Open) ) {
			iOpen++;
		}
		if ( iOpen == pTemplate->SourceSize ) {
			break;
		}
		if ( __xrtTemplateMarkerAt(
			pTemplate,
			iOpen + pConfig->Open.Size,
			pConfig->Open
		) ) {
			if ( !__xrtTemplatePushText(
				&Parser,
				iTextStart,
				iOpen - iTextStart
			) || !__xrtTemplatePushText(
				&Parser,
				iOpen,
				pConfig->Open.Size
			) ) {
				return false;
			}
			Parser.Position = iOpen + (pConfig->Open.Size * 2u);
			iTextStart = Parser.Position;
			continue;
		}
		if ( !__xrtTemplatePushText(
			&Parser,
			iTextStart,
			iOpen - iTextStart
		) ) {
			return false;
		}
		iBodyStart = iOpen + pConfig->Open.Size;
		if ( !__xrtTemplateFindClose(&Parser, iBodyStart, &iClose) ) {
			__xrtTemplateError(
				XERR_VALUE,
				XTEMPLATE_ERROR_SYNTAX,
				"compile",
				"template tag has no closing marker",
				pTemplate,
				iOpen,
				pTemplate->SourceSize - iOpen
			);
			return false;
		}
		iTagEnd = iClose + pConfig->Close.Size;
		if ( !__xrtTemplateCompileOutput(
			&Parser,
			iOpen,
			iBodyStart,
			iClose,
			iTagEnd
		) ) {
			return false;
		}
		Parser.Position = iTagEnd;
		iTextStart = Parser.Position;
	}
	return __xrtTemplatePushText(
		&Parser,
		iTextStart,
		pTemplate->SourceSize - iTextStart
	);
	#endif
}

#endif
