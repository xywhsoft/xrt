


#define XTE_PRIVATE_INVALID_INDEX		0xFFFFFFFFu
#define XTE_PRIVATE_FILE_VERSION		2u

#define XTE_ERROR_OK					0
#define XTE_ERROR_MALLOC				1
#define XTE_ERROR_PARSE					2
#define XTE_ERROR_UNSUPPORTED			3
#define XTE_ERROR_UNKNOWN_STATEMENT		4
#define XTE_ERROR_RENDER				5
#define XTE_ERROR_FILE					6

#define XTE_PRIVATE_ARG_HAS_EXPR		0x0001u
#define XTE_PRIVATE_ARG_NAMED			0x0002u
#define XTE_PRIVATE_LOOP_MAX_ITERATIONS	100000u


typedef struct
{
	const XTE_StatementDef* pDef;
	ptr pOwned;
} XTE_PrivateStatementReg;

typedef struct
{
	const XTE_FunctionDef* pDef;
	ptr pOwned;
} XTE_PrivateFunctionReg;

typedef struct
{
	XTE_StatementDef Def;
	char* sName;
	void (*FreeUserData)(ptr pUserData);
} XTE_PrivateOwnedStatement;

typedef struct
{
	XTE_FunctionDef Def;
	char* sName;
	void (*FreeUserData)(ptr pUserData);
} XTE_PrivateOwnedFunction;

typedef struct
{
	uint32 iNameOff;
	uint32 iNameSize;
	XTE_NodeSpan tBody;
} XTE_PrivateSubTemplateItem;

typedef struct
{
	const char* sText;
	uint32 iSize;
} XTE_PrivateView;

typedef struct
{
	const char* sOpen;
	const char* sClose;
	uint32 iOpenSize;
	uint32 iCloseSize;
} XTE_PrivateBracket;

typedef struct
{
	char* sName;
	uint32 iNameSize;
	char* sRaw;
	uint32 iRawSize;
	uint32 iFlags;
} XTE_PrivateAstArg;

typedef struct XTE_PrivateAstNode_Struct XTE_PrivateAstNode;

typedef struct
{
	xarray_struct arrNode;
} XTE_PrivateAstList;

struct XTE_PrivateAstNode_Struct
{
	uint32 iType;
	uint32 iFlags;
	uint32 iPos;
	uint32 iSize;

	union {
		struct {
			char* sText;
			uint32 iTextSize;
		} Text;
		struct {
			uint32 iOutputType;
			char* sExpr;
			uint32 iExprSize;
			char* sFormat;
			uint32 iFormatSize;
			char* sFuncName;
			uint32 iFuncNameSize;
			xarray_struct arrArg;
		} Output;
		struct {
			char* sExpr;
			uint32 iExprSize;
			char* sTrueText;
			uint32 iTrueSize;
			char* sFalseText;
			uint32 iFalseSize;
		} InlineBool;
		struct {
			const XTE_StatementDef* pDef;
			char* sStmtName;
			uint32 iStmtNameSize;
			xarray_struct arrArg;
			XTE_PrivateAstList tBody;
			char* sRawBody;
			uint32 iRawBodySize;
		} Statement;
	} Data;
};

typedef struct
{
	xteengine hEngine;
	const char* sText;
	uint32 iSize;
	uint32 iPos;
	uint32 iBasePos;
	const char* sRootText;
	uint32 iRootSize;
	XTE_PrivateBracket tBracket;
	XTE_Error* pError;
} XTE_PrivateParser;


struct XTE_Engine_Struct
{
	volatile int32 iRefCount;
	xarray_struct arrStatement;
	xarray_struct arrFunction;
};

struct XTE_Template_Struct
{
	xteengine hEngine;
	int bOwnEngine;
	XTE_Error LastError;
	xbuffer_struct tStringPool;
	xarray_struct arrNode;
	xarray_struct arrExpr;
	xarray_struct arrArg;
	xarray_struct arrSubTemplate;
	XTE_NodeSpan tRoot;
};

struct XTE_RenderCtx_Struct
{
	xteengine hEngine;
	xtetemplate hTemplate;
	XTE_Writer* pWriter;
	xdict pIncludeMap;
	xvalue pLocal;
	xvalue pCurrent;
	xvalue pRoot;
	xvalue pGlobal;
	XTE_Error* pError;
	uint32 iFlags;
	uint32 iLoopDepth;
};

static XTE_Node* xte_private_template_get_node(xtetemplate hTemplate, uint32 iIndex);
static XTE_ExprNode* xte_private_template_get_expr(xtetemplate hTemplate, uint32 iIndex);
static XTE_ArgItem* xte_private_template_get_arg(xtetemplate hTemplate, uint32 iIndex);
static XTE_PrivateSubTemplateItem* xte_private_template_get_subtemplate(xtetemplate hTemplate, uint32 iIndex);
static const char* xte_private_pool_ptr(xtetemplate hTemplate, uint32 iOff);
static uint32 xte_private_pool_add_copy(xtetemplate hTemplate, const char* sText, uint32 iSize);
static int xte_private_value_truthy(xvalue pVal);
static char* xte_private_value_to_text(xvalue pVal);

#ifdef XTE_ENABLE_FILE
	typedef struct
	{
		char sMagic[8];
		uint32 iVersion;
		uint32 iFlags;
		uint32 iStringPoolSize;
		uint32 iNodeCount;
		uint32 iExprCount;
		uint32 iArgCount;
		XTE_NodeSpan tRoot;
	} XTE_PrivateFileHeader;
#endif


// xte_private_clear_error 相关处理
static void xte_private_clear_error(XTE_Error* pError)
{
	if ( pError ) {
		memset(pError, 0, sizeof(*pError));
	}
}


// xte_private_copy_error 相关处理
static void xte_private_copy_error(XTE_Error* pDst, const XTE_Error* pSrc)
{
	if ( pDst && pSrc ) {
		*pDst = *pSrc;
	}
}


// xte_private_fill_error_pos 相关处理
// 根据文本偏移位置计算行列号，填充错误信息结构体
static void xte_private_fill_error_pos(const char* sText, uint32 iSize, uint32 iPos, XTE_Error* pError, int iCode, const char* sDesc)
{
	uint32 i = 0;
	uint32 iLine = 1;
	uint32 iColumn = 1;

	if ( pError == NULL ) {
		return;
	}

	// 清空并填充错误码和描述
	memset(pError, 0, sizeof(*pError));
	pError->iCode = iCode;
	pError->sDesc = sDesc;
	pError->iPos = iPos;

	// 逐字符扫描到目标位置，累计行号和列号
	while ( (i < iSize) && (i < iPos) ) {
		if ( sText[i] == '\n' ) {
			iLine++;
			iColumn = 1;
		} else {
			iColumn++;
		}
		i++;
	}

	// 填充行列信息
	pError->iLine = iLine;
	pError->iColumn = iColumn;
	pError->iRefLine = iLine;
	pError->iRefColumn = iColumn;
	pError->iRefPos = iPos;
}


// xte_private_set_parser_error 相关处理
static void xte_private_set_parser_error(XTE_PrivateParser* pParser, uint32 iLocalPos, int iCode, const char* sDesc)
{
	uint32 iAbsPos = pParser->iBasePos + iLocalPos;
	xte_private_fill_error_pos(pParser->sRootText, pParser->iRootSize, iAbsPos, pParser->pError, iCode, sDesc);
}


// xte_private_str_eq 相关处理
static int xte_private_str_eq(const char* sTextA, uint32 iSizeA, const char* sTextB, uint32 iSizeB)
{
	if ( iSizeA != iSizeB ) {
		return 0;
	}
	if ( iSizeA == 0u ) {
		return 1;
	}
	return memcmp(sTextA, sTextB, iSizeA) == 0;
}


// xte_private_trim_view 相关处理
static void xte_private_trim_view(XTE_PrivateView* pView)
{
	while ( pView->iSize && ((pView->sText[0] == ' ') || (pView->sText[0] == '\t') || (pView->sText[0] == '\r') || (pView->sText[0] == '\n')) ) {
		pView->sText++;
		pView->iSize--;
	}

	while ( pView->iSize && ((pView->sText[pView->iSize - 1] == ' ') || (pView->sText[pView->iSize - 1] == '\t') || (pView->sText[pView->iSize - 1] == '\r') || (pView->sText[pView->iSize - 1] == '\n')) ) {
		pView->iSize--;
	}
}


// xte_private_is_ident_start 相关处理
static int xte_private_is_ident_start(char ch)
{
	return ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || (ch == '_');
}


// xte_private_is_ident_char 相关处理
static int xte_private_is_ident_char(char ch)
{
	return xte_private_is_ident_start(ch) || ((ch >= '0') && (ch <= '9'));
}


// xte_private_setup_bracket 相关处理
static int xte_private_setup_bracket(const XTE_ParseOptions* pOptions, XTE_PrivateBracket* pBracket)
{
	const char* sBracket = NULL;
	size_t iLen = 0;

	if ( pBracket == NULL ) {
		return 0;
	}

	sBracket = pOptions ? pOptions->sBracket : NULL;
	if ( (sBracket == NULL) || (sBracket[0] == 0) ) {
		pBracket->sOpen = "{";
		pBracket->sClose = "}";
		pBracket->iOpenSize = 1;
		pBracket->iCloseSize = 1;
		return 1;
	}

	iLen = strlen(sBracket);
	if ( (iLen < 2u) || ((iLen % 2u) != 0u) ) {
		return 0;
	}

	pBracket->sOpen = sBracket;
	pBracket->iOpenSize = (uint32)(iLen / 2u);
	pBracket->sClose = &sBracket[pBracket->iOpenSize];
	pBracket->iCloseSize = pBracket->iOpenSize;
	return 1;
}


// xte_private_init 相关处理
static void xte_private_init(void)
{
}


// xte_private_unit 相关处理
static void xte_private_unit(void)
{
}


// xte_private_match_open 相关处理
static int xte_private_match_open(const XTE_PrivateBracket* pBracket, const char* sText, uint32 iSize, uint32 iPos)
{
	if ( (iPos + pBracket->iOpenSize) > iSize ) {
		return 0;
	}
	return memcmp(&sText[iPos], pBracket->sOpen, pBracket->iOpenSize) == 0;
}


// xte_private_match_close 相关处理
static int xte_private_match_close(const XTE_PrivateBracket* pBracket, const char* sText, uint32 iSize, uint32 iPos)
{
	if ( (iPos + pBracket->iCloseSize) > iSize ) {
		return 0;
	}
	return memcmp(&sText[iPos], pBracket->sClose, pBracket->iCloseSize) == 0;
}


// xte_private_copy_view 相关处理
static char* xte_private_copy_view(const char* sText, uint32 iSize)
{
	return __xrt_str(xrtCopyStr((str)sText, iSize));
}


// xte_private_copy_view_unescaped 相关处理
// 复制文本视图并处理转义字符（如 \n \" 等）
static char* xte_private_copy_view_unescaped(const char* sText, uint32 iSize)
{
	xbuffer_struct tBuf = { 0 };
	uint32 i = 0;
	char chZero = 0;
	char* sRet = NULL;

	xrtBufferInit(&tBuf, 0);

	// 逐字符扫描，遇到反斜杠跳过并取下一个字符作为实际内容
	for ( i = 0; i < iSize; i++ ) {
		char ch = sText[i];

		if ( (ch == '\\') && ((i + 1u) < iSize) ) {
			i++;
			ch = sText[i];
		}

		if ( !xrtBufferAppend(&tBuf, &ch, 1, XBUF_BINARY) ) {
			xrtBufferUnit(&tBuf);
			return NULL;
		}
	}

	// 追加字符串结束符
	if ( !xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		return NULL;
	}

	// 空内容返回 null 常量
	if ( tBuf.Length == 1u ) {
		xrtBufferUnit(&tBuf);
		return __xrt_str(xCore.sNull);
	}

	// 分配内存并复制结果
	sRet = xrtMalloc(tBuf.Length);
	if ( sRet ) {
		memcpy(sRet, tBuf.Buffer, tBuf.Length);
	}

	xrtBufferUnit(&tBuf);
	return sRet;
}


// xte_private_is_integer_view 相关处理
static int xte_private_is_integer_view(const char* sText, uint32 iSize)
{
	uint32 i = 0;

	if ( iSize == 0u ) {
		return 0;
	}
	if ( (sText[0] == '-') || (sText[0] == '+') ) {
		if ( iSize == 1u ) {
			return 0;
		}
		i = 1;
	}
	for ( ; i < iSize; i++ ) {
		if ( (sText[i] < '0') || (sText[i] > '9') ) {
			return 0;
		}
	}
	return 1;
}


// xte_private_is_number_view 相关处理
static int xte_private_is_number_view(const char* sText, uint32 iSize)
{
	uint32 i = 0;
	int bDot = 0;
	int bDigit = 0;

	if ( iSize == 0u ) {
		return 0;
	}
	if ( (sText[0] == '-') || (sText[0] == '+') ) {
		if ( iSize == 1u ) {
			return 0;
		}
		i = 1;
	}

	for ( ; i < iSize; i++ ) {
		if ( (sText[i] >= '0') && (sText[i] <= '9') ) {
			bDigit = 1;
			continue;
		}
		if ( (sText[i] == '.') && !bDot ) {
			bDot = 1;
			continue;
		}
		return 0;
	}

	return bDigit;
}


// xte_private_is_expr_sep_char 相关处理
static int xte_private_is_expr_sep_char(char ch)
{
	return (ch == 0)
		|| (ch == ' ')
		|| (ch == '\t')
		|| (ch == '\r')
		|| (ch == '\n')
		|| (ch == '(')
		|| (ch == ')')
		|| (ch == '!')
		|| (ch == '=')
		|| (ch == '>')
		|| (ch == '<')
		|| (ch == '~')
		|| (ch == '&')
		|| (ch == '|');
}


// xte_private_expr_has_keyword 相关处理
static int xte_private_expr_has_keyword(const char* sText, uint32 iSize, const char* sKeyword)
{
	uint32 i = 0;
	uint32 iKeywordSize = (uint32)strlen(sKeyword);

	if ( iSize < iKeywordSize ) {
		return 0;
	}

	for ( i = 0; i + iKeywordSize <= iSize; i++ ) {
		char chPrev = (i == 0u) ? 0 : sText[i - 1u];
		char chNext = ((i + iKeywordSize) < iSize) ? sText[i + iKeywordSize] : 0;

		if ( !xte_private_is_expr_sep_char(chPrev) ) {
			continue;
		}
		if ( memcmp(&sText[i], sKeyword, iKeywordSize) != 0 ) {
			continue;
		}
		if ( !xte_private_is_expr_sep_char(chNext) ) {
			continue;
		}
		return 1;
	}

	return 0;
}


// xte_private_expr_is_complex 相关处理
// 判断表达式是否为复杂表达式（含运算符或逻辑关键字），用于决定编译时的表达式类型
static int xte_private_expr_is_complex(const char* sText, uint32 iSize)
{
	uint32 i = 0;
	char chQuote = 0;
	int bEscape = 0;

	// 逐字符扫描，跳过引号内的内容
	for ( i = 0; i < iSize; i++ ) {
		char ch = sText[i];

		// 处于引号字符串内部
		if ( chQuote != 0 ) {
			if ( bEscape ) {
				bEscape = 0;
				continue;
			}
			if ( ch == '\\' ) {
				bEscape = 1;
				continue;
			}
			if ( ch == chQuote ) {
				chQuote = 0;
			}
			continue;
		}

		// 遇到引号开始，标记进入字符串
		if ( (ch == '\'') || (ch == '"') ) {
			chQuote = ch;
			continue;
		}

		// 遇到运算符字符则判定为复杂表达式
		if ( (ch == '(') || (ch == ')') || (ch == '!') || (ch == '=') || (ch == '>') || (ch == '<') || (ch == '~') ) {
			return 1;
		}
	}

	// 检查是否包含逻辑关键字 and/or/not
	if ( xte_private_expr_has_keyword(sText, iSize, "and")
		|| xte_private_expr_has_keyword(sText, iSize, "or")
		|| xte_private_expr_has_keyword(sText, iSize, "not") ) {
		return 1;
	}

	return 0;
}

typedef enum
{
	XTE_PRIVATE_CMP_NONE = 0,
	XTE_PRIVATE_CMP_EQ,
	XTE_PRIVATE_CMP_NE,
	XTE_PRIVATE_CMP_APPROX,
	XTE_PRIVATE_CMP_GT,
	XTE_PRIVATE_CMP_LT,
	XTE_PRIVATE_CMP_GTE,
	XTE_PRIVATE_CMP_LTE
} XTE_PrivateCompareOp;

typedef struct
{
	XTE_RenderCtx* pRender;
	const char* sText;
	uint32 iSize;
	uint32 iPos;
	int bError;
} XTE_PrivateExprParser;


// xte_private_expr_skip_space 相关处理
static void xte_private_expr_skip_space(XTE_PrivateExprParser* pParser)
{
	while ( (pParser->iPos < pParser->iSize)
		&& ((pParser->sText[pParser->iPos] == ' ')
			|| (pParser->sText[pParser->iPos] == '\t')
			|| (pParser->sText[pParser->iPos] == '\r')
			|| (pParser->sText[pParser->iPos] == '\n')) ) {
		pParser->iPos++;
	}
}


// xte_private_expr_match_char 相关处理
static int xte_private_expr_match_char(XTE_PrivateExprParser* pParser, char ch)
{
	xte_private_expr_skip_space(pParser);
	if ( (pParser->iPos < pParser->iSize) && (pParser->sText[pParser->iPos] == ch) ) {
		pParser->iPos++;
		return 1;
	}
	return 0;
}


// xte_private_expr_match_keyword 相关处理
static int xte_private_expr_match_keyword(XTE_PrivateExprParser* pParser, const char* sKeyword)
{
	uint32 iKeywordSize = (uint32)strlen(sKeyword);
	char chNext = 0;

	xte_private_expr_skip_space(pParser);
	if ( (pParser->iPos + iKeywordSize) > pParser->iSize ) {
		return 0;
	}
	if ( memcmp(&pParser->sText[pParser->iPos], sKeyword, iKeywordSize) != 0 ) {
		return 0;
	}

	chNext = ((pParser->iPos + iKeywordSize) < pParser->iSize) ? pParser->sText[pParser->iPos + iKeywordSize] : 0;
	if ( !xte_private_is_expr_sep_char(chNext) ) {
		return 0;
	}

	pParser->iPos += iKeywordSize;
	return 1;
}


// xte_private_value_is_numeric 相关处理
static int xte_private_value_is_numeric(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	}

	switch ( pVal->Type ) {
		case XVO_DT_BOOL:
		case XVO_DT_INT:
		case XVO_DT_FLOAT:
		case XVO_DT_TIME:
			return 1;
		case XVO_DT_TEXT:
			return xte_private_is_number_view(__xrt_cstr(xvoGetText(pVal)), xvoGetSize(pVal));
		default:
			return 0;
	}
}


// xte_private_value_to_number 相关处理
static double xte_private_value_to_number(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0.0;
	}

	switch ( pVal->Type ) {
		case XVO_DT_BOOL:
			return xvoGetBool(pVal) ? 1.0 : 0.0;
		case XVO_DT_INT:
			return (double)xvoGetInt(pVal);
		case XVO_DT_FLOAT:
			return xvoGetFloat(pVal);
		case XVO_DT_TIME:
			return (double)xvoGetTime(pVal);
		case XVO_DT_TEXT:
			return xrtStrToNum(xvoGetText(pVal));
		default:
			return 0.0;
	}
}


// xte_private_compare_values 相关处理
// 值比较算法 - 数值型直接转 double 比较，非数值型转字符串比较
static int xte_private_compare_values(xvalue pLeft, xvalue pRight, XTE_PrivateCompareOp iOp)
{
	// 两侧均为数值时，使用浮点数比较
	if ( xte_private_value_is_numeric(pLeft) && xte_private_value_is_numeric(pRight) ) {
		double fLeft = xte_private_value_to_number(pLeft);
		double fRight = xte_private_value_to_number(pRight);
		double fDiff = fLeft - fRight;
		double fAbs = (fDiff < 0.0) ? -fDiff : fDiff;

		switch ( iOp ) {
			case XTE_PRIVATE_CMP_EQ:
				return fLeft == fRight;
			case XTE_PRIVATE_CMP_NE:
				return fLeft != fRight;
			case XTE_PRIVATE_CMP_APPROX:
				return fAbs <= 0.000000001;
			case XTE_PRIVATE_CMP_GT:
				return fLeft > fRight;
			case XTE_PRIVATE_CMP_LT:
				return fLeft < fRight;
			case XTE_PRIVATE_CMP_GTE:
				return fLeft >= fRight;
			case XTE_PRIVATE_CMP_LTE:
				return fLeft <= fRight;
			default:
				return 0;
		}
	}

	// 非数值型转为字符串后使用 strcmp 比较
	{
		char* sLeft = xte_private_value_to_text(pLeft);
		char* sRight = xte_private_value_to_text(pRight);
		int iCmp = strcmp(sLeft ? sLeft : "", sRight ? sRight : "");
		int bRet = 0;

		switch ( iOp ) {
			case XTE_PRIVATE_CMP_EQ:
			case XTE_PRIVATE_CMP_APPROX:
				bRet = (iCmp == 0);
				break;
			case XTE_PRIVATE_CMP_NE:
				bRet = (iCmp != 0);
				break;
			case XTE_PRIVATE_CMP_GT:
				bRet = (iCmp > 0);
				break;
			case XTE_PRIVATE_CMP_LT:
				bRet = (iCmp < 0);
				break;
			case XTE_PRIVATE_CMP_GTE:
				bRet = (iCmp >= 0);
				break;
			case XTE_PRIVATE_CMP_LTE:
				bRet = (iCmp <= 0);
				break;
			default:
				bRet = 0;
				break;
		}

		xrtFree(sLeft);
		xrtFree(sRight);
		return bRet;
	}
}


