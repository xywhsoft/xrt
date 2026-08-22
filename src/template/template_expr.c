#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)



/* 表达式编译器只借用模板源串和统一编译预算。 */
typedef struct xrt_template_expression_parser {
	xrt_template_parser* Parser;
	size_t Start;
	size_t End;
	size_t Position;
	size_t Depth;
} xrt_template_expression_parser;



/* 跳过表达式当前位置之后的 ASCII 空白。 */
static void __xrtTemplateExprSkipSpace(
	xrt_template_expression_parser* pExpr
)
{
	while ( (pExpr->Position < pExpr->End) &&
		__xrtTemplateSpace(
			pExpr->Parser->Template->Source[pExpr->Position]
		) ) {
		pExpr->Position++;
	}
}



/* 判断字节是否能继续组成关键字或路径名称。 */
static bool __xrtTemplateExprNameByte(char iByte)
{
	return ((iByte >= 'a') && (iByte <= 'z')) ||
		((iByte >= 'A') && (iByte <= 'Z')) ||
		((iByte >= '0') && (iByte <= '9')) || (iByte == '_');
}



/* 在名称边界上匹配一个不区分上下文的 ASCII 关键字。 */
static bool __xrtTemplateExprKeyword(
	xrt_template_expression_parser* pExpr,
	cstr sKeyword
)
{
	size_t iSize = strlen(sKeyword);
	size_t iEnd;

	__xrtTemplateExprSkipSpace(pExpr);
	if ( iSize > (pExpr->End - pExpr->Position) ) {
		return false;
	}
	iEnd = pExpr->Position + iSize;
	if ( (memcmp(
		pExpr->Parser->Template->Source + pExpr->Position,
		sKeyword,
		iSize
	) != 0) ||
		 ((iEnd < pExpr->End) &&
		  __xrtTemplateExprNameByte(
			pExpr->Parser->Template->Source[iEnd]
		  )) ) {
		return false;
	}
	pExpr->Position = iEnd;
	return true;
}



/* 匹配一个固定运算符并推进表达式位置。 */
static bool __xrtTemplateExprOperator(
	xrt_template_expression_parser* pExpr,
	cstr sOperator
)
{
	size_t iSize = strlen(sOperator);

	__xrtTemplateExprSkipSpace(pExpr);
	if ( (iSize > (pExpr->End - pExpr->Position)) ||
		 (memcmp(
			pExpr->Parser->Template->Source + pExpr->Position,
			sOperator,
			iSize
		 ) != 0) ) {
		return false;
	}
	pExpr->Position += iSize;
	return true;
}



