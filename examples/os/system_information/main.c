/*
 * XRT Example - System Information
 * XRT 范例 - 系统信息获取
 *
 * Description / 说明:
 *   EN: Demonstrates comprehensive system information retrieval including
 *       hardware details, operating system information, memory statistics,
 *       CPU information, disk space, network interfaces, and process information.
 *   CN: 演示全面的系统信息检索，包括硬件详情、操作系统信息、内存统计、
 *       CPU信息、磁盘空间、网络接口和进程信息。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/os_system_information.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/os_system_information -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Cross-platform system information gathering
 *   - Hardware and software inventory
 *   - Performance monitoring capabilities
 *   - Resource utilization statistics
 *   - System health checking
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#include <dirent.h>
#endif

// System information structure
// 系统信息结构
typedef struct {
	// Hardware info / 硬件信息
	str sCpuModel;
	int iCpuCores;
	int iCpuThreads;
	unsigned long long ullTotalMemory;
	unsigned long long ullFreeMemory;
	
	// OS info / 操作系统信息
	str sOsName;
	str sOsVersion;
	str sArchitecture;
	
	// Disk info / 磁盘信息
	unsigned long long ullTotalDiskSpace;
	unsigned long long ullFreeDiskSpace;
	
	// Network info / 网络信息
	int iNetworkInterfaces;
	str sPrimaryIpAddress;
	
	// Process info / 进程信息
	int iProcessCount;
	unsigned long ulCurrentProcessId;
} SystemInfo;

// Get CPU information
// 获取 CPU 信息
void GetCpuInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows CPU info
	// Windows CPU 信息
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	
	pInfo->iCpuCores = sysInfo.dwNumberOfProcessors;
	pInfo->iCpuThreads = sysInfo.dwNumberOfProcessors;
	
	// Get CPU model from registry (simplified)
	// 从注册表获取 CPU 型号（简化）
	HKEY hKey;
	if ( RegOpenKeyExA(HKEY_LOCAL_MACHINE,
	                   "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
	                   0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		char sBuffer[256];
		DWORD dwSize = sizeof(sBuffer);
		if ( RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
		                     (LPBYTE)sBuffer, &dwSize) == ERROR_SUCCESS ) {
			pInfo->sCpuModel = xrtCopyStr(sBuffer, strlen(sBuffer) + 1);
		} else {
			pInfo->sCpuModel = xrtCopyStr("Unknown CPU", 12);
		}
		RegCloseKey(hKey);
	} else {
		pInfo->sCpuModel = xrtCopyStr("Unknown CPU", 12);
	}
#else
	// Linux CPU info
	// Linux CPU 信息
	FILE* pFile = fopen("/proc/cpuinfo", "r");
	if ( pFile ) {
		char sLine[256];
		while ( fgets(sLine, sizeof(sLine), pFile) ) {
			if ( strncmp(sLine, "model name", 10) == 0 ) {
				char* sColon = strchr(sLine, ':');
				if ( sColon ) {
					char* sModel = sColon + 2;
					sModel[strcspn(sModel, "\n")] = '\0';
					pInfo->sCpuModel = xrtCopyStr(sModel, strlen(sModel) + 1);
					break;
				}
			}
		}
		fclose(pFile);
	} else {
		pInfo->sCpuModel = xrtCopyStr("Unknown CPU", 12);
	}
	
	// Get CPU cores
	// 获取 CPU 核心数
	pInfo->iCpuCores = sysconf(_SC_NPROCESSORS_ONLN);
	pInfo->iCpuThreads = pInfo->iCpuCores;
#endif
}

// Get memory information
// 获取内存信息
void GetMemoryInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows memory info
	// Windows 内存信息
	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);
	GlobalMemoryStatusEx(&memStatus);
	
	pInfo->ullTotalMemory = memStatus.ullTotalPhys;
	pInfo->ullFreeMemory = memStatus.ullAvailPhys;
#else
	// Linux memory info
	// Linux 内存信息
	struct sysinfo si;
	sysinfo(&si);
	
	pInfo->ullTotalMemory = si.totalram * si.mem_unit;
	pInfo->ullFreeMemory = si.freeram * si.mem_unit;
#endif
}

// Get OS information
// 获取操作系统信息
void GetOsInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows OS info
	// Windows 操作系统信息
	OSVERSIONINFOA osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	GetVersionExA(&osvi);
	
	pInfo->sOsName = xrtCopyStr("Windows", 8);
	pInfo->sOsVersion = xrtFormat("%d.%d.%d",
	                             osvi.dwMajorVersion,
	                             osvi.dwMinorVersion,
	                             osvi.dwBuildNumber);
	
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	
	switch ( sysInfo.wProcessorArchitecture ) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			pInfo->sArchitecture = xrtCopyStr("x64", 4);
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			pInfo->sArchitecture = xrtCopyStr("x86", 4);
			break;
		default:
			pInfo->sArchitecture = xrtCopyStr("Unknown", 8);
			break;
	}
#else
	// Linux OS info
	// Linux 操作系统信息
	struct utsname unameData;
	uname(&unameData);
	
	pInfo->sOsName = xrtCopyStr(unameData.sysname, strlen(unameData.sysname) + 1);
	pInfo->sOsVersion = xrtCopyStr(unameData.release, strlen(unameData.release) + 1);
	pInfo->sArchitecture = xrtCopyStr(unameData.machine, strlen(unameData.machine) + 1);
#endif
}

// Get disk space information
// 获取磁盘空间信息
void GetDiskInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows disk info
	// Windows 磁盘信息
	ULARGE_INTEGER ulFreeBytesAvailable;
	ULARGE_INTEGER ulTotalNumberOfBytes;
	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	
	if ( GetDiskFreeSpaceExA("C:\\", &ulFreeBytesAvailable,
	                        &ulTotalNumberOfBytes, &ulTotalNumberOfFreeBytes) ) {
		pInfo->ullTotalDiskSpace = ulTotalNumberOfBytes.QuadPart;
		pInfo->ullFreeDiskSpace = ulTotalNumberOfFreeBytes.QuadPart;
	} else {
		pInfo->ullTotalDiskSpace = 0;
		pInfo->ullFreeDiskSpace = 0;
	}
#else
	// Linux disk info
	// Linux 磁盘信息
	struct statvfs svfs;
	if ( statvfs("/", &svfs) == 0 ) {
		pInfo->ullTotalDiskSpace = (unsigned long long)svfs.f_blocks * svfs.f_frsize;
		pInfo->ullFreeDiskSpace = (unsigned long long)svfs.f_bavail * svfs.f_frsize;
	} else {
		pInfo->ullTotalDiskSpace = 0;
		pInfo->ullFreeDiskSpace = 0;
	}
#endif
}

// Get network information
// 获取网络信息
void GetNetworkInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows network info
	// Windows 网络信息
	pInfo->iNetworkInterfaces = 0;
	pInfo->sPrimaryIpAddress = xrtCopyStr("127.0.0.1", 10);
	
	// Count network adapters
	// 计算网络适配器
	ULONG ulOutBufLen = 15000;
	PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)xrtMalloc(ulOutBufLen);
	
	if ( GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR ) {
		PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
		while ( pAdapter ) {
			pInfo->iNetworkInterfaces++;
			pAdapter = pAdapter->Next;
		}
	}
	
	xrtFree(pAdapterInfo);
#else
	// Linux network info
	// Linux 网络信息
	pInfo->iNetworkInterfaces = 0;
	pInfo->sPrimaryIpAddress = xrtCopyStr("127.0.0.1", 10);
	
	DIR* pDir = opendir("/sys/class/net");
	if ( pDir ) {
		struct dirent* pEntry;
		while ( (pEntry = readdir(pDir)) != NULL ) {
			if ( pEntry->d_name[0] != '.' ) {
				pInfo->iNetworkInterfaces++;
			}
		}
		closedir(pDir);
	}
#endif
}

// Get process information
// 获取进程信息
void GetProcessInfo(SystemInfo* pInfo)
{
#ifdef _WIN32
	// Windows process info
	// Windows 进程信息
	pInfo->ulCurrentProcessId = GetCurrentProcessId();
	
	// Count running processes (simplified)
	// 计算运行进程数（简化）
	DWORD arrProcesses[1024];
	DWORD cbNeeded;
	if ( EnumProcesses(arrProcesses, sizeof(arrProcesses), &cbNeeded) ) {
		pInfo->iProcessCount = cbNeeded / sizeof(DWORD);
	} else {
		pInfo->iProcessCount = 1;
	}
#else
	// Linux process info
	// Linux 进程信息
	pInfo->ulCurrentProcessId = getpid();
	
	// Count processes by reading /proc
	// 通过读取 /proc 计算进程数
	pInfo->iProcessCount = 0;
	DIR* pDir = opendir("/proc");
	if ( pDir ) {
		struct dirent* pEntry;
		while ( (pEntry = readdir(pDir)) != NULL ) {
			// Check if directory name is numeric (process ID)
			// 检查目录名是否为数字（进程ID）
			if ( strspn(pEntry->d_name, "0123456789") == strlen(pEntry->d_name) ) {
				pInfo->iProcessCount++;
			}
		}
		closedir(pDir);
	}
#endif
}

// Format bytes to human readable format
// 将字节格式化为人类可读格式
str FormatBytes(unsigned long long ullBytes)
{
	const char* arrUnits[] = {"B", "KB", "MB", "GB", "TB"};
	int iUnit = 0;
	double dSize = (double)ullBytes;
	
	while ( dSize >= 1024.0 && iUnit < 4 ) {
		dSize /= 1024.0;
		iUnit++;
	}
	
	return xrtFormat("%.2f %s", dSize, arrUnits[iUnit]);
}

// Display system information
// 显示系统信息
void DisplaySystemInfo(SystemInfo* pInfo)
{
	printf("=== System Information ===\n");
	printf("=== 系统信息 ===\n");
	
	// Hardware Information
	// 硬件信息
	printf("\nHardware Information / 硬件信息:\n");
	printf("  CPU Model: %s\n", pInfo->sCpuModel);
	printf("  CPU Cores: %d\n", pInfo->iCpuCores);
	printf("  CPU Threads: %d\n", pInfo->iCpuThreads);
	printf("  Total Memory: %s\n", FormatBytes(pInfo->ullTotalMemory));
	printf("  Free Memory: %s\n", FormatBytes(pInfo->ullFreeMemory));
	printf("  Memory Usage: %.1f%%\n",
	       100.0 * (pInfo->ullTotalMemory - pInfo->ullFreeMemory) / pInfo->ullTotalMemory);
	
	// Operating System Information
	// 操作系统信息
	printf("\nOperating System / 操作系统:\n");
	printf("  OS Name: %s\n", pInfo->sOsName);
	printf("  OS Version: %s\n", pInfo->sOsVersion);
	printf("  Architecture: %s\n", pInfo->sArchitecture);
	
	// Disk Information
	// 磁盘信息
	printf("\nDisk Information / 磁盘信息:\n");
	printf("  Total Disk Space: %s\n", FormatBytes(pInfo->ullTotalDiskSpace));
	printf("  Free Disk Space: %s\n", FormatBytes(pInfo->ullFreeDiskSpace));
	if ( pInfo->ullTotalDiskSpace > 0 ) {
		printf("  Disk Usage: %.1f%%\n",
		       100.0 * (pInfo->ullTotalDiskSpace - pInfo->ullFreeDiskSpace) / pInfo->ullTotalDiskSpace);
	}
	
	// Network Information
	// 网络信息
	printf("\nNetwork Information / 网络信息:\n");
	printf("  Network Interfaces: %d\n", pInfo->iNetworkInterfaces);
	printf("  Primary IP Address: %s\n", pInfo->sPrimaryIpAddress);
	
	// Process Information
	// 进程信息
	printf("\nProcess Information / 进程信息:\n");
	printf("  Current Process ID: %lu\n", pInfo->ulCurrentProcessId);
	printf("  Running Processes: %d\n", pInfo->iProcessCount);
	
	// System Health Summary
	// 系统健康摘要
	printf("\nSystem Health Summary / 系统健康摘要:\n");
	double dMemoryUsage = 100.0 * (pInfo->ullTotalMemory - pInfo->ullFreeMemory) / pInfo->ullTotalMemory;
	double dDiskUsage = 0;
	if ( pInfo->ullTotalDiskSpace > 0 ) {
		dDiskUsage = 100.0 * (pInfo->ullTotalDiskSpace - pInfo->ullFreeDiskSpace) / pInfo->ullTotalDiskSpace;
	}
	
	printf("  Memory Load: %.1f%% %s\n", dMemoryUsage,
	       dMemoryUsage > 80 ? "[HIGH]" : (dMemoryUsage > 60 ? "[MODERATE]" : "[NORMAL]"));
	printf("  Disk Usage: %.1f%% %s\n", dDiskUsage,
	       dDiskUsage > 90 ? "[CRITICAL]" : (dDiskUsage > 80 ? "[HIGH]" : "[NORMAL]"));
	printf("  System Status: %s\n",
	       (dMemoryUsage > 80 || dDiskUsage > 90) ? "WARNING" : "HEALTHY");
}

// Test system monitoring
// 测试系统监控
void TestSystemMonitoring()
{
	printf("\n=== System Monitoring Test ===\n");
	printf("=== 系统监控测试 ===\n");
	
	// Monitor system resources over time
	// 随时间监控系统资源
	printf("Monitoring system resources for 5 seconds...\n");
	
	time_t tStart = time(NULL);
	int iSamples = 0;
	
	while ( time(NULL) - tStart < 5 ) {
		SystemInfo info = {0};
		GetMemoryInfo(&info);
		
		double dMemoryUsage = 100.0 * (info.ullTotalMemory - info.ullFreeMemory) / info.ullTotalMemory;
		printf("Sample %d: Memory usage: %.1f%%\n", ++iSamples, dMemoryUsage);
		
#ifdef _WIN32
		Sleep(1000);  // 1 second
#else
		sleep(1);
#endif
	}
	
	printf("Monitoring completed\n");
}

// Test resource intensive operations
// 测试资源密集型操作
void TestResourceIntensiveOps()
{
	printf("\n=== Resource Intensive Operations ===\n");
	printf("=== 资源密集型操作 ===\n");
	
	// Measure impact of different operations
	// 测量不同操作的影响
	
	// Operation 1: Memory allocation
	// 操作1：内存分配
	printf("Testing memory allocation impact:\n");
	unsigned long long ullStartMem, ullEndMem;
	
	SystemInfo infoBefore = {0};
	GetMemoryInfo(&infoBefore);
	ullStartMem = infoBefore.ullFreeMemory;
	
	// Allocate 100MB
	// 分配 100MB
	size_t iAllocSize = 100 * 1024 * 1024;
	str pBigBlock = xrtMalloc(iAllocSize);
	memset(pBigBlock, 0, iAllocSize);  // Touch memory
	
	SystemInfo infoAfter = {0};
	GetMemoryInfo(&infoAfter);
	ullEndMem = infoAfter.ullFreeMemory;
	
	printf("  Allocated: %s\n", FormatBytes(iAllocSize));
	printf("  Memory change: %s\n", FormatBytes(ullStartMem - ullEndMem));
	
	xrtFree(pBigBlock);
	
	// Operation 2: CPU intensive task
	// 操作2：CPU 密集型任务
	printf("Testing CPU intensive task:\n");
	clock_t clkStart = clock();
	
	volatile double dResult = 1.0;
	for ( int i = 0; i < 1000000; i++ ) {
		dResult = sin(dResult) * cos(dResult) + sqrt(dResult);
	}
	
	clock_t clkEnd = clock();
	double dCpuTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("  CPU time: %.6f seconds\n", dCpuTime);
	printf("  Final calculation result: %.6f\n", dResult);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT System Information Demo\n");
	printf("XRT 系统信息获取演示\n");
	printf("========================\n\n");
	
	// Collect system information
	// 收集系统信息
	SystemInfo sysInfo = {0};
	
	GetCpuInfo(&sysInfo);
	GetMemoryInfo(&sysInfo);
	GetOsInfo(&sysInfo);
	GetDiskInfo(&sysInfo);
	GetNetworkInfo(&sysInfo);
	GetProcessInfo(&sysInfo);
	
	// Display collected information
	// 显示收集的信息
	DisplaySystemInfo(&sysInfo);
	
	// Run additional tests
	// 运行额外测试
	TestSystemMonitoring();
	TestResourceIntensiveOps();
	
	// Cleanup
	// 清理
	xrtFree(sysInfo.sCpuModel);
	xrtFree(sysInfo.sOsName);
	xrtFree(sysInfo.sOsVersion);
	xrtFree(sysInfo.sArchitecture);
	xrtFree(sysInfo.sPrimaryIpAddress);
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}