// xte_private_expr_parse_value 相关处理
// 表达式求值核心 - 解析字面量（字符串、布尔、整数、浮点）和变量路径
static xvalue xte_private_expr_parse_value(XTE_PrivateExprParser* pParser)
{
	uint32 iStart = 0;
	uint32 iEnd = 0;

	xte_private_expr_skip_space(pParser);
	if ( pParser->iPos >= pParser->iSize ) {
		pParser->bError = 1;
		return xvoCreateNull();
	}

	// 处理引号字符串字面量
	if ( (pParser->sText[pParser->iPos] == '\'') || (pParser->sText[pParser->iPos] == '"') ) {
		char chQuote = pParser->sText[pParser->iPos++];
		int bEscape = 0;

		iStart = pParser->iPos;
		while ( pParser->iPos < pParser->iSize ) {
			char ch = pParser->sText[pParser->iPos];

			// 转义状态处理
			if ( bEscape ) {
				bEscape = 0;
				pParser->iPos++;
				continue;
			}
			if ( ch == '\\' ) {
				bEscape = 1;
				pParser->iPos++;
				continue;
			}
			// 遇到闭合引号，提取字符串值
			if ( ch == chQuote ) {
				char* sValue = xte_private_copy_view_unescaped(&pParser->sText[iStart], pParser->iPos - iStart);
				xvalue pValue = NULL;

				pParser->iPos++;
				if ( sValue == NULL ) {
					pParser->bError = 1;
					return xvoCreateNull();
				}
				pValue = xvoCreateText(sValue, (uint32)strlen(sValue), TRUE);
				if ( pValue == NULL ) {
					xrtFree(sValue);
					pParser->bError = 1;
					return xvoCreateNull();
				}
				return pValue;
			}
			pParser->iPos++;
		}

		// 引号未闭合
		pParser->bError = 1;
		return xvoCreateNull();
	}

	// 扫描非引号 token，遇到空白或运算符字符停止
	iStart = pParser->iPos;
	while ( pParser->iPos < pParser->iSize ) {
		char ch = pParser->sText[pParser->iPos];

		if ( (ch == ' ')
			|| (ch == '\t')
			|| (ch == '\r')
			|| (ch == '\n')
			|| (ch == ')')
			|| (ch == '!')
			|| (ch == '=')
			|| (ch == '>')
			|| (ch == '<')
			|| (ch == '~')
			|| (ch == '&')
			|| (ch == '|') ) {
			break;
		}
		pParser->iPos++;
	}
	iEnd = pParser->iPos;

	if ( iEnd <= iStart ) {
		pParser->bError = 1;
		return xvoCreateNull();
	}

	// 依次尝试匹配布尔字面量 true/false/null
	if ( xte_private_str_eq(&pParser->sText[iStart], iEnd - iStart, "true", 4) ) {
		return xvoCreateBool(TRUE);
	}
	if ( xte_private_str_eq(&pParser->sText[iStart], iEnd - iStart, "false", 5) ) {
		return xvoCreateBool(FALSE);
	}
	if ( xte_private_str_eq(&pParser->sText[iStart], iEnd - iStart, "null", 4) ) {
		return xvoCreateNull();
	}
	// 尝试解析为整数
	if ( xte_private_is_integer_view(&pParser->sText[iStart], iEnd - iStart) ) {
		char* sTemp = xte_private_copy_view(&pParser->sText[iStart], iEnd - iStart);
		int64 iVal = 0;

		if ( sTemp == NULL ) {
			pParser->bError = 1;
			return xvoCreateNull();
		}
		iVal = xrtStrToI64(sTemp);
		xrtFree(sTemp);
		return xvoCreateInt(iVal);
	}
	// 尝试解析为浮点数
	if ( xte_private_is_number_view(&pParser->sText[iStart], iEnd - iStart) ) {
		char* sTemp = xte_private_copy_view(&pParser->sText[iStart], iEnd - iStart);
		double fVal = 0.0;

		if ( sTemp == NULL ) {
			pParser->bError = 1;
			return xvoCreateNull();
		}
		fVal = xrtStrToNum(sTemp);
		xrtFree(sTemp);
		return xvoCreateFloat(fVal);
	}

	// 以上都不匹配，按变量路径从作用域中解析
	return xvoCopy(xteResolvePath(
		&pParser->sText[iStart],
		iEnd - iStart,
		pParser->pRender->pCurrent,
		pParser->pRender->pRoot,
		pParser->pRender->pLocal,
		pParser->pRender->pGlobal));
}


// xte_private_expr_parse_compare_op 相关处理
// 比较运算符解析 - 优先匹配双字符运算符(>=, <=, !=, ~=, ==)，再匹配单字符
static int xte_private_expr_parse_compare_op(XTE_PrivateExprParser* pParser, XTE_PrivateCompareOp* pOp)
{
	xte_private_expr_skip_space(pParser);

	// 优先匹配双字符运算符
	if ( (pParser->iPos + 2u) <= pParser->iSize ) {
		if ( memcmp(&pParser->sText[pParser->iPos], ">=", 2) == 0 ) {
			pParser->iPos += 2u;
			*pOp = XTE_PRIVATE_CMP_GTE;
			return 1;
		}
		if ( memcmp(&pParser->sText[pParser->iPos], "<=", 2) == 0 ) {
			pParser->iPos += 2u;
			*pOp = XTE_PRIVATE_CMP_LTE;
			return 1;
		}
		if ( memcmp(&pParser->sText[pParser->iPos], "!=", 2) == 0 ) {
			pParser->iPos += 2u;
			*pOp = XTE_PRIVATE_CMP_NE;
			return 1;
		}
		if ( memcmp(&pParser->sText[pParser->iPos], "~=", 2) == 0 ) {
			pParser->iPos += 2u;
			*pOp = XTE_PRIVATE_CMP_APPROX;
			return 1;
		}
		if ( memcmp(&pParser->sText[pParser->iPos], "==", 2) == 0 ) {
			pParser->iPos += 2u;
			*pOp = XTE_PRIVATE_CMP_EQ;
			return 1;
		}
	}

	if ( pParser->iPos >= pParser->iSize ) {
		return 0;
	}

	// 匹配单字符运算符
	switch ( pParser->sText[pParser->iPos] ) {
		case '=':
			pParser->iPos++;
			*pOp = XTE_PRIVATE_CMP_EQ;
			return 1;
		case '>':
			pParser->iPos++;
			*pOp = XTE_PRIVATE_CMP_GT;
			return 1;
		case '<':
			pParser->iPos++;
			*pOp = XTE_PRIVATE_CMP_LT;
			return 1;
		default:
			return 0;
	}
}

static int xte_private_expr_parse_or(XTE_PrivateExprParser* pParser, int* pOut);


// xte_private_expr_parse_primary_bool 相关处理
// 布尔表达式解析 - 处理括号子表达式、比较运算和真值判定
static int xte_private_expr_parse_primary_bool(XTE_PrivateExprParser* pParser, int* pOut)
{
	xvalue pLeft = NULL;
	xvalue pRight = NULL;
	XTE_PrivateCompareOp iOp = XTE_PRIVATE_CMP_NONE;

	xte_private_expr_skip_space(pParser);

	// 处理括号子表达式，递归解析
	if ( xte_private_expr_match_char(pParser, '(') ) {
		if ( !xte_private_expr_parse_or(pParser, pOut) ) {
			return 0;
		}
		if ( !xte_private_expr_match_char(pParser, ')') ) {
			pParser->bError = 1;
			return 0;
		}
		return 1;
	}

	// 解析左侧值
	pLeft = xte_private_expr_parse_value(pParser);
	if ( pParser->bError ) {
		xvoUnref(pLeft);
		return 0;
	}

	// 尝试解析比较运算符
	if ( xte_private_expr_parse_compare_op(pParser, &iOp) ) {
		// 有比较运算符，解析右侧值并执行比较
		pRight = xte_private_expr_parse_value(pParser);
		if ( pParser->bError ) {
			xvoUnref(pLeft);
			xvoUnref(pRight);
			return 0;
		}

		*pOut = xte_private_compare_values(pLeft, pRight, iOp);
		xvoUnref(pLeft);
		xvoUnref(pRight);
		return 1;
	}

	// 无比较运算符，直接判断左侧值的真假
	*pOut = xte_private_value_truthy(pLeft);
	xvoUnref(pLeft);
	return 1;
}


// xte_private_expr_parse_unary 相关处理
static int xte_private_expr_parse_unary(XTE_PrivateExprParser* pParser, int* pOut)
{
	if ( xte_private_expr_match_keyword(pParser, "not") || xte_private_expr_match_char(pParser, '!') ) {
		if ( !xte_private_expr_parse_unary(pParser, pOut) ) {
			return 0;
		}
		*pOut = !(*pOut);
		return 1;
	}

	return xte_private_expr_parse_primary_bool(pParser, pOut);
}


// xte_private_expr_parse_and 相关处理
static int xte_private_expr_parse_and(XTE_PrivateExprParser* pParser, int* pOut)
{
	int bValue = 0;

	if ( !xte_private_expr_parse_unary(pParser, &bValue) ) {
		return 0;
	}

	while ( 1 ) {
		int bRight = 0;

		if ( !(xte_private_expr_match_keyword(pParser, "and")
			|| ((pParser->iPos + 2u) <= pParser->iSize && memcmp(&pParser->sText[pParser->iPos], "&&", 2) == 0 && (pParser->iPos += 2u, 1))) ) {
			break;
		}

		if ( !xte_private_expr_parse_unary(pParser, &bRight) ) {
			return 0;
		}
		bValue = bValue && bRight;
	}

	*pOut = bValue;
	return 1;
}


// xte_private_expr_parse_or 相关处理
static int xte_private_expr_parse_or(XTE_PrivateExprParser* pParser, int* pOut)
{
	int bValue = 0;

	if ( !xte_private_expr_parse_and(pParser, &bValue) ) {
		return 0;
	}

	while ( 1 ) {
		int bRight = 0;

		if ( !(xte_private_expr_match_keyword(pParser, "or")
			|| ((pParser->iPos + 2u) <= pParser->iSize && memcmp(&pParser->sText[pParser->iPos], "||", 2) == 0 && (pParser->iPos += 2u, 1))) ) {
			break;
		}

		if ( !xte_private_expr_parse_and(pParser, &bRight) ) {
			return 0;
		}
		bValue = bValue || bRight;
	}

	*pOut = bValue;
	return 1;
}


// xte_private_eval_bool_expr 相关处理
static int xte_private_eval_bool_expr(XTE_RenderCtx* pCtx, const char* sText, uint32 iSize, int* pOut)
{
	XTE_PrivateExprParser tParser = { 0 };
	int bValue = 0;

	if ( sText == NULL ) {
		return 0;
	}
	if ( iSize == 0u ) {
		iSize = (uint32)strlen(__xrt_cstr(sText));
	}

	tParser.pRender = pCtx;
	tParser.sText = sText;
	tParser.iSize = iSize;

	if ( !xte_private_expr_parse_or(&tParser, &bValue) ) {
		return 0;
	}

	xte_private_expr_skip_space(&tParser);
	if ( (tParser.iPos != tParser.iSize) || tParser.bError ) {
		return 0;
	}

	if ( pOut ) {
		*pOut = bValue;
	}
	return 1;
}


// xte_private_split_colon 相关处理
// 冒号分隔符切分算法 - 按冒号分隔文本，支持反斜杠转义
static uint32 xte_private_split_colon(const char* sText, uint32 iSize, XTE_PrivateView* arrView, uint32 iMaxCount)
{
	uint32 i = 0;
	uint32 iStart = 0;
	uint32 iCount = 0;
	int bEscape = 0;

	for ( i = 0; i <= iSize; i++ ) {
		int bSplit = 0;

		// 到达末尾或遇到未转义的冒号时触发切分
		if ( i == iSize ) {
			bSplit = 1;
		} else if ( !bEscape && (sText[i] == ':') ) {
			bSplit = 1;
		}

		if ( bSplit ) {
			if ( iCount < iMaxCount ) {
				arrView[iCount].sText = &sText[iStart];
				arrView[iCount].iSize = i - iStart;
			}
			iCount++;
			iStart = i + 1u;
			bEscape = 0;
			continue;
		}

		// 跟踪转义状态
		if ( (!bEscape) && (sText[i] == '\\') ) {
			bEscape = 1;
		} else {
			bEscape = 0;
		}
	}

	return iCount;
}


// xte_private_find_unescaped_eq 相关处理
static int xte_private_find_unescaped_eq(const char* sText, uint32 iSize)
{
	uint32 i = 0;
	int bEscape = 0;

	for ( i = 0; i < iSize; i++ ) {
		if ( !bEscape && (sText[i] == '=') ) {
			return (int)i;
		}
		if ( (!bEscape) && (sText[i] == '\\') ) {
			bEscape = 1;
		} else {
			bEscape = 0;
		}
	}

	return -1;
}


// xte_private_writer_write 相关处理
static int xte_private_writer_write(XTE_Writer* pWriter, const char* sText, size_t iSize)
{
	if ( iSize == 0u ) {
		return 1;
	}
	if ( (pWriter == NULL) || (pWriter->procWrite == NULL) ) {
		return 0;
	}
	if ( pWriter->procWrite(pWriter->pUserData, sText, iSize) == 0 ) {
		return 0;
	}
	pWriter->iWritten += iSize;
	return 1;
}


// xte_private_buffer_writer_proc 相关处理
static int xte_private_buffer_writer_proc(void* pUserData, const char* sText, size_t iSize)
{
	return xrtBufferAppend((xbuffer)pUserData, (ptr)sText, iSize, XBUF_BINARY) ? 1 : 0;
}


// xte_private_find_statement 相关处理
static const XTE_StatementDef* xte_private_find_statement(xteengine hEngine, const char* sName, uint32 iNameSize)
{
	uint32 i = 0;

	if ( hEngine == NULL ) {
		return NULL;
	}

	for ( i = 0; i < hEngine->arrStatement.Count; i++ ) {
		XTE_PrivateStatementReg* pReg = xrtArrayGet_Inline(&hEngine->arrStatement, i + 1u);
		uint32 iLen = (uint32)strlen(pReg->pDef->sName);

		if ( xte_private_str_eq(pReg->pDef->sName, iLen, sName, iNameSize) ) {
			return pReg->pDef;
		}
	}

	return NULL;
}


// xte_private_find_function 相关处理
static const XTE_FunctionDef* xte_private_find_function(xteengine hEngine, const char* sName, uint32 iNameSize)
{
	uint32 i = 0;

	if ( hEngine == NULL ) {
		return NULL;
	}

	for ( i = 0; i < hEngine->arrFunction.Count; i++ ) {
		XTE_PrivateFunctionReg* pReg = xrtArrayGet_Inline(&hEngine->arrFunction, i + 1u);
		uint32 iLen = (uint32)strlen(pReg->pDef->sName);

		if ( xte_private_str_eq(pReg->pDef->sName, iLen, sName, iNameSize) ) {
			return pReg->pDef;
		}
	}

	return NULL;
}


// xte_private_statement_name_eq 相关处理
static int xte_private_statement_name_eq(xtetemplate hTemplate, const XTE_Node* pNode, const char* sName)
{
	if ( (hTemplate == NULL) || (pNode == NULL) || (pNode->iType != XTE_NODE_STATEMENT) || (sName == NULL) ) {
		return 0;
	}

	return xte_private_str_eq(
		xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iStmtNameOff),
		pNode->Data.Statement.iStmtNameSize,
		sName,
		(uint32)strlen(__xrt_cstr(sName))
	);
}


// xte_private_fill_arg_list 相关处理
static void xte_private_fill_arg_list(xtetemplate hTemplate, uint32 iArgStart, uint32 iArgCount, XTE_ArgList* pArgs)
{
	memset(pArgs, 0, sizeof(*pArgs));

	if ( hTemplate == NULL ) {
		return;
	}

	pArgs->hTemplate = hTemplate;
	pArgs->iCount = iArgCount;
	pArgs->pItems = (iArgCount != 0u) ? xte_private_template_get_arg(hTemplate, iArgStart) : NULL;
}


// xte_private_make_arg_list 相关处理
static void xte_private_make_arg_list(xtetemplate hTemplate, const XTE_Node* pNode, XTE_ArgList* pArgs)
{
	if ( (hTemplate == NULL) || (pNode == NULL) || (pNode->iType != XTE_NODE_STATEMENT) ) {
		memset(pArgs, 0, sizeof(*pArgs));
		return;
	}

	xte_private_fill_arg_list(hTemplate, pNode->Data.Statement.iArgStart, pNode->Data.Statement.iArgCount, pArgs);
}


// xte_private_fill_node_error 相关处理
static void xte_private_fill_node_error(const XTE_Node* pNode, XTE_Error* pError, int iCode, const char* sDesc)
{
	if ( pError == NULL ) {
		return;
	}

	memset(pError, 0, sizeof(*pError));
	pError->iCode = iCode;
	pError->sDesc = sDesc;
	pError->iLine = 1u;
	pError->iColumn = 1u;
	pError->iRefLine = 1u;
	pError->iRefColumn = 1u;

	if ( pNode != NULL ) {
		pError->iPos = pNode->iPos;
		pError->iRefPos = pNode->iPos;
	}
}


// xte_private_bind_statement_node 相关处理
// 将语句节点绑定到语句定义，调用语句的解析回调函数
static int xte_private_bind_statement_node(xtetemplate hTemplate, XTE_Node* pNode, const XTE_StatementDef* pDef, int iDefaultCode, const char* sDefaultDesc)
{
	const char* sName = NULL;
	XTE_ArgList tArgs = { 0 };
	XTE_NodeSpan* pBody = NULL;
	const char* sRawBody = NULL;
	XTE_StmtParseCtx tCtx = { 0 };
	void* pData = NULL;

	if ( (hTemplate == NULL) || (pNode == NULL) || (pNode->iType != XTE_NODE_STATEMENT) ) {
		return 1;
	}

	// 查找语句定义
	if ( pDef == NULL ) {
		sName = xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iStmtNameOff);
		pDef = xte_private_find_statement(hTemplate->hEngine, sName, pNode->Data.Statement.iStmtNameSize);
	}
	if ( pDef == NULL ) {
		xte_private_fill_node_error(pNode, &hTemplate->LastError, XTE_ERROR_UNKNOWN_STATEMENT, "template statement is not registered");
		return 0;
	}

	// 释放已有的私有数据
	if ( pNode->Data.Statement.pData != NULL ) {
		if ( pDef->procFreeData != NULL ) {
			pDef->procFreeData(pNode->Data.Statement.pData);
		}
		pNode->Data.Statement.pData = NULL;
	}

	// 没有解析回调则跳过
	if ( pDef->procParse == NULL ) {
		return 1;
	}

	// 构造参数列表和解析上下文
	xte_private_make_arg_list(hTemplate, pNode, &tArgs);
	pBody = (pNode->Data.Statement.tBody.iCount != 0u) ? &pNode->Data.Statement.tBody : NULL;
	sRawBody = (pNode->Data.Statement.iRawBodyOff != XTE_PRIVATE_INVALID_INDEX) ? xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iRawBodyOff) : NULL;

	tCtx.hEngine = hTemplate->hEngine;
	tCtx.hTemplate = hTemplate;
	tCtx.pDef = pDef;
	tCtx.pArgs = &tArgs;
	tCtx.pBody = pBody;
	tCtx.sRawBody = sRawBody;
	tCtx.iRawBodySize = pNode->Data.Statement.iRawBodySize;
	tCtx.pError = &hTemplate->LastError;
	tCtx.pUserData = pDef->pUserData;

	// 调用语句的解析回调
	xte_private_clear_error(&hTemplate->LastError);
	if ( pDef->procParse(&tCtx, &pData) == 0 ) {
		if ( hTemplate->LastError.iCode == 0 ) {
			xte_private_fill_node_error(pNode, &hTemplate->LastError, iDefaultCode, sDefaultDesc);
		}
		return 0;
	}

	// 保存解析结果
	pNode->Data.Statement.pData = pData;
	return 1;
}


// xte_private_rebuild_statement_data 相关处理
static int UNUSED_ATTR xte_private_rebuild_statement_data(xtetemplate hTemplate, int iDefaultCode, const char* sDefaultDesc)
{
	uint32 i = 0;

	if ( hTemplate == NULL ) {
		return 0;
	}

	for ( i = 0; i < hTemplate->arrNode.Count; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(hTemplate, i);

		if ( (pNode == NULL) || (pNode->iType != XTE_NODE_STATEMENT) ) {
			continue;
		}
		if ( !xte_private_bind_statement_node(hTemplate, pNode, NULL, iDefaultCode, sDefaultDesc) ) {
			return 0;
		}
	}

	return 1;
}


// xte_private_find_subtemplate 相关处理
static const XTE_PrivateSubTemplateItem* xte_private_find_subtemplate(xtetemplate hTemplate, const char* sName, uint32 iNameSize)
{
	uint32 i = 0;

	if ( (hTemplate == NULL) || (sName == NULL) ) {
		return NULL;
	}
	if ( iNameSize == 0u ) {
		iNameSize = (uint32)strlen(__xrt_cstr(sName));
	}

	for ( i = 0; i < hTemplate->arrSubTemplate.Count; i++ ) {
		XTE_PrivateSubTemplateItem* pItem = xte_private_template_get_subtemplate(hTemplate, i);
		const char* sItemName = NULL;

		if ( pItem == NULL ) {
			continue;
		}

		sItemName = xte_private_pool_ptr(hTemplate, pItem->iNameOff);
		if ( xte_private_str_eq(sItemName, pItem->iNameSize, sName, iNameSize) ) {
			return pItem;
		}
	}

	return NULL;
}


// xte_private_rebuild_subtemplates 相关处理
// 重建子模板索引表，扫描所有 define 语句并注册到子模板数组
static int xte_private_rebuild_subtemplates(xtetemplate hTemplate, int iErrorCode, const char* sDefaultDesc)
{
	uint32 i = 0;

	if ( hTemplate == NULL ) {
		return 0;
	}

	// 清空并重新初始化子模板数组
	(xrtArrayUnit)(&hTemplate->arrSubTemplate);
	xrtArrayInit(&hTemplate->arrSubTemplate, sizeof(XTE_PrivateSubTemplateItem), XRT_OBJMODE_LOCAL);

	// 遍历所有节点，找出 define 语句
	for ( i = 0; i < hTemplate->arrNode.Count; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(hTemplate, i);
		XTE_PrivateSubTemplateItem tItem = { 0 };
		const char* sStmtName = NULL;
		const char* sDefineName = NULL;
		uint32 iDefineNameSize = 0;
		uint32 iIndex = 0;

		if ( (pNode == NULL) || (pNode->iType != XTE_NODE_STATEMENT) ) {
			continue;
		}

		// 只处理 define 语句
		sStmtName = xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iStmtNameOff);
		if ( !xte_private_str_eq(sStmtName, pNode->Data.Statement.iStmtNameSize, "define", 6) ) {
			continue;
		}

		// 获取 define 名称（存储在 pData 中）
		sDefineName = (const char*)pNode->Data.Statement.pData;
		if ( (sDefineName == NULL) || (sDefineName[0] == 0) ) {
			xte_private_fill_node_error(pNode, &hTemplate->LastError, iErrorCode, sDefaultDesc);
			return 0;
		}

		// 检查名称是否重复
		iDefineNameSize = (uint32)strlen(sDefineName);
		if ( xte_private_find_subtemplate(hTemplate, sDefineName, iDefineNameSize) != NULL ) {
			xte_private_fill_node_error(pNode, &hTemplate->LastError, iErrorCode, "template define name is duplicated");
			return 0;
		}

		// 构建子模板项并加入数组
		tItem.iNameOff = xte_private_pool_add_copy(hTemplate, sDefineName, iDefineNameSize);
		tItem.iNameSize = iDefineNameSize;
		tItem.tBody = pNode->Data.Statement.tBody;

		if ( tItem.iNameOff == XTE_PRIVATE_INVALID_INDEX ) {
			xte_private_fill_node_error(pNode, &hTemplate->LastError, XTE_ERROR_MALLOC, "template sub template alloc failed");
			return 0;
		}

		iIndex = xrtArrayAppend(&hTemplate->arrSubTemplate, 1u);
		if ( iIndex == 0u ) {
			xte_private_fill_node_error(pNode, &hTemplate->LastError, XTE_ERROR_MALLOC, "template sub template alloc failed");
			return 0;
		}

		memcpy(xrtArrayGet_Inline(&hTemplate->arrSubTemplate, iIndex), &tItem, sizeof(tItem));
	}

	return 1;
}


// xte_private_ast_list_init 相关处理
static void xte_private_ast_list_init(XTE_PrivateAstList* pList)
{
	xrtArrayInit(&pList->arrNode, sizeof(XTE_PrivateAstNode), XRT_OBJMODE_LOCAL);
}


