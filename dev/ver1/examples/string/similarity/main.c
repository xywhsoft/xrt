/*
 * XRT Example - String Similarity
 * XRT 范例 - 字符串相似度
 *
 * Description / 说明:
 *   EN: Demonstrates xrtStrSim for calculating string similarity and
 *       xrtStrApprox for approximate string comparison.
 *   CN: 演示 xrtStrSim 计算字符串相似度和 xrtStrApprox 进行近似字符串比较。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_similarity.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_similarity -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtStrSim returns similarity score (0.0 to 1.0)
 *   - xrtStrApprox uses xCore.fApproxStrTol threshold (default 0.95)
 *   - Uses Levenshtein distance algorithm internally
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic similarity calculation
// 测试基本相似度计算
void TestBasicSimilarity()
{
	printf("=== Basic Similarity (xrtStrSim) ===\n");
	printf("=== 基本相似度 (xrtStrSim) ===\n");
	
	struct {
		char* s1;
		char* s2;
	} sPairs[] = {
		{"hello", "hello"},
		{"hello", "helo"},
		{"hello", "hallo"},
		{"hello", "world"},
		{"kitten", "sitting"},
		{"saturday", "sunday"},
		{"", ""},
		{"", "hello"},
		{"hello", ""},
	};
	int iNumPairs = 9;
	int i;
	
	for ( i = 0; i < iNumPairs; i++ ) {
		double fSim = xrtStrSim(sPairs[i].s1, 0, sPairs[i].s2, 0);
		printf("  \"%s\" vs \"%s\" -> %.4f (%.1f%%)\n", 
		       sPairs[i].s1, sPairs[i].s2, fSim, fSim * 100.0);
	}
	printf("\n");
}

// Test similarity with different string types
// 测试不同字符串类型的相似度
void TestDifferentTypes()
{
	printf("=== Similarity with Different Types ===\n");
	printf("=== 不同类型的相似度 ===\n");
	
	// Case sensitivity
	// 大小写敏感
	printf("Case sensitivity:\n");
	printf("大小写敏感:\n");
	double fSim1 = xrtStrSim("Hello", 0, "hello", 0);
	double fSim2 = xrtStrSim("Hello", 0, "HELLO", 0);
	printf("  \"Hello\" vs \"hello\": %.4f\n", fSim1);
	printf("  \"Hello\" vs \"HELLO\": %.4f\n\n", fSim2);
	
	// Similar words
	// 相似单词
	printf("Similar words:\n");
	printf("相似单词:\n");
	struct {
		char* s1;
		char* s2;
		char* sDesc;
		char* sDescCN;
	} sWords[] = {
		{"color", "colour", "US vs UK spelling", "美式 vs 英式拼写"},
		{"theater", "theatre", "US vs UK spelling", "美式 vs 英式拼写"},
		{"program", "programme", "US vs UK spelling", "美式 vs 英式拼写"},
		{"center", "centre", "US vs UK spelling", "美式 vs 英式拼写"},
	};
	int iNumWords = 4;
	int i;
	for ( i = 0; i < iNumWords; i++ ) {
		double fSim = xrtStrSim(sWords[i].s1, 0, sWords[i].s2, 0);
		printf("  \"%s\" vs \"%s\" (%s): %.4f\n", 
		       sWords[i].s1, sWords[i].s2, sWords[i].sDescCN, fSim);
	}
	printf("\n");
	
	// Typos
	// 拼写错误
	printf("Common typos:\n");
	printf("常见拼写错误:\n");
	struct {
		char* sCorrect;
		char* sTypo;
	} sTypos[] = {
		{"receive", "recieve"},
		{"separate", "seperate"},
		{"definitely", "definately"},
		{"accommodate", "accomodate"},
		{"occurrence", "occurence"},
	};
	int iNumTypos = 5;
	for ( i = 0; i < iNumTypos; i++ ) {
		double fSim = xrtStrSim(sTypos[i].sCorrect, 0, sTypos[i].sTypo, 0);
		printf("  \"%s\" vs \"%s\": %.4f\n", 
		       sTypos[i].sCorrect, sTypos[i].sTypo, fSim);
	}
	printf("\n");
}

// Test xrtStrApprox with threshold
// 测试带阈值的 xrtStrApprox
void TestApprox()
{
	printf("=== Approximate Comparison (xrtStrApprox) ===\n");
	printf("=== 近似比较 (xrtStrApprox) ===\n");
	
	// Show current threshold
	// 显示当前阈值
	printf("Current threshold: %.2f (%.0f%%)\n", 
	       xCore.fApproxStrTol, xCore.fApproxStrTol * 100.0);
	printf("当前阈值: %.2f (%.0f%%)\n\n", 
	       xCore.fApproxStrTol, xCore.fApproxStrTol * 100.0);
	
	struct {
		char* s1;
		char* s2;
	} sPairs[] = {
		{"hello", "hello"},     // Exact match
		{"hello", "helo"},      // 1 char missing
		{"hello", "hallo"},     // 1 char different
		{"hello", "helloo"},    // 1 char extra
		{"hello", "world"},     // Very different
	};
	int iNumPairs = 5;
	int i;
	
	for ( i = 0; i < iNumPairs; i++ ) {
		double fSim = xrtStrSim(sPairs[i].s1, 0, sPairs[i].s2, 0);
		bool bApprox = xrtStrApprox(sPairs[i].s1, 0, sPairs[i].s2, 0);
		printf("  \"%s\" vs \"%s\" -> sim=%.4f, approx=%s\n", 
		       sPairs[i].s1, sPairs[i].s2, fSim, bApprox ? "TRUE" : "FALSE");
	}
	printf("\n");
}

// Test with custom threshold
// 测试自定义阈值
void TestCustomThreshold()
{
	printf("=== Custom Threshold Test ===\n");
	printf("=== 自定义阈值测试 ===\n");
	
	char* s1 = "hello";
	char* s2 = "helo";
	double fSim = xrtStrSim(s1, 0, s2, 0);
	
	printf("Comparing \"%s\" vs \"%s\" (similarity: %.4f)\n\n", s1, s2, fSim);
	printf("比较 \"%s\" 和 \"%s\" (相似度: %.4f)\n\n", s1, s2, fSim);
	
	double fThresholds[] = {1.0, 0.95, 0.90, 0.85, 0.80, 0.75};
	int iNumThresholds = 6;
	int i;
	
	for ( i = 0; i < iNumThresholds; i++ ) {
		double fOldTol = xCore.fApproxStrTol;
		xCore.fApproxStrTol = fThresholds[i];
		bool bResult = xrtStrApprox(s1, 0, s2, 0);
		printf("  Threshold %.2f: %s\n", fThresholds[i], bResult ? "MATCH" : "NO MATCH");
		xCore.fApproxStrTol = fOldTol;
	}
	printf("\n");
}

// Test with explicit length parameters
// 测试显式长度参数
void TestWithLength()
{
	printf("=== Explicit Length Parameters ===\n");
	printf("=== 显式长度参数 ===\n");
	
	char* sText = "hello world";
	
	// Compare full string
	// 比较完整字符串
	double fSim1 = xrtStrSim(sText, 0, "hello", 0);
	printf("Full vs \"hello\": %.4f\n", fSim1);
	printf("完整 vs \"hello\": %.4f\n", fSim1);
	
	// Compare only first 5 chars
	// 只比较前 5 个字符
	double fSim2 = xrtStrSim(sText, 5, "hello", 0);
	printf("First 5 chars vs \"hello\": %.4f\n", fSim2);
	printf("前 5 个字符 vs \"hello\": %.4f\n", fSim2);
	
	// Compare with length limit on second string
	// 第二个字符串带长度限制
	double fSim3 = xrtStrSim("hello", 0, "hello world", 5);
	printf("\"hello\" vs first 5 of \"hello world\": %.4f\n", fSim3);
	printf("\"hello\" vs \"hello world\" 前 5 个字符: %.4f\n", fSim3);
	
	printf("\n");
}

// Test practical use case - fuzzy search
// 测试实际用例 - 模糊搜索
void TestFuzzySearch()
{
	printf("=== Fuzzy Search Example ===\n");
	printf("=== 模糊搜索示例 ===\n");
	
	char* psDatabase[] = {
		"apple", "application", "banana", "orange", 
		"applet", "appliance", "pineapple", "grape"
	};
	int iNumItems = 8;
	char* sQuery = "aple";  // Typo for "apple"
	
	printf("Search query: \"%s\" (looking for \"apple\")\n", sQuery);
	printf("搜索查询: \"%s\" (查找 \"apple\")\n\n", sQuery);
	
	printf("Results sorted by similarity:\n");
	printf("按相似度排序的结果:\n");
	
	// Calculate all similarities
	// 计算所有相似度
	int i;
	for ( i = 0; i < iNumItems; i++ ) {
		double fSim = xrtStrSim(sQuery, 0, psDatabase[i], 0);
		printf("  %.4f - %s\n", fSim, psDatabase[i]);
	}
	printf("\n");
}

// Test practical use case - spell check suggestions
// 测试实际用例 - 拼写检查建议
void TestSpellCheck()
{
	printf("=== Spell Check Suggestions ===\n");
	printf("=== 拼写检查建议 ===\n");
	
	char* psDictionary[] = {
		"receive", "receipt", "ceiling", "deceive", 
		"conceive", "perceive", "believe", "relieve"
	};
	int iNumWords = 8;
	char* sTypo = "recieve";  // Common misspelling
	
	printf("Misspelled word: \"%s\"\n", sTypo);
	printf("拼写错误单词: \"%s\"\n\n", sTypo);
	
	printf("Suggestions (similarity > 0.7):\n");
	printf("建议 (相似度 > 0.7):\n");
	
	int i;
	for ( i = 0; i < iNumWords; i++ ) {
		double fSim = xrtStrSim(sTypo, 0, psDictionary[i], 0);
		if ( fSim > 0.7 ) {
			printf("  %.4f - %s\n", fSim, psDictionary[i]);
		}
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String - String Similarity Demo\n");
	printf("XRT 字符串 - 字符串相似度演示\n");
	printf("===================================\n\n");
	
	TestBasicSimilarity();
	TestDifferentTypes();
	TestApprox();
	TestCustomThreshold();
	TestWithLength();
	TestFuzzySearch();
	TestSpellCheck();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
