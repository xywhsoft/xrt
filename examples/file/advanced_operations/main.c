/*
 * XRT Example - File Advanced Operations
 * XRT 范例 - 文件高级操作
 *
 * Description / 说明:
 *   EN: Demonstrates advanced file operations including file locking, temporary files,
 *       memory mapping, file monitoring, and atomic operations.
 *   CN: 演示高级文件操作，包括文件锁定、临时文件、内存映射、
 *       文件监控和原子操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_advanced_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_advanced_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Platform-specific advanced file operations
 *   - File locking mechanisms for concurrent access
 *   - Temporary file creation and cleanup
 *   - File change monitoring (platform dependent)
 *   - Atomic file operations for data consistency
 *   - Memory-mapped file access
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Test file locking mechanisms
// 测试文件锁定机制
void TestFileLocking()
{
	printf("=== File Locking Mechanisms ===\n");
	printf("=== 文件锁定机制 ===\n");
	
	str sTestFile = "./test_files/locked_file.txt";
	
	// Create test file
	// 创建测试文件
	FILE* pFile = fopen(sTestFile, "w");
	if ( pFile ) {
		fprintf(pFile, "This file will be locked for exclusive access.\n");
		fclose(pFile);
	}
	
#ifdef _WIN32
	// Windows file locking
	// Windows 文件锁定
	HANDLE hFile = CreateFileA(sTestFile, 
	                          GENERIC_READ | GENERIC_WRITE,
	                          0,  // No sharing - exclusive access
	                          NULL,
	                          OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL,
	                          NULL);
	
	if ( hFile != INVALID_HANDLE_VALUE ) {
		// Lock the entire file
		// 锁定整个文件
		OVERLAPPED ov = {0};
		if ( LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov) ) {
			printf("Windows: File locked successfully: %s\n", sTestFile);
			
			// Simulate work with locked file
			// 模拟使用锁定文件的工作
			Sleep(1000);
			
			// Unlock file
			// 解锁文件
			UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov);
			printf("Windows: File unlocked\n");
		}
		
		CloseHandle(hFile);
	}
#else
	// POSIX file locking
	// POSIX 文件锁定
	int fd = open(sTestFile, O_RDWR);
	if ( fd != -1 ) {
		struct flock fl = {0};
		fl.l_type = F_WRLCK;    // Write lock
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;           // Lock entire file
		
		if ( fcntl(fd, F_SETLK, &fl) != -1 ) {
			printf("POSIX: File locked successfully: %s\n", sTestFile);
			
			// Simulate work with locked file
			// 模拟使用锁定文件的工作
			sleep(1);
			
			// Unlock file
			// 解锁文件
			fl.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &fl);
			printf("POSIX: File unlocked\n");
		}
		
		close(fd);
	}
#endif
	
	// Test concurrent access prevention
	// 测试并发访问阻止
	printf("Testing concurrent access...\n");
	
	// Try to open locked file from another process/thread
	// 尝试从另一个进程/线程打开锁定的文件
	FILE* pTestFile = fopen(sTestFile, "r");
	if ( pTestFile ) {
		printf("  Another process can still read the file (advisory locking)\n");
		fclose(pTestFile);
	} else {
		printf("  File access blocked by locking mechanism\n");
	}
}

// Test temporary file operations
// 测试临时文件操作
void TestTemporaryFiles()
{
	printf("\n=== Temporary File Operations ===\n");
	printf("=== 临时文件操作 ===\n");
	
	// Method 1: Standard library tmpfile
	// 方法1：标准库 tmpfile
	FILE* pTmpFile1 = tmpfile();
	if ( pTmpFile1 ) {
		fprintf(pTmpFile1, "This is temporary file content 1\n");
		fflush(pTmpFile1);
		
		// Get file descriptor for size check
		// 获取文件描述符以检查大小
#ifdef _WIN32
		int fd = _fileno(pTmpFile1);
#else
		int fd = fileno(pTmpFile1);
#endif
		fseek(pTmpFile1, 0, SEEK_END);
		long iSize1 = ftell(pTmpFile1);
		rewind(pTmpFile1);
		
		printf("Method 1 - tmpfile():\n");
		printf("  File descriptor: %d\n", fd);
		printf("  File size: %ld bytes\n", iSize1);
		
		// Read content back
		// 读回内容
		char sBuffer[256];
		if ( fgets(sBuffer, sizeof(sBuffer), pTmpFile1) ) {
			printf("  Content: %s", sBuffer);
		}
		
		// tmpfile() automatically deletes on close
		// tmpfile() 关闭时自动删除
		fclose(pTmpFile1);
		printf("  File automatically deleted on close\n");
	}
	
	// Method 2: Manual temporary file with unique name
	// 方法2：使用唯一名称的手动临时文件
	char sTmpPath[256];
#ifdef _WIN32
	GetTempPathA(sizeof(sTmpPath), sTmpPath);
	char sTmpName[MAX_PATH];
	GetTempFileNameA(sTmpPath, "xrt", 0, sTmpName);
#else
	strcpy(sTmpPath, "/tmp/");
	sprintf(sTmpName, "/tmp/xrt_temp_%d.tmp", getpid());
#endif
	
	FILE* pTmpFile2 = fopen(sTmpName, "w+");
	if ( pTmpFile2 ) {
		fprintf(pTmpFile2, "This is temporary file content 2\n");
		fprintf(pTmpFile2, "Created at: %s", ctime(NULL));
		fflush(pTmpFile2);
		
		fseek(pTmpFile2, 0, SEEK_END);
		long iSize2 = ftell(pTmpFile2);
		rewind(pTmpFile2);
		
		printf("Method 2 - Named temporary file:\n");
		printf("  File path: %s\n", sTmpName);
		printf("  File size: %ld bytes\n", iSize2);
		
		// Read content back
		// 读回内容
		char sBuffer2[256];
		while ( fgets(sBuffer2, sizeof(sBuffer2), pTmpFile2) ) {
			printf("  Content: %s", sBuffer2);
		}
		
		fclose(pTmpFile2);
		
		// Manually delete temporary file
		// 手动删除临时文件
		if ( remove(sTmpName) == 0 ) {
			printf("  File manually deleted\n");
		} else {
			printf("  Failed to delete temporary file\n");
		}
	}
	
	// Method 3: In-memory temporary storage
	// 方法3：内存中临时存储
	printf("Method 3 - In-memory temporary storage:\n");
	
	// Simulate temporary data that doesn't need file system
	// 模拟不需要文件系统的临时数据
	str sTempData = xrtMalloc(1024);
	strcpy(sTempData, "Temporary in-memory data\n");
	strcat(sTempData, "This data exists only in RAM\n");
	strcat(sTempData, "No file system I/O required\n");
	
	size_t iDataLen = strlen(sTempData);
	printf("  Memory allocated: %zu bytes\n", iDataLen);
	printf("  Content:\n%s", sTempData);
	
	// Free when done
	// 完成后释放
	xrtFree(sTempData);
	printf("  Memory freed\n");
}

// Test file change monitoring
// 测试文件变更监控
void TestFileMonitoring()
{
	printf("\n=== File Change Monitoring ===\n");
	printf("=== 文件变更监控 ===\n");
	
	str sMonitorFile = "./test_files/monitor_file.txt";
	
	// Create monitor file
	// 创建监控文件
	FILE* pFile = fopen(sMonitorFile, "w");
	if ( pFile ) {
		fprintf(pFile, "Initial content\n");
		fclose(pFile);
		printf("Created monitor file: %s\n", sMonitorFile);
	}
	
#ifdef _WIN32
	// Windows file change notification
	// Windows 文件变更通知
	HANDLE hDir = CreateFileA("./test_files",
	                         FILE_LIST_DIRECTORY,
	                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                         NULL,
	                         OPEN_EXISTING,
	                         FILE_FLAG_BACKUP_SEMANTICS,
	                         NULL);
	
	if ( hDir != INVALID_HANDLE_VALUE ) {
		char sBuffer[1024];
		DWORD dwBytesReturned;
		
		printf("Windows: Monitoring directory for changes...\n");
		printf("  (Monitoring for 3 seconds - make changes to test_files/)\n");
		
		// Monitor for a short time
		// 短时间监控
		time_t tStart = time(NULL);
		while ( time(NULL) - tStart < 3 ) {
			if ( ReadDirectoryChangesW(hDir,
			                          sBuffer,
			                          sizeof(sBuffer),
			                          TRUE,
			                          FILE_NOTIFY_CHANGE_FILE_NAME |
			                          FILE_NOTIFY_CHANGE_DIR_NAME |
			                          FILE_NOTIFY_CHANGE_ATTRIBUTES |
			                          FILE_NOTIFY_CHANGE_SIZE |
			                          FILE_NOTIFY_CHANGE_LAST_WRITE,
			                          &dwBytesReturned,
			                          NULL,
			                          NULL) ) {
				// Process changes
				// 处理变更
				FILE_NOTIFY_INFORMATION* pInfo = (FILE_NOTIFY_INFORMATION*)sBuffer;
				do {
					switch ( pInfo->Action ) {
						case FILE_ACTION_ADDED:
							printf("  File added\n");
							break;
						case FILE_ACTION_REMOVED:
							printf("  File removed\n");
							break;
						case FILE_ACTION_MODIFIED:
							printf("  File modified\n");
							break;
						case FILE_ACTION_RENAMED_OLD_NAME:
							printf("  File renamed (old name)\n");
							break;
						case FILE_ACTION_RENAMED_NEW_NAME:
							printf("  File renamed (new name)\n");
							break;
					}
					
					if ( pInfo->NextEntryOffset == 0 ) break;
					pInfo = (FILE_NOTIFY_INFORMATION*)((char*)pInfo + pInfo->NextEntryOffset);
				} while ( TRUE );
			}
		}
		
		CloseHandle(hDir);
	}
#else
	// Linux file monitoring using inotify
	// Linux 使用 inotify 的文件监控
#ifdef __linux__
	int fd = inotify_init();
	if ( fd != -1 ) {
		int wd = inotify_add_watch(fd, "./test_files",
		                          IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
		
		if ( wd != -1 ) {
			char sBuffer[4096];
			
			printf("Linux: Monitoring directory for changes...\n");
			printf("  (Monitoring for 3 seconds - make changes to test_files/)\n");
			
			// Monitor for a short time
			// 短时间监控
			fd_set rfds;
			struct timeval tv;
			
			time_t tStart = time(NULL);
			while ( time(NULL) - tStart < 3 ) {
				FD_ZERO(&rfds);
				FD_SET(fd, &rfds);
				tv.tv_sec = 0;
				tv.tv_usec = 100000;  // 100ms timeout
				
				int iResult = select(fd + 1, &rfds, NULL, NULL, &tv);
				if ( iResult > 0 && FD_ISSET(fd, &rfds) ) {
					int iLength = read(fd, sBuffer, sizeof(sBuffer));
					if ( iLength > 0 ) {
						int i = 0;
						while ( i < iLength ) {
							struct inotify_event* pevent = (struct inotify_event*)&sBuffer[i];
							
							if ( pevent->len ) {
								if ( pevent->mask & IN_CREATE ) {
									printf("  File created: %s\n", pevent->name);
								}
								if ( pevent->mask & IN_DELETE ) {
									printf("  File deleted: %s\n", pevent->name);
								}
								if ( pevent->mask & IN_MODIFY ) {
									printf("  File modified: %s\n", pevent->name);
								}
							}
							i += sizeof(struct inotify_event) + pevent->len;
						}
					}
				}
			}
			
			inotify_rm_watch(fd, wd);
		}
		close(fd);
	}
#else
	printf("File monitoring not implemented for this platform\n");
#endif
#endif
}

// Test atomic file operations
// 测试原子文件操作
void TestAtomicOperations()
{
	printf("\n=== Atomic File Operations ===\n");
	printf("=== 原子文件操作 ===\n");
	
	// Atomic write operation
	// 原子写入操作
	str sAtomicFile = "./test_files/atomic_data.txt";
	
	// Method: Write to temp file, then rename (atomic on most filesystems)
	// 方法：写入临时文件，然后重命名（在大多数文件系统上是原子的）
	str sTempFile = "./test_files/atomic_data.tmp";
	
	// Create and write to temporary file
	// 创建并写入临时文件
	FILE* pTemp = fopen(sTempFile, "w");
	if ( pTemp ) {
		time_t tNow = time(NULL);
		fprintf(pTemp, "Atomic data written at: %s", ctime(&tNow));
		fprintf(pTemp, "Process ID: %d\n", 
#ifdef _WIN32
		        GetCurrentProcessId()
#else
		        getpid()
#endif
		       );
		fclose(pTemp);
		
		printf("Created temporary file: %s\n", sTempFile);
	}
	
	// Atomically replace the original file
	// 原子地替换原文件
#ifdef _WIN32
	// Windows: Remove old file first, then rename
	// Windows：先删除旧文件，再重命名
	remove(sAtomicFile);
	if ( rename(sTempFile, sAtomicFile) == 0 ) {
		printf("Atomic replacement completed: %s\n", sAtomicFile);
	}
#else
	// POSIX: rename() is atomic on same filesystem
	// POSIX：在同一文件系统上 rename() 是原子的
	if ( rename(sTempFile, sAtomicFile) == 0 ) {
		printf("Atomic replacement completed: %s\n", sAtomicFile);
	}
#endif
	
	// Verify atomic operation
	// 验证原子操作
	FILE* pVerify = fopen(sAtomicFile, "r");
	if ( pVerify ) {
		char sLine[256];
		printf("Verification - file content:\n");
		while ( fgets(sLine, sizeof(sLine), pVerify) ) {
			printf("  %s", sLine);
		}
		fclose(pVerify);
	}
	
	// Test concurrent atomic writes
	// 测试并发原子写入
	printf("Testing concurrent atomic writes...\n");
	
	// Simulate multiple processes trying to write atomically
	// 模拟多个进程尝试原子写入
	for ( int i = 0; i < 3; i++ ) {
		str sConcurrentTemp = xrtFormat("./test_files/concurrent_%d.tmp", i);
		str sConcurrentFile = "./test_files/concurrent_final.txt";
		
		FILE* pConcurrent = fopen(sConcurrentTemp, "w");
		if ( pConcurrent ) {
			fprintf(pConcurrent, "Concurrent write %d at %s", i, ctime(NULL));
			fclose(pConcurrent);
			
#ifdef _WIN32
			remove(sConcurrentFile);
			rename(sConcurrentTemp, sConcurrentFile);
#else
			rename(sConcurrentTemp, sConcurrentFile);
#endif
			
			printf("  Concurrent write %d completed\n", i);
		}
		
		xrtFree(sConcurrentTemp);
	}
}

// Test memory-mapped file access
// 测试内存映射文件访问
void TestMemoryMappedFiles()
{
	printf("\n=== Memory-Mapped File Access ===\n");
	printf("=== 内存映射文件访问 ===\n");
	
	str sMappedFile = "./test_files/mapped_file.dat";
	size_t iFileSize = 4096;  // 4KB
	
	// Create file with known content
	// 创建具有已知内容的文件
	FILE* pFile = fopen(sMappedFile, "w+b");
	if ( pFile ) {
		// Write pattern data
		// 写入模式数据
		for ( size_t i = 0; i < iFileSize; i++ ) {
			fputc(i % 256, pFile);
		}
		fclose(pFile);
		printf("Created mapped file: %s (%zu bytes)\n", sMappedFile, iFileSize);
	}
	
#ifdef _WIN32
	// Windows memory mapping
	// Windows 内存映射
	HANDLE hFile = CreateFileA(sMappedFile,
	                          GENERIC_READ | GENERIC_WRITE,
	                          FILE_SHARE_READ,
	                          NULL,
	                          OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL,
	                          NULL);
	
	if ( hFile != INVALID_HANDLE_VALUE ) {
		HANDLE hMapping = CreateFileMappingA(hFile,
		                                    NULL,
		                                    PAGE_READWRITE,
		                                    0,
		                                    iFileSize,
		                                    NULL);
		
		if ( hMapping ) {
			void* pMapped = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, iFileSize);
			
			if ( pMapped ) {
				printf("Windows: File mapped to memory at %p\n", pMapped);
				
				// Access mapped memory
				// 访问映射内存
				unsigned char* pData = (unsigned char*)pMapped;
				
				// Read some data
				// 读取一些数据
				printf("  First 16 bytes: ");
				for ( int i = 0; i < 16; i++ ) {
					printf("%02X ", pData[i]);
				}
				printf("\n");
				
				// Modify some data
				// 修改一些数据
				pData[0] = 0xFF;
				pData[1] = 0xEE;
				pData[2] = 0xDD;
				
				printf("  Modified first 3 bytes to: %02X %02X %02X\n", 
				       pData[0], pData[1], pData[2]);
				
				// Unmap and cleanup
				// 取消映射并清理
				UnmapViewOfFile(pMapped);
				printf("Windows: Memory unmapped\n");
			}
			
			CloseHandle(hMapping);
		}
		
		CloseHandle(hFile);
	}
#else
	// POSIX memory mapping
	// POSIX 内存映射
	int fd = open(sMappedFile, O_RDWR);
	if ( fd != -1 ) {
		void* pMapped = mmap(NULL, iFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		
		if ( pMapped != MAP_FAILED ) {
			printf("POSIX: File mapped to memory at %p\n", pMapped);
			
			// Access mapped memory
			// 访问映射内存
			unsigned char* pData = (unsigned char*)pMapped;
			
			// Read some data
			// 读取一些数据
			printf("  First 16 bytes: ");
			for ( int i = 0; i < 16; i++ ) {
				printf("%02X ", pData[i]);
			}
			printf("\n");
			
			// Modify some data
			// 修改一些数据
			pData[0] = 0xFF;
			pData[1] = 0xEE;
			pData[2] = 0xDD;
			
			printf("  Modified first 3 bytes to: %02X %02X %02X\n", 
			       pData[0], pData[1], pData[2]);
			
			// Synchronize changes
			// 同步更改
			msync(pMapped, iFileSize, MS_SYNC);
			printf("POSIX: Changes synchronized to disk\n");
			
			// Unmap and cleanup
			// 取消映射并清理
			munmap(pMapped, iFileSize);
			printf("POSIX: Memory unmapped\n");
		}
		
		close(fd);
	}
#endif
	
	// Verify changes persisted
	// 验证更改已持久化
	FILE* pVerify = fopen(sMappedFile, "rb");
	if ( pVerify ) {
		unsigned char sVerify[16];
		fread(sVerify, 1, 16, pVerify);
		fclose(pVerify);
		
		printf("Verification - first 16 bytes from file:\n  ");
		for ( int i = 0; i < 16; i++ ) {
			printf("%02X ", sVerify[i]);
		}
		printf("\n");
	}
}

// Test file system operations
// 测试文件系统操作
void TestFileSystemOperations()
{
	printf("\n=== File System Operations ===\n");
	printf("=== 文件系统操作 ===\n");
	
	// Get current working directory
	// 获取当前工作目录
	char sCwd[1024];
#ifdef _WIN32
	if ( GetCurrentDirectoryA(sizeof(sCwd), sCwd) ) {
		printf("Current working directory: %s\n", sCwd);
	}
#else
	if ( getcwd(sCwd, sizeof(sCwd)) ) {
		printf("Current working directory: %s\n", sCwd);
	}
#endif
	
	// Get available disk space
	// 获取可用磁盘空间
#ifdef _WIN32
	ULARGE_INTEGER ulFreeBytesAvailable;
	ULARGE_INTEGER ulTotalNumberOfBytes;
	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	
	if ( GetDiskFreeSpaceExA(".", &ulFreeBytesAvailable, &ulTotalNumberOfBytes, &ulTotalNumberOfFreeBytes) ) {
		printf("Disk space information:\n");
		printf("  Total space: %.2f GB\n", ulTotalNumberOfBytes.QuadPart / (1024.0 * 1024.0 * 1024.0));
		printf("  Free space: %.2f GB\n", ulTotalNumberOfFreeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0));
		printf("  Available to caller: %.2f GB\n", ulFreeBytesAvailable.QuadPart / (1024.0 * 1024.0 * 1024.0));
	}
#else
	struct statvfs svfs;
	if ( statvfs(".", &svfs) == 0 ) {
		unsigned long long iTotal = (unsigned long long)svfs.f_blocks * svfs.f_frsize;
		unsigned long long iFree = (unsigned long long)svfs.f_bavail * svfs.f_frsize;
		
		printf("Disk space information:\n");
		printf("  Total space: %.2f GB\n", iTotal / (1024.0 * 1024.0 * 1024.0));
		printf("  Free space: %.2f GB\n", iFree / (1024.0 * 1024.0 * 1024.0));
	}
#endif
	
	// Test symbolic links (if supported)
	// 测试符号链接（如果支持）
#ifdef _WIN32
	// Windows: Create junction point or symbolic link
	// Windows：创建联结点或符号链接
	str sLinkTarget = "./test_files";
	str sLinkName = "./test_link";
	
	// Note: Requires administrator privileges or developer mode
	// 注意：需要管理员权限或开发者模式
	printf("Symbolic link creation: Requires elevated privileges on Windows\n");
#else
	// POSIX: Create symbolic link
	// POSIX：创建符号链接
	str sLinkTarget = "./test_files";
	str sLinkName = "./test_link";
	
	if ( symlink(sLinkTarget, sLinkName) == 0 ) {
		printf("Created symbolic link: %s -> %s\n", sLinkName, sLinkTarget);
		
		// Verify link
		// 验证链接
		char sResolved[1024];
		if ( realpath(sLinkName, sResolved) ) {
			printf("  Resolves to: %s\n", sResolved);
		}
		
		// Clean up
		// 清理
		unlink(sLinkName);
		printf("  Symbolic link removed\n");
	}
#endif
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File Advanced Operations Demo\n");
	printf("XRT 文件高级操作演示\n");
	printf("===============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestFileLocking();
	TestTemporaryFiles();
	TestFileMonitoring();
	TestAtomicOperations();
	TestMemoryMappedFiles();
	TestFileSystemOperations();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}