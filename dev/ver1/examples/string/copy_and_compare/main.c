/*
 * XRT Example - String Copy and Compare
 * XRT 范例 - 字符串复制与比较
 *
 * Description / 说明:
 *   EN: Demonstrates string copy functions (xrtCopyStr/xrtCopyMem) and 
 *       comparison functions (xrtStrComp) with case-sensitive and case-insensitive modes.
 *   CN: 演示字符串复制函数（xrtCopyStr/xrtCopyMem）和比较函数（xrtStrComp），
 *       支持区分大小写和不区分大小写模式。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_copy_and_compare.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_copy_and_compare -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtCopyStr returns allocated memory that must be freed with xrtFree
 *   - xrtCopyMem copies raw memory bytes
 *   - xrtStrComp returns: <0 (s1<s2), =0 (equal), >0 (s1>s2)
 *   - bCase parameter: TRUE=case sensitive, FALSE=case insensitive
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test string copy functions
// 测试字符串复制函数
void TestStringCopy()
{
	str sSource = "Hello, 世界! Hello, World!";
	size_t iLen = strlen(sSource);
	
	printf("=== String Copy Functions ===\n");
	printf("=== 字符串复制函数 ===\n");
	printf("Source string: \"%s\"\n", sSource);
	printf("Length: %zu bytes\n\n", iLen);
	
	// Test xrtCopyStr
	// 测试 xrtCopyStr
	str sCopy1 = xrtCopyStr(sSource, iLen + 1);  // +1 for null terminator
	if ( sCopy1 )
	{
		printf("xrtCopyStr result: \"%s\"\n", sCopy1);
		printf("Addresses: source=%p, copy=%p\n", sSource, sCopy1);
		printf("Strings equal: %s\n", strcmp(sSource, sCopy1) == 0 ? "YES" : "NO");
		
		// Modify copy to verify independence
		// 修改副本来验证独立性
		sCopy1[0] = 'h';
		printf("After modifying copy: source=\"%s\", copy=\"%s\"\n", sSource, sCopy1);
		
		// Free allocated memory
		// 释放分配的内存
		xrtFree(sCopy1);
		printf("Memory freed.\n\n");
	}
	else
	{
		printf("xrtCopyStr failed!\n\n");
	}
	
	// Test xrtCopyMem
	// 测试 xrtCopyMem
	void* pCopy2 = xrtCopyMem(sSource, iLen + 1);  // +1 for null terminator
	if ( pCopy2 )
	{
		str sCopy2 = (str)pCopy2;
		printf("xrtCopyMem result: \"%s\"\n", sCopy2);
		printf("Addresses: source=%p, copy=%p\n", sSource, sCopy2);
		printf("Memory equal: %s\n", memcmp(sSource, sCopy2, iLen + 1) == 0 ? "YES" : "NO");
		
		xrtFree(pCopy2);
		printf("Memory freed.\n\n");
	}
	else
	{
		printf("xrtCopyMem failed!\n\n");
	}
}

// Test string comparison functions
// 测试字符串比较函数
void TestStringCompare()
{
	str s1 = "Hello";
	str s2 = "hello";
	str s3 = "Hello";
	str s4 = "World";
	str s5 = "Hello, World!";
	str s6 = "Hello, 世界!";
	
	printf("=== String Comparison Functions ===\n");
	printf("=== 字符串比较函数 ===\n");
	printf("Test strings:\n");
	printf("  s1: \"%s\"\n", s1);
	printf("  s2: \"%s\"\n", s2);
	printf("  s3: \"%s\"\n", s3);
	printf("  s4: \"%s\"\n", s4);
	printf("  s5: \"%s\"\n", s5);
	printf("  s6: \"%s\"\n\n", s6);
	
	// Case-sensitive comparison
	// 区分大小写比较
	printf("Case-sensitive (bCase=TRUE):\n");
	printf("  xrtStrComp(s1, s2) = %d  (%s)\n", 
	       xrtStrComp(s1, s2, strlen(s1)+1, TRUE), 
	       xrtStrComp(s1, s2, strlen(s1)+1, TRUE) == 0 ? "EQUAL" : "NOT EQUAL");
	printf("  xrtStrComp(s1, s3) = %d  (%s)\n", 
	       xrtStrComp(s1, s3, strlen(s1)+1, TRUE), 
	       xrtStrComp(s1, s3, strlen(s1)+1, TRUE) == 0 ? "EQUAL" : "NOT EQUAL");
	printf("  xrtStrComp(s1, s4) = %d  (%s)\n", 
	       xrtStrComp(s1, s4, strlen(s1)+1, TRUE), 
	       xrtStrComp(s1, s4, strlen(s1)+1, TRUE) < 0 ? "s1 < s4" : "s1 >= s4");
	printf("  xrtStrComp(s5, s6) = %d  (%s)\n\n", 
	       xrtStrComp(s5, s6, strlen(s5)+1, TRUE), 
	       xrtStrComp(s5, s6, strlen(s5)+1, TRUE) == 0 ? "EQUAL" : "NOT EQUAL");
	
	// Case-insensitive comparison
	// 不区分大小写比较
	printf("Case-insensitive (bCase=FALSE):\n");
	printf("  xrtStrComp(s1, s2) = %d  (%s)\n", 
	       xrtStrComp(s1, s2, strlen(s1)+1, FALSE), 
	       xrtStrComp(s1, s2, strlen(s1)+1, FALSE) == 0 ? "EQUAL" : "NOT EQUAL");
	printf("  xrtStrComp(s1, s3) = %d  (%s)\n", 
	       xrtStrComp(s1, s3, strlen(s1)+1, FALSE), 
	       xrtStrComp(s1, s3, strlen(s1)+1, FALSE) == 0 ? "EQUAL" : "NOT EQUAL");
	printf("  xrtStrComp(s1, s4) = %d  (%s)\n", 
	       xrtStrComp(s1, s4, strlen(s1)+1, FALSE), 
	       xrtStrComp(s1, s4, strlen(s1)+1, FALSE) < 0 ? "s1 < s4" : "s1 >= s4");
	printf("  xrtStrComp(s5, s6) = %d  (%s)\n\n", 
	       xrtStrComp(s5, s6, strlen(s5)+1, FALSE), 
	       xrtStrComp(s5, s6, strlen(s5)+1, FALSE) == 0 ? "EQUAL" : "NOT EQUAL");
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// User input validation
	// 用户输入验证
	str sInput1 = "ADMIN";
	str sInput2 = "admin";
	str sExpected = "admin";
	
	printf("User login validation:\n");
	printf("  Expected: \"%s\"\n", sExpected);
	printf("  Input 1:  \"%s\" -> %s\n", 
	       sInput1, 
	       xrtStrComp(sInput1, sExpected, strlen(sInput1)+1, FALSE) == 0 ? "VALID" : "INVALID");
	printf("  Input 2:  \"%s\" -> %s\n\n", 
	       sInput2, 
	       xrtStrComp(sInput2, sExpected, strlen(sInput2)+1, FALSE) == 0 ? "VALID" : "INVALID");
	
	// File extension check
	// 文件扩展名检查
	str sFilename1 = "document.txt";
	str sFilename2 = "image.JPG";
	str sExtension = ".txt";
	
	// Extract extension (simplified)
	// 提取扩展名（简化版）
	char* sDot1 = strrchr(sFilename1, '.');
	char* sDot2 = strrchr(sFilename2, '.');
	
	printf("File extension check:\n");
	printf("  File 1: \"%s\" -> Extension: %s -> %s\n", 
	       sFilename1, 
	       sDot1 ? sDot1 : "(none)",
	       sDot1 && xrtStrComp(sDot1, sExtension, strlen(sDot1)+1, FALSE) == 0 ? "MATCH" : "NO MATCH");
	printf("  File 2: \"%s\" -> Extension: %s -> %s\n\n", 
	       sFilename2,
	       sDot2 ? sDot2 : "(none)",
	       sDot2 && xrtStrComp(sDot2, sExtension, strlen(sDot2)+1, FALSE) == 0 ? "MATCH" : "NO MATCH");
	
	// Sorting example
	// 排序示例
	str arrNames[] = {"Charlie", "alice", "Bob", "delta"};
	int iCount = sizeof(arrNames) / sizeof(arrNames[0]);
	
	printf("Sorting names (case-insensitive):\n");
	printf("Before: ");
	for ( int i = 0; i < iCount; i++ )
	{
		printf("\"%s\" ", arrNames[i]);
	}
	printf("\n");
	
	// Simple bubble sort
	// 简单冒泡排序
	for ( int i = 0; i < iCount - 1; i++ )
	{
		for ( int j = 0; j < iCount - 1 - i; j++ )
		{
			if ( xrtStrComp(arrNames[j], arrNames[j+1], strlen(arrNames[j])+1, FALSE) > 0 )
			{
				str temp = arrNames[j];
				arrNames[j] = arrNames[j+1];
				arrNames[j+1] = temp;
			}
		}
	}
	
	printf("After:  ");
	for ( int i = 0; i < iCount; i++ )
	{
		printf("\"%s\" ", arrNames[i]);
	}
	printf("\n\n");
}

// Test memory management
// 测试内存管理
void TestMemoryManagement()
{
	printf("=== Memory Management ===\n");
	printf("=== 内存管理 ===\n");
	
	str sOriginal = "Original String";
	printf("Original: \"%s\" at %p\n", sOriginal, sOriginal);
	
	// Copy and verify
	// 复制并验证
	str sCopy = xrtCopyStr(sOriginal, strlen(sOriginal) + 1);
	if ( sCopy )
	{
		printf("Copy:     \"%s\" at %p\n", sCopy, sCopy);
		printf("Content equal: %s\n", strcmp(sOriginal, sCopy) == 0 ? "YES" : "NO");
		printf("Address equal: %s\n", sOriginal == sCopy ? "YES" : "NO");
		
		// Demonstrate memory independence
		// 演示内存独立性
		sCopy[0] = 'X';
		printf("After modifying copy:\n");
		printf("  Original: \"%s\"\n", sOriginal);
		printf("  Copy:     \"%s\"\n", sCopy);
		
		xrtFree(sCopy);
		printf("Copy memory freed.\n\n");
	}
	
	// Test copying NULL
	// 测试复制 NULL
	str sNullCopy = xrtCopyStr(NULL, 0);
	printf("Copying NULL: result = %p (%s)\n\n", 
	       sNullCopy, 
	       sNullCopy == NULL ? "NULL" : "NOT NULL");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Copy and Compare Demo\n");
	printf("XRT 字符串复制与比较演示\n");
	printf("==============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestStringCopy();
	TestStringCompare();
	TestPracticalExamples();
	TestMemoryManagement();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}