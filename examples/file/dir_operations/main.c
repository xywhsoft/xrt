/*
 * XRT Example - Directory Operations
 * XRT 范例 - 目录操作
 *
 * Description / 说明:
 *   EN: Demonstrates xrtDirCreate, xrtDirCreateAll, xrtDirScan, xrtDirCopy,
 *       xrtDirDelete for directory management.
 *   CN: 演示 xrtDirCreate、xrtDirCreateAll、xrtDirScan、xrtDirCopy、xrtDirDelete
 *       进行目录管理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_dir_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_dir_operations              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtDirCreateAll creates parent directories as needed
 *   - xrtDirScan can scan recursively
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Callback for directory scanning
// 目录扫描回调
int PrintFileCallback(str sPath, int bIsDir, ptr pParam)
{
	printf("  %s %s\n", bIsDir ? "[DIR]" : "[FILE]", sPath);
	return 0;  // Continue scanning
}

// Test directory creation
// 测试目录创建
void TestDirCreate()
{
	printf("=== Directory Creation ===\n");
	printf("=== 目录创建 ===\n");
	
	// Create single directory
	// 创建单个目录
	str sPath = "examples/bin/testdir";
	bool bResult = xrtDirCreate(sPath);
	printf("Create %s: %s\n", sPath, bResult ? "OK" : "FAILED");
	printf("创建 %s: %s\n", sPath, bResult ? "成功" : "失败");
	
	// Create nested directories
	// 创建嵌套目录
	sPath = "examples/bin/nested/deep/path";
	bResult = xrtDirCreateAll(sPath);
	printf("Create nested %s: %s\n", sPath, bResult ? "OK" : "FAILED");
	printf("创建嵌套 %s: %s\n", sPath, bResult ? "成功" : "失败");
	printf("\n");
}

// Test directory scanning
// 测试目录扫描
void TestDirScan()
{
	printf("=== Directory Scan ===\n");
	printf("=== 目录扫描 ===\n");
	
	// Create some test structure
	// 创建一些测试结构
	xrtDirCreateAll("examples/bin/scan_test/subdir");
	xrtFileWriteAll("examples/bin/scan_test/file1.txt", "content1", 8, 0);
	xrtFileWriteAll("examples/bin/scan_test/file2.txt", "content2", 8, 0);
	xrtFileWriteAll("examples/bin/scan_test/subdir/file3.txt", "content3", 8, 0);
	
	printf("Non-recursive scan of examples/bin/scan_test:\n");
	printf("非递归扫描 examples/bin/scan_test:\n");
	xrtDirScan("examples/bin/scan_test", FALSE, (ptr)PrintFileCallback, NULL);
	
	printf("\nRecursive scan:\n");
	printf("递归扫描:\n");
	xrtDirScan("examples/bin/scan_test", TRUE, (ptr)PrintFileCallback, NULL);
	printf("\n");
}

// Test directory copy
// 测试目录复制
void TestDirCopy()
{
	printf("=== Directory Copy ===\n");
	printf("=== 目录复制 ===\n");
	
	// Source already exists from previous test
	// 源目录从前面的测试已存在
	str sSrc = "examples/bin/scan_test";
	str sDst = "examples/bin/scan_test_copy";
	
	printf("Copying %s to %s\n", sSrc, sDst);
	printf("复制 %s 到 %s\n", sSrc, sDst);
	
	bool bResult = xrtDirCopy(sSrc, sDst, TRUE);
	printf("Result: %s\n", bResult ? "OK" : "FAILED");
	printf("结果: %s\n", bResult ? "成功" : "失败");
	
	if ( bResult ) {
		printf("\nContents of copy:\n");
		printf("副本内容:\n");
		xrtDirScan(sDst, TRUE, (ptr)PrintFileCallback, NULL);
	}
	printf("\n");
}

// Test directory delete
// 测试目录删除
void TestDirDelete()
{
	printf("=== Directory Delete ===\n");
	printf("=== 目录删除 ===\n");
	
	str sPath = "examples/bin/scan_test_copy";
	
	printf("Deleting %s\n", sPath);
	printf("删除 %s\n", sPath);
	printf("Exists before: %s\n", xrtPathExists(sPath) ? "YES" : "NO");
	printf("删除前存在: %s\n", xrtPathExists(sPath) ? "是" : "否");
	
	bool bResult = xrtDirDelete(sPath);
	printf("Delete result: %s\n", bResult ? "OK" : "FAILED");
	printf("删除结果: %s\n", bResult ? "成功" : "失败");
	printf("Exists after: %s\n", xrtPathExists(sPath) ? "YES" : "NO");
	printf("删除后存在: %s\n", xrtPathExists(sPath) ? "是" : "否");
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Project structure creation
	// 项目结构创建
	printf("Creating project structure:\n");
	printf("创建项目结构:\n");
	
	char* psDirs[] = {
		"examples/bin/project/src",
		"examples/bin/project/include",
		"examples/bin/project/docs",
		"examples/bin/project/build",
		"examples/bin/project/tests",
	};
	
	int iNumDirs = 5;
	int i;
	for ( i = 0; i < iNumDirs; i++ ) {
		xrtDirCreateAll(psDirs[i]);
		printf("  Created: %s\n", psDirs[i]);
		printf("  已创建: %s\n", psDirs[i]);
	}
	
	// Create some files
	// 创建一些文件
	xrtFileWriteAll("examples/bin/project/src/main.c", "int main() { return 0; }\n", 24, 0);
	xrtFileWriteAll("examples/bin/project/README.md", "# Project\n", 10, 0);
	
	printf("\nProject structure:\n");
	printf("项目结构:\n");
	xrtDirScan("examples/bin/project", TRUE, (ptr)PrintFileCallback, NULL);
	printf("\n");
	
	// Clean up
	// 清理
	printf("Cleaning up project directory...\n");
	printf("清理项目目录...\n");
	xrtDirDelete("examples/bin/project");
	printf("Done.\n");
	printf("完成。\n");
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File - Directory Operations Demo\n");
	printf("XRT 文件 - 目录操作演示\n");
	printf("====================================\n\n");
	
	TestDirCreate();
	TestDirScan();
	TestDirCopy();
	TestDirDelete();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