// xte_private_ast_node_unit 相关处理
// 递归释放 AST 节点及其所有子资源
static void xte_private_ast_node_unit(XTE_PrivateAstNode* pNode)
{
	uint32 i = 0;

	switch ( pNode->iType ) {
		case XTE_NODE_TEXT:
			xrtFree(pNode->Data.Text.sText);
			break;
		case XTE_NODE_OUTPUT:
			xrtFree(pNode->Data.Output.sExpr);
			xrtFree(pNode->Data.Output.sFormat);
			xrtFree(pNode->Data.Output.sFuncName);
			// 函数调用类型需要释放参数数组
			if ( pNode->Data.Output.iOutputType == XTE_OUTPUT_FUNC ) {
				for ( i = 0; i < pNode->Data.Output.arrArg.Count; i++ ) {
					XTE_PrivateAstArg* pArg = xrtArrayGet_Inline(&pNode->Data.Output.arrArg, i + 1u);
					xrtFree(pArg->sName);
					xrtFree(pArg->sRaw);
				}
				(xrtArrayUnit)(&pNode->Data.Output.arrArg);
			}
			break;
		case XTE_NODE_INLINE_BOOL:
			xrtFree(pNode->Data.InlineBool.sExpr);
			xrtFree(pNode->Data.InlineBool.sTrueText);
			xrtFree(pNode->Data.InlineBool.sFalseText);
			break;
		case XTE_NODE_STATEMENT:
			xrtFree(pNode->Data.Statement.sStmtName);
			xrtFree(pNode->Data.Statement.sRawBody);
			// 释放语句参数
			for ( i = 0; i < pNode->Data.Statement.arrArg.Count; i++ ) {
				XTE_PrivateAstArg* pArg = xrtArrayGet_Inline(&pNode->Data.Statement.arrArg, i + 1u);
				xrtFree(pArg->sName);
				xrtFree(pArg->sRaw);
			}
			(xrtArrayUnit)(&pNode->Data.Statement.arrArg);
			// 递归释放子节点
			for ( i = 0; i < pNode->Data.Statement.tBody.arrNode.Count; i++ ) {
				XTE_PrivateAstNode* pChild = xrtArrayGet_Inline(&pNode->Data.Statement.tBody.arrNode, i + 1u);
				xte_private_ast_node_unit(pChild);
			}
			(xrtArrayUnit)(&pNode->Data.Statement.tBody.arrNode);
			break;
	}
}


// xte_private_ast_list_unit 相关处理
static void xte_private_ast_list_unit(XTE_PrivateAstList* pList)
{
	uint32 i = 0;

	for ( i = 0; i < pList->arrNode.Count; i++ ) {
		XTE_PrivateAstNode* pNode = xrtArrayGet_Inline(&pList->arrNode, i + 1u);
		xte_private_ast_node_unit(pNode);
	}

	(xrtArrayUnit)(&pList->arrNode);
}


// xte_private_ast_add_node 相关处理
static uint32 xte_private_ast_add_node(XTE_PrivateAstList* pList, const XTE_PrivateAstNode* pNode)
{
	uint32 iIndex = xrtArrayAppend(&pList->arrNode, 1);

	if ( iIndex == 0u ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	memcpy(xrtArrayGet_Inline(&pList->arrNode, iIndex), pNode, sizeof(*pNode));
	return iIndex - 1u;
}


// 获取 private 模板节点
static XTE_Node* xte_private_template_get_node(xtetemplate hTemplate, uint32 iIndex)
{
	if ( (hTemplate == NULL) || (iIndex >= hTemplate->arrNode.Count) ) {
		return NULL;
	}
	return xrtArrayGet_Inline(&hTemplate->arrNode, iIndex + 1u);
}


// xte_private_template_get_expr 相关处理
static XTE_ExprNode* xte_private_template_get_expr(xtetemplate hTemplate, uint32 iIndex)
{
	if ( (hTemplate == NULL) || (iIndex >= hTemplate->arrExpr.Count) ) {
		return NULL;
	}
	return xrtArrayGet_Inline(&hTemplate->arrExpr, iIndex + 1u);
}


// xte_private_template_get_arg 相关处理
static XTE_ArgItem* xte_private_template_get_arg(xtetemplate hTemplate, uint32 iIndex)
{
	if ( (hTemplate == NULL) || (iIndex >= hTemplate->arrArg.Count) ) {
		return NULL;
	}
	return xrtArrayGet_Inline(&hTemplate->arrArg, iIndex + 1u);
}


// xte_private_template_get_subtemplate 相关处理
static XTE_PrivateSubTemplateItem* xte_private_template_get_subtemplate(xtetemplate hTemplate, uint32 iIndex)
{
	if ( (hTemplate == NULL) || (iIndex >= hTemplate->arrSubTemplate.Count) ) {
		return NULL;
	}
	return xrtArrayGet_Inline(&hTemplate->arrSubTemplate, iIndex + 1u);
}


// xte_private_pool_add_copy 相关处理
static uint32 xte_private_pool_add_copy(xtetemplate hTemplate, const char* sText, uint32 iSize)
{
	char chZero = 0;
	uint32 iOff = 0;

	if ( hTemplate == NULL ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	iOff = hTemplate->tStringPool.Length;

	if ( iSize && !xrtBufferAppend(&hTemplate->tStringPool, (ptr)sText, iSize, XBUF_BINARY) ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}
	if ( !xrtBufferAppend(&hTemplate->tStringPool, &chZero, 1, XBUF_BINARY) ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	return iOff;
}


// xte_private_pool_add_unescaped 相关处理
static uint32 xte_private_pool_add_unescaped(xtetemplate hTemplate, const char* sText, uint32 iSize)
{
	char* sTemp = xte_private_copy_view_unescaped(sText, iSize);
	uint32 iOff = XTE_PRIVATE_INVALID_INDEX;

	if ( sTemp == NULL ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	iOff = xte_private_pool_add_copy(hTemplate, sTemp, (uint32)strlen(sTemp));
	xrtFree(sTemp);
	return iOff;
}


// xte_private_pool_ptr 相关处理
static const char* xte_private_pool_ptr(xtetemplate hTemplate, uint32 iOff)
{
	if ( (hTemplate == NULL) || (iOff >= hTemplate->tStringPool.Length) ) {
		return NULL;
	}
	return &hTemplate->tStringPool.Buffer[iOff];
}


// xte_private_add_expr 相关处理
static uint32 xte_private_add_expr(xtetemplate hTemplate, const XTE_ExprNode* pExpr)
{
	uint32 iIndex = xrtArrayAppend(&hTemplate->arrExpr, 1);

	if ( iIndex == 0u ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	memcpy(xrtArrayGet_Inline(&hTemplate->arrExpr, iIndex), pExpr, sizeof(*pExpr));
	return iIndex - 1u;
}


// xte_private_add_arg 相关处理
static uint32 xte_private_add_arg(xtetemplate hTemplate, const XTE_ArgItem* pArg)
{
	uint32 iIndex = xrtArrayAppend(&hTemplate->arrArg, 1);

	if ( iIndex == 0u ) {
		return XTE_PRIVATE_INVALID_INDEX;
	}

	memcpy(xrtArrayGet_Inline(&hTemplate->arrArg, iIndex), pArg, sizeof(*pArg));
	return iIndex - 1u;
}


// xte_private_lookup_first_value 相关处理
// 变量查找算法 - 按 local -> current -> root -> global 的优先级在作用域链中查找变量
static xvalue xte_private_lookup_first_value(const char* sName, uint32 iNameSize, xvalue pCurrent, xvalue pRoot, xvalue pLocal, xvalue pGlobal)
{
	xvalue pRet = &XVO_VALUE_NULL;

	// 优先从局部作用域查找
	if ( pLocal && (pLocal->Type == XVO_DT_TABLE) ) {
		pRet = xvoTableGetValue(pLocal, sName, iNameSize);
		if ( pRet->Type != XVO_DT_NULL ) {
			return pRet;
		}
	}
	// 从当前数据上下文查找
	if ( pCurrent && (pCurrent->Type == XVO_DT_TABLE) ) {
		pRet = xvoTableGetValue(pCurrent, sName, iNameSize);
		if ( pRet->Type != XVO_DT_NULL ) {
			return pRet;
		}
	}
	// 从根数据上下文查找
	if ( pRoot && (pRoot->Type == XVO_DT_TABLE) ) {
		pRet = xvoTableGetValue(pRoot, sName, iNameSize);
		if ( pRet->Type != XVO_DT_NULL ) {
			return pRet;
		}
	}
	// 从全局数据上下文查找
	if ( pGlobal && (pGlobal->Type == XVO_DT_TABLE) ) {
		pRet = xvoTableGetValue(pGlobal, sName, iNameSize);
		if ( pRet->Type != XVO_DT_NULL ) {
			return pRet;
		}
	}

	return &XVO_VALUE_NULL;
}


// xte_private_find_tag_end 相关处理
static int xte_private_find_tag_end(XTE_PrivateParser* pParser, uint32 iContentPos, uint32* pClosePos)
{
	uint32 i = iContentPos;

	while ( i < pParser->iSize ) {
		if ( pParser->sText[i] == '\\' ) {
			i += 2u;
			continue;
		}
		if ( xte_private_match_close(&pParser->tBracket, pParser->sText, pParser->iSize, i) ) {
			*pClosePos = i;
			return 1;
		}
		i++;
	}

	return 0;
}


// xte_private_find_next_tag 相关处理
static uint32 xte_private_find_next_tag(XTE_PrivateParser* pParser, uint32 iPos)
{
	while ( iPos < pParser->iSize ) {
		if ( xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, iPos) ) {
			return iPos;
		}
		iPos++;
	}
	return XTE_PRIVATE_INVALID_INDEX;
}


// xte_private_compile_expr 相关处理
// 编译表达式文本为表达式节点，自动推断类型（字符串/布尔/整数/复杂表达式/路径）
static int xte_private_compile_expr(xtetemplate hTemplate, const char* sText, uint32 iSize, uint32* pExprIndex)
{
	XTE_PrivateView tView = { sText, iSize };
	XTE_ExprNode tExpr = { 0 };
	char* sTemp = NULL;

	xte_private_trim_view(&tView);

	// 根据内容推断表达式类型
	if ( (tView.iSize >= 2u) && (((tView.sText[0] == '"') && (tView.sText[tView.iSize - 1u] == '"')) || ((tView.sText[0] == '\'') && (tView.sText[tView.iSize - 1u] == '\''))) ) {
		// 引号字符串字面量
		tExpr.iType = XTE_EXPR_TEXT;
		tExpr.iTextOff = xte_private_pool_add_unescaped(hTemplate, &tView.sText[1], tView.iSize - 2u);
		tExpr.iTextSize = (tExpr.iTextOff == XTE_PRIVATE_INVALID_INDEX) ? 0u : (uint32)strlen(xte_private_pool_ptr(hTemplate, tExpr.iTextOff));
	} else if ( xte_private_str_eq(tView.sText, tView.iSize, "true", 4) ) {
		tExpr.iType = XTE_EXPR_BOOL;
		tExpr.iBoolValue = 1;
	} else if ( xte_private_str_eq(tView.sText, tView.iSize, "false", 5) ) {
		tExpr.iType = XTE_EXPR_BOOL;
		tExpr.iBoolValue = 0;
	} else if ( xte_private_is_integer_view(tView.sText, tView.iSize) ) {
		// 整数字面量
		tExpr.iType = XTE_EXPR_INT;
		sTemp = xte_private_copy_view(tView.sText, tView.iSize);
		if ( sTemp == NULL ) {
			return 0;
		}
		tExpr.iIntValue = xrtStrToI64(sTemp);
		xrtFree(sTemp);
	} else if ( xte_private_expr_is_complex(tView.sText, tView.iSize) ) {
		// 复杂布尔表达式（含运算符）
		tExpr.iType = XTE_EXPR_BOOL_EXPR;
		tExpr.iTextOff = xte_private_pool_add_copy(hTemplate, tView.sText, tView.iSize);
		tExpr.iTextSize = tView.iSize;
	} else {
		// 简单变量路径
		tExpr.iType = XTE_EXPR_PATH;
		tExpr.iTextOff = xte_private_pool_add_copy(hTemplate, tView.sText, tView.iSize);
		tExpr.iTextSize = tView.iSize;
	}

	// 检查字符串池分配是否成功
	if ( ((tExpr.iType == XTE_EXPR_TEXT) || (tExpr.iType == XTE_EXPR_PATH) || (tExpr.iType == XTE_EXPR_BOOL_EXPR)) && (tExpr.iTextOff == XTE_PRIVATE_INVALID_INDEX) ) {
		return 0;
	}

	// 添加到表达式数组
	*pExprIndex = xte_private_add_expr(hTemplate, &tExpr);
	return (*pExprIndex != XTE_PRIVATE_INVALID_INDEX);
}


// xte_private_parse_text 相关处理
// 解析纯文本节点 - 扫描直到遇到模板标签开标记，处理双开标记转义（{{ -> {）
static int xte_private_parse_text(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList)
{
	xbuffer_struct tBuf = { 0 };
	uint32 iStart = pParser->iPos;
	uint32 iChunkPos = pParser->iPos;
	XTE_PrivateAstNode tNode = { 0 };

	xrtBufferInit(&tBuf, 0);

	// 扫描文本直到遇到模板标签开标记
	while ( pParser->iPos < pParser->iSize ) {
		// 不是开标记，继续扫描
		if ( !xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, pParser->iPos) ) {
			pParser->iPos++;
			continue;
		}

		// 双开标记表示转义，输出一个开标记字符
		if ( xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, pParser->iPos + pParser->tBracket.iOpenSize) ) {
			if ( pParser->iPos > iChunkPos ) {
				if ( !xrtBufferAppend(&tBuf, (ptr)&pParser->sText[iChunkPos], pParser->iPos - iChunkPos, XBUF_BINARY) ) {
					xrtBufferUnit(&tBuf);
					xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template text alloc failed");
					return 0;
				}
			}
			if ( !xrtBufferAppend(&tBuf, (ptr)pParser->tBracket.sOpen, pParser->tBracket.iOpenSize, XBUF_BINARY) ) {
				xrtBufferUnit(&tBuf);
				xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template text alloc failed");
				return 0;
			}
			pParser->iPos += pParser->tBracket.iOpenSize * 2u;
			iChunkPos = pParser->iPos;
			continue;
		}

		// 单开标记，停止扫描
		break;
	}

	// 追加最后一段文本
	if ( pParser->iPos > iChunkPos ) {
		if ( !xrtBufferAppend(&tBuf, (ptr)&pParser->sText[iChunkPos], pParser->iPos - iChunkPos, XBUF_BINARY) ) {
			xrtBufferUnit(&tBuf);
			xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template text alloc failed");
			return 0;
		}
	}

	// 构建文本节点并添加到 AST 列表
	tNode.iType = XTE_NODE_TEXT;
	tNode.iPos = pParser->iBasePos + iStart;
	tNode.iSize = pParser->iPos - iStart;
	tNode.Data.Text.sText = xte_private_copy_view(tBuf.Buffer, tBuf.Length);
	tNode.Data.Text.iTextSize = tBuf.Length;
	xrtBufferUnit(&tBuf);

	if ( tNode.Data.Text.sText == NULL ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template text alloc failed");
		return 0;
	}

	if ( xte_private_ast_add_node(pList, &tNode) == XTE_PRIVATE_INVALID_INDEX ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template node alloc failed");
		return 0;
	}

	return 1;
}


// 解析 private 输出节点
// 解析输出标签节点（$/ %/ &/ 类型），提取表达式和可选格式化字符串
static int xte_private_parse_output_node(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, uint32 iClosePos, char chKind)
{
	XTE_PrivateAstNode tNode = { 0 };
	XTE_PrivateView arrView[3] = { 0 };
	XTE_PrivateView tExprView = { 0 };
	XTE_PrivateView tFmtView = { 0 };
	uint32 iStart = pParser->iPos;
	uint32 iTagStart = iStart + pParser->tBracket.iOpenSize + 1u;
	uint32 iPartCount = xte_private_split_colon(&pParser->sText[iTagStart], iClosePos - iTagStart, arrView, 3u);

	if ( iPartCount == 0u ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template output tag is empty");
		return 0;
	}

	// 提取表达式部分
	tExprView = arrView[0];
	xte_private_trim_view(&tExprView);

	if ( tExprView.iSize == 0u ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template output expression is empty");
		return 0;
	}

	// 确定输出类型：$ 文本、% 数字、& 时间
	tNode.iType = XTE_NODE_OUTPUT;
	tNode.iPos = pParser->iBasePos + iStart;
	tNode.iSize = (iClosePos + pParser->tBracket.iCloseSize) - iStart;

	if ( chKind == '$' ) {
		tNode.Data.Output.iOutputType = XTE_OUTPUT_TEXT;
	} else if ( chKind == '%' ) {
		tNode.Data.Output.iOutputType = XTE_OUTPUT_NUM;
	} else {
		tNode.Data.Output.iOutputType = XTE_OUTPUT_TIME;
	}

	// 复制表达式文本
	tNode.Data.Output.sExpr = xte_private_copy_view(tExprView.sText, tExprView.iSize);
	tNode.Data.Output.iExprSize = tExprView.iSize;
	if ( tNode.Data.Output.sExpr == NULL ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template output alloc failed");
		return 0;
	}

	// 解析可选的格式化字符串（第二部分）
	if ( iPartCount >= 2u ) {
		tFmtView = arrView[1];
		xte_private_trim_view(&tFmtView);
		if ( tFmtView.iSize ) {
			tNode.Data.Output.sFormat = xte_private_copy_view_unescaped(tFmtView.sText, tFmtView.iSize);
			tNode.Data.Output.iFormatSize = (tNode.Data.Output.sFormat == NULL) ? 0u : (uint32)strlen(tNode.Data.Output.sFormat);
			if ( tNode.Data.Output.sFormat == NULL ) {
				xte_private_ast_node_unit(&tNode);
				xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template output format alloc failed");
				return 0;
			}
		}
	}

	// 添加到 AST 列表
	if ( xte_private_ast_add_node(pList, &tNode) == XTE_PRIVATE_INVALID_INDEX ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template node alloc failed");
		return 0;
	}

	pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
	return 1;
}


// xte_private_parse_inline_bool_node 相关处理
// 解析内联布尔标签 {?expr:true_text:false_text}，按冒号分为表达式/真值文本/假值文本
static int xte_private_parse_inline_bool_node(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, uint32 iClosePos)
{
	XTE_PrivateAstNode tNode = { 0 };
	XTE_PrivateView arrView[3] = { 0 };
	XTE_PrivateView tExprView = { 0 };
	XTE_PrivateView tTrueView = { 0 };
	XTE_PrivateView tFalseView = { 0 };
	uint32 iStart = pParser->iPos;
	uint32 iTagStart = iStart + pParser->tBracket.iOpenSize + 1u;
	uint32 iPartCount = xte_private_split_colon(&pParser->sText[iTagStart], iClosePos - iTagStart, arrView, 3u);

	// 至少需要表达式和真值文本两部分
	if ( iPartCount < 2u ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template inline bool requires at least 2 parts");
		return 0;
	}

	// 提取各部分
	tExprView = arrView[0];
	tTrueView = arrView[1];
	xte_private_trim_view(&tExprView);

	if ( tExprView.iSize == 0u ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template inline bool expression is empty");
		return 0;
	}

	// 可选的假值文本（第三部分）
	if ( iPartCount >= 3u ) {
		tFalseView = arrView[2];
	}

	// 构建内联布尔节点
	tNode.iType = XTE_NODE_INLINE_BOOL;
	tNode.iPos = pParser->iBasePos + iStart;
	tNode.iSize = (iClosePos + pParser->tBracket.iCloseSize) - iStart;
	tNode.Data.InlineBool.sExpr = xte_private_copy_view(tExprView.sText, tExprView.iSize);
	tNode.Data.InlineBool.iExprSize = tExprView.iSize;
	tNode.Data.InlineBool.sTrueText = xte_private_copy_view_unescaped(tTrueView.sText, tTrueView.iSize);
	tNode.Data.InlineBool.iTrueSize = (tNode.Data.InlineBool.sTrueText == NULL) ? 0u : (uint32)strlen(tNode.Data.InlineBool.sTrueText);
	tNode.Data.InlineBool.sFalseText = xte_private_copy_view_unescaped(tFalseView.sText, tFalseView.iSize);
	tNode.Data.InlineBool.iFalseSize = (tNode.Data.InlineBool.sFalseText == NULL) ? 0u : (uint32)strlen(tNode.Data.InlineBool.sFalseText);

	// 检查内存分配结果
	if ( (tNode.Data.InlineBool.sExpr == NULL) || (tNode.Data.InlineBool.sTrueText == NULL) || ((tFalseView.iSize != 0u) && (tNode.Data.InlineBool.sFalseText == NULL)) ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template inline bool alloc failed");
		return 0;
	}

	// 添加到 AST 列表
	if ( xte_private_ast_add_node(pList, &tNode) == XTE_PRIVATE_INVALID_INDEX ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template node alloc failed");
		return 0;
	}

	pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
	return 1;
}


// xte_private_scan_matching_end 相关处理
// 扫描匹配的 end 标签算法 - 跳过嵌套的块语句，找到对应的闭合标签位置
static int xte_private_scan_matching_end(XTE_PrivateParser* pParser, uint32 iSearchPos, uint32* pBodyEndPos, uint32* pAfterEndPos)
{
	uint32 iPos = iSearchPos;
	uint32 iTagPos = 0;

	while ( 1 ) {
		uint32 iContentStart = 0;
		uint32 iClosePos = 0;
		uint32 iNameStart = 0;
		uint32 iNameEnd = 0;
		XTE_PrivateView tNameView = { 0 };
		const XTE_StatementDef* pDef = NULL;

		// 查找下一个标签位置
		iTagPos = xte_private_find_next_tag(pParser, iPos);
		if ( iTagPos == XTE_PRIVATE_INVALID_INDEX ) {
			return 0;
		}
		// 跳过双开标记转义
		if ( xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, iTagPos + pParser->tBracket.iOpenSize) ) {
			iPos = iTagPos + (pParser->tBracket.iOpenSize * 2u);
			continue;
		}

		// 检查是否为指令标签（以 # 开头）
		iContentStart = iTagPos + pParser->tBracket.iOpenSize;
		if ( (iContentStart >= pParser->iSize) || (pParser->sText[iContentStart] != '#') ) {
			iPos = iContentStart;
			continue;
		}

		// 查找标签闭合位置
		if ( !xte_private_find_tag_end(pParser, iContentStart + 1u, &iClosePos) ) {
			return 0;
		}

		// 提取指令名称
		iNameStart = iContentStart + 1u;
		while ( (iNameStart < iClosePos) && ((pParser->sText[iNameStart] == ' ') || (pParser->sText[iNameStart] == '\t')) ) {
			iNameStart++;
		}
		iNameEnd = iNameStart;
		while ( (iNameEnd < iClosePos) && xte_private_is_ident_char(pParser->sText[iNameEnd]) ) {
			iNameEnd++;
		}

		tNameView.sText = &pParser->sText[iNameStart];
		tNameView.iSize = iNameEnd - iNameStart;

		// 找到 end 标签，返回匹配位置
		if ( xte_private_str_eq(tNameView.sText, tNameView.iSize, "end", 3) ) {
			*pBodyEndPos = iTagPos;
			*pAfterEndPos = iClosePos + pParser->tBracket.iCloseSize;
			return 1;
		}

		// 遇到嵌套的块语句，递归跳过
		pDef = xte_private_find_statement(pParser->hEngine, tNameView.sText, tNameView.iSize);
		if ( pDef && ((pDef->iFlags & XTE_STMT_BLOCK) != 0u) && ((pDef->iFlags & XTE_STMT_INLINE) == 0u) ) {
			uint32 iNestedBodyEnd = 0;
			uint32 iNestedAfterEnd = 0;

			if ( !xte_private_scan_matching_end(pParser, iClosePos + pParser->tBracket.iCloseSize, &iNestedBodyEnd, &iNestedAfterEnd) ) {
				return 0;
			}
			iPos = iNestedAfterEnd;
		} else {
			iPos = iClosePos + pParser->tBracket.iCloseSize;
		}
	}
}


// xte_private_parse_arg_list 相关处理
// 解析标签参数列表 - 按冒号分隔，支持命名参数(name=value)和位置参数
static int xte_private_parse_arg_list(XTE_PrivateParser* pParser, const char* sArgText, uint32 iArgSize, uint32 iMinArgs, uint32 iMaxArgs, int bAllowNamedArgs, xarray pArrArg)
{
	XTE_PrivateView arrView[32] = { 0 };
	uint32 i = 0;
	uint32 iCount = xte_private_split_colon(sArgText, iArgSize, arrView, 32u);

	if ( iCount > 32u ) {
		xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template tag has too many args");
		return 0;
	}

	xrtArrayInit(pArrArg, sizeof(XTE_PrivateAstArg), XRT_OBJMODE_LOCAL);

	// 逐个解析参数
	for ( i = 0; i < iCount; i++ ) {
		XTE_PrivateView tView = arrView[i];
		XTE_PrivateAstArg tArg = { 0 };
		int iEqPos = -1;

		xte_private_trim_view(&tView);
		// 查找未转义的等号，判断是否为命名参数
		iEqPos = bAllowNamedArgs ? xte_private_find_unescaped_eq(tView.sText, tView.iSize) : -1;

		if ( iEqPos >= 0 ) {
			// 命名参数：提取名称和原始值
			XTE_PrivateView tNameView = { tView.sText, (uint32)iEqPos };
			XTE_PrivateView tRawView = { tView.sText + iEqPos + 1u, tView.iSize - (uint32)iEqPos - 1u };

			xte_private_trim_view(&tNameView);
			xte_private_trim_view(&tRawView);
			tArg.sName = xte_private_copy_view(tNameView.sText, tNameView.iSize);
			tArg.iNameSize = tNameView.iSize;
			tArg.sRaw = xte_private_copy_view(tRawView.sText, tRawView.iSize);
			tArg.iRawSize = tRawView.iSize;
			tArg.iFlags = XTE_PRIVATE_ARG_HAS_EXPR | XTE_PRIVATE_ARG_NAMED;
		} else {
			// 位置参数：只有原始值
			tArg.sRaw = xte_private_copy_view(tView.sText, tView.iSize);
			tArg.iRawSize = tView.iSize;
			tArg.iFlags = XTE_PRIVATE_ARG_HAS_EXPR;
		}

		// 检查内存分配
		if ( ((tArg.iNameSize != 0u) && (tArg.sName == NULL)) || (tArg.sRaw == NULL) ) {
			xrtFree(tArg.sName);
			xrtFree(tArg.sRaw);
			(xrtArrayUnit)(pArrArg);
			xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_MALLOC, "template arg alloc failed");
			return 0;
		}

		// 添加到参数数组
		if ( xrtArrayAppend(pArrArg, 1) == 0u ) {
			xrtFree(tArg.sName);
			xrtFree(tArg.sRaw);
			(xrtArrayUnit)(pArrArg);
			xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_MALLOC, "template arg alloc failed");
			return 0;
		}
		memcpy(xrtArrayGet_Inline(pArrArg, pArrArg->Count), &tArg, sizeof(tArg));
	}

	// 校验参数数量范围
	if ( (iCount < iMinArgs) || ((iMaxArgs != 0u) && (iCount > iMaxArgs)) ) {
		(xrtArrayUnit)(pArrArg);
		xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template arg count mismatch");
		return 0;
	}

	return 1;
}


