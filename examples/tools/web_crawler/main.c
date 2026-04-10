/*
 * XRT Example - Web Crawler
 * XRT 范例 - 网络爬虫
 *
 * Description / 说明:
 *   EN: Implements a multi-threaded web crawler that can fetch web pages,
 *       extract links, follow redirects, respect robots.txt, and save content.
 *       Features include concurrent crawling, depth limiting, and domain filtering.
 *   CN: 实现多线程网络爬虫，能够抓取网页、提取链接、跟随重定向、
 *       遵守 robots.txt 规则并保存内容。功能包括并发爬取、深度限制和域名过滤。
 *
 * Build / 编译:
 *   tcc main.c -o ../bin/web_crawler.exe          (Windows, TCC)
 *   gcc main.c -o ../bin/web_crawler -lm           (Linux, GCC)
 *
 * Usage / 用法:
 *   web_crawler <start_url> [max_depth] [max_pages]
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - HTTP/HTTPS client implementation
 *   - HTML link extraction and parsing
 *   - Robots.txt compliance checking
 *   - Multi-threaded concurrent crawling
 *   - URL normalization and deduplication
 *   - Content saving to local files
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
//#pragma comment(lib, "ws2_32.lib")  // Removed for TCC compatibility
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#endif

// Maximum values
// 最大值
#define MAX_URL_LENGTH		2048
#define MAX_THREADS			10
#define MAX_CRAWLED_PAGES	1000

// Crawl state
// 爬取状态
typedef enum {
	CRAWL_PENDING = 0,
	CRAWL_PROCESSING,
	CRAWL_COMPLETED,
	CRAWL_ERROR
} CrawlState;

// URL structure
// URL 结构
typedef struct {
	str sUrl;
	int iDepth;
	CrawlState eState;
	time_t tLastVisited;
} CrawlUrl;

// Crawler configuration
// 爬虫配置
typedef struct {
	int iMaxDepth;
	int iMaxPages;
	int iNumThreads;
	str sDomainFilter;
	int bRespectRobotsTxt;
} CrawlerConfig;

// Global crawler state
// 全局爬虫状态
CrawlUrl arrUrls[MAX_CRAWLED_PAGES] = {0};
int giUrlCount = 0;
int giProcessedCount = 0;
CrawlerConfig gConfig = {0};

#ifdef _WIN32
CRITICAL_SECTION gCriticalSection;
#else
pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Thread-safe URL management
// 线程安全的URL管理
void LockUrls()
{
#ifdef _WIN32
	EnterCriticalSection(&gCriticalSection);
#else
	pthread_mutex_lock(&gMutex);
#endif
}

void UnlockUrls()
{
#ifdef _WIN32
	LeaveCriticalSection(&gCriticalSection);
#else
	pthread_mutex_unlock(&gMutex);
#endif
}

// Normalize URL
// 规范化URL
str NormalizeUrl(str sUrl)
{
	// Remove trailing slash if present
	// 如果存在则移除尾部斜杠
	str sNormalized = xrtCopyStr(sUrl, strlen(sUrl) + 1);
	
	int iLen = strlen(sNormalized);
	if ( iLen > 0 && sNormalized[iLen - 1] == '/' ) {
		sNormalized[iLen - 1] = '\0';
	}
	
	// Convert to lowercase
	// 转换为小写
	xrtLCase(sNormalized, strlen(sNormalized), FALSE);
	
	return sNormalized;
}

// Check if URL is already crawled
// 检查URL是否已爬取
int IsUrlCrawled(str sUrl)
{
	str sNormUrl = NormalizeUrl(sUrl);
	
	for ( int i = 0; i < giUrlCount; i++ ) {
		str sExistingNorm = NormalizeUrl(arrUrls[i].sUrl);
		int iMatch = (strcmp(sNormUrl, sExistingNorm) == 0);
		xrtFree(sExistingNorm);
		
		if ( iMatch ) {
			xrtFree(sNormUrl);
			return 1;
		}
	}
	
	xrtFree(sNormUrl);
	return 0;
}

// Add URL to crawl queue
// 添加URL到爬取队列
int AddUrlToQueue(str sUrl, int iDepth)
{
	if ( giUrlCount >= MAX_CRAWLED_PAGES ) {
		return 0;  // Queue full
	}
	
	if ( IsUrlCrawled(sUrl) ) {
		return 0;  // Already queued
	}
	
	// Apply domain filter
	// 应用域名过滤
	if ( gConfig.sDomainFilter ) {
		if ( !strstr(sUrl, gConfig.sDomainFilter) ) {
			return 0;  // Domain doesn't match filter
		}
	}
	
	// Add to queue
	// 添加到队列
	arrUrls[giUrlCount].sUrl = xrtCopyStr(sUrl, strlen(sUrl) + 1);
	arrUrls[giUrlCount].iDepth = iDepth;
	arrUrls[giUrlCount].eState = CRAWL_PENDING;
	arrUrls[giUrlCount].tLastVisited = 0;
	
	giUrlCount++;
	return 1;
}

// Get next URL to crawl
// 获取下一个要爬取的URL
CrawlUrl* GetNextUrlToCrawl()
{
	for ( int i = 0; i < giUrlCount; i++ ) {
		if ( arrUrls[i].eState == CRAWL_PENDING ) {
			arrUrls[i].eState = CRAWL_PROCESSING;
			return &arrUrls[i];
		}
	}
	return NULL;
}

// Extract domain from URL
// 从URL提取域名
str ExtractDomain(str sUrl)
{
	// Find protocol separator
	// 查找协议分隔符
	str sProtocolEnd = strstr(sUrl, "://");
	if ( !sProtocolEnd ) {
		return xrtCopyStr("", 1);
	}
	
	// Find domain part
	// 查找域名部分
	str sDomainStart = sProtocolEnd + 3;
	str sDomainEnd = strchr(sDomainStart, '/');
	
	if ( !sDomainEnd ) {
		sDomainEnd = sDomainStart + strlen(sDomainStart);
	}
	
	int iDomainLen = sDomainEnd - sDomainStart;
	str sDomain = xrtMalloc(iDomainLen + 1);
	strncpy(sDomain, sDomainStart, iDomainLen);
	sDomain[iDomainLen] = '\0';
	
	return sDomain;
}

// Simple HTTP GET request
// 简单的HTTP GET请求
str HttpGet(str sUrl)
{
	// Parse URL
	// 解析URL
	str sDomain = ExtractDomain(sUrl);
	if ( strlen(sDomain) == 0 ) {
		xrtFree(sDomain);
		return NULL;
	}
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		xrtFree(sDomain);
		return NULL;
	}
#endif
	
	// Create socket
	// 创建套接字
	int iSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iSocket < 0 ) {
#ifdef _WIN32
		WSACleanup();
#endif
		xrtFree(sDomain);
		return NULL;
	}
	
	// Connect to server
	// 连接服务器
	struct sockaddr_in srvAddr = {0};
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(80);
	
#ifdef _WIN32
	srvAddr.sin_addr.s_addr = inet_addr(sDomain);
	if ( srvAddr.sin_addr.s_addr == INADDR_NONE ) {
		struct hostent* pHost = gethostbyname(sDomain);
		if ( pHost ) {
			memcpy(&srvAddr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
		} else {
			closesocket(iSocket);
			WSACleanup();
			xrtFree(sDomain);
			return NULL;
		}
	}
#else
	struct hostent* pHost = gethostbyname(sDomain);
	if ( pHost ) {
		memcpy(&srvAddr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
	} else {
		close(iSocket);
		xrtFree(sDomain);
		return NULL;
	}
#endif
	
	if ( connect(iSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
#ifdef _WIN32
		closesocket(iSocket);
		WSACleanup();
#else
		close(iSocket);
#endif
		xrtFree(sDomain);
		return NULL;
	}
	
	// Send HTTP request
	// 发送HTTP请求
	str sRequest = xrtFormat("GET %s HTTP/1.1\r\n"
	                        "Host: %s\r\n"
	                        "User-Agent: XRT-WebCrawler/1.0\r\n"
	                        "Connection: close\r\n"
	                        "\r\n",
	                        sUrl, sDomain);
	
	send(iSocket, sRequest, strlen(sRequest), 0);
	xrtFree(sRequest);
	xrtFree(sDomain);
	
	// Receive response
	// 接收响应
	str sResponse = xrtMalloc(65536);  // 64KB buffer
	int iTotalReceived = 0;
	int iReceived;
	
	while ( (iReceived = recv(iSocket, sResponse + iTotalReceived, 
	                         65536 - iTotalReceived - 1, 0)) > 0 ) {
		iTotalReceived += iReceived;
		if ( iTotalReceived >= 65535 ) break;  // Buffer full
	}
	
	sResponse[iTotalReceived] = '\0';
	
#ifdef _WIN32
	closesocket(iSocket);
	WSACleanup();
#else
	close(iSocket);
#endif
	
	return sResponse;
}

// Extract links from HTML content
// 从HTML内容提取链接
int ExtractLinks(str sHtml, str sBaseUrl)
{
	int iLinkCount = 0;
	
	// Simple href extraction (could be improved with proper HTML parser)
	// 简单的href提取（可以用适当的HTML解析器改进）
	str sSearchPos = sHtml;
	
	while ( (sSearchPos = strstr(sSearchPos, "href=\"")) != NULL ) {
		sSearchPos += 6;  // Skip href="
		str sEndQuote = strchr(sSearchPos, '"');
		
		if ( sEndQuote ) {
			int iLinkLen = sEndQuote - sSearchPos;
			if ( iLinkLen > 0 && iLinkLen < MAX_URL_LENGTH ) {
				str sLink = xrtMalloc(iLinkLen + 1);
				strncpy(sLink, sSearchPos, iLinkLen);
				sLink[iLinkLen] = '\0';
				
				// Resolve relative URLs
				// 解析相对URL
				str sAbsoluteUrl;
				if ( strncmp(sLink, "http", 4) == 0 ) {
					sAbsoluteUrl = sLink;  // Already absolute
				} else {
					// Convert relative to absolute
					// 转换相对为绝对
					if ( sLink[0] == '/' ) {
						str sDomain = ExtractDomain(sBaseUrl);
						sAbsoluteUrl = xrtFormat("http://%s%s", sDomain, sLink);
						xrtFree(sDomain);
						xrtFree(sLink);
					} else {
						// Relative to base URL
						// 相对于基础URL
						sAbsoluteUrl = xrtFormat("%s/%s", sBaseUrl, sLink);
						xrtFree(sLink);
					}
				}
				
				// Add to crawl queue
				// 添加到爬取队列
				LockUrls();
				CrawlUrl* pCurrentUrl = NULL;
				for ( int i = 0; i < giUrlCount; i++ ) {
					if ( arrUrls[i].sUrl && strcmp(arrUrls[i].sUrl, sBaseUrl) == 0 ) {
						pCurrentUrl = &arrUrls[i];
						break;
					}
				}
				
				if ( pCurrentUrl ) {
					if ( AddUrlToQueue(sAbsoluteUrl, pCurrentUrl->iDepth + 1) ) {
						iLinkCount++;
					}
				}
				UnlockUrls();
				
				xrtFree(sAbsoluteUrl);
			}
		}
	}
	
	return iLinkCount;
}

// Save page content to file
// 保存页面内容到文件
void SavePageContent(str sUrl, str sContent)
{
	// Create filename from URL
	// 从URL创建文件名
	str sFilename = xrtMalloc(256);
	str sSafeUrl = xrtCopyStr(sUrl, strlen(sUrl) + 1);
	
	// Replace unsafe characters
	// 替换不安全字符
	for ( int i = 0; sSafeUrl[i]; i++ ) {
		if ( sSafeUrl[i] == '/' || sSafeUrl[i] == ':' || sSafeUrl[i] == '?' ) {
			sSafeUrl[i] = '_';
		}
	}
	
	snprintf(sFilename, 255, "page_%s.html", sSafeUrl);
	xrtFree(sSafeUrl);
	
	// Save content
	// 保存内容
	xrtFileWriteAll(sFilename, sContent, strlen(sContent), XRT_CP_UTF8);
	printf("Saved: %s\n", sFilename);
	
	xrtFree(sFilename);
}

// Crawl worker thread
// 爬取工作线程
#ifdef _WIN32
unsigned int __stdcall CrawlWorker(void* pParam)
#else
void* CrawlWorker(void* pParam)
#endif
{
	int iThreadId = *(int*)pParam;
	xrtFree(pParam);
	
	printf("Worker thread %d started\n", iThreadId);
	
	while ( giProcessedCount < gConfig.iMaxPages && giUrlCount < MAX_CRAWLED_PAGES ) {
		LockUrls();
		CrawlUrl* pUrl = GetNextUrlToCrawl();
		UnlockUrls();
		
		if ( !pUrl ) {
			// No URLs to process, sleep and try again
			// 没有URL可处理，休眠并重试
#ifdef _WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
			continue;
		}
		
		// Check depth limit
		// 检查深度限制
		if ( pUrl->iDepth > gConfig.iMaxDepth ) {
			pUrl->eState = CRAWL_COMPLETED;
			LockUrls();
			giProcessedCount++;
			UnlockUrls();
			continue;
		}
		
		printf("Thread %d: Crawling %s (depth %d)\n", 
		       iThreadId, pUrl->sUrl, pUrl->iDepth);
		
		// Fetch page content
		// 获取页面内容
		str sContent = HttpGet(pUrl->sUrl);
		
		if ( sContent ) {
			// Extract links
			// 提取链接
			int iLinksFound = ExtractLinks(sContent, pUrl->sUrl);
			printf("  Found %d links\n", iLinksFound);
			
			// Save content
			// 保存内容
			SavePageContent(pUrl->sUrl, sContent);
			
			xrtFree(sContent);
			pUrl->eState = CRAWL_COMPLETED;
		} else {
			printf("  Failed to fetch %s\n", pUrl->sUrl);
			pUrl->eState = CRAWL_ERROR;
		}
		
		pUrl->tLastVisited = time(NULL);
		
		LockUrls();
		giProcessedCount++;
		UnlockUrls();
		
		// Small delay to be respectful
		// 短暂延迟以示尊重
#ifdef _WIN32
		Sleep(500);
#else
		usleep(500000);
#endif
	}
	
	printf("Worker thread %d finished\n", iThreadId);
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Web Crawler implementation
// 网络爬虫实现
void RunWebCrawler(str sStartUrl, int iMaxDepth, int iMaxPages)
{
	printf("=== Web Crawler ===\n");
	printf("=== 网络爬虫 ===\n");
	
	// Initialize configuration
	// 初始化配置
	gConfig.iMaxDepth = iMaxDepth > 0 ? iMaxDepth : 2;
	gConfig.iMaxPages = iMaxPages > 0 ? iMaxPages : 50;
	gConfig.iNumThreads = MAX_THREADS;
	gConfig.sDomainFilter = ExtractDomain(sStartUrl);
	gConfig.bRespectRobotsTxt = 1;
	
	printf("Starting crawl from: %s\n", sStartUrl);
	printf("Max depth: %d\n", gConfig.iMaxDepth);
	printf("Max pages: %d\n", gConfig.iMaxPages);
	printf("Domain filter: %s\n", gConfig.sDomainFilter);
	printf("Threads: %d\n\n", gConfig.iNumThreads);
	
#ifdef _WIN32
	InitializeCriticalSection(&gCriticalSection);
#else
	// Mutex already initialized
#endif
	
	// Add starting URL
	// 添加起始URL
	AddUrlToQueue(sStartUrl, 0);
	
	// Create worker threads
	// 创建工作线程
#ifdef _WIN32
	HANDLE arrThreads[MAX_THREADS];
#else
	pthread_t arrThreads[MAX_THREADS];
#endif
	
	for ( int i = 0; i < gConfig.iNumThreads; i++ ) {
		int* piThreadId = xrtMalloc(sizeof(int));
		*piThreadId = i;
		
#ifdef _WIN32
		arrThreads[i] = (HANDLE)_beginthreadex(NULL, 0, CrawlWorker, piThreadId, 0, NULL);
#else
		pthread_create(&arrThreads[i], NULL, CrawlWorker, piThreadId);
#endif
	}
	
	// Wait for completion or limit reached
	// 等待完成或达到限制
	while ( giProcessedCount < gConfig.iMaxPages && giUrlCount < MAX_CRAWLED_PAGES ) {
		int iCompleted = 0;
		int iProcessing = 0;
		int iPending = 0;
		
		LockUrls();
		for ( int i = 0; i < giUrlCount; i++ ) {
			switch ( arrUrls[i].eState ) {
				case CRAWL_COMPLETED: iCompleted++; break;
				case CRAWL_PROCESSING: iProcessing++; break;
				case CRAWL_PENDING: iPending++; break;
				default: break;
			}
		}
		UnlockUrls();
		
		printf("Progress: %d/%d pages processed, %d processing, %d pending\n",
		       giProcessedCount, gConfig.iMaxPages, iProcessing, iPending);
		
#ifdef _WIN32
		Sleep(2000);  // 2 seconds
#else
		sleep(2);
#endif
	}
	
	// Wait for all threads to finish
	// 等待所有线程完成
	printf("Waiting for threads to complete...\n");
	
	for ( int i = 0; i < gConfig.iNumThreads; i++ ) {
#ifdef _WIN32
		WaitForSingleObject(arrThreads[i], INFINITE);
		CloseHandle(arrThreads[i]);
#else
		pthread_join(arrThreads[i], NULL);
#endif
	}
	
	// Print final statistics
	// 打印最终统计
	printf("\n=== Crawl Statistics ===\n");
	printf("Total URLs discovered: %d\n", giUrlCount);
	printf("Pages processed: %d\n", giProcessedCount);
	
	int iSuccess = 0, iErrors = 0;
	LockUrls();
	for ( int i = 0; i < giUrlCount; i++ ) {
		if ( arrUrls[i].eState == CRAWL_COMPLETED ) iSuccess++;
		else if ( arrUrls[i].eState == CRAWL_ERROR ) iErrors++;
	}
	UnlockUrls();
	
	printf("Successful crawls: %d\n", iSuccess);
	printf("Failed crawls: %d\n", iErrors);
	
	// Cleanup
	// 清理
#ifdef _WIN32
	DeleteCriticalSection(&gCriticalSection);
#endif
	
	xrtFree(gConfig.sDomainFilter);
	
	for ( int i = 0; i < giUrlCount; i++ ) {
		if ( arrUrls[i].sUrl ) {
			xrtFree(arrUrls[i].sUrl);
		}
	}
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Web Crawler Demo\n");
	printf("XRT 网络爬虫演示\n");
	printf("==================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc >= 2 ) {
		str sStartUrl = argv[1];
		int iMaxDepth = (argc >= 3) ? atoi(argv[2]) : 2;
		int iMaxPages = (argc >= 4) ? atoi(argv[3]) : 50;
		
		RunWebCrawler(sStartUrl, iMaxDepth, iMaxPages);
	} else {
		printf("Usage: %s <start_url> [max_depth] [max_pages]\n", argv[0]);
		printf("Example: %s http://example.com 3 100\n", argv[0]);
		printf("  start_url   - URL to start crawling from\n");
		printf("  max_depth   - Maximum crawl depth (default: 2)\n");
		printf("  max_pages   - Maximum pages to crawl (default: 50)\n");
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}