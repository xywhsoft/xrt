/*
 * XRT Example - JSON Parser
 * XRT 范例 - JSON解析器
 *
 * Description / 说明:
 *   EN: Demonstrates recursive descent JSON parser implementation including
 *       lexical analysis, grammar parsing, and AST generation. Supports
 *       objects, arrays, strings, numbers, booleans, and null values.
 *       Shows proper error handling and validation.
 *   CN: 演示递归下降JSON解析器实现，包括词法分析、语法解析和AST生成。
 *       支持对象、数组、字符串、数字、布尔值和null值。
 *       展示正确的错误处理和验证。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_json_parser.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_json_parser -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Recursive descent parsing
 *   - JSON value types support
 *   - Error reporting and recovery
 *   - Memory management
 *   - AST generation and traversal
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <ctype.h>
#include <math.h>
#include <time.h>

// JSON value types
// JSON值类型
typedef enum {
	MY_JSON_NULL = 1,
	MY_JSON_BOOLEAN,
	MY_JSON_NUMBER,
	MY_JSON_STRING,
	MY_JSON_ARRAY,
	MY_JSON_OBJECT
} JsonValueType;

// Forward declarations
// 前向声明
typedef struct JsonValue JsonValue;
typedef struct JsonObjectEntry JsonObjectEntry;

// JSON array structure
// JSON数组结构
typedef struct {
	JsonValue** ppValues;
	size_t iSize;
	size_t iCapacity;
} JsonArray;

// JSON object entry structure
// JSON对象条目结构
struct JsonObjectEntry {
	str sKey;
	JsonValue* pValue;
	JsonObjectEntry* pNext;
};

// JSON object structure
// JSON对象结构
typedef struct {
	JsonObjectEntry** ppBuckets;
	size_t iBucketCount;
	size_t iSize;
} JsonObject;

// JSON value structure
// JSON值结构
struct JsonValue {
	JsonValueType eType;
	union {
		int bBoolean;		// For boolean / 用于布尔值
		double dNumber;		// For number / 用于数字
		str sString;		// For string / 用于字符串
		JsonArray* pArray;	// For array / 用于数组
		JsonObject* pObject;// For object / 用于对象
	};
};

// JSON parser structure
// JSON解析器结构
typedef struct {
	str sJsonText;
	size_t iPosition;
	size_t iLength;
	str sErrorMessage;
	size_t iErrorPosition;
} JsonParser;

// Create JSON value
// 创建JSON值
JsonValue* JsonValueCreate(JsonValueType eType)
{
	JsonValue* pValue = xrtMalloc(sizeof(JsonValue));
	pValue->eType = eType;
	switch ( eType ) {
		case MY_JSON_NULL:
		case MY_JSON_BOOLEAN:
		case MY_JSON_NUMBER:
			break;
		case MY_JSON_STRING:
			pValue->sString = NULL;
			break;
		case MY_JSON_ARRAY:
			pValue->pArray = NULL;
			break;
		case MY_JSON_OBJECT:
			pValue->pObject = NULL;
			break;
	}
	return pValue;
}

// Destroy JSON value
// 销毁JSON值
void JsonValueDestroy(JsonValue* pValue);

// Destroy JSON array
// 销毁JSON数组
void JsonArrayDestroy(JsonArray* pArray)
{
	if ( pArray ) {
		for ( size_t i = 0; i < pArray->iSize; i++ ) {
			JsonValueDestroy(pArray->ppValues[i]);
		}
		if ( pArray->ppValues ) {
			xrtFree(pArray->ppValues);
		}
		xrtFree(pArray);
	}
}

// Destroy JSON object entry
// 销毁JSON对象条目
void JsonObjectEntryDestroy(JsonObjectEntry* pEntry)
{
	if ( pEntry ) {
		if ( pEntry->sKey ) xrtFree(pEntry->sKey);
		if ( pEntry->pValue ) JsonValueDestroy(pEntry->pValue);
		JsonObjectEntryDestroy(pEntry->pNext);
		xrtFree(pEntry);
	}
}

// Destroy JSON object
// 销毁JSON对象
void JsonObjectDestroy(JsonObject* pObject)
{
	if ( pObject ) {
		for ( size_t i = 0; i < pObject->iBucketCount; i++ ) {
			JsonObjectEntryDestroy(pObject->ppBuckets[i]);
		}
		if ( pObject->ppBuckets ) {
			xrtFree(pObject->ppBuckets);
		}
		xrtFree(pObject);
	}
}

// Destroy JSON value
// 销毁JSON值
void JsonValueDestroy(JsonValue* pValue)
{
	if ( pValue ) {
		switch ( pValue->eType ) {
			case MY_JSON_STRING:
				if ( pValue->sString ) xrtFree(pValue->sString);
				break;
			case MY_JSON_ARRAY:
				JsonArrayDestroy(pValue->pArray);
				break;
			case MY_JSON_OBJECT:
				JsonObjectDestroy(pValue->pObject);
				break;
			default:
				break;
		}
		xrtFree(pValue);
	}
}

// Create JSON array
// 创建JSON数组
JsonArray* JsonArrayCreate()
{
	JsonArray* pArray = xrtMalloc(sizeof(JsonArray));
	pArray->iSize = 0;
	pArray->iCapacity = 4;
	pArray->ppValues = xrtMalloc(pArray->iCapacity * sizeof(JsonValue*));
	return pArray;
}

// Add value to JSON array
// 向JSON数组添加值
void JsonArrayAdd(JsonArray* pArray, JsonValue* pValue)
{
	if ( pArray->iSize >= pArray->iCapacity ) {
		pArray->iCapacity *= 2;
		pArray->ppValues = xrtRealloc(pArray->ppValues, pArray->iCapacity * sizeof(JsonValue*));
	}
	
	pArray->ppValues[pArray->iSize] = pValue;
	pArray->iSize++;
}

// Create JSON object
// 创建JSON对象
JsonObject* JsonObjectCreate()
{
	JsonObject* pObject = xrtMalloc(sizeof(JsonObject));
	pObject->iBucketCount = 16;
	pObject->iSize = 0;
	pObject->ppBuckets = xrtMalloc(pObject->iBucketCount * sizeof(JsonObjectEntry*));
	
	for ( size_t i = 0; i < pObject->iBucketCount; i++ ) {
		pObject->ppBuckets[i] = NULL;
	}
	
	return pObject;
}

// Simple hash function for strings
// 字符串的简单哈希函数
size_t StringHash(str sString, size_t iTableSize)
{
	size_t iHash = 5381;
	int iChar;
	
	while ( (iChar = *sString++) ) {
		iHash = ((iHash << 5) + iHash) + iChar;
	}
	
	return iHash % iTableSize;
}

// Add key-value pair to JSON object
// 向JSON对象添加键值对
void JsonObjectAdd(JsonObject* pObject, str sKey, JsonValue* pValue)
{
	size_t iIndex = StringHash(sKey, pObject->iBucketCount);
	
	// Check if key already exists
	// 检查键是否已存在
	JsonObjectEntry* pEntry = pObject->ppBuckets[iIndex];
	while ( pEntry ) {
		if ( strcmp(pEntry->sKey, sKey) == 0 ) {
			// Replace existing value
			// 替换现有值
			JsonValueDestroy(pEntry->pValue);
			pEntry->pValue = pValue;
			return;
		}
		pEntry = pEntry->pNext;
	}
	
	// Add new entry
	// 添加新条目
	JsonObjectEntry* pNewEntry = xrtMalloc(sizeof(JsonObjectEntry));
	pNewEntry->sKey = xrtCopyStr(sKey, strlen(sKey) + 1);
	pNewEntry->pValue = pValue;
	pNewEntry->pNext = pObject->ppBuckets[iIndex];
	pObject->ppBuckets[iIndex] = pNewEntry;
	pObject->iSize++;
}

// Get value from JSON object
// 从JSON对象获取值
JsonValue* JsonObjectGet(JsonObject* pObject, str sKey)
{
	size_t iIndex = StringHash(sKey, pObject->iBucketCount);
	
	JsonObjectEntry* pEntry = pObject->ppBuckets[iIndex];
	while ( pEntry ) {
		if ( strcmp(pEntry->sKey, sKey) == 0 ) {
			return pEntry->pValue;
		}
		pEntry = pEntry->pNext;
	}
	
	return NULL;
}

// Create JSON parser
// 创建JSON解析器
JsonParser* JsonParserCreate(str sJsonText)
{
	JsonParser* pParser = xrtMalloc(sizeof(JsonParser));
	pParser->sJsonText = sJsonText;
	pParser->iPosition = 0;
	pParser->iLength = strlen(sJsonText);
	pParser->sErrorMessage = NULL;
	pParser->iErrorPosition = 0;
	return pParser;
}

// Destroy JSON parser
// 销毁JSON解析器
void JsonParserDestroy(JsonParser* pParser)
{
	if ( pParser ) {
		if ( pParser->sErrorMessage ) {
			xrtFree(pParser->sErrorMessage);
		}
		xrtFree(pParser);
	}
}

// Skip whitespace
// 跳过空白字符
void JsonSkipWhitespace(JsonParser* pParser)
{
	while ( pParser->iPosition < pParser->iLength ) {
		char c = pParser->sJsonText[pParser->iPosition];
		if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) {
			pParser->iPosition++;
		} else {
			break;
		}
	}
}

// Parse string
// 解析字符串
str JsonParseString(JsonParser* pParser)
{
	if ( pParser->iPosition >= pParser->iLength || 
	     pParser->sJsonText[pParser->iPosition] != '"' ) {
		return NULL;
	}
	
	pParser->iPosition++;  // Skip opening quote / 跳过开始引号
	
	size_t iStart = pParser->iPosition;
	
	// Find closing quote
	// 查找结束引号
	while ( pParser->iPosition < pParser->iLength ) {
		if ( pParser->sJsonText[pParser->iPosition] == '"' ) {
			break;
		}
		pParser->iPosition++;
	}
	
	if ( pParser->iPosition >= pParser->iLength ) {
		return NULL;  // Unclosed string / 未闭合的字符串
	}
	
	// Extract string content
	// 提取字符串内容
	size_t iLength = pParser->iPosition - iStart;
	str sResult = xrtMalloc(iLength + 1);
	memcpy(sResult, &pParser->sJsonText[iStart], iLength);
	sResult[iLength] = '\0';
	
	pParser->iPosition++;  // Skip closing quote / 跳过结束引号
	
	return sResult;
}

// Parse number
// 解析数字
double JsonParseNumber(JsonParser* pParser)
{
	size_t iStart = pParser->iPosition;
	
	// Parse number (simplified)
	// 解析数字（简化）
	while ( pParser->iPosition < pParser->iLength ) {
		char c = pParser->sJsonText[pParser->iPosition];
		if ( isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E' ) {
			pParser->iPosition++;
		} else {
			break;
		}
	}
	
	// Extract number substring
	// 提取数字子串
	size_t iLength = pParser->iPosition - iStart;
	char* sNumber = xrtMalloc(iLength + 1);
	memcpy(sNumber, &pParser->sJsonText[iStart], iLength);
	sNumber[iLength] = '\0';
	
	double dResult = atof(sNumber);
	xrtFree(sNumber);
	
	return dResult;
}

// Forward declaration
// 前向声明
JsonValue* JsonParseValue(JsonParser* pParser);

// Parse array
// 解析数组
JsonArray* JsonParseArray(JsonParser* pParser)
{
	if ( pParser->iPosition >= pParser->iLength || 
	     pParser->sJsonText[pParser->iPosition] != '[' ) {
		return NULL;
	}
	
	pParser->iPosition++;  // Skip opening bracket / 跳过开始括号
	JsonSkipWhitespace(pParser);
	
	JsonArray* pArray = JsonArrayCreate();
	
	// Check for empty array
	// 检查空数组
	if ( pParser->iPosition < pParser->iLength && 
	     pParser->sJsonText[pParser->iPosition] == ']' ) {
		pParser->iPosition++;  // Skip closing bracket / 跳过结束括号
		return pArray;
	}
	
	// Parse array elements
	// 解析数组元素
	while ( pParser->iPosition < pParser->iLength ) {
		JsonValue* pValue = JsonParseValue(pParser);
		if ( !pValue ) {
			JsonArrayDestroy(pArray);
			return NULL;
		}
		
		JsonArrayAdd(pArray, pValue);
		JsonSkipWhitespace(pParser);
		
		if ( pParser->iPosition >= pParser->iLength ) {
			JsonArrayDestroy(pArray);
			return NULL;
		}
		
		char c = pParser->sJsonText[pParser->iPosition];
		if ( c == ']' ) {
			pParser->iPosition++;  // Skip closing bracket / 跳过结束括号
			break;
		} else if ( c == ',' ) {
			pParser->iPosition++;  // Skip comma / 跳过逗号
			JsonSkipWhitespace(pParser);
		} else {
			JsonArrayDestroy(pArray);
			return NULL;
		}
	}
	
	return pArray;
}

// Parse object
// 解析对象
JsonObject* JsonParseObject(JsonParser* pParser)
{
	if ( pParser->iPosition >= pParser->iLength || 
	     pParser->sJsonText[pParser->iPosition] != '{' ) {
		return NULL;
	}
	
	pParser->iPosition++;  // Skip opening brace / 跳过开始大括号
	JsonSkipWhitespace(pParser);
	
	JsonObject* pObject = JsonObjectCreate();
	
	// Check for empty object
	// 检查空对象
	if ( pParser->iPosition < pParser->iLength && 
	     pParser->sJsonText[pParser->iPosition] == '}' ) {
		pParser->iPosition++;  // Skip closing brace / 跳过结束大括号
		return pObject;
	}
	
	// Parse object members
	// 解析对象成员
	while ( pParser->iPosition < pParser->iLength ) {
		// Parse key
		// 解析键
		str sKey = JsonParseString(pParser);
		if ( !sKey ) {
			JsonObjectDestroy(pObject);
			return NULL;
		}
		
		JsonSkipWhitespace(pParser);
		
		// Expect colon
		// 期望冒号
		if ( pParser->iPosition >= pParser->iLength || 
		     pParser->sJsonText[pParser->iPosition] != ':' ) {
			xrtFree(sKey);
			JsonObjectDestroy(pObject);
			return NULL;
		}
		
		pParser->iPosition++;  // Skip colon / 跳过冒号
		JsonSkipWhitespace(pParser);
		
		// Parse value
		// 解析值
		JsonValue* pValue = JsonParseValue(pParser);
		if ( !pValue ) {
			xrtFree(sKey);
			JsonObjectDestroy(pObject);
			return NULL;
		}
		
		JsonObjectAdd(pObject, sKey, pValue);
		xrtFree(sKey);
		
		JsonSkipWhitespace(pParser);
		
		if ( pParser->iPosition >= pParser->iLength ) {
			JsonObjectDestroy(pObject);
			return NULL;
		}
		
		char c = pParser->sJsonText[pParser->iPosition];
		if ( c == '}' ) {
			pParser->iPosition++;  // Skip closing brace / 跳过结束大括号
			break;
		} else if ( c == ',' ) {
			pParser->iPosition++;  // Skip comma / 跳过逗号
			JsonSkipWhitespace(pParser);
		} else {
			JsonObjectDestroy(pObject);
			return NULL;
		}
	}
	
	return pObject;
}

// Parse JSON value
// 解析JSON值
JsonValue* JsonParseValue(JsonParser* pParser)
{
	JsonSkipWhitespace(pParser);
	
	if ( pParser->iPosition >= pParser->iLength ) {
		return NULL;
	}
	
	char c = pParser->sJsonText[pParser->iPosition];
	
	// Null
	// 空值
	if ( c == 'n' ) {
		if ( pParser->iPosition + 3 < pParser->iLength &&
		     strncmp(&pParser->sJsonText[pParser->iPosition], "null", 4) == 0 ) {
			pParser->iPosition += 4;
			JsonValue* pValue = JsonValueCreate(MY_JSON_NULL);
			return pValue;
		}
		return NULL;
	}
			
	// Boolean true
	// 布尔真
	if ( c == 't' ) {
		if ( pParser->iPosition + 3 < pParser->iLength &&
		     strncmp(&pParser->sJsonText[pParser->iPosition], "true", 4) == 0 ) {
			pParser->iPosition += 4;
			JsonValue* pValue = JsonValueCreate(MY_JSON_BOOLEAN);
			pValue->bBoolean = 1;
			return pValue;
		}
		return NULL;
	}
			
	// Boolean false
	// 布尔假
	if ( c == 'f' ) {
		if ( pParser->iPosition + 4 < pParser->iLength &&
		     strncmp(&pParser->sJsonText[pParser->iPosition], "false", 5) == 0 ) {
			pParser->iPosition += 5;
			JsonValue* pValue = JsonValueCreate(MY_JSON_BOOLEAN);
			pValue->bBoolean = 0;
			return pValue;
		}
		return NULL;
	}
			
	// String
	// 字符串
	if ( c == '"' ) {
		str sString = JsonParseString(pParser);
		if ( sString ) {
			JsonValue* pValue = JsonValueCreate(MY_JSON_STRING);
			pValue->sString = sString;
			return pValue;
		}
		return NULL;
	}
			
	// Number
	// 数字
	if ( isdigit(c) || c == '-' ) {
		double dNumber = JsonParseNumber(pParser);
		JsonValue* pValue = JsonValueCreate(MY_JSON_NUMBER);
		pValue->dNumber = dNumber;
		return pValue;
	}
			
	// Array
	// 数组
	if ( c == '[' ) {
		JsonArray* pArray = JsonParseArray(pParser);
		if ( pArray ) {
			JsonValue* pValue = JsonValueCreate(MY_JSON_ARRAY);
			pValue->pArray = pArray;
			return pValue;
		}
		return NULL;
	}
			
	// Object
	// 对象
	if ( c == '{' ) {
		JsonObject* pObject = JsonParseObject(pParser);
		if ( pObject ) {
			JsonValue* pValue = JsonValueCreate(MY_JSON_OBJECT);
			pValue->pObject = pObject;
			return pValue;
		}
		return NULL;
	}
	
	return NULL;
}

// Pretty print JSON value
// 美化打印JSON值
void JsonPrintValue(JsonValue* pValue, int iIndent)
{
	if ( !pValue ) {
		printf("null");
		return;
	}
	
	switch ( pValue->eType ) {
		case MY_JSON_NULL:
			printf("null");
			break;
			
		case MY_JSON_BOOLEAN:
			printf("%s", pValue->bBoolean ? "true" : "false");
			break;
			
		case MY_JSON_NUMBER:
			printf("%.6g", pValue->dNumber);
			break;
			
		case MY_JSON_STRING:
			printf("\"%s\"", pValue->sString ? pValue->sString : "");
			break;
			
		case MY_JSON_ARRAY:
			printf("[\n");
			for ( size_t i = 0; i < pValue->pArray->iSize; i++ ) {
				for ( int j = 0; j < iIndent + 2; j++ ) printf(" ");
				JsonPrintValue(pValue->pArray->ppValues[i], iIndent + 2);
				if ( i < pValue->pArray->iSize - 1 ) {
					printf(",");
				}
				printf("\n");
			}
			for ( int j = 0; j < iIndent; j++ ) printf(" ");
			printf("]");
			break;
			
		case MY_JSON_OBJECT:
			printf("{\n");
			for ( size_t i = 0; i < pValue->pObject->iBucketCount; i++ ) {
				JsonObjectEntry* pEntry = pValue->pObject->ppBuckets[i];
				while ( pEntry ) {
					for ( int j = 0; j < iIndent + 2; j++ ) printf(" ");
					printf("\"%s\": ", pEntry->sKey);
					JsonPrintValue(pEntry->pValue, iIndent + 2);
					pEntry = pEntry->pNext;
					if ( pEntry ) {
						printf(",");
					}
					printf("\n");
				}
			}
			for ( int j = 0; j < iIndent; j++ ) printf(" ");
			printf("}");
			break;
	}
}

// Test JSON parser
// 测试JSON解析器
void TestJsonParser()
{
	printf("=== JSON Parser Test ===\n");
	printf("=== JSON解析器测试 ===\n");
	
	// Test JSON strings
	// 测试JSON字符串
	str arrJsonTests[] = {
		// Simple values
		// 简单值
		"null",
		"true",
		"false",
		"\"hello world\"",
		"123.45",
		"-42",
		
		// Arrays
		// 数组
		"[1, 2, 3]",
		"[\"a\", \"b\", \"c\"]",
		"[1, true, \"mixed\", null]",
		
		// Objects
		// 对象
		"{\"name\": \"John\", \"age\": 30}",
		"{\"person\": {\"name\": \"Alice\", \"active\": true}}",
		
		// Complex nested structure
		// 复杂嵌套结构
		"{\"users\": [{\"id\": 1, \"name\": \"Alice\"}, {\"id\": 2, \"name\": \"Bob\"}], \"count\": 2}"
	};
	
	size_t iTestCount = sizeof(arrJsonTests) / sizeof(arrJsonTests[0]);
	
	for ( size_t i = 0; i < iTestCount; i++ ) {
		printf("\nParsing: %s\n", arrJsonTests[i]);
		
		JsonParser* pParser = JsonParserCreate(arrJsonTests[i]);
		JsonValue* pValue = JsonParseValue(pParser);
		
		if ( pValue ) {
			printf("Parsed successfully:\n");
			JsonPrintValue(pValue, 0);
			printf("\n");
			JsonValueDestroy(pValue);
		} else {
			printf("Parse failed\n");
		}
		
		JsonParserDestroy(pParser);
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT JSON Parser Demo\n");
	printf("XRT JSON解析器演示\n");
	printf("==================\n\n");
	
	// Run tests
	// 运行测试
	TestJsonParser();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}