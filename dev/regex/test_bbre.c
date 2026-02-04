/*
 * bbre 正则表达式库完整能力测试
 * 编译命令 (TCC): tcc -m64 test_bbre.c bbre.c -o test_bbre.exe
 * 编译命令 (GCC): gcc -m64 test_bbre.c bbre.c -O2 -o test_bbre.exe
 */

#include <stdio.h>
#include <string.h>
#include "bbre.h"

static int g_iTestCount = 0;
static int g_iPassCount = 0;



// 打印匹配的子串
void print_span(const char *sText, bbre_span span)
{
	for ( size_t i = span.begin; i < span.end; i++ ) {
		putchar(sText[i]);
	}
}

// 获取匹配子串
void get_span(const char *sText, bbre_span span, char *sBuf, size_t iBufSize)
{
	size_t iLen = span.end - span.begin;
	if ( iLen >= iBufSize ) {
		iLen = iBufSize - 1;
	}
	memcpy(sBuf, sText + span.begin, iLen);
	sBuf[iLen] = '\0';
}

// 测试辅助宏
#define TEST_ASSERT(cond, msg) do { \
	g_iTestCount++; \
	if ( cond ) { \
		g_iPassCount++; \
		printf("  [PASS] %s\n", msg); \
	} else { \
		printf("  [FAIL] %s\n", msg); \
	} \
} while(0)



// 测试1：基本匹配
void test_basic_match()
{
	printf("=== 测试1：基本匹配 ===\n");
	
	bbre *reg = bbre_init_pattern("hello");
	TEST_ASSERT(reg != NULL, "Pattern 'hello' compiles");
	
	const char *sText1 = "say hello world";
	const char *sText2 = "goodbye world";
	
	int iResult1 = bbre_is_match(reg, sText1, strlen(sText1));
	int iResult2 = bbre_is_match(reg, sText2, strlen(sText2));
	
	TEST_ASSERT(iResult1 == 1, "'say hello world' matches 'hello'");
	TEST_ASSERT(iResult2 == 0, "'goodbye world' does not match 'hello'");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试2：查找匹配位置
void test_find_match()
{
	printf("=== 测试2：查找匹配位置 ===\n");
	
	bbre *reg = bbre_init_pattern("\\d+");
	TEST_ASSERT(reg != NULL, "Pattern '\\d+' compiles");
	
	const char *sText = "Order: 12345, Price: 99";
	bbre_span span;
	char sBuf[64];
	
	int iResult = bbre_find(reg, sText, strlen(sText), &span);
	TEST_ASSERT(iResult == 1, "Found match in text");
	
	get_span(sText, span, sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "12345") == 0, "First number is '12345'");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试3：捕获组提取
void test_capture_groups()
{
	printf("=== 测试3：捕获组提取 ===\n");
	
	bbre *reg = bbre_init_pattern("(\\d{4})-(\\d{2})-(\\d{2})");
	TEST_ASSERT(reg != NULL, "Date pattern compiles");
	
	const char *sText = "Date is 2024-12-25 Christmas";
	bbre_span captures[4];
	char sBuf[64];
	
	int iResult = bbre_captures(reg, sText, strlen(sText), captures, 4);
	TEST_ASSERT(iResult == 1, "Date pattern matches");
	
	get_span(sText, captures[0], sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "2024-12-25") == 0, "Full match is '2024-12-25'");
	
	get_span(sText, captures[1], sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "2024") == 0, "Year is '2024'");
	
	get_span(sText, captures[2], sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "12") == 0, "Month is '12'");
	
	get_span(sText, captures[3], sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "25") == 0, "Day is '25'");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试4：多次匹配（遍历所有匹配项）
