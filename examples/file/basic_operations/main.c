/*
 * XRT Example - File Basic Operations
 * XRT 范例 - 文件基本操作
 *
 * Description / 说明:
 *   EN: Demonstrates basic file operations including creation, reading, writing,
 *       copying, moving, and deletion using XRT file functions.
 *   CN: 演示基本文件操作，包括创建、读取、写入、复制、移动和删除，
 *       使用 XRT 文件函数。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_basic_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_basic_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - File operations: xrtFileExists, xrtFileSize, xrtFileReadAll, xrtFileWriteAll
 *   - Directory operations: xrtDirExists, xrtMakeDir, xrtListDir
 *   - Path operations: xrtPathJoin, xrtPathExt, xrtPathBase
 *   - All file handles must be closed properly
 *   - Error handling for file operations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test file creation and writing
// 测试文件创建和写入
void TestFileCreationAndWriting()
{
	printf("=== File Creation and Writing ===\n");
	printf("=== 文件创建和写入 ===\n");
	
	// Create test directory
	// 创建测试目录
	str sTestDir = "./test_files";
	if ( !xrtDirExists(sTestDir) ) {
		xrtDirCreate(sTestDir);
		printf("Created test directory: %s\n", sTestDir);
	}
	
	// Create and write to text file
	// 创建并写入文本文件
	str sTextFile = xrtPathJoin(2, sTestDir, "test.txt");
	str sContent = "Hello World!\nThis is a test file.\nLine 3: Numbers 12345\n";
	
	int iWriteResult = xrtFileWriteAll(sTextFile, sContent, strlen(sContent), XRT_CP_UTF8);
	
	printf("Text file operations:\n");
	printf("  File path: %s\n", sTextFile);
	printf("  Content to write: %zu characters\n", strlen(sContent));
	printf("  Write result: %s\n", iWriteResult ? "SUCCESS" : "FAILED");
	
	// Create binary file
	// 创建二进制文件
	str sBinaryFile = xrtPathJoin(2, sTestDir, "test.bin");
	unsigned char arrData[] = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF};
	size_t iDataSize = sizeof(arrData);
	
	FILE* pFile = fopen(sBinaryFile, "wb");
	if ( pFile ) {
		fwrite(arrData, 1, iDataSize, pFile);
		fclose(pFile);
		printf("  Binary file created: %s (%zu bytes)\n", sBinaryFile, iDataSize);
	}
	
	// Cleanup
	// 清理
	xrtFree(sTextFile);
	xrtFree(sBinaryFile);
}

// Test file reading and information
// 测试文件读取和信息获取
void TestFileReadingAndInfo()
{
	printf("\n=== File Reading and Information ===\n");
	printf("=== 文件读取和信息获取 ===\n");
	
	str sTestDir = "./test_files";
	str sTextFile = xrtPathJoin(2, sTestDir, "test.txt");
	str sBinaryFile = xrtPathJoin(2, sTestDir, "test.bin");
	
	// Check if files exist
	// 检查文件是否存在
	bool bTextExists = xrtFileExists(sTextFile);
	bool bBinaryExists = xrtFileExists(sBinaryFile);
	
	printf("File existence check:\n");
	printf("  %s: %s\n", sTextFile, bTextExists ? "EXISTS" : "NOT FOUND");
	printf("  %s: %s\n", sBinaryFile, bBinaryExists ? "EXISTS" : "NOT FOUND");
	
	if ( bTextExists ) {
		// Get file size
		// 获取文件大小
		struct stat stText;
		if ( stat(sTextFile, &stText) == 0 ) {
			printf("  Text file size: %lld bytes\n", (long long)stText.st_size);
		}
		
		// Read text file content
		// 读取文本文件内容
		size_t iReadSize = 0;
		str sReadContent = xrtFileReadAll(sTextFile, XRT_CP_UTF8, &iReadSize);
		
		if ( sReadContent ) {
			printf("  Text file content:\n-----\n%s-----\n", sReadContent);
			xrtFree(sReadContent);
		}
	}
	
	if ( bBinaryExists ) {
		// Get binary file size
		// 获取二进制文件大小
		struct stat stBinary;
		if ( stat(sBinaryFile, &stBinary) == 0 ) {
			printf("  Binary file size: %lld bytes\n", (long long)stBinary.st_size);
		}
		
		// Read binary file
		// 读取二进制文件
		FILE* pFile = fopen(sBinaryFile, "rb");
		if ( pFile ) {
			// Get file size first
			fseek(pFile, 0, SEEK_END);
			long iFileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			
			unsigned char* arrBuffer = xrtMalloc(iFileSize);
			size_t iBytesRead = fread(arrBuffer, 1, iFileSize, pFile);
			fclose(pFile);
			
			printf("  Binary content: ");
			for ( size_t i = 0; i < iBytesRead && i < 16; i++ ) {  // Show first 16 bytes
				printf("%02X ", arrBuffer[i]);
			}
			if ( iBytesRead > 16 ) {
				printf("...");
			}
			printf("\n");
			
			xrtFree(arrBuffer);
		}
	}
	
	// Cleanup
	// 清理
	xrtFree(sTextFile);
	xrtFree(sBinaryFile);
}

// Test file copying and moving
// 测试文件复制和移动
void TestFileCopyingAndMoving()
{
	printf("\n=== File Copying and Moving ===\n");
	printf("=== 文件复制和移动 ===\n");
	
	str sTestDir = "./test_files";
	str sSourceFile = xrtPathJoin(2, sTestDir, "test.txt");
	str sCopyFile = xrtPathJoin(2, sTestDir, "test_copy.txt");
	str sMoveFile = xrtPathJoin(2, sTestDir, "test_moved.txt");
	
	// Copy file
	// 复制文件
	if ( xrtFileExists(sSourceFile) ) {
		// Manual copy implementation
		// 手动复制实现
		size_t iFileSize = 0;
		str sContent = xrtFileReadAll(sSourceFile, XRT_CP_UTF8, &iFileSize);
		
		if ( sContent ) {
			int iCopyResult = xrtFileWriteAll(sCopyFile, sContent, iFileSize, XRT_CP_UTF8);
			printf("File copying:\n");
			printf("  Source: %s\n", sSourceFile);
			printf("  Destination: %s\n", sCopyFile);
			printf("  Copy result: %s\n", iCopyResult ? "SUCCESS" : "FAILED");
			
			xrtFree(sContent);
		}
	}
	
	// Move/rename file
	// 移动/重命名文件
	if ( xrtFileExists(sCopyFile) ) {
		// Manual move implementation using copy + delete
		// 使用复制+删除的手动移动实现
		size_t iFileSize = 0;
		str sContent = xrtFileReadAll(sCopyFile, XRT_CP_UTF8, &iFileSize);
		
		if ( sContent ) {
			int iMoveResult = xrtFileWriteAll(sMoveFile, sContent, iFileSize, XRT_CP_UTF8);
			
			if ( iMoveResult ) {
				// Delete original file
				// 删除原文件
				remove(sCopyFile);
				printf("File moving:\n");
				printf("  Original: %s (deleted)\n", sCopyFile);
				printf("  New location: %s\n", sMoveFile);
			}
			
			xrtFree(sContent);
		}
	}
	
	// Verify operations
	// 验证操作
	bool bCopyExists = xrtFileExists(sCopyFile);
	bool bMoveExists = xrtFileExists(sMoveFile);
	
	printf("Verification:\n");
	printf("  Copy file exists: %s\n", bCopyExists ? "YES" : "NO");
	printf("  Moved file exists: %s\n", bMoveExists ? "YES" : "NO");
	
	// Cleanup
	// 清理
	xrtFree(sSourceFile);
	xrtFree(sCopyFile);
	xrtFree(sMoveFile);
}

// Test directory operations
// 测试目录操作
void TestDirectoryOperations()
{
	printf("\n=== Directory Operations ===\n");
	printf("=== 目录操作 ===\n");
	
	// Create subdirectories
	// 创建子目录
	str sTestDir = "./test_files";
	str sSubDir1 = xrtPathJoin(2, sTestDir, "subdir1");
	str sSubDir2 = xrtPathJoin(2, sTestDir, "subdir2");
	
	if ( !xrtDirExists(sSubDir1) ) {
		xrtDirCreate(sSubDir1);
		printf("Created directory: %s\n", sSubDir1);
	}
	
	if ( !xrtDirExists(sSubDir2) ) {
		xrtDirCreate(sSubDir2);
		printf("Created directory: %s\n", sSubDir2);
	}
	
	// List directory contents
	// 列出目录内容
	printf("Directory listing for %s:\n", sTestDir);
	
	// Manual directory listing (platform dependent)
	// 手动目录列出（平台相关）
#ifdef _WIN32
	system("dir /b test_files");
#else
	system("ls -la test_files");
#endif
	
	// Create files in subdirectories
	// 在子目录中创建文件
	str sFileInSub1 = xrtPathJoin(2, sSubDir1, "file1.txt");
	str sContent1 = "Content of file in subdir1";
	xrtFileWriteAll(sFileInSub1, sContent1, strlen(sContent1), XRT_CP_UTF8);
	printf("Created file in subdir1: %s\n", sFileInSub1);
	
	str sFileInSub2 = xrtPathJoin(2, sSubDir2, "file2.txt");
	str sContent2 = "Content of file in subdir2";
	xrtFileWriteAll(sFileInSub2, sContent2, strlen(sContent2), XRT_CP_UTF8);
	printf("Created file in subdir2: %s\n", sFileInSub2);
	
	// Cleanup
	// 清理
	xrtFree(sSubDir1);
	xrtFree(sSubDir2);
	xrtFree(sFileInSub1);
	xrtFree(sFileInSub2);
	xrtFree(sContent1);
	xrtFree(sContent2);
}

// Test path operations
// 测试路径操作
void TestPathOperations()
{
	printf("\n=== Path Operations ===\n");
	printf("=== 路径操作 ===\n");
	
	// Test path joining
	// 测试路径连接
	str sBasePath = "/home/user";
	str sFileName = "document.txt";
	str sFullPath = xrtPathJoin(2, sBasePath, sFileName);
	
	printf("Path joining:\n");
	printf("  Base: %s\n", sBasePath);
	printf("  File: %s\n", sFileName);
	printf("  Full path: %s\n\n", sFullPath);
	
	// Test path extension extraction
	// 测试路径扩展名提取
	str sPath1 = "/path/to/file.txt";
	str sPath2 = "/path/to/archive.tar.gz";
	str sPath3 = "/path/to/directory/";
	
	// Manual extension extraction
	// 手动扩展名提取
	str sExt1 = strrchr(sPath1, '.');
	str sExt2 = strrchr(sPath2, '.');
	str sExt3 = strrchr(sPath3, '.');
	
	printf("Path extension extraction:\n");
	printf("  %s -> extension: %s\n", sPath1, sExt1 ? sExt1 : "(none)");
	printf("  %s -> extension: %s\n", sPath2, sExt2 ? sExt2 : "(none)");
	printf("  %s -> extension: %s\n\n", sPath3, sExt3 ? sExt3 : "(none)");
	
	// Test basename extraction
	// 测试基名提取
	str sBase1 = strrchr(sPath1, '/');
	if ( sBase1 ) sBase1++; else sBase1 = sPath1;
	
	str sBase2 = strrchr(sPath2, '/');
	if ( sBase2 ) sBase2++; else sBase2 = sPath2;
	
	str sBase3 = strrchr(sPath3, '/');
	if ( sBase3 ) {
		// Remove trailing slash for directory
		// 移除目录的尾部斜杠
		*sBase3 = '\0';
		sBase3 = strrchr(sPath3, '/');
		if ( sBase3 ) sBase3++; else sBase3 = sPath3;
	} else {
		sBase3 = sPath3;
	}
	
	printf("Basename extraction:\n");
	printf("  %s -> basename: %s\n", sPath1, sBase1);
	printf("  %s -> basename: %s\n", sPath2, sBase2);
	printf("  %s -> basename: %s\n\n", sPath3, sBase3);
	
	// Test path normalization
	// 测试路径规范化
	str sRelativePath = "./test/../subdir/./file.txt";
	printf("Path normalization demo:\n");
	printf("  Original: %s\n", sRelativePath);
	printf("  Normalized: (manual implementation needed)\n\n");
	
	// Cleanup
	// 清理
	xrtFree(sFullPath);
}

// Test file attributes and metadata
// 测试文件属性和元数据
void TestFileAttributes()
{
	printf("=== File Attributes and Metadata ===\n");
	printf("=== 文件属性和元数据 ===\n");
	
	str sTestFile = "./test_files/test.txt";
	
	if ( xrtFileExists(sTestFile) ) {
		// Get file size
		// 获取文件大小
		struct stat stFile;
		if ( stat(sTestFile, &stFile) == 0 ) {
			printf("File metadata for %s:\n", sTestFile);
			printf("  Size: %lld bytes\n", (long long)stFile.st_size);
		}
		
		// Get file modification time (platform dependent)
		// 获取文件修改时间（平台相关）
		struct stat st;
		if ( stat(sTestFile, &st) == 0 ) {
			printf("  Last modified: %s", ctime(&st.st_mtime));
#ifdef _WIN32
			printf("  Created: %s", ctime(&st.st_ctime));
#endif
		}
		
		// Check file permissions (basic)
		// 检查文件权限（基本）
		FILE* pFile = fopen(sTestFile, "r");
		if ( pFile ) {
			printf("  Read permission: YES\n");
			fclose(pFile);
		} else {
			printf("  Read permission: NO\n");
		}
		
		pFile = fopen(sTestFile, "w");
		if ( pFile ) {
			printf("  Write permission: YES\n");
			fclose(pFile);
		} else {
			printf("  Write permission: NO\n");
		}
	} else {
		printf("Test file not found: %s\n", sTestFile);
	}
}

// Test practical file operation scenarios
// 测试实际文件操作场景
void TestPracticalScenarios()
{
	printf("\n=== Practical File Operation Scenarios ===\n");
	printf("=== 实际文件操作场景 ===\n");
	
	// Scenario 1: Configuration file management
	// 场景1：配置文件管理
	printf("Scenario 1: Configuration file management\n");
	
	str sConfigDir = "./config";
	if ( !xrtDirExists(sConfigDir) ) {
		xrtDirCreate(sConfigDir);
	}
	
	str sConfigFile = xrtPathJoin(2, sConfigDir, "app.conf");
	
	// Create default configuration if not exists
	// 如果不存在则创建默认配置
	if ( !xrtFileExists(sConfigFile) ) {
		str sDefaultConfig = 
			"# Application Configuration\n"
			"app.name=MyApp\n"
			"app.version=1.0\n"
			"server.port=8080\n"
			"server.host=localhost\n"
			"log.level=INFO\n";
		
		xrtFileWriteAll(sConfigFile, sDefaultConfig, strlen(sDefaultConfig), XRT_CP_UTF8);
		printf("  Created default config: %s\n", sConfigFile);
		xrtFree(sDefaultConfig);
	} else {
		printf("  Config file already exists: %s\n", sConfigFile);
	}
	
	// Read and parse configuration
	// 读取并解析配置
	size_t iConfigSize = 0;
	str sConfigContent = xrtFileReadAll(sConfigFile, XRT_CP_UTF8, &iConfigSize);
	if ( sConfigContent ) {
		printf("  Config content:\n-----\n%s-----\n", sConfigContent);
		xrtFree(sConfigContent);
	}
	
	// Scenario 2: Log file rotation
	// 场景2：日志文件轮转
	printf("Scenario 2: Log file rotation simulation\n");
	
	str sLogDir = "./logs";
	if ( !xrtDirExists(sLogDir) ) {
		xrtDirCreate(sLogDir);
	}
	
	str sLogFile = xrtPathJoin(2, sLogDir, "app.log");
	
	// Simulate writing log entries
	// 模拟写入日志条目
	time_t tNow = time(NULL);
	str sTimestamp = ctime(&tNow);
	sTimestamp[strlen(sTimestamp)-1] = '\0';  // Remove newline
	
	str sLogEntry = xrtFormat("[%s] INFO: Application started\n", sTimestamp);
	
	// Append to log file
	// 追加到日志文件
	FILE* pLogFile = fopen(sLogFile, "a");
	if ( pLogFile ) {
		fputs(sLogEntry, pLogFile);
		fclose(pLogFile);
		printf("  Added log entry to: %s\n", sLogFile);
	}
	
	xrtFree(sLogEntry);
	
	// Scenario 3: Backup file creation
	// 场景3：备份文件创建
	printf("Scenario 3: Backup file creation\n");
	
	if ( xrtFileExists(sConfigFile) ) {
		// Create backup with timestamp
		// 创建带时间戳的备份
		time_t tBackup = time(NULL);
		struct tm* ptm = localtime(&tBackup);
		str sBackupName = xrtFormat("app_backup_%04d%02d%02d_%02d%02d%02d.conf",
		                           ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
		                           ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		
		str sBackupFile = xrtPathJoin(2, sConfigDir, sBackupName);
		
		// Copy config to backup
		// 复制配置到备份
		size_t iOrigSize = 0;
		str sOrigContent = xrtFileReadAll(sConfigFile, XRT_CP_UTF8, &iOrigSize);
		if ( sOrigContent ) {
			xrtFileWriteAll(sBackupFile, sOrigContent, iOrigSize, XRT_CP_UTF8);
			printf("  Created backup: %s\n", sBackupFile);
			xrtFree(sOrigContent);
		}
		
		xrtFree(sBackupName);
		xrtFree(sBackupFile);
	}
	
	// Cleanup
	// 清理
	xrtFree(sConfigDir);
	xrtFree(sConfigFile);
	xrtFree(sLogDir);
	xrtFree(sLogFile);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File Basic Operations Demo\n");
	printf("XRT 文件基本操作演示\n");
	printf("============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestFileCreationAndWriting();
	TestFileReadingAndInfo();
	TestFileCopyingAndMoving();
	TestDirectoryOperations();
	TestPathOperations();
	TestFileAttributes();
	TestPracticalScenarios();
	
	// Note: Not cleaning up test files to allow inspection
	// 注意：不清除测试文件以便检查
	printf("\nTest files remain in ./test_files/ directory for inspection\n");
	printf("测试文件保留在 ./test_files/ 目录中供检查\n");
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}