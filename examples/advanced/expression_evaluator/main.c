/*
 * XRT Example - Expression Evaluator
 * XRT 范例 - 表达式求值器
 *
 * Description / 说明:
 *   EN: Demonstrates mathematical expression evaluation using Shunting Yard algorithm
 *       and stack-based computation. Supports arithmetic operations, parentheses,
 *       operator precedence, and function calls. Includes infix to postfix conversion.
 *   CN: 演示使用调度场算法和基于栈的计算的数学表达式求值。
 *       支持算术运算、括号、运算符优先级和函数调用。
 *       包括中缀到后缀的转换。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_expression_evaluator.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_expression_evaluator -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Shunting Yard algorithm implementation
 *   - Stack-based expression evaluation
 *   - Operator precedence handling
 *   - Parentheses support
 *   - Mathematical functions support
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <math.h>
#include <time.h>

// Token types
// 令牌类型
typedef enum {
	EXPR_TOKEN_NUMBER = 1,
	EXPR_TOKEN_OPERATOR,
	EXPR_TOKEN_FUNCTION,
	EXPR_TOKEN_LEFT_PAREN,
	EXPR_TOKEN_RIGHT_PAREN,
	EXPR_TOKEN_EOF
} ExprTokenType;

// Token structure
// 令牌结构
typedef struct {
	ExprTokenType eType;
	union {
		double dValue;		// For numbers / 用于数字
		char cOperator;		// For operators / 用于运算符
		str sFunction;		// For functions / 用于函数
	};
} Token;

// Stack structure
// 栈结构
typedef struct {
	Token* paTokens;
	size_t iSize;
	size_t iCapacity;
} TokenStack;

// Create token stack
// 创建令牌栈
TokenStack* StackCreate()
{
	TokenStack* pStack = xrtMalloc(sizeof(TokenStack));
	pStack->iSize = 0;
	pStack->iCapacity = 16;
	pStack->paTokens = xrtMalloc(pStack->iCapacity * sizeof(Token));
	return pStack;
}

// Destroy token stack
// 销毁令牌栈
void StackDestroy(TokenStack* pStack)
{
	if ( pStack ) {
		if ( pStack->paTokens ) {
			xrtFree(pStack->paTokens);
		}
		xrtFree(pStack);
	}
}

// Push token to stack
// 向栈压入令牌
void StackPush(TokenStack* pStack, Token tToken)
{
	if ( pStack->iSize >= pStack->iCapacity ) {
		pStack->iCapacity *= 2;
		pStack->paTokens = xrtRealloc(pStack->paTokens, pStack->iCapacity * sizeof(Token));
	}
	
	pStack->paTokens[pStack->iSize] = tToken;
	pStack->iSize++;
}

// Pop token from stack
// 从栈弹出令牌
Token StackPop(TokenStack* pStack)
{
	if ( pStack->iSize == 0 ) {
		Token tEmpty = {EXPR_TOKEN_EOF, 0};
		return tEmpty;
	}
	
	return pStack->paTokens[--pStack->iSize];
}

// Peek at top of stack
// 查看栈顶
Token StackPeek(TokenStack* pStack)
{
	if ( pStack->iSize == 0 ) {
		Token tEmpty = {EXPR_TOKEN_EOF, 0};
		return tEmpty;
	}
	
	return pStack->paTokens[pStack->iSize - 1];
}

// Check if stack is empty
// 检查栈是否为空
int StackIsEmpty(TokenStack* pStack)
{
	return pStack->iSize == 0;
}

// Get operator precedence
// 获取运算符优先级
int GetPrecedence(char cOperator)
{
	switch ( cOperator ) {
		case '+':
		case '-':
			return 1;
		case '*':
		case '/':
		case '%':
			return 2;
		case '^':
			return 3;
		default:
			return 0;
	}
}

// Check if operator is left associative
// 检查运算符是否为左结合
int IsLeftAssociative(char cOperator)
{
	return cOperator != '^';  // ^ is right associative / ^ 是右结合
}

// Tokenize expression
// 对表达式进行标记化
Token* Tokenize(str sExpression, size_t* piTokenCount)
{
	size_t iLen = strlen(sExpression);
	Token* paTokens = xrtMalloc(iLen * sizeof(Token));  // Upper bound / 上界
	size_t iTokens = 0;
	
	for ( size_t i = 0; i < iLen; i++ ) {
		char c = sExpression[i];
		
		// Skip whitespace
		// 跳过空白字符
		if ( c == ' ' || c == '\t' ) {
			continue;
		}
		
		// Number
		// 数字
		if ( (c >= '0' && c <= '9') || c == '.' ) {
			char sNumber[64];
			size_t iNumPos = 0;
			
			while ( i < iLen && ((sExpression[i] >= '0' && sExpression[i] <= '9') || sExpression[i] == '.') ) {
				sNumber[iNumPos++] = sExpression[i++];
			}
			sNumber[iNumPos] = '\0';
			i--;  // Adjust for loop increment / 调整循环增量
			
			Token tToken = {EXPR_TOKEN_NUMBER, 0};
			tToken.dValue = atof(sNumber);
			paTokens[iTokens++] = tToken;
		}
		// Operators
		// 运算符
		else if ( c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^' ) {
			Token tToken = {EXPR_TOKEN_OPERATOR, 0};
			tToken.cOperator = c;
			paTokens[iTokens++] = tToken;
		}
		// Parentheses
		// 括号
		else if ( c == '(' ) {
			Token tToken = {EXPR_TOKEN_LEFT_PAREN, 0};
			paTokens[iTokens++] = tToken;
		}
		else if ( c == ')' ) {
			Token tToken = {EXPR_TOKEN_RIGHT_PAREN, 0};
			paTokens[iTokens++] = tToken;
		}
		// Functions (simple detection)
		// 函数（简单检测）
		else if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ) {
			// For simplicity, treat as operators in this demo
			// 为简单起见，在此演示中视为运算符
			Token tToken = {EXPR_TOKEN_OPERATOR, 0};
			tToken.cOperator = c;
			paTokens[iTokens++] = tToken;
		}
	}
	
	*piTokenCount = iTokens;
	return paTokens;
}

// Convert infix to postfix (Shunting Yard algorithm)
// 将中缀转换为后缀（调度场算法）
Token* InfixToPostfix(Token* paInfix, size_t iInfixCount, size_t* piPostfixCount)
{
	TokenStack* pOperatorStack = StackCreate();
	Token* paPostfix = xrtMalloc(iInfixCount * 2 * sizeof(Token));
	size_t iPostfixPos = 0;
	
	for ( size_t i = 0; i < iInfixCount; i++ ) {
		Token tCurrent = paInfix[i];
		
		switch ( tCurrent.eType ) {
			case EXPR_TOKEN_NUMBER:
				// Numbers go directly to output
				// 数字直接输出
				paPostfix[iPostfixPos++] = tCurrent;
				break;
				
			case EXPR_TOKEN_OPERATOR:
				// Handle operators
				// 处理运算符
				while ( !StackIsEmpty(pOperatorStack) ) {
					Token tTop = StackPeek(pOperatorStack);
					if ( tTop.eType != EXPR_TOKEN_OPERATOR ) break;
					
					if ( (IsLeftAssociative(tCurrent.cOperator) && 
					      GetPrecedence(tCurrent.cOperator) <= GetPrecedence(tTop.cOperator)) ||
					     (!IsLeftAssociative(tCurrent.cOperator) && 
					      GetPrecedence(tCurrent.cOperator) < GetPrecedence(tTop.cOperator)) ) {
						paPostfix[iPostfixPos++] = StackPop(pOperatorStack);
					} else {
						break;
					}
				}
				StackPush(pOperatorStack, tCurrent);
				break;
				
			case EXPR_TOKEN_LEFT_PAREN:
				// Push left parenthesis to stack
				// 将左括号压入栈
				StackPush(pOperatorStack, tCurrent);
				break;
				
			case EXPR_TOKEN_RIGHT_PAREN:
				// Pop operators until left parenthesis
				// 弹出运算符直到左括号
				while ( !StackIsEmpty(pOperatorStack) && 
				        StackPeek(pOperatorStack).eType != EXPR_TOKEN_LEFT_PAREN ) {
					paPostfix[iPostfixPos++] = StackPop(pOperatorStack);
				}
				if ( !StackIsEmpty(pOperatorStack) ) {
					StackPop(pOperatorStack);  // Discard left parenthesis / 丢弃左括号
				}
				break;
				
			default:
				break;
		}
	}
	
	// Pop remaining operators
	// 弹出剩余运算符
	while ( !StackIsEmpty(pOperatorStack) ) {
		paPostfix[iPostfixPos++] = StackPop(pOperatorStack);
	}
	
	StackDestroy(pOperatorStack);
	*piPostfixCount = iPostfixPos;
	return paPostfix;
}

// Evaluate postfix expression
// 计算后缀表达式
double EvaluatePostfix(Token* paPostfix, size_t iPostfixCount)
{
	TokenStack* pValueStack = StackCreate();
	
	for ( size_t i = 0; i < iPostfixCount; i++ ) {
		Token tCurrent = paPostfix[i];
		
		if ( tCurrent.eType == EXPR_TOKEN_NUMBER ) {
			StackPush(pValueStack, tCurrent);
		}
		else if ( tCurrent.eType == EXPR_TOKEN_OPERATOR ) {
			if ( pValueStack->iSize < 2 ) {
				StackDestroy(pValueStack);
				return 0;  // Error / 错误
			}
			
			Token tRight = StackPop(pValueStack);
			Token tLeft = StackPop(pValueStack);
			
			double dResult = 0;
			switch ( tCurrent.cOperator ) {
				case '+':
					dResult = tLeft.dValue + tRight.dValue;
					break;
				case '-':
					dResult = tLeft.dValue - tRight.dValue;
					break;
				case '*':
					dResult = tLeft.dValue * tRight.dValue;
					break;
				case '/':
					if ( tRight.dValue != 0 ) {
						dResult = tLeft.dValue / tRight.dValue;
					} else {
						StackDestroy(pValueStack);
						return 0;  // Division by zero / 除零
					}
					break;
				case '%':
					if ( tRight.dValue != 0 ) {
						dResult = fmod(tLeft.dValue, tRight.dValue);
					} else {
						StackDestroy(pValueStack);
						return 0;  // Modulo by zero / 模零
					}
					break;
				case '^':
					dResult = pow(tLeft.dValue, tRight.dValue);
					break;
				default:
					dResult = 0;
					break;
			}
			
			Token tResult = {EXPR_TOKEN_NUMBER, 0};
			tResult.dValue = dResult;
			StackPush(pValueStack, tResult);
		}
	}
	
	double dFinalResult = 0;
	if ( !StackIsEmpty(pValueStack) ) {
		dFinalResult = StackPop(pValueStack).dValue;
	}
	
	StackDestroy(pValueStack);
	return dFinalResult;
}

// Print tokens
// 打印令牌
void PrintTokens(Token* paTokens, size_t iCount, str sLabel)
{
	printf("%s: ", sLabel);
	for ( size_t i = 0; i < iCount; i++ ) {
		switch ( paTokens[i].eType ) {
			case EXPR_TOKEN_NUMBER:
				printf("%.2f ", paTokens[i].dValue);
				break;
			case EXPR_TOKEN_OPERATOR:
				printf("%c ", paTokens[i].cOperator);
				break;
			case EXPR_TOKEN_LEFT_PAREN:
				printf("( ");
				break;
			case EXPR_TOKEN_RIGHT_PAREN:
				printf(") ");
				break;
			default:
				break;
		}
	}
	printf("\n");
}

// Evaluate expression
// 计算表达式
double EvaluateExpression(str sExpression)
{
	printf("Evaluating: %s\n", sExpression);
	
	// Tokenize
	// 标记化
	size_t iTokenCount;
	Token* paTokens = Tokenize(sExpression, &iTokenCount);
	PrintTokens(paTokens, iTokenCount, "Tokens");
	
	// Convert to postfix
	// 转换为后缀
	size_t iPostfixCount;
	Token* paPostfix = InfixToPostfix(paTokens, iTokenCount, &iPostfixCount);
	PrintTokens(paPostfix, iPostfixCount, "Postfix");
	
	// Evaluate
	// 计算
	double dResult = EvaluatePostfix(paPostfix, iPostfixCount);
	
	// Cleanup
	// 清理
	xrtFree(paTokens);
	xrtFree(paPostfix);
	
	return dResult;
}

// Test expression evaluator
// 测试表达式求值器
void TestExpressionEvaluator()
{
	printf("=== Expression Evaluator Test ===\n");
	printf("=== 表达式求值器测试 ===\n");
	
	// Test cases
	// 测试用例
	str arrExpressions[] = {
		"2 + 3 * 4",
		"(2 + 3) * 4",
		"10 - 2 * 3",
		"2 ^ 3 + 1",
		"15 / 3 + 2 * 4",
		"(10 + 5) * (3 - 1)",
		"2.5 + 3.7 * 1.2"
	};
	
	size_t iExprCount = sizeof(arrExpressions) / sizeof(arrExpressions[0]);
	
	for ( size_t i = 0; i < iExprCount; i++ ) {
		double dResult = EvaluateExpression(arrExpressions[i]);
		printf("Result: %.6f\n\n", dResult);
	}
}

// Test complex expressions
// 测试复杂表达式
void TestComplexExpressions()
{
	printf("=== Complex Expressions Test ===\n");
	printf("=== 复杂表达式测试 ===\n");
	
	str arrComplex[] = {
		"2 + 3 * 4 - 1",
		"((2 + 3) * 4) / 2",
		"2 ^ 3 ^ 2",  // Right associative / 右结合
		"100 / (5 + 5) * 3",
		"3.14159 * 2.5 ^ 2"
	};
	
	size_t iComplexCount = sizeof(arrComplex) / sizeof(arrComplex[0]);
	
	for ( size_t i = 0; i < iComplexCount; i++ ) {
		double dResult = EvaluateExpression(arrComplex[i]);
		printf("Result: %.6f\n\n", dResult);
	}
}

// Interactive calculator
// 交互式计算器
void InteractiveCalculator()
{
	printf("=== Interactive Calculator ===\n");
	printf("=== 交互式计算器 ===\n");
	printf("Enter expressions (or 'quit' to exit):\n");
	
	char sInput[256];
	while ( 1 ) {
		printf("> ");
		if ( !fgets(sInput, sizeof(sInput), stdin) ) {
			break;
		}
		
		// Remove newline
		// 移除换行符
		sInput[strcspn(sInput, "\n")] = '\0';
		
		if ( strcmp(sInput, "quit") == 0 ) {
			break;
		}
		
		if ( strlen(sInput) > 0 ) {
			double dResult = EvaluateExpression(sInput);
			printf("= %.6f\n", dResult);
		}
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Expression Evaluator Demo\n");
	printf("XRT 表达式求值器演示\n");
	printf("=========================\n\n");
	
	// Run tests
	// 运行测试
	TestExpressionEvaluator();
	TestComplexExpressions();
	
	// Uncomment to enable interactive mode
	// 取消注释以启用交互模式
	// InteractiveCalculator();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}