/* 把一个表达式节点追加到统一数组并返回稳定索引。 */
static bool __xrtTemplateExprPush(
	xrt_template_expression_parser* pExpr,
	const xrt_template_expr* pNode,
	uint32* pIndex
)
{
	if ( pExpr->Parser->Template->Expressions.Count >=
		pExpr->Parser->Config->MaxExpressions ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"compile-expression",
			"template expression node limit exceeded",
			pExpr->Parser->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	*pIndex = (uint32)pExpr->Parser->Template->Expressions.Count;
	return xrtArrayPush(
		&pExpr->Parser->Template->Expressions,
		pNode
	);
}



/* 统一报告表达式语法错误并保留源区间。 */
static bool __xrtTemplateExprSyntax(
	xrt_template_expression_parser* pExpr,
	size_t iOffset,
	size_t iSize,
	cstr sMessage
)
{
	__xrtTemplateError(
		XERR_VALUE,
		XTEMPLATE_ERROR_SYNTAX,
		"compile-expression",
		sMessage,
		pExpr->Parser->Template,
		iOffset,
		iSize
	);
	return false;
}



/* 判断无引号 token 是否具有完整整数词法形态。 */
static bool __xrtTemplateExprIntegerToken(xstrview Token)
{
	size_t i = 0;

	if ( Token.Size == 0 ) {
		return false;
	}
	if ( (Token.Data[i] == '+') || (Token.Data[i] == '-') ) {
		i++;
	}
	if ( i == Token.Size ) {
		return false;
	}
	for ( ; i < Token.Size; i++ ) {
		if ( (Token.Data[i] < '0') || (Token.Data[i] > '9') ) {
			return false;
		}
	}
	return true;
}



/* 判断无引号 token 是否应交给严格浮点解析器。 */
static bool __xrtTemplateExprFloatToken(xstrview Token)
{
	size_t i = 0;
	bool bDigit = false;
	bool bMarker = false;

	if ( Token.Size == 0 ) {
		return false;
	}
	if ( (Token.Data[i] == '+') || (Token.Data[i] == '-') ) {
		i++;
	}
	for ( ; i < Token.Size; i++ ) {
		char iByte = Token.Data[i];

		if ( (iByte >= '0') && (iByte <= '9') ) {
			bDigit = true;
			continue;
		}
		if ( (iByte == '.') || (iByte == 'e') || (iByte == 'E') ||
			 (iByte == '+') || (iByte == '-') ) {
			bMarker = true;
			continue;
		}
		return false;
	}
	return bDigit && bMarker;
}



static bool __xrtTemplateExprParseOr(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
);



/* 编译字符串、数值、布尔、null、路径和括号原子。 */
static bool __xrtTemplateExprParsePrimary(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
)
{
	xrt_template_expr Node;
	cstr sSource = pExpr->Parser->Template->Source;
	size_t iStart;
	size_t iEnd;

	__xrtTemplateExprSkipSpace(pExpr);
	if ( pExpr->Position >= pExpr->End ) {
		return __xrtTemplateExprSyntax(
			pExpr,
			pExpr->Position,
			0,
			"template expression is incomplete"
		);
	}
	if ( sSource[pExpr->Position] == '(' ) {
		if ( pExpr->Depth >=
			pExpr->Parser->Config->MaxExpressionDepth ) {
			__xrtTemplateError(
				XERR_RANGE,
				XTEMPLATE_ERROR_LIMIT,
				"compile-expression",
				"template expression depth limit exceeded",
				pExpr->Parser->Template,
				pExpr->Position,
				1u
			);
			return false;
		}
		pExpr->Position++;
		pExpr->Depth++;
		if ( !__xrtTemplateExprParseOr(pExpr, pIndex) ) {
			return false;
		}
		pExpr->Depth--;
		__xrtTemplateExprSkipSpace(pExpr);
		if ( (pExpr->Position >= pExpr->End) ||
			 (sSource[pExpr->Position] != ')') ) {
			return __xrtTemplateExprSyntax(
				pExpr,
				pExpr->Position,
				0,
				"template expression has no closing parenthesis"
			);
		}
		pExpr->Position++;
		return true;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Left = XRT_TEMPLATE_INDEX_NONE;
	Node.Right = XRT_TEMPLATE_INDEX_NONE;
	if ( (sSource[pExpr->Position] == '\'') ||
		 (sSource[pExpr->Position] == '"') ) {
		char iQuote = sSource[pExpr->Position++];

		iStart = pExpr->Position;
		while ( pExpr->Position < pExpr->End ) {
			if ( sSource[pExpr->Position] == '\\' ) {
				if ( (pExpr->Position + 1u) >= pExpr->End ) {
					break;
				}
				pExpr->Position += 2u;
				continue;
			}
			if ( sSource[pExpr->Position] == iQuote ) {
				break;
			}
			pExpr->Position++;
		}
		if ( (pExpr->Position >= pExpr->End) ||
			 (sSource[pExpr->Position] != iQuote) ) {
			return __xrtTemplateExprSyntax(
				pExpr,
				iStart - 1u,
				pExpr->End - (iStart - 1u),
				"template string literal has no closing quote"
			);
		}
		iEnd = pExpr->Position++;
		Node.Type = XRT_TEMPLATE_EXPR_STRING;
		Node.SourceOffset = (uint32)(iStart - 1u);
		Node.SourceSize = (uint32)(pExpr->Position - (iStart - 1u));
		if ( !__xrtTemplateTextUnescape(
			pExpr->Parser,
			iStart,
			iEnd,
			&Node.TextOffset,
			&Node.TextSize
		) ) {
			return false;
		}
		return __xrtTemplateExprPush(pExpr, &Node, pIndex);
	}
	iStart = pExpr->Position;
	while ( pExpr->Position < pExpr->End ) {
		char iByte = sSource[pExpr->Position];

		if ( __xrtTemplateSpace(iByte) || (iByte == '(') ||
			 (iByte == ')') || (iByte == '!') || (iByte == '=') ||
			 (iByte == '>') || (iByte == '<') || (iByte == '~') ||
			 (iByte == '&') || (iByte == '|') ) {
			break;
		}
		pExpr->Position++;
	}
	iEnd = pExpr->Position;
	if ( iStart == iEnd ) {
		return __xrtTemplateExprSyntax(
			pExpr,
			iStart,
			1u,
			"invalid template expression token"
		);
	}
	Node.SourceOffset = (uint32)iStart;
	Node.SourceSize = (uint32)(iEnd - iStart);
	{
		xstrview Token = { sSource + iStart, iEnd - iStart };

		if ( (Token.Size == 4u) &&
			 (memcmp(Token.Data, "true", 4u) == 0) ) {
			Node.Type = XRT_TEMPLATE_EXPR_BOOL;
			Node.Value.Bool = true;
		} else if ( (Token.Size == 5u) &&
			 (memcmp(Token.Data, "false", 5u) == 0) ) {
			Node.Type = XRT_TEMPLATE_EXPR_BOOL;
			Node.Value.Bool = false;
		} else if ( (Token.Size == 4u) &&
			 (memcmp(Token.Data, "null", 4u) == 0) ) {
			Node.Type = XRT_TEMPLATE_EXPR_NULL;
		} else if ( __xrtTemplateExprIntegerToken(Token) ) {
			Node.Type = XRT_TEMPLATE_EXPR_INT;
			if ( !xrtIntParse(Token, 10u, 0, &Node.Value.Integer) ) {
				__xrtTemplateWrapCurrent(
					XTEMPLATE_ERROR_SYNTAX,
					"compile-expression",
					"invalid template integer literal",
					pExpr->Parser->Template,
					iStart,
					iEnd - iStart
				);
				return false;
			}
		} else if ( __xrtTemplateExprFloatToken(Token) ) {
			Node.Type = XRT_TEMPLATE_EXPR_FLOAT;
			if ( !xrtNumParse(Token, 0, &Node.Value.Float) ) {
				__xrtTemplateWrapCurrent(
					XTEMPLATE_ERROR_SYNTAX,
					"compile-expression",
					"invalid template floating-point literal",
					pExpr->Parser->Template,
					iStart,
					iEnd - iStart
				);
				return false;
			}
		} else {
			Node.Type = XRT_TEMPLATE_EXPR_PATH;
			if ( !__xrtTemplateCompilePath(
				pExpr->Parser,
				iStart,
				iEnd,
				&Node.PathStart,
				&Node.PathCount
			) ) {
				return false;
			}
		}
	}
	return __xrtTemplateExprPush(pExpr, &Node, pIndex);
}



/* 编译一元逻辑非。 */
static bool __xrtTemplateExprParseUnary(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
)
{
	xrt_template_expr Node;
	size_t iStart;
	uint32 iChild;

	__xrtTemplateExprSkipSpace(pExpr);
	iStart = pExpr->Position;
	if ( __xrtTemplateExprKeyword(pExpr, "not") ||
		 __xrtTemplateExprOperator(pExpr, "!") ) {
		if ( !__xrtTemplateExprParseUnary(pExpr, &iChild) ) {
			return false;
		}
		memset(&Node, 0, sizeof(Node));
		Node.Type = XRT_TEMPLATE_EXPR_NOT;
		Node.SourceOffset = (uint32)iStart;
		Node.SourceSize = (uint32)(pExpr->Position - iStart);
		Node.Left = iChild;
		Node.Right = XRT_TEMPLATE_INDEX_NONE;
		return __xrtTemplateExprPush(pExpr, &Node, pIndex);
	}
	return __xrtTemplateExprParsePrimary(pExpr, pIndex);
}



/* 编译可选的单个比较运算。 */
static bool __xrtTemplateExprParseCompare(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
)
{
	xrt_template_expr Node;
	size_t iStart = pExpr->Position;
	uint16 Type = 0;
	uint32 iLeft;
	uint32 iRight;

	if ( !__xrtTemplateExprParseUnary(pExpr, &iLeft) ) {
		return false;
	}
	if ( __xrtTemplateExprOperator(pExpr, ">=") ) {
		Type = XRT_TEMPLATE_EXPR_GREATER_EQUAL;
	} else if ( __xrtTemplateExprOperator(pExpr, "<=") ) {
		Type = XRT_TEMPLATE_EXPR_LESS_EQUAL;
	} else if ( __xrtTemplateExprOperator(pExpr, "!=") ) {
		Type = XRT_TEMPLATE_EXPR_NOT_EQUAL;
	} else if ( __xrtTemplateExprOperator(pExpr, "~=") ) {
		Type = XRT_TEMPLATE_EXPR_APPROX;
	} else if ( __xrtTemplateExprOperator(pExpr, "==") ||
		 __xrtTemplateExprOperator(pExpr, "=") ) {
		Type = XRT_TEMPLATE_EXPR_EQUAL;
	} else if ( __xrtTemplateExprOperator(pExpr, ">") ) {
		Type = XRT_TEMPLATE_EXPR_GREATER;
	} else if ( __xrtTemplateExprOperator(pExpr, "<") ) {
		Type = XRT_TEMPLATE_EXPR_LESS;
	}
	if ( Type == 0 ) {
		*pIndex = iLeft;
		return true;
	}
	if ( !__xrtTemplateExprParseUnary(pExpr, &iRight) ) {
		return false;
	}
	memset(&Node, 0, sizeof(Node));
	Node.Type = Type;
	Node.SourceOffset = (uint32)iStart;
	Node.SourceSize = (uint32)(pExpr->Position - iStart);
	Node.Left = iLeft;
	Node.Right = iRight;
	return __xrtTemplateExprPush(pExpr, &Node, pIndex);
}



/* 编译左结合逻辑与。 */
static bool __xrtTemplateExprParseAnd(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
)
{
	uint32 iLeft;

	if ( !__xrtTemplateExprParseCompare(pExpr, &iLeft) ) {
		return false;
	}
	for ( ;; ) {
		xrt_template_expr Node;
		size_t iSaved = pExpr->Position;
		uint32 iRight;

		if ( !__xrtTemplateExprKeyword(pExpr, "and") &&
			 !__xrtTemplateExprOperator(pExpr, "&&") ) {
			pExpr->Position = iSaved;
			break;
		}
		if ( !__xrtTemplateExprParseCompare(pExpr, &iRight) ) {
			return false;
		}
		memset(&Node, 0, sizeof(Node));
		Node.Type = XRT_TEMPLATE_EXPR_AND;
		Node.SourceOffset = (uint32)iSaved;
		Node.SourceSize = (uint32)(pExpr->Position - iSaved);
		Node.Left = iLeft;
		Node.Right = iRight;
		if ( !__xrtTemplateExprPush(pExpr, &Node, &iLeft) ) {
			return false;
		}
	}
	*pIndex = iLeft;
	return true;
}



/* 编译左结合逻辑或。 */
static bool __xrtTemplateExprParseOr(
	xrt_template_expression_parser* pExpr,
	uint32* pIndex
)
{
	uint32 iLeft;

	if ( !__xrtTemplateExprParseAnd(pExpr, &iLeft) ) {
		return false;
	}
	for ( ;; ) {
		xrt_template_expr Node;
		size_t iSaved = pExpr->Position;
		uint32 iRight;

		if ( !__xrtTemplateExprKeyword(pExpr, "or") &&
			 !__xrtTemplateExprOperator(pExpr, "||") ) {
			pExpr->Position = iSaved;
			break;
		}
		if ( !__xrtTemplateExprParseAnd(pExpr, &iRight) ) {
			return false;
		}
		memset(&Node, 0, sizeof(Node));
		Node.Type = XRT_TEMPLATE_EXPR_OR;
		Node.SourceOffset = (uint32)iSaved;
		Node.SourceSize = (uint32)(pExpr->Position - iSaved);
		Node.Left = iLeft;
		Node.Right = iRight;
		if ( !__xrtTemplateExprPush(pExpr, &Node, &iLeft) ) {
			return false;
		}
	}
	*pIndex = iLeft;
	return true;
}



/* 编译一个完整表达式并拒绝尾随无效 token。 */
bool __xrtTemplateCompileExpression(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pExpression
)
{
	xrt_template_expression_parser Expr;

	while ( (iStart < iEnd) &&
		__xrtTemplateSpace(pParser->Template->Source[iStart]) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		__xrtTemplateSpace(pParser->Template->Source[iEnd - 1u]) ) {
		iEnd--;
	}
	if ( (pExpression == NULL) || (iStart == iEnd) ) {
		return __xrtTemplateExprSyntax(
			&(xrt_template_expression_parser){
				pParser, iStart, iEnd, iStart, 0
			},
			iStart,
			iEnd - iStart,
			"template expression is empty"
		);
	}
	memset(&Expr, 0, sizeof(Expr));
	Expr.Parser = pParser;
	Expr.Start = iStart;
	Expr.End = iEnd;
	Expr.Position = iStart;
	if ( !__xrtTemplateExprParseOr(&Expr, pExpression) ) {
		return false;
	}
	__xrtTemplateExprSkipSpace(&Expr);
	if ( Expr.Position != Expr.End ) {
		return __xrtTemplateExprSyntax(
			&Expr,
			Expr.Position,
			Expr.End - Expr.Position,
			"template expression contains trailing syntax"
		);
	}
	return true;
}



/* 从借用的 xvalue 创建不分配的表达式求值结果。 */
static void __xrtTemplateEvalValue(
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



/* 按模板语言规则读取求值结果的真值。 */
bool __xrtTemplateEvalTruthy(
	const xrt_template_eval* pValue,
	bool* pResult
)
{
	if ( (pValue == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pValue->Value != NULL ) {
		*pResult = xrtValueTruthy(pValue->Value);
		return true;
	}
	switch ( pValue->Type ) {
		case XVALUE_NULL: *pResult = false; break;
		case XVALUE_BOOL: *pResult = pValue->Data.Bool; break;
		case XVALUE_INT: *pResult = pValue->Data.Integer != 0; break;
		case XVALUE_FLOAT: *pResult = pValue->Data.Float != 0.0; break;
		case XVALUE_STRING:
		case XVALUE_BYTES: *pResult = pValue->Data.String.Size != 0; break;
		case XVALUE_TIME: *pResult = true; break;
		default:
			__xrtErrorSetType();
			return false;
	}
	return true;
}



/* 严格读取求值结果中的整数。 */
bool __xrtTemplateEvalInteger(
	const xrt_template_eval* pValue,
	int64* pResult
)
{
	if ( (pValue == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pValue->Type != XVALUE_INT ) {
		__xrtErrorSetType();
		return false;
	}
	if ( pValue->Value != NULL ) {
		return xrtValueGetInt(pValue->Value, pResult);
	}
	*pResult = pValue->Data.Integer;
	return true;
}



/* 读取可比较的数字并保留整数精确比较所需的类型。 */
static bool __xrtTemplateEvalNumber(
	const xrt_template_eval* pValue,
	int64* pInteger,
	double* pFloat,
	bool* pIsInteger
)
{
	if ( pValue->Type == XVALUE_INT ) {
		*pIsInteger = true;
		if ( pValue->Value != NULL ) {
			return xrtValueGetInt(pValue->Value, pInteger);
		}
		*pInteger = pValue->Data.Integer;
		return true;
	}
	if ( pValue->Type == XVALUE_FLOAT ) {
		*pIsInteger = false;
		if ( pValue->Value != NULL ) {
			return xrtValueGetFloat(pValue->Value, pFloat);
		}
		*pFloat = pValue->Data.Float;
		return true;
	}
	return false;
}



/* 借用字符串或二进制值的完整显式长度视图。 */
bool __xrtTemplateEvalText(
	const xrt_template_eval* pValue,
	xstrview* pText
)
{
	if ( pValue->Type == XVALUE_STRING ) {
		if ( pValue->Value != NULL ) {
			return xrtValueGetString(pValue->Value, pText);
		}
		*pText = pValue->Data.String;
		return true;
	}
	if ( pValue->Type == XVALUE_BYTES ) {
		xbytesview Data;

		if ( pValue->Value != NULL ) {
			if ( !xrtValueGetBytes(pValue->Value, &Data) ) {
				return false;
			}
			*pText = (xstrview){ (cstr)Data.Data, Data.Size };
		} else {
			*pText = pValue->Data.String;
		}
		return true;
	}
	return false;
}



/* 精确比较 int64 与 double，避免先转 double 丢失大整数低位。 */
static int __xrtTemplateCompareIntegerFloat(
	int64 iInteger,
	double fValue,
	bool* pUnordered
)
{
	int64 iFloatInteger;

	*pUnordered = fValue != fValue;
	if ( *pUnordered ) {
		return 0;
	}
	if ( fValue >= 9223372036854775808.0 ) {
		return -1;
	}
	if ( fValue < -9223372036854775808.0 ) {
		return 1;
	}
	iFloatInteger = (int64)fValue;
	if ( iInteger < iFloatInteger ) {
		return -1;
	}
	if ( iInteger > iFloatInteger ) {
		return 1;
	}
	if ( fValue > (double)iFloatInteger ) {
		return -1;
	}
	if ( fValue < (double)iFloatInteger ) {
		return 1;
	}
	return 0;
}



/* 比较两个标量且不把它们临时转换为字符串。 */
static bool __xrtTemplateEvalCompare(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint16 Type,
	const xrt_template_eval* pLeft,
	const xrt_template_eval* pRight,
	bool* pResult
)
{
	int64 iLeft = 0;
	int64 iRight = 0;
	double fLeft = 0.0;
	double fRight = 0.0;
	bool bLeftInt = false;
	bool bRightInt = false;
	bool bUnordered = false;
	int iCompare = 0;

	if ( __xrtTemplateEvalNumber(
		pLeft, &iLeft, &fLeft, &bLeftInt
	) && __xrtTemplateEvalNumber(
		pRight, &iRight, &fRight, &bRightInt
	) ) {
		if ( bLeftInt && bRightInt ) {
			iCompare = iLeft < iRight ? -1 : (iLeft > iRight ? 1 : 0);
			fLeft = (double)iLeft;
			fRight = (double)iRight;
		} else if ( bLeftInt ) {
			iCompare = __xrtTemplateCompareIntegerFloat(
				iLeft,
				fRight,
				&bUnordered
			);
			fLeft = (double)iLeft;
		} else if ( bRightInt ) {
			iCompare = -__xrtTemplateCompareIntegerFloat(
				iRight,
				fLeft,
				&bUnordered
			);
			fRight = (double)iRight;
		} else {
			bUnordered = (fLeft != fLeft) || (fRight != fRight);
			if ( !bUnordered ) {
				iCompare = fLeft < fRight ? -1 :
					(fLeft > fRight ? 1 : 0);
			}
		}
		if ( Type == XRT_TEMPLATE_EXPR_APPROX ) {
			double fDiff = fLeft - fRight;
			double fScale;

			if ( bUnordered ) {
				*pResult = false;
				return true;
			}

			if ( fDiff < 0.0 ) {
				fDiff = -fDiff;
			}
			fScale = fLeft < 0.0 ? -fLeft : fLeft;
			if ( (fRight < 0.0 ? -fRight : fRight) > fScale ) {
				fScale = fRight < 0.0 ? -fRight : fRight;
			}
			if ( fScale < 1.0 ) {
				fScale = 1.0;
			}
			*pResult = fDiff <= (fScale * 1e-9);
			return true;
		}
		if ( bUnordered ) {
			*pResult = Type == XRT_TEMPLATE_EXPR_NOT_EQUAL;
			return true;
		}
	} else if ( ((pLeft->Type == XVALUE_STRING) ||
			 (pLeft->Type == XVALUE_BYTES)) &&
		 ((pRight->Type == XVALUE_STRING) ||
			 (pRight->Type == XVALUE_BYTES)) ) {
		xstrview Left;
		xstrview Right;
		size_t iCommon;

		if ( !__xrtTemplateEvalText(pLeft, &Left) ||
			 !__xrtTemplateEvalText(pRight, &Right) ) {
			return false;
		}
		iCommon = Left.Size < Right.Size ? Left.Size : Right.Size;
		iCompare = iCommon != 0
			? memcmp(Left.Data, Right.Data, iCommon) : 0;
		if ( iCompare == 0 ) {
			iCompare = Left.Size < Right.Size ? -1 :
				(Left.Size > Right.Size ? 1 : 0);
		}
	} else if ( (pLeft->Type == XVALUE_BOOL) &&
		 (pRight->Type == XVALUE_BOOL) ) {
		bool bLeft;
		bool bRight;

		if ( pLeft->Value != NULL ) {
			if ( !xrtValueGetBool(pLeft->Value, &bLeft) ) {
				return false;
			}
		} else {
			bLeft = pLeft->Data.Bool;
		}
		if ( pRight->Value != NULL ) {
			if ( !xrtValueGetBool(pRight->Value, &bRight) ) {
				return false;
			}
		} else {
			bRight = pRight->Data.Bool;
		}
		iCompare = bLeft == bRight ? 0 : (bLeft ? 1 : -1);
	} else if ( (pLeft->Type == XVALUE_NULL) &&
		 (pRight->Type == XVALUE_NULL) ) {
		iCompare = 0;
	} else if ( (Type == XRT_TEMPLATE_EXPR_EQUAL) ||
		 (Type == XRT_TEMPLATE_EXPR_NOT_EQUAL) ) {
		iCompare = 1;
	} else {
		__xrtTemplateError(
			XERR_TYPE,
			XTEMPLATE_ERROR_TYPE,
			"evaluate-expression",
			"template comparison operands are not compatible scalars",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	switch ( Type ) {
		case XRT_TEMPLATE_EXPR_EQUAL:
		case XRT_TEMPLATE_EXPR_APPROX: *pResult = iCompare == 0; break;
		case XRT_TEMPLATE_EXPR_NOT_EQUAL: *pResult = iCompare != 0; break;
		case XRT_TEMPLATE_EXPR_GREATER: *pResult = iCompare > 0; break;
		case XRT_TEMPLATE_EXPR_LESS: *pResult = iCompare < 0; break;
		case XRT_TEMPLATE_EXPR_GREATER_EQUAL: *pResult = iCompare >= 0; break;
		case XRT_TEMPLATE_EXPR_LESS_EQUAL: *pResult = iCompare <= 0; break;
		default:
			__xrtErrorSetInternal();
			return false;
	}
	return true;
}



/* 递归求值后序表达式 AST，并对逻辑运算执行短路。 */
static bool __xrtTemplateEvalExpressionDepth(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iExpression,
	size_t iDepth,
	xrt_template_eval* pValue
)
{
	const xrt_template_expr* pExpr;
	xrt_template_eval Left;
	xrt_template_eval Right;
	bool bValue;
	bool bRight;

	if ( iDepth >= pRender->Config->MaxDepth ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"evaluate-expression",
			"template expression render depth limit exceeded",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return false;
	}
	pExpr = (const xrt_template_expr*)xrtArrayConstGet(
		&pRender->Template->Expressions,
		iExpression
	);
	if ( (pExpr == NULL) || !__xrtTemplateStep(pRender, pNode) ) {
		return false;
	}
	memset(pValue, 0, sizeof(*pValue));
	switch ( pExpr->Type ) {
		case XRT_TEMPLATE_EXPR_NULL:
			pValue->Type = XVALUE_NULL;
			return true;
		case XRT_TEMPLATE_EXPR_BOOL:
			pValue->Type = XVALUE_BOOL;
			pValue->Data.Bool = pExpr->Value.Bool;
			return true;
		case XRT_TEMPLATE_EXPR_INT:
			pValue->Type = XVALUE_INT;
			pValue->Data.Integer = pExpr->Value.Integer;
			return true;
		case XRT_TEMPLATE_EXPR_FLOAT:
			pValue->Type = XVALUE_FLOAT;
			pValue->Data.Float = pExpr->Value.Float;
			return true;
		case XRT_TEMPLATE_EXPR_STRING:
			pValue->Type = XVALUE_STRING;
			pValue->Data.String = (xstrview){
				pRender->Template->Text.Data != NULL
					? pRender->Template->Text.Data + pExpr->TextOffset
					: NULL,
				pExpr->TextSize
			};
			return true;
		case XRT_TEMPLATE_EXPR_PATH:
		{
			bool bFound;

			if ( !__xrtTemplateResolveCompiled(
				pRender,
				pNode,
				pExpr->PathStart,
				pExpr->PathCount,
				pValue,
				&bFound
			) ) {
				return false;
			}
			if ( !bFound ) {
				if ( (pRender->Config->Flags &
					XTEMPLATE_STRICT_UNDEFINED) != 0 ) {
					__xrtTemplateError(
						XERR_NOT_FOUND,
						XTEMPLATE_ERROR_UNDEFINED,
						"evaluate-expression",
						"template expression path is undefined",
						pRender->Template,
						pExpr->SourceOffset,
						pExpr->SourceSize
					);
					return false;
				}
				__xrtTemplateEvalValue(NULL, pValue);
			}
			return true;
		}
		case XRT_TEMPLATE_EXPR_NOT:
			if ( !__xrtTemplateEvalExpressionDepth(
				pRender, pNode, pExpr->Left, iDepth + 1u, &Left
			) || !__xrtTemplateEvalTruthy(&Left, &bValue) ) {
				return false;
			}
			pValue->Type = XVALUE_BOOL;
			pValue->Data.Bool = !bValue;
			return true;
		case XRT_TEMPLATE_EXPR_AND:
		case XRT_TEMPLATE_EXPR_OR:
			if ( !__xrtTemplateEvalExpressionDepth(
				pRender, pNode, pExpr->Left, iDepth + 1u, &Left
			) || !__xrtTemplateEvalTruthy(&Left, &bValue) ) {
				return false;
			}
			if ( ((pExpr->Type == XRT_TEMPLATE_EXPR_AND) && !bValue) ||
				 ((pExpr->Type == XRT_TEMPLATE_EXPR_OR) && bValue) ) {
				pValue->Type = XVALUE_BOOL;
				pValue->Data.Bool = bValue;
				return true;
			}
			if ( !__xrtTemplateEvalExpressionDepth(
				pRender, pNode, pExpr->Right, iDepth + 1u, &Right
			) || !__xrtTemplateEvalTruthy(&Right, &bRight) ) {
				return false;
			}
			pValue->Type = XVALUE_BOOL;
			pValue->Data.Bool = bRight;
			return true;
		default:
			if ( !__xrtTemplateEvalExpressionDepth(
				pRender, pNode, pExpr->Left, iDepth + 1u, &Left
			) || !__xrtTemplateEvalExpressionDepth(
				pRender, pNode, pExpr->Right, iDepth + 1u, &Right
			) || !__xrtTemplateEvalCompare(
				pRender, pNode, pExpr->Type, &Left, &Right, &bValue
			) ) {
				return false;
			}
			pValue->Type = XVALUE_BOOL;
			pValue->Data.Bool = bValue;
			return true;
	}
}



/* 求值已经编译的表达式。 */
bool __xrtTemplateEvalExpression(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iExpression,
	xrt_template_eval* pValue
)
{
	if ( (pRender == NULL) || (pNode == NULL) || (pValue == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTemplateEvalExpressionDepth(
		pRender,
		pNode,
		iExpression,
		0,
		pValue
	);
}

#endif
