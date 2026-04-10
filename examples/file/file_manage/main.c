/*
 * XRT Example - File Management
 * XRT 范例 - 文件管理
 *
 * Description / 说明:
 *   EN: Demonstrates xrtFileCopy, xrtFileMove, xrtFileDelete for
 *       file management operations.
 *   CN: 演示 xrtFileCopy、xrtFileMove、xrtFileDelete 进行文件管理操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_file_manage.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_file_manage              (Linux, GCC)
 *
 * Note / 注意:
 *   - bReWrite=TRUE overwrites existing destination
 *   - All functions return TRUE on success
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test file copy
// 测试文件复制
void TestFileCopy()
{
	printf("=== File Copy ===\n");
	printf("=== 文件复制 ===\n");
	
	// Create source file
	// 创建源文件
	str sSrcPath = "examples/bin/source.txt";
	str sContent = "Original content.\n原始内容。\n";
	xrtFileWriteAll(sSrcPath, sContent, strlen(sContent), 0);
	printf("Created source: %s\n", sSrcPath);
	printf("创建源文件: %s\n", sSrcPath);
	
	// Copy to destination
	// 复制到目标
	str sDstPath = "examples/bin/destination.txt";
	bool bResult = xrtFileCopy(sSrcPath, sDstPath, TRUE);
	
	if ( bResult ) {
		printf("Copied to: %s\n", sDstPath);
		printf("复制到: %s\n", sDstPath);
		
		// Verify
		// 验证
		str sCopied = xrtFileReadAll(sDstPath, 0, NULL);
		printf("Destination content: %s", sCopied);
		printf("目标内容: %s", sCopied);
		xrtFree(sCopied);
	} else {
		printf("Copy failed.\n");
		printf("复制失败。\n");
	}
	printf("\n");
}

// Test file move
// 测试文件移动
void TestFileMove()
{
	printf("=== File Move ===\n");
	printf("=== 文件移动 ===\n");
	
	// Create file to move
	// 创建要移动的文件
	str sSrcPath = "examples/bin/to_move.txt";
	str sContent = "File to be moved.\n要移动的文件。\n";
	xrtFileWriteAll(sSrcPath, sContent, strlen(sContent), 0);
	printf("Created: %s\n", sSrcPath);
	printf("创建: %s\n", sSrcPath);
	printf("Exists before move: %s\n", xrtPathExists(sSrcPath) ? "YES" : "NO");
	printf("移动前存在: %s\n", xrtPathExists(sSrcPath) ? "是" : "否");
	
	// Move file
	// 移动文件
	str sDstPath = "examples/bin/moved_file.txt";
	bool bResult = xrtFileMove(sSrcPath, sDstPath, TRUE);
	
	if ( bResult ) {
		printf("Moved to: %s\n", sDstPath);
		printf("移动到: %s\n", sDstPath);
		printf("Source exists after move: %s\n", xrtPathExists(sSrcPath) ? "YES" : "NO");
		printf("移动后源文件存在: %s\n", xrtPathExists(sSrcPath) ? "是" : "否");
		printf("Destination exists: %s\n", xrtPathExists(sDstPath) ? "YES" : "NO");
		printf("目标存在: %s\n", xrtPathExists(sDstPath) ? "是" : "否");
	} else {
		printf("Move failed.\n");
		printf("移动失败。\n");
	}
	printf("\n");
}

// Test file delete
// 测试文件删除
void TestFileDelete()
{
	printf("=== File Delete ===\n");
	printf("=== 文件删除 ===\n");
	
	// Create file to delete
	// 创建要删除的文件
	str sPath = "examples/bin/to_delete.txt";
	str sContent = "This file will be deleted.\n此文件将被删除。\n";
	xrtFileWriteAll(sPath, sContent, strlen(sContent), 0);
	printf("Created: %s\n", sPath);
	printf("创建: %s\n", sPath);
	printf("Exists before delete: %s\n", xrtPathExists(sPath) ? "YES" : "NO");
	printf("删除前存在: %s\n", xrtPathExists(sPath) ? "是" : "否");
	
	// Delete file
	// 删除文件
	bool bResult = xrtFileDelete(sPath);
	
	if ( bResult ) {
		printf("Deleted successfully.\n");
		printf("删除成功。\n");
		printf("Exists after delete: %s\n", xrtPathExists(sPath) ? "YES" : "NO");
		printf("删除后存在: %s\n", xrtPathExists(sPath) ? "是" : "否");
	} else {
		printf("Delete failed.\n");
		printf("删除失败。\n");
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Backup file before modification
	// 修改前备份文件
	str sOriginalPath = "examples/bin/config_backup.txt";
	str sBackupPath = "examples/bin/config_backup.bak";
	
	// Create original
	// 创建原始文件
	str sConfig = "setting1=value1\nsetting2=value2\n";
	xrtFileWriteAll(sOriginalPath, sConfig, strlen(sConfig), 0);
	printf("Created config: %s\n", sOriginalPath);
	printf("创建配置: %s\n", sOriginalPath);
	
	// Create backup
	// 创建备份
	if ( xrtFileCopy(sOriginalPath, sBackupPath, TRUE) ) {
		printf("Backup created: %s\n", sBackupPath);
		printf("备份已创建: %s\n", sBackupPath);
	}
	
	// Modify original
	// 修改原始文件
	str sNewConfig = "setting1=newvalue1\nsetting2=newvalue2\nsetting3=value3\n";
	xrtFileWriteAll(sOriginalPath, sNewConfig, strlen(sNewConfig), 0);
	printf("Config updated.\n");
	printf("配置已更新。\n\n");
	
	// Restore from backup
	// 从备份恢复
	printf("Restoring from backup...\n");
	printf("从备份恢复...\n");
	if ( xrtFileCopy(sBackupPath, sOriginalPath, TRUE) ) {
		printf("Restored from backup.\n");
		printf("已从备份恢复。\n");
	}
	printf("\n");
	
	// Clean up old files
	// 清理旧文件
	printf("Cleaning up temporary files...\n");
	printf("清理临时文件...\n");
	xrtFileDelete(sBackupPath);
	printf("Done.\n");
	printf("完成。\n");
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File - File Management Demo\n");
	printf("XRT 文件 - 文件管理演示\n");
	printf("================================\n\n");
	
	TestFileCopy();
	TestFileMove();
	TestFileDelete();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