// xte_private_parse_statement_args 相关处理
static int xte_private_parse_statement_args(XTE_PrivateParser* pParser, const XTE_StatementDef* pDef, const char* sArgText, uint32 iArgSize, xarray pArrArg)
{
	if ( pDef == NULL ) {
		xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template statement def is invalid");
		return 0;
	}

	return xte_private_parse_arg_list(
		pParser,
		sArgText,
		iArgSize,
		pDef->iMinArgs,
		pDef->iMaxArgs,
		((pDef->iFlags & XTE_STMT_ALLOW_NAMED_ARGS) != 0u),
		pArrArg);
}


// xte_private_parse_function_output_node 相关处理
// 解析函数调用输出标签 @funcName:arg1:arg2
static int xte_private_parse_function_output_node(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, uint32 iClosePos)
{
	uint32 iStart = pParser->iPos;
	uint32 iContentStart = iStart + pParser->tBracket.iOpenSize + 1u;
	uint32 iNameStart = iContentStart;
	uint32 iNameEnd = iContentStart;
	XTE_PrivateAstNode tNode = { 0 };
	XTE_PrivateView tArgView = { 0 };
	const XTE_FunctionDef* pDef = NULL;

	// 提取函数名称
	while ( (iNameStart < iClosePos) && ((pParser->sText[iNameStart] == ' ') || (pParser->sText[iNameStart] == '\t')) ) {
		iNameStart++;
	}
	while ( (iNameEnd < iClosePos) && xte_private_is_ident_char(pParser->sText[iNameEnd]) ) {
		iNameEnd++;
	}

	if ( iNameEnd == iNameStart ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template function name is empty");
		return 0;
	}

	// 查找已注册的函数定义
	pDef = xte_private_find_function(pParser->hEngine, &pParser->sText[iNameStart], iNameEnd - iNameStart);
	if ( pDef == NULL ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template function is not registered");
		return 0;
	}

	// 构建函数调用输出节点
	tNode.iType = XTE_NODE_OUTPUT;
	tNode.iPos = pParser->iBasePos + iStart;
	tNode.iSize = (iClosePos + pParser->tBracket.iCloseSize) - iStart;
	tNode.Data.Output.iOutputType = XTE_OUTPUT_FUNC;
	tNode.Data.Output.sFuncName = xte_private_copy_view(&pParser->sText[iNameStart], iNameEnd - iNameStart);
	tNode.Data.Output.iFuncNameSize = iNameEnd - iNameStart;
	xrtArrayInit(&tNode.Data.Output.arrArg, sizeof(XTE_PrivateAstArg), XRT_OBJMODE_LOCAL);

	if ( tNode.Data.Output.sFuncName == NULL ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template function alloc failed");
		return 0;
	}

	// 提取参数文本区域
	if ( iNameEnd < iClosePos ) {
		if ( pParser->sText[iNameEnd] == ':' ) {
			tArgView.sText = &pParser->sText[iNameEnd + 1u];
			tArgView.iSize = iClosePos - iNameEnd - 1u;
		} else {
			tArgView.sText = &pParser->sText[iNameEnd];
			tArgView.iSize = iClosePos - iNameEnd;
		}
		xte_private_trim_view(&tArgView);
	}

	// 解析参数列表
	if ( tArgView.iSize ) {
		if ( !xte_private_parse_arg_list(pParser, tArgView.sText, tArgView.iSize, pDef->iMinArgs, pDef->iMaxArgs, 1, &tNode.Data.Output.arrArg) ) {
			xte_private_ast_node_unit(&tNode);
			return 0;
		}
	} else if ( pDef->iMinArgs != 0u ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template arg count mismatch");
		return 0;
	}

	// 添加到 AST 列表
	if ( xte_private_ast_add_node(pList, &tNode) == XTE_PRIVATE_INVALID_INDEX ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template node alloc failed");
		return 0;
	}

	pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
	return 1;
}

static int xte_private_parse_nodes(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, int bAllowEnd, int* pEndedByEnd);


// xte_private_parse_statement 相关处理
// 语句解析核心 - 解析语句名称、参数列表，处理块语句的闭合标签匹配和子节点递归解析
static int xte_private_parse_statement(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, uint32 iClosePos)
{
	uint32 iStart = pParser->iPos;
	uint32 iContentStart = iStart + pParser->tBracket.iOpenSize + 1u;
	uint32 iNameStart = iContentStart;
	uint32 iNameEnd = iContentStart;
	XTE_PrivateAstNode tNode = { 0 };
	XTE_PrivateView tArgView = { 0 };
	const XTE_StatementDef* pDef = NULL;
	int bNeedBlock = 0;
	int bUseBlock = 0;

	// 提取语句名称（跳过空白，读取标识符）
	while ( (iNameStart < iClosePos) && ((pParser->sText[iNameStart] == ' ') || (pParser->sText[iNameStart] == '\t')) ) {
		iNameStart++;
	}
	while ( (iNameEnd < iClosePos) && xte_private_is_ident_char(pParser->sText[iNameEnd]) ) {
		iNameEnd++;
	}

	if ( iNameEnd == iNameStart ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template statement name is empty");
		return 0;
	}

	// 查找语句定义
	pDef = xte_private_find_statement(pParser->hEngine, &pParser->sText[iNameStart], iNameEnd - iNameStart);
	if ( pDef == NULL ) {
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_UNKNOWN_STATEMENT, "template statement is not registered");
		return 0;
	}

	// 初始化语句 AST 节点
	tNode.iType = XTE_NODE_STATEMENT;
	tNode.iPos = pParser->iBasePos + iStart;
	tNode.iSize = (iClosePos + pParser->tBracket.iCloseSize) - iStart;
	tNode.Data.Statement.pDef = pDef;
	tNode.Data.Statement.sStmtName = xte_private_copy_view(&pParser->sText[iNameStart], iNameEnd - iNameStart);
	tNode.Data.Statement.iStmtNameSize = iNameEnd - iNameStart;
	xrtArrayInit(&tNode.Data.Statement.arrArg, sizeof(XTE_PrivateAstArg), XRT_OBJMODE_LOCAL);
	xte_private_ast_list_init(&tNode.Data.Statement.tBody);

	if ( tNode.Data.Statement.sStmtName == NULL ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template statement alloc failed");
		return 0;
	}

	// 提取参数区域（跳过冒号分隔符）
	if ( iNameEnd < iClosePos ) {
		if ( pParser->sText[iNameEnd] == ':' ) {
			tArgView.sText = &pParser->sText[iNameEnd + 1u];
			tArgView.iSize = iClosePos - iNameEnd - 1u;
		} else {
			tArgView.sText = &pParser->sText[iNameEnd];
			tArgView.iSize = iClosePos - iNameEnd;
		}
		xte_private_trim_view(&tArgView);
	}

	// 解析语句参数
	if ( tArgView.iSize ) {
		if ( !xte_private_parse_statement_args(pParser, pDef, tArgView.sText, tArgView.iSize, &tNode.Data.Statement.arrArg) ) {
			xte_private_ast_node_unit(&tNode);
			return 0;
		}
	}

	// 处理块语句（需要匹配 end 标签的语句）
	bNeedBlock = ((pDef->iFlags & XTE_STMT_BLOCK) != 0u) && ((pDef->iFlags & XTE_STMT_INLINE) == 0u);
	if ( (pDef->iFlags & XTE_STMT_BLOCK) != 0u ) {
		uint32 iBodyEndPos = 0;
		uint32 iAfterEndPos = 0;

		// 扫描匹配的 end 标签
		if ( xte_private_scan_matching_end(pParser, iClosePos + pParser->tBracket.iCloseSize, &iBodyEndPos, &iAfterEndPos) ) {
			bUseBlock = 1;
			if ( (pDef->iFlags & XTE_STMT_RAW_BODY) != 0u ) {
				// 原始体模式：直接保存标签间文本
				uint32 iRawStart = iClosePos + pParser->tBracket.iCloseSize;
				uint32 iRawSize = iBodyEndPos - iRawStart;

				tNode.Data.Statement.sRawBody = xte_private_copy_view(&pParser->sText[iRawStart], iRawSize);
				tNode.Data.Statement.iRawBodySize = iRawSize;
				if ( (iRawSize != 0u) && (tNode.Data.Statement.sRawBody == NULL) ) {
					xte_private_ast_node_unit(&tNode);
					xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template raw body alloc failed");
					return 0;
				}
			} else {
				// 标准模式：递归解析子节点
				XTE_PrivateParser tSubParser = *pParser;
				int bEndedByEnd = 0;

				tSubParser.sText = &pParser->sText[iClosePos + pParser->tBracket.iCloseSize];
				tSubParser.iSize = iBodyEndPos - (iClosePos + pParser->tBracket.iCloseSize);
				tSubParser.iPos = 0u;
				tSubParser.iBasePos = pParser->iBasePos + iClosePos + pParser->tBracket.iCloseSize;

				if ( !xte_private_parse_nodes(&tSubParser, &tNode.Data.Statement.tBody, 0, &bEndedByEnd) ) {
					xte_private_ast_node_unit(&tNode);
					return 0;
				}
			}

			// 更新节点范围和解析位置
			tNode.iSize = iAfterEndPos - iStart;
			pParser->iPos = iAfterEndPos;
		} else if ( bNeedBlock ) {
			// 必须有块体的语句未找到 end 标签
			xte_private_ast_node_unit(&tNode);
			xte_private_set_parser_error(pParser, iStart, XTE_ERROR_PARSE, "template block statement is not closed");
			return 0;
		}
	}

	// 非块语句直接跳过闭合标签
	if ( !bUseBlock ) {
		pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
	}

	// 添加到 AST 列表
	if ( xte_private_ast_add_node(pList, &tNode) == XTE_PRIVATE_INVALID_INDEX ) {
		xte_private_ast_node_unit(&tNode);
		xte_private_set_parser_error(pParser, iStart, XTE_ERROR_MALLOC, "template node alloc failed");
		return 0;
	}

	return 1;
}


// xte_private_parse_nodes 相关处理
// 模板解析主循环 - 逐字符扫描文本，根据标签类型(!, #, @, $, %, &, ?)分派到对应的解析函数
static int xte_private_parse_nodes(XTE_PrivateParser* pParser, XTE_PrivateAstList* pList, int bAllowEnd, int* pEndedByEnd)
{
	if ( pEndedByEnd ) {
		*pEndedByEnd = 0;
	}

	while ( pParser->iPos < pParser->iSize ) {
		uint32 iClosePos = 0;
		uint32 iContentPos = 0;
		char chKind = 0;

		// 未匹配到开标记，按纯文本解析
		if ( !xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, pParser->iPos) ) {
			if ( !xte_private_parse_text(pParser, pList) ) {
				return 0;
			}
			continue;
		}

		// 双开标记为转义，按文本处理
		if ( xte_private_match_open(&pParser->tBracket, pParser->sText, pParser->iSize, pParser->iPos + pParser->tBracket.iOpenSize) ) {
			if ( !xte_private_parse_text(pParser, pList) ) {
				return 0;
			}
			continue;
		}

		// 查找标签闭合位置
		iContentPos = pParser->iPos + pParser->tBracket.iOpenSize;
		if ( !xte_private_find_tag_end(pParser, iContentPos, &iClosePos) ) {
			xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template tag is not closed");
			return 0;
		}

		// 标签内容不能为空
		if ( iContentPos >= iClosePos ) {
			xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template tag is empty");
			return 0;
		}

		// 根据标签类型字符分派解析
		chKind = pParser->sText[iContentPos];

		// 注释标签 {!...}，跳过
		if ( chKind == '!' ) {
			pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
			continue;
		}

		// 指令标签 {#...}，可能是 end 或语句
		if ( chKind == '#' ) {
			uint32 iNameStart = iContentPos + 1u;
			uint32 iNameEnd = iNameStart;

			while ( (iNameStart < iClosePos) && ((pParser->sText[iNameStart] == ' ') || (pParser->sText[iNameStart] == '\t')) ) {
				iNameStart++;
			}
			while ( (iNameEnd < iClosePos) && xte_private_is_ident_char(pParser->sText[iNameEnd]) ) {
				iNameEnd++;
			}

			// 遇到 end 标签
			if ( xte_private_str_eq(&pParser->sText[iNameStart], iNameEnd - iNameStart, "end", 3) ) {
				if ( bAllowEnd ) {
					pParser->iPos = iClosePos + pParser->tBracket.iCloseSize;
					if ( pEndedByEnd ) {
						*pEndedByEnd = 1;
					}
					return 1;
				}
				xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_PARSE, "template end tag is not allowed here");
				return 0;
			}

			// 其他指令按语句解析
			if ( !xte_private_parse_statement(pParser, pList, iClosePos) ) {
				return 0;
			}
			continue;
		}

		// 函数调用输出标签 {@...}
		if ( chKind == '@' ) {
			if ( !xte_private_parse_function_output_node(pParser, pList, iClosePos) ) {
				return 0;
			}
			continue;
		}

		// 值输出标签 {$/%/&...}
		if ( (chKind == '$') || (chKind == '%') || (chKind == '&') ) {
			if ( !xte_private_parse_output_node(pParser, pList, iClosePos, chKind) ) {
				return 0;
			}
			continue;
		}

		// 内联布尔标签 {?...}
		if ( chKind == '?' ) {
			if ( !xte_private_parse_inline_bool_node(pParser, pList, iClosePos) ) {
				return 0;
			}
			continue;
		}

		xte_private_set_parser_error(pParser, pParser->iPos, XTE_ERROR_UNSUPPORTED, "template tag kind is not supported");
		return 0;
	}

	return 1;
}


// xte_private_template_create 相关处理
static xtetemplate xte_private_template_create(xteengine hEngine, int bOwnEngine)
{
	xtetemplate hTemplate = xrtCalloc(1, sizeof(*hTemplate));

	if ( hTemplate == NULL ) {
		return NULL;
	}

	hTemplate->hEngine = hEngine;
	/* 每个模板持有一份引擎引用，模板和自定义回调的生命周期不依赖调用顺序。 */
	if ( hEngine && !bOwnEngine ) {
		xteEngineAddRef(hEngine);
	}
	hTemplate->bOwnEngine = hEngine != NULL;
	xrtBufferInit(&hTemplate->tStringPool, 0);
	xrtArrayInit(&hTemplate->arrNode, sizeof(XTE_Node), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&hTemplate->arrExpr, sizeof(XTE_ExprNode), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&hTemplate->arrArg, sizeof(XTE_ArgItem), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&hTemplate->arrSubTemplate, sizeof(XTE_PrivateSubTemplateItem), XRT_OBJMODE_LOCAL);
	return hTemplate;
}

static int xte_private_compile_ast_list(xtetemplate hTemplate, XTE_PrivateAstList* pList, XTE_NodeSpan* pSpan);


// xte_private_compile_ast_args 相关处理
// 编译 AST 参数数组，将参数名称和原始值写入字符串池，编译参数表达式
static int xte_private_compile_ast_args(xtetemplate hTemplate, xarray pArrArg, uint32* pArgStart, uint32* pArgCount)
{
	uint32 i = 0;

	*pArgStart = hTemplate->arrArg.Count;
	*pArgCount = pArrArg->Count;

	for ( i = 0; i < pArrArg->Count; i++ ) {
		XTE_PrivateAstArg* pAstArg = xrtArrayGet_Inline(pArrArg, i + 1u);
		XTE_ArgItem tArg = { 0 };

		// 编译命名参数的名称
		if ( pAstArg->iFlags & XTE_PRIVATE_ARG_NAMED ) {
			tArg.iNameOff = xte_private_pool_add_copy(hTemplate, pAstArg->sName, pAstArg->iNameSize);
			tArg.iNameSize = pAstArg->iNameSize;
			if ( tArg.iNameOff == XTE_PRIVATE_INVALID_INDEX ) {
				return 0;
			}
		}

		// 将原始值文本写入字符串池
		tArg.iRawOff = xte_private_pool_add_copy(hTemplate, pAstArg->sRaw, pAstArg->iRawSize);
		tArg.iRawSize = pAstArg->iRawSize;
		if ( tArg.iRawOff == XTE_PRIVATE_INVALID_INDEX ) {
			return 0;
		}

		// 编译参数表达式
		tArg.iFlags = 0u;
		if ( pAstArg->iFlags & XTE_PRIVATE_ARG_NAMED ) {
			tArg.iFlags |= XTE_PRIVATE_ARG_NAMED;
		}
		if ( !xte_private_compile_expr(hTemplate, pAstArg->sRaw, pAstArg->iRawSize, &tArg.iExprIndex) ) {
			return 0;
		}
		tArg.iFlags |= XTE_PRIVATE_ARG_HAS_EXPR;

		if ( xte_private_add_arg(hTemplate, &tArg) == XTE_PRIVATE_INVALID_INDEX ) {
			return 0;
		}
	}

	return 1;
}


