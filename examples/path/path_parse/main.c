/*
 * XRT Example - Path Parsing
 * XRT 范例 - 路径解析
 *
 * Description / 说明:
 *   EN: Demonstrates xrtPathGetNameExt, xrtPathGetName, xrtPathGetExt,
 *       xrtPathGetDir for extracting path components.
 *   CN: 演示 xrtPathGetNameExt、xrtPathGetName、xrtPathGetExt、xrtPathGetDir 提取路径组件。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/path_path_parse.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/path_path_parse              (Linux, GCC)
 *
 * Note / 注意:
 *   - All functions return newly allocated strings, must free with xrtFree
 *   - Size=0 means auto-detect string length
 *   - Handles both forward and backslash separators
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test path component extraction
// 测试路径组件提取
void TestPathComponents()
{
	printf("=== Path Component Extraction ===\n");
	printf("=== 路径组件提取 ===\n");
	
	char* psPaths[] = {
		"/home/user/documents/file.txt",
		"C:\\Users\\Admin\\Desktop\\report.pdf",
		"/var/log/app.log",
		"file_without_extension",
		".hidden_file",
		"/path/to/directory/",
		"simple.txt",
	};
	
	char* psDesc[] = {
		"Unix path with extension",
		"Windows path with extension",
		"Log file path",
		"No extension",
		"Hidden file",
		"Directory path",
		"Simple filename",
	};
	
	int iNumPaths = 7;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		printf("\nPath: %s\n", psPaths[i]);
		printf("描述: %s\n", psDesc[i]);
		
		str sNameExt = xrtPathGetNameExt(psPaths[i], 0);
		str sName = xrtPathGetName(psPaths[i], 0);
		str sExt = xrtPathGetExt(psPaths[i], 0);
		str sDir = xrtPathGetDir(psPaths[i], 0);
		
		printf("  Name+Ext: \"%s\"\n", sNameExt);
		printf("  Name:     \"%s\"\n", sName);
		printf("  Ext:      \"%s\"\n", sExt);
		printf("  Dir:      \"%s\"\n", sDir);
		
		xrtFree(sNameExt);
		xrtFree(sName);
		xrtFree(sExt);
		xrtFree(sDir);
	}
	printf("\n");
}

// Test GetNameExt - filename with extension
// 测试 GetNameExt - 带扩展名的文件名
void TestGetNameExt()
{
	printf("=== GetNameExt (Filename with Extension) ===\n");
	printf("=== GetNameExt (带扩展名的文件名) ===\n");
	
	char* psPaths[] = {
		"/a/b/c.txt",
		"filename.ext",
		"/path/to/file",
		"/",
		"file.tar.gz",
	};
	
	int iNumPaths = 5;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		str sNameExt = xrtPathGetNameExt(psPaths[i], 0);
		printf("  \"%s\" -> \"%s\"\n", psPaths[i], sNameExt);
		xrtFree(sNameExt);
	}
	printf("\n");
}

// Test GetName - filename without extension
// 测试 GetName - 不带扩展名的文件名
void TestGetName()
{
	printf("=== GetName (Filename without Extension) ===\n");
	printf("=== GetName (不带扩展名的文件名) ===\n");
	
	char* psPaths[] = {
		"/a/b/c.txt",
		"filename.ext",
		"/path/to/file",
		"file.tar.gz",
		".gitignore",
	};
	
	int iNumPaths = 5;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		str sName = xrtPathGetName(psPaths[i], 0);
		printf("  \"%s\" -> \"%s\"\n", psPaths[i], sName);
		xrtFree(sName);
	}
	printf("\n");
}

// Test GetExt - extension only
// 测试 GetExt - 仅扩展名
void TestGetExt()
{
	printf("=== GetExt (Extension Only) ===\n");
	printf("=== GetExt (仅扩展名) ===\n");
	
	char* psPaths[] = {
		"file.txt",
		"document.pdf",
		"archive.tar.gz",
		"noextension",
		".hidden",
		"image.JPEG",
	};
	
	int iNumPaths = 6;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		str sExt = xrtPathGetExt(psPaths[i], 0);
		printf("  \"%s\" -> \"%s\"\n", psPaths[i], sExt);
		xrtFree(sExt);
	}
	printf("\n");
}

// Test GetDir - directory path
// 测试 GetDir - 目录路径
void TestGetDir()
{
	printf("=== GetDir (Directory Path) ===\n");
	printf("=== GetDir (目录路径) ===\n");
	
	char* psPaths[] = {
		"/home/user/file.txt",
		"file.txt",
		"/root/",
		"/root",
		"C:\\Windows\\System32\\drivers",
		"./relative/path/file",
	};
	
	int iNumPaths = 6;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		str sDir = xrtPathGetDir(psPaths[i], 0);
		printf("  \"%s\" -> \"%s\"\n", psPaths[i], sDir);
		xrtFree(sDir);
	}
	printf("\n");
}

// Test with size parameter
// 测试带大小参数
void TestWithSize()
{
	printf("=== Explicit Size Parameter ===\n");
	printf("=== 显式大小参数 ===\n");
	
	char* sPath = "/home/user/documents/file.txt";
	size_t iLen = strlen(sPath);
	
	printf("Full path: %s\n", sPath);
	printf("完整路径: %s\n", sPath);
	
	// Parse only first 17 chars ("/home/user/docs")
	// 只解析前 17 个字符
	str sDir = xrtPathGetDir(sPath, 17);
	printf("Dir (first 17 chars): \"%s\"\n", sDir);
	printf("目录 (前 17 字符): \"%s\"\n", sDir);
	xrtFree(sDir);
	
	// Full path
	// 完整路径
	sDir = xrtPathGetDir(sPath, 0);
	printf("Dir (full): \"%s\"\n", sDir);
	printf("目录 (完整): \"%s\"\n", sDir);
	xrtFree(sDir);
	
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Path - Path Parsing Demo\n");
	printf("XRT 路径 - 路径解析演示\n");
	printf("============================\n\n");
	
	TestPathComponents();
	TestGetNameExt();
	TestGetName();
	TestGetExt();
	TestGetDir();
	TestWithSize();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
