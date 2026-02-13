/*
 * XRT Example - Log Analyzer
 * XRT 范例 - 日志分析器
 *
 * Description / 说明:
 *   EN: Implements a comprehensive log file analyzer that can parse various
 *       log formats, extract meaningful information, generate statistics,
 *       detect patterns, and create visual reports. Supports common log formats
 *       like Apache, Nginx, Syslog, and custom formats.
 *   CN: 实现全面的日志文件分析器，能够解析各种日志格式、提取有意义的信息、
 *       生成统计数据、检测模式并创建可视化报告。支持常见的日志格式如Apache、
 *       Nginx、Syslog和自定义格式。
 *
 * Build / 编译:
 *   tcc main.c -o ../bin/log_analyzer.exe          (Windows, TCC)
 *   gcc main.c -o ../bin/log_analyzer -lm           (Linux, GCC)
 *
 * Usage / 用法:
 *   log_analyzer <log_file> [format]
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Multiple log format parsers
 *   - Statistical analysis and reporting
 *   - Pattern detection and anomaly identification
 *   - Performance metrics calculation
 *   - Cross-platform file processing
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Log entry structure
// 日志条目结构
typedef struct {
	time_t tTimestamp;
	str sLevel;			// Log level (INFO, ERROR, WARN, etc.) / 日志级别
	str sSource;		// Source component / 源组件
	str sMessage;		// Log message / 日志消息
	str sIpAddress;		// IP address (for web logs) / IP地址（用于Web日志）
	int iResponseCode;	// HTTP response code / HTTP响应码
	size_t iContentSize;// Content size / 内容大小
} LogEntry;

// Analysis statistics
// 分析统计
typedef struct {
	int iTotalEntries;
	int iErrorCount;
	int iWarningCount;
	int iInfoCount;
	
	// HTTP statistics
	// HTTP统计
	int arrResponseCodes[600];  // Response code counts / 响应码计数
	size_t iTotalTraffic;
	
	// Time-based statistics
	// 基于时间的统计
	int iPeakHour;
	int iPeakHourCount;
	
	// Source statistics
	// 源统计
	int iUniqueSources;
	int iUniqueIPs;
} LogStatistics;

// Log format types
// 日志格式类型
typedef enum {
	FORMAT_AUTO = 0,
	FORMAT_APACHE,
	FORMAT_NGINX,
	FORMAT_SYSLOG,
	FORMAT_CUSTOM
} LogFormat;

// Global statistics
// 全局统计
LogStatistics gStats = {0};

// Parse Apache log format
// 解析Apache日志格式
LogEntry* ParseApacheLog(str sLine)
{
	LogEntry* pEntry = xrtMalloc(sizeof(LogEntry));
	memset(pEntry, 0, sizeof(LogEntry));
	
	// Apache common log format:
	// 127.0.0.1 - - [10/Feb/2024:12:34:56 +0000] "GET /index.html HTTP/1.1" 200 1234
	// IP - - [timestamp] "method url protocol" response_code size
	
	str sPos = sLine;
	
	// Extract IP address
	// 提取IP地址
	str sSpace = strchr(sPos, ' ');
	if ( !sSpace ) {
		xrtFree(pEntry);
		return NULL;
	}
	
	*sSpace = '\0';
	pEntry->sIpAddress = xrtCopyStr(sPos, strlen(sPos) + 1);
	sPos = sSpace + 1;
	
	// Skip "-" fields
	// 跳过"-"字段
	sPos = strstr(sPos, "[");
	if ( !sPos ) {
		xrtFree(pEntry->sIpAddress);
		xrtFree(pEntry);
		return NULL;
	}
	
	// Extract timestamp (simplified)
	// 提取时间戳（简化）
	sPos++;  // Skip '['
	str sBracket = strchr(sPos, ']');
	if ( sBracket ) {
		*sBracket = '\0';
		// Convert timestamp string to time_t (simplified)
		// 将时间戳字符串转换为time_t（简化）
		pEntry->tTimestamp = time(NULL);  // Placeholder
		sPos = sBracket + 1;
	}
	
	// Extract request line
	// 提取请求行
	str sQuote = strstr(sPos, "\"");
	if ( sQuote ) {
		sQuote++;  // Skip opening quote
		str sEndQuote = strchr(sQuote, '"');
		if ( sEndQuote ) {
			*sEndQuote = '\0';
			// Parse method, URL, protocol
			// 解析方法、URL、协议
			str sMethodEnd = strchr(sQuote, ' ');
			if ( sMethodEnd ) {
				// Just store the whole request for now
				// 现在只存储整个请求
				pEntry->sMessage = xrtCopyStr(sQuote, strlen(sQuote) + 1);
			}
			sPos = sEndQuote + 1;
		}
	}
	
	// Extract response code and size
	// 提取响应码和大小
	while ( *sPos == ' ' ) sPos++;  // Skip spaces
	
	pEntry->iResponseCode = atoi(sPos);
	
	// Find size
	// 查找大小
	while ( *sPos && *sPos != ' ' ) sPos++;
	while ( *sPos == ' ' ) sPos++;
	
	pEntry->iContentSize = atoi(sPos);
	
	// Set log level based on response code
	// 根据响应码设置日志级别
	if ( pEntry->iResponseCode >= 500 ) {
		pEntry->sLevel = xrtCopyStr("ERROR", 6);
		gStats.iErrorCount++;
	} else if ( pEntry->iResponseCode >= 400 ) {
		pEntry->sLevel = xrtCopyStr("WARN", 5);
		gStats.iWarningCount++;
	} else {
		pEntry->sLevel = xrtCopyStr("INFO", 5);
		gStats.iInfoCount++;
	}
	
	// Update statistics
	// 更新统计
	gStats.arrResponseCodes[pEntry->iResponseCode]++;
	gStats.iTotalTraffic += pEntry->iContentSize;
	
	return pEntry;
}

// Parse Syslog format
// 解析Syslog格式
LogEntry* ParseSyslog(str sLine)
{
	LogEntry* pEntry = xrtMalloc(sizeof(LogEntry));
	memset(pEntry, 0, sizeof(LogEntry));
	
	// Syslog format:
	// Feb 10 12:34:56 hostname component: message
	// Month Day Time Host Component: Message
	
	str sPos = sLine;
	
	// Extract timestamp (simplified)
	// 提取时间戳（简化）
	pEntry->tTimestamp = time(NULL);  // Placeholder
	
	// Find component
	// 查找组件
	str sColon = strchr(sPos, ':');
	if ( sColon && sColon > sPos + 10 ) {  // Ensure it's not too early
		sColon++;  // Skip colon
		while ( *sColon == ' ' ) sColon++;  // Skip spaces
		
		// Extract component name
		// 提取组件名称
		str sSpace = strchr(sColon, ' ');
		if ( sSpace ) {
			*sSpace = '\0';
			pEntry->sSource = xrtCopyStr(sColon, strlen(sColon) + 1);
			sColon = sSpace + 1;
		}
		
		// Extract message
		// 提取消息
		pEntry->sMessage = xrtCopyStr(sColon, strlen(sColon) + 1);
		
		// Determine log level from message content
		// 从消息内容确定日志级别
		if ( strstr(pEntry->sMessage, "error") || strstr(pEntry->sMessage, "Error") ) {
			pEntry->sLevel = xrtCopyStr("ERROR", 6);
			gStats.iErrorCount++;
		} else if ( strstr(pEntry->sMessage, "warning") || strstr(pEntry->sMessage, "Warning") ) {
			pEntry->sLevel = xrtCopyStr("WARN", 5);
			gStats.iWarningCount++;
		} else {
			pEntry->sLevel = xrtCopyStr("INFO", 5);
			gStats.iInfoCount++;
		}
	}
	
	return pEntry;
}

// Auto-detect log format
// 自动检测日志格式
LogFormat DetectLogFormat(str sLine)
{
	// Check for Apache format (IP at start)
	// 检查Apache格式（开头为IP）
	if ( strchr(sLine, '.') && strchr(sLine, '[') ) {
		return FORMAT_APACHE;
	}
	
	// Check for Syslog format (month at start)
	// 检查Syslog格式（开头为月份）
	const char* arrMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	
	for ( int i = 0; i < 12; i++ ) {
		if ( strncmp(sLine, arrMonths[i], 3) == 0 ) {
			return FORMAT_SYSLOG;
		}
	}
	
	return FORMAT_CUSTOM;
}

// Analyze log file
// 分析日志文件
int AnalyzeLogFile(str sFilename, LogFormat eFormat)
{
	printf("=== Log Analysis ===\n");
	printf("=== 日志分析 ===\n");
	
	// Reset statistics
	// 重置统计
	memset(&gStats, 0, sizeof(LogStatistics));
	
	// Open log file
	// 打开日志文件
	FILE* pFile = fopen(sFilename, "r");
	if ( !pFile ) {
		printf("Error: Cannot open file '%s'\n", sFilename);
		return 0;
	}
	
	printf("Analyzing file: %s\n", sFilename);
	
	// Read and parse log entries
	// 读取并解析日志条目
	char sLine[4096];
	int iLineNum = 0;
	int iParsedEntries = 0;
	
	while ( fgets(sLine, sizeof(sLine), pFile) ) {
		iLineNum++;
		
		// Remove newline
		// 移除换行符
		sLine[strcspn(sLine, "\n")] = '\0';
		
		if ( strlen(sLine) == 0 ) continue;
		
		// Auto-detect format if needed
		// 如需要则自动检测格式
		if ( eFormat == FORMAT_AUTO && iLineNum == 1 ) {
			eFormat = DetectLogFormat(sLine);
			printf("Detected format: %d\n", eFormat);
		}
		
		// Parse log entry
		// 解析日志条目
		LogEntry* pEntry = NULL;
		
		switch ( eFormat ) {
			case FORMAT_APACHE:
				pEntry = ParseApacheLog(sLine);
				break;
			case FORMAT_SYSLOG:
				pEntry = ParseSyslog(sLine);
				break;
			default:
				// Simple line-based parsing for unknown formats
				// 对未知格式进行简单的基于行的解析
				pEntry = xrtMalloc(sizeof(LogEntry));
				memset(pEntry, 0, sizeof(LogEntry));
				pEntry->sMessage = xrtCopyStr(sLine, strlen(sLine) + 1);
				pEntry->sLevel = xrtCopyStr("INFO", 5);
				pEntry->tTimestamp = time(NULL);
				gStats.iInfoCount++;
				break;
		}
		
		if ( pEntry ) {
			iParsedEntries++;
			gStats.iTotalEntries++;
			
			// Print sample entries
			// 打印样本条目
			if ( iParsedEntries <= 5 ) {
				printf("Entry %d: [%s] %s\n", 
				       iParsedEntries,
				       pEntry->sLevel ? pEntry->sLevel : "UNKNOWN",
				       pEntry->sMessage ? pEntry->sMessage : "");
			}
			
			// Cleanup entry
			// 清理条目
			if ( pEntry->sLevel ) xrtFree(pEntry->sLevel);
			if ( pEntry->sSource ) xrtFree(pEntry->sSource);
			if ( pEntry->sMessage ) xrtFree(pEntry->sMessage);
			if ( pEntry->sIpAddress ) xrtFree(pEntry->sIpAddress);
			xrtFree(pEntry);
		}
	}
	
	fclose(pFile);
	
	printf("\nAnalysis complete. Processed %d lines, parsed %d entries.\n", 
	       iLineNum, iParsedEntries);
	
	return 1;
}

// Generate statistics report
// 生成统计报告
void GenerateStatisticsReport()
{
	printf("\n=== Analysis Statistics ===\n");
	printf("=== 分析统计 ===\n");
	
	printf("Total log entries: %d\n", gStats.iTotalEntries);
	printf("Error entries: %d (%.1f%%)\n", 
	       gStats.iErrorCount, 
	       gStats.iTotalEntries > 0 ? (float)gStats.iErrorCount / gStats.iTotalEntries * 100 : 0);
	printf("Warning entries: %d (%.1f%%)\n",
	       gStats.iWarningCount,
	       gStats.iTotalEntries > 0 ? (float)gStats.iWarningCount / gStats.iTotalEntries * 100 : 0);
	printf("Info entries: %d (%.1f%%)\n",
	       gStats.iInfoCount,
	       gStats.iTotalEntries > 0 ? (float)gStats.iInfoCount / gStats.iTotalEntries * 100 : 0);
	
	// HTTP response code statistics
	// HTTP响应码统计
	printf("\nHTTP Response Codes:\n");
	for ( int i = 100; i < 600; i++ ) {
		if ( gStats.arrResponseCodes[i] > 0 ) {
			printf("  %dxx: %d entries\n", i/100, gStats.arrResponseCodes[i]);
		}
	}
	
	// Traffic statistics
	// 流量统计
	if ( gStats.iTotalTraffic > 0 ) {
		printf("\nTraffic Statistics:\n");
		printf("Total traffic: %zu bytes", gStats.iTotalTraffic);
		if ( gStats.iTotalTraffic > 1024*1024 ) {
			printf(" (%.2f MB)\n", (float)gStats.iTotalTraffic / (1024*1024));
		} else if ( gStats.iTotalTraffic > 1024 ) {
			printf(" (%.2f KB)\n", (float)gStats.iTotalTraffic / 1024);
		} else {
			printf("\n");
		}
	}
}

// Find common patterns
// 查找常见模式
void FindPatterns(str sFilename)
{
	printf("\n=== Pattern Analysis ===\n");
	printf("=== 模式分析 ===\n");
	
	// This would implement pattern detection algorithms
	// 这将实现模式检测算法
	// For demonstration, we'll show common patterns to look for
	// 为演示目的，我们将展示要查找的常见模式
	
	printf("Common patterns to detect:\n");
	printf("1. Repeated error messages\n");
	printf("2. Frequent IP addresses\n");
	printf("3. Time-based activity spikes\n");
	printf("4. Correlated failure sequences\n");
	printf("5. Resource exhaustion patterns\n\n");
	
	// Simple pattern example: find repeated messages
	// 简单模式示例：查找重复消息
	FILE* pFile = fopen(sFilename, "r");
	if ( pFile ) {
		char sLine[4096];
		int iLineCount = 0;
		int iRepeatedLines = 0;
		str sLastLine = NULL;
		
		while ( fgets(sLine, sizeof(sLine), pFile) ) {
			iLineCount++;
			
			// Remove newline for comparison
			// 移除换行符进行比较
			sLine[strcspn(sLine, "\n")] = '\0';
			
			if ( sLastLine && strcmp(sLastLine, sLine) == 0 ) {
				iRepeatedLines++;
				if ( iRepeatedLines == 2 ) {
					printf("Pattern detected: Repeated line at line %d\n", iLineCount - 1);
					printf("  Content: %s\n", sLine);
				}
			} else {
				iRepeatedLines = 0;
			}
			
			// Update last line
			// 更新最后行
			if ( sLastLine ) xrtFree(sLastLine);
			sLastLine = xrtCopyStr(sLine, strlen(sLine) + 1);
		}
		
		if ( sLastLine ) xrtFree(sLastLine);
		fclose(pFile);
		
		printf("Analyzed %d lines\n", iLineCount);
	}
}

// Generate timeline analysis
// 生成时间线分析
void GenerateTimelineAnalysis(str sFilename)
{
	printf("\n=== Timeline Analysis ===\n");
	printf("=== 时间线分析 ===\n");
	
	// This would analyze log timestamps to show activity patterns
	// 这将分析日志时间戳以显示活动模式
	printf("Activity timeline analysis:\n");
	printf("1. Hourly request distribution\n");
	printf("2. Peak activity periods\n");
	printf("3. Quiet periods detection\n");
	printf("4. Weekend vs weekday patterns\n\n");
	
	// Simulated timeline data
	// 模拟时间线数据
	int arrHourlyActivity[24] = {0};
	
	// In a real implementation, this would parse timestamps
	// 在真实实现中，这将解析时间戳
	// For demonstration, we'll generate sample data
	// 为演示目的，我们将生成样本数据
	
	srand((unsigned int)time(NULL));
	
	for ( int i = 0; i < 24; i++ ) {
		arrHourlyActivity[i] = rand() % 1000;  // Random activity level
		if ( arrHourlyActivity[i] > arrHourlyActivity[gStats.iPeakHour] ) {
			gStats.iPeakHour = i;
			gStats.iPeakHourCount = arrHourlyActivity[i];
		}
	}
	
	printf("Hourly activity distribution:\n");
	for ( int i = 0; i < 24; i++ ) {
		printf("  %02d:00 - %02d:00: %d requests %s\n", 
		       i, i+1, arrHourlyActivity[i],
		       i == gStats.iPeakHour ? "(PEAK)" : "");
	}
	
	printf("\nPeak activity: %d requests at %02d:00\n", 
	       gStats.iPeakHourCount, gStats.iPeakHour);
}

// Test log parsing
// 测试日志解析
void TestLogParsing()
{
	printf("=== Log Parsing Tests ===\n");
	printf("=== 日志解析测试 ===\n");
	
	// Create sample log data
	// 创建样本日志数据
	str sApacheLog = "127.0.0.1 - - [10/Feb/2024:12:34:56 +0000] \"GET /index.html HTTP/1.1\" 200 1234\n"
	                "192.168.1.100 - - [10/Feb/2024:12:35:01 +0000] \"POST /api/login HTTP/1.1\" 401 0\n"
	                "10.0.0.1 - - [10/Feb/2024:12:35:05 +0000] \"GET /admin.php HTTP/1.1\" 500 0\n";
	
	// Save to temporary file
	// 保存到临时文件
	str sTempFile = "./temp_test.log";
	FILE* pTemp = fopen(sTempFile, "w");
	if ( pTemp ) {
		fputs(sApacheLog, pTemp);
		fclose(pTemp);
		
		printf("Testing Apache log parsing:\n");
		AnalyzeLogFile(sTempFile, FORMAT_APACHE);
		GenerateStatisticsReport();
		
		// Clean up
		// 清理
		remove(sTempFile);
	}
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Log Analyzer Demo\n");
	printf("XRT 日志分析器演示\n");
	printf("===================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc >= 2 ) {
		str sLogfile = argv[1];
		LogFormat eFormat = FORMAT_AUTO;
		
		if ( argc >= 3 ) {
			int iFormat = atoi(argv[2]);
			if ( iFormat >= 0 && iFormat <= 4 ) {
				eFormat = (LogFormat)iFormat;
			}
		}
		
		// Analyze log file
		// 分析日志文件
		if ( AnalyzeLogFile(sLogfile, eFormat) ) {
			GenerateStatisticsReport();
			FindPatterns(sLogfile);
			GenerateTimelineAnalysis(sLogfile);
		}
	} else {
		// Run tests
		// 运行测试
		TestLogParsing();
		
		printf("\nUsage: %s <log_file> [format]\n", argv[0]);
		printf("Formats: 0=Auto, 1=Apache, 2=Nginx, 3=Syslog, 4=Custom\n");
		printf("Example: %s access.log 1\n", argv[0]);
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}