// xte_private_compile_ast_list 相关处理
// AST 编译核心 - 将 AST 节点列表转换为紧凑的运行时节点数组，两遍处理：先编译节点数据，再递归编译子节点和触发回调
static int xte_private_compile_ast_list(xtetemplate hTemplate, XTE_PrivateAstList* pList, XTE_NodeSpan* pSpan)
{
	uint32 i = 0;
	uint32 iStart = hTemplate->arrNode.Count;

	pSpan->iStart = iStart;
	pSpan->iCount = pList->arrNode.Count;

	if ( pList->arrNode.Count == 0u ) {
		return 1;
	}

	// 预分配节点数组空间
	if ( xrtArrayAppend(&hTemplate->arrNode, pList->arrNode.Count) == 0u ) {
		return 0;
	}

	// 第一遍：编译所有节点的基本数据和类型特定内容
	for ( i = 0; i < pList->arrNode.Count; i++ ) {
		XTE_PrivateAstNode* pAstNode = xrtArrayGet_Inline(&pList->arrNode, i + 1u);
		XTE_Node* pNode = xte_private_template_get_node(hTemplate, iStart + i);

		memset(pNode, 0, sizeof(*pNode));
		pNode->iType = pAstNode->iType;
		pNode->iFlags = pAstNode->iFlags;
		pNode->iPos = pAstNode->iPos;
		pNode->iSize = pAstNode->iSize;

		switch ( pAstNode->iType ) {
			case XTE_NODE_TEXT:
				// 文本节点：写入字符串池
				pNode->Data.Text.iTextOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.Text.sText, pAstNode->Data.Text.iTextSize);
				pNode->Data.Text.iTextSize = pAstNode->Data.Text.iTextSize;
				if ( pNode->Data.Text.iTextOff == XTE_PRIVATE_INVALID_INDEX ) {
					return 0;
				}
				break;

			case XTE_NODE_OUTPUT:
				pNode->Data.Output.iOutputType = pAstNode->Data.Output.iOutputType;
				pNode->Data.Output.iExprIndex = XTE_PRIVATE_INVALID_INDEX;
				pNode->Data.Output.iFormatOff = XTE_PRIVATE_INVALID_INDEX;
				pNode->Data.Output.iNameOff = XTE_PRIVATE_INVALID_INDEX;
				if ( pAstNode->Data.Output.iOutputType == XTE_OUTPUT_FUNC ) {
					// 函数调用：编译函数名和参数
					pNode->Data.Output.iNameOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.Output.sFuncName, pAstNode->Data.Output.iFuncNameSize);
					pNode->Data.Output.iNameSize = pAstNode->Data.Output.iFuncNameSize;
					if ( pNode->Data.Output.iNameOff == XTE_PRIVATE_INVALID_INDEX ) {
						return 0;
					}
					if ( !xte_private_compile_ast_args(hTemplate, &pAstNode->Data.Output.arrArg, &pNode->Data.Output.iArgStart, &pNode->Data.Output.iArgCount) ) {
						return 0;
					}
				} else {
					// 普通输出：编译表达式和格式化字符串
					if ( !xte_private_compile_expr(hTemplate, pAstNode->Data.Output.sExpr, pAstNode->Data.Output.iExprSize, &pNode->Data.Output.iExprIndex) ) {
						return 0;
					}
					if ( pAstNode->Data.Output.iFormatSize ) {
						pNode->Data.Output.iFormatOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.Output.sFormat, pAstNode->Data.Output.iFormatSize);
						pNode->Data.Output.iFormatSize = pAstNode->Data.Output.iFormatSize;
						if ( pNode->Data.Output.iFormatOff == XTE_PRIVATE_INVALID_INDEX ) {
							return 0;
						}
					}
				}
				break;

			case XTE_NODE_INLINE_BOOL:
				// 内联布尔：编译表达式和真假文本
				if ( !xte_private_compile_expr(hTemplate, pAstNode->Data.InlineBool.sExpr, pAstNode->Data.InlineBool.iExprSize, &pNode->Data.InlineBool.iExprIndex) ) {
					return 0;
				}
				pNode->Data.InlineBool.iTrueOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.InlineBool.sTrueText, pAstNode->Data.InlineBool.iTrueSize);
				pNode->Data.InlineBool.iTrueSize = pAstNode->Data.InlineBool.iTrueSize;
				if ( pNode->Data.InlineBool.iTrueOff == XTE_PRIVATE_INVALID_INDEX ) {
					return 0;
				}
				pNode->Data.InlineBool.iFalseOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.InlineBool.sFalseText ? pAstNode->Data.InlineBool.sFalseText : "", pAstNode->Data.InlineBool.iFalseSize);
				pNode->Data.InlineBool.iFalseSize = pAstNode->Data.InlineBool.iFalseSize;
				if ( pNode->Data.InlineBool.iFalseOff == XTE_PRIVATE_INVALID_INDEX ) {
					return 0;
				}
				break;

			case XTE_NODE_STATEMENT:
				// 语句：编译名称、参数和原始体
				pNode->Data.Statement.iStmtNameOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.Statement.sStmtName, pAstNode->Data.Statement.iStmtNameSize);
				pNode->Data.Statement.iStmtNameSize = pAstNode->Data.Statement.iStmtNameSize;
				if ( pNode->Data.Statement.iStmtNameOff == XTE_PRIVATE_INVALID_INDEX ) {
					return 0;
				}
				if ( !xte_private_compile_ast_args(hTemplate, &pAstNode->Data.Statement.arrArg, &pNode->Data.Statement.iArgStart, &pNode->Data.Statement.iArgCount) ) {
					return 0;
				}
				if ( pAstNode->Data.Statement.iRawBodySize ) {
					pNode->Data.Statement.iRawBodyOff = xte_private_pool_add_copy(hTemplate, pAstNode->Data.Statement.sRawBody, pAstNode->Data.Statement.iRawBodySize);
					pNode->Data.Statement.iRawBodySize = pAstNode->Data.Statement.iRawBodySize;
					if ( pNode->Data.Statement.iRawBodyOff == XTE_PRIVATE_INVALID_INDEX ) {
						return 0;
					}
				} else {
					pNode->Data.Statement.iRawBodyOff = XTE_PRIVATE_INVALID_INDEX;
				}
				break;
		}
	}

	// 第二遍：递归编译语句子节点，并触发语句解析回调
	for ( i = 0; i < pList->arrNode.Count; i++ ) {
		XTE_PrivateAstNode* pAstNode = xrtArrayGet_Inline(&pList->arrNode, i + 1u);

		if ( pAstNode->iType == XTE_NODE_STATEMENT ) {
			XTE_Node* pNode = xte_private_template_get_node(hTemplate, iStart + i);

			// 递归编译子节点列表
			if ( pAstNode->Data.Statement.tBody.arrNode.Count ) {
				if ( !xte_private_compile_ast_list(hTemplate, &pAstNode->Data.Statement.tBody, &pNode->Data.Statement.tBody) ) {
					return 0;
				}
			}

			// 触发语句的解析回调
			if ( pAstNode->Data.Statement.pDef && pAstNode->Data.Statement.pDef->procParse ) {
				if ( !xte_private_bind_statement_node(hTemplate, pNode, pAstNode->Data.Statement.pDef, XTE_ERROR_PARSE, "template statement parse callback failed") ) {
					return 0;
				}
			}
		}
	}

	return 1;
}


// xte_private_value_truthy 相关处理
static int xte_private_value_truthy(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return 0;
	}

	switch ( pVal->Type ) {
		case XVO_DT_BOOL:
			return xvoGetBool(pVal) ? 1 : 0;
		case XVO_DT_INT:
			return xvoGetInt(pVal) != 0;
		case XVO_DT_FLOAT:
			return xvoGetFloat(pVal) != 0.0;
		case XVO_DT_TEXT:
			return (xvoGetText(pVal) != NULL) && (xvoGetText(pVal)[0] != 0);
		case XVO_DT_ARRAY:
		case XVO_DT_LIST:
		case XVO_DT_TABLE:
			return xvoGetSize(pVal) != 0;
		default:
			return 1;
	}
}


// xte_private_value_to_text 相关处理
static char* xte_private_value_to_text(xvalue pVal)
{
	char sBuf[128] = { 0 };
	int iLen = 0;

	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return __xrt_str(xrtCopyStr((str)"", 0));
	}

	switch ( pVal->Type ) {
		case XVO_DT_TEXT:
			return __xrt_str(xrtCopyStr((str)xvoGetText(pVal), xvoGetSize(pVal)));
		case XVO_DT_BOOL:
			return __xrt_str(xrtCopyStr((str)(xvoGetBool(pVal) ? "true" : "false"), 0));
		case XVO_DT_INT:
			iLen = xrtI64ToStr(xvoGetInt(pVal), sBuf);
			return __xrt_str(xrtCopyStr((str)sBuf, iLen));
		case XVO_DT_FLOAT:
			iLen = xrtNumToStr(xvoGetFloat(pVal), sBuf);
			return __xrt_str(xrtCopyStr((str)sBuf, iLen));
		case XVO_DT_TIME:
			return (char*)xrtTimeToStr(xvoGetTime(pVal), 0);
		default:
			return (char*)xrtCopyStr((str)"", 0);
	}
}


// xte_private_eval_expr_value 相关处理
static xvalue xte_private_eval_expr_value(XTE_RenderCtx* pCtx, uint32 iExprIndex)
{
	XTE_ExprNode* pExpr = xte_private_template_get_expr(pCtx->hTemplate, iExprIndex);

	if ( pExpr == NULL ) {
		return xvoCreateNull();
	}

	switch ( pExpr->iType ) {
		case XTE_EXPR_PATH:
			return xvoCopy(xteResolvePath(xte_private_pool_ptr(pCtx->hTemplate, pExpr->iTextOff), pExpr->iTextSize, pCtx->pCurrent, pCtx->pRoot, pCtx->pLocal, pCtx->pGlobal));
		case XTE_EXPR_TEXT:
			return xvoCreateText((ptr)xte_private_pool_ptr(pCtx->hTemplate, pExpr->iTextOff), pExpr->iTextSize, FALSE);
		case XTE_EXPR_INT:
			return xvoCreateInt(pExpr->iIntValue);
		case XTE_EXPR_BOOL:
			return xvoCreateBool(pExpr->iBoolValue ? TRUE : FALSE);
		case XTE_EXPR_BOOL_EXPR: {
			int bValue = 0;

			if ( !xte_private_eval_bool_expr(pCtx, xte_private_pool_ptr(pCtx->hTemplate, pExpr->iTextOff), pExpr->iTextSize, &bValue) ) {
				return xvoCreateNull();
			}
			return xvoCreateBool(bValue ? TRUE : FALSE);
		}
		default:
			return xvoCreateNull();
	}
}


// xte_private_call_output_function 相关处理
// 调用已注册的输出函数，构造函数上下文并执行回调
static xvalue xte_private_call_output_function(XTE_RenderCtx* pCtx, XTE_Node* pNode)
{
	const char* sName = NULL;
	const XTE_FunctionDef* pDef = NULL;
	XTE_ArgList tArgs = { 0 };
	XTE_FuncCtx tFuncCtx = { 0 };
	xvalue pRet = NULL;

	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->Data.Output.iOutputType != XTE_OUTPUT_FUNC) ) {
		return xvoCreateNull();
	}

	// 查找函数定义
	sName = xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Output.iNameOff);
	pDef = xte_private_find_function(pCtx->hEngine, sName, pNode->Data.Output.iNameSize);
	if ( (pDef == NULL) || (pDef->procCall == NULL) ) {
		if ( (pCtx->pError != NULL) && (pCtx->pError->iCode == 0) ) {
			pCtx->pError->iCode = XTE_ERROR_RENDER;
			pCtx->pError->sDesc = "template function is not registered";
		}
		return xvoCreateNull();
	}

	// 构造函数调用上下文
	xte_private_fill_arg_list(pCtx->hTemplate, pNode->Data.Output.iArgStart, pNode->Data.Output.iArgCount, &tArgs);
	tFuncCtx.pRender = pCtx;
	tFuncCtx.pDef = pDef;
	tFuncCtx.pArgs = &tArgs;
	tFuncCtx.pUserData = pDef->pUserData;

	// 执行函数回调
	if ( pDef->procCall(&tFuncCtx, &pRet) == 0 ) {
		if ( (pCtx->pError != NULL) && (pCtx->pError->iCode == 0) ) {
			pCtx->pError->iCode = XTE_ERROR_RENDER;
			pCtx->pError->sDesc = "template function call failed";
		}
		return xvoCreateNull();
	}

	return pRet ? pRet : xvoCreateNull();
}


// xte_private_render_output_node 相关处理
// 渲染输出节点 - 根据输出类型（函数/文本/数字/时间）求值并格式化为文本输出
static int xte_private_render_output_node(XTE_RenderCtx* pCtx, XTE_Node* pNode)
{
	xvalue pVal = NULL;
	char* sOut = NULL;
	int bOk = 1;

	// 函数调用类型
	if ( pNode->Data.Output.iOutputType == XTE_OUTPUT_FUNC ) {
		pVal = xte_private_call_output_function(pCtx, pNode);
		if ( (pCtx->pError != NULL) && (pCtx->pError->iCode != 0) ) {
			xvoUnref(pVal);
			return 0;
		}
		sOut = xte_private_value_to_text(pVal);
	} else {
		// 求值表达式
		pVal = xte_private_eval_expr_value(pCtx, pNode->Data.Output.iExprIndex);
		if ( pNode->Data.Output.iOutputType == XTE_OUTPUT_TEXT ) {
			// 文本输出：直接转字符串
			sOut = xte_private_value_to_text(pVal);
		} else if ( pNode->Data.Output.iOutputType == XTE_OUTPUT_NUM ) {
			// 数字输出：应用格式化
			const char* sFormat = (pNode->Data.Output.iFormatSize != 0u) ? xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Output.iFormatOff) : NULL;

			if ( sFormat && (pVal->Type == XVO_DT_INT) ) {
				sOut = (char*)xrtIntFormat(xvoGetInt(pVal), (str)sFormat);
			} else if ( sFormat && ((pVal->Type == XVO_DT_FLOAT) || (pVal->Type == XVO_DT_INT) || (pVal->Type == XVO_DT_TEXT)) ) {
				double fValue = (pVal->Type == XVO_DT_TEXT) ? xrtStrToNum(xvoGetText(pVal)) : xvoGetFloat(pVal);
				if ( pVal->Type == XVO_DT_INT ) {
					fValue = (double)xvoGetInt(pVal);
				}
				sOut = (char*)xrtNumFormat(fValue, (str)sFormat);
			} else {
				sOut = xte_private_value_to_text(pVal);
			}
		} else {
			// 时间输出：应用时间格式化
			const char* sFormat = (pNode->Data.Output.iFormatSize != 0u) ? xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Output.iFormatOff) : "yyyy-mm-dd hh:nn:ss";
			xtime tValue = 0;

			if ( pVal->Type == XVO_DT_TIME ) {
				tValue = xvoGetTime(pVal);
			} else if ( pVal->Type == XVO_DT_INT ) {
				tValue = (xtime)xvoGetInt(pVal);
			}
			sOut = (char*)xrtTimeFormat(tValue, sFormat);
		}
	}

	// 写入输出
	if ( sOut ) {
		bOk = xte_private_writer_write(pCtx->pWriter, sOut, strlen(sOut));
		xrtFree(sOut);
	} else {
		bOk = 0;
	}

	xvoUnref(pVal);
	return bOk;
}

static XTE_Flow xte_private_render_span(XTE_RenderCtx* pCtx, XTE_NodeSpan tSpan);


// xte_private_render_template 相关处理
static XTE_Flow xte_private_render_template(XTE_RenderCtx* pCtx, xtetemplate hTemplate, xvalue pCurrent)
{
	XTE_RenderCtx tChildCtx = { 0 };

	if ( (pCtx == NULL) || (hTemplate == NULL) ) {
		return XTE_FLOW_ERROR;
	}

	tChildCtx = *pCtx;
	tChildCtx.hEngine = hTemplate->hEngine;
	tChildCtx.hTemplate = hTemplate;
	tChildCtx.pCurrent = (pCurrent != NULL) ? pCurrent : pCtx->pCurrent;
	if ( tChildCtx.pRoot == NULL ) {
		tChildCtx.pRoot = tChildCtx.pCurrent;
	}

	return xte_private_render_span(&tChildCtx, hTemplate->tRoot);
}


// xte_private_render_body_with_scope_ex 相关处理
static XTE_Flow xte_private_render_body_with_scope_ex(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent)
{
	xvalue pOldLocal = NULL;
	xvalue pOldCurrent = NULL;
	XTE_Flow iFlow = XTE_FLOW_OK;

	if ( (pCtx == NULL) || (pCtx->pRender == NULL) || (pCtx->pBody == NULL) ) {
		return XTE_FLOW_OK;
	}

	pOldLocal = pCtx->pRender->pLocal;
	pOldCurrent = pCtx->pRender->pCurrent;
	pCtx->pRender->pLocal = pLocal;
	pCtx->pRender->pCurrent = (pCurrent != NULL) ? pCurrent : pOldCurrent;
	iFlow = xte_private_render_span(pCtx->pRender, *pCtx->pBody);
	pCtx->pRender->pLocal = pOldLocal;
	pCtx->pRender->pCurrent = pOldCurrent;
	return iFlow;
}

static XTE_Flow xte_private_stmt_render_error(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc);


// xte_private_stmt_parse_define 相关处理
// define 语句解析回调 - 提取子模板名称并存储到语句私有数据
static int xte_private_stmt_parse_define(XTE_StmtParseCtx* pCtx, void** ppData)
{
	const XTE_ArgItem* pArg = NULL;
	XTE_ExprNode* pExpr = NULL;
	const char* sText = NULL;
	char* sName = NULL;

	if ( ppData != NULL ) {
		ppData[0] = NULL;
	}
	if ( (pCtx == NULL) || (pCtx->hTemplate == NULL) || (ppData == NULL) ) {
		return 0;
	}

	// 查找 name 命名参数或第一个位置参数
	pArg = xteFindNamedArg(pCtx->pArgs, "name", 4u);
	if ( pArg == NULL ) {
		pArg = xteArgAt(pCtx->pArgs, 0u);
	}
	if ( pArg == NULL ) {
		return xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, "template define name is required");
	}
	// 参数类型必须是文本或路径
	if ( (xteArgExprType(pCtx->pArgs, pArg) != XTE_EXPR_TEXT) && (xteArgExprType(pCtx->pArgs, pArg) != XTE_EXPR_PATH) ) {
		return xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, "template define name must be text or path");
	}

	// 从表达式节点获取名称文本
	pExpr = xte_private_template_get_expr(pCtx->hTemplate, pArg->iExprIndex);
	if ( (pExpr == NULL) || (pExpr->iTextOff == XTE_PRIVATE_INVALID_INDEX) ) {
		return xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, "template define expr is invalid");
	}

	sText = xte_private_pool_ptr(pCtx->hTemplate, pExpr->iTextOff);
	sName = xte_private_copy_view(sText, pExpr->iTextSize);
	if ( (pExpr->iTextSize != 0u) && (sName == NULL) ) {
		return xteStmtParseSetError(pCtx, XTE_ERROR_MALLOC, "template define name alloc failed");
	}
	// 名称不能为空
	if ( (sName == NULL) || (sName[0] == 0) ) {
		xrtFree(sName);
		return xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, "template define name is empty");
	}

	ppData[0] = sName;
	return 1;
}


// xte_private_stmt_render_define 相关处理
static XTE_Flow xte_private_stmt_render_define(XTE_StmtRenderCtx* pCtx)
{
	(void)pCtx;
	return XTE_FLOW_OK;
}


// xte_private_stmt_render_script 相关处理
static XTE_Flow xte_private_stmt_render_script(XTE_StmtRenderCtx* pCtx)
{
	if ( pCtx == NULL ) {
		return XTE_FLOW_ERROR;
	}
	if ( (pCtx->sRawBody == NULL) || (pCtx->iRawBodySize == 0u) ) {
		return XTE_FLOW_OK;
	}
	if ( xteStmtWrite(pCtx, pCtx->sRawBody, pCtx->iRawBodySize) ) {
		return XTE_FLOW_OK;
	}
	return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template script write failed");
}


// xte_private_stmt_render_if 相关处理
// if/elseif/else 条件渲染 - 按分支顺序求值条件，仅渲染第一个为真的分支
static XTE_Flow xte_private_stmt_render_if(XTE_StmtRenderCtx* pCtx)
{
	int bActive = 0;
	int bSelected = 0;
	uint32 i = 0;

	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}
	// 求值 if 条件
	if ( !xteEvalArgBool(pCtx->pRender, xteArgAt(pCtx->pArgs, 0), &bActive) ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template if condition eval failed");
	}
	bSelected = bActive ? 1 : 0;

	if ( pCtx->pBody == NULL ) {
		return XTE_FLOW_OK;
	}

	// 遍历体节点，处理 elseif/else 分支
	for ( i = 0; i < pCtx->pBody->iCount; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(pCtx->pRender->hTemplate, pCtx->pBody->iStart + i);

		if ( pNode == NULL ) {
			return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template if branch node is invalid");
		}

		// elseif 分支：已选中的分支跳过，未选中时求值条件
		if ( xte_private_statement_name_eq(pCtx->pRender->hTemplate, pNode, "elseif") ) {
			XTE_ArgList tArgs = { 0 };

			if ( bSelected ) {
				bActive = 0;
				continue;
			}

			xte_private_make_arg_list(pCtx->pRender->hTemplate, pNode, &tArgs);
			if ( !xteEvalArgBool(pCtx->pRender, xteArgAt(&tArgs, 0), &bActive) ) {
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template elseif condition eval failed");
			}
			if ( bActive ) {
				bSelected = 1;
			}
			continue;
		}

		// else 分支：所有之前的分支都未选中时激活
		if ( xte_private_statement_name_eq(pCtx->pRender->hTemplate, pNode, "else") ) {
			bActive = bSelected ? 0 : 1;
			if ( bActive ) {
				bSelected = 1;
			}
			continue;
		}

		// 普通节点：当前分支激活时渲染
		if ( bActive ) {
			XTE_NodeSpan tNodeSpan = { pCtx->pBody->iStart + i, 1u };
			XTE_Flow iFlow = xte_private_render_span(pCtx->pRender, tNodeSpan);

			if ( iFlow != XTE_FLOW_OK ) {
				return iFlow;
			}
		}
	}

	return XTE_FLOW_OK;
}


// xte_private_stmt_render_else 相关处理
static XTE_Flow xte_private_stmt_render_else(XTE_StmtRenderCtx* pCtx)
{
	return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template else must be inside if");
}


// xte_private_stmt_render_elseif 相关处理
static XTE_Flow xte_private_stmt_render_elseif(XTE_StmtRenderCtx* pCtx)
{
	return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template elseif must be inside if");
}


// xte_private_stmt_render_break 相关处理
static XTE_Flow xte_private_stmt_render_break(XTE_StmtRenderCtx* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}
	if ( pCtx->pRender->iLoopDepth == 0u ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template break must be inside loop");
	}
	return XTE_FLOW_BREAK;
}


// xte_private_stmt_render_continue 相关处理
static XTE_Flow xte_private_stmt_render_continue(XTE_StmtRenderCtx* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}
	if ( pCtx->pRender->iLoopDepth == 0u ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template continue must be inside loop");
	}
	return XTE_FLOW_CONTINUE;
}


// xte_private_stmt_render_for 相关处理
// for 循环渲染 - 从起始值到结束值按步进迭代，支持 break/continue 控制
static XTE_Flow xte_private_stmt_render_for(XTE_StmtRenderCtx* pCtx)
{
	int64 iStart = 0;
	int64 iEnd = 0;
	int64 iStep = 0;
	int64 iIndex = 0;
	uint32 iLoopCount = 0;
	uint32 iPrevLoopDepth = 0;
	xvalue pLocal = NULL;

	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}
	// 解析起始和结束参数
	if ( xteArgCount(pCtx->pArgs) < 2u ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for requires at least 2 args");
	}
	if ( !xteEvalArgInt(pCtx->pRender, xteArgAt(pCtx->pArgs, 0), &iStart) ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for start eval failed");
	}
	if ( !xteEvalArgInt(pCtx->pRender, xteArgAt(pCtx->pArgs, 1), &iEnd) ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for end eval failed");
	}

	// 解析可选步进参数，默认方向由起止值决定
	if ( xteArgCount(pCtx->pArgs) >= 3u ) {
		if ( !xteEvalArgInt(pCtx->pRender, xteArgAt(pCtx->pArgs, 2), &iStep) ) {
			return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for step eval failed");
		}
	} else {
		iStep = (iStart <= iEnd) ? 1 : -1;
	}

	if ( iStep == 0 ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for step cannot be zero");
	}

	// 创建局部作用域
	pLocal = xvoCreateTable();
	if ( pLocal == NULL ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_MALLOC, "template for local alloc failed");
	}

	iPrevLoopDepth = pCtx->pRender->iLoopDepth;
	pCtx->pRender->iLoopDepth = iPrevLoopDepth + 1u;

	// 迭代循环
	for ( iIndex = iStart;
		((iStep > 0) && (iIndex <= iEnd)) || ((iStep < 0) && (iIndex >= iEnd));
		iIndex += iStep ) {
		XTE_Flow iFlow = XTE_FLOW_OK;

		// 防止无限循环
		iLoopCount++;
		if ( iLoopCount > XTE_PRIVATE_LOOP_MAX_ITERATIONS ) {
			pCtx->pRender->iLoopDepth = iPrevLoopDepth;
			xvoUnref(pLocal);
			return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for exceeded max iterations");
		}
		// 设置循环变量 __index__
		if ( !xvoTableSetInt(pLocal, "__index__", 0, iIndex) ) {
			pCtx->pRender->iLoopDepth = iPrevLoopDepth;
			xvoUnref(pLocal);
			return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template for local set failed");
		}

		// 在局部作用域中渲染循环体
		iFlow = xte_private_render_body_with_scope_ex(pCtx, pLocal, pCtx->pRender->pCurrent);
		if ( iFlow == XTE_FLOW_BREAK ) {
			break;
		}
		if ( iFlow == XTE_FLOW_CONTINUE ) {
			continue;
		}
		if ( iFlow == XTE_FLOW_ERROR ) {
			pCtx->pRender->iLoopDepth = iPrevLoopDepth;
			xvoUnref(pLocal);
			return iFlow;
		}
	}

	// 恢复循环深度
	pCtx->pRender->iLoopDepth = iPrevLoopDepth;
	xvoUnref(pLocal);
	return XTE_FLOW_OK;
}