void test_find_all()
{
	printf("=== 测试4：多次匹配 ===\n");
	
	bbre *reg = bbre_init_pattern("\\w+@\\w+\\.\\w+");
	TEST_ASSERT(reg != NULL, "Email pattern compiles");
	
	const char *sText = "alice@test.com bob@example.org";
	size_t iPos = 0;
	size_t iTextLen = strlen(sText);
	int iCount = 0;
	
	while ( iPos < iTextLen ) {
		bbre_span span;
		int iResult = bbre_find_at(reg, sText, iTextLen, iPos, &span);
		if ( iResult != 1 ) {
			break;
		}
		iCount++;
		iPos = span.end;
	}
	
	TEST_ASSERT(iCount == 2, "Found 2 emails");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试5：忽略大小写 (?i)
void test_case_insensitive()
{
	printf("=== 测试5：忽略大小写 (?i) ===\n");
	
	bbre *reg = bbre_init_pattern("(?i)hello");
	TEST_ASSERT(reg != NULL, "(?i)hello compiles");
	
	TEST_ASSERT(bbre_is_match(reg, "Hello", 5) == 1, "'Hello' matches (?i)hello");
	TEST_ASSERT(bbre_is_match(reg, "HELLO", 5) == 1, "'HELLO' matches (?i)hello");
	TEST_ASSERT(bbre_is_match(reg, "hello", 5) == 1, "'hello' matches (?i)hello");
	TEST_ASSERT(bbre_is_match(reg, "HeLLo", 5) == 1, "'HeLLo' matches (?i)hello");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试6：贪婪与非贪婪匹配 (*? +? ??)
void test_greedy()
{
	printf("=== 测试6：贪婪与非贪婪匹配 ===\n");
	
	const char *sText = "<div>content</div>";
	bbre_span span;
	char sBuf[64];
	
	// 贪婪匹配 <.*>
	bbre *reg1 = bbre_init_pattern("<.*>");
	TEST_ASSERT(reg1 != NULL, "Greedy <.*> compiles");
	bbre_find(reg1, sText, strlen(sText), &span);
	get_span(sText, span, sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "<div>content</div>") == 0, "Greedy matches entire string");
	bbre_destroy(reg1);
	
	// 非贪婪匹配 <.*?>
	bbre *reg2 = bbre_init_pattern("<.*?>");
	TEST_ASSERT(reg2 != NULL, "Non-greedy <.*?> compiles");
	bbre_find(reg2, sText, strlen(sText), &span);
	get_span(sText, span, sBuf, sizeof(sBuf));
	TEST_ASSERT(strcmp(sBuf, "<div>") == 0, "Non-greedy matches '<div>' only");
	bbre_destroy(reg2);
	
	printf("\n");
}



// 测试7：字符类 (\d \D \w \W \s \S)
void test_char_classes()
{
	printf("=== 测试7：字符类 ===\n");
	
	// \d 数字
	bbre *reg_d = bbre_init_pattern("\\d+");
	TEST_ASSERT(reg_d != NULL, "\\d+ compiles");
	TEST_ASSERT(bbre_is_match(reg_d, "abc123def", 9) == 1, "\\d+ matches digits");
	bbre_destroy(reg_d);
	
	// \D 非数字
	bbre *reg_D = bbre_init_pattern("\\D+");
	TEST_ASSERT(reg_D != NULL, "\\D+ compiles");
	TEST_ASSERT(bbre_is_match(reg_D, "abc", 3) == 1, "\\D+ matches non-digits");
	bbre_destroy(reg_D);
	
	// \w 单词字符
	bbre *reg_w = bbre_init_pattern("\\w+");
	TEST_ASSERT(reg_w != NULL, "\\w+ compiles");
	TEST_ASSERT(bbre_is_match(reg_w, "hello_123", 9) == 1, "\\w+ matches word chars");
	bbre_destroy(reg_w);
	
	// \s 空白
	bbre *reg_s = bbre_init_pattern("\\s+");
	TEST_ASSERT(reg_s != NULL, "\\s+ compiles");
	TEST_ASSERT(bbre_is_match(reg_s, "hello world", 11) == 1, "\\s+ matches whitespace");
	bbre_destroy(reg_s);
	
	printf("\n");
}



// 测试8：错误处理
void test_error_handling()
{
	printf("=== 测试8：错误处理 ===\n");
	
	// 无效的正则表达式 - 未关闭的字符类
	bbre *reg1 = bbre_init_pattern("[invalid");
	TEST_ASSERT(reg1 == NULL, "Invalid pattern '[invalid' fails");
	
	// 无效的正则表达式 - 未匹配的括号
	bbre *reg2 = bbre_init_pattern("(unclosed");
	TEST_ASSERT(reg2 == NULL, "Invalid pattern '(unclosed' fails");
	
	// 无效的量词
	bbre *reg3 = bbre_init_pattern("a{invalid}");
	TEST_ASSERT(reg3 == NULL, "Invalid pattern 'a{invalid}' fails");
	
	printf("\n");
}



// 测试9：正则表达式集合 (bbre_set)
void test_regex_set()
{
	printf("=== 测试9：正则表达式集合 ===\n");
	
	const char *arrPatterns[] = { "apple", "banana", "cherry" };
	bbre_set *set = bbre_set_init_patterns(arrPatterns, 3);
	TEST_ASSERT(set != NULL, "Regex set created");
	
	TEST_ASSERT(bbre_set_is_match(set, "apple pie", 9) == 1, "Set matches 'apple'");
	TEST_ASSERT(bbre_set_is_match(set, "banana", 6) == 1, "Set matches 'banana'");
	TEST_ASSERT(bbre_set_is_match(set, "no fruit", 8) == 0, "Set does not match 'no fruit'");
	
	// 测试多模式同时匹配
	unsigned int arrIdxs[3];
	unsigned int iNumIdxs = 0;
	bbre_set_matches(set, "cherry and apple", 16, arrIdxs, 3, &iNumIdxs);
	TEST_ASSERT(iNumIdxs == 2, "Multiple patterns match");
	
	bbre_set_destroy(set);
	printf("\n");
}



// 测试10：命名捕获组 (?<name>...)
void test_named_captures()
{
	printf("=== 测试10：命名捕获组 ===\n");
	
	// (?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})
	bbre *reg = bbre_init_pattern("(?<year>\\d{4})-(?<month>\\d{2})-(?<day>\\d{2})");
	TEST_ASSERT(reg != NULL, "Named capture pattern compiles");
	
	// 检查捕获组数量
	unsigned int iCount = bbre_capture_count(reg);
	TEST_ASSERT(iCount == 4, "4 capture groups (0=full, 1=year, 2=month, 3=day)");
	
	// 检查捕获组名称
	const char *sName1 = bbre_capture_name(reg, 1, NULL);
	TEST_ASSERT(sName1 != NULL && strcmp(sName1, "year") == 0, "Group 1 named 'year'");
	
	const char *sName2 = bbre_capture_name(reg, 2, NULL);
	TEST_ASSERT(sName2 != NULL && strcmp(sName2, "month") == 0, "Group 2 named 'month'");
	
	const char *sName3 = bbre_capture_name(reg, 3, NULL);
	TEST_ASSERT(sName3 != NULL && strcmp(sName3, "day") == 0, "Group 3 named 'day'");
	
	bbre_destroy(reg);
	printf("\n");
}



// 测试11：边界断言 (\b \B \A \z ^ $)
void test_assertions()
{
	printf("=== 测试11：边界断言 ===\n");
	
	// \b 词边界
	bbre *reg_b = bbre_init_pattern("\\bword\\b");
	TEST_ASSERT(reg_b != NULL, "\\bword\\b compiles");
	TEST_ASSERT(bbre_is_match(reg_b, "a word here", 11) == 1, "\\b matches word boundary");
	TEST_ASSERT(bbre_is_match(reg_b, "awordhere", 9) == 0, "\\b does not match mid-word");
	bbre_destroy(reg_b);
	
	// ^ 开头
	bbre *reg_start = bbre_init_pattern("^hello");
	TEST_ASSERT(reg_start != NULL, "^hello compiles");
	TEST_ASSERT(bbre_is_match(reg_start, "hello world", 11) == 1, "^ matches start");
	TEST_ASSERT(bbre_is_match(reg_start, "say hello", 9) == 0, "^ does not match mid");
	bbre_destroy(reg_start);
	
	// $ 结尾
	bbre *reg_end = bbre_init_pattern("world$");
	TEST_ASSERT(reg_end != NULL, "world$ compiles");
	TEST_ASSERT(bbre_is_match(reg_end, "hello world", 11) == 1, "$ matches end");
	TEST_ASSERT(bbre_is_match(reg_end, "world hello", 11) == 0, "$ does not match start");
	bbre_destroy(reg_end);
	
	printf("\n");
}



// 测试12：多行模式 (?m)
void test_multiline()
{
	printf("=== 测试12：多行模式 (?m) ===\n");
	
	const char *sText = "line1\nline2\nline3";
	
	// 没有 (?m)，^ 只匹配文本开头
	bbre *reg1 = bbre_init_pattern("^line2");
	TEST_ASSERT(reg1 != NULL, "^line2 compiles");
	TEST_ASSERT(bbre_is_match(reg1, sText, strlen(sText)) == 0, "Without (?m), ^line2 does not match");
	bbre_destroy(reg1);
	
	// 有 (?m)，^ 匹配每行开头
	bbre *reg2 = bbre_init_pattern("(?m)^line2");
	TEST_ASSERT(reg2 != NULL, "(?m)^line2 compiles");
	TEST_ASSERT(bbre_is_match(reg2, sText, strlen(sText)) == 1, "With (?m), ^line2 matches");
	bbre_destroy(reg2);
	
	printf("\n");
}



// 测试13：引用文本 \Q...\E
void test_quoted_text()
{
	printf("=== 测试13：引用文本 \\Q...\\E ===\n");
	
	// \Q...\E 内的内容作为普通文本匹配
	bbre *reg = bbre_init_pattern("\\Q.*+?\\E");
	TEST_ASSERT(reg != NULL, "\\Q.*+?\\E compiles");
	TEST_ASSERT(bbre_is_match(reg, ".*+?", 4) == 1, "\\Q...\\E matches literal '.*+?'");
	TEST_ASSERT(bbre_is_match(reg, "abc", 3) == 0, "\\Q...\\E does not match other text");
	bbre_destroy(reg);
	
	printf("\n");
}



// 测试14：量词 {n} {n,} {n,m}
void test_quantifiers()
{
	printf("=== 测试14：量词 {n} {n,} {n,m} ===\n");
	
	// {3} 精确匹配
	bbre *reg1 = bbre_init_pattern("a{3}");
	TEST_ASSERT(reg1 != NULL, "a{3} compiles");
	TEST_ASSERT(bbre_is_match(reg1, "aaa", 3) == 1, "a{3} matches 'aaa'");
	TEST_ASSERT(bbre_is_match(reg1, "aa", 2) == 0, "a{3} does not match 'aa'");
	bbre_destroy(reg1);
	
	// {2,4} 范围匹配
	bbre *reg2 = bbre_init_pattern("^a{2,4}$");
	TEST_ASSERT(reg2 != NULL, "a{2,4} compiles");
	TEST_ASSERT(bbre_is_match(reg2, "aa", 2) == 1, "a{2,4} matches 'aa'");
	TEST_ASSERT(bbre_is_match(reg2, "aaaa", 4) == 1, "a{2,4} matches 'aaaa'");
	TEST_ASSERT(bbre_is_match(reg2, "aaaaa", 5) == 0, "a{2,4} does not match 'aaaaa'");
	bbre_destroy(reg2);
	
	// {2,} 至少匹配
	bbre *reg3 = bbre_init_pattern("a{2,}");
	TEST_ASSERT(reg3 != NULL, "a{2,} compiles");
	TEST_ASSERT(bbre_is_match(reg3, "aaaaa", 5) == 1, "a{2,} matches 'aaaaa'");
	TEST_ASSERT(bbre_is_match(reg3, "a", 1) == 0, "a{2,} does not match 'a'");
	bbre_destroy(reg3);
	
	printf("\n");
}



// 测试15：Unicode 属性 \p{...}
void test_unicode_property()
{
	printf("=== 测试15：Unicode 属性 ===\n");
	
	// \p{L} 匹配字母
	bbre *reg_L = bbre_init_pattern("\\p{L}+");
	TEST_ASSERT(reg_L != NULL, "\\p{L}+ compiles");
	TEST_ASSERT(bbre_is_match(reg_L, "Hello", 5) == 1, "\\p{L}+ matches letters");
	bbre_destroy(reg_L);
	
	// \p{N} 匹配数字
	bbre *reg_N = bbre_init_pattern("\\p{N}+");
	TEST_ASSERT(reg_N != NULL, "\\p{N}+ compiles");
	TEST_ASSERT(bbre_is_match(reg_N, "12345", 5) == 1, "\\p{N}+ matches numbers");
	bbre_destroy(reg_N);
	
	// \p{Han} 匹配中文字符
	bbre *reg_Han = bbre_init_pattern("\\p{Han}+");
	if ( reg_Han != NULL ) {
		TEST_ASSERT(bbre_is_match(reg_Han, "中文", 6) == 1, "\\p{Han}+ matches Chinese");
		bbre_destroy(reg_Han);
	} else {
		printf("  [SKIP] \\p{Han} not supported\n");
	}
	
	printf("\n");
}



// 测试16：bbre_clone 克隆
void test_clone()
{
	printf("=== 测试16：bbre_clone 克隆 ===\n");
	
	bbre *reg = bbre_init_pattern("hello");
	TEST_ASSERT(reg != NULL, "Original pattern compiles");
	
	bbre *reg_clone = NULL;
	int iErr = bbre_clone(&reg_clone, reg, NULL);
	TEST_ASSERT(iErr == 0 && reg_clone != NULL, "Clone succeeds");
	
	// 验证克隆后的正则可用
	TEST_ASSERT(bbre_is_match(reg_clone, "hello", 5) == 1, "Cloned regex works");
	
	bbre_destroy(reg);
	bbre_destroy(reg_clone);
	printf("\n");
}



int main()
{
	printf("========================================\n");
	printf("    bbre 正则表达式库完整能力测试\n");
	printf("         version: %s\n", bbre_version());
	printf("========================================\n\n");
	
	test_basic_match();
	test_find_match();
	test_capture_groups();
	test_find_all();
	test_case_insensitive();
	test_greedy();
	test_char_classes();
	test_error_handling();
	test_regex_set();
	test_named_captures();
	test_assertions();
	test_multiline();
	test_quoted_text();
	test_quantifiers();
	test_unicode_property();
	test_clone();
	
	printf("========================================\n");
	printf("    测试结果: %d/%d PASSED\n", g_iPassCount, g_iTestCount);
	printf("========================================\n");
	
	return (g_iPassCount == g_iTestCount) ? 0 : 1;
}
