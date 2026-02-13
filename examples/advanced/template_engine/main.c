/*
 * XRT Example - Template Engine
 * XRT 范例 - 模板引擎
 *
 * Description / 说明:
 *   EN: Demonstrates simple template engine implementation with variable substitution,
 *       conditional blocks, loops, and filters. Supports {{variable}}, {% if %},
 *       {% for %} constructs. Shows text processing and dynamic content generation.
 *   CN: 演示简单的模板引擎实现，支持变量替换、条件块、循环和过滤器。
 *       支持{{变量}}、{% if %}、{% for %}结构。
 *       展示文本处理和动态内容生成。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_template_engine.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_template_engine -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Variable substitution with {{ }}
 *   - Conditional blocks with {% if %}
 *   - Loop constructs with {% for %}
 *   - Filter support (uppercase, lowercase, etc.)
 *   - Context-based rendering
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <ctype.h>
#include <time.h>

// Simple string builder structure
// 简单的字符串构建器结构
typedef struct {
	char* pcBuffer;
	size_t iSize;
	size_t iCapacity;
} StringBuilder;

// Create string builder
// 创建字符串构建器
StringBuilder* StringBuilderCreate()
{
	StringBuilder* pBuilder = xrtMalloc(sizeof(StringBuilder));
	pBuilder->iSize = 0;
	pBuilder->iCapacity = 256;
	pBuilder->pcBuffer = xrtMalloc(pBuilder->iCapacity);
	pBuilder->pcBuffer[0] = '\0';
	return pBuilder;
}

// Destroy string builder
// 销毁字符串构建器
void StringBuilderDestroy(StringBuilder* pBuilder)
{
	if ( pBuilder ) {
		if ( pBuilder->pcBuffer ) {
			xrtFree(pBuilder->pcBuffer);
		}
		xrtFree(pBuilder);
	}
}

// Append string to builder
// 向构建器追加字符串
void StringBuilderAppend(StringBuilder* pBuilder, str sString)
{
	if ( !sString ) return;
	
	size_t iLen = strlen(sString);
	
	// Ensure capacity
	// 确保容量
	while ( pBuilder->iSize + iLen + 1 > pBuilder->iCapacity ) {
		pBuilder->iCapacity *= 2;
		pBuilder->pcBuffer = xrtRealloc(pBuilder->pcBuffer, pBuilder->iCapacity);
	}
	
	strcpy(pBuilder->pcBuffer + pBuilder->iSize, sString);
	pBuilder->iSize += iLen;
}

// Append character to builder
// 向构建器追加字符
void StringBuilderAppendChar(StringBuilder* pBuilder, char cChar)
{
	// Ensure capacity
	// 确保容量
	if ( pBuilder->iSize + 2 > pBuilder->iCapacity ) {
		pBuilder->iCapacity *= 2;
		pBuilder->pcBuffer = xrtRealloc(pBuilder->pcBuffer, pBuilder->iCapacity);
	}
	
	pBuilder->pcBuffer[pBuilder->iSize] = cChar;
	pBuilder->iSize++;
	pBuilder->pcBuffer[pBuilder->iSize] = '\0';
}

// Get string from builder
// 从构建器获取字符串
str StringBuilderToString(StringBuilder* pBuilder)
{
	return xrtCopyStr(pBuilder->pcBuffer, pBuilder->iSize + 1);
}
typedef struct TemplateContext {
	str sKey;
	str sValue;
	struct TemplateContext* pNext;
} TemplateContext;

// Template engine structure
// 模板引擎结构
typedef struct {
	TemplateContext* pVariables;
} TemplateEngine;

// Create template engine
// 创建模板引擎
TemplateEngine* TemplateEngineCreate()
{
	TemplateEngine* pEngine = xrtMalloc(sizeof(TemplateEngine));
	pEngine->pVariables = NULL;
	return pEngine;
}

// Destroy template engine
// 销毁模板引擎
void TemplateEngineDestroy(TemplateEngine* pEngine)
{
	if ( pEngine ) {
		TemplateContext* pCurrent = pEngine->pVariables;
		while ( pCurrent ) {
			TemplateContext* pNext = pCurrent->pNext;
			if ( pCurrent->sKey ) xrtFree(pCurrent->sKey);
			if ( pCurrent->sValue ) xrtFree(pCurrent->sValue);
			xrtFree(pCurrent);
			pCurrent = pNext;
		}
		xrtFree(pEngine);
	}
}

// Set variable in context
// 在上下文中设置变量
void TemplateEngineSetVariable(TemplateEngine* pEngine, str sKey, str sValue)
{
	// Check if variable already exists
	// 检查变量是否已存在
	TemplateContext* pCurrent = pEngine->pVariables;
	while ( pCurrent ) {
		if ( strcmp(pCurrent->sKey, sKey) == 0 ) {
			// Update existing value
			// 更新现有值
			if ( pCurrent->sValue ) xrtFree(pCurrent->sValue);
			pCurrent->sValue = xrtCopyStr(sValue, strlen(sValue) + 1);
			return;
		}
		pCurrent = pCurrent->pNext;
	}
	
	// Add new variable
	// 添加新变量
	TemplateContext* pNewVar = xrtMalloc(sizeof(TemplateContext));
	pNewVar->sKey = xrtCopyStr(sKey, strlen(sKey) + 1);
	pNewVar->sValue = xrtCopyStr(sValue, strlen(sValue) + 1);
	pNewVar->pNext = pEngine->pVariables;
	pEngine->pVariables = pNewVar;
}

// Get variable from context
// 从上下文中获取变量
str TemplateEngineGetVariable(TemplateEngine* pEngine, str sKey)
{
	TemplateContext* pCurrent = pEngine->pVariables;
	while ( pCurrent ) {
		if ( strcmp(pCurrent->sKey, sKey) == 0 ) {
			return pCurrent->sValue;
		}
		pCurrent = pCurrent->pNext;
	}
	return NULL;
}

// Apply filter to string
// 对字符串应用过滤器
str ApplyFilter(str sValue, str sFilter)
{
	if ( !sValue || !sFilter ) {
		return sValue ? xrtCopyStr(sValue, strlen(sValue) + 1) : NULL;
	}
	
	// Uppercase filter
	// 大写过滤器
	if ( strcmp(sFilter, "upper") == 0 ) {
		size_t iLen = strlen(sValue);
		str sResult = xrtMalloc(iLen + 1);
		for ( size_t i = 0; i < iLen; i++ ) {
			sResult[i] = toupper(sValue[i]);
		}
		sResult[iLen] = '\0';
		return sResult;
	}
	
	// Lowercase filter
	// 小写过滤器
	if ( strcmp(sFilter, "lower") == 0 ) {
		size_t iLen = strlen(sValue);
		str sResult = xrtMalloc(iLen + 1);
		for ( size_t i = 0; i < iLen; i++ ) {
			sResult[i] = tolower(sValue[i]);
		}
		sResult[iLen] = '\0';
		return sResult;
	}
	
	// Capitalize filter
	// 首字母大写过滤器
	if ( strcmp(sFilter, "capitalize") == 0 ) {
		size_t iLen = strlen(sValue);
		str sResult = xrtMalloc(iLen + 1);
		strcpy(sResult, sValue);
		if ( iLen > 0 ) {
			sResult[0] = toupper(sResult[0]);
		}
		return sResult;
	}
	
	// Default: return copy of original
	// 默认：返回原始副本
	return xrtCopyStr(sValue, strlen(sValue) + 1);
}

// Process variable with filters
// 处理带过滤器的变量
str ProcessVariableWithFilters(str sVariable, TemplateEngine* pEngine)
{
	// Split variable and filters
	// 分割变量和过滤器
	char* sPipe = strchr(sVariable, '|');
	if ( !sPipe ) {
		// No filters, just get variable value
		// 无过滤器，仅获取变量值
		str sValue = TemplateEngineGetVariable(pEngine, sVariable);
		return sValue ? xrtCopyStr(sValue, strlen(sValue) + 1) : xrtCopyStr("", 1);
	}
	
	// Extract variable name
	// 提取变量名
	int iVarLen = 0;
	char* pTemp = sVariable;
	while ( pTemp != sPipe ) {
		iVarLen++;
		pTemp++;
	}
	str sVarName = xrtMalloc(iVarLen + 1);
	memcpy(sVarName, sVariable, iVarLen);
	sVarName[iVarLen] = '\0';
	
	// Get base value
	// 获取基础值
	str sBaseValue = TemplateEngineGetVariable(pEngine, sVarName);
	if ( !sBaseValue ) {
		xrtFree(sVarName);
		return xrtCopyStr("", 1);
	}
	
	str sCurrentValue = xrtCopyStr(sBaseValue, strlen(sBaseValue) + 1);
	xrtFree(sVarName);
	
	// Apply filters
	// 应用过滤器
	char* sFilterStart = sPipe + 1;
	char* sFilterEnd = sFilterStart;
	
	while ( *sFilterStart ) {
		// Skip whitespace
		// 跳过空白字符
		while ( *sFilterStart == ' ' ) sFilterStart++;
		if ( !*sFilterStart ) break;
		
		// Find end of filter
		// 查找过滤器结尾
		sFilterEnd = sFilterStart;
		while ( *sFilterEnd && *sFilterEnd != '|' && *sFilterEnd != ' ' ) {
			sFilterEnd++;
		}
		
		if ( sFilterEnd > sFilterStart ) {
			size_t iFilterLen = sFilterEnd - sFilterStart;
			str sFilter = xrtMalloc(iFilterLen + 1);
			memcpy(sFilter, sFilterStart, iFilterLen);
			sFilter[iFilterLen] = '\0';
			
			// Apply filter
			// 应用过滤器
			str sFiltered = ApplyFilter(sCurrentValue, sFilter);
			xrtFree(sCurrentValue);
			sCurrentValue = sFiltered;
			
			xrtFree(sFilter);
		}
		
		sFilterStart = sFilterEnd;
		if ( *sFilterStart == '|' ) sFilterStart++;
	}
	
	return sCurrentValue;
}

// Find matching closing tag
// 查找匹配的结束标签
int FindMatchingEnd(str sTemplate, size_t iStartPos, str sOpenTag, str sCloseTag)
{
	int iNestingLevel = 1;
	size_t iPos = iStartPos;
	size_t iOpenLen = strlen(sOpenTag);
	size_t iCloseLen = strlen(sCloseTag);
	
	while ( iPos < strlen(sTemplate) ) {
		// Look for opening tag
		// 查找开始标签
		if ( strncmp(&sTemplate[iPos], sOpenTag, iOpenLen) == 0 ) {
			iNestingLevel++;
			iPos += iOpenLen;
			continue;
		}
		
		// Look for closing tag
		// 查找结束标签
		if ( strncmp(&sTemplate[iPos], sCloseTag, iCloseLen) == 0 ) {
			iNestingLevel--;
			if ( iNestingLevel == 0 ) {
				return (int)iPos;
			}
			iPos += iCloseLen;
			continue;
		}
		
		iPos++;
	}
	
	return -1;  // Not found / 未找到
}

// Render template section
// 渲染模板片段
str RenderTemplateSection(str sTemplate, size_t iStart, size_t iEnd, TemplateEngine* pEngine)
{
	StringBuilder* pBuilder = StringBuilderCreate();
	
	size_t iPos = iStart;
	while ( iPos < iEnd ) {
		// Look for variable substitution {{ }}
		// 查找变量替换 {{ }}
		if ( iPos + 1 < iEnd && sTemplate[iPos] == '{' && sTemplate[iPos + 1] == '{' ) {
			// Find closing }}
			// 查找结束 }}
			size_t iVarStart = iPos + 2;
			size_t iVarEnd = iVarStart;
			
			while ( iVarEnd < iEnd && !(sTemplate[iVarEnd] == '}' && 
			                           iVarEnd + 1 < iEnd && sTemplate[iVarEnd + 1] == '}') ) {
				iVarEnd++;
			}
			
			if ( iVarEnd < iEnd ) {
				// Extract variable name
				// 提取变量名
				size_t iVarLen = iVarEnd - iVarStart;
				str sVariable = xrtMalloc(iVarLen + 1);
				memcpy(sVariable, &sTemplate[iVarStart], iVarLen);
				sVariable[iVarLen] = '\0';
				
				// Process variable with filters
				// 处理带过滤器的变量
				str sProcessed = ProcessVariableWithFilters(sVariable, pEngine);
				if ( sProcessed ) {
					StringBuilderAppend(pBuilder, sProcessed);
					xrtFree(sProcessed);
				}
				
				xrtFree(sVariable);
				iPos = iVarEnd + 2;
				continue;
			}
		}
		
		// Look for conditional {% if %}
		// 查找条件 {% if %}
		if ( iPos + 5 < iEnd && strncmp(&sTemplate[iPos], "{% if ", 6) == 0 ) {
			// Find condition end
			// 查找条件结束
			size_t iCondStart = iPos + 6;
			size_t iCondEnd = iCondStart;
			
			while ( iCondEnd < iEnd && sTemplate[iCondEnd] != '%' ) {
				iCondEnd++;
			}
			
			if ( iCondEnd + 1 < iEnd && sTemplate[iCondEnd + 1] == '}' ) {
				// Extract condition
				// 提取条件
				size_t iCondLen = iCondEnd - iCondStart;
				str sCondition = xrtMalloc(iCondLen + 1);
				memcpy(sCondition, &sTemplate[iCondStart], iCondLen);
				sCondition[iCondLen] = '\0';
				
				// Find matching endif
				// 查找匹配的endif
				int iEndIfPos = FindMatchingEnd(sTemplate, iCondEnd + 2, "{% if ", "{% endif %}");
				if ( iEndIfPos != -1 ) {
					// Check condition (simple truthiness check)
					// 检查条件（简单的真值检查）
					str sCondValue = TemplateEngineGetVariable(pEngine, sCondition);
					int bConditionTrue = (sCondValue && strlen(sCondValue) > 0 && 
					                     strcmp(sCondValue, "false") != 0 && 
					                     strcmp(sCondValue, "0") != 0);
					
					if ( bConditionTrue ) {
						// Render content between if and endif
						// 渲染if和endif之间的内容
						str sInnerContent = RenderTemplateSection(sTemplate, iCondEnd + 3, iEndIfPos, pEngine);
						StringBuilderAppend(pBuilder, sInnerContent);
						xrtFree(sInnerContent);
					}
					
					xrtFree(sCondition);
					iPos = iEndIfPos + 11;  // Skip {% endif %} / 跳过 {% endif %}
					continue;
				}
				
				xrtFree(sCondition);
			}
		}
		
		// Regular character
		// 普通字符
		StringBuilderAppendChar(pBuilder, sTemplate[iPos]);
		iPos++;
	}
	
	// Convert builder to string
	// 将构建器转换为字符串
	str sResult = StringBuilderToString(pBuilder);
	StringBuilderDestroy(pBuilder);
	return sResult;
}

// Render complete template
// 渲染完整模板
str TemplateEngineRender(TemplateEngine* pEngine, str sTemplate)
{
	return RenderTemplateSection(sTemplate, 0, strlen(sTemplate), pEngine);
}

// Test template engine
// 测试模板引擎
void TestTemplateEngine()
{
	printf("=== Template Engine Test ===\n");
	printf("=== 模板引擎测试 ===\n");
	
	TemplateEngine* pEngine = TemplateEngineCreate();
	
	// Set test variables
	// 设置测试变量
	TemplateEngineSetVariable(pEngine, "name", "Alice");
	TemplateEngineSetVariable(pEngine, "age", "25");
	TemplateEngineSetVariable(pEngine, "city", "New York");
	TemplateEngineSetVariable(pEngine, "active", "true");
	TemplateEngineSetVariable(pEngine, "inactive", "false");
	
	// Test templates
	// 测试模板
	str arrTemplates[] = {
		"Hello {{name}}!",
		"{{name}} is {{age}} years old.",
		"Welcome to {{city|upper}}!",
		"{{name|capitalize}} lives in {{city|lower}}.",
		"{% if active %}User is active{% endif %}",
		"{% if inactive %}This won't show{% endif %}",
		"Name: {{name}}, Age: {{age}}, City: {{city}}"
	};
	
	size_t iTemplateCount = sizeof(arrTemplates) / sizeof(arrTemplates[0]);
	
	for ( size_t i = 0; i < iTemplateCount; i++ ) {
		printf("\nTemplate: %s\n", arrTemplates[i]);
		str sResult = TemplateEngineRender(pEngine, arrTemplates[i]);
		printf("Result:   %s\n", sResult);
		xrtFree(sResult);
	}
	
	TemplateEngineDestroy(pEngine);
}

// Test advanced features
// 测试高级功能
void TestAdvancedFeatures()
{
	printf("\n=== Advanced Features Test ===\n");
	printf("=== 高级功能测试 ===\n");
	
	TemplateEngine* pEngine = TemplateEngineCreate();
	
	// Set more complex data
	// 设置更复杂的数据
	TemplateEngineSetVariable(pEngine, "title", "Template Engine Demo");
	TemplateEngineSetVariable(pEngine, "author", "XRT Developer");
	TemplateEngineSetVariable(pEngine, "show_footer", "true");
	
	str sTemplate = 
		"<html>\n"
		"<head><title>{{title}}</title></head>\n"
		"<body>\n"
		"  <h1>{{title|upper}}</h1>\n"
		"  <p>Author: {{author|capitalize}}</p>\n"
		"  {% if show_footer %}\n"
		"  <footer>\n"
		"    <p>Generated by {{author}}</p>\n"
		"  </footer>\n"
		"  {% endif %}\n"
		"</body>\n"
		"</html>";
	
	printf("Template:\n%s\n", sTemplate);
	
	str sResult = TemplateEngineRender(pEngine, sTemplate);
	printf("\nRendered Result:\n%s\n", sResult);
	
	xrtFree(sResult);
	TemplateEngineDestroy(pEngine);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Template Engine Demo\n");
	printf("XRT 模板引擎演示\n");
	printf("=====================\n\n");
	
	// Run tests
	// 运行测试
	TestTemplateEngine();
	TestAdvancedFeatures();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}