// xte_private_stmt_render_foreach 相关处理
// foreach 遍历渲染 - 支持数组、列表、字典三种数据类型的迭代，设置 __index__/__value__/__key__ 循环变量
static XTE_Flow xte_private_stmt_render_foreach(XTE_StmtRenderCtx* pCtx)
{
	xvalue pIter = NULL;
	xvalue pLocal = NULL;
	uint32 iLoopCount = 0;
	uint32 iPrevLoopDepth = 0;

	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}

	// 求值迭代对象
	pIter = xteEvalArgValue(pCtx->pRender, xteArgAt(pCtx->pArgs, 0));
	if ( pIter == NULL ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach eval failed");
	}

	// 创建局部作用域
	pLocal = xvoCreateTable();
	if ( pLocal == NULL ) {
		xvoUnref(pIter);
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_MALLOC, "template foreach local alloc failed");
	}

	iPrevLoopDepth = pCtx->pRender->iLoopDepth;
	pCtx->pRender->iLoopDepth = iPrevLoopDepth + 1u;

	// 按数据类型分派迭代逻辑
	if ( pIter->Type == XVO_DT_ARRAY ) {
		uint32 i = 0;
		uint32 iCount = xvoArrayItemCount(pIter);

		for ( i = 0; i < iCount; i++ ) {
			XTE_Flow iFlow = XTE_FLOW_OK;
			xvalue pItem = xvoArrayGetValue(pIter, i);

			// 最大迭代次数检查
			iLoopCount++;
			if ( iLoopCount > XTE_PRIVATE_LOOP_MAX_ITERATIONS ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach exceeded max iterations");
			}

			// 设置循环变量 __index__ 和 __value__
			xvoTableClear(pLocal);
			if ( !xvoTableSetInt(pLocal, "__index__", 0, (int64)i) || !xvoTableSetValue(pLocal, "__value__", 0, pItem, FALSE) ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach local set failed");
			}

			// 在局部作用域中渲染循环体
			iFlow = xte_private_render_body_with_scope_ex(pCtx, pLocal, pItem);
			if ( iFlow == XTE_FLOW_BREAK ) {
				break;
			}
			if ( iFlow == XTE_FLOW_CONTINUE ) {
				continue;
			}
			if ( iFlow == XTE_FLOW_ERROR ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return iFlow;
			}
		}
	} else if ( pIter->Type == XVO_DT_LIST ) {
		uint32 i = 0;
		uint32 iCount = xvoListItemCount(pIter);

		for ( i = 0; i < iCount; i++ ) {
			XTE_Flow iFlow = XTE_FLOW_OK;
			xvalue pItem = xvoListGetValue(pIter, i);

			iLoopCount++;
			if ( iLoopCount > XTE_PRIVATE_LOOP_MAX_ITERATIONS ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach exceeded max iterations");
			}

			xvoTableClear(pLocal);
			if ( !xvoTableSetInt(pLocal, "__index__", 0, (int64)i) || !xvoTableSetValue(pLocal, "__value__", 0, pItem, FALSE) ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach local set failed");
			}

			iFlow = xte_private_render_body_with_scope_ex(pCtx, pLocal, pItem);
			if ( iFlow == XTE_FLOW_BREAK ) {
				break;
			}
			if ( iFlow == XTE_FLOW_CONTINUE ) {
				continue;
			}
			if ( iFlow == XTE_FLOW_ERROR ) {
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return iFlow;
			}
		}
	} else if ( pIter->Type == XVO_DT_TABLE ) {
		// 字典类型迭代：使用字典迭代器遍历键值对
		uint32 iIndex = 0;
		xdict pDict = xvoGetTable(pIter);

		xrtDictIterBegin(pDict);
		while ( 1 ) {
			Dict_Key* pKey = xrtDictIterNext(pDict);
			XTE_Flow iFlow = XTE_FLOW_OK;
			xvalue* ppItem = NULL;

			if ( pKey == NULL ) {
				break;
			}

			iLoopCount++;
			if ( iLoopCount > XTE_PRIVATE_LOOP_MAX_ITERATIONS ) {
				xrtDictIterEnd(pDict);
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach exceeded max iterations");
			}

			// 设置 __index__、__key__ 和 __value__
			ppItem = (xvalue*)(&pKey[1]);
			xvoTableClear(pLocal);
			if ( !xvoTableSetInt(pLocal, "__index__", 0, (int64)iIndex)
				|| !xvoTableSetText(pLocal, "__key__", 0, pKey->Key, pKey->KeyLen, FALSE)
				|| !xvoTableSetValue(pLocal, "__value__", 0, ppItem[0], FALSE) ) {
				xrtDictIterEnd(pDict);
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template foreach local set failed");
			}

			iFlow = xte_private_render_body_with_scope_ex(pCtx, pLocal, ppItem[0]);
			if ( iFlow == XTE_FLOW_BREAK ) {
				break;
			}
			if ( iFlow == XTE_FLOW_CONTINUE ) {
				iIndex++;
				continue;
			}
			if ( iFlow == XTE_FLOW_ERROR ) {
				xrtDictIterEnd(pDict);
				pCtx->pRender->iLoopDepth = iPrevLoopDepth;
				xvoUnref(pLocal);
				xvoUnref(pIter);
				return iFlow;
			}

			iIndex++;
		}
		xrtDictIterEnd(pDict);
	}

	// 恢复循环深度并释放资源
	pCtx->pRender->iLoopDepth = iPrevLoopDepth;
	xvoUnref(pLocal);
	xvoUnref(pIter);
	return XTE_FLOW_OK;
}


// xte_private_stmt_render_include 相关处理
// include 渲染 - 优先查找子模板(define)，再从外部模板映射表中查找
static XTE_Flow xte_private_stmt_render_include(XTE_StmtRenderCtx* pCtx)
{
	char* sName = NULL;
	xtetemplate hTemplate = NULL;
	const XTE_PrivateSubTemplateItem* pSubTemplate = NULL;
	XTE_Flow iFlow = XTE_FLOW_OK;

	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return XTE_FLOW_ERROR;
	}

	// 求值模板名称
	sName = xteEvalArgText(pCtx->pRender, xteArgAt(pCtx->pArgs, 0));
	if ( sName == NULL ) {
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_MALLOC, "template include name alloc failed");
	}

	// 优先在子模板中查找
	pSubTemplate = xte_private_find_subtemplate(pCtx->pRender->hTemplate, sName, 0u);
	if ( pSubTemplate != NULL ) {
		iFlow = xte_private_render_span(pCtx->pRender, pSubTemplate->tBody);
		xrtFree(sName);
		return iFlow;
	}

	// 在外部模板映射表中查找
	if ( pCtx->pRender->pIncludeMap == NULL ) {
		xrtFree(sName);
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template include target not found");
	}

	hTemplate = (xtetemplate)xrtDictGetPtr(pCtx->pRender->pIncludeMap, sName, 0);
	if ( hTemplate == NULL ) {
		xrtFree(sName);
		return xte_private_stmt_render_error(pCtx, XTE_ERROR_RENDER, "template include target not found");
	}

	// 渲染外部模板
	iFlow = xte_private_render_template(pCtx->pRender, hTemplate, pCtx->pRender->pCurrent);
	xrtFree(sName);
	return iFlow;
}


// xte_private_render_node 相关处理
// 节点渲染分发 - 根据节点类型（文本/输出/内联布尔/语句）调用对应的渲染逻辑
static XTE_Flow xte_private_render_node(XTE_RenderCtx* pCtx, XTE_Node* pNode)
{
	if ( pNode == NULL ) {
		xte_private_fill_error_pos("", 0, 0, pCtx->pError, XTE_ERROR_RENDER, "template node index is invalid");
		return XTE_FLOW_ERROR;
	}

	switch ( pNode->iType ) {
		// 文本节点：直接输出
		case XTE_NODE_TEXT: {
			const char* sText = xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Text.iTextOff);
			if ( !xte_private_writer_write(pCtx->pWriter, sText, pNode->Data.Text.iTextSize) ) {
				xte_private_fill_error_pos("", 0, 0, pCtx->pError, XTE_ERROR_RENDER, "template writer failed");
				return XTE_FLOW_ERROR;
			}
			return XTE_FLOW_OK;
		}

		// 输出节点：求值表达式并输出
		case XTE_NODE_OUTPUT:
			if ( !xte_private_render_output_node(pCtx, pNode) ) {
				if ( (pCtx->pError != NULL) && (pCtx->pError->iCode == 0) ) {
					xte_private_fill_error_pos("", 0, 0, pCtx->pError, XTE_ERROR_RENDER, "template output render failed");
				}
				return XTE_FLOW_ERROR;
			}
			return XTE_FLOW_OK;

		// 内联布尔节点：条件求值后输出对应文本
		case XTE_NODE_INLINE_BOOL: {
			xvalue pVal = xte_private_eval_expr_value(pCtx, pNode->Data.InlineBool.iExprIndex);
			int bTruth = xte_private_value_truthy(pVal);
			const char* sText = bTruth ? xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.InlineBool.iTrueOff) : xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.InlineBool.iFalseOff);
			uint32 iSize = bTruth ? pNode->Data.InlineBool.iTrueSize : pNode->Data.InlineBool.iFalseSize;

			xvoUnref(pVal);
			if ( iSize && !xte_private_writer_write(pCtx->pWriter, sText, iSize) ) {
				xte_private_fill_error_pos("", 0, 0, pCtx->pError, XTE_ERROR_RENDER, "template writer failed");
				return XTE_FLOW_ERROR;
			}
			return XTE_FLOW_OK;
		}

		// 语句节点：查找语句定义并调用渲染回调
		case XTE_NODE_STATEMENT: {
			const char* sName = xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Statement.iStmtNameOff);
			const XTE_StatementDef* pDef = xte_private_find_statement(pCtx->hEngine, sName, pNode->Data.Statement.iStmtNameSize);
			XTE_ArgList tArgs = { 0 };
			XTE_NodeSpan* pBody = (pNode->Data.Statement.tBody.iCount != 0u) ? &pNode->Data.Statement.tBody : NULL;
			const char* sRawBody = (pNode->Data.Statement.iRawBodyOff != XTE_PRIVATE_INVALID_INDEX) ? xte_private_pool_ptr(pCtx->hTemplate, pNode->Data.Statement.iRawBodyOff) : NULL;
			XTE_StmtRenderCtx tStmtCtx = { 0 };
			XTE_Flow iFlow = XTE_FLOW_OK;

			if ( pDef == NULL ) {
				xte_private_fill_error_pos("", 0, 0, pCtx->pError, XTE_ERROR_UNKNOWN_STATEMENT, "template statement is not registered");
				return XTE_FLOW_ERROR;
			}

			// 构造语句渲染上下文
			xte_private_make_arg_list(pCtx->hTemplate, pNode, &tArgs);

			tStmtCtx.pRender = pCtx;
			tStmtCtx.pDef = pDef;
			tStmtCtx.pArgs = &tArgs;
			tStmtCtx.pBody = pBody;
			tStmtCtx.sRawBody = sRawBody;
			tStmtCtx.iRawBodySize = pNode->Data.Statement.iRawBodySize;
			tStmtCtx.pData = pNode->Data.Statement.pData;
			tStmtCtx.pUserData = pDef->pUserData;

			// 调用语句的渲染回调，或直接渲染体
			if ( pDef->procRender ) {
				iFlow = pDef->procRender(&tStmtCtx);
			} else if ( pBody ) {
				iFlow = xte_private_render_span(pCtx, *pBody);
			}

			if ( iFlow == XTE_FLOW_ERROR ) {
				if ( (pCtx->pError != NULL) && (pCtx->pError->iCode == 0) ) {
					pCtx->pError->iCode = XTE_ERROR_RENDER;
					pCtx->pError->sDesc = "template statement render failed";
				}
			}
			return iFlow;
		}
	}

	return XTE_FLOW_OK;
}


// xte_private_render_span 相关处理
static XTE_Flow xte_private_render_span(XTE_RenderCtx* pCtx, XTE_NodeSpan tSpan)
{
	uint32 i = 0;

	for ( i = 0; i < tSpan.iCount; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(pCtx->hTemplate, tSpan.iStart + i);
		XTE_Flow iFlow = xte_private_render_node(pCtx, pNode);

		if ( iFlow != XTE_FLOW_OK ) {
			return iFlow;
		}
	}

	return XTE_FLOW_OK;
}

#ifdef XTE_DEBUGMODE
// xte_private_console_writer_proc 相关处理
static int xte_private_console_writer_proc(void* pUserData, const char* sText, size_t iSize)
{
	FILE* fp = (FILE*)pUserData;
	return (fwrite(sText, 1, iSize, fp) == iSize) ? 1 : 0;
}


// xte_private_dump_indent 相关处理
static int xte_private_dump_indent(XTE_Writer* pWriter, uint32 iDepth)
{
	uint32 i = 0;
	for ( i = 0; i < iDepth; i++ ) {
		if ( !xte_private_writer_write(pWriter, "\t", 1) ) {
			return 0;
		}
	}
	return 1;
}


// xte_private_expr_type_name 相关处理
static const char* xte_private_expr_type_name(uint32 iType)
{
	switch ( iType ) {
		case XTE_EXPR_PATH:
			return "PATH";
		case XTE_EXPR_TEXT:
			return "TEXT";
		case XTE_EXPR_INT:
			return "INT";
		case XTE_EXPR_BOOL:
			return "BOOL";
		case XTE_EXPR_BOOL_EXPR:
			return "BOOL_EXPR";
		default:
			return "EXPR";
	}
}


// xte_private_expr_type_name_by_index 相关处理
static const char* xte_private_expr_type_name_by_index(xtetemplate hTemplate, uint32 iExprIndex)
{
	XTE_ExprNode* pExpr = xte_private_template_get_expr(hTemplate, iExprIndex);

	return pExpr ? xte_private_expr_type_name(pExpr->iType) : "INVALID";
}


// xte_private_dump_expr_value 相关处理
static int xte_private_dump_expr_value(XTE_Writer* pWriter, xtetemplate hTemplate, uint32 iExprIndex)
{
	XTE_ExprNode* pExpr = xte_private_template_get_expr(hTemplate, iExprIndex);
	char sBuf[128] = { 0 };

	if ( pExpr == NULL ) {
		return xte_private_writer_write(pWriter, "<invalid-expr>", 14);
	}

	switch ( pExpr->iType ) {
		case XTE_EXPR_PATH:
		case XTE_EXPR_TEXT:
		case XTE_EXPR_BOOL_EXPR:
			return xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pExpr->iTextOff), pExpr->iTextSize);
		case XTE_EXPR_INT:
			snprintf(sBuf, sizeof(sBuf), "%lld", (long long)pExpr->iIntValue);
			return xte_private_writer_write(pWriter, sBuf, strlen(sBuf));
		case XTE_EXPR_BOOL:
			return xte_private_writer_write(pWriter, pExpr->iBoolValue ? "true" : "false", pExpr->iBoolValue ? 4 : 5);
		default:
			return xte_private_writer_write(pWriter, "<expr>", 6);
	}
}


// xte_private_dump_expr_typed 相关处理
static int xte_private_dump_expr_typed(XTE_Writer* pWriter, xtetemplate hTemplate, uint32 iExprIndex)
{
	XTE_ExprNode* pExpr = xte_private_template_get_expr(hTemplate, iExprIndex);
	const char* sType = NULL;

	if ( pExpr == NULL ) {
		return xte_private_writer_write(pWriter, "INVALID: <invalid-expr>", 23);
	}

	sType = xte_private_expr_type_name(pExpr->iType);
	if ( !xte_private_writer_write(pWriter, sType, strlen(sType)) ) {
		return 0;
	}
	if ( !xte_private_writer_write(pWriter, ": ", 2) ) {
		return 0;
	}
	return xte_private_dump_expr_value(pWriter, hTemplate, iExprIndex);
}


// xte_private_dump_arg_list 相关处理
// 调试导出参数列表 - 输出每个参数的名称、原始值和表达式类型
static int xte_private_dump_arg_list(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iArgStart, uint32 iArgCount, uint32 iDepth)
{
	uint32 i = 0;

	for ( i = 0; i < iArgCount; i++ ) {
		XTE_ArgItem* pArg = xte_private_template_get_arg(hTemplate, iArgStart + i);

		if ( pArg == NULL ) {
			return 0;
		}
		// 输出缩进和 ARG 标记
		if ( !xte_private_dump_indent(pWriter, iDepth) ) {
			return 0;
		}
		if ( !xte_private_writer_write(pWriter, "ARG", 3) ) {
			return 0;
		}
		// 输出命名参数名称
		if ( pArg->iNameSize != 0u ) {
			if ( !xte_private_writer_write(pWriter, " ", 1) ) {
				return 0;
			}
			if ( !xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pArg->iNameOff), pArg->iNameSize) ) {
				return 0;
			}
		}
		// 输出参数原始值
		if ( !xte_private_writer_write(pWriter, ": ", 2) ) {
			return 0;
		}
		if ( pArg->iRawSize != 0u ) {
			if ( !xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pArg->iRawOff), pArg->iRawSize) ) {
				return 0;
			}
		}
		if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
			return 0;
		}
		// 输出参数表达式信息
		if ( pArg->iFlags & XTE_PRIVATE_ARG_HAS_EXPR ) {
			if ( !xte_private_dump_indent(pWriter, iDepth + 1u) ) {
				return 0;
			}
			if ( !xte_private_writer_write(pWriter, "EXPR ", 5) ) {
				return 0;
			}
			if ( !xte_private_dump_expr_typed(pWriter, hTemplate, pArg->iExprIndex) ) {
				return 0;
			}
			if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
				return 0;
			}
		}
	}

	return 1;
}


// xte_private_dump_span 相关处理
// 调试导出节点跨度 - 递归输出节点树结构，包含 TEXT/OUTPUT/INLINE_BOOL/STATEMENT 各类型信息
static int xte_private_dump_span(xtetemplate hTemplate, XTE_Writer* pWriter, XTE_NodeSpan tSpan, uint32 iDepth)
{
	uint32 i = 0;

	for ( i = 0; i < tSpan.iCount; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(hTemplate, tSpan.iStart + i);
		if ( pNode == NULL ) {
			return 0;
		}

		// 输出缩进
		if ( !xte_private_dump_indent(pWriter, iDepth) ) {
			return 0;
		}

		// 按节点类型输出不同格式的调试信息
		switch ( pNode->iType ) {
			// 文本节点
			case XTE_NODE_TEXT:
				if ( !xte_private_writer_write(pWriter, "TEXT: ", 6) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pNode->Data.Text.iTextOff), pNode->Data.Text.iTextSize) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
					return 0;
				}
				break;

			// 函数调用输出节点
			case XTE_NODE_OUTPUT:
				if ( pNode->Data.Output.iOutputType == XTE_OUTPUT_FUNC ) {
					if ( !xte_private_writer_write(pWriter, "OUTPUT_FUNC: ", 13) ) {
						return 0;
					}
					if ( !xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pNode->Data.Output.iNameOff), pNode->Data.Output.iNameSize) ) {
						return 0;
					}
					if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
						return 0;
					}
					// 导出函数参数
					if ( pNode->Data.Output.iArgCount != 0u ) {
						if ( !xte_private_dump_arg_list(hTemplate, pWriter, pNode->Data.Output.iArgStart, pNode->Data.Output.iArgCount, iDepth + 1u) ) {
							return 0;
						}
					}
					break;
				}

				// 普通输出节点
				if ( !xte_private_writer_write(pWriter, "OUTPUT(", 7) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, xte_private_expr_type_name_by_index(hTemplate, pNode->Data.Output.iExprIndex), strlen(xte_private_expr_type_name_by_index(hTemplate, pNode->Data.Output.iExprIndex))) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "): ", 3) ) {
					return 0;
				}
				if ( !xte_private_dump_expr_value(pWriter, hTemplate, pNode->Data.Output.iExprIndex) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
					return 0;
				}
				break;

			// 内联布尔节点
			case XTE_NODE_INLINE_BOOL:
				if ( !xte_private_writer_write(pWriter, "INLINE_BOOL(", 12) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, xte_private_expr_type_name_by_index(hTemplate, pNode->Data.InlineBool.iExprIndex), strlen(xte_private_expr_type_name_by_index(hTemplate, pNode->Data.InlineBool.iExprIndex))) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "): ", 3) ) {
					return 0;
				}
				if ( !xte_private_dump_expr_value(pWriter, hTemplate, pNode->Data.InlineBool.iExprIndex) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
					return 0;
				}
				break;

			// 语句节点：递归导出参数和子节点
			case XTE_NODE_STATEMENT:
				if ( !xte_private_writer_write(pWriter, "STATEMENT: ", 11) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iStmtNameOff), pNode->Data.Statement.iStmtNameSize) ) {
					return 0;
				}
				if ( !xte_private_writer_write(pWriter, "\n", 1) ) {
					return 0;
				}
				// 导出语句参数
				if ( pNode->Data.Statement.iArgCount != 0u ) {
					if ( !xte_private_dump_arg_list(hTemplate, pWriter, pNode->Data.Statement.iArgStart, pNode->Data.Statement.iArgCount, iDepth + 1u) ) {
						return 0;
					}
				}
				// 递归导出子节点
				if ( pNode->Data.Statement.tBody.iCount ) {
					if ( !xte_private_dump_span(hTemplate, pWriter, pNode->Data.Statement.tBody, iDepth + 1u) ) {
						return 0;
					}
				}
				break;
		}
	}

	return 1;
}
#endif


// 创建引擎
XXAPI xteengine xteCreateEngine(void)
{
	xteengine hEngine = xrtCalloc(1, sizeof(*hEngine));

	if ( hEngine == NULL ) {
		return NULL;
	}

	hEngine->iRefCount = 1;
	xrtArrayInit(&hEngine->arrStatement, sizeof(XTE_PrivateStatementReg), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&hEngine->arrFunction, sizeof(XTE_PrivateFunctionReg), XRT_OBJMODE_LOCAL);
	if ( !xteRegisterBuiltinStatements(hEngine) ) {
		(xrtArrayUnit)(&hEngine->arrStatement);
		(xrtArrayUnit)(&hEngine->arrFunction);
		xrtFree(hEngine);
		return NULL;
	}
	return hEngine;
}


// 保留引擎引用
XXAPI xteengine xteEngineAddRef(xteengine hEngine)
{
	if ( hEngine ) {
		xrtAtomicRefRetain(&hEngine->iRefCount);
	}
	return hEngine;
}


// 释放引擎实际资源
static void xte_private_destroy_engine(xteengine hEngine)
{
	uint32 i;

	if ( hEngine == NULL ) {
		return;
	}

	for ( i = 0; i < hEngine->arrStatement.Count; i++ ) {
		XTE_PrivateStatementReg* pReg = (XTE_PrivateStatementReg*)xrtArrayGet_Inline(&hEngine->arrStatement, i + 1u);
		XTE_PrivateOwnedStatement* pOwned = pReg ? (XTE_PrivateOwnedStatement*)pReg->pOwned : NULL;
		if ( pOwned ) {
			if ( pOwned->FreeUserData && pOwned->Def.pUserData ) {
				pOwned->FreeUserData(pOwned->Def.pUserData);
			}
			xrtFree(pOwned->sName);
			xrtFree(pOwned);
		}
	}
	for ( i = 0; i < hEngine->arrFunction.Count; i++ ) {
		XTE_PrivateFunctionReg* pReg = (XTE_PrivateFunctionReg*)xrtArrayGet_Inline(&hEngine->arrFunction, i + 1u);
		XTE_PrivateOwnedFunction* pOwned = pReg ? (XTE_PrivateOwnedFunction*)pReg->pOwned : NULL;
		if ( pOwned ) {
			if ( pOwned->FreeUserData && pOwned->Def.pUserData ) {
				pOwned->FreeUserData(pOwned->Def.pUserData);
			}
			xrtFree(pOwned->sName);
			xrtFree(pOwned);
		}
	}
	(xrtArrayUnit)(&hEngine->arrStatement);
	(xrtArrayUnit)(&hEngine->arrFunction);
	xrtFree(hEngine);
}


// 释放引擎引用
XXAPI void xteEngineRelease(xteengine hEngine)
{
	if ( hEngine && xrtAtomicRefRelease(&hEngine->iRefCount) == 0 ) {
		xte_private_destroy_engine(hEngine);
	}
}


// 销毁引擎，等价于释放创建者持有的引用
XXAPI void xteDestroyEngine(xteengine hEngine)
{
	xteEngineRelease(hEngine);
}


// xteRegisterBuiltinStatements 相关处理
XXAPI int xteRegisterBuiltinStatements(xteengine hEngine)
{
	static const XTE_StatementDef tIf = {
		.sName = "if",
		.iFlags = XTE_STMT_BLOCK,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = xte_private_stmt_render_if
	};
	static const XTE_StatementDef tElseIf = {
		.sName = "elseif",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = xte_private_stmt_render_elseif
	};
	static const XTE_StatementDef tElse = {
		.sName = "else",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = xte_private_stmt_render_else
	};
	static const XTE_StatementDef tFor = {
		.sName = "for",
		.iFlags = XTE_STMT_BLOCK,
		.iMinArgs = 2,
		.iMaxArgs = 3,
		.procRender = xte_private_stmt_render_for
	};
	static const XTE_StatementDef tForeach = {
		.sName = "foreach",
		.iFlags = XTE_STMT_BLOCK,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = xte_private_stmt_render_foreach
	};
	static const XTE_StatementDef tBreak = {
		.sName = "break",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = xte_private_stmt_render_break
	};
	static const XTE_StatementDef tContinue = {
		.sName = "continue",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = xte_private_stmt_render_continue
	};
	static const XTE_StatementDef tDefine = {
		.sName = "define",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_ALLOW_NAMED_ARGS,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procParse = xte_private_stmt_parse_define,
		.procRender = xte_private_stmt_render_define,
		.procFreeData = xrtFree
	};
	static const XTE_StatementDef tScript = {
		.sName = "script",
		.iFlags = XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		.iMinArgs = 0,
		.iMaxArgs = 0,
		.procRender = xte_private_stmt_render_script
	};
	static const XTE_StatementDef tInclude = {
		.sName = "include",
		.iFlags = XTE_STMT_INLINE,
		.iMinArgs = 1,
		.iMaxArgs = 1,
		.procRender = xte_private_stmt_render_include
	};

	if ( hEngine == NULL ) {
		return 0;
	}

	if ( !xte_private_find_statement(hEngine, "if", 2) && !xteRegisterStatement(hEngine, &tIf) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "elseif", 6) && !xteRegisterStatement(hEngine, &tElseIf) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "else", 4) && !xteRegisterStatement(hEngine, &tElse) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "for", 3) && !xteRegisterStatement(hEngine, &tFor) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "foreach", 7) && !xteRegisterStatement(hEngine, &tForeach) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "break", 5) && !xteRegisterStatement(hEngine, &tBreak) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "continue", 8) && !xteRegisterStatement(hEngine, &tContinue) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "define", 6) && !xteRegisterStatement(hEngine, &tDefine) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "script", 6) && !xteRegisterStatement(hEngine, &tScript) ) {
		return 0;
	}
	if ( !xte_private_find_statement(hEngine, "include", 7) && !xteRegisterStatement(hEngine, &tInclude) ) {
		return 0;
	}

	return 1;
}


// xteRegisterStatement 相关处理
XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef)
{
	XTE_PrivateStatementReg tReg = { 0 };
	uint32 iIndex = 0;

	if ( (hEngine == NULL) || (pDef == NULL) || (pDef->sName == NULL) || (pDef->sName[0] == 0) ) {
		return 0;
	}
	if ( xte_private_find_statement(hEngine, pDef->sName, (uint32)strlen(pDef->sName)) ) {
		return 0;
	}

	tReg.pDef = pDef;
	iIndex = xrtArrayAppend(&hEngine->arrStatement, 1);
	if ( iIndex == 0u ) {
		return 0;
	}
	memcpy(xrtArrayGet_Inline(&hEngine->arrStatement, iIndex), &tReg, sizeof(tReg));
	return 1;
}


// 注册由引擎拥有定义和用户数据的自定义模板语句
XXAPI int xteRegisterStatementEx(
	xteengine hEngine,
	const char* sName,
	uint32 iFlags,
	uint16 iMinArgs,
	uint16 iMaxArgs,
	int (*procParse)(XTE_StmtParseCtx* pCtx, void** ppData),
	XTE_Flow (*procRender)(XTE_StmtRenderCtx* pCtx),
	void (*procFreeData)(void* pData),
	ptr pUserData,
	void (*FreeUserData)(ptr pUserData))
{
	XTE_PrivateOwnedStatement* pOwned;
	XTE_PrivateStatementReg tReg = { 0 };
	uint32 iIndex;

	if ( !hEngine || !sName || !sName[0] || !procRender || iMinArgs > iMaxArgs ||
	     xte_private_find_statement(hEngine, sName, (uint32)strlen(sName)) ) {
		return 0;
	}
	pOwned = (XTE_PrivateOwnedStatement*)xrtCalloc(1, sizeof(*pOwned));
	if ( !pOwned ) {
		return 0;
	}
	pOwned->sName = (char*)xrtCopyStr((str)sName, 0);
	if ( !pOwned->sName ) {
		xrtFree(pOwned);
		return 0;
	}
	pOwned->Def.sName = pOwned->sName;
	pOwned->Def.iFlags = iFlags;
	pOwned->Def.iMinArgs = iMinArgs;
	pOwned->Def.iMaxArgs = iMaxArgs;
	pOwned->Def.pUserData = pUserData;
	pOwned->Def.procParse = procParse;
	pOwned->Def.procRender = procRender;
	pOwned->Def.procFreeData = procFreeData;
	pOwned->FreeUserData = FreeUserData;
	tReg.pDef = &pOwned->Def;
	tReg.pOwned = pOwned;
	iIndex = xrtArrayAppend(&hEngine->arrStatement, 1);
	if ( iIndex == 0u ) {
		xrtFree(pOwned->sName);
		xrtFree(pOwned);
		return 0;
	}
	memcpy(xrtArrayGet_Inline(&hEngine->arrStatement, iIndex), &tReg, sizeof(tReg));
	return 1;
}


// xteRegisterFunction 相关处理
XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef)
{
	XTE_PrivateFunctionReg tReg = { 0 };
	uint32 iIndex = 0;

	if ( (hEngine == NULL) || (pDef == NULL) || (pDef->sName == NULL) || (pDef->sName[0] == 0) || (pDef->procCall == NULL) ) {
		return 0;
	}
	if ( xte_private_find_function(hEngine, pDef->sName, (uint32)strlen(pDef->sName)) ) {
		return 0;
	}

	tReg.pDef = pDef;
	iIndex = xrtArrayAppend(&hEngine->arrFunction, 1);
	if ( iIndex == 0u ) {
		return 0;
	}
	memcpy(xrtArrayGet_Inline(&hEngine->arrFunction, iIndex), &tReg, sizeof(tReg));
	return 1;
}


// 注册由引擎拥有定义和用户数据的自定义模板函数
XXAPI int xteRegisterFunctionEx(
	xteengine hEngine,
	const char* sName,
	uint16 iMinArgs,
	uint16 iMaxArgs,
	int (*procCall)(XTE_FuncCtx* pCtx, xvalue* ppRet),
	ptr pUserData,
	void (*FreeUserData)(ptr pUserData))
{
	XTE_PrivateOwnedFunction* pOwned;
	XTE_PrivateFunctionReg tReg = { 0 };
	uint32 iIndex;

	if ( !hEngine || !sName || !sName[0] || !procCall || iMinArgs > iMaxArgs ||
	     xte_private_find_function(hEngine, sName, (uint32)strlen(sName)) ) {
		return 0;
	}
	pOwned = (XTE_PrivateOwnedFunction*)xrtCalloc(1, sizeof(*pOwned));
	if ( !pOwned ) {
		return 0;
	}
	pOwned->sName = (char*)xrtCopyStr((str)sName, 0);
	if ( !pOwned->sName ) {
		xrtFree(pOwned);
		return 0;
	}
	pOwned->Def.sName = pOwned->sName;
	pOwned->Def.iMinArgs = iMinArgs;
	pOwned->Def.iMaxArgs = iMaxArgs;
	pOwned->Def.pUserData = pUserData;
	pOwned->Def.procCall = procCall;
	pOwned->FreeUserData = FreeUserData;
	tReg.pDef = &pOwned->Def;
	tReg.pOwned = pOwned;
	iIndex = xrtArrayAppend(&hEngine->arrFunction, 1);
	if ( iIndex == 0u ) {
		xrtFree(pOwned->sName);
		xrtFree(pOwned);
		return 0;
	}
	memcpy(xrtArrayGet_Inline(&hEngine->arrFunction, iIndex), &tReg, sizeof(tReg));
	return 1;
}


// 解析扩展
XXAPI xtetemplate xteParseEx(xteengine hEngine, const char* sText, size_t iSize, const XTE_ParseOptions* pOptions, XTE_Error* pError)
{
	int bOwnEngine = 0;
	xtetemplate hTemplate = NULL;
	XTE_PrivateParser tParser = { 0 };
	XTE_PrivateAstList tAstRoot = { 0 };
	int bEndedByEnd = 0;

	xte_private_clear_error(pError);

	if ( sText == NULL ) {
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_PARSE, "template text is null");
		return NULL;
	}

	if ( iSize == 0u ) {
		iSize = strlen(__xrt_cstr(sText));
	}

	if ( hEngine == NULL ) {
		hEngine = xteCreateEngine();
		if ( hEngine == NULL ) {
			xte_private_fill_error_pos(sText, (uint32)iSize, 0, pError, XTE_ERROR_MALLOC, "template engine alloc failed");
			return NULL;
		}
		xteRegisterBuiltinStatements(hEngine);
		bOwnEngine = 1;
	}

	hTemplate = xte_private_template_create(hEngine, bOwnEngine);
	if ( hTemplate == NULL ) {
		if ( bOwnEngine ) {
			xteDestroyEngine(hEngine);
		}
		xte_private_fill_error_pos(sText, (uint32)iSize, 0, pError, XTE_ERROR_MALLOC, "template alloc failed");
		return NULL;
	}

	if ( !xte_private_setup_bracket(pOptions, &tParser.tBracket) ) {
		xteDestroyTemplate(hTemplate);
		xte_private_fill_error_pos(sText, (uint32)iSize, 0, pError, XTE_ERROR_PARSE, "template bracket config is invalid");
		return NULL;
	}

	xte_private_ast_list_init(&tAstRoot);
	tParser.hEngine = hEngine;
	tParser.sText = sText;
	tParser.iSize = (uint32)iSize;
	tParser.iPos = 0u;
	tParser.iBasePos = 0u;
	tParser.sRootText = sText;
	tParser.iRootSize = (uint32)iSize;
	tParser.pError = &hTemplate->LastError;
	xte_private_clear_error(&hTemplate->LastError);

	if ( !xte_private_parse_nodes(&tParser, &tAstRoot, 0, &bEndedByEnd) ) {
		xte_private_copy_error(pError, &hTemplate->LastError);
		xte_private_ast_list_unit(&tAstRoot);
		xteDestroyTemplate(hTemplate);
		return NULL;
	}

	if ( !xte_private_compile_ast_list(hTemplate, &tAstRoot, &hTemplate->tRoot) ) {
		if ( hTemplate->LastError.iCode == 0 ) {
			xte_private_fill_error_pos(sText, (uint32)iSize, 0, &hTemplate->LastError, XTE_ERROR_MALLOC, "template compile failed");
		}
		xte_private_copy_error(pError, &hTemplate->LastError);
		xte_private_ast_list_unit(&tAstRoot);
		xteDestroyTemplate(hTemplate);
		return NULL;
	}
	if ( !xte_private_rebuild_subtemplates(hTemplate, XTE_ERROR_PARSE, "template define compile failed") ) {
		xte_private_copy_error(pError, &hTemplate->LastError);
		xte_private_ast_list_unit(&tAstRoot);
		xteDestroyTemplate(hTemplate);
		return NULL;
	}

	xte_private_ast_list_unit(&tAstRoot);
	xte_private_copy_error(pError, &hTemplate->LastError);
	return hTemplate;
}


// 解析
XXAPI xtetemplate xteParse(const char* sText, size_t iSize, const char* sBracket)
{
	XTE_ParseOptions tOptions = { 0 };

	tOptions.sBracket = sBracket;
	return xteParseEx(NULL, sText, iSize, &tOptions, NULL);
}


// 销毁模板
XXAPI void xteDestroyTemplate(xtetemplate hTemplate)
{
	uint32 i = 0;

	if ( hTemplate == NULL ) {
		return;
	}

	for ( i = 0; i < hTemplate->arrNode.Count; i++ ) {
		XTE_Node* pNode = xte_private_template_get_node(hTemplate, i);
		if ( (pNode != NULL) && (pNode->iType == XTE_NODE_STATEMENT) && (pNode->Data.Statement.pData != NULL) && (hTemplate->hEngine != NULL) ) {
			const char* sName = xte_private_pool_ptr(hTemplate, pNode->Data.Statement.iStmtNameOff);
			const XTE_StatementDef* pDef = xte_private_find_statement(hTemplate->hEngine, sName, pNode->Data.Statement.iStmtNameSize);
			if ( pDef && pDef->procFreeData ) {
				pDef->procFreeData(pNode->Data.Statement.pData);
			}
		}
	}

	(xrtArrayUnit)(&hTemplate->arrArg);
	(xrtArrayUnit)(&hTemplate->arrExpr);
	(xrtArrayUnit)(&hTemplate->arrNode);
	(xrtArrayUnit)(&hTemplate->arrSubTemplate);
	xrtBufferUnit(&hTemplate->tStringPool);

	if ( hTemplate->bOwnEngine && hTemplate->hEngine ) {
		xteDestroyEngine(hTemplate->hEngine);
	}

	xrtFree(hTemplate);
}


// xteParseFree 相关处理
XXAPI void xteParseFree(xtetemplate hTemplate)
{
	xteDestroyTemplate(hTemplate);
}


// xteRenderEx 相关处理
XXAPI int xteRenderEx(xtetemplate hTemplate, const XTE_RenderOptions* pOptions, XTE_Error* pError)
{
	XTE_RenderCtx tCtx = { 0 };
	XTE_Flow iFlow = XTE_FLOW_OK;
	XTE_Error tLocalError = { 0 };
	XTE_Error* pTargetError = hTemplate ? &hTemplate->LastError : (pError ? pError : &tLocalError);

	xte_private_clear_error(pTargetError);

	if ( (hTemplate == NULL) || (pOptions == NULL) || (pOptions->pWriter == NULL) || (pOptions->pWriter->procWrite == NULL) ) {
		xte_private_fill_error_pos("", 0, 0, pTargetError, XTE_ERROR_RENDER, "template render options are invalid");
		if ( pError != pTargetError ) {
			xte_private_copy_error(pError, pTargetError);
		}
		return 0;
	}

	tCtx.hEngine = hTemplate->hEngine;
	tCtx.hTemplate = hTemplate;
	tCtx.pWriter = pOptions->pWriter;
	tCtx.pIncludeMap = pOptions->pIncludeMap;
	tCtx.pCurrent = pOptions->pCurrent ? pOptions->pCurrent : pOptions->pRoot;
	tCtx.pRoot = pOptions->pRoot ? pOptions->pRoot : pOptions->pCurrent;
	tCtx.pGlobal = pOptions->pGlobal;
	tCtx.pError = pTargetError;
	tCtx.iFlags = pOptions->iFlags;

	iFlow = xte_private_render_span(&tCtx, hTemplate->tRoot);
	if ( iFlow == XTE_FLOW_OK ) {
		if ( pError != pTargetError ) {
			xte_private_copy_error(pError, pTargetError);
		}
		return 1;
	}
	if ( (iFlow == XTE_FLOW_BREAK) && (pTargetError->iCode == 0) ) {
		xte_private_fill_error_pos("", 0, 0, pTargetError, XTE_ERROR_RENDER, "template break must be inside loop");
	}
	if ( (iFlow == XTE_FLOW_CONTINUE) && (pTargetError->iCode == 0) ) {
		xte_private_fill_error_pos("", 0, 0, pTargetError, XTE_ERROR_RENDER, "template continue must be inside loop");
	}
	if ( pError != pTargetError ) {
		xte_private_copy_error(pError, pTargetError);
	}
	return 0;
}


// 构建
XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize)
{
	XTE_RenderOptions tOptions = { 0 };
	XTE_Writer tWriter = { 0 };
	xbuffer_struct tBuf = { 0 };
	char chZero = 0;

	xrtBufferInit(&tBuf, 0);

	tWriter.procWrite = xte_private_buffer_writer_proc;
	tWriter.pUserData = &tBuf;

	tOptions.pCurrent = pCurrent;
	tOptions.pRoot = pCurrent;
	tOptions.pGlobal = pGlobal;
	tOptions.pIncludeMap = pIncludeMap;
	tOptions.pWriter = &tWriter;

	if ( !xteRenderEx(hTemplate, &tOptions, NULL) ) {
		xrtBufferUnit(&tBuf);
		if ( pRetSize ) {
			*pRetSize = 0u;
		}
		return NULL;
	}

	if ( !xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		if ( pRetSize ) {
			*pRetSize = 0u;
		}
		return NULL;
	}

	if ( pRetSize ) {
		*pRetSize = tBuf.Length - 1u;
	}

	return tBuf.Buffer;
}


// 解析路径
XXAPI xvalue xteResolvePath(const char* sPath, size_t iPathSize, xvalue pCurrent, xvalue pRoot, xvalue pLocal, xvalue pGlobal)
{
	const char* sText = sPath;
	uint32 iSize = (uint32)iPathSize;
	uint32 iPos = 0;
	uint32 iNameStart = 0;
	xvalue pVal = &XVO_VALUE_NULL;

	if ( sText == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( iSize == 0u ) {
		iSize = strlen(__xrt_cstr(sText));
	}
	if ( iSize == 0u ) {
		return &XVO_VALUE_NULL;
	}

	iNameStart = iPos;
	if ( !xte_private_is_ident_start(sText[iNameStart]) ) {
		return &XVO_VALUE_NULL;
	}
	iPos++;
	while ( (iPos < iSize) && xte_private_is_ident_char(sText[iPos]) ) {
		iPos++;
	}

	pVal = xte_private_lookup_first_value(&sText[iNameStart], iPos - iNameStart, pCurrent, pRoot, pLocal, pGlobal);

	while ( iPos < iSize ) {
		if ( sText[iPos] == '.' ) {
			uint32 iSubStart = ++iPos;

			if ( (iPos >= iSize) || !xte_private_is_ident_start(sText[iPos]) ) {
				return &XVO_VALUE_NULL;
			}
			while ( (iPos < iSize) && xte_private_is_ident_char(sText[iPos]) ) {
				iPos++;
			}
			if ( (pVal == NULL) || (pVal->Type != XVO_DT_TABLE) ) {
				return &XVO_VALUE_NULL;
			}
			pVal = xvoTableGetValue(pVal, &sText[iSubStart], iPos - iSubStart);
			continue;
		}

		if ( sText[iPos] == '[' ) {
			uint32 iIndexStart = ++iPos;
			char* sTemp = NULL;
			int64 iIndex = 0;

			while ( (iPos < iSize) && (sText[iPos] != ']') ) {
				iPos++;
			}
			if ( (iPos >= iSize) || (iPos == iIndexStart) ) {
				return &XVO_VALUE_NULL;
			}
			sTemp = xte_private_copy_view(&sText[iIndexStart], iPos - iIndexStart);
			if ( sTemp == NULL ) {
				return &XVO_VALUE_NULL;
			}
			iIndex = xrtStrToI64(sTemp);
			xrtFree(sTemp);
			iPos++;

			if ( pVal == NULL ) {
				return &XVO_VALUE_NULL;
			}
			if ( pVal->Type == XVO_DT_ARRAY ) {
				pVal = xvoArrayGetValue(pVal, (uint32)iIndex);
			} else if ( pVal->Type == XVO_DT_LIST ) {
				pVal = xvoListGetValue(pVal, iIndex);
			} else {
				return &XVO_VALUE_NULL;
			}
			continue;
		}

		return &XVO_VALUE_NULL;
	}

	return pVal ? pVal : &XVO_VALUE_NULL;
}


// 统计模板 get 节点
XXAPI uint32 xteTemplateGetNodeCount(xtetemplate hTemplate)
{
	return hTemplate ? hTemplate->arrNode.Count : 0u;
}


// xteTemplateGetExprCount 相关处理
XXAPI uint32 xteTemplateGetExprCount(xtetemplate hTemplate)
{
	return hTemplate ? hTemplate->arrExpr.Count : 0u;
}


// xteTemplateGetArgCount 相关处理
XXAPI uint32 xteTemplateGetArgCount(xtetemplate hTemplate)
{
	return hTemplate ? hTemplate->arrArg.Count : 0u;
}


// 获取模板字符串内存池大小
XXAPI uint32 xteTemplateGetStringPoolSize(xtetemplate hTemplate)
{
	return hTemplate ? hTemplate->tStringPool.Length : 0u;
}


// 获取模板最近一次解析或渲染错误
XXAPI const XTE_Error* xteTemplateGetLastError(xtetemplate hTemplate)
{
	return hTemplate ? &hTemplate->LastError : NULL;
}


// xteTemplateGetRootSpan 相关处理
XXAPI XTE_NodeSpan xteTemplateGetRootSpan(xtetemplate hTemplate)
{
	XTE_NodeSpan tSpan = { 0 };

	if ( hTemplate ) {
		tSpan = hTemplate->tRoot;
	}
	return tSpan;
}


// 获取模板节点
XXAPI const XTE_Node* xteTemplateGetNode(xtetemplate hTemplate, uint32 iIndex)
{
	return xte_private_template_get_node(hTemplate, iIndex);
}


// xteTemplateGetExpr 相关处理
XXAPI const XTE_ExprNode* xteTemplateGetExpr(xtetemplate hTemplate, uint32 iIndex)
{
	return xte_private_template_get_expr(hTemplate, iIndex);
}


// xteTemplateGetArg 相关处理
XXAPI const XTE_ArgItem* xteTemplateGetArg(xtetemplate hTemplate, uint32 iIndex)
{
	return xte_private_template_get_arg(hTemplate, iIndex);
}


// 获取模板字符串
XXAPI const char* xteTemplateGetString(xtetemplate hTemplate, uint32 iOff)
{
	return xte_private_pool_ptr(hTemplate, iOff);
}


// xteArgCount 相关处理
XXAPI uint32 xteArgCount(const XTE_ArgList* pArgs)
{
	return pArgs ? pArgs->iCount : 0u;
}


// xteArgAt 相关处理
XXAPI const XTE_ArgItem* xteArgAt(const XTE_ArgList* pArgs, uint32 iIndex)
{
	if ( (pArgs == NULL) || (iIndex >= pArgs->iCount) ) {
		return NULL;
	}
	return &pArgs->pItems[iIndex];
}


// xteFindNamedArg 相关处理
XXAPI const XTE_ArgItem* xteFindNamedArg(const XTE_ArgList* pArgs, const char* sName, size_t iNameSize)
{
	uint32 i = 0;

	if ( (pArgs == NULL) || (sName == NULL) ) {
		return NULL;
	}
	if ( iNameSize == 0u ) {
		iNameSize = strlen(__xrt_cstr(sName));
	}

	for ( i = 0; i < pArgs->iCount; i++ ) {
		const XTE_ArgItem* pArg = &pArgs->pItems[i];
		const char* sArgName = NULL;

		if ( pArg->iNameSize == 0u ) {
			continue;
		}
		sArgName = xteTemplateGetString(pArgs->hTemplate, pArg->iNameOff);
		if ( xte_private_str_eq(sArgName, pArg->iNameSize, sName, (uint32)iNameSize) ) {
			return pArg;
		}
	}

	return NULL;
}


// xteArgNameText 相关处理
XXAPI const char* xteArgNameText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg)
{
	if ( (pArgs == NULL) || (pArg == NULL) || (pArg->iNameSize == 0u) ) {
		return NULL;
	}
	return xteTemplateGetString(pArgs->hTemplate, pArg->iNameOff);
}


// xteArgRawText 相关处理
XXAPI const char* xteArgRawText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg)
{
	if ( (pArgs == NULL) || (pArg == NULL) || (pArg->iRawSize == 0u) ) {
		return NULL;
	}
	return xteTemplateGetString(pArgs->hTemplate, pArg->iRawOff);
}


// xteArgExprType 相关处理
XXAPI uint32 xteArgExprType(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg)
{
	const XTE_ExprNode* pExpr = NULL;

	if ( (pArgs == NULL) || (pArg == NULL) ) {
		return 0u;
	}
	pExpr = xteTemplateGetExpr(pArgs->hTemplate, pArg->iExprIndex);
	return pExpr ? pExpr->iType : 0u;
}


// xteEvalArgValue 相关处理
XXAPI xvalue xteEvalArgValue(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg)
{
	if ( (pCtx == NULL) || (pArg == NULL) ) {
		return xvoCreateNull();
	}
	return xte_private_eval_expr_value(pCtx, pArg->iExprIndex);
}


// xteEvalArgBool 相关处理
XXAPI int xteEvalArgBool(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut)
{
	xvalue pVal = xteEvalArgValue(pCtx, pArg);

	if ( pOut ) {
		*pOut = xte_private_value_truthy(pVal);
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgInt 相关处理
XXAPI int xteEvalArgInt(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut)
{
	xvalue pVal = xteEvalArgValue(pCtx, pArg);

	if ( pOut ) {
		if ( pVal->Type == XVO_DT_TEXT ) {
			*pOut = xrtStrToI64(xvoGetText(pVal));
		} else if ( pVal->Type == XVO_DT_FLOAT ) {
			*pOut = (int64)xvoGetFloat(pVal);
		} else {
			*pOut = xvoGetInt(pVal);
		}
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgFloat 相关处理
XXAPI int xteEvalArgFloat(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut)
{
	xvalue pVal = xteEvalArgValue(pCtx, pArg);

	if ( pOut ) {
		if ( pVal->Type == XVO_DT_TEXT ) {
			*pOut = xrtStrToNum(xvoGetText(pVal));
		} else if ( pVal->Type == XVO_DT_INT ) {
			*pOut = (double)xvoGetInt(pVal);
		} else {
			*pOut = xvoGetFloat(pVal);
		}
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgText 相关处理
XXAPI char* xteEvalArgText(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg)
{
	xvalue pVal = xteEvalArgValue(pCtx, pArg);
	char* sRet = xte_private_value_to_text(pVal);

	xvoUnref(pVal);
	return sRet;
}


// xte_private_eval_arg_strict_type 相关处理
static int xte_private_eval_arg_strict_type(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, uint32 iType, xvalue* ppVal)
{
	xvalue pVal = xteEvalArgValue(pCtx, pArg);

	if ( (pVal == NULL) || ((uint32)pVal->Type != iType) ) {
		xvoUnref(pVal);
		if ( ppVal != NULL ) {
			ppVal[0] = NULL;
		}
		return 0;
	}
	if ( ppVal != NULL ) {
		ppVal[0] = pVal;
		return 1;
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgBoolStrict 相关处理
XXAPI int xteEvalArgBoolStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut)
{
	xvalue pVal = NULL;

	if ( !xte_private_eval_arg_strict_type(pCtx, pArg, XVO_DT_BOOL, &pVal) ) {
		return 0;
	}
	if ( pOut != NULL ) {
		*pOut = xvoGetBool(pVal) ? 1 : 0;
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgIntStrict 相关处理
XXAPI int xteEvalArgIntStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut)
{
	xvalue pVal = NULL;

	if ( !xte_private_eval_arg_strict_type(pCtx, pArg, XVO_DT_INT, &pVal) ) {
		return 0;
	}
	if ( pOut != NULL ) {
		*pOut = xvoGetInt(pVal);
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgFloatStrict 相关处理
XXAPI int xteEvalArgFloatStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut)
{
	xvalue pVal = NULL;

	if ( !xte_private_eval_arg_strict_type(pCtx, pArg, XVO_DT_FLOAT, &pVal) ) {
		return 0;
	}
	if ( pOut != NULL ) {
		*pOut = xvoGetFloat(pVal);
	}
	xvoUnref(pVal);
	return 1;
}


// xteEvalArgTextStrict 相关处理
XXAPI char* xteEvalArgTextStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg)
{
	xvalue pVal = NULL;
	char* sRet = NULL;

	if ( !xte_private_eval_arg_strict_type(pCtx, pArg, XVO_DT_TEXT, &pVal) ) {
		return NULL;
	}
	sRet = (char*)xrtCopyStr(xvoGetText(pVal), pVal->Size);
	xvoUnref(pVal);
	return sRet;
}


// xteStmtParseRequireArg 相关处理
XXAPI const XTE_ArgItem* xteStmtParseRequireArg(XTE_StmtParseCtx* pCtx, uint32 iIndex, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteArgAt(pCtx ? pCtx->pArgs : NULL, iIndex);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, sDesc ? sDesc : "template argument is required");
	return NULL;
}


// xteStmtParseRequireNamedArg 相关处理
XXAPI const XTE_ArgItem* xteStmtParseRequireNamedArg(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFindNamedArg(pCtx ? pCtx->pArgs : NULL, sName, iNameSize);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, sDesc ? sDesc : "template named argument is required");
	return NULL;
}


// xte_private_parse_expr_desc 相关处理
static const char* xte_private_parse_expr_desc(uint32 iExprType, int bNamed)
{
	switch ( iExprType ) {
		case XTE_EXPR_PATH:
			return bNamed ? "template named argument must be path expression" : "template argument must be path expression";
		case XTE_EXPR_TEXT:
			return bNamed ? "template named argument must be text literal" : "template argument must be text literal";
		case XTE_EXPR_INT:
			return bNamed ? "template named argument must be int literal" : "template argument must be int literal";
		case XTE_EXPR_BOOL:
			return bNamed ? "template named argument must be bool literal" : "template argument must be bool literal";
		case XTE_EXPR_BOOL_EXPR:
			return bNamed ? "template named argument must be bool expression" : "template argument must be bool expression";
		default:
			return bNamed ? "template named argument expression type mismatch" : "template argument expression type mismatch";
	}
}


// xteStmtParseRequireExprType 相关处理
XXAPI const XTE_ArgItem* xteStmtParseRequireExprType(XTE_StmtParseCtx* pCtx, uint32 iIndex, uint32 iExprType, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtParseRequireArg(pCtx, iIndex, NULL);

	if ( pArg == NULL ) {
		return NULL;
	}
	if ( xteArgExprType((pCtx != NULL) ? pCtx->pArgs : NULL, pArg) == iExprType ) {
		return pArg;
	}
	xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, sDesc ? sDesc : xte_private_parse_expr_desc(iExprType, 0));
	return NULL;
}


// xteStmtParseRequireNamedExprType 相关处理
XXAPI const XTE_ArgItem* xteStmtParseRequireNamedExprType(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, uint32 iExprType, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtParseRequireNamedArg(pCtx, sName, iNameSize, NULL);

	if ( pArg == NULL ) {
		return NULL;
	}
	if ( xteArgExprType((pCtx != NULL) ? pCtx->pArgs : NULL, pArg) == iExprType ) {
		return pArg;
	}
	xteStmtParseSetError(pCtx, XTE_ERROR_PARSE, sDesc ? sDesc : xte_private_parse_expr_desc(iExprType, 1));
	return NULL;
}


// xteStmtRequireArg 相关处理
XXAPI const XTE_ArgItem* xteStmtRequireArg(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteArgAt((pCtx != NULL) ? pCtx->pArgs : NULL, iIndex);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template argument is required");
	return NULL;
}


// xteStmtRequireNamedArg 相关处理
XXAPI const XTE_ArgItem* xteStmtRequireNamedArg(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFindNamedArg((pCtx != NULL) ? pCtx->pArgs : NULL, sName, iNameSize);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template named argument is required");
	return NULL;
}


// xteStmtRequireBoolStrict 相关处理
XXAPI int xteStmtRequireBoolStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgBoolStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template bool argument must stay bool") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireNamedBoolStrict 相关处理
XXAPI int xteStmtRequireNamedBoolStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgBoolStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template bool argument must stay bool") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireIntStrict 相关处理
XXAPI int xteStmtRequireIntStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgIntStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template int argument must stay int") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireNamedIntStrict 相关处理
XXAPI int xteStmtRequireNamedIntStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgIntStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template int argument must stay int") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireFloatStrict 相关处理
XXAPI int xteStmtRequireFloatStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgFloatStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template float argument must stay float") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireNamedFloatStrict 相关处理
XXAPI int xteStmtRequireNamedFloatStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgFloatStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return (xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template float argument must stay float") == XTE_FLOW_ERROR) ? 0 : 1;
}


// xteStmtRequireTextStrict 相关处理
XXAPI char* xteStmtRequireTextStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireArg(pCtx, iIndex, sDesc);
	char* sRet = NULL;

	if ( pArg == NULL ) {
		return NULL;
	}
	sRet = xteEvalArgTextStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg);
	if ( sRet != NULL ) {
		return sRet;
	}
	xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template text argument must stay text");
	return NULL;
}


// xteStmtRequireNamedTextStrict 相关处理
XXAPI char* xteStmtRequireNamedTextStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteStmtRequireNamedArg(pCtx, sName, iNameSize, sDesc);
	char* sRet = NULL;

	if ( pArg == NULL ) {
		return NULL;
	}
	sRet = xteEvalArgTextStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg);
	if ( sRet != NULL ) {
		return sRet;
	}
	xteStmtSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template text argument must stay text");
	return NULL;
}


// xteFuncRequireArg 相关处理
XXAPI const XTE_ArgItem* xteFuncRequireArg(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteArgAt((pCtx != NULL) ? pCtx->pArgs : NULL, iIndex);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template argument is required");
	return NULL;
}


// xteFuncRequireNamedArg 相关处理
XXAPI const XTE_ArgItem* xteFuncRequireNamedArg(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFindNamedArg((pCtx != NULL) ? pCtx->pArgs : NULL, sName, iNameSize);

	if ( pArg != NULL ) {
		return pArg;
	}
	xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template named argument is required");
	return NULL;
}


// xteFuncRequireBoolStrict 相关处理
XXAPI int xteFuncRequireBoolStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgBoolStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template bool argument must stay bool");
}


// xteFuncRequireNamedBoolStrict 相关处理
XXAPI int xteFuncRequireNamedBoolStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgBoolStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template bool argument must stay bool");
}


// xteFuncRequireIntStrict 相关处理
XXAPI int xteFuncRequireIntStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgIntStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template int argument must stay int");
}


// xteFuncRequireNamedIntStrict 相关处理
XXAPI int xteFuncRequireNamedIntStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgIntStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template int argument must stay int");
}


// xteFuncRequireFloatStrict 相关处理
XXAPI int xteFuncRequireFloatStrict(XTE_FuncCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireArg(pCtx, iIndex, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgFloatStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template float argument must stay float");
}


// xteFuncRequireNamedFloatStrict 相关处理
XXAPI int xteFuncRequireNamedFloatStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireNamedArg(pCtx, sName, iNameSize, sDesc);

	if ( pArg == NULL ) {
		return 0;
	}
	if ( xteEvalArgFloatStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg, pOut) ) {
		return 1;
	}
	return xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template float argument must stay float");
}


// xteFuncRequireTextStrict 相关处理
XXAPI char* xteFuncRequireTextStrict(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireArg(pCtx, iIndex, sDesc);
	char* sRet = NULL;

	if ( pArg == NULL ) {
		return NULL;
	}
	sRet = xteEvalArgTextStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg);
	if ( sRet != NULL ) {
		return sRet;
	}
	xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template text argument must stay text");
	return NULL;
}


// xteFuncRequireNamedTextStrict 相关处理
XXAPI char* xteFuncRequireNamedTextStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc)
{
	const XTE_ArgItem* pArg = xteFuncRequireNamedArg(pCtx, sName, iNameSize, sDesc);
	char* sRet = NULL;

	if ( pArg == NULL ) {
		return NULL;
	}
	sRet = xteEvalArgTextStrict((pCtx != NULL) ? pCtx->pRender : NULL, pArg);
	if ( sRet != NULL ) {
		return sRet;
	}
	xteFuncSetError(pCtx, XTE_ERROR_RENDER, sDesc ? sDesc : "template text argument must stay text");
	return NULL;
}


// xteStmtParseSetError 相关处理
XXAPI int xteStmtParseSetError(XTE_StmtParseCtx* pCtx, int iCode, const char* sDesc)
{
	if ( (pCtx != NULL) && (pCtx->pError != NULL) && (pCtx->pError->iCode == 0) ) {
		pCtx->pError->iCode = iCode;
		pCtx->pError->sDesc = sDesc;
	}
	return 0;
}


// xteStmtSetError 相关处理
XXAPI XTE_Flow xteStmtSetError(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc)
{
	if ( (pCtx != NULL) && (pCtx->pRender != NULL) && (pCtx->pRender->pError != NULL) && (pCtx->pRender->pError->iCode == 0) ) {
		pCtx->pRender->pError->iCode = iCode;
		pCtx->pRender->pError->sDesc = sDesc;
	}
	return XTE_FLOW_ERROR;
}


// xteFuncSetError 相关处理
XXAPI int xteFuncSetError(XTE_FuncCtx* pCtx, int iCode, const char* sDesc)
{
	if ( (pCtx != NULL) && (pCtx->pRender != NULL) && (pCtx->pRender->pError != NULL) && (pCtx->pRender->pError->iCode == 0) ) {
		pCtx->pRender->pError->iCode = iCode;
		pCtx->pRender->pError->sDesc = sDesc;
	}
	return 0;
}


// xteStmtWrite 相关处理
XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize)
{
	if ( (pCtx == NULL) || (pCtx->pRender == NULL) ) {
		return 0;
	}
	if ( iSize == 0u ) {
		iSize = strlen(__xrt_cstr(sText));
	}
	return xte_private_writer_write(pCtx->pRender->pWriter, sText, iSize);
}


// xteStmtRenderBody 相关处理
XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->pRender == NULL) || (pCtx->pBody == NULL) ) {
		return 1;
	}
	return xte_private_render_span(pCtx->pRender, *pCtx->pBody) != XTE_FLOW_ERROR;
}


// xteStmtRenderBodyWithScope 相关处理
XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent)
{
	if ( (pCtx == NULL) || (pCtx->pRender == NULL) || (pCtx->pBody == NULL) ) {
		return 1;
	}

	return xte_private_render_body_with_scope_ex(pCtx, pLocal, pCurrent) != XTE_FLOW_ERROR;
}


// xte_private_stmt_render_error 相关处理
static XTE_Flow xte_private_stmt_render_error(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc)
{
	return xteStmtSetError(pCtx, iCode, sDesc);
}

#ifdef XTE_ENABLE_FILE
// xteTemplateSaveFile 相关处理
XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError)
{
	xbuffer_struct tBuf = { 0 };
	XTE_PrivateFileHeader tHeader = { 0 };
	uint32 i = 0;
	char sMagic[8] = { 'X', 'T', 'E', 'B', 'I', 'N', '1', 0 };

	(void)iFlags;
	xte_private_clear_error(pError);

	if ( (hTemplate == NULL) || (sFilePath == NULL) ) {
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file args are invalid");
		return 0;
	}

	xrtBufferInit(&tBuf, 0);
	memcpy(tHeader.sMagic, sMagic, sizeof(sMagic));
	tHeader.iVersion = XTE_PRIVATE_FILE_VERSION;
	tHeader.iStringPoolSize = hTemplate->tStringPool.Length;
	tHeader.iNodeCount = hTemplate->arrNode.Count;
	tHeader.iExprCount = hTemplate->arrExpr.Count;
	tHeader.iArgCount = hTemplate->arrArg.Count;
	tHeader.tRoot = hTemplate->tRoot;

	if ( !xrtBufferAppend(&tBuf, &tHeader, sizeof(tHeader), XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file buffer alloc failed");
		return 0;
	}
	if ( tHeader.iStringPoolSize && !xrtBufferAppend(&tBuf, hTemplate->tStringPool.Buffer, tHeader.iStringPoolSize, XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file buffer alloc failed");
		return 0;
	}
	if ( tHeader.iExprCount && !xrtBufferAppend(&tBuf, hTemplate->arrExpr.Memory, hTemplate->arrExpr.Count * sizeof(XTE_ExprNode), XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file buffer alloc failed");
		return 0;
	}
	if ( tHeader.iArgCount && !xrtBufferAppend(&tBuf, hTemplate->arrArg.Memory, hTemplate->arrArg.Count * sizeof(XTE_ArgItem), XBUF_BINARY) ) {
		xrtBufferUnit(&tBuf);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file buffer alloc failed");
		return 0;
	}
	if ( tHeader.iNodeCount ) {
		for ( i = 0; i < tHeader.iNodeCount; i++ ) {
			XTE_Node tNode = *xte_private_template_get_node(hTemplate, i);
			tNode.Data.Statement.pData = NULL;
			if ( !xrtBufferAppend(&tBuf, &tNode, sizeof(tNode), XBUF_BINARY) ) {
				xrtBufferUnit(&tBuf);
				xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file buffer alloc failed");
				return 0;
			}
		}
	}

	if ( xrtFilePutAll((str)sFilePath, tBuf.Buffer, tBuf.Length) == 0 ) {
		xrtBufferUnit(&tBuf);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file save failed");
		return 0;
	}

	xrtBufferUnit(&tBuf);
	return 1;
}


// 加载模板文件
XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError)
{
	ptr pMem = NULL;
	size_t iSize = 0;
	XTE_PrivateFileHeader* pHeader = NULL;
	const char sMagic[8] = { 'X', 'T', 'E', 'B', 'I', 'N', '1', 0 };
	xtetemplate hTemplate = NULL;
	int bOwnEngine = 0;
	uint32 iOffset = 0;

	(void)iFlags;
	xte_private_clear_error(pError);

	if ( sFilePath == NULL ) {
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file path is null");
		return NULL;
	}

	pMem = xrtFileGetAll((str)sFilePath, &iSize);
	if ( (pMem == NULL) || (iSize < sizeof(XTE_PrivateFileHeader)) ) {
		xrtFree(pMem);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file read failed");
		return NULL;
	}

	pHeader = (XTE_PrivateFileHeader*)pMem;
	if ( (memcmp(pHeader->sMagic, sMagic, sizeof(sMagic)) != 0) || (pHeader->iVersion != XTE_PRIVATE_FILE_VERSION) ) {
		xrtFree(pMem);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file header is invalid");
		return NULL;
	}

	if ( hEngine == NULL ) {
		hEngine = xteCreateEngine();
		if ( hEngine == NULL ) {
			xrtFree(pMem);
			xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template engine alloc failed");
			return NULL;
		}
		xteRegisterBuiltinStatements(hEngine);
		bOwnEngine = 1;
	}

	hTemplate = xte_private_template_create(hEngine, bOwnEngine);
	if ( hTemplate == NULL ) {
		if ( bOwnEngine ) {
			xteDestroyEngine(hEngine);
		}
		xrtFree(pMem);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template alloc failed");
		return NULL;
	}

	iOffset = sizeof(XTE_PrivateFileHeader);
	if ( (iOffset + pHeader->iStringPoolSize + (pHeader->iExprCount * sizeof(XTE_ExprNode)) + (pHeader->iArgCount * sizeof(XTE_ArgItem)) + (pHeader->iNodeCount * sizeof(XTE_Node))) > iSize ) {
		xteDestroyTemplate(hTemplate);
		xrtFree(pMem);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_FILE, "template file size is invalid");
		return NULL;
	}

	if ( pHeader->iStringPoolSize && !xrtBufferAppend(&hTemplate->tStringPool, (ptr)((char*)pMem + iOffset), pHeader->iStringPoolSize, XBUF_BINARY) ) {
		xteDestroyTemplate(hTemplate);
		xrtFree(pMem);
		xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template load alloc failed");
		return NULL;
	}
	iOffset += pHeader->iStringPoolSize;

	if ( pHeader->iExprCount ) {
		if ( xrtArrayAppend(&hTemplate->arrExpr, pHeader->iExprCount) == 0u ) {
			xteDestroyTemplate(hTemplate);
			xrtFree(pMem);
			xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template load alloc failed");
			return NULL;
		}
		memcpy(hTemplate->arrExpr.Memory, (char*)pMem + iOffset, pHeader->iExprCount * sizeof(XTE_ExprNode));
		iOffset += pHeader->iExprCount * sizeof(XTE_ExprNode);
	}

	if ( pHeader->iArgCount ) {
		if ( xrtArrayAppend(&hTemplate->arrArg, pHeader->iArgCount) == 0u ) {
			xteDestroyTemplate(hTemplate);
			xrtFree(pMem);
			xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template load alloc failed");
			return NULL;
		}
		memcpy(hTemplate->arrArg.Memory, (char*)pMem + iOffset, pHeader->iArgCount * sizeof(XTE_ArgItem));
		iOffset += pHeader->iArgCount * sizeof(XTE_ArgItem);
	}

	if ( pHeader->iNodeCount ) {
		if ( xrtArrayAppend(&hTemplate->arrNode, pHeader->iNodeCount) == 0u ) {
			xteDestroyTemplate(hTemplate);
			xrtFree(pMem);
			xte_private_fill_error_pos("", 0, 0, pError, XTE_ERROR_MALLOC, "template load alloc failed");
			return NULL;
		}
		memcpy(hTemplate->arrNode.Memory, (char*)pMem + iOffset, pHeader->iNodeCount * sizeof(XTE_Node));
	}

	hTemplate->tRoot = pHeader->tRoot;
	if ( !xte_private_rebuild_statement_data(hTemplate, XTE_ERROR_FILE, "template statement load parse callback failed") ) {
		xte_private_copy_error(pError, &hTemplate->LastError);
		xteDestroyTemplate(hTemplate);
		xrtFree(pMem);
		return NULL;
	}
	if ( !xte_private_rebuild_subtemplates(hTemplate, XTE_ERROR_FILE, "template define load failed") ) {
		xte_private_copy_error(pError, &hTemplate->LastError);
		xteDestroyTemplate(hTemplate);
		xrtFree(pMem);
		return NULL;
	}
	xrtFree(pMem);
	return hTemplate;
}
#endif

#ifdef XTE_DEBUGMODE
// 导出模板
XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags)
{
	(void)iFlags;

	if ( (hTemplate == NULL) || (pWriter == NULL) || (pWriter->procWrite == NULL) ) {
		return 0;
	}

	return xte_private_dump_span(hTemplate, pWriter, hTemplate->tRoot, 0u);
}


// xteTemplateDumpConsole 相关处理
XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags)
{
	XTE_Writer tWriter = { 0 };

	tWriter.procWrite = xte_private_console_writer_proc;
	tWriter.pUserData = stdout;
	return xteTemplateDump(hTemplate, &tWriter, iFlags);
}
